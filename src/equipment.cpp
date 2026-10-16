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
        std::lock_guard<std::mutex> lock(equipmentMutex_);
        
        // Validate equipment type first
        if (type_ != EquipmentType::Forklift) {
            throw std::invalid_argument("Equipment is not a forklift");
        }
        
        // Comprehensive input validation
        if (x < 0 || y < 0 || z < 0) {
            throw std::invalid_argument("Coordinates cannot be negative");
        }
        
        // Check for potential overflow with warehouse bounds
        if (x > WAREHOUSE_MAX_X || y > WAREHOUSE_MAX_Y || z > WAREHOUSE_MAX_Z ||
            x < WAREHOUSE_MIN_X || y < WAREHOUSE_MIN_Y || z < WAREHOUSE_MIN_Z) {
            throw std::out_of_range("Coordinates outside warehouse boundaries");
        }
        
        // Safety checks for forklift operations
        if (status_ == EquipmentStatus::Offline || status_ == EquipmentStatus::Maintenance) {
            throw std::runtime_error("Forklift is not operational");
        }
        
        // Handle emergency stop with proper transaction management
        if (emergencyStop) {
            std::unique_ptr<DatabaseTransaction> transaction;
            try {
                auto& db = Database::getInstance();
                if (!db.isConnected()) {
                    throw std::runtime_error("Database connection not available for emergency stop");
                }
                
                transaction = db.beginTransaction();
                auto stmt = db.prepareStatement(
                    "UPDATE equipment SET status=?, speed=?, last_update=? WHERE id=?");
                if (!stmt) {
                    throw std::runtime_error("Failed to prepare emergency stop statement");
                }
                
                auto now = std::chrono::system_clock::now();
                stmt->setInt(1, static_cast<int>(EquipmentStatus::EmergencyStop));
                stmt->setInt(2, 0);
                stmt->setTimestamp(3, now);
                stmt->setInt(4, id_);
                
                if (!stmt->execute()) {
                    throw std::runtime_error("Failed to update emergency stop in database");
                }
                
                transaction->commit();
                
                // Update internal state only after successful database commit
                status_ = EquipmentStatus::EmergencyStop;
                currentSpeed_ = 0;
                lastUpdateTime_ = now;
                
                Logger::logCritical("Emergency stop activated for forklift " + std::to_string(id_));
            } catch (const std::exception& e) {
                if (transaction) {
                    try {
                        transaction->rollback();
                    } catch (const std::exception& rollbackError) {
                        Logger::logError("Transaction rollback failed: " + std::string(rollbackError.what()));
                    }
                }
                Logger::logError("Emergency stop failed: " + std::string(e.what()));
                throw std::runtime_error("Failed to process emergency stop: " + std::string(e.what()));
            }
            return;
        }
        
        // Zone validation with proper warehouse zone checking
        if (!isValidWarehouseZone(x, y, z)) {
            throw std::invalid_argument("Target position is in restricted zone");
        }
        
        // Calculate safe speed with bounds checking
        int safeSpeed = calculateSafeSpeed(x, y, z);
        safeSpeed = std::clamp(safeSpeed, 0, MAX_FORKLIFT_SPEED);
        
        // Database transaction with proper rollback handling
        std::unique_ptr<DatabaseTransaction> transaction;
        try {
            auto& db = Database::getInstance();
            if (!db.isConnected()) {
                throw std::runtime_error("Database connection not available");
            }
            
            transaction = db.beginTransaction();
            auto now = std::chrono::system_clock::now();
            
            // Update position table
            auto posStmt = db.prepareStatement(
                "INSERT INTO forklift_positions (equipment_id, x, y, z, timestamp, speed) VALUES (?, ?, ?, ?, ?, ?) "
                "ON DUPLICATE KEY UPDATE x=VALUES(x), y=VALUES(y), z=VALUES(z), timestamp=VALUES(timestamp), speed=VALUES(speed)");
            if (!posStmt) {
                throw std::runtime_error("Failed to prepare position statement");
            }
            
            posStmt->setInt(1, id_);
            posStmt->setInt(2, x);
            posStmt->setInt(3, y);
            posStmt->setInt(4, z);
            posStmt->setTimestamp(5, now);
            posStmt->setInt(6, safeSpeed);
            
            if (!posStmt->execute()) {
                throw std::runtime_error("Position update failed");
            }
            
            // Update equipment status
            auto equipStmt = db.prepareStatement(
                "UPDATE equipment SET last_x=?, last_y=?, last_z=?, speed=?, last_update=? WHERE id=?");
            if (!equipStmt) {
                throw std::runtime_error("Failed to prepare equipment statement");
            }
            
            equipStmt->setInt(1, x);
            equipStmt->setInt(2, y);
            equipStmt->setInt(3, z);
            equipStmt->setInt(4, safeSpeed);
            equipStmt->setTimestamp(5, now);
            equipStmt->setInt(6, id_);
            
            if (!equipStmt->execute()) {
                throw std::runtime_error("Equipment update failed");
            }
            
            transaction->commit();
            
            // Update internal state only after successful database commit
            WarehousePosition newPos(x, y, z);
            recordWarehousePosition(newPos);
            currentSpeed_ = safeSpeed;
            lastUpdateTime_ = now;
            
            Logger::logInfo("Forklift " + std::to_string(id_) + " moved to position (" + 
                           std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + 
                           ") at speed " + std::to_string(safeSpeed));
            
        } catch (const std::exception& e) {
            if (transaction) {
                try {
                    transaction->rollback();
                } catch (const std::exception& rollbackError) {
                    Logger::logError("Transaction rollback failed: " + std::string(rollbackError.what()));
                }
            }
            Logger::logError("Forklift movement failed: " + std::string(e.what()));
            throw std::runtime_error("Failed to move forklift: " + std::string(e.what()));
        }
    }
} // namespace equipment_tracker