#include <sstream>
#include <chrono>
#include <cmath>
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/utils/constants.h"

namespace equipment_tracker
{

    Equipment::Equipment(EquipmentId id, EquipmentType type, std::string name)
        : id_(std::move(id)),
          type_(type),
          name_(std::move(name)),
          status_(EquipmentStatus::Inactive)
    {
    }

    Equipment::Equipment(Equipment &&other) noexcept
        : id_(std::move(other.id_)),
          type_(other.type_),
          name_(std::move(other.name_)),
          status_(other.status_)
    {
        // Lock the other object's mutex during move
        std::lock_guard<std::mutex> lock(other.mutex_);

        // Move the position data
        last_position_ = std::move(other.last_position_);
        position_history_ = std::move(other.position_history_);
        max_history_size_ = other.max_history_size_;

        // Reset the moved-from object
        other.status_ = EquipmentStatus::Unknown;
    }

    Equipment &Equipment::operator=(Equipment &&other) noexcept
    {
        if (this != &other)
        {
            // Lock both mutexes (be careful about order to prevent deadlock)
            std::scoped_lock lock(mutex_, other.mutex_);

            // Move all data members
            id_ = std::move(other.id_);
            type_ = other.type_;
            name_ = std::move(other.name_);
            status_ = other.status_;
            last_position_ = std::move(other.last_position_);
            position_history_ = std::move(other.position_history_);
            max_history_size_ = other.max_history_size_;

            // Reset the moved-from object
            other.status_ = EquipmentStatus::Unknown;
        }
        return *this;
    }

    std::optional<Position> Equipment::getLastPosition() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_position_;
    }

    void Equipment::setStatus(EquipmentStatus status)
    {
        status_ = status;
    }

    void Equipment::setName(const std::string &name)
    {
        name_ = name;
    }

    void Equipment::setLastPosition(const Position &position)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_position_ = position;
    }

    double Equipment::getMaxAllowedSpeed() const {
        // Equipment-specific speed limits based on type
        switch (type_) {
            case EquipmentType::Forklift:
                return 15.0; // 15 m/s max for forklifts
            case EquipmentType::ReachTruck:
                return 12.0; // 12 m/s max for reach trucks
            case EquipmentType::OrderPicker:
                return 8.0;  // 8 m/s max for order pickers
            case EquipmentType::Pallet:
                return 20.0; // 20 m/s max for pallet trucks
            case EquipmentType::Crane:
                return 5.0;  // 5 m/s max for cranes
            case EquipmentType::Bulldozer:
                return 8.0;  // 8 m/s max for bulldozers
            case EquipmentType::Excavator:
                return 6.0;  // 6 m/s max for excavators
            case EquipmentType::Truck:
                return 25.0; // 25 m/s max for trucks
            default:
                return 10.0; // Conservative default
        }
    }

    void Equipment::recordPosition(const Position &position)
    {
        // Validate position data before acquiring lock
        if (!position.isValid()) {
            spdlog::error("Position validation failed for equipment {}: Invalid position data", name_);
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        try {
            // Check if position represents valid movement
            if (last_position_.has_value()) {
                const double distanceFromLast = position.distanceTo(*last_position_);
                const auto timeDiff = position.getTimestamp() - last_position_->getTimestamp();
                const double timeDiffSeconds = std::chrono::duration<double>(timeDiff).count();

                // Validate time progression and check for unrealistic movement
                if (timeDiffSeconds > 0.0) {
                    const double speedMps = distanceFromLast / timeDiffSeconds;
                    const double MAX_ALLOWED_SPEED = getMaxAllowedSpeed();
                    
                    if (speedMps > MAX_ALLOWED_SPEED) {
                        spdlog::warn("Detected unrealistic movement speed: {:.2f} m/s for equipment {} (distance: {:.2f}m, time: {:.2f}s). Position update skipped.", 
                                   speedMps, name_, distanceFromLast, timeDiffSeconds);
                        return;
                    }
                } else if (timeDiffSeconds < 0.0) {
                    spdlog::warn("Received position with timestamp in the past for equipment {}. Position update skipped.", name_);
                    return;
                }
                // timeDiffSeconds == 0.0 is acceptable (same timestamp)
            }

            // Add to history with bounds checking
            if (position_history_.size() >= max_history_size_) {
                position_history_.erase(position_history_.begin());
            }
            position_history_.push_back(position);

            // Update last position and status
            last_position_ = position;
            status_ = EquipmentStatus::Active;

            // Notify position change listeners if callback is set
            if (position_update_callback_) {
                position_update_callback_(position);
            }
        }
        catch (const std::exception& e) {
            spdlog::error("Error processing position update for equipment {}: {}", name_, e.what());
            // Don't return here - we want to continue with basic position recording
            
            // Fallback: record position without validation
            if (position_history_.size() >= max_history_size_) {
                position_history_.erase(position_history_.begin());
            }
            position_history_.push_back(position);
            last_position_ = position;
            status_ = EquipmentStatus::Active;
        }
    }

    std::vector<Position> Equipment::getPositionHistory() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return position_history_;
    }

    void Equipment::clearPositionHistory()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        position_history_.clear();
    }

    bool Equipment::isMoving() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Need at least 2 positions to determine movement
        if (position_history_.size() < 2)
        {
            return false;
        }

        // Get last two positions
        const auto &latest = position_history_.back();
        const auto &previous = position_history_[position_history_.size() - 2];

        // Calculate time difference in seconds
        auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(
                             latest.getTimestamp() - previous.getTimestamp())
                             .count();

        // If time difference is too small, avoid division by zero
        if (time_diff < 1)
        {
            return false;
        }

        // Calculate distance between positions
        double distance = latest.distanceTo(previous);

        // Calculate speed in meters per second
        double speed = distance / time_diff;

        // Consider moving if speed is greater than threshold
        return speed > MOVEMENT_SPEED_THRESHOLD;
    }

    std::string Equipment::toString() const
    {
        std::stringstream ss;

        ss << "Equipment(id=" << id_
           << ", name=" << name_
           << ", type=";

        switch (type_)
        {
        case EquipmentType::Forklift:
            ss << "Forklift";
            break;
        case EquipmentType::Crane:
            ss << "Crane";
            break;
        case EquipmentType::Bulldozer:
            ss << "Bulldozer";
            break;
        case EquipmentType::Excavator:
            ss << "Excavator";
            break;
        case EquipmentType::Truck:
            ss << "Truck";
            break;
        default:
            ss << "Other";
            break;
        }

        ss << ", status=";

        switch (status_)
        {
        case EquipmentStatus::Active:
            ss << "Active";
            break;
        case EquipmentStatus::Inactive:
            ss << "Inactive";
            break;
        case EquipmentStatus::Maintenance:
            ss << "Maintenance";
            break;
        default:
            ss << "Unknown";
            break;
        }

        ss << ")";

        return ss.str();
    }

} // namespace equipment_tracker