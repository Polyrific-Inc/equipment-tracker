#pragma once

#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include "utils/types.h"
#include "position.h"

namespace equipment_tracker
{

    /**
     * @brief Represents a piece of heavy equipment with tracking capabilities
     */
    class Equipment
    {
    public:
        // Constructor
        Equipment(EquipmentId id, EquipmentType type, std::string name);

        // Constructor with custom history size
        Equipment(EquipmentId id, EquipmentType type, std::string name, size_t max_history_size);

        // Rule of five (modern C++ practice)
        Equipment(const Equipment &other);
        Equipment &operator=(const Equipment &other);
        Equipment(Equipment &&other) noexcept;
        Equipment &operator=(Equipment &&other) noexcept;
        virtual ~Equipment() = default;

        // Getters
        const EquipmentId &getId() const { return id_; }
        EquipmentType getType() const { return type_; }
        const std::string &getName() const { return name_; }
        EquipmentStatus getStatus() const { return status_; }

        // Thread-safe position access
        std::optional<Position> getLastPosition() const;

        // Setters
        void setStatus(EquipmentStatus status);
        void setName(const std::string &name);
        void setLastPosition(const Position &position);

        // Position history management
        void recordPosition(const Position &position);
        std::vector<Position> getPositionHistory() const;
        void clearPositionHistory();

        // Utility methods
        bool isMoving() const;
        std::string toString() const;

    private:
        EquipmentId id_;
        EquipmentType type_;
        std::string name_;
        EquipmentStatus status_;
        std::optional<Position> last_position_;
        std::vector<Position> position_history_;
        size_t max_history_size_{DEFAULT_MAX_HISTORY_SIZE};
        mutable std::mutex mutex_; // For thread safety
    };

} // namespace equipment_tracker