// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/constants.h"

// Define M_PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace equipment_tracker {
namespace {

// Helper function to create a timestamp for testing
Timestamp createTimestamp(int year, int month, int day, int hour, int minute, int second) {
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
    time_t = timegm(&timeinfo);
#endif

    return std::chrono::system_clock::from_time_t(time_t);
}

class PositionTest : public ::testing::Test {
protected:
    // Some common test coordinates
    const double nyc_lat = 40.7128;
    const double nyc_lon = -74.0060;
    const double london_lat = 51.5074;
    const double london_lon = -0.1278;
    const double tokyo_lat = 35.6762;
    const double tokyo_lon = 139.6503;
    const double sydney_lat = -33.8688;
    const double sydney_lon = 151.2093;
    
    // Fixed timestamp for testing
    const Timestamp test_timestamp = createTimestamp(2023, 5, 15, 12, 30, 45);
};

TEST_F(PositionTest, DefaultConstructor) {
    Position pos;
    EXPECT_DOUBLE_EQ(0.0, pos.getLatitude());
    EXPECT_DOUBLE_EQ(0.0, pos.getLongitude());
    EXPECT_DOUBLE_EQ(0.0, pos.getAltitude());
    EXPECT_DOUBLE_EQ(DEFAULT_POSITION_ACCURACY, pos.getAccuracy());
    // We can't test exact timestamp as it's set to current time
}

TEST_F(PositionTest, ParameterizedConstructor) {
    Position pos(nyc_lat, nyc_lon, 10.5, 5.0, test_timestamp);
    EXPECT_DOUBLE_EQ(nyc_lat, pos.getLatitude());
    EXPECT_DOUBLE_EQ(nyc_lon, pos.getLongitude());
    EXPECT_DOUBLE_EQ(10.5, pos.getAltitude());
    EXPECT_DOUBLE_EQ(5.0, pos.getAccuracy());
    EXPECT_EQ(test_timestamp, pos.getTimestamp());
}

TEST_F(PositionTest, SettersAndGetters) {
    Position pos;
    
    pos.setLatitude(tokyo_lat);
    pos.setLongitude(tokyo_lon);
    pos.setAltitude(100.0);
    pos.setAccuracy(3.5);
    pos.setTimestamp(test_timestamp);
    
    EXPECT_DOUBLE_EQ(tokyo_lat, pos.getLatitude());
    EXPECT_DOUBLE_EQ(tokyo_lon, pos.getLongitude());
    EXPECT_DOUBLE_EQ(100.0, pos.getAltitude());
    EXPECT_DOUBLE_EQ(3.5, pos.getAccuracy());
    EXPECT_EQ(test_timestamp, pos.getTimestamp());
}

TEST_F(PositionTest, Builder) {
    Position pos = Position::builder()
        .withLatitude(sydney_lat)
        .withLongitude(sydney_lon)
        .withAltitude(50.0)
        .withAccuracy(1.5)
        .withTimestamp(test_timestamp)
        .build();
    
    EXPECT_DOUBLE_EQ(sydney_lat, pos.getLatitude());
    EXPECT_DOUBLE_EQ(sydney_lon, pos.getLongitude());
    EXPECT_DOUBLE_EQ(50.0, pos.getAltitude());
    EXPECT_DOUBLE_EQ(1.5, pos.getAccuracy());
    EXPECT_EQ(test_timestamp, pos.getTimestamp());
}

TEST_F(PositionTest, DistanceToSamePoint) {
    Position pos1(nyc_lat, nyc_lon);
    Position pos2(nyc_lat, nyc_lon);
    
    double distance = pos1.distanceTo(pos2);
    EXPECT_NEAR(0.0, distance, 0.001);
}

TEST_F(PositionTest, DistanceBetweenCities) {
    // Test with known distances between major cities
    Position nyc(nyc_lat, nyc_lon);
    Position london(london_lat, london_lon);
    Position tokyo(tokyo_lat, tokyo_lon);
    
    // Approximate distances (in meters)
    // NYC to London: ~5,570 km
    EXPECT_NEAR(5570000.0, nyc.distanceTo(london), 50000.0);
    
    // NYC to Tokyo: ~10,840 km
    EXPECT_NEAR(10840000.0, nyc.distanceTo(tokyo), 100000.0);
    
    // London to Tokyo: ~9,560 km
    EXPECT_NEAR(9560000.0, london.distanceTo(tokyo), 100000.0);
}

TEST_F(PositionTest, DistanceSymmetry) {
    Position pos1(nyc_lat, nyc_lon);
    Position pos2(london_lat, london_lon);
    
    double distance1to2 = pos1.distanceTo(pos2);
    double distance2to1 = pos2.distanceTo(pos1);
    
    EXPECT_NEAR(distance1to2, distance2to1, 0.001);
}

TEST_F(PositionTest, ToStringFormat) {
    Position pos(nyc_lat, nyc_lon, 10.5, 5.0, test_timestamp);
    std::string str = pos.toString();
    
    // Check that the string contains all the expected components
    EXPECT_THAT(str, ::testing::HasSubstr("Position(lat="));
    EXPECT_THAT(str, ::testing::HasSubstr("lon="));
    EXPECT_THAT(str, ::testing::HasSubstr("alt=10.50m"));
    EXPECT_THAT(str, ::testing::HasSubstr("acc=5.00m"));
    EXPECT_THAT(str, ::testing::HasSubstr("time="));
    
    // Check that the latitude and longitude are formatted correctly
    std::stringstream expected_lat;
    expected_lat << std::fixed << std::setprecision(6) << nyc_lat;
    EXPECT_THAT(str, ::testing::HasSubstr(expected_lat.str()));
    
    std::stringstream expected_lon;
    expected_lon << std::fixed << std::setprecision(6) << nyc_lon;
    EXPECT_THAT(str, ::testing::HasSubstr(expected_lon.str()));
}

TEST_F(PositionTest, EdgeCaseCoordinates) {
    // Test with extreme coordinates
    Position northPole(90.0, 0.0);
    Position southPole(-90.0, 0.0);
    Position dateLine1(0.0, 180.0);
    Position dateLine2(0.0, -180.0);
    
    // Distance from North Pole to South Pole should be approximately Earth's circumference/2
    double poleDistance = northPole.distanceTo(southPole);
    EXPECT_NEAR(EARTH_RADIUS_METERS * M_PI, poleDistance, 1.0);
    
    // Points at opposite sides of the date line but same latitude should be close
    double dateLineDistance = dateLine1.distanceTo(dateLine2);
    EXPECT_NEAR(0.0, dateLineDistance, 0.001);
}

} // namespace
} // namespace equipment_tracker
// </test_code>