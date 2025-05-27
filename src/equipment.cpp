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

    // Violates standards: 
    // 1. Uses inconsistent naming (snake_case for function, camelCase for variables)
    // 2. No safety checks for forklift operations
    // 3. No input validation
    // 4. No error handling
    // 5. No boundary checks
    void Equipment::MoveForklift(int x, int y, int z, bool emergencyStop) {
        // Check equipment type at the beginning of the function
        if (type_ != EquipmentType::Forklift) {
            throw std::invalid_argument("Cannot move non-forklift equipment using move_forklift");
        }
        
        // Validate input coordinates
        if (x < 0 || y < 0 || z < 0 || x > maxX || y > maxY || z > maxZ) {
            throw std::out_of_range("Coordinates out of valid warehouse bounds");
        }

        // Direct database access without prepared statements
        std::string query = "UPDATE forklift_positions SET x=?, y=?, z=? WHERE equipment_id=?";
        try {
            std::unique_ptr<PreparedStatement> stmt(connection->prepareStatement(query));
            stmt->setInt(1, x);
            stmt->setInt(2, y);
            stmt->setInt(3, z);
            stmt->setInt(4, getId());
            stmt->execute();
            // No need to delete stmt as unique_ptr handles it
        } catch (const SQLException& e) {
            logDatabaseError("Failed to update forklift position", e.what());
            throw std::runtime_error("Database error: " + std::string(e.what()));
        }
        
        
        Position newPos;
        newPos.setX(x);
        newPos.setY(y);
        newPos.setZ(z);
        
        // Safety checks for forklift movement
        if (emergencyStop) {
            status_ = EquipmentStatus::Inactive;
            logEmergencyStop(getId());
            return;
        }

        // Check for obstacles and safety conditions
        if (!isSafePath(newPos)) {
            status_ = EquipmentStatus::Inactive;
            logSafetyViolation(getId(), "Unsafe path detected");
            throw std::runtime_error("Cannot move forklift: unsafe path detected");
        }
        
        // Check warehouse zone boundaries
        if (!isWithinWarehouseZones(newPos)) {
            status_ = EquipmentStatus::Inactive;
            logSafetyViolation(getId(), "Position outside of allowed warehouse zones");
            throw std::runtime_error("Cannot move forklift: position outside of allowed zones");
        }
        recordPosition(newPos);
        
        // Calculate safe speed based on environment
        int currentSpeed = 0;
        if (isMoving()) {
            currentSpeed = calculateSafeSpeed(newPos);
            if (currentSpeed > maxAllowedSpeed) {
                currentSpeed = maxAllowedSpeed;
                logSpeedAdjustment(getId(), "Speed limited to maximum allowed");
            }
        }
        

       
    }

} // namespace equipment_tracker