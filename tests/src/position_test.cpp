// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <chrono>
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/constants.h"

// Mock the constants if not available in the test environment
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
    // Common timestamp for testing
    Timestamp GetTestTimestamp() {
        // Create a fixed timestamp for testing (2023-01-01 12:00:00)
        std::tm timeinfo = {};
        timeinfo.tm_year = 2023 - 1900;
        timeinfo.tm_mon = 0;
        timeinfo.tm_mday = 1;
        timeinfo.tm_hour = 12;
        timeinfo.tm_min = 0;
        timeinfo.tm_sec = 0;

        // Convert to time_t, handling platform differences
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
    double lat = 37.7749;
    double lon = -122.4194;
    double alt = 10.5;
    double acc = 5.0;
    Timestamp ts = GetTestTimestamp();

    Position pos(lat, lon, alt, acc, ts);

    // We can only test public interface, so we'll use toString() to verify
    std::string posStr = pos.toString();
    EXPECT_THAT(posStr, ::testing::HasSubstr("lat=37.774900"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("lon=-122.419400"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("alt=10.50m"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("acc=5.00m"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("2023-01-01"));
}

TEST_F(PositionTest, DistanceToSamePosition) {
    Position pos(40.7128, -74.0060, 10.0, 5.0, GetTestTimestamp());
    
    double distance = pos.distanceTo(pos);
    
    // Distance to self should be very close to zero
    EXPECT_NEAR(distance, 0.0, 1e-10);
}

TEST_F(PositionTest, DistanceBetweenKnownPoints) {
    // New York City coordinates
    Position nyc(40.7128, -74.0060, 10.0, 5.0, GetTestTimestamp());
    
    // Los Angeles coordinates
    Position la(34.0522, -118.2437, 20.0, 5.0, GetTestTimestamp());
    
    double distance = nyc.distanceTo(la);
    
    // The distance between NYC and LA is approximately 3935-3945 km
    // Converting to meters and allowing for some calculation differences
    double expectedDistance = 3940000.0;
    EXPECT_NEAR(distance, expectedDistance, 10000.0);
}

TEST_F(PositionTest, DistanceIsCommutative) {
    Position pos1(40.7128, -74.0060, 10.0, 5.0, GetTestTimestamp());
    Position pos2(34.0522, -118.2437, 20.0, 5.0, GetTestTimestamp());
    
    double distance1to2 = pos1.distanceTo(pos2);
    double distance2to1 = pos2.distanceTo(pos1);
    
    EXPECT_NEAR(distance1to2, distance2to1, 1e-10);
}

TEST_F(PositionTest, DistanceWithExtremeCoordinates) {
    // North Pole
    Position northPole(90.0, 0.0, 0.0, 5.0, GetTestTimestamp());
    
    // South Pole
    Position southPole(-90.0, 0.0, 0.0, 5.0, GetTestTimestamp());
    
    double distance = northPole.distanceTo(southPole);
    
    // Distance should be approximately half the Earth's circumference
    double expectedDistance = M_PI * EARTH_RADIUS_METERS;
    EXPECT_NEAR(distance, expectedDistance, 1.0);
}

TEST_F(PositionTest, ToStringFormatsCorrectly) {
    Position pos(37.7749, -122.4194, 10.5, 5.0, GetTestTimestamp());
    
    std::string posStr = pos.toString();
    
    // Check format of each component
    EXPECT_THAT(posStr, ::testing::HasSubstr("Position("));
    EXPECT_THAT(posStr, ::testing::HasSubstr("lat=37.774900"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("lon=-122.419400"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("alt=10.50m"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("acc=5.00m"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("time=2023-01-01 12:00:00"));
    EXPECT_THAT(posStr, ::testing::HasSubstr(")"));
}

TEST_F(PositionTest, ToStringHandlesNegativeValues) {
    Position pos(-37.7749, -122.4194, -10.5, 5.0, GetTestTimestamp());
    
    std::string posStr = pos.toString();
    
    EXPECT_THAT(posStr, ::testing::HasSubstr("lat=-37.774900"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("lon=-122.419400"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("alt=-10.50m"));
}

TEST_F(PositionTest, ToStringHandlesZeroValues) {
    Position pos(0.0, 0.0, 0.0, 0.0, GetTestTimestamp());
    
    std::string posStr = pos.toString();
    
    EXPECT_THAT(posStr, ::testing::HasSubstr("lat=0.000000"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("lon=0.000000"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("alt=0.00m"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("acc=0.00m"));
}

} // namespace equipment_tracker
// </test_code>