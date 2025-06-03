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
        // Validate coordinate ranges using proper bounds
        if (latitude_ < -90.0 || latitude_ > 90.0 || longitude_ < -180.0 || longitude_ > 180.0 ||
            other.latitude_ < -90.0 || other.latitude_ > 90.0 || other.longitude_ < -180.0 || other.longitude_ > 180.0) {
            throw std::invalid_argument("Invalid latitude or longitude values - must be within valid Earth coordinate ranges");
        }
        
        // Check for NaN or infinite values
        if (std::isnan(latitude_) || std::isnan(longitude_) || std::isnan(other.latitude_) || std::isnan(other.longitude_) ||
            std::isinf(latitude_) || std::isinf(longitude_) || std::isinf(other.latitude_) || std::isinf(other.longitude_)) {
            throw std::invalid_argument("Coordinate values cannot be NaN or infinite");
        }
        
        // Use practical tolerance for position comparison (approximately 1 meter at equator)
        const double POSITION_TOLERANCE = 1e-5;
        if (std::abs(latitude_ - other.latitude_) < POSITION_TOLERANCE &&
            std::abs(longitude_ - other.longitude_) < POSITION_TOLERANCE) {
            return 0.0; // Bearing undefined for nearly identical positions
        }
        
        // Handle extreme latitude cases near poles where calculations become unstable
        if (std::abs(latitude_) > 89.9 || std::abs(other.latitude_) > 89.9) {
            // For positions very close to poles, use simplified bearing calculation
            if (other.longitude_ > longitude_) {
                return 90.0; // East
            } else if (other.longitude_ < longitude_) {
                return 270.0; // West
            } else {
                return (other.latitude_ > latitude_) ? 0.0 : 180.0; // North or South
            }
        }
        
        // Convert to radians for calculations
        const double DEG_TO_RAD = M_PI / 180.0;
        const double RAD_TO_DEG = 180.0 / M_PI;
        
        double lat1Rad = latitude_ * DEG_TO_RAD;
        double lon1Rad = longitude_ * DEG_TO_RAD;
        double lat2Rad = other.latitude_ * DEG_TO_RAD;
        double lon2Rad = other.longitude_ * DEG_TO_RAD;
        
        // Calculate bearing with proper bounds checking
        double deltaLon = lon2Rad - lon1Rad;
        
        // Normalize longitude difference to [-π, π] using fmod for better precision
        deltaLon = std::fmod(deltaLon + 3.0 * M_PI, 2.0 * M_PI) - M_PI;
        
        // Calculate bearing components with numerical stability
        double y = std::sin(deltaLon) * std::cos(lat2Rad);
        double x = std::cos(lat1Rad) * std::sin(lat2Rad) - 
                std::sin(lat1Rad) * std::cos(lat2Rad) * std::cos(deltaLon);
        
        // Calculate bearing using atan2 (handles all quadrants and division by zero)
        double bearingRad = std::atan2(y, x);
        
        // Convert to degrees and normalize to [0, 360)
        double bearingDeg = bearingRad * RAD_TO_DEG;
        if (bearingDeg < 0.0) {
            bearingDeg += 360.0;
        }
        
        // Final validation - should not be needed but provides safety
        if (std::isnan(bearingDeg) || std::isinf(bearingDeg)) {
            throw std::runtime_error("Bearing calculation resulted in invalid value");
        }
        
        // Ensure result is properly normalized
        bearingDeg = std::fmod(bearingDeg, 360.0);
        if (bearingDeg < 0.0) {
            bearingDeg += 360.0;
        }
        
        return bearingDeg;
    }

} // namespace equipment_tracker