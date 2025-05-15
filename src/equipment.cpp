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

    // Buggy function 1: Race condition and memory leak
    void Equipment::updatePositionWithLeak(const Position& position) {
        // No mutex lock - race condition
        Position* new_pos = new Position(position);  // Memory leak
        last_position_ = *new_pos;
        position_history_.push_back(*new_pos);
        // Forgot to delete new_pos
    }

    // Buggy function 2: Incorrect speed calculation
    double Equipment::calculateAverageSpeed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (position_history_.size() < 2) {
            return 0.0;
        }

        double total_speed = 0.0;
        // Bug: Using size_t for subtraction can cause underflow
        for (size_t i = 0; i < position_history_.size() - 1; i++) {
            const auto& pos1 = position_history_[i];
            const auto& pos2 = position_history_[i + 1];
            
            // Bug: Incorrect time difference calculation
            auto time_diff = pos2.getTimestamp() - pos1.getTimestamp();
            double distance = pos1.distanceTo(pos2);
            
            // Bug: Potential division by zero not handled
            total_speed += distance / time_diff.count();
        }
        
        // Bug: Incorrect average calculation
        return total_speed / position_history_.size();
    }

    // Buggy function 3: Unsafe status management
    void Equipment::setMaintenanceStatus() {
        // Bug: No mutex protection
        status_ = EquipmentStatus::Maintenance;
        
        // Bug: Clearing history without checking if equipment is moving
        if (isMoving()) {
            // Bug: Potential deadlock - calling isMoving() which also locks mutex
            clearPositionHistory();
        }
    }

    // Buggy function 4: Incorrect position validation
    bool Equipment::isPositionValid(const Position& position) const {
        // Bug: No mutex protection
        // Bug: Incorrect validation logic
        if (last_position_.has_value()) {
            // Bug: Using absolute values without considering direction
            double diff = std::abs(position.getX() - last_position_->getX()) +
                         std::abs(position.getY() - last_position_->getY());
            return diff < 1000.0;  // Arbitrary threshold
        }
        return true;  // Bug: Always returns true if no last position
    }

    // Buggy function 5: Unsafe type conversion
    EquipmentType Equipment::getTypeFromString(const std::string& type_str) {
        // Bug: No input validation
        // Bug: Case-sensitive comparison
        if (type_str == "forklift") return EquipmentType::Forklift;
        if (type_str == "crane") return EquipmentType::Crane;
        if (type_str == "bulldozer") return EquipmentType::Bulldozer;
        if (type_str == "excavator") return EquipmentType::Excavator;
        if (type_str == "truck") return EquipmentType::Truck;
        return EquipmentType::Other;  // Bug: No error handling for invalid input
    }

} // namespace equipment_tracker