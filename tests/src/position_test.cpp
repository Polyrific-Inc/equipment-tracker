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
    // Common test setup
    void SetUp() override {
        // Current time for testing
        now_ = std::chrono::system_clock::now();
    }

    std::chrono::system_clock::time_point now_;
};

TEST_F(PositionTest, ConstructorInitializesAllFields) {
    // Arrange
    double latitude = 37.7749;
    double longitude = -122.4194;
    double altitude = 10.5;
    double accuracy = 5.0;
    
    // Act
    Position position(latitude, longitude, altitude, accuracy, now_);
    
    // Assert
    EXPECT_DOUBLE_EQ(latitude, position.getLatitude());
    EXPECT_DOUBLE_EQ(longitude, position.getLongitude());
    EXPECT_DOUBLE_EQ(altitude, position.getAltitude());
    EXPECT_DOUBLE_EQ(accuracy, position.getAccuracy());
    EXPECT_EQ(now_, position.getTimestamp());
}

TEST_F(PositionTest, DistanceToSamePosition) {
    // Arrange
    Position position(37.7749, -122.4194, 10.0, 5.0, now_);
    
    // Act
    double distance = position.distanceTo(position);
    
    // Assert
    EXPECT_NEAR(0.0, distance, 0.001);
}

TEST_F(PositionTest, DistanceToNearbyPosition) {
    // Arrange
    Position position1(37.7749, -122.4194, 10.0, 5.0, now_);
    Position position2(37.7749, -122.4195, 10.0, 5.0, now_);
    
    // Act
    double distance = position1.distanceTo(position2);
    
    // Assert
    // ~0.88m at this latitude
    EXPECT_GT(distance, 0.0);
    EXPECT_LT(distance, 10.0);
}

TEST_F(PositionTest, DistanceToDifferentPosition) {
    // Arrange
    // San Francisco
    Position position1(37.7749, -122.4194, 10.0, 5.0, now_);
    // Los Angeles
    Position position2(34.0522, -118.2437, 10.0, 5.0, now_);
    
    // Act
    double distance = position1.distanceTo(position2);
    
    // Assert
    // ~559km between SF and LA
    EXPECT_NEAR(559000.0, distance, 1000.0);
}

TEST_F(PositionTest, DistanceToAntipodes) {
    // Arrange
    // Point on Earth
    Position position1(37.7749, -122.4194, 10.0, 5.0, now_);
    // Approximate antipode
    Position position2(-37.7749, 57.5806, 10.0, 5.0, now_);
    
    // Act
    double distance = position1.distanceTo(position2);
    
    // Assert
    // Should be close to half the Earth's circumference
    EXPECT_NEAR(M_PI * EARTH_RADIUS_METERS, distance, 100000.0);
}

TEST_F(PositionTest, DistanceToEquatorCrossing) {
    // Arrange
    // North of equator
    Position position1(10.0, 0.0, 10.0, 5.0, now_);
    // South of equator
    Position position2(-10.0, 0.0, 10.0, 5.0, now_);
    
    // Act
    double distance = position1.distanceTo(position2);
    
    // Assert
    // 20 degrees of latitude at the prime meridian
    double expected = (20.0 * M_PI / 180.0) * EARTH_RADIUS_METERS;
    EXPECT_NEAR(expected, distance, 1.0);
}

TEST_F(PositionTest, ToStringFormatting) {
    // Arrange
    double latitude = 37.774900;
    double longitude = -122.419400;
    double altitude = 10.50;
    double accuracy = 5.25;
    
    // Use a fixed timestamp for predictable output
    std::tm timeinfo = {};
    timeinfo.tm_year = 2023 - 1900;  // Years since 1900
    timeinfo.tm_mon = 6;             // 0-based month (July)
    timeinfo.tm_mday = 15;
    timeinfo.tm_hour = 14;
    timeinfo.tm_min = 30;
    timeinfo.tm_sec = 45;
    
    std::time_t time_c;
#ifdef _WIN32
    time_c = _mkgmtime(&timeinfo);
#else
    time_c = timegm(&timeinfo);
#endif
    std::chrono::system_clock::time_point timestamp = 
        std::chrono::system_clock::from_time_t(time_c);
    
    Position position(latitude, longitude, altitude, accuracy, timestamp);
    
    // Act
    std::string result = position.toString();
    
    // Assert
    EXPECT_THAT(result, ::testing::HasSubstr("lat=37.774900"));
    EXPECT_THAT(result, ::testing::HasSubstr("lon=-122.419400"));
    EXPECT_THAT(result, ::testing::HasSubstr("alt=10.50m"));
    EXPECT_THAT(result, ::testing::HasSubstr("acc=5.25m"));
    // Note: The exact time string might vary by platform due to timezone differences
    // So we just check for the date part which should be consistent
    EXPECT_THAT(result, ::testing::HasSubstr("2023"));
}

TEST_F(PositionTest, ToStringEdgeCases) {
    // Arrange - extreme values
    Position position1(90.0, 180.0, 9999.99, 0.01, now_);
    Position position2(-90.0, -180.0, -9999.99, 9999.99, now_);
    
    // Act
    std::string result1 = position1.toString();
    std::string result2 = position2.toString();
    
    // Assert
    EXPECT_THAT(result1, ::testing::HasSubstr("lat=90.000000"));
    EXPECT_THAT(result1, ::testing::HasSubstr("lon=180.000000"));
    EXPECT_THAT(result1, ::testing::HasSubstr("alt=9999.99m"));
    EXPECT_THAT(result1, ::testing::HasSubstr("acc=0.01m"));
    
    EXPECT_THAT(result2, ::testing::HasSubstr("lat=-90.000000"));
    EXPECT_THAT(result2, ::testing::HasSubstr("lon=-180.000000"));
    EXPECT_THAT(result2, ::testing::HasSubstr("alt=-9999.99m"));
    EXPECT_THAT(result2, ::testing::HasSubstr("acc=9999.99m"));
}

} // namespace equipment_tracker
// </test_code>