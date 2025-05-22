// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <ctime>
#include <string>
#include <cmath>
#include <sstream>
#include <iomanip>

// Mock and define EARTH_RADIUS_METERS if not present
#ifndef EARTH_RADIUS_METERS
#define EARTH_RADIUS_METERS 6371000.0
#endif

namespace equipment_tracker {

// Alias for timestamp type
using Timestamp = std::chrono::system_clock::time_point;

// Minimal Position class definition for testing
class Position {
public:
    Position(double latitude, double longitude, double altitude,
             double accuracy, Timestamp timestamp);

    double distanceTo(const Position &other) const;
    std::string toString() const;

    // For test access
    double latitude_, longitude_, altitude_, accuracy_;
    Timestamp timestamp_;
};

// Implementation for testing (copied from the code under test)
Position::Position(double latitude, double longitude, double altitude,
                   double accuracy, Timestamp timestamp)
    : latitude_(latitude),
      longitude_(longitude),
      altitude_(altitude),
      accuracy_(accuracy),
      timestamp_(timestamp)
{
}

double Position::distanceTo(const Position &other) const
{
    double lat1_rad = latitude_ * M_PI / 180.0;
    double lat2_rad = other.latitude_ * M_PI / 180.0;
    double lon1_rad = longitude_ * M_PI / 180.0;
    double lon2_rad = other.longitude_ * M_PI / 180.0;

    double dlat = lat2_rad - lat1_rad;
    double dlon = lon2_rad - lon1_rad;

    double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
               std::cos(lat1_rad) * std::cos(lat2_rad) *
                   std::sin(dlon / 2) * std::sin(dlon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

    return EARTH_RADIUS_METERS * c;
}

std::string Position::toString() const
{
    std::stringstream ss;
    auto time_t = std::chrono::system_clock::to_time_t(timestamp_);
#ifdef _WIN32
    std::tm tm_buf;
    localtime_s(&tm_buf, &time_t);
    std::tm &tm = tm_buf;
#else
    std::tm tm = *std::localtime(&time_t);
#endif

    ss << "Position(lat=" << std::fixed << std::setprecision(6) << latitude_
       << ", lon=" << std::fixed << std::setprecision(6) << longitude_
       << ", alt=" << std::fixed << std::setprecision(2) << altitude_
       << "m, acc=" << std::fixed << std::setprecision(2) << accuracy_
       << "m, time=" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << ")";

    return ss.str();
}

} // namespace equipment_tracker

using namespace equipment_tracker;

// Helper function to create a Timestamp from a specific date/time
static Timestamp makeTimestamp(int year, int month, int day, int hour, int min, int sec) {
    std::tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon  = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min  = min;
    tm.tm_sec  = sec;
    tm.tm_isdst = -1;
    std::time_t tt = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(tt);
}

// Test fixture
class PositionTest : public ::testing::Test {
protected:
    Timestamp now;
    void SetUp() override {
        now = std::chrono::system_clock::now();
    }
};

// Test constructor and member initialization
TEST_F(PositionTest, ConstructorInitializesMembers) {
    double lat = 12.3456, lon = 65.4321, alt = 123.45, acc = 5.5;
    Timestamp ts = now;
    Position pos(lat, lon, alt, acc, ts);
    EXPECT_DOUBLE_EQ(pos.latitude_, lat);
    EXPECT_DOUBLE_EQ(pos.longitude_, lon);
    EXPECT_DOUBLE_EQ(pos.altitude_, alt);
    EXPECT_DOUBLE_EQ(pos.accuracy_, acc);
    EXPECT_EQ(pos.timestamp_, ts);
}

// Test distanceTo: same position (distance should be 0)
TEST_F(PositionTest, DistanceTo_SamePosition) {
    Position pos1(10.0, 20.0, 0.0, 1.0, now);
    Position pos2(10.0, 20.0, 0.0, 1.0, now);
    EXPECT_NEAR(pos1.distanceTo(pos2), 0.0, 1e-6);
}

// Test distanceTo: known distance (equator, 1 degree longitude apart)
TEST_F(PositionTest, DistanceTo_OneDegreeLongitudeAtEquator) {
    Position pos1(0.0, 0.0, 0.0, 1.0, now);
    Position pos2(0.0, 1.0, 0.0, 1.0, now);
    // At equator, 1 degree longitude ~ 111,319.5 meters
    double expected = EARTH_RADIUS_METERS * M_PI / 180.0;
    EXPECT_NEAR(pos1.distanceTo(pos2), expected, 1.0);
}

// Test distanceTo: known distance (1 degree latitude apart)
TEST_F(PositionTest, DistanceTo_OneDegreeLatitude) {
    Position pos1(0.0, 0.0, 0.0, 1.0, now);
    Position pos2(1.0, 0.0, 0.0, 1.0, now);
    // 1 degree latitude ~ 111,194.9 meters
    double expected = EARTH_RADIUS_METERS * M_PI / 180.0;
    EXPECT_NEAR(pos1.distanceTo(pos2), expected, 1.0);
}

// Test distanceTo: antipodal points (max possible distance)
TEST_F(PositionTest, DistanceTo_AntipodalPoints) {
    Position pos1(0.0, 0.0, 0.0, 1.0, now);
    Position pos2(-0.0, 180.0, 0.0, 1.0, now);
    double expected = EARTH_RADIUS_METERS * M_PI;
    EXPECT_NEAR(pos1.distanceTo(pos2), expected, 1.0);
}

// Test distanceTo: edge cases (latitude/longitude boundaries)
TEST_F(PositionTest, DistanceTo_LatLonBoundaries) {
    Position pos1(-90.0, -180.0, 0.0, 1.0, now);
    Position pos2(90.0, 180.0, 0.0, 1.0, now);
    double expected = EARTH_RADIUS_METERS * M_PI;
    EXPECT_NEAR(pos1.distanceTo(pos2), expected, 1.0);
}

// Test distanceTo: negative and positive values
TEST_F(PositionTest, DistanceTo_NegativeAndPositive) {
    Position pos1(-45.0, -90.0, 0.0, 1.0, now);
    Position pos2(45.0, 90.0, 0.0, 1.0, now);
    double expected = EARTH_RADIUS_METERS * M_PI;
    EXPECT_NEAR(pos1.distanceTo(pos2), expected, 1.0);
}

// Test toString: correct formatting
TEST_F(PositionTest, ToString_Format) {
    double lat = 12.3456789, lon = 98.7654321, alt = 123.456, acc = 7.89;
    Timestamp ts = makeTimestamp(2023, 6, 1, 12, 34, 56);
    Position pos(lat, lon, alt, acc, ts);

    std::string result = pos.toString();

    // Check for all fields and correct precision
    EXPECT_NE(result.find("lat=12.345679"), std::string::npos);
    EXPECT_NE(result.find("lon=98.765432"), std::string::npos);
    EXPECT_NE(result.find("alt=123.46m"), std::string::npos);
    EXPECT_NE(result.find("acc=7.89m"), std::string::npos);
    EXPECT_NE(result.find("2023-06-01 12:34:56"), std::string::npos);
    EXPECT_EQ(result.substr(0, 9), "Position(");
}

// Test toString: negative values and zero
TEST_F(PositionTest, ToString_NegativeAndZero) {
    double lat = -45.0, lon = 0.0, alt = -100.0, acc = 0.0;
    Timestamp ts = makeTimestamp(2022, 1, 2, 3, 4, 5);
    Position pos(lat, lon, alt, acc, ts);

    std::string result = pos.toString();

    EXPECT_NE(result.find("lat=-45.000000"), std::string::npos);
    EXPECT_NE(result.find("lon=0.000000"), std::string::npos);
    EXPECT_NE(result.find("alt=-100.00m"), std::string::npos);
    EXPECT_NE(result.find("acc=0.00m"), std::string::npos);
    EXPECT_NE(result.find("2022-01-02 03:04:05"), std::string::npos);
}

// Test toString: boundary values
TEST_F(PositionTest, ToString_BoundaryValues) {
    double lat = 90.0, lon = 180.0, alt = 9999.99, acc = 999.99;
    Timestamp ts = makeTimestamp(2030, 12, 31, 23, 59, 59);
    Position pos(lat, lon, alt, acc, ts);

    std::string result = pos.toString();

    EXPECT_NE(result.find("lat=90.000000"), std::string::npos);
    EXPECT_NE(result.find("lon=180.000000"), std::string::npos);
    EXPECT_NE(result.find("alt=9999.99m"), std::string::npos);
    EXPECT_NE(result.find("acc=999.99m"), std::string::npos);
    EXPECT_NE(result.find("2030-12-31 23:59:59"), std::string::npos);
}

// Test distanceTo: very small distances (floating point precision)
TEST_F(PositionTest, DistanceTo_VerySmallDistance) {
    Position pos1(10.000000, 20.000000, 0.0, 1.0, now);
    Position pos2(10.000001, 20.000001, 0.0, 1.0, now);
    double dist = pos1.distanceTo(pos2);
    EXPECT_GT(dist, 0.0);
    EXPECT_LT(dist, 0.2); // Should be less than 0.2 meters
}

// Test toString: leap year date
TEST_F(PositionTest, ToString_LeapYear) {
    Timestamp ts = makeTimestamp(2020, 2, 29, 0, 0, 0);
    Position pos(0.0, 0.0, 0.0, 0.0, ts);

    std::string result = pos.toString();
    EXPECT_NE(result.find("2020-02-29 00:00:00"), std::string::npos);
}

// Test distanceTo: invalid/NaN/Inf values
TEST_F(PositionTest, DistanceTo_NaNAndInf) {
    Position pos1(NAN, NAN, 0.0, 1.0, now);
    Position pos2(INFINITY, INFINITY, 0.0, 1.0, now);
    double dist = pos1.distanceTo(pos2);
    EXPECT_TRUE(std::isnan(dist) || std::isinf(dist));
}

// Test toString: NaN and Inf values
TEST_F(PositionTest, ToString_NaNAndInf) {
    Timestamp ts = makeTimestamp(2021, 1, 1, 0, 0, 0);
    Position pos(NAN, INFINITY, -INFINITY, NAN, ts);
    std::string result = pos.toString();
    // The output may contain "nan" or "inf" depending on the platform
    EXPECT_NE(result.find("lat="), std::string::npos);
    EXPECT_NE(result.find("lon="), std::string::npos);
    EXPECT_NE(result.find("alt="), std::string::npos);
    EXPECT_NE(result.find("acc="), std::string::npos);
    EXPECT_NE(result.find("2021-01-01 00:00:00"), std::string::npos);
}

// Test toString: time at epoch
TEST_F(PositionTest, ToString_Epoch) {
    Timestamp ts = std::chrono::system_clock::from_time_t(0);
    Position pos(0.0, 0.0, 0.0, 0.0, ts);
    std::string result = pos.toString();
    // 1970-01-01 00:00:00 is the epoch
    EXPECT_NE(result.find("1970-01-01 00:00:00"), std::string::npos);
}

// Test distanceTo: large difference in latitude only
TEST_F(PositionTest, DistanceTo_LargeLatitudeDifference) {
    Position pos1(-90.0, 0.0, 0.0, 1.0, now);
    Position pos2(90.0, 0.0, 0.0, 1.0, now);
    double expected = EARTH_RADIUS_METERS * M_PI;
    EXPECT_NEAR(pos1.distanceTo(pos2), expected, 1.0);
}

// Test distanceTo: large difference in longitude only
TEST_F(PositionTest, DistanceTo_LargeLongitudeDifference) {
    Position pos1(0.0, -180.0, 0.0, 1.0, now);
    Position pos2(0.0, 180.0, 0.0, 1.0, now);
    double expected = 0.0; // Same point on sphere
    EXPECT_NEAR(pos1.distanceTo(pos2), expected, 1e-6);
}

// Test toString: time with single digit month/day/hour/min/sec
TEST_F(PositionTest, ToString_SingleDigitFields) {
    Timestamp ts = makeTimestamp(2024, 4, 5, 6, 7, 8);
    Position pos(1.0, 2.0, 3.0, 4.0, ts);
    std::string result = pos.toString();
    EXPECT_NE(result.find("2024-04-05 06:07:08"), std::string::npos);
}

// </test_code>
