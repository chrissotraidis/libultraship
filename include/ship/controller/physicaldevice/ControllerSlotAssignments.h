#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Ship {

/**
 * @brief Keeps SDL controller instance IDs in stable N64 player slots.
 *
 * SDL instance IDs may change after sleep or reconnect. This helper therefore
 * preserves an attached instance first, then a matching device identity, and
 * finally assigns new devices to the first free slot.
 */
class ControllerSlotAssignments {
  public:
    static constexpr uint8_t SlotCount = 4;

    struct Device {
        int32_t instanceId;
        std::string stableId;
    };

    void Reconcile(const std::vector<Device>& devices);
    bool Assign(int32_t instanceId, uint8_t slot);
    bool Unassign(int32_t instanceId, uint8_t slot);

    std::optional<uint8_t> GetSlot(int32_t instanceId) const;
    std::optional<int32_t> GetInstanceId(uint8_t slot) const;

  private:
    struct Slot {
        std::optional<int32_t> instanceId;
        std::string stableId;
    };

    std::array<Slot, SlotCount> mSlots = {};
    std::unordered_map<int32_t, std::string> mKnownDevices;
    std::unordered_set<std::string> mSuppressedDevices;
};

} // namespace Ship
