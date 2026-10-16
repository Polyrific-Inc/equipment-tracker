// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/constants.h"
#include <chrono>
#include <thread>
#include <cmath>

// Define M_PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace equipment_tracker {
namespace testing {

class PositionTest : public ::testing::Test {
protected:
    // Helper function to create a timestamp at a specific time
    static Timestamp createTimestamp(int year, int month, int day, 
                                    int hour, int minute, int second) {
        std::tm timeinfo = {};
        timeinfo.tm_year = year - 1900;  // Years since 1900
        timeinfo.tm_mon = month - 1;     // Months since January (0-11)
        timeinfo.tm_mday = day;          // Day of the month (1-31)
        timeinfo.tm_hour = hour;         // Hours (0-23)
        timeinfo.tm_min = minute;        // Minutes (0-59)
        timeinfo.tm_sec = second;        // Seconds (0-59)
        
        std::time_t time_t;
#ifdef _WIN32
        time_t = _mkgmtime(&timeinfo);
#else
        timeinfo.tm_isdst = -1;  // Let the system determine DST
        time_t = timegm(&timeinfo);
#endif
        return std::chrono::system_clock::from_time_t(time_t);
    }
};

TEST_F(PositionTest, DefaultConstructor) {
    Position position;
    EXPECT_DOUBLE_EQ(0.0, position.getLatitude());
    EXPECT_DOUBLE_EQ(0.0, position.getLongitude());
    EXPECT_DOUBLE_EQ(0.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(DEFAULT_POSITION_ACCURACY, position.getAccuracy());
    // We can't easily test the timestamp as it's set to current time
}

TEST_F(PositionTest, ParameterizedConstructor) {
    Timestamp timestamp = createTimestamp(2023, 1, 1, 12, 0, 0);
    Position position(40.7128, -74.0060, 10.5, 5.0, timestamp);
    
    EXPECT_DOUBLE_EQ(40.7128, position.getLatitude());
    EXPECT_DOUBLE_EQ(-74.0060, position.getLongitude());
    EXPECT_DOUBLE_EQ(10.5, position.getAltitude());
    EXPECT_DOUBLE_EQ(5.0, position.getAccuracy());
    EXPECT_EQ(timestamp, position.getTimestamp());
}

TEST_F(PositionTest, Setters) {
    Position position;
    
    position.setLatitude(40.7128);
    position.setLongitude(-74.0060);
    position.setAltitude(10.5);
    position.setAccuracy(5.0);
    
    Timestamp timestamp = createTimestamp(2023, 1, 1, 12, 0, 0);
    position.setTimestamp(timestamp);
    
    EXPECT_DOUBLE_EQ(40.7128, position.getLatitude());
    EXPECT_DOUBLE_EQ(-74.0060, position.getLongitude());
    EXPECT_DOUBLE_EQ(10.5, position.getAltitude());
    EXPECT_DOUBLE_EQ(5.0, position.getAccuracy());
    EXPECT_EQ(timestamp, position.getTimestamp());
}

TEST_F(PositionTest, Builder) {
    Timestamp timestamp = createTimestamp(2023, 1, 1, 12, 0, 0);
    
    Position position = Position::builder()
        .withLatitude(40.7128)
        .withLongitude(-74.0060)
        .withAltitude(10.5)
        .withAccuracy(5.0)
        .withTimestamp(timestamp)
        .build();
    
    EXPECT_DOUBLE_EQ(40.7128, position.getLatitude());
    EXPECT_DOUBLE_EQ(-74.0060, position.getLongitude());
    EXPECT_DOUBLE_EQ(10.5, position.getAltitude());
    EXPECT_DOUBLE_EQ(5.0, position.getAccuracy());
    EXPECT_EQ(timestamp, position.getTimestamp());
}

TEST_F(PositionTest, DistanceToSamePoint) {
    Position position(40.7128, -74.0060);
    Position samePosition(40.7128, -74.0060);
    
    double distance = position.distanceTo(samePosition);
    EXPECT_NEAR(0.0, distance, 0.001);
}

TEST_F(PositionTest, DistanceToDifferentPoint) {
    // New York City coordinates
    Position nyc(40.7128, -74.0060);
    
    // Los Angeles coordinates
    Position la(34.0522, -118.2437);
    
    // Expected distance between NYC and LA is approximately 3935.9 km (3935900 meters)
    double distance = nyc.distanceTo(la);
    EXPECT_NEAR(3935900.0, distance, 1000.0);  // Allow 1km tolerance due to different calculation methods
    
    // Distance should be the same in reverse
    double reverseDistance = la.distanceTo(nyc);
    EXPECT_DOUBLE_EQ(distance, reverseDistance);
}

TEST_F(PositionTest, DistanceToNearbyPoint) {
    // Two points 100 meters apart (approximately)
    Position point1(40.7128, -74.0060);
    Position point2(40.7128, -74.0071);  // ~100m west at this latitude
    
    double distance = point1.distanceTo(point2);
    EXPECT_NEAR(100.0, distance, 10.0);  // Allow 10m tolerance
}

TEST_F(PositionTest, ToStringFormat) {
    Timestamp timestamp = createTimestamp(2023, 1, 1, 12, 0, 0);
    Position position(40.7128, -74.0060, 10.5, 5.0, timestamp);
    
    std::string positionString = position.toString();
    
    // Check that the string contains all the expected parts
    EXPECT_THAT(positionString, ::testing::HasSubstr("lat=40.712800"));
    EXPECT_THAT(positionString, ::testing::HasSubstr("lon=-74.006000"));
    EXPECT_THAT(positionString, ::testing::HasSubstr("alt=10.50m"));
    EXPECT_THAT(positionString, ::testing::HasSubstr("acc=5.00m"));
    
    // We can't test the exact time format as it depends on the local timezone
    // But we can check that it contains the date
    EXPECT_THAT(positionString, ::testing::HasSubstr("2023"));
}

TEST_F(PositionTest, DistanceToEdgeCases) {
    // Test with poles
    Position northPole(90.0, 0.0);
    Position southPole(-90.0, 0.0);
    
    // Distance between poles should be approximately 20015.1 km (Earth's circumference / 2)
    double poleDistance = northPole.distanceTo(southPole);
    EXPECT_NEAR(20015100.0, poleDistance, 1000.0);  // Allow 1km tolerance
    
    // Test with international date line crossing
    Position tokyo(35.6762, 139.6503);
    Position sanFrancisco(37.7749, -122.4194);
    
    double distance = tokyo.distanceTo(sanFrancisco);
    EXPECT_GT(distance, 0.0);  // Should be a positive distance
    
    // Test with equator points
    Position equator1(0.0, 0.0);
    Position equator2(0.0, 90.0);
    
    // Distance should be 1/4 of Earth's circumference
    double equatorDistance = equator1.distanceTo(equator2);
    EXPECT_NEAR(10007550.0, equatorDistance, 1000.0);  // Allow 1km tolerance
}

} // namespace testing
} // namespace equipment_tracker
// </test_code>