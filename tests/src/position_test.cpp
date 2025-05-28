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
    // Current time for testing
    std::chrono::system_clock::time_point current_time = std::chrono::system_clock::now();
    
    // Sample positions for testing
    Position nyc{40.7128, -74.0060, 10.0, 5.0, current_time}; // New York
    Position london{51.5074, -0.1278, 25.0, 8.0, current_time}; // London
    Position tokyo{35.6762, 139.6503, 40.0, 3.0, current_time}; // Tokyo
    Position sydney{-33.8688, 151.2093, 30.0, 7.0, current_time}; // Sydney
    
    // Same position with different accuracy/altitude
    Position nyc_alt{40.7128, -74.0060, 20.0, 10.0, current_time}; 
    
    // Position at null island (0,0)
    Position null_island{0.0, 0.0, 0.0, 1.0, current_time};
    
    // Position at poles
    Position north_pole{90.0, 0.0, 0.0, 1.0, current_time};
    Position south_pole{-90.0, 0.0, 0.0, 1.0, current_time};
};

TEST_F(PositionTest, ConstructorSetsValues) {
    Position pos(12.34, 56.78, 100.0, 5.5, current_time);
    
    EXPECT_DOUBLE_EQ(12.34, pos.getLatitude());
    EXPECT_DOUBLE_EQ(56.78, pos.getLongitude());
    EXPECT_DOUBLE_EQ(100.0, pos.getAltitude());
    EXPECT_DOUBLE_EQ(5.5, pos.getAccuracy());
    EXPECT_EQ(current_time, pos.getTimestamp());
}

TEST_F(PositionTest, DistanceBetweenSamePositions) {
    EXPECT_DOUBLE_EQ(0.0, nyc.distanceTo(nyc));
    EXPECT_DOUBLE_EQ(0.0, london.distanceTo(london));
    EXPECT_DOUBLE_EQ(0.0, tokyo.distanceTo(tokyo));
}

TEST_F(PositionTest, DistanceBetweenDifferentPositions) {
    // Known approximate distances
    double nyc_to_london = 5570000.0; // ~5570 km
    double tokyo_to_sydney = 7920000.0; // ~7920 km
    
    // Test with 1% tolerance due to different calculation methods
    EXPECT_NEAR(nyc_to_london, nyc.distanceTo(london), nyc_to_london * 0.01);
    EXPECT_NEAR(nyc_to_london, london.distanceTo(nyc), nyc_to_london * 0.01);
    
    EXPECT_NEAR(tokyo_to_sydney, tokyo.distanceTo(sydney), tokyo_to_sydney * 0.01);
    EXPECT_NEAR(tokyo_to_sydney, sydney.distanceTo(tokyo), tokyo_to_sydney * 0.01);
}

TEST_F(PositionTest, DistanceIgnoresAltitudeAndAccuracy) {
    // Same lat/long but different altitude and accuracy
    EXPECT_DOUBLE_EQ(0.0, nyc.distanceTo(nyc_alt));
}

TEST_F(PositionTest, DistanceToPoles) {
    // Distance from null island (0,0) to North Pole should be a quarter of Earth's circumference
    double expected_distance = M_PI * EARTH_RADIUS_METERS / 2;
    EXPECT_NEAR(expected_distance, null_island.distanceTo(north_pole), expected_distance * 0.01);
    EXPECT_NEAR(expected_distance, null_island.distanceTo(south_pole), expected_distance * 0.01);
    
    // Distance between poles should be half of Earth's circumference
    double pole_to_pole = M_PI * EARTH_RADIUS_METERS;
    EXPECT_NEAR(pole_to_pole, north_pole.distanceTo(south_pole), pole_to_pole * 0.01);
}

TEST_F(PositionTest, ToStringFormat) {
    Position pos(40.7128, -74.0060, 10.5, 5.25, current_time);
    std::string str = pos.toString();
    
    // Check that the string contains the expected parts
    EXPECT_THAT(str, ::testing::HasSubstr("Position(lat=40.712800, lon=-74.006000"));
    EXPECT_THAT(str, ::testing::HasSubstr("alt=10.50m"));
    EXPECT_THAT(str, ::testing::HasSubstr("acc=5.25m"));
    EXPECT_THAT(str, ::testing::HasSubstr("time="));
}

TEST_F(PositionTest, ToStringPrecision) {
    // Test precision of floating point values
    Position pos(12.3456789, -98.7654321, 123.456, 7.890, current_time);
    std::string str = pos.toString();
    
    EXPECT_THAT(str, ::testing::HasSubstr("lat=12.345679")); // 6 decimal places
    EXPECT_THAT(str, ::testing::HasSubstr("lon=-98.765432")); // 6 decimal places
    EXPECT_THAT(str, ::testing::HasSubstr("alt=123.46")); // 2 decimal places
    EXPECT_THAT(str, ::testing::HasSubstr("acc=7.89")); // 2 decimal places
}

TEST_F(PositionTest, EdgeCaseCoordinates) {
    // Test extreme latitude values
    Position max_lat(90.0, 0.0, 0.0, 0.0, current_time);
    Position min_lat(-90.0, 0.0, 0.0, 0.0, current_time);
    
    EXPECT_DOUBLE_EQ(90.0, max_lat.getLatitude());
    EXPECT_DOUBLE_EQ(-90.0, min_lat.getLatitude());
    
    // Test longitude wrapping (these should be the same point)
    Position lon_180(0.0, 180.0, 0.0, 0.0, current_time);
    Position lon_neg_180(0.0, -180.0, 0.0, 0.0, current_time);
    
    EXPECT_NEAR(0.0, lon_180.distanceTo(lon_neg_180), 0.001);
}

} // namespace equipment_tracker
// </test_code>