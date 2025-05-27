#pragma once

#include <string>
#include <chrono>
#include <functional>

namespace equipment_tracker
{

    // Type aliases for improved semantics
    using EquipmentId = std::string;
    using Timestamp = std::chrono::system_clock::time_point;
    using PositionCallback = std::function<void(double latitude, double longitude, double altitude, Timestamp timestamp)>;

    // Enumerations
    enum class EquipmentType
    {
        Forklift,
        Crane,
        Bulldozer,
        Excavator,
        Truck,
        Other
    };

    enum class EquipmentStatus
    {
        Active,
        Inactive,
        Maintenance,
        Unknown
    };

    // Movement status enumeration for analysis
    enum class MovementStatus
    {
        Stationary,
        Moving,
        Unknown
    };

    // Movement analysis result structure
    struct MovementAnalysis
    {
        MovementStatus status;
        double currentSpeed;         // m/s
        double averageSpeed;         // m/s over time window
        double totalDistance;        // meters traveled in time window
        bool hasSignificantMovement; // moved more than threshold distance

        MovementAnalysis()
            : status(MovementStatus::Unknown),
              currentSpeed(0.0),
              averageSpeed(0.0),
              totalDistance(0.0),
              hasSignificantMovement(false) {}
    };

    // Inline utility functions that use time_utils functions
    inline double getTimeDifferenceSeconds(const Timestamp &earlier, const Timestamp &later)
    {
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(later - earlier);
        return diff.count() / 1000.0;
    }

    inline std::string timestampToString(const Timestamp &timestamp)
    {
        auto timeT = std::chrono::system_clock::to_time_t(timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      timestamp.time_since_epoch()) %
                  1000;

        char buffer[100];
        if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&timeT)))
        {
            return std::string(buffer) + "." + std::to_string(ms.count());
        }
        return "Unknown time";
    }

} // namespace equipment_tracker