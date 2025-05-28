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
    // Helper function to create a timestamp for a specific date/time
    Timestamp createTimestamp(int year, int month, int day, int hour = 0, int minute = 0, int second = 0) {
        std::tm timeinfo = {};
        timeinfo.tm_year = year - 1900;  // Years since 1900
        timeinfo.tm_mon = month - 1;     // Months since January (0-11)
        timeinfo.tm_mday = day;
        timeinfo.tm_hour = hour;
        timeinfo.tm_min = minute;
        timeinfo.tm_sec = second;
        timeinfo.tm_isdst = -1;          // Let system determine DST status

        std::time_t time_t = std::mktime(&timeinfo);
        return std::chrono::system_clock::from_time_t(time_t);
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
    // Create a fixed timestamp for testing
    auto timestamp = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Test different format strings
    EXPECT_EQ(formatTimestamp(timestamp, "%Y-%m-%d"), "2023-05-15");
    EXPECT_EQ(formatTimestamp(timestamp, "%H:%M:%S"), "14:30:45");
    EXPECT_EQ(formatTimestamp(timestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
    EXPECT_EQ(formatTimestamp(timestamp, "%a, %d %b %Y"), "Mon, 15 May 2023");
}

TEST_F(TimestampUtilsTest, ParseTimestamp) {
    // Test parsing with different format strings
    auto timestamp1 = parseTimestamp("2023-05-15", "%Y-%m-%d");
    auto timestamp2 = parseTimestamp("14:30:45", "%H:%M:%S");
    auto timestamp3 = parseTimestamp("2023-05-15 14:30:45", "%Y-%m-%d %H:%M:%S");
    
    // Verify the parsed timestamps
    EXPECT_EQ(formatTimestamp(timestamp1, "%Y-%m-%d"), "2023-05-15");
    EXPECT_EQ(formatTimestamp(timestamp2, "%H:%M:%S"), "14:30:45");
    EXPECT_EQ(formatTimestamp(timestamp3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
}

TEST_F(TimestampUtilsTest, TimestampDiffSeconds) {
    auto t1 = createTimestamp(2023, 5, 15, 14, 30, 45);
    auto t2 = createTimestamp(2023, 5, 15, 14, 30, 15);
    
    EXPECT_EQ(timestampDiffSeconds(t1, t2), 30);
    EXPECT_EQ(timestampDiffSeconds(t2, t1), -30);
    
    // Test with zero difference
    auto t3 = createTimestamp(2023, 5, 15, 14, 30, 45);
    EXPECT_EQ(timestampDiffSeconds(t1, t3), 0);
}

TEST_F(TimestampUtilsTest, TimestampDiffMinutes) {
    auto t1 = createTimestamp(2023, 5, 15, 14, 45, 0);
    auto t2 = createTimestamp(2023, 5, 15, 14, 15, 0);
    
    EXPECT_EQ(timestampDiffMinutes(t1, t2), 30);
    EXPECT_EQ(timestampDiffMinutes(t2, t1), -30);
    
    // Test with partial minutes (should truncate)
    auto t3 = createTimestamp(2023, 5, 15, 14, 45, 30);
    auto t4 = createTimestamp(2023, 5, 15, 14, 15, 15);
    EXPECT_EQ(timestampDiffMinutes(t3, t4), 30);
}

TEST_F(TimestampUtilsTest, TimestampDiffHours) {
    auto t1 = createTimestamp(2023, 5, 15, 18, 0, 0);
    auto t2 = createTimestamp(2023, 5, 15, 12, 0, 0);
    
    EXPECT_EQ(timestampDiffHours(t1, t2), 6);
    EXPECT_EQ(timestampDiffHours(t2, t1), -6);
    
    // Test with partial hours (should truncate)
    auto t3 = createTimestamp(2023, 5, 15, 18, 30, 0);
    auto t4 = createTimestamp(2023, 5, 15, 12, 15, 0);
    EXPECT_EQ(timestampDiffHours(t3, t4), 6);
}

TEST_F(TimestampUtilsTest, TimestampDiffDays) {
    auto t1 = createTimestamp(2023, 5, 20, 12, 0, 0);
    auto t2 = createTimestamp(2023, 5, 15, 12, 0, 0);
    
    EXPECT_EQ(timestampDiffDays(t1, t2), 5);
    EXPECT_EQ(timestampDiffDays(t2, t1), -5);
    
    // Test with partial days (should truncate)
    auto t3 = createTimestamp(2023, 5, 20, 18, 0, 0);
    auto t4 = createTimestamp(2023, 5, 15, 6, 0, 0);
    EXPECT_EQ(timestampDiffDays(t3, t4), 5);
}

TEST_F(TimestampUtilsTest, AddSeconds) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Add positive seconds
    auto result1 = addSeconds(base, 30);
    EXPECT_EQ(formatTimestamp(result1, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:31:15");
    
    // Add negative seconds
    auto result2 = addSeconds(base, -30);
    EXPECT_EQ(formatTimestamp(result2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:15");
    
    // Add zero seconds
    auto result3 = addSeconds(base, 0);
    EXPECT_EQ(formatTimestamp(result3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
    
    // Add large number of seconds
    auto result4 = addSeconds(base, 3600);
    EXPECT_EQ(formatTimestamp(result4, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:30:45");
}

TEST_F(TimestampUtilsTest, AddMinutes) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Add positive minutes
    auto result1 = addMinutes(base, 30);
    EXPECT_EQ(formatTimestamp(result1, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:00:45");
    
    // Add negative minutes
    auto result2 = addMinutes(base, -30);
    EXPECT_EQ(formatTimestamp(result2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:00:45");
    
    // Add zero minutes
    auto result3 = addMinutes(base, 0);
    EXPECT_EQ(formatTimestamp(result3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
    
    // Add large number of minutes
    auto result4 = addMinutes(base, 60);
    EXPECT_EQ(formatTimestamp(result4, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:30:45");
}

TEST_F(TimestampUtilsTest, AddHours) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Add positive hours
    auto result1 = addHours(base, 5);
    EXPECT_EQ(formatTimestamp(result1, "%Y-%m-%d %H:%M:%S"), "2023-05-15 19:30:45");
    
    // Add negative hours
    auto result2 = addHours(base, -5);
    EXPECT_EQ(formatTimestamp(result2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 09:30:45");
    
    // Add zero hours
    auto result3 = addHours(base, 0);
    EXPECT_EQ(formatTimestamp(result3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
    
    // Add hours crossing day boundary
    auto result4 = addHours(base, 12);
    EXPECT_EQ(formatTimestamp(result4, "%Y-%m-%d %H:%M:%S"), "2023-05-16 02:30:45");
}

TEST_F(TimestampUtilsTest, AddDays) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Add positive days
    auto result1 = addDays(base, 5);
    EXPECT_EQ(formatTimestamp(result1, "%Y-%m-%d %H:%M:%S"), "2023-05-20 14:30:45");
    
    // Add negative days
    auto result2 = addDays(base, -5);
    EXPECT_EQ(formatTimestamp(result2, "%Y-%m-%d %H:%M:%S"), "2023-05-10 14:30:45");
    
    // Add zero days
    auto result3 = addDays(base, 0);
    EXPECT_EQ(formatTimestamp(result3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
    
    // Add days crossing month boundary
    auto result4 = addDays(base, 20);
    EXPECT_EQ(formatTimestamp(result4, "%Y-%m-%d %H:%M:%S"), "2023-06-04 14:30:45");
}

} // namespace equipment_tracker
// </test_code>