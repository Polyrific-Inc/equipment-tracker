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

    Equipment::Equipment(const Equipment &other)
        : id_(other.id_),
          type_(other.type_),
          name_(other.name_),
          status_(other.status_),
          max_history_size_(other.max_history_size_)
    {
        // Lock the other object's mutex during copy
        std::lock_guard<std::mutex> lock(other.mutex_);

        // Copy the position data
        last_position_ = other.last_position_;
        position_history_ = other.position_history_;
    }

    Equipment &Equipment::operator=(const Equipment &other)
    {
        if (this != &other)
        {
            // Lock both mutexes (be careful about order to prevent deadlock)
            std::scoped_lock lock(mutex_, other.mutex_);

            // Copy all data members
            id_ = other.id_;
            type_ = other.type_;
            name_ = other.name_;
            status_ = other.status_;
            last_position_ = other.last_position_;
            position_history_ = other.position_history_;
            max_history_size_ = other.max_history_size_;
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
        // Equipment-specific speed limits based on type (warehouse-appropriate speeds)
        switch (type_) {
            case EquipmentType::Forklift:
                return 3.5; // 3.5 m/s (12.6 km/h) max for forklifts - warehouse safety standard
            case EquipmentType::ReachTruck:
                return 2.8; // 2.8 m/s (10 km/h) max for reach trucks
            case EquipmentType::OrderPicker:
                return 2.2; // 2.2 m/s (8 km/h) max for order pickers
            case EquipmentType::Pallet:
                return 2.5; // 2.5 m/s (9 km/h) max for pallet trucks
            case EquipmentType::Crane:
                return 1.4; // 1.4 m/s (5 km/h) max for cranes
            case EquipmentType::Bulldozer:
                return 4.2; // 4.2 m/s (15 km/h) max for bulldozers (outdoor)
            case EquipmentType::Excavator:
                return 3.9; // 3.9 m/s (14 km/h) max for excavators (outdoor)
            case EquipmentType::Truck:
                return 8.3; // 8.3 m/s (30 km/h) max for trucks in warehouse areas
            default:
                return 2.0; // Conservative default for unknown equipment
        }
    }

    void Equipment::recordPosition(const Position &position)
    {
        // Validate position data before acquiring lock
        if (!position.isValid()) {
            spdlog::error("Position validation failed for equipment {}: Invalid position data", name_);
            return;
        }

        // Raymond safety check: Validate equipment is in safe operating state
        if (status_ == EquipmentStatus::Maintenance || status_ == EquipmentStatus::Error) {
            spdlog::warn("Position update rejected for equipment {} in unsafe state: {}", name_, static_cast<int>(status_));
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
                    const double maxAllowedSpeed = getMaxAllowedSpeed();
                    
                    if (speedMps > maxAllowedSpeed) {
                        spdlog::warn("Detected unrealistic movement speed: {:.2f} m/s for equipment {} (distance: {:.2f}m, time: {:.2f}s, limit: {:.2f} m/s). Position update rejected.", 
                                   speedMps, name_, distanceFromLast, timeDiffSeconds, maxAllowedSpeed);
                        return;
                    }
                } else if (timeDiffSeconds < 0.0) {
                    spdlog::warn("Received position with timestamp in the past for equipment {}. Position update rejected.", name_);
                    return;
                }
                // Handle timeDiffSeconds == 0.0 case: allow duplicate timestamps but log for monitoring
                else if (timeDiffSeconds == 0.0 && distanceFromLast > 0.001) {
                    spdlog::debug("Equipment {} moved {:.3f}m with identical timestamp - possible GPS jitter", name_, distanceFromLast);
                }
            }

            // Maintain history size limit with proper bounds checking
            while (position_history_.size() >= max_history_size_) {
                position_history_.erase(position_history_.begin());
            }
            position_history_.push_back(position);

            // Update last position and status
            last_position_ = position;
            status_ = EquipmentStatus::Active;

            // Notify position change listeners if callback is set
            if (position_update_callback_) {
                try {
                    position_update_callback_(position);
                } catch (const std::exception& callbackError) {
                    spdlog::error("Position update callback failed for equipment {}: {}", name_, callbackError.what());
                    // Continue execution - callback failure shouldn't prevent position recording
                }
            }
        }
        catch (const std::exception& e) {
            spdlog::error("Error processing position update for equipment {}: {}", name_, e.what());
            
            // Critical: Do not record potentially invalid positions in fallback
            // Instead, mark equipment as having an error state
            status_ = EquipmentStatus::Error;
            spdlog::critical("Equipment {} marked as error state due to position processing failure", name_);
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