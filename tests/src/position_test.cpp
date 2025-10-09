// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <cmath>
#include <string>
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/constants.h"

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
        timeinfo.tm_isdst = -1;          // Determine DST automatically

        std::time_t time_t;
#ifdef _WIN32
        time_t = _mkgmtime(&timeinfo);
#else
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
    // We can't test exact timestamp since it's set to current time
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
    position.setAltitude(15.2);
    position.setAccuracy(3.5);
    
    Timestamp timestamp = createTimestamp(2023, 6, 20, 14, 45, 30);
    position.setTimestamp(timestamp);
    
    EXPECT_DOUBLE_EQ(37.7749, position.getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194, position.getLongitude());
    EXPECT_DOUBLE_EQ(15.2, position.getAltitude());
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

TEST_F(PositionTest, DistanceToSamePosition) {
    Position position(40.7128, -74.0060);
    EXPECT_NEAR(0.0, position.distanceTo(position), 0.001);
}

TEST_F(PositionTest, DistanceToNearbyPosition) {
    // Two points in New York City, about 2 km apart
    Position position1(40.7128, -74.0060); // NYC
    Position position2(40.7306, -73.9867); // Empire State Building
    
    // Expected distance is approximately 2 km
    double distance = position1.distanceTo(position2);
    EXPECT_NEAR(2000.0, distance, 100.0); // Allow 100m tolerance
}

TEST_F(PositionTest, DistanceToDifferentContinents) {
    // New York and London, approximately 5,570 km apart
    Position newYork(40.7128, -74.0060);
    Position london(51.5074, -0.1278);
    
    double distance = newYork.distanceTo(london);
    EXPECT_NEAR(5570000.0, distance, 10000.0); // Allow 10km tolerance
}

TEST_F(PositionTest, DistanceToAntipodes) {
    // Approximately antipodal points (opposite sides of Earth)
    Position point1(40.0, 40.0);
    Position point2(-40.0, -140.0);
    
    double distance = point1.distanceTo(point2);
    // Should be close to half the Earth's circumference
    EXPECT_NEAR(M_PI * EARTH_RADIUS_METERS, distance, 100000.0);
}

TEST_F(PositionTest, ToStringFormat) {
    Timestamp timestamp = createTimestamp(2023, 8, 25, 12, 30, 45);
    Position position(37.7749, -122.4194, 15.2, 3.5, timestamp);
    
    std::string posStr = position.toString();
    
    // Check that the string contains all the expected components
    EXPECT_THAT(posStr, ::testing::HasSubstr("lat=37.774900"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("lon=-122.419400"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("alt=15.20m"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("acc=3.50m"));
    
    // Time format might vary by locale, so just check for the date components
    EXPECT_THAT(posStr, ::testing::HasSubstr("2023"));
}

TEST_F(PositionTest, EdgeCaseLatitudeBoundaries) {
    // Test at the poles
    Position northPole(90.0, 0.0);
    Position southPole(-90.0, 0.0);
    
    EXPECT_DOUBLE_EQ(90.0, northPole.getLatitude());
    EXPECT_DOUBLE_EQ(-90.0, southPole.getLatitude());
    
    // Distance between poles should be approximately Earth's diameter
    double polarDistance = northPole.distanceTo(southPole);
    EXPECT_NEAR(2 * M_PI * EARTH_RADIUS_METERS / 2, polarDistance, 100.0);
}

TEST_F(PositionTest, EdgeCaseLongitudeBoundaries) {
    // Test at the international date line
    Position westDateLine(0.0, -180.0);
    Position eastDateLine(0.0, 180.0);
    
    EXPECT_DOUBLE_EQ(-180.0, westDateLine.getLongitude());
    EXPECT_DOUBLE_EQ(180.0, eastDateLine.getLongitude());
    
    // These points should be at the same location
    double dateLineDistance = westDateLine.distanceTo(eastDateLine);
    EXPECT_NEAR(0.0, dateLineDistance, 0.001);
}

} // namespace testing
} // namespace equipment_tracker
// </test_code>