// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <chrono>
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/constants.h"

// Mock the constants header if needed
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
    Timestamp timestamp = now_;

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
    Position position(37.7749, -122.4194, 10.0, 5.0, now_);

    // Act
    double distance = position.distanceTo(position);

    // Assert
    EXPECT_NEAR(0.0, distance, 1e-9);
}

TEST_F(PositionTest, DistanceToDifferentPosition) {
    // Arrange
    Position sf(37.7749, -122.4194, 10.0, 5.0, now_);
    Position ny(40.7128, -74.0060, 10.0, 5.0, now_);

    // Act
    double distance = sf.distanceTo(ny);

    // Assert - approximate distance between SF and NY is ~4130 km
    EXPECT_NEAR(4130000.0, distance, 5000.0);
}

TEST_F(PositionTest, DistanceToNearbyPosition) {
    // Arrange - positions 100m apart approximately
    Position pos1(37.7749, -122.4194, 10.0, 5.0, now_);
    Position pos2(37.7749, -122.4204, 10.0, 5.0, now_);

    // Act
    double distance = pos1.distanceTo(pos2);

    // Assert
    EXPECT_GT(distance, 0.0);
    EXPECT_LT(distance, 200.0);
}

TEST_F(PositionTest, DistanceToAntipodes) {
    // Arrange - approximately antipodal points
    Position pos1(37.7749, -122.4194, 10.0, 5.0, now_);
    Position pos2(-37.7749, 57.5806, 10.0, 5.0, now_);

    // Act
    double distance = pos1.distanceTo(pos2);

    // Assert - should be close to half the Earth's circumference
    EXPECT_NEAR(EARTH_RADIUS_METERS * M_PI, distance, EARTH_RADIUS_METERS * 0.1);
}

TEST_F(PositionTest, ToStringFormatsCorrectly) {
    // Arrange
    double latitude = 37.7749;
    double longitude = -122.4194;
    double altitude = 10.5;
    double accuracy = 5.2;
    
    // Use a fixed timestamp for predictable output
    std::tm timeinfo = {};
    timeinfo.tm_year = 2023 - 1900;
    timeinfo.tm_mon = 5;  // June (0-based)
    timeinfo.tm_mday = 15;
    timeinfo.tm_hour = 12;
    timeinfo.tm_min = 30;
    timeinfo.tm_sec = 45;
    
    std::time_t time_c;
#ifdef _WIN32
    time_c = _mkgmtime(&timeinfo);
#else
    time_c = timegm(&timeinfo);
#endif
    Timestamp timestamp = std::chrono::system_clock::from_time_t(time_c);
    
    Position position(latitude, longitude, altitude, accuracy, timestamp);

    // Act
    std::string result = position.toString();

    // Assert - using HasSubstr to avoid time zone issues
    EXPECT_THAT(result, ::testing::HasSubstr("lat=37.774900"));
    EXPECT_THAT(result, ::testing::HasSubstr("lon=-122.419400"));
    EXPECT_THAT(result, ::testing::HasSubstr("alt=10.50m"));
    EXPECT_THAT(result, ::testing::HasSubstr("acc=5.20m"));
    EXPECT_THAT(result, ::testing::HasSubstr("2023"));  // Year should be present
}

TEST_F(PositionTest, EdgeCaseLatitudeBoundaries) {
    // Arrange
    Position northPole(90.0, 0.0, 0.0, 0.0, now_);
    Position southPole(-90.0, 0.0, 0.0, 0.0, now_);

    // Act
    double distance = northPole.distanceTo(southPole);

    // Assert - should be approximately Earth's half-circumference
    EXPECT_NEAR(EARTH_RADIUS_METERS * M_PI, distance, EARTH_RADIUS_METERS * 0.01);
}

TEST_F(PositionTest, EdgeCaseLongitudeBoundaries) {
    // Arrange
    Position pos1(0.0, 180.0, 0.0, 0.0, now_);
    Position pos2(0.0, -180.0, 0.0, 0.0, now_);

    // Act
    double distance = pos1.distanceTo(pos2);

    // Assert - should be approximately 0 as these are the same point
    EXPECT_NEAR(0.0, distance, 1e-9);
}

TEST_F(PositionTest, NegativeAltitudeAndAccuracy) {
    // Arrange
    double latitude = 37.7749;
    double longitude = -122.4194;
    double altitude = -10.5;  // Below sea level
    double accuracy = 5.2;
    
    // Act
    Position position(latitude, longitude, altitude, accuracy, now_);
    std::string result = position.toString();

    // Assert
    EXPECT_DOUBLE_EQ(altitude, position.getAltitude());
    EXPECT_THAT(result, ::testing::HasSubstr("alt=-10.50m"));
}

}  // namespace equipment_tracker
// </test_code>