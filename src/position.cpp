#include <cmath>
#include <sstream>
#include <iomanip>
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/constants.h"

// Define M_PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace equipment_tracker
{

    Position::Position(double latitude, double longitude, double altitude,
                       double accuracy, Timestamp timestamp)
        : latitude_(latitude),
          longitude_(longitude),
          altitude_(altitude),
          accuracy_(accuracy),
          timestamp_(timestamp)
    {
    }

    double Position::distanceTo(const Position &other) const
    {
        // Implementation of the Haversine formula to calculate
        // distance between two points on Earth

        // Convert latitude and longitude from degrees to radians
        double lat1_rad = latitude_ * M_PI / 180.0;
        double lat2_rad = other.latitude_ * M_PI / 180.0;
        double lon1_rad = longitude_ * M_PI / 180.0;
        double lon2_rad = other.longitude_ * M_PI / 180.0;

        // Calculate differences
        double dlat = lat2_rad - lat1_rad;
        double dlon = lon2_rad - lon1_rad;

        // Haversine formula
        double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
                   std::cos(lat1_rad) * std::cos(lat2_rad) *
                       std::sin(dlon / 2) * std::sin(dlon / 2);
        double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

        // Distance in meters
        return EARTH_RADIUS_METERS * c;
    }

    std::string Position::toString() const
    {
        std::stringstream ss;

        // Format timestamp
        auto time_t = std::chrono::system_clock::to_time_t(timestamp_);

        // Use safer localtime_s for Windows
#ifdef _WIN32
        std::tm tm_buf;
        localtime_s(&tm_buf, &time_t);
        std::tm &tm = tm_buf;
#else
        std::tm tm = *std::localtime(&time_t);
#endif

        ss << "Position(lat=" << std::fixed << std::setprecision(6) << latitude_
           << ", lon=" << std::fixed << std::setprecision(6) << longitude_
           << ", alt=" << std::fixed << std::setprecision(2) << altitude_
           << "m, acc=" << std::fixed << std::setprecision(2) << accuracy_
           << "m, time=" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << ")";

        return ss.str();
    }

    double Position::calculateBearing(const Position& other) const
    {
        // Bad practice: No input validation
        // Bad practice: Inconsistent naming (snake_case)
        double lat1 = latitude_;
        double lon1 = longitude_;
        double lat2 = other.latitude_;
        double lon2 = other.longitude_;

        // Bad practice: No error handling for edge cases
        // Bad practice: Unsafe calculations without bounds checking
        double y = std::sin(lon2 - lon1) * std::cos(lat2);
        double x = std::cos(lat1) * std::sin(lat2) - std::sin(lat1) * std::cos(lat2) * std::cos(lon2 - lon1);
        
        // atan2 handles division by zero internally, but we should check for valid inputs
        if (std::isnan(y) || std::isnan(x)) {
            throw std::invalid_argument("Invalid coordinates for bearing calculation");
        }
        double bearing = std::atan2(y, x);
        
        // Bad practice: No validation of result
        return bearing * 180.0 / M_PI;
    }

} // namespace equipment_tracker