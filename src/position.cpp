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
        // Basic coordinate validation
        if (std::abs(latitude_) > 90.0 || std::abs(longitude_) > 180.0 ||
            std::abs(other.latitude_) > 90.0 || std::abs(other.longitude_) > 180.0) {
            throw std::invalid_argument("Invalid latitude or longitude values");
        }
        
        // Check for identical positions with practical tolerance
        constexpr double POSITION_TOLERANCE = 1e-9; // ~0.1mm precision
        if (std::abs(latitude_ - other.latitude_) < POSITION_TOLERANCE &&
            std::abs(longitude_ - other.longitude_) < POSITION_TOLERANCE) {
            return 0.0; // Bearing is undefined for identical positions
        }
        
        // Convert to radians for calculations
        const double lat1Rad = latitude_ * M_PI / 180.0;
        const double lon1Rad = longitude_ * M_PI / 180.0;
        const double lat2Rad = other.latitude_ * M_PI / 180.0;
        const double lon2Rad = other.longitude_ * M_PI / 180.0;
        
        // Calculate bearing with proper longitude difference handling
        double deltaLon = lon2Rad - lon1Rad;
        
        // Normalize longitude difference to [-π, π] efficiently
        if (deltaLon > M_PI) {
            deltaLon -= 2.0 * M_PI;
        } else if (deltaLon < -M_PI) {
            deltaLon += 2.0 * M_PI;
        }
        
        // Calculate bearing components
        const double y = std::sin(deltaLon) * std::cos(lat2Rad);
        const double x = std::cos(lat1Rad) * std::sin(lat2Rad) - 
                         std::sin(lat1Rad) * std::cos(lat2Rad) * std::cos(deltaLon);
        
        // Calculate bearing using atan2 (handles all quadrants and division by zero)
        double bearingRad = std::atan2(y, x);
        
        // Convert to degrees and normalize to [0, 360)
        double bearingDeg = bearingRad * 180.0 / M_PI;
        if (bearingDeg < 0.0) {
            bearingDeg += 360.0;
        }
        
        // Ensure result is in valid range (defensive programming)
        if (bearingDeg >= 360.0) {
            bearingDeg = std::fmod(bearingDeg, 360.0);
        }
        
        return bearingDeg;
    }

} // namespace equipment_tracker