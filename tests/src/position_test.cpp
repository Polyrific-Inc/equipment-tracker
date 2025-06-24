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
        
        std::time_t time_t_value;
#ifdef _WIN32
        time_t_value = _mkgmtime(&timeinfo);
#else
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
    // Can't test exact timestamp as it's set to current time
}

TEST_F(PositionTest, ParameterizedConstructor) {
    const double latitude = 37.7749;
    const double longitude = -122.4194;
    const double altitude = 10.5;
    const double accuracy = 1.2;
    const Timestamp timestamp = createTimestamp(2023, 5, 15, 10, 30, 0);
    
    Position position(latitude, longitude, altitude, accuracy, timestamp);
    
    EXPECT_DOUBLE_EQ(latitude, position.getLatitude());
    EXPECT_DOUBLE_EQ(longitude, position.getLongitude());
    EXPECT_DOUBLE_EQ(altitude, position.getAltitude());
    EXPECT_DOUBLE_EQ(accuracy, position.getAccuracy());
    EXPECT_EQ(timestamp, position.getTimestamp());
}

TEST_F(PositionTest, ParameterizedConstructorWithDefaults) {
    const double latitude = 37.7749;
    const double longitude = -122.4194;
    
    Position position(latitude, longitude);
    
    EXPECT_DOUBLE_EQ(latitude, position.getLatitude());
    EXPECT_DOUBLE_EQ(longitude, position.getLongitude());
    EXPECT_DOUBLE_EQ(0.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(DEFAULT_POSITION_ACCURACY, position.getAccuracy());
    // Can't test exact timestamp as it's set to current time
}

TEST_F(PositionTest, Setters) {
    Position position;
    
    const double latitude = 37.7749;
    const double longitude = -122.4194;
    const double altitude = 10.5;
    const double accuracy = 1.2;
    const Timestamp timestamp = createTimestamp(2023, 5, 15, 10, 30, 0);
    
    position.setLatitude(latitude);
    position.setLongitude(longitude);
    position.setAltitude(altitude);
    position.setAccuracy(accuracy);
    position.setTimestamp(timestamp);
    
    EXPECT_DOUBLE_EQ(latitude, position.getLatitude());
    EXPECT_DOUBLE_EQ(longitude, position.getLongitude());
    EXPECT_DOUBLE_EQ(altitude, position.getAltitude());
    EXPECT_DOUBLE_EQ(accuracy, position.getAccuracy());
    EXPECT_EQ(timestamp, position.getTimestamp());
}

TEST_F(PositionTest, Builder) {
    const double latitude = 37.7749;
    const double longitude = -122.4194;
    const double altitude = 10.5;
    const double accuracy = 1.2;
    const Timestamp timestamp = createTimestamp(2023, 5, 15, 10, 30, 0);
    
    Position position = Position::builder()
        .withLatitude(latitude)
        .withLongitude(longitude)
        .withAltitude(altitude)
        .withAccuracy(accuracy)
        .withTimestamp(timestamp)
        .build();
    
    EXPECT_DOUBLE_EQ(latitude, position.getLatitude());
    EXPECT_DOUBLE_EQ(longitude, position.getLongitude());
    EXPECT_DOUBLE_EQ(altitude, position.getAltitude());
    EXPECT_DOUBLE_EQ(accuracy, position.getAccuracy());
    EXPECT_EQ(timestamp, position.getTimestamp());
}

TEST_F(PositionTest, BuilderWithDefaults) {
    const double latitude = 37.7749;
    const double longitude = -122.4194;
    
    Position position = Position::builder()
        .withLatitude(latitude)
        .withLongitude(longitude)
        .build();
    
    EXPECT_DOUBLE_EQ(latitude, position.getLatitude());
    EXPECT_DOUBLE_EQ(longitude, position.getLongitude());
    EXPECT_DOUBLE_EQ(0.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(DEFAULT_POSITION_ACCURACY, position.getAccuracy());
    // Can't test exact timestamp as it's set to current time
}

TEST_F(PositionTest, DistanceToSamePosition) {
    Position position(37.7749, -122.4194);
    
    double distance = position.distanceTo(position);
    
    EXPECT_NEAR(0.0, distance, 1e-10);
}

TEST_F(PositionTest, DistanceBetweenTwoPositions) {
    // San Francisco
    Position sf(37.7749, -122.4194);
    
    // Los Angeles
    Position la(34.0522, -118.2437);
    
    // Expected distance between SF and LA is approximately 559.65 km (559650 meters)
    double distance = sf.distanceTo(la);
    
    EXPECT_NEAR(559650.0, distance, 1000.0);  // Allow 1km tolerance due to different calculation methods
}

TEST_F(PositionTest, DistanceBetweenClosePositions) {
    // Two positions 100 meters apart (approximately)
    Position pos1(37.7749, -122.4194);
    Position pos2(37.7749, -122.4204);  // Slightly different longitude
    
    double distance = pos1.distanceTo(pos2);
    
    // The distance should be approximately 100 meters
    EXPECT_NEAR(100.0, distance, 10.0);  // Allow 10m tolerance
}

TEST_F(PositionTest, DistanceAtPoles) {
    // North Pole
    Position northPole(90.0, 0.0);
    
    // South Pole
    Position southPole(-90.0, 0.0);
    
    double distance = northPole.distanceTo(southPole);
    
    // The distance should be approximately the half circumference of Earth
    EXPECT_NEAR(2 * M_PI * EARTH_RADIUS_METERS / 2, distance, 1000.0);
}

TEST_F(PositionTest, DistanceAtEquator) {
    // Two points on the equator, 90 degrees apart
    Position pos1(0.0, 0.0);
    Position pos2(0.0, 90.0);
    
    double distance = pos1.distanceTo(pos2);
    
    // The distance should be approximately a quarter of Earth's circumference
    EXPECT_NEAR(2 * M_PI * EARTH_RADIUS_METERS / 4, distance, 1000.0);
}

TEST_F(PositionTest, ToStringFormat) {
    const double latitude = 37.7749;
    const double longitude = -122.4194;
    const double altitude = 10.5;
    const double accuracy = 1.2;
    const Timestamp timestamp = createTimestamp(2023, 5, 15, 10, 30, 0);
    
    Position position(latitude, longitude, altitude, accuracy, timestamp);
    
    std::string positionStr = position.toString();
    
    // Check that the string contains all the expected parts
    EXPECT_THAT(positionStr, ::testing::HasSubstr("Position(lat=37.774900"));
    EXPECT_THAT(positionStr, ::testing::HasSubstr("lon=-122.419400"));
    EXPECT_THAT(positionStr, ::testing::HasSubstr("alt=10.50m"));
    EXPECT_THAT(positionStr, ::testing::HasSubstr("acc=1.20m"));
    EXPECT_THAT(positionStr, ::testing::HasSubstr("time="));
    // Note: We can't test the exact time string as it depends on the local timezone
}

TEST_F(PositionTest, ToStringPrecision) {
    // Test with many decimal places to verify precision
    const double latitude = 37.7749283;
    const double longitude = -122.4194155;
    const double altitude = 10.567;
    const double accuracy = 1.234;
    
    Position position(latitude, longitude, altitude, accuracy);
    
    std::string positionStr = position.toString();
    
    // Check that the string has the correct precision
    EXPECT_THAT(positionStr, ::testing::HasSubstr("lat=37.774928"));  // 6 decimal places
    EXPECT_THAT(positionStr, ::testing::HasSubstr("lon=-122.419416")); // 6 decimal places
    EXPECT_THAT(positionStr, ::testing::HasSubstr("alt=10.57m"));     // 2 decimal places
    EXPECT_THAT(positionStr, ::testing::HasSubstr("acc=1.23m"));      // 2 decimal places
}

} // namespace testing
} // namespace equipment_tracker
// </test_code>