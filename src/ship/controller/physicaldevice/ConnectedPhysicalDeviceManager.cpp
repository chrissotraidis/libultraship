#include "ship/controller/physicaldevice/ConnectedPhysicalDeviceManager.h"

#include <algorithm>
#include <spdlog/spdlog.h>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace Ship {
namespace {
bool IsUsableController(int32_t instanceId, SDL_GameController* gamepad) {
    SDL_Joystick* joystick = gamepad == nullptr ? nullptr : SDL_GameControllerGetJoystick(gamepad);
    return gamepad != nullptr && SDL_GameControllerGetAttached(gamepad) && joystick != nullptr &&
           SDL_JoystickInstanceID(joystick) == instanceId;
}
} // namespace

ConnectedPhysicalDeviceManager::ConnectedPhysicalDeviceManager() {
}

ConnectedPhysicalDeviceManager::~ConnectedPhysicalDeviceManager() {
    CloseConnectedSDLGamepads();
}

void ConnectedPhysicalDeviceManager::CloseConnectedSDLGamepads() {
    for (const auto& [instanceId, gamepad] : mConnectedSDLGamepads) {
        (void)instanceId;
        SDL_GameControllerClose(gamepad);
    }
    mConnectedSDLGamepads.clear();
    mConnectedSDLGamepadNames.clear();
}

std::unordered_map<int32_t, SDL_GameController*>
ConnectedPhysicalDeviceManager::GetConnectedSDLGamepadsForPort(uint8_t portIndex) {
    std::unordered_map<int32_t, SDL_GameController*> result;

    for (const auto& [instanceId, gamepad] : mConnectedSDLGamepads) {
        if (IsUsableController(instanceId, gamepad) && !PortIsIgnoringInstanceId(portIndex, instanceId)) {
            result[instanceId] = gamepad;
        }
    }

    return result;
}

std::unordered_map<int32_t, std::string> ConnectedPhysicalDeviceManager::GetConnectedSDLGamepadNames() {
    return mConnectedSDLGamepadNames;
}

std::unordered_set<int32_t> ConnectedPhysicalDeviceManager::GetIgnoredInstanceIdsForPort(uint8_t portIndex) {
    return mIgnoredInstanceIds[portIndex];
}

bool ConnectedPhysicalDeviceManager::PortIsIgnoringInstanceId(uint8_t portIndex, int32_t instanceId) {
    return GetIgnoredInstanceIdsForPort(portIndex).contains(instanceId);
}

void ConnectedPhysicalDeviceManager::IgnoreInstanceIdForPort(uint8_t portIndex, int32_t instanceId) {
    mIgnoredInstanceIds[portIndex].insert(instanceId);
}

void ConnectedPhysicalDeviceManager::UnignoreInstanceIdForPort(uint8_t portIndex, int32_t instanceId) {
    mIgnoredInstanceIds[portIndex].erase(instanceId);
}

void ConnectedPhysicalDeviceManager::HandlePhysicalDeviceConnect(int32_t sdlDeviceIndex) {
    std::string reason = "connect event device-index=" + std::to_string(sdlDeviceIndex);
    ReconcileConnectedSDLGamepads(reason.c_str());
}

void ConnectedPhysicalDeviceManager::HandlePhysicalDeviceDisconnect(int32_t sdlJoystickInstanceId) {
    std::string reason = "disconnect event instance=" + std::to_string(sdlJoystickInstanceId);
    ReconcileConnectedSDLGamepads(reason.c_str());
}

void ConnectedPhysicalDeviceManager::RefreshConnectedSDLGamepads() {
    ReconcileConnectedSDLGamepads("manual refresh");
}

void ConnectedPhysicalDeviceManager::ReconcileConnectedSDLGamepads(const char* reason) {
#ifndef __IOS__
    CloseConnectedSDLGamepads();
#endif
    const char* reconciliationReason = reason == nullptr ? "unspecified" : reason;
    bool isPeriodicCheck = SDL_strcmp(reconciliationReason, "active periodic check") == 0;
    if (!isPeriodicCheck) {
        SPDLOG_DEBUG("Reconciling SDL controllers: {}", reconciliationReason);
    }
    static SDL_JoystickGUID sZeroGuid;

#ifdef __IOS__
    struct CurrentDevice {
        int32_t deviceIndex;
        int32_t instanceId;
        std::string guid;
        std::string name;
        bool isTouchController;
    };
    std::vector<CurrentDevice> currentDevices;
#endif

    for (int32_t i = 0; i < SDL_NumJoysticks(); i++) {
        SDL_JoystickGUID deviceGUID = SDL_JoystickGetDeviceGUID(i);
        if (SDL_memcmp(&deviceGUID, &sZeroGuid, sizeof(deviceGUID)) == 0) {
            SPDLOG_WARN(
                "Calling SDL JoystickGetDeviceGUID with index ({:d}) returned zero GUID. This is likely due to an "
                "invalid index. Refer to https://wiki.libsdl.org/SDL2/SDL_JoystickGetDeviceGUID for more information.",
                i);
            continue;
        }

        char deviceGuidCStr[33] = "";
        SDL_JoystickGetGUIDString(deviceGUID, deviceGuidCStr, sizeof(deviceGuidCStr));

        if (!SDL_IsGameController(i)) {
#ifdef __IOS__
            if (!isPeriodicCheck) {
                SPDLOG_DEBUG("Ignoring non-controller SDL joystick at device index {} (GUID {})", i, deviceGuidCStr);
            }
#else
            SPDLOG_WARN("SDL Joystick (GUID: {}) not recognized as gamepad."
                        "This is likely due to a missing mapping string in gamecontrollerdb.txt."
                        "Refer to https://github.com/mdqinc/SDL_GameControllerDB for more information.",
                        deviceGuidCStr);
#endif
            continue;
        }

#if TARGET_OS_SIMULATOR
        const char* simulatorControllerName = SDL_GameControllerNameForIndex(i);
        if (simulatorControllerName != nullptr && SDL_strcmp(simulatorControllerName, "Gamepad") == 0) {
            SPDLOG_DEBUG("Ignoring CoreSimulator's placeholder gamepad");
            continue;
        }
#endif

#ifdef __IOS__
        int32_t instanceId = SDL_JoystickGetDeviceInstanceID(i);
        if (instanceId < 0) {
            SPDLOG_ERROR("SDL device instance lookup failed (device index {}, GUID {}): {}", i, deviceGuidCStr,
                         SDL_GetError());
            continue;
        }
        const char* name = SDL_GameControllerNameForIndex(i);
        std::string gamepadName = name == nullptr ? deviceGuidCStr : name;
        currentDevices.push_back({ i, instanceId, deviceGuidCStr, gamepadName,
                                   gamepadName == "SpaghettiPad Touch Controller" });
#else
        SDL_GameController* gamepad = SDL_GameControllerOpen(i);
        if (gamepad == nullptr) {
            SPDLOG_ERROR("SDL GameControllerOpen error (GUID: {}): {}", deviceGuidCStr, SDL_GetError());
            continue;
        }

        int32_t instanceId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gamepad));
        if (instanceId < 0) {
            SPDLOG_ERROR("SDL JoystickInstanceID error (GUID: {}): {}", deviceGuidCStr, SDL_GetError());
            SDL_GameControllerClose(gamepad);
            continue;
        }

        const char* name = SDL_GameControllerName(gamepad);
        std::string gamepadName = name == nullptr ? deviceGuidCStr : name;
        mConnectedSDLGamepads[instanceId] = gamepad;
        mConnectedSDLGamepadNames[instanceId] = gamepadName;
        for (uint8_t port = 1; port < 4; port++) {
            mIgnoredInstanceIds[port].insert(instanceId);
        }
#endif
    }

#ifdef __IOS__
    bool hasExternalController = std::any_of(currentDevices.begin(), currentDevices.end(), [](const auto& device) {
        return !device.isTouchController;
    });
    if (hasExternalController) {
        std::erase_if(currentDevices, [](const auto& device) { return device.isTouchController; });
    }

    std::vector<int32_t> currentInstanceIds;
    currentInstanceIds.reserve(currentDevices.size());
    for (const auto& device : currentDevices) {
        currentInstanceIds.push_back(device.instanceId);
    }

    for (auto iterator = mConnectedSDLGamepads.begin(); iterator != mConnectedSDLGamepads.end();) {
        int32_t instanceId = iterator->first;
        SDL_GameController* gamepad = iterator->second;
        bool present = std::find(currentInstanceIds.begin(), currentInstanceIds.end(), instanceId) !=
                       currentInstanceIds.end();
        if (present && IsUsableController(instanceId, gamepad)) {
            ++iterator;
            continue;
        }

        auto slot = mIOSControllerSlots.SlotFor(instanceId);
        SPDLOG_INFO("Closing stale iOS controller instance {} from {} during {}", instanceId,
                    slot.has_value() ? "player " + std::to_string(*slot + 1) : "unassigned ownership",
                    reason == nullptr ? "unspecified reconciliation" : reason);
        if (gamepad != nullptr) {
            SDL_GameControllerClose(gamepad);
        }
        mConnectedSDLGamepadNames.erase(instanceId);
        iterator = mConnectedSDLGamepads.erase(iterator);
    }

    for (const auto& device : currentDevices) {
        if (mConnectedSDLGamepads.contains(device.instanceId)) {
            mConnectedSDLGamepadNames[device.instanceId] = device.name;
            continue;
        }

        SDL_GameController* gamepad = SDL_GameControllerOpen(device.deviceIndex);
        if (gamepad == nullptr) {
            SPDLOG_ERROR("SDL GameControllerOpen failed (device index {}, GUID {}, name \"{}\") during {}: {}",
                         device.deviceIndex, device.guid, device.name,
                         reason == nullptr ? "unspecified reconciliation" : reason, SDL_GetError());
            continue;
        }

        SDL_Joystick* joystick = SDL_GameControllerGetJoystick(gamepad);
        int32_t openedInstanceId = joystick == nullptr ? -1 : SDL_JoystickInstanceID(joystick);
        if (openedInstanceId != device.instanceId) {
            SPDLOG_ERROR("Closing SDL controller after instance mismatch (device index {}, expected {}, opened {})",
                         device.deviceIndex, device.instanceId, openedInstanceId);
            SDL_GameControllerClose(gamepad);
            continue;
        }

        mConnectedSDLGamepads[device.instanceId] = gamepad;
        mConnectedSDLGamepadNames[device.instanceId] = device.name;
    }

    std::vector<int32_t> usableInstanceIds;
    for (const auto& device : currentDevices) {
        auto gamepad = mConnectedSDLGamepads.find(device.instanceId);
        if (gamepad != mConnectedSDLGamepads.end() && IsUsableController(device.instanceId, gamepad->second)) {
            usableInstanceIds.push_back(device.instanceId);
        }
    }

    for (const auto& [slot, instanceId] : mIOSControllerSlots.ReleaseMissing(usableInstanceIds)) {
        SPDLOG_INFO("Released stale iOS controller instance {} from player {} during {}", instanceId, slot + 1,
                    reason == nullptr ? "unspecified reconciliation" : reason);
    }

    for (const auto& device : currentDevices) {
        if (std::find(usableInstanceIds.begin(), usableInstanceIds.end(), device.instanceId) == usableInstanceIds.end() ||
            mIOSControllerSlots.SlotFor(device.instanceId).has_value()) {
            continue;
        }
        auto slot = mIOSControllerSlots.Assign(device.instanceId);
        if (slot.has_value()) {
            SPDLOG_INFO("Assigned iOS controller \"{}\" (device index {}, instance {}) to player {} during {}",
                        device.name, device.deviceIndex, device.instanceId, *slot + 1,
                        reason == nullptr ? "unspecified reconciliation" : reason);
        } else {
            SPDLOG_INFO("Leaving extra iOS controller \"{}\" (device index {}, instance {}) unassigned", device.name,
                        device.deviceIndex, device.instanceId);
        }
    }

    mIgnoredInstanceIds.clear();
    for (const auto& [instanceId, gamepad] : mConnectedSDLGamepads) {
        (void)gamepad;
        auto slot = mIOSControllerSlots.SlotFor(instanceId);
        for (uint8_t port = 0; port < SpaghettiPadControllerSlots::kPlayerCount; ++port) {
            if (!slot.has_value() || *slot != port) {
                mIgnoredInstanceIds[port].insert(instanceId);
            }
        }
    }
#endif
}
} // namespace Ship
