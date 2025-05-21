<test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cmath>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include "equipment_tracker/position.h"

// Mock or define constants if not included
#ifndef EARTH_RADIUS_METERS
#define EARTH_RADIUS_METERS 6371000.0
#endif

using namespace equipment_tracker;

// Helper function to create a fixed timestamp
Timestamp createTimestamp(int year, int month, int day, int hour, int min, int sec) {
    std::tm tm_time = {};
    tm_time.tm_year = year - 1900;
    tm_time.tm_mon = month - 1;
    tm_time.tm_mday = day;
    tm_time.tm_hour = hour;
    tm_time.tm_min = min;
    tm_time.tm_sec = sec;
    return std::chrono::system_clock::from_time_t(std::mktime(&tm_time));
}

TEST(PositionTest, DistanceTo_SamePoint_ReturnsZero) {
    Position p1(0.0, 0.0, 0.0, 1.0, createTimestamp(2023, 1, 1, 0, 0, 0));
    Position p2(0.0, 0.0, 0.0, 1.0, createTimestamp(2023, 1, 1, 0, 0, 0));
    EXPECT_DOUBLE_EQ(p1.distanceTo(p2), 0.0);
}

TEST(PositionTest, DistanceTo_KnownPoints_CorrectDistance) {
    // Approximate distance between Paris (48.8566, 2.3522) and London (51.5074, -0.1278)
    Position paris(48.8566, 2.3522, 35.0, 5.0, createTimestamp(2023, 1, 1, 12, 0, 0));
    Position london(51.5074, -0.1278, 15.0, 5.0, createTimestamp(2023, 1, 1, 12, 0, 0));
    double dist = paris.distanceTo(london);
    // The actual distance is approximately 343 km
    EXPECT_NEAR(dist / 1000.0, 343.0, 5.0); // within 5 km tolerance
}

TEST(PositionTest, DistanceTo_EdgeCases_NegativeCoordinates) {
    Position p1(-45.0, -45.0, 0.0, 1.0, createTimestamp(2023, 1, 1, 0, 0, 0));
    Position p2(45.0, 45.0, 0.0, 1.0, createTimestamp(2023, 1, 1, 0, 0, 0));
    double dist = p1.distanceTo(p2);
    EXPECT_GT(dist, 0);
}

TEST(PositionTest, ToString_FormatsCorrectly) {
    auto timestamp = createTimestamp(2023, 3, 14, 15, 9, 26);
    Position p(12.345678, -98.765432, 123.45, 5.67, timestamp);
    std::string str = p.toString();

    // Check that the string contains formatted values
    EXPECT_NE(str.find("Position("), std::string::npos);
    EXPECT_NE(str.find("lat=12.345678"), std::string::npos);
    EXPECT_NE(str.find("lon=-98.765432"), std::string::npos);
    EXPECT_NE(str.find("alt=123.45m"), std::string::npos);
    EXPECT_NE(str.find("acc=5.67m"), std::string::npos);
    EXPECT_NE(str.find("2023-03-14 15:09:26"), std::string::npos);
}

TEST(PositionTest, ToString_HandlesDifferentTimes) {
    auto timestamp = createTimestamp(2022, 12, 31, 23, 59, 59);
    Position p(0.0, 0.0, 0.0, 0.0, timestamp);
    std::string str = p.toString();
    EXPECT_NE(str.find("2022-12-31 23:59:59"), std::string::npos);
}

#endif
