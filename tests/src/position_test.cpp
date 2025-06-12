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
namespace {

class PositionTest : public ::testing::Test {
protected:
    // Helper method to create a timestamp for a specific time
    static Timestamp createTimestamp(int year, int month, int day, 
                                    int hour, int minute, int second) {
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
    Position pos;
    EXPECT_DOUBLE_EQ(0.0, pos.getLatitude());
    EXPECT_DOUBLE_EQ(0.0, pos.getLongitude());
    EXPECT_DOUBLE_EQ(0.0, pos.getAltitude());
    EXPECT_DOUBLE_EQ(DEFAULT_POSITION_ACCURACY, pos.getAccuracy());
    // We can't easily test the timestamp as it's set to current time
}

TEST_F(PositionTest, ParameterizedConstructor) {
    Timestamp ts = createTimestamp(2023, 1, 1, 12, 0, 0);
    Position pos(40.7128, -74.0060, 10.5, 5.0, ts);
    
    EXPECT_DOUBLE_EQ(40.7128, pos.getLatitude());
    EXPECT_DOUBLE_EQ(-74.0060, pos.getLongitude());
    EXPECT_DOUBLE_EQ(10.5, pos.getAltitude());
    EXPECT_DOUBLE_EQ(5.0, pos.getAccuracy());
    EXPECT_EQ(ts, pos.getTimestamp());
}

TEST_F(PositionTest, SettersAndGetters) {
    Position pos;
    
    pos.setLatitude(37.7749);
    pos.setLongitude(-122.4194);
    pos.setAltitude(15.2);
    pos.setAccuracy(3.5);
    
    Timestamp ts = createTimestamp(2023, 2, 15, 14, 30, 0);
    pos.setTimestamp(ts);
    
    EXPECT_DOUBLE_EQ(37.7749, pos.getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194, pos.getLongitude());
    EXPECT_DOUBLE_EQ(15.2, pos.getAltitude());
    EXPECT_DOUBLE_EQ(3.5, pos.getAccuracy());
    EXPECT_EQ(ts, pos.getTimestamp());
}

TEST_F(PositionTest, Builder) {
    Timestamp ts = createTimestamp(2023, 3, 10, 8, 45, 30);
    
    Position pos = Position::builder()
        .withLatitude(51.5074)
        .withLongitude(-0.1278)
        .withAltitude(25.0)
        .withAccuracy(1.5)
        .withTimestamp(ts)
        .build();
    
    EXPECT_DOUBLE_EQ(51.5074, pos.getLatitude());
    EXPECT_DOUBLE_EQ(-0.1278, pos.getLongitude());
    EXPECT_DOUBLE_EQ(25.0, pos.getAltitude());
    EXPECT_DOUBLE_EQ(1.5, pos.getAccuracy());
    EXPECT_EQ(ts, pos.getTimestamp());
}

TEST_F(PositionTest, DistanceToSamePosition) {
    Position pos(40.7128, -74.0060);
    EXPECT_NEAR(0.0, pos.distanceTo(pos), 0.001);
}

TEST_F(PositionTest, DistanceToDifferentPosition) {
    // New York City coordinates
    Position nyc(40.7128, -74.0060);
    
    // Los Angeles coordinates
    Position la(34.0522, -118.2437);
    
    // Expected distance between NYC and LA is approximately 3935 km or 3935000 meters
    // Allow for some floating point error with a reasonable tolerance
    EXPECT_NEAR(3935000.0, nyc.distanceTo(la), 5000.0);
    
    // Distance should be the same in reverse direction
    EXPECT_NEAR(nyc.distanceTo(la), la.distanceTo(nyc), 0.001);
}

TEST_F(PositionTest, DistanceToNearbyPosition) {
    // Two positions 100 meters apart (approximately)
    // At latitude 40.7128, 0.001 degrees longitude is roughly 85 meters
    Position pos1(40.7128, -74.0060);
    Position pos2(40.7128, -74.0072);  // ~100m west
    
    // Expected distance should be close to 100 meters
    EXPECT_NEAR(100.0, pos1.distanceTo(pos2), 5.0);
}

TEST_F(PositionTest, DistanceToAcrossDateline) {
    // Test positions on opposite sides of the International Date Line
    Position west(0.0, 179.9);
    Position east(0.0, -179.9);
    
    // These points should be close to each other (about 22.2 km)
    EXPECT_NEAR(22200.0, west.distanceTo(east), 100.0);
}

TEST_F(PositionTest, DistanceToAcrossPoles) {
    // Test positions on opposite sides of the North Pole
    Position pos1(89.9, 0.0);
    Position pos2(89.9, 180.0);
    
    // These points should be close to each other
    EXPECT_NEAR(222000.0, pos1.distanceTo(pos2), 1000.0);
}

TEST_F(PositionTest, ToStringFormat) {
    Timestamp ts = createTimestamp(2023, 4, 20, 15, 30, 45);
    Position pos(37.7749, -122.4194, 12.3, 4.5, ts);
    
    std::string result = pos.toString();
    
    // Check that the string contains all the expected components
    EXPECT_THAT(result, ::testing::HasSubstr("lat=37.774900"));
    EXPECT_THAT(result, ::testing::HasSubstr("lon=-122.419400"));
    EXPECT_THAT(result, ::testing::HasSubstr("alt=12.30m"));
    EXPECT_THAT(result, ::testing::HasSubstr("acc=4.50m"));
    EXPECT_THAT(result, ::testing::HasSubstr("2023-04-20"));
    // Note: We don't check the exact time because it might be affected by timezone
}

TEST_F(PositionTest, ToStringPrecision) {
    // Test that the precision is correct for each value
    Position pos(12.3456789, -98.7654321, 123.456, 7.89);
    
    std::string result = pos.toString();
    
    // Latitude and longitude should have 6 decimal places
    EXPECT_THAT(result, ::testing::HasSubstr("lat=12.345679"));
    EXPECT_THAT(result, ::testing::HasSubstr("lon=-98.765432"));
    
    // Altitude and accuracy should have 2 decimal places
    EXPECT_THAT(result, ::testing::HasSubstr("alt=123.46m"));
    EXPECT_THAT(result, ::testing::HasSubstr("acc=7.89m"));
}

}  // namespace
}  // namespace equipment_tracker
// </test_code>