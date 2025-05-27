#pragma once

namespace equipment_tracker
{

    // Default configuration values
    constexpr int DEFAULT_UPDATE_INTERVAL_MS = 5000;     // 5 seconds
    constexpr int DEFAULT_CONNECTION_TIMEOUT_MS = 10000; // 10 seconds
    constexpr double DEFAULT_POSITION_ACCURACY = 2.5;    // meters
    constexpr size_t DEFAULT_MAX_HISTORY_SIZE = 100;     // Maximum history entries per equipment
    constexpr double EARTH_RADIUS_METERS = 6371000.0;    // Earth radius in meters for distance calculations

    // Legacy movement threshold - keeping for backward compatibility
    constexpr double MOVEMENT_SPEED_THRESHOLD = 0.5; // Speed threshold (m/s) to consider equipment is moving

    // Enhanced movement detection constants
    constexpr double DEFAULT_MIN_MOVEMENT_DISTANCE = 2.0;         // meters - minimum distance to consider as movement
    constexpr double DEFAULT_SIGNIFICANT_MOVEMENT_DISTANCE = 5.0; // meters - significant movement threshold
    constexpr int DEFAULT_MOVEMENT_TIME_WINDOW_SECONDS = 10;      // seconds - time window for movement analysis
    constexpr int DEFAULT_STATIONARY_TIME_WINDOW_SECONDS = 30;    // seconds - time window for stationary analysis
    constexpr size_t MIN_POSITIONS_FOR_MOVEMENT = 2;              // minimum positions needed for movement detection

    // Speed-related constants
    constexpr double MAX_REASONABLE_SPEED = 50.0;      // m/s (180 km/h) - filter out GPS errors
    constexpr double MIN_TIME_BETWEEN_POSITIONS = 0.1; // seconds - minimum time for speed calculation

    // GPS accuracy thresholds
    constexpr double HIGH_ACCURACY_THRESHOLD = 3.0;    // meters
    constexpr double MEDIUM_ACCURACY_THRESHOLD = 10.0; // meters
    constexpr double LOW_ACCURACY_THRESHOLD = 50.0;    // meters

    // Database configuration
    constexpr const char *DEFAULT_DB_PATH = "equipment_tracker.db";

    // Network configuration
    constexpr const char *DEFAULT_SERVER_URL = "https://tracking.example.com/api";
    constexpr int DEFAULT_SERVER_PORT = 8080;

} // namespace equipment_tracker