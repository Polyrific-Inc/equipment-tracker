// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <chrono>
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/constants.h"

// Mock the constants if needed
#ifndef EARTH_RADIUS_METERS
#define EARTH_RADIUS_METERS 6371000.0
#endif

// Define M_PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace equipment_tracker {

class PositionTest : public ::testing::Test {
protected:
    // Helper method to create a timestamp for testing
    std::chrono::system_clock::time_point createTimestamp(int year, int month, int day, 
                                                         int hour, int minute, int second) {
        std::tm timeinfo = {};
        timeinfo.tm_year = year - 1900;  // Years since 1900
        timeinfo.tm_mon = month - 1;     // Months since January (0-11)
        timeinfo.tm_mday = day;          // Day of the month (1-31)
        timeinfo.tm_hour = hour;         // Hours (0-23)
        timeinfo.tm_min = minute;        // Minutes (0-59)
        timeinfo.tm_sec = second;        // Seconds (0-59)
        timeinfo.tm_isdst = -1;          // Determine DST automatically

        time_t time_c;
#ifdef _WIN32
        time_c = _mkgmtime(&timeinfo);
#else
        time_c = timegm(&timeinfo);
#endif
        return std::chrono::system_clock::from_time_t(time_c);
    }
};

TEST_F(PositionTest, ConstructorInitializesAllFields) {
    // Arrange
    double latitude = 37.7749;
    double longitude = -122.4194;
    double altitude = 10.5;
    double accuracy = 5.0;
    auto timestamp = std::chrono::system_clock::now();

    // Act
    Position position(latitude, longitude, altitude, accuracy, timestamp);

    // Assert
    EXPECT_DOUBLE_EQ(latitude, position.getLatitude());
    EXPECT_DOUBLE_EQ(longitude, position.getLongitude());
    EXPECT_DOUBLE_EQ(altitude, position.getAltitude());
    EXPECT_DOUBLE_EQ(accuracy, position.getAccuracy());
    EXPECT_EQ(timestamp, position.getTimestamp());
}

TEST_F(PositionTest, DistanceToSamePosition) {
    // Arrange
    Position position(37.7749, -122.4194, 10.5, 5.0, std::chrono::system_clock::now());

    // Act
    double distance = position.distanceTo(position);

    // Assert
    EXPECT_NEAR(0.0, distance, 1e-9);
}

TEST_F(PositionTest, DistanceToDifferentPosition) {
    // Arrange
    Position sf(37.7749, -122.4194, 10.5, 5.0, std::chrono::system_clock::now());
    Position ny(40.7128, -74.0060, 10.0, 5.0, std::chrono::system_clock::now());

    // Act
    double distance = sf.distanceTo(ny);

    // Assert - approximate distance between SF and NY is ~4130 km
    EXPECT_NEAR(4130000.0, distance, 10000.0);
}

TEST_F(PositionTest, DistanceToNearbyPosition) {
    // Arrange - two positions 100 meters apart
    Position pos1(37.7749, -122.4194, 10.5, 5.0, std::chrono::system_clock::now());
    
    // Move approximately 100 meters north
    Position pos2(37.7758, -122.4194, 10.5, 5.0, std::chrono::system_clock::now());

    // Act
    double distance = pos1.distanceTo(pos2);

    // Assert
    EXPECT_NEAR(100.0, distance, 5.0);
}

TEST_F(PositionTest, DistanceToAntipodes) {
    // Arrange - antipodal points (opposite sides of Earth)
    Position pos1(0.0, 0.0, 0.0, 5.0, std::chrono::system_clock::now());
    Position pos2(0.0, 180.0, 0.0, 5.0, std::chrono::system_clock::now());

    // Act
    double distance = pos1.distanceTo(pos2);

    // Assert - should be approximately half the Earth's circumference
    EXPECT_NEAR(M_PI * EARTH_RADIUS_METERS, distance, 1.0);
}

TEST_F(PositionTest, ToStringFormatsCorrectly) {
    // Arrange
    auto timestamp = createTimestamp(2023, 5, 15, 14, 30, 45);
    Position position(37.774900, -122.419400, 10.50, 5.25, timestamp);

    // Act
    std::string result = position.toString();

    // Assert
    EXPECT_THAT(result, ::testing::HasSubstr("lat=37.774900"));
    EXPECT_THAT(result, ::testing::HasSubstr("lon=-122.419400"));
    EXPECT_THAT(result, ::testing::HasSubstr("alt=10.50m"));
    EXPECT_THAT(result, ::testing::HasSubstr("acc=5.25m"));
    
    // Note: We can't test the exact time string since it depends on the local timezone
    // But we can check that a time string is present
    EXPECT_THAT(result, ::testing::HasSubstr("time="));
}

TEST_F(PositionTest, DistanceToEdgeCases) {
    auto now = std::chrono::system_clock::now();
    
    // Test with extreme latitudes
    Position northPole(90.0, 0.0, 0.0, 1.0, now);
    Position southPole(-90.0, 0.0, 0.0, 1.0, now);
    
    double polarDistance = northPole.distanceTo(southPole);
    EXPECT_NEAR(M_PI * EARTH_RADIUS_METERS, polarDistance, 1.0);
    
    // Test with positions at the same latitude but different longitudes
    Position pos1(0.0, 0.0, 0.0, 1.0, now);
    Position pos2(0.0, 90.0, 0.0, 1.0, now);
    
    double quarterEarthDistance = pos1.distanceTo(pos2);
    EXPECT_NEAR(M_PI * EARTH_RADIUS_METERS / 2.0, quarterEarthDistance, 1.0);
}

TEST_F(PositionTest, DistanceIsSymmetric) {
    // Arrange
    Position sf(37.7749, -122.4194, 10.5, 5.0, std::chrono::system_clock::now());
    Position ny(40.7128, -74.0060, 10.0, 5.0, std::chrono::system_clock::now());

    // Act
    double distance1 = sf.distanceTo(ny);
    double distance2 = ny.distanceTo(sf);

    // Assert
    EXPECT_DOUBLE_EQ(distance1, distance2);
}

} // namespace equipment_tracker
// </test_code>