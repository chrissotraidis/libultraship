#include "ship/controller/physicaldevice/ControllerSlotAssignments.h"

#include <unordered_set>

namespace Ship {

void ControllerSlotAssignments::Reconcile(const std::vector<Device>& devices) {
    mKnownDevices.clear();
    for (const auto& device : devices) {
        mKnownDevices[device.instanceId] = device.stableId;
    }

    auto previousSlots = mSlots;
    for (auto& slot : mSlots) {
        slot.instanceId.reset();
    }

    std::unordered_set<int32_t> assigned;

    // Preserve valid connected instances in their existing slots.
    for (uint8_t slotIndex = 0; slotIndex < SlotCount; ++slotIndex) {
        const auto& previous = previousSlots[slotIndex];
        if (!previous.instanceId.has_value()) {
            continue;
        }

        auto known = mKnownDevices.find(*previous.instanceId);
        if (known != mKnownDevices.end() && known->second == previous.stableId &&
            !mSuppressedDevices.contains(known->second)) {
            mSlots[slotIndex] = previous;
            assigned.insert(*previous.instanceId);
        }
    }

    // A valid controller keeps its slot. If the sole device is instead a
    // returning/new instance after all prior ownership went stale, it becomes
    // player 1.
    if (devices.size() == 1 && assigned.empty() && !mSuppressedDevices.contains(devices[0].stableId)) {
        mSlots[0] = { devices[0].instanceId, devices[0].stableId };
        return;
    }

    // A reconnected controller with a new SDL instance ID reclaims its prior
    // slot when its backend identity is still available.
    for (const auto& device : devices) {
        if (assigned.contains(device.instanceId)) {
            continue;
        }
        if (mSuppressedDevices.contains(device.stableId)) {
            continue;
        }
        for (uint8_t slotIndex = 0; slotIndex < SlotCount; ++slotIndex) {
            if (!mSlots[slotIndex].instanceId.has_value() &&
                previousSlots[slotIndex].stableId == device.stableId) {
                mSlots[slotIndex] = { device.instanceId, device.stableId };
                assigned.insert(device.instanceId);
                break;
            }
        }
    }

    // Genuinely additional controllers take the next free player slot.
    for (const auto& device : devices) {
        if (assigned.contains(device.instanceId)) {
            continue;
        }
        if (mSuppressedDevices.contains(device.stableId)) {
            continue;
        }
        for (uint8_t slotIndex = 0; slotIndex < SlotCount; ++slotIndex) {
            if (!mSlots[slotIndex].instanceId.has_value()) {
                mSlots[slotIndex] = { device.instanceId, device.stableId };
                assigned.insert(device.instanceId);
                break;
            }
        }
    }
}

bool ControllerSlotAssignments::Assign(int32_t instanceId, uint8_t slot) {
    auto known = mKnownDevices.find(instanceId);
    if (known == mKnownDevices.end() || slot >= SlotCount) {
        return false;
    }

    auto currentSlot = GetSlot(instanceId);
    auto displacedInstanceId = mSlots[slot].instanceId;
    if (currentSlot.has_value()) {
        mSlots[*currentSlot].instanceId.reset();
    }
    mSlots[slot] = { instanceId, known->second };
    mSuppressedDevices.erase(known->second);

    if (displacedInstanceId.has_value() && *displacedInstanceId != instanceId) {
        auto displaced = mKnownDevices.find(*displacedInstanceId);
        if (currentSlot.has_value() && displaced != mKnownDevices.end()) {
            mSlots[*currentSlot] = { *displacedInstanceId, displaced->second };
        } else if (displaced != mKnownDevices.end()) {
            mSuppressedDevices.insert(displaced->second);
        }
    }
    return true;
}

bool ControllerSlotAssignments::Unassign(int32_t instanceId, uint8_t slot) {
    if (slot >= SlotCount || mSlots[slot].instanceId != instanceId) {
        return false;
    }
    auto known = mKnownDevices.find(instanceId);
    if (known != mKnownDevices.end()) {
        mSuppressedDevices.insert(known->second);
    }
    mSlots[slot].instanceId.reset();
    return true;
}

std::optional<uint8_t> ControllerSlotAssignments::GetSlot(int32_t instanceId) const {
    for (uint8_t slotIndex = 0; slotIndex < SlotCount; ++slotIndex) {
        if (mSlots[slotIndex].instanceId == instanceId) {
            return slotIndex;
        }
    }
    return std::nullopt;
}

std::optional<int32_t> ControllerSlotAssignments::GetInstanceId(uint8_t slot) const {
    if (slot >= SlotCount) {
        return std::nullopt;
    }
    return mSlots[slot].instanceId;
}

} // namespace Ship
