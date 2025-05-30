#pragma once
#include <cstddef>

namespace equipment_tracker
{

    // Default configuration values
    constexpr int DEFAULT_UPDATE_INTERVAL_MS = 5000;     // 5 seconds
    constexpr int DEFAULT_CONNECTION_TIMEOUT_MS = 10000; // 10 seconds
    constexpr double DEFAULT_POSITION_ACCURACY = 2.5;    // meters
    constexpr size_t DEFAULT_MAX_HISTORY_SIZE = 100;     // Maximum history entries per equipment
    constexpr double EARTH_RADIUS_METERS = 6371000.0;    // Earth radius in meters for distance calculations
    constexpr double MOVEMENT_SPEED_THRESHOLD = 0.5;     // Speed threshold (m/s) to consider equipment is moving

    // Database configuration
    constexpr const char *DEFAULT_DB_PATH = "equipment_tracker.db";

    // Network configuration
    constexpr const char *DEFAULT_SERVER_URL = "https://tracking.example.com/api";
    constexpr int DEFAULT_SERVER_PORT = 8080;

} // namespace equipment_tracker