// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include "equipment_tracker/utils/types.h"

namespace equipment_tracker {

class TimestampUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set a fixed timezone for consistent test results
#ifdef _WIN32
        _putenv_s("TZ", "UTC");
        _tzset();
#else
        setenv("TZ", "UTC", 1);
        tzset();
#endif
    }

    void TearDown() override {
        // Reset timezone
#ifdef _WIN32
        _putenv_s("TZ", "");
        _tzset();
#else
        unsetenv("TZ");
        tzset();
#endif
    }
};

TEST_F(TimestampUtilsTest, GetCurrentTimestamp) {
    auto before = std::chrono::system_clock::now();
    auto current = getCurrentTimestamp();
    auto after = std::chrono::system_clock::now();
    
    // Current timestamp should be between before and after
    EXPECT_LE(before, current);
    EXPECT_LE(current, after);
}

TEST_F(TimestampUtilsTest, FormatTimestamp) {
    // Create a fixed timestamp for testing (2023-05-15 10:30:45 UTC)
    std::tm timeinfo = {};
    timeinfo.tm_year = 2023 - 1900;
    timeinfo.tm_mon = 5 - 1;
    timeinfo.tm_mday = 15;
    timeinfo.tm_hour = 10;
    timeinfo.tm_min = 30;
    timeinfo.tm_sec = 45;
    
    std::time_t time_t = std::mktime(&timeinfo);
    Timestamp timestamp = std::chrono::system_clock::from_time_t(time_t);
    
    // Test various formats
    EXPECT_EQ(formatTimestamp(timestamp, "%Y-%m-%d"), "2023-05-15");
    EXPECT_EQ(formatTimestamp(timestamp, "%H:%M:%S"), "10:30:45");
    EXPECT_EQ(formatTimestamp(timestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:45");
    EXPECT_EQ(formatTimestamp(timestamp, "%a, %d %b %Y"), "Mon, 15 May 2023");
}

TEST_F(TimestampUtilsTest, ParseTimestamp) {
    // Test parsing with different formats
    Timestamp ts1 = parseTimestamp("2023-05-15", "%Y-%m-%d");
    EXPECT_EQ(formatTimestamp(ts1, "%Y-%m-%d"), "2023-05-15");
    
    Timestamp ts2 = parseTimestamp("10:30:45", "%H:%M:%S");
    EXPECT_EQ(formatTimestamp(ts2, "%H:%M:%S"), "10:30:45");
    
    Timestamp ts3 = parseTimestamp("2023-05-15 10:30:45", "%Y-%m-%d %H:%M:%S");
    EXPECT_EQ(formatTimestamp(ts3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:45");
}

TEST_F(TimestampUtilsTest, TimestampDiffSeconds) {
    Timestamp t1 = parseTimestamp("2023-05-15 10:30:45", "%Y-%m-%d %H:%M:%S");
    Timestamp t2 = parseTimestamp("2023-05-15 10:30:15", "%Y-%m-%d %H:%M:%S");
    
    EXPECT_EQ(timestampDiffSeconds(t1, t2), 30);
    EXPECT_EQ(timestampDiffSeconds(t2, t1), -30);
    
    // Test with same timestamp
    EXPECT_EQ(timestampDiffSeconds(t1, t1), 0);
}

TEST_F(TimestampUtilsTest, TimestampDiffMinutes) {
    Timestamp t1 = parseTimestamp("2023-05-15 10:30:00", "%Y-%m-%d %H:%M:%S");
    Timestamp t2 = parseTimestamp("2023-05-15 10:00:00", "%Y-%m-%d %H:%M:%S");
    
    EXPECT_EQ(timestampDiffMinutes(t1, t2), 30);
    EXPECT_EQ(timestampDiffMinutes(t2, t1), -30);
    
    // Test with same timestamp
    EXPECT_EQ(timestampDiffMinutes(t1, t1), 0);
}

TEST_F(TimestampUtilsTest, TimestampDiffHours) {
    Timestamp t1 = parseTimestamp("2023-05-15 10:00:00", "%Y-%m-%d %H:%M:%S");
    Timestamp t2 = parseTimestamp("2023-05-15 07:00:00", "%Y-%m-%d %H:%M:%S");
    
    EXPECT_EQ(timestampDiffHours(t1, t2), 3);
    EXPECT_EQ(timestampDiffHours(t2, t1), -3);
    
    // Test with same timestamp
    EXPECT_EQ(timestampDiffHours(t1, t1), 0);
}

TEST_F(TimestampUtilsTest, TimestampDiffDays) {
    Timestamp t1 = parseTimestamp("2023-05-15 00:00:00", "%Y-%m-%d %H:%M:%S");
    Timestamp t2 = parseTimestamp("2023-05-12 00:00:00", "%Y-%m-%d %H:%M:%S");
    
    EXPECT_EQ(timestampDiffDays(t1, t2), 3);
    EXPECT_EQ(timestampDiffDays(t2, t1), -3);
    
    // Test with same timestamp
    EXPECT_EQ(timestampDiffDays(t1, t1), 0);
    
    // Test with partial days
    Timestamp t3 = parseTimestamp("2023-05-15 12:00:00", "%Y-%m-%d %H:%M:%S");
    Timestamp t4 = parseTimestamp("2023-05-12 00:00:00", "%Y-%m-%d %H:%M:%S");
    
    EXPECT_EQ(timestampDiffDays(t3, t4), 3); // Should round down to 3 days
}

TEST_F(TimestampUtilsTest, AddSeconds) {
    Timestamp t1 = parseTimestamp("2023-05-15 10:30:45", "%Y-%m-%d %H:%M:%S");
    
    Timestamp t2 = addSeconds(t1, 30);
    EXPECT_EQ(formatTimestamp(t2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:31:15");
    
    Timestamp t3 = addSeconds(t1, -30);
    EXPECT_EQ(formatTimestamp(t3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:15");
    
    // Test with zero
    Timestamp t4 = addSeconds(t1, 0);
    EXPECT_EQ(formatTimestamp(t4, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:45");
}

TEST_F(TimestampUtilsTest, AddMinutes) {
    Timestamp t1 = parseTimestamp("2023-05-15 10:30:45", "%Y-%m-%d %H:%M:%S");
    
    Timestamp t2 = addMinutes(t1, 30);
    EXPECT_EQ(formatTimestamp(t2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 11:00:45");
    
    Timestamp t3 = addMinutes(t1, -30);
    EXPECT_EQ(formatTimestamp(t3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:00:45");
    
    // Test with zero
    Timestamp t4 = addMinutes(t1, 0);
    EXPECT_EQ(formatTimestamp(t4, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:45");
}

TEST_F(TimestampUtilsTest, AddHours) {
    Timestamp t1 = parseTimestamp("2023-05-15 10:30:45", "%Y-%m-%d %H:%M:%S");
    
    Timestamp t2 = addHours(t1, 5);
    EXPECT_EQ(formatTimestamp(t2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:30:45");
    
    Timestamp t3 = addHours(t1, -5);
    EXPECT_EQ(formatTimestamp(t3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 05:30:45");
    
    // Test day boundary
    Timestamp t4 = addHours(t1, 15);
    EXPECT_EQ(formatTimestamp(t4, "%Y-%m-%d %H:%M:%S"), "2023-05-16 01:30:45");
    
    // Test with zero
    Timestamp t5 = addHours(t1, 0);
    EXPECT_EQ(formatTimestamp(t5, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:45");
}

TEST_F(TimestampUtilsTest, AddDays) {
    Timestamp t1 = parseTimestamp("2023-05-15 10:30:45", "%Y-%m-%d %H:%M:%S");
    
    Timestamp t2 = addDays(t1, 3);
    EXPECT_EQ(formatTimestamp(t2, "%Y-%m-%d %H:%M:%S"), "2023-05-18 10:30:45");
    
    Timestamp t3 = addDays(t1, -3);
    EXPECT_EQ(formatTimestamp(t3, "%Y-%m-%d %H:%M:%S"), "2023-05-12 10:30:45");
    
    // Test month boundary
    Timestamp t4 = addDays(t1, 20);
    EXPECT_EQ(formatTimestamp(t4, "%Y-%m-%d %H:%M:%S"), "2023-06-04 10:30:45");
    
    // Test with zero
    Timestamp t5 = addDays(t1, 0);
    EXPECT_EQ(formatTimestamp(t5, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:45");
}

} // namespace equipment_tracker
// </test_code>