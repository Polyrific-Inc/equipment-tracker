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

TEST_F(PositionTest, ConstructorSetsAllFields) {
    // Arrange
    double latitude = 37.7749;
    double longitude = -122.4194;
    double altitude = 10.5;
    double accuracy = 5.2;
    
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

TEST_F(PositionTest, DistanceToDifferentPosition) {
    // Arrange
    Position sf(37.7749, -122.4194, 10.0, 5.0, now_); // San Francisco
    Position ny(40.7128, -74.0060, 10.0, 5.0, now_);  // New York
    
    // Expected distance between SF and NY is approximately 4130 km
    double expected_distance_meters = 4130000.0;
    
    // Act
    double distance = sf.distanceTo(ny);
    
    // Assert - allow for some variation due to floating point precision
    EXPECT_NEAR(expected_distance_meters, distance, expected_distance_meters * 0.05);
}

TEST_F(PositionTest, DistanceToNearbyPosition) {
    // Arrange - two positions 100m apart (approximately)
    Position pos1(37.7749, -122.4194, 10.0, 5.0, now_);
    // Move approximately 100m north
    Position pos2(37.7758, -122.4194, 10.0, 5.0, now_);
    
    // Act
    double distance = pos1.distanceTo(pos2);
    
    // Assert - allow for some variation
    EXPECT_NEAR(100.0, distance, 5.0);
}

TEST_F(PositionTest, DistanceToAntipodes) {
    // Arrange - antipodal points (opposite sides of Earth)
    Position pos1(0.0, 0.0, 0.0, 5.0, now_);        // Point on equator at Greenwich
    Position pos2(0.0, 180.0, 0.0, 5.0, now_);      // Opposite side of Earth
    
    // Expected distance is half the circumference of Earth
    double expected_distance = M_PI * EARTH_RADIUS_METERS;
    
    // Act
    double distance = pos1.distanceTo(pos2);
    
    // Assert
    EXPECT_NEAR(expected_distance, distance, 1.0);
}

TEST_F(PositionTest, ToStringFormatsCorrectly) {
    // Arrange
    Position position(37.7749, -122.4194, 10.5, 5.2, now_);
    
    // Act
    std::string result = position.toString();
    
    // Assert - check that the string contains expected substrings
    EXPECT_THAT(result, ::testing::HasSubstr("lat=37.774900"));
    EXPECT_THAT(result, ::testing::HasSubstr("lon=-122.419400"));
    EXPECT_THAT(result, ::testing::HasSubstr("alt=10.50m"));
    EXPECT_THAT(result, ::testing::HasSubstr("acc=5.20m"));
    EXPECT_THAT(result, ::testing::HasSubstr("time="));
}

TEST_F(PositionTest, ToStringHandlesZeroValues) {
    // Arrange
    Position position(0.0, 0.0, 0.0, 0.0, now_);
    
    // Act
    std::string result = position.toString();
    
    // Assert
    EXPECT_THAT(result, ::testing::HasSubstr("lat=0.000000"));
    EXPECT_THAT(result, ::testing::HasSubstr("lon=0.000000"));
    EXPECT_THAT(result, ::testing::HasSubstr("alt=0.00m"));
    EXPECT_THAT(result, ::testing::HasSubstr("acc=0.00m"));
}

TEST_F(PositionTest, ToStringHandlesNegativeValues) {
    // Arrange
    Position position(-37.7749, -122.4194, -10.5, 5.2, now_);
    
    // Act
    std::string result = position.toString();
    
    // Assert
    EXPECT_THAT(result, ::testing::HasSubstr("lat=-37.774900"));
    EXPECT_THAT(result, ::testing::HasSubstr("lon=-122.419400"));
    EXPECT_THAT(result, ::testing::HasSubstr("alt=-10.50m"));
}

TEST_F(PositionTest, DistanceToHandlesEdgeCases) {
    // Arrange
    Position north_pole(90.0, 0.0, 0.0, 0.0, now_);
    Position south_pole(-90.0, 0.0, 0.0, 0.0, now_);
    
    // Act
    double distance = north_pole.distanceTo(south_pole);
    
    // Assert - distance should be approximately Earth's circumference / 2
    EXPECT_NEAR(M_PI * EARTH_RADIUS_METERS, distance, 1.0);
}

TEST_F(PositionTest, DistanceToHandlesSmallDistances) {
    // Arrange - two positions very close to each other
    Position pos1(37.7749, -122.4194, 10.0, 5.0, now_);
    // Move approximately 1m east
    Position pos2(37.7749, -122.41939, 10.0, 5.0, now_);
    
    // Act
    double distance = pos1.distanceTo(pos2);
    
    // Assert - distance should be very small
    EXPECT_LT(distance, 2.0);  // Less than 2 meters
    EXPECT_GT(distance, 0.0);  // Greater than 0 meters
}

} // namespace equipment_tracker
// </test_code>