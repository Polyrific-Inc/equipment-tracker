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

    void Equipment::recordPosition(const Position &position)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Add to history
        position_history_.push_back(position);

        // Update last position
        last_position_ = position;

        // Keep history within size limits
        if (position_history_.size() > max_history_size_)
        {
            position_history_.erase(position_history_.begin());
        }

        // Update status to active when position is recorded
        status_ = EquipmentStatus::Active;
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

    void Equipment::moveForklift(int x, int y, int z, bool emergencyStop) {
        // Validate equipment type first for safety
        if (type_ != EquipmentType::Forklift) {
            throw std::invalid_argument("Operation only valid for forklift equipment");
        }
        
        // Input validation for coordinates
        if (x < WAREHOUSE_MIN_X || x > WAREHOUSE_MAX_X ||
            y < WAREHOUSE_MIN_Y || y > WAREHOUSE_MAX_Y ||
            z < WAREHOUSE_MIN_Z || z > WAREHOUSE_MAX_Z) {
            throw std::out_of_range("Coordinates outside warehouse boundaries");
        }
        
        // Safety checks for forklift operations
        if (status_ == EquipmentStatus::Maintenance || 
            status_ == EquipmentStatus::Error) {
            throw std::runtime_error("Cannot move forklift in current status");
        }
        
        // Emergency stop handling with immediate safety response
        if (emergencyStop) {
            status_ = EquipmentStatus::EmergencyStop;
            currentSpeed_ = 0;
            // Log emergency stop event
            logSafetyEvent("Emergency stop activated");
            return;
        }
        
        // Validate zone boundaries for warehouse mapping
        if (!isValidWarehouseZone(x, y, z)) {
            throw std::runtime_error("Target position not in valid warehouse zone");
        }
        
        // Create position with validation
        Position newPosition;
        newPosition.setX(x);
        newPosition.setY(y);
        newPosition.setZ(z);
        
        // Safety speed limits based on zone type
        int maxSpeed = getMaxSpeedForZone(x, y, z);
        int currentSpeed = std::min(calculateSafeSpeed(), maxSpeed);
        
        try {
            // Use prepared statement for database update
            auto stmt = database_->prepareStatement(
                "UPDATE forklift_positions SET x=?, y=?, z=?, speed=?, timestamp=NOW() WHERE equipment_id=?");
            stmt->setInt(1, x);
            stmt->setInt(2, y);
            stmt->setInt(3, z);
            stmt->setInt(4, currentSpeed);
            stmt->setString(5, equipmentId_);
            stmt->executeUpdate();
            
            // Record position after successful database update
            recordPosition(newPosition);
            currentSpeed_ = currentSpeed;
            
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to update forklift position: " + std::string(e.what()));
        }
    }
} // namespace equipment_tracker