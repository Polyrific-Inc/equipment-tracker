#include <sstream>
#include <chrono>
#include <cmath>
#include <mutex>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <string>
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/utils/constants.h"
#include "equipment_tracker/position.h"
#include "equipment_tracker/database.h"
#include "equipment_tracker/logger.h"

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
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Validate equipment type first
        if (type_ != EquipmentType::Forklift) {
            throw std::invalid_argument("Equipment is not a forklift");
        }
        
        // Define warehouse boundaries (should be moved to constants.h)
        constexpr int WAREHOUSE_MIN_X = 0;
        constexpr int WAREHOUSE_MAX_X = 1000;
        constexpr int WAREHOUSE_MIN_Y = 0;
        constexpr int WAREHOUSE_MAX_Y = 1000;
        constexpr int WAREHOUSE_MIN_Z = 0;
        constexpr int WAREHOUSE_MAX_Z = 100;
        constexpr int MAX_FORKLIFT_SPEED = 50;
        
        // Input validation for coordinates
        if (x < WAREHOUSE_MIN_X || x > WAREHOUSE_MAX_X ||
            y < WAREHOUSE_MIN_Y || y > WAREHOUSE_MAX_Y ||
            z < WAREHOUSE_MIN_Z || z > WAREHOUSE_MAX_Z) {
            throw std::out_of_range("Coordinates outside warehouse boundaries");
        }
        
        // Safety checks for forklift operations
        if (status_ == EquipmentStatus::Offline || status_ == EquipmentStatus::Maintenance) {
            throw std::runtime_error("Forklift is not operational");
        }
        
        // Handle emergency stop with proper safety protocols
        if (emergencyStop) {
            status_ = EquipmentStatus::EmergencyStop;
            currentSpeed_ = 0;
            Logger::logCritical("Emergency stop activated for forklift " + std::to_string(id_));
            return;
        }
        
        // Basic zone validation (implement proper zone checking logic)
        bool isValidZone = true; // Placeholder - implement actual zone validation
        if (!isValidZone) {
            throw std::invalid_argument("Target position is in restricted zone");
        }
        
        // Create position with validation
        Position newPos;
        try {
            newPos.setX(x);
            newPos.setY(y);
            newPos.setZ(z);
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid position coordinates: " + std::string(e.what()));
        }
        
        // Calculate safe speed (basic implementation)
        int safeSpeed = std::min(MAX_FORKLIFT_SPEED, 30); // Default safe speed
        
        // Use prepared statement for database operation with connection validation
        try {
            auto& db = Database::getInstance();
            if (!db.isConnected()) {
                throw std::runtime_error("Database connection not available");
            }
            
            auto stmt = db.prepareStatement(
                "UPDATE forklift_positions SET x=?, y=?, z=?, timestamp=?, speed=? WHERE equipment_id=?");
            if (!stmt) {
                throw std::runtime_error("Failed to prepare database statement");
            }
            
            stmt->setInt(1, x);
            stmt->setInt(2, y);
            stmt->setInt(3, z);
            stmt->setTimestamp(4, std::chrono::system_clock::now());
            stmt->setInt(5, safeSpeed);
            stmt->setInt(6, id_);
            
            if (!stmt->execute()) {
                throw std::runtime_error("Database statement execution failed");
            }
        } catch (const std::exception& e) {
            Logger::logError("Database update failed for forklift movement: " + std::string(e.what()));
            throw std::runtime_error("Failed to update forklift position: " + std::string(e.what()));
        }
        
        // Update internal state
        try {
            recordPosition(newPos);
            currentSpeed_ = safeSpeed;
            lastUpdateTime_ = std::chrono::system_clock::now();
            
            Logger::logInfo("Forklift " + std::to_string(id_) + " moved to position (" + 
                           std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")");
        } catch (const std::exception& e) {
            Logger::logError("Failed to update forklift state: " + std::string(e.what()));
            throw;
        }
    }
} // namespace equipment_tracker