// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <string>
#include <ctime>
#include <cmath>
#include <sstream>
#include <iomanip>

// Mock or define EARTH_RADIUS_METERS if not included
#ifndef EARTH_RADIUS_METERS
#define EARTH_RADIUS_METERS 6371000.0
#endif

// Mock Timestamp as std::chrono::system_clock::time_point if not included
namespace equipment_tracker {
    using Timestamp = std::chrono::system_clock::time_point;
}

// Minimal Position class definition for testing
namespace equipment_tracker {

class Position {
public:
    Position(double latitude, double longitude, double altitude,
             double accuracy, Timestamp timestamp);

    double distanceTo(const Position &other) const;
    std::string toString() const;

private:
    double latitude_;
    double longitude_;
    double altitude_;
    double accuracy_;
    Timestamp timestamp_;
};

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

class PositionTest : public ::testing::Test {
protected:
    Timestamp MakeTimestamp(int year, int month, int day, int hour, int min, int sec) {
        std::tm tm = {};
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = min;
        tm.tm_sec = sec;
#ifdef _WIN32
        time_t tt = _mkgmtime(&tm);
#else
        time_t tt = timegm(&tm);
#endif
        return std::chrono::system_clock::from_time_t(tt);
    }
};

TEST_F(PositionTest, DistanceTo_SamePosition_ReturnsZero) {
    Timestamp ts = std::chrono::system_clock::now();
    Position p1(10.0, 20.0, 100.0, 5.0, ts);
    Position p2(10.0, 20.0, 200.0, 10.0, ts);
    EXPECT_NEAR(p1.distanceTo(p2), 0.0, 1e-6);
}

TEST_F(PositionTest, DistanceTo_KnownDistance) {
    // Paris (48.8566, 2.3522) to London (51.5074, -0.1278)
    Timestamp ts = std::chrono::system_clock::now();
    Position paris(48.8566, 2.3522, 35.0, 3.0, ts);
    Position london(51.5074, -0.1278, 25.0, 2.0, ts);
    double dist = paris.distanceTo(london);
    // Actual distance is about 343,556 meters
    EXPECT_NEAR(dist, 343556, 1000); // 1km tolerance
}

TEST_F(PositionTest, DistanceTo_AntipodalPoints) {
    // (0,0) and (0,180) are antipodal, should be about half Earth's circumference
    Timestamp ts = std::chrono::system_clock::now();
    Position p1(0.0, 0.0, 0.0, 0.0, ts);
    Position p2(0.0, 180.0, 0.0, 0.0, ts);
    double expected = M_PI * EARTH_RADIUS_METERS;
    EXPECT_NEAR(p1.distanceTo(p2), expected, 1.0);
}

TEST_F(PositionTest, DistanceTo_NearPoles) {
    // North pole to slightly off north pole
    Timestamp ts = std::chrono::system_clock::now();
    Position p1(90.0, 0.0, 0.0, 0.0, ts);
    Position p2(89.999, 0.0, 0.0, 0.0, ts);
    double dist = p1.distanceTo(p2);
    EXPECT_GT(dist, 0.0);
    EXPECT_LT(dist, 200.0); // Should be less than 200 meters
}

TEST_F(PositionTest, ToString_FormatsCorrectly) {
    Timestamp ts = MakeTimestamp(2024, 6, 1, 12, 34, 56);
    Position pos(12.345678, 98.765432, 123.45, 6.78, ts);
    std::string s = pos.toString();
    EXPECT_THAT(s, ::testing::HasSubstr("Position(lat=12.345678"));
    EXPECT_THAT(s, ::testing::HasSubstr("lon=98.765432"));
    EXPECT_THAT(s, ::testing::HasSubstr("alt=123.45m"));
    EXPECT_THAT(s, ::testing::HasSubstr("acc=6.78m"));
    EXPECT_THAT(s, ::testing::HasSubstr("2024-06-01 12:34:56"));
}

TEST_F(PositionTest, ToString_HandlesNegativeCoordinates) {
    Timestamp ts = MakeTimestamp(2023, 1, 2, 3, 4, 5);
    Position pos(-45.123456, -179.999999, -50.0, 0.0, ts);
    std::string s = pos.toString();
    EXPECT_THAT(s, ::testing::HasSubstr("lat=-45.123456"));
    EXPECT_THAT(s, ::testing::HasSubstr("lon=-179.999999"));
    EXPECT_THAT(s, ::testing::HasSubstr("alt=-50.00m"));
    EXPECT_THAT(s, ::testing::HasSubstr("acc=0.00m"));
    EXPECT_THAT(s, ::testing::HasSubstr("2023-01-02 03:04:05"));
}

TEST_F(PositionTest, DistanceTo_InvalidInputs_NaN) {
    Timestamp ts = std::chrono::system_clock::now();
    Position p1(NAN, 0.0, 0.0, 0.0, ts);
    Position p2(0.0, NAN, 0.0, 0.0, ts);
    double d1 = p1.distanceTo(p2);
    double d2 = p2.distanceTo(p1);
    EXPECT_TRUE(std::isnan(d1) || std::isnan(d2));
}

TEST_F(PositionTest, DistanceTo_InvalidInputs_Inf) {
    Timestamp ts = std::chrono::system_clock::now();
    Position p1(INFINITY, 0.0, 0.0, 0.0, ts);
    Position p2(0.0, INFINITY, 0.0, 0.0, ts);
    double d1 = p1.distanceTo(p2);
    double d2 = p2.distanceTo(p1);
    EXPECT_TRUE(std::isnan(d1) || std::isnan(d2) || std::isinf(d1) || std::isinf(d2));
}

TEST_F(PositionTest, ToString_HandlesEpochTime) {
    Timestamp ts = std::chrono::system_clock::from_time_t(0);
    Position pos(0.0, 0.0, 0.0, 0.0, ts);
    std::string s = pos.toString();
    // 1970-01-01 00:00:00 UTC or local time, depending on system
    EXPECT_THAT(s, ::testing::HasSubstr("1970-"));
}

TEST_F(PositionTest, ToString_HandlesLargeValues) {
    Timestamp ts = MakeTimestamp(2099, 12, 31, 23, 59, 59);
    Position pos(89.999999, 179.999999, 99999.99, 999.99, ts);
    std::string s = pos.toString();
    EXPECT_THAT(s, ::testing::HasSubstr("lat=89.999999"));
    EXPECT_THAT(s, ::testing::HasSubstr("lon=179.999999"));
    EXPECT_THAT(s, ::testing::HasSubstr("alt=99999.99m"));
    EXPECT_THAT(s, ::testing::HasSubstr("acc=999.99m"));
    EXPECT_THAT(s, ::testing::HasSubstr("2099-12-31 23:59:59"));
}
// </test_code>
