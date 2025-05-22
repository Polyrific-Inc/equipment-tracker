// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <ctime>
#include <string>
#include <cmath>
#include <sstream>
#include <iomanip>

// Mocks and constants
namespace equipment_tracker {
    // Mock EARTH_RADIUS_METERS if not defined
    #ifndef EARTH_RADIUS_METERS
    #define EARTH_RADIUS_METERS 6371000.0
    #endif

    using Timestamp = std::chrono::system_clock::time_point;

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
}

// Implementation for testing (since only header was provided)
namespace equipment_tracker
{
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
}

// Test code
using namespace equipment_tracker;
using ::testing::HasSubstr;

namespace {

constexpr double kTolerance = 1e-3; // 1mm tolerance for distance

// Helper to create a Timestamp from UTC time
std::chrono::system_clock::time_point MakeTime(int year, int mon, int day, int hour, int min, int sec) {
    std::tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon = mon - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    tm.tm_isdst = -1;
#ifdef _WIN32
    time_t t = _mkgmtime(&tm);
#else
    time_t t = timegm(&tm);
#endif
    return std::chrono::system_clock::from_time_t(t);
}

TEST(PositionTest, DistanceTo_SamePoint_ZeroDistance) {
    auto ts = std::chrono::system_clock::now();
    Position p1(10.0, 20.0, 100.0, 5.0, ts);
    Position p2(10.0, 20.0, 200.0, 10.0, ts); // Different alt/acc, but same lat/lon
    EXPECT_NEAR(p1.distanceTo(p2), 0.0, kTolerance);
}

TEST(PositionTest, DistanceTo_KnownDistance) {
    // Paris (48.8566, 2.3522) to London (51.5074, -0.1278)
    auto ts = std::chrono::system_clock::now();
    Position paris(48.8566, 2.3522, 35.0, 5.0, ts);
    Position london(51.5074, -0.1278, 25.0, 10.0, ts);
    double dist = paris.distanceTo(london);
    // Known distance ~343556 meters
    EXPECT_NEAR(dist, 343556, 200.0); // Allow 200m tolerance for platform differences
}

TEST(PositionTest, DistanceTo_AntipodalPoints) {
    // (0,0) and (0,180) are antipodal, should be ~pi*R
    auto ts = std::chrono::system_clock::now();
    Position p1(0.0, 0.0, 0.0, 0.0, ts);
    Position p2(0.0, 180.0, 0.0, 0.0, ts);
    double expected = M_PI * EARTH_RADIUS_METERS;
    EXPECT_NEAR(p1.distanceTo(p2), expected, 1.0); // 1m tolerance
}

TEST(PositionTest, DistanceTo_NearPoles) {
    auto ts = std::chrono::system_clock::now();
    Position north1(89.9999, 0.0, 0.0, 0.0, ts);
    Position north2(89.9999, 90.0, 0.0, 0.0, ts);
    double dist = north1.distanceTo(north2);
    // Should be a very small distance
    EXPECT_NEAR(dist, 0.0, 20.0); // Allow 20m tolerance
}

TEST(PositionTest, ToString_FormatsAllFields) {
    auto ts = MakeTime(2024, 6, 1, 12, 34, 56);
    Position pos(12.345678, 98.765432, 123.45, 6.78, ts);
    std::string s = pos.toString();
    EXPECT_THAT(s, HasSubstr("lat=12.345678"));
    EXPECT_THAT(s, HasSubstr("lon=98.765432"));
    EXPECT_THAT(s, HasSubstr("alt=123.45m"));
    EXPECT_THAT(s, HasSubstr("acc=6.78m"));
    EXPECT_THAT(s, HasSubstr("2024-06-01 12:34:56"));
    EXPECT_THAT(s, HasSubstr("Position("));
}

TEST(PositionTest, ToString_NegativeCoordinates) {
    auto ts = MakeTime(2023, 1, 2, 3, 4, 5);
    Position pos(-45.123456, -179.999999, -50.0, 0.0, ts);
    std::string s = pos.toString();
    EXPECT_THAT(s, HasSubstr("lat=-45.123456"));
    EXPECT_THAT(s, HasSubstr("lon=-179.999999"));
    EXPECT_THAT(s, HasSubstr("alt=-50.00m"));
    EXPECT_THAT(s, HasSubstr("acc=0.00m"));
    EXPECT_THAT(s, HasSubstr("2023-01-02 03:04:05"));
}

TEST(PositionTest, ToString_ZeroValues) {
    auto ts = MakeTime(2000, 1, 1, 0, 0, 0);
    Position pos(0.0, 0.0, 0.0, 0.0, ts);
    std::string s = pos.toString();
    EXPECT_THAT(s, HasSubstr("lat=0.000000"));
    EXPECT_THAT(s, HasSubstr("lon=0.000000"));
    EXPECT_THAT(s, HasSubstr("alt=0.00m"));
    EXPECT_THAT(s, HasSubstr("acc=0.00m"));
    EXPECT_THAT(s, HasSubstr("2000-01-01 00:00:00"));
}

TEST(PositionTest, DistanceTo_InvalidLatitudes) {
    auto ts = std::chrono::system_clock::now();
    // Latitude out of range, but class does not validate, so should still compute
    Position p1(100.0, 0.0, 0.0, 0.0, ts);
    Position p2(-100.0, 0.0, 0.0, 0.0, ts);
    double dist = p1.distanceTo(p2);
    // Should not be NaN or inf
    EXPECT_TRUE(std::isfinite(dist));
}

TEST(PositionTest, DistanceTo_InvalidLongitudes) {
    auto ts = std::chrono::system_clock::now();
    Position p1(0.0, 200.0, 0.0, 0.0, ts);
    Position p2(0.0, -200.0, 0.0, 0.0, ts);
    double dist = p1.distanceTo(p2);
    EXPECT_TRUE(std::isfinite(dist));
}

TEST(PositionTest, DistanceTo_BoundaryLatitudes) {
    auto ts = std::chrono::system_clock::now();
    Position p1(90.0, 0.0, 0.0, 0.0, ts);
    Position p2(-90.0, 0.0, 0.0, 0.0, ts);
    double expected = M_PI * EARTH_RADIUS_METERS;
    EXPECT_NEAR(p1.distanceTo(p2), expected, 1.0);
}

TEST(PositionTest, DistanceTo_BoundaryLongitudes) {
    auto ts = std::chrono::system_clock::now();
    Position p1(0.0, 180.0, 0.0, 0.0, ts);
    Position p2(0.0, -180.0, 0.0, 0.0, ts);
    EXPECT_NEAR(p1.distanceTo(p2), 0.0, kTolerance);
}

} // namespace
// </test_code>
