#include <sstream>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/utils/constants.h"
#include "equipment_tracker/utils/time_utils.h" // For getCurrentTimestamp

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
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = status;
    }

    void Equipment::setName(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(mutex_);
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
        last_position_.reset();
    }

    // Legacy movement detection method (backward compatibility)
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

    // Enhanced movement detection methods
    bool Equipment::isMovingEnhanced(double distanceThreshold, std::chrono::seconds timeWindow) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Need at least 2 positions to determine movement
        if (position_history_.size() < MIN_POSITIONS_FOR_MOVEMENT)
        {
            return false;
        }

        // Get positions within the time window
        auto recentPositions = getPositionsInTimeWindow(timeWindow);

        // Need at least 2 recent positions
        if (recentPositions.size() < MIN_POSITIONS_FOR_MOVEMENT)
        {
            return false;
        }

        // Check if any recent positions show significant movement
        const Position &referencePos = recentPositions.front();

        for (size_t i = 1; i < recentPositions.size(); ++i)
        {
            double distance = referencePos.distanceTo(recentPositions[i]);
            if (distance >= distanceThreshold)
            {
                return true;
            }
        }

        return false;
    }

    double Equipment::getCurrentSpeed() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (position_history_.size() < 2)
        {
            return 0.0;
        }

        // Calculate speed based on last two positions
        const Position &current = position_history_.back();
        const Position &previous = position_history_[position_history_.size() - 2];

        double distance = previous.distanceTo(current);
        double timeDiff = getTimeDifferenceSeconds(previous.getTimestamp(), current.getTimestamp());

        if (timeDiff < MIN_TIME_BETWEEN_POSITIONS)
        {
            return 0.0;
        }

        double speed = distance / timeDiff; // m/s

        // Filter out unreasonable speeds (likely GPS errors)
        return isReasonableSpeed(speed) ? speed : 0.0;
    }

    double Equipment::getAverageSpeed(std::chrono::minutes timeWindow) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (position_history_.size() < 2)
        {
            return 0.0;
        }

        auto timeWindowSeconds = std::chrono::duration_cast<std::chrono::seconds>(timeWindow);
        auto recentPositions = getPositionsInTimeWindow(timeWindowSeconds);

        if (recentPositions.size() < 2)
        {
            return 0.0;
        }

        // Calculate total distance and time
        double totalDistance = 0.0;
        double totalTime = 0.0;

        for (size_t i = 1; i < recentPositions.size(); ++i)
        {
            totalDistance += recentPositions[i - 1].distanceTo(recentPositions[i]);
            totalTime += getTimeDifferenceSeconds(
                recentPositions[i - 1].getTimestamp(),
                recentPositions[i].getTimestamp());
        }

        if (totalTime < MIN_TIME_BETWEEN_POSITIONS)
        {
            return 0.0;
        }

        double avgSpeed = totalDistance / totalTime;

        // Filter out unreasonable speeds
        return isReasonableSpeed(avgSpeed) ? avgSpeed : 0.0;
    }

    bool Equipment::hasMovedSignificantly(std::chrono::minutes timeWindow, double minimumDistance) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (position_history_.empty())
        {
            return false;
        }

        auto timeWindowSeconds = std::chrono::duration_cast<std::chrono::seconds>(timeWindow);
        auto recentPositions = getPositionsInTimeWindow(timeWindowSeconds);

        if (recentPositions.size() < 2)
        {
            return false;
        }

        // Find the earliest and latest positions within the time window
        const Position &earliestPos = recentPositions.front();
        const Position &latestPos = recentPositions.back();

        double totalMovement = earliestPos.distanceTo(latestPos);
        return totalMovement >= minimumDistance;
    }

    double Equipment::getTotalDistanceTraveled(std::chrono::minutes timeWindow) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (position_history_.size() < 2)
        {
            return 0.0;
        }

        auto timeWindowSeconds = std::chrono::duration_cast<std::chrono::seconds>(timeWindow);
        auto recentPositions = getPositionsInTimeWindow(timeWindowSeconds);

        if (recentPositions.size() < 2)
        {
            return 0.0;
        }

        double totalDistance = 0.0;
        for (size_t i = 1; i < recentPositions.size(); ++i)
        {
            totalDistance += recentPositions[i - 1].distanceTo(recentPositions[i]);
        }

        return totalDistance;
    }

    bool Equipment::isStationary(double distanceThreshold, std::chrono::seconds timeWindow) const
    {
        return !isMovingEnhanced(distanceThreshold, timeWindow);
    }

    MovementAnalysis Equipment::getMovementAnalysis(std::chrono::minutes timeWindow) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        MovementAnalysis analysis;

        if (position_history_.size() < 2)
        {
            return analysis; // Returns default values (all zeros, Unknown status)
        }

        // Calculate current speed
        analysis.currentSpeed = getCurrentSpeed();

        // Calculate average speed
        analysis.averageSpeed = getAverageSpeed(timeWindow);

        // Calculate total distance
        analysis.totalDistance = getTotalDistanceTraveled(timeWindow);

        // Check for significant movement
        analysis.hasSignificantMovement = hasMovedSignificantly(timeWindow);

        // Determine movement status
        if (isMovingEnhanced())
        {
            analysis.status = MovementStatus::Moving;
        }
        else if (isStationary())
        {
            analysis.status = MovementStatus::Stationary;
        }
        else
        {
            analysis.status = MovementStatus::Unknown;
        }

        return analysis;
    }

    std::string Equipment::toString() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

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

        // Add movement information if available
        if (last_position_ && position_history_.size() >= 2)
        {
            ss << ", currentSpeed=" << std::fixed << std::setprecision(2) << getCurrentSpeed() << "m/s";
            ss << ", moving=" << (isMovingEnhanced() ? "yes" : "no");
            ss << ", legacy_moving=" << (isMoving() ? "yes" : "no"); // Show both for comparison
        }

        ss << ", historySize=" << position_history_.size();
        ss << ")";

        return ss.str();
    }

    // Private helper methods

    std::vector<Position> Equipment::getPositionsInTimeWindow(std::chrono::seconds timeWindow) const
    {
        // Note: mutex is already locked by calling methods

        if (position_history_.empty())
        {
            return {};
        }

        auto now = getCurrentTimestamp();
        auto cutoffTime = now - timeWindow;

        std::vector<Position> recentPositions;
        for (const auto &pos : position_history_)
        {
            if (pos.getTimestamp() >= cutoffTime)
            {
                recentPositions.push_back(pos);
            }
        }

        // Sort by timestamp to ensure chronological order
        std::sort(recentPositions.begin(), recentPositions.end(),
                  [](const Position &a, const Position &b)
                  {
                      return a.getTimestamp() < b.getTimestamp();
                  });

        return recentPositions;
    }

    double Equipment::getTimeDifferenceSeconds(Timestamp earlier, Timestamp later) const
    {
        // Use the time_utils function instead of duplicating the logic
        return static_cast<double>(timestampDiffSeconds(later, earlier));
    }

    bool Equipment::isReasonableSpeed(double speed) const
    {
        return speed >= 0.0 && speed <= MAX_REASONABLE_SPEED;
    }

} // namespace equipment_tracker