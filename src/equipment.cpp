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

    double Equipment::calculateAverageSpeed() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Need at least 2 positions to calculate average speed
        if (position_history_.size() < 2)
        {
            return 0.0;
        }

        double totalDistance = 0.0;
        double totalTime = 0.0;

        // Calculate total distance and time between consecutive positions
        for (size_t i = 1; i < position_history_.size(); ++i)
        {
            const auto& currentPos = position_history_[i];
            const auto& previousPos = position_history_[i - 1];

            // Calculate time difference in seconds
            auto timeDiff = std::chrono::duration_cast<std::chrono::seconds>(
                currentPos.getTimestamp() - previousPos.getTimestamp()).count();

            // Skip if time difference is too small
            if (timeDiff < 1)
            {
                continue;
            }

            // Calculate distance between positions
            double distance = currentPos.distanceTo(previousPos);
            
            totalDistance += distance;
            totalTime += timeDiff;
        }

        // Avoid division by zero
        if (totalTime == 0.0)
        {
            return 0.0;
        }

        // Return average speed in meters per second
        return totalDistance / totalTime;
    }

} // namespace equipment_tracker