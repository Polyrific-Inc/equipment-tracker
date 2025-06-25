// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/position.h"
#include <chrono>
#include <thread>
#include <cmath>

// Define M_PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace equipment_tracker {
namespace {

class PositionTest : public ::testing::Test {
protected:
    // Helper method to create a timestamp at a specific time
    static Timestamp createTimestamp(int year, int month, int day, int hour, int minute, int second) {
        std::tm timeinfo = {};
        timeinfo.tm_year = year - 1900;  // Years since 1900
        timeinfo.tm_mon = month - 1;     // Months since January (0-11)
        timeinfo.tm_mday = day;          // Day of the month (1-31)
        timeinfo.tm_hour = hour;         // Hours (0-23)
        timeinfo.tm_min = minute;        // Minutes (0-59)
        timeinfo.tm_sec = second;        // Seconds (0-59)
        
        std::time_t time_t_value;
#ifdef _WIN32
        time_t_value = _mkgmtime(&timeinfo);
#else
        timeinfo.tm_isdst = -1;  // Let the system determine DST
        time_t_value = timegm(&timeinfo);
#endif
        
        return std::chrono::system_clock::from_time_t(time_t_value);
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
    Timestamp timestamp = createTimestamp(2023, 5, 15, 10, 30, 0);
    Position position(40.7128, -74.0060, 10.5, 5.0, timestamp);
    
    EXPECT_DOUBLE_EQ(40.7128, position.getLatitude());
    EXPECT_DOUBLE_EQ(-74.0060, position.getLongitude());
    EXPECT_DOUBLE_EQ(10.5, position.getAltitude());
    EXPECT_DOUBLE_EQ(5.0, position.getAccuracy());
    EXPECT_EQ(timestamp, position.getTimestamp());
}

TEST_F(PositionTest, Setters) {
    Position position;
    
    position.setLatitude(37.7749);
    position.setLongitude(-122.4194);
    position.setAltitude(15.0);
    position.setAccuracy(3.5);
    
    Timestamp timestamp = createTimestamp(2023, 6, 20, 14, 45, 30);
    position.setTimestamp(timestamp);
    
    EXPECT_DOUBLE_EQ(37.7749, position.getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194, position.getLongitude());
    EXPECT_DOUBLE_EQ(15.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(3.5, position.getAccuracy());
    EXPECT_EQ(timestamp, position.getTimestamp());
}

TEST_F(PositionTest, Builder) {
    Timestamp timestamp = createTimestamp(2023, 7, 10, 8, 15, 45);
    
    Position position = Position::builder()
        .withLatitude(51.5074)
        .withLongitude(-0.1278)
        .withAltitude(25.0)
        .withAccuracy(1.5)
        .withTimestamp(timestamp)
        .build();
    
    EXPECT_DOUBLE_EQ(51.5074, position.getLatitude());
    EXPECT_DOUBLE_EQ(-0.1278, position.getLongitude());
    EXPECT_DOUBLE_EQ(25.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(1.5, position.getAccuracy());
    EXPECT_EQ(timestamp, position.getTimestamp());
}

TEST_F(PositionTest, DistanceToSamePoint) {
    Position position(40.7128, -74.0060);
    Position samePosition(40.7128, -74.0060);
    
    double distance = position.distanceTo(samePosition);
    EXPECT_NEAR(0.0, distance, 0.001);
}

TEST_F(PositionTest, DistanceBetweenKnownPoints) {
    // New York City coordinates
    Position nyc(40.7128, -74.0060);
    
    // Los Angeles coordinates
    Position la(34.0522, -118.2437);
    
    // Expected distance between NYC and LA is approximately 3935 km or 3935000 meters
    double distance = nyc.distanceTo(la);
    EXPECT_NEAR(3935000.0, distance, 5000.0);  // Allow 5km tolerance due to different calculation methods
}

TEST_F(PositionTest, DistanceToNorthPole) {
    Position equator(0.0, 0.0);
    Position northPole(90.0, 0.0);
    
    double distance = equator.distanceTo(northPole);
    // Distance from equator to north pole should be approximately 10000 km (1/4 of Earth's circumference)
    EXPECT_NEAR(10000000.0, distance, 50000.0);  // Allow 50km tolerance
}

TEST_F(PositionTest, ToStringFormat) {
    Timestamp timestamp = createTimestamp(2023, 8, 25, 12, 30, 45);
    Position position(37.7749, -122.4194, 15.0, 3.5, timestamp);
    
    std::string positionStr = position.toString();
    
    // Check that the string contains all the expected components
    EXPECT_THAT(positionStr, ::testing::HasSubstr("lat=37.774900"));
    EXPECT_THAT(positionStr, ::testing::HasSubstr("lon=-122.419400"));
    EXPECT_THAT(positionStr, ::testing::HasSubstr("alt=15.00m"));
    EXPECT_THAT(positionStr, ::testing::HasSubstr("acc=3.50m"));
    
    // We can't easily test the exact time string due to timezone differences,
    // but we can check that it contains a date-time format
    EXPECT_THAT(positionStr, ::testing::HasSubstr("time="));
    EXPECT_THAT(positionStr, ::testing::ContainsRegex("\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}"));
}

TEST_F(PositionTest, BuilderWithDefaultValues) {
    Position position = Position::builder().build();
    
    EXPECT_DOUBLE_EQ(0.0, position.getLatitude());
    EXPECT_DOUBLE_EQ(0.0, position.getLongitude());
    EXPECT_DOUBLE_EQ(0.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(DEFAULT_POSITION_ACCURACY, position.getAccuracy());
    // Timestamp is current time, can't easily test exact value
}

TEST_F(PositionTest, BuilderWithPartialValues) {
    Position position = Position::builder()
        .withLatitude(35.6762)
        .withLongitude(139.6503)
        .build();
    
    EXPECT_DOUBLE_EQ(35.6762, position.getLatitude());
    EXPECT_DOUBLE_EQ(139.6503, position.getLongitude());
    EXPECT_DOUBLE_EQ(0.0, position.getAltitude());  // Default value
    EXPECT_DOUBLE_EQ(DEFAULT_POSITION_ACCURACY, position.getAccuracy());  // Default value
}

TEST_F(PositionTest, DistanceWithDifferentAltitudes) {
    // Two points with same lat/long but different altitudes
    Position p1(40.7128, -74.0060, 0.0);
    Position p2(40.7128, -74.0060, 100.0);
    
    // The Haversine formula only considers surface distance, not altitude
    double distance = p1.distanceTo(p2);
    EXPECT_NEAR(0.0, distance, 0.001);
}

TEST_F(PositionTest, DistanceAtEquator) {
    // Two points on the equator, 1 degree apart
    Position p1(0.0, 0.0);
    Position p2(0.0, 1.0);
    
    double distance = p1.distanceTo(p2);
    // At the equator, 1 degree of longitude is approximately 111.32 km
    EXPECT_NEAR(111320.0, distance, 100.0);
}

TEST_F(PositionTest, DistanceNearPoles) {
    // Two points near the north pole, 1 degree apart in longitude
    Position p1(89.0, 0.0);
    Position p2(89.0, 1.0);
    
    double distance = p1.distanceTo(p2);
    // Near the poles, 1 degree of longitude is much shorter than at the equator
    EXPECT_LT(distance, 5000.0);  // Should be much less than 111 km
}

}  // namespace
}  // namespace equipment_tracker
// </test_code>