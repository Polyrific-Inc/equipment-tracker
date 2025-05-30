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

    bool Equipment::moveForklift(int x, int y, int z, bool emergencyStop) {
        // Validate equipment type first for safety
        if (type_ != EquipmentType::Forklift) {
            throw std::invalid_argument("Operation only valid for forklift equipment");
        }

        // Validate database connection
        if (!database_) {
            throw std::runtime_error("Database connection not available");
        }

        // Input validation for coordinates with reasonable bounds
        if (x < 0 || y < 0 || z < 0) {
            throw std::invalid_argument("Coordinates must be non-negative");
        }
        
        // Add upper bounds validation for safety
        const int MAX_COORDINATE = 100000; // Define reasonable warehouse limits
        if (x > MAX_COORDINATE || y > MAX_COORDINATE || z > MAX_COORDINATE) {
            throw std::invalid_argument("Coordinates exceed maximum warehouse bounds");
        }

        // Emergency stop safety check - handle immediately without other validations
        if (emergencyStop) {
            status_ = EquipmentStatus::EmergencyStop;
            try {
                return executeEmergencyStop();
            } catch (const std::exception& e) {
                logError("Emergency stop failed: " + std::string(e.what()));
                status_ = EquipmentStatus::Error;
                return false;
            }
        }

        try {
            // Warehouse zone boundary validation with error handling
            if (!isValidWarehousePosition(x, y, z)) {
                throw std::out_of_range("Position outside valid warehouse boundaries");
            }

            // Safety checks for forklift movement
            if (!performSafetyChecks()) {
                throw std::runtime_error("Safety checks failed - movement aborted");
            }

            // Validate movement constraints
            Position currentPos = getCurrentPosition();
            Position targetPos(x, y, z);
            if (!isValidMovement(currentPos, targetPos)) {
                throw std::invalid_argument("Invalid movement - exceeds safety limits");
            }

            // Begin database transaction for atomic operation
            auto transaction = database_->beginTransaction();
            
            try {
                // Use prepared statement for database operation
                auto stmt = database_->prepareStatement(
                    "UPDATE forklift_positions SET x=?, y=?, z=?, timestamp=? WHERE equipment_id=?");
                
                if (!stmt) {
                    throw std::runtime_error("Failed to prepare database statement");
                }
                
                stmt->setInt(1, x);
                stmt->setInt(2, y);
                stmt->setInt(3, z);
                stmt->setTimestamp(4, getCurrentTimestamp());
                stmt->setString(5, equipmentId_);
                
                if (!stmt->execute()) {
                    throw std::runtime_error("Failed to update position in database");
                }

                // Update position with validation
                Position newPosition(x, y, z);
                recordPosition(newPosition);
                
                // Calculate safe speed based on movement and load
                int maxSafeSpeed = calculateMaxSafeSpeed(newPosition);
                int configuredMax = getConfiguredMaxSpeed();
                int currentSpeed = std::min(maxSafeSpeed, configuredMax);
                
                if (isMoving()) {
                    setSpeed(currentSpeed);
                }
                
                // Commit transaction
                transaction->commit();
                
                status_ = EquipmentStatus::Active;
                return true;
                
            } catch (const std::exception& e) {
                // Rollback transaction on any failure
                transaction->rollback();
                throw; // Re-throw to be caught by outer handler
            }
            
        } catch (const std::invalid_argument& e) {
            // Handle validation errors - don't change status for invalid input
            logError("Invalid forklift movement parameters: " + std::string(e.what()));
            throw; // Re-throw validation errors to caller
        } catch (const std::out_of_range& e) {
            // Handle boundary errors
            logError("Forklift movement out of bounds: " + std::string(e.what()));
            throw; // Re-throw boundary errors to caller
        } catch (const std::runtime_error& e) {
            // Handle system/safety errors - set error status
            logError("Forklift movement system error: " + std::string(e.what()));
            status_ = EquipmentStatus::Error;
            return false;
        } catch (const std::exception& e) {
            // Handle any other unexpected errors
            logError("Unexpected error in forklift movement: " + std::string(e.what()));
            status_ = EquipmentStatus::Error;
            return false;
        }
    }
} // namespace equipment_tracker