#include <cmath>
#include <sstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
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
        // Input validation for coordinates
        if (!isValidLatitude(latitude_) || !isValidLongitude(longitude_) ||
            !isValidLatitude(other.latitude_) || !isValidLongitude(other.longitude_)) {
            throw std::invalid_argument("Invalid latitude or longitude values");
        }
        
        // Check for identical positions
        if (std::abs(latitude_ - other.latitude_) < std::numeric_limits<double>::epsilon() &&
            std::abs(longitude_ - other.longitude_) < std::numeric_limits<double>::epsilon()) {
            return 0.0; // Bearing is undefined for identical positions, return 0
        }
        
        // Convert to radians for calculations
        double lat1Rad = latitude_ * M_PI / 180.0;
        double lon1Rad = longitude_ * M_PI / 180.0;
        double lat2Rad = other.latitude_ * M_PI / 180.0;
        double lon2Rad = other.longitude_ * M_PI / 180.0;
        
        // Validate converted values
        if (!std::isfinite(lat1Rad) || !std::isfinite(lon1Rad) ||
            !std::isfinite(lat2Rad) || !std::isfinite(lon2Rad)) {
            throw std::runtime_error("Coordinate conversion resulted in invalid values");
        }
        
        // Calculate bearing with bounds checking
        double deltaLon = lon2Rad - lon1Rad;
        
        // Normalize longitude difference to [-π, π]
        while (deltaLon > M_PI) deltaLon -= 2.0 * M_PI;
        while (deltaLon < -M_PI) deltaLon += 2.0 * M_PI;
        
        double y = std::sin(deltaLon) * std::cos(lat2Rad);
        double x = std::cos(lat1Rad) * std::sin(lat2Rad) - 
                   std::sin(lat1Rad) * std::cos(lat2Rad) * std::cos(deltaLon);
        
        // Check for numerical stability
        if (!std::isfinite(x) || !std::isfinite(y)) {
            throw std::runtime_error("Bearing calculation resulted in invalid intermediate values");
        }
        
        // Calculate bearing using atan2 (handles division by zero automatically)
        double bearingRad = std::atan2(y, x);
        
        // Validate atan2 result
        if (!std::isfinite(bearingRad)) {
            throw std::runtime_error("Bearing calculation failed");
        }
        
        // Convert to degrees and normalize to [0, 360)
        double bearingDeg = bearingRad * 180.0 / M_PI;
        if (bearingDeg < 0.0) {
            bearingDeg += 360.0;
        }
        
        // Final validation
        if (bearingDeg < 0.0 || bearingDeg >= 360.0 || !std::isfinite(bearingDeg)) {
            throw std::runtime_error("Invalid bearing result");
        }
        
        return bearingDeg;
    }

} // namespace equipment_tracker