#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm> // For std::clamp
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
        // Input validation
        if (std::abs(latitude_) > 90.0 || std::abs(other.latitude_) > 90.0) {
            throw std::invalid_argument("Latitude must be between -90 and 90 degrees");
        }
        if (std::abs(longitude_) > 180.0 || std::abs(other.longitude_) > 180.0) {
            throw std::invalid_argument("Longitude must be between -180 and 180 degrees");
        }
        if (EARTH_RADIUS_METERS <= 0.0) {
            throw std::runtime_error("Earth radius must be positive");
        }

        // Early return for same position (pointer comparison)
        if (this == &other) {
            return 0.0;
        }

        // Early return for identical coordinates (using appropriate epsilon for GPS precision)
        const double epsilon = 1e-7; // ~1cm precision at equator
        if (std::abs(latitude_ - other.latitude_) < epsilon &&
            std::abs(longitude_ - other.longitude_) < epsilon) {
            return 0.0;
        }

        // Convert latitude and longitude from degrees to radians
        const double lat1_rad = latitude_ * M_PI / 180.0;
        const double lat2_rad = other.latitude_ * M_PI / 180.0;
        const double lon1_rad = longitude_ * M_PI / 180.0;
        const double lon2_rad = other.longitude_ * M_PI / 180.0;

        // Calculate differences in radians
        const double dlat = lat2_rad - lat1_rad;
        const double dlon = lon2_rad - lon1_rad;

        // Haversine formula - numerically stable implementation
        const double sin_dlat_half = std::sin(dlat * 0.5);
        const double sin_dlon_half = std::sin(dlon * 0.5);
        
        const double a = sin_dlat_half * sin_dlat_half +
                         std::cos(lat1_rad) * std::cos(lat2_rad) *
                             sin_dlon_half * sin_dlon_half;

        // Use numerically stable form of atan2 for small distances
        // This avoids the need for clamping which can introduce errors
        const double sqrt_a = std::sqrt(a);
        const double sqrt_1_minus_a = std::sqrt(1.0 - a);
        const double c = 2.0 * std::atan2(sqrt_a, sqrt_1_minus_a);

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

} // namespace equipment_tracker