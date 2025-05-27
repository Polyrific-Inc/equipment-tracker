#pragma once

#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <chrono>
#include "utils/types.h"
#include "utils/time_utils.h"
#include "position.h"

namespace equipment_tracker
{

    /**
     * @brief Represents a piece of heavy equipment with tracking capabilities
     */
    class Equipment
    {
    public:
        // Constructor
        Equipment(EquipmentId id, EquipmentType type, std::string name);

        // Rule of five (modern C++ practice)
        Equipment(const Equipment &) = delete;
        Equipment &operator=(const Equipment &) = delete;
        Equipment(Equipment &&other) noexcept;
        Equipment &operator=(Equipment &&other) noexcept;
        virtual ~Equipment() = default;

        // Getters
        const EquipmentId &getId() const { return id_; }
        EquipmentType getType() const { return type_; }
        const std::string &getName() const { return name_; }
        EquipmentStatus getStatus() const { return status_; }

        // Thread-safe position access
        std::optional<Position> getLastPosition() const;

        // Setters
        void setStatus(EquipmentStatus status);
        void setName(const std::string &name);
        void setLastPosition(const Position &position);

        // Position history management
        void recordPosition(const Position &position);
        std::vector<Position> getPositionHistory() const;
        void clearPositionHistory();

        // Legacy movement detection method (backward compatibility)
        bool isMoving() const;

        // Enhanced movement detection methods with configurable parameters

        /**
         * @brief Check if equipment is currently moving with enhanced logic
         * @param distanceThreshold Minimum distance in meters to consider as movement
         * @param timeWindow Time window to analyze recent positions
         * @return True if equipment is moving, false otherwise
         */
        bool isMovingEnhanced(double distanceThreshold = DEFAULT_MIN_MOVEMENT_DISTANCE,
                              std::chrono::seconds timeWindow = std::chrono::seconds(DEFAULT_MOVEMENT_TIME_WINDOW_SECONDS)) const;

        /**
         * @brief Calculate current speed based on last two positions
         * @return Speed in meters per second, 0.0 if insufficient data
         */
        double getCurrentSpeed() const;

        /**
         * @brief Calculate average speed over a time window
         * @param timeWindow Time window to calculate average over (default: 5 minutes)
         * @return Average speed in meters per second
         */
        double getAverageSpeed(std::chrono::minutes timeWindow = std::chrono::minutes(5)) const;

        /**
         * @brief Check if equipment has moved significantly within a time window
         * @param timeWindow Time window to check (default: 1 minute)
         * @param minimumDistance Minimum total distance to consider significant
         * @return True if equipment has moved significantly, false otherwise
         */
        bool hasMovedSignificantly(std::chrono::minutes timeWindow = std::chrono::minutes(1),
                                   double minimumDistance = DEFAULT_SIGNIFICANT_MOVEMENT_DISTANCE) const;

        /**
         * @brief Get the total distance traveled within a time window
         * @param timeWindow Time window to calculate distance over
         * @return Total distance in meters
         */
        double getTotalDistanceTraveled(std::chrono::minutes timeWindow = std::chrono::minutes(5)) const;

        /**
         * @brief Check if equipment is stationary (opposite of moving)
         * @param distanceThreshold Minimum distance threshold
         * @param timeWindow Time window to analyze
         * @return True if equipment is stationary, false otherwise
         */
        bool isStationary(double distanceThreshold = DEFAULT_MIN_MOVEMENT_DISTANCE,
                          std::chrono::seconds timeWindow = std::chrono::seconds(DEFAULT_STATIONARY_TIME_WINDOW_SECONDS)) const;

        /**
         * @brief Get comprehensive movement analysis
         * @param timeWindow Time window for analysis (default: 5 minutes)
         * @return MovementAnalysis struct with detailed movement information
         */
        MovementAnalysis getMovementAnalysis(std::chrono::minutes timeWindow = std::chrono::minutes(5)) const;

        // Utility methods
        std::string toString() const;

    private:
        // Core equipment data
        EquipmentId id_;
        EquipmentType type_;
        std::string name_;
        EquipmentStatus status_;
        std::optional<Position> last_position_;
        std::vector<Position> position_history_;
        size_t max_history_size_{DEFAULT_MAX_HISTORY_SIZE};
        mutable std::mutex mutex_; // For thread safety

        // Movement detection constants (using values from constants.h)
        static constexpr double DEFAULT_MIN_MOVEMENT_DISTANCE = 2.0;         // meters
        static constexpr double DEFAULT_SIGNIFICANT_MOVEMENT_DISTANCE = 5.0; // meters
        static constexpr int DEFAULT_MOVEMENT_TIME_WINDOW_SECONDS = 10;      // seconds
        static constexpr int DEFAULT_STATIONARY_TIME_WINDOW_SECONDS = 30;    // seconds
        static constexpr size_t MIN_POSITIONS_FOR_MOVEMENT = 2;

        // Private helper methods for movement analysis

        /**
         * @brief Get positions within a specific time window
         * @param timeWindow Time window to filter positions
         * @return Vector of positions within the time window
         */
        std::vector<Position> getPositionsInTimeWindow(std::chrono::seconds timeWindow) const;

        /**
         * @brief Calculate time difference between two timestamps
         * @param earlier Earlier timestamp
         * @param later Later timestamp
         * @return Time difference in seconds
         */
        double getTimeDifferenceSeconds(Timestamp earlier, Timestamp later) const;

        /**
         * @brief Filter out unreasonable speed values (GPS errors)
         * @param speed Speed to validate
         * @return True if speed is reasonable, false otherwise
         */
        bool isReasonableSpeed(double speed) const;
    };

} // namespace equipment_tracker