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

    // Utility to get current timestamp
    Timestamp getCurrentTimestamp();

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

} // namespace equipment_tracker