#pragma once

#include <string>
#include <sstream>
#include <ctime>
#include <cmath>
#include "utils/types.h"
#include "utils/constants.h"
#include "utils/time_utils.h"

namespace equipment_tracker
{

    // Forward declare Builder class
    class PositionBuilder;

    /**
     * @brief Represents a geographic position with latitude, longitude, altitude, and timestamp
     */
    class Position
    {
    public:
        // Default constructor
        Position() = default;

        // Constructor
        Position(double latitude, double longitude, double altitude = 0.0,
                 double accuracy = DEFAULT_POSITION_ACCURACY,
                 Timestamp timestamp = getCurrentTimestamp());

        // Getters
        double getLatitude() const { return latitude_; }
        double getLongitude() const { return longitude_; }
        double getAltitude() const { return altitude_; }
        double getAccuracy() const { return accuracy_; }
        Timestamp getTimestamp() const { return timestamp_; }

        // Setters
        void setLatitude(double latitude) { latitude_ = latitude; }
        void setLongitude(double longitude) { longitude_ = longitude; }
        void setAltitude(double altitude) { altitude_ = altitude; }
        void setAccuracy(double accuracy) { accuracy_ = accuracy; }
        void setTimestamp(Timestamp timestamp) { timestamp_ = timestamp; }

        // Create a builder for fluent interface
        static PositionBuilder builder();

        // Utility methods

        /**
         * @brief Calculate distance to another position in meters using Haversine formula
         * @param other The other position
         * @return Distance in meters
         */
        double distanceTo(const Position &other) const;

        /**
         * @brief Format position as a string for debugging
         * @return Formatted string
         */
        std::string toString() const;

    private:
        double latitude_{0.0};
        double longitude_{0.0};
        double altitude_{0.0};
        double accuracy_{DEFAULT_POSITION_ACCURACY};
        Timestamp timestamp_{getCurrentTimestamp()};
    };

    /**
     * @brief Builder class for Position objects
     */
    class PositionBuilder
    {
    public:
        PositionBuilder() = default;

        PositionBuilder &withLatitude(double value)
        {
            latitude_ = value;
            return *this;
        }

        PositionBuilder &withLongitude(double value)
        {
            longitude_ = value;
            return *this;
        }

        PositionBuilder &withAltitude(double value)
        {
            altitude_ = value;
            return *this;
        }

        PositionBuilder &withAccuracy(double value)
        {
            accuracy_ = value;
            return *this;
        }

        PositionBuilder &withTimestamp(Timestamp value)
        {
            timestamp_ = value;
            return *this;
        }

        Position build() const
        {
            return Position(latitude_, longitude_, altitude_, accuracy_, timestamp_);
        }

    private:
        double latitude_{0.0};
        double longitude_{0.0};
        double altitude_{0.0};
        double accuracy_{DEFAULT_POSITION_ACCURACY};
        Timestamp timestamp_{getCurrentTimestamp()};
    };

    // Inline implementation of the static builder method
    inline PositionBuilder Position::builder()
    {
        return PositionBuilder();
    }

} // namespace equipment_tracker