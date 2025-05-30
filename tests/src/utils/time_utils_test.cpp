// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/utils/time_utils.h"
#include "equipment_tracker/utils/types.h"
#include <chrono>
#include <thread>
#include <ctime>

namespace equipment_tracker {
namespace testing {

class TimeUtilsTest : public ::testing::Test {
protected:
    // Helper function to create a timestamp for a specific date/time
    Timestamp createTimestamp(int year, int month, int day, int hour = 0, int minute = 0, int second = 0) {
        std::tm timeinfo = {};
        timeinfo.tm_year = year - 1900;  // Years since 1900
        timeinfo.tm_mon = month - 1;     // Months since January (0-11)
        timeinfo.tm_mday = day;          // Day of the month (1-31)
        timeinfo.tm_hour = hour;         // Hours (0-23)
        timeinfo.tm_min = minute;        // Minutes (0-59)
        timeinfo.tm_sec = second;        // Seconds (0-59)
        timeinfo.tm_isdst = -1;          // Let system determine DST

        std::time_t time = std::mktime(&timeinfo);
        return std::chrono::system_clock::from_time_t(time);
    }
};

TEST_F(TimeUtilsTest, GetCurrentTimestamp) {
    // Test that getCurrentTimestamp returns a timestamp close to now
    auto before = std::chrono::system_clock::now();
    auto timestamp = getCurrentTimestamp();
    auto after = std::chrono::system_clock::now();
    
    // The timestamp should be between before and after
    EXPECT_LE(before, timestamp);
    EXPECT_LE(timestamp, after);
}

TEST_F(TimeUtilsTest, FormatTimestamp) {
    // Create a fixed timestamp for testing (2023-05-15 14:30:45)
    auto timestamp = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Test various format strings
    EXPECT_EQ(formatTimestamp(timestamp, "%Y-%m-%d"), "2023-05-15");
    EXPECT_EQ(formatTimestamp(timestamp, "%H:%M:%S"), "14:30:45");
    EXPECT_EQ(formatTimestamp(timestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
    EXPECT_EQ(formatTimestamp(timestamp, "%a, %d %b %Y"), "Mon, 15 May 2023");
}

TEST_F(TimeUtilsTest, ParseTimestamp) {
    // Test parsing various timestamp formats
    auto expected = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    auto parsed1 = parseTimestamp("2023-05-15 14:30:45", "%Y-%m-%d %H:%M:%S");
    auto parsed2 = parseTimestamp("15/05/2023 14:30:45", "%d/%m/%Y %H:%M:%S");
    
    // Compare timestamps by formatting them to a common string representation
    EXPECT_EQ(formatTimestamp(parsed1, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
    EXPECT_EQ(formatTimestamp(parsed2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
    
    // Test with incomplete format (missing seconds)
    auto parsed3 = parseTimestamp("2023-05-15 14:30", "%Y-%m-%d %H:%M");
    EXPECT_EQ(formatTimestamp(parsed3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:00");
}

TEST_F(TimeUtilsTest, TimestampDiffSeconds) {
    auto t1 = createTimestamp(2023, 5, 15, 14, 30, 45);
    auto t2 = createTimestamp(2023, 5, 15, 14, 30, 15);
    
    EXPECT_EQ(timestampDiffSeconds(t1, t2), 30);
    EXPECT_EQ(timestampDiffSeconds(t2, t1), -30);
    
    // Test with same timestamp
    EXPECT_EQ(timestampDiffSeconds(t1, t1), 0);
}

TEST_F(TimeUtilsTest, TimestampDiffMinutes) {
    auto t1 = createTimestamp(2023, 5, 15, 14, 45, 0);
    auto t2 = createTimestamp(2023, 5, 15, 14, 15, 0);
    
    EXPECT_EQ(timestampDiffMinutes(t1, t2), 30);
    EXPECT_EQ(timestampDiffMinutes(t2, t1), -30);
    
    // Test with partial minutes
    auto t3 = createTimestamp(2023, 5, 15, 14, 45, 30);
    auto t4 = createTimestamp(2023, 5, 15, 14, 15, 15);
    EXPECT_EQ(timestampDiffMinutes(t3, t4), 30); // Truncated to minutes
}

TEST_F(TimeUtilsTest, TimestampDiffHours) {
    auto t1 = createTimestamp(2023, 5, 15, 18, 0, 0);
    auto t2 = createTimestamp(2023, 5, 15, 12, 0, 0);
    
    EXPECT_EQ(timestampDiffHours(t1, t2), 6);
    EXPECT_EQ(timestampDiffHours(t2, t1), -6);
    
    // Test with partial hours
    auto t3 = createTimestamp(2023, 5, 15, 18, 30, 0);
    auto t4 = createTimestamp(2023, 5, 15, 12, 15, 0);
    EXPECT_EQ(timestampDiffHours(t3, t4), 6); // Truncated to hours
}

TEST_F(TimeUtilsTest, TimestampDiffDays) {
    auto t1 = createTimestamp(2023, 5, 20, 12, 0, 0);
    auto t2 = createTimestamp(2023, 5, 15, 12, 0, 0);
    
    EXPECT_EQ(timestampDiffDays(t1, t2), 5);
    EXPECT_EQ(timestampDiffDays(t2, t1), -5);
    
    // Test with partial days
    auto t3 = createTimestamp(2023, 5, 20, 18, 0, 0);
    auto t4 = createTimestamp(2023, 5, 15, 6, 0, 0);
    EXPECT_EQ(timestampDiffDays(t3, t4), 5); // Should be 5.5 days, but truncated to 5
}

TEST_F(TimeUtilsTest, AddSeconds) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    auto result1 = addSeconds(base, 30);
    EXPECT_EQ(formatTimestamp(result1, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:31:15");
    
    auto result2 = addSeconds(base, -30);
    EXPECT_EQ(formatTimestamp(result2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:15");
    
    auto result3 = addSeconds(base, 3600);
    EXPECT_EQ(formatTimestamp(result3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:30:45");
}

TEST_F(TimeUtilsTest, AddMinutes) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    auto result1 = addMinutes(base, 30);
    EXPECT_EQ(formatTimestamp(result1, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:00:45");
    
    auto result2 = addMinutes(base, -30);
    EXPECT_EQ(formatTimestamp(result2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:00:45");
    
    auto result3 = addMinutes(base, 60);
    EXPECT_EQ(formatTimestamp(result3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:30:45");
}

TEST_F(TimeUtilsTest, AddHours) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    auto result1 = addHours(base, 5);
    EXPECT_EQ(formatTimestamp(result1, "%Y-%m-%d %H:%M:%S"), "2023-05-15 19:30:45");
    
    auto result2 = addHours(base, -5);
    EXPECT_EQ(formatTimestamp(result2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 09:30:45");
    
    auto result3 = addHours(base, 24);
    EXPECT_EQ(formatTimestamp(result3, "%Y-%m-%d %H:%M:%S"), "2023-05-16 14:30:45");
}

TEST_F(TimeUtilsTest, AddDays) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    auto result1 = addDays(base, 5);
    EXPECT_EQ(formatTimestamp(result1, "%Y-%m-%d %H:%M:%S"), "2023-05-20 14:30:45");
    
    auto result2 = addDays(base, -5);
    EXPECT_EQ(formatTimestamp(result2, "%Y-%m-%d %H:%M:%S"), "2023-05-10 14:30:45");
    
    auto result3 = addDays(base, 30);
    EXPECT_EQ(formatTimestamp(result3, "%Y-%m-%d %H:%M:%S"), "2023-06-14 14:30:45");
}

TEST_F(TimeUtilsTest, EdgeCases) {
    // Test year boundary
    auto newYearsEve = createTimestamp(2023, 12, 31, 23, 59, 59);
    auto newYear = addSeconds(newYearsEve, 1);
    EXPECT_EQ(formatTimestamp(newYear, "%Y-%m-%d %H:%M:%S"), "2024-01-01 00:00:00");
    
    // Test month boundary
    auto endOfMonth = createTimestamp(2023, 4, 30, 23, 59, 59);
    auto nextMonth = addSeconds(endOfMonth, 1);
    EXPECT_EQ(formatTimestamp(nextMonth, "%Y-%m-%d %H:%M:%S"), "2023-05-01 00:00:00");
    
    // Test leap year (2024 is a leap year)
    auto febLeapYear = createTimestamp(2024, 2, 28, 23, 59, 59);
    auto leapDay = addSeconds(febLeapYear, 1);
    EXPECT_EQ(formatTimestamp(leapDay, "%Y-%m-%d %H:%M:%S"), "2024-02-29 00:00:00");
    
    // Test non-leap year (2023 is not a leap year)
    auto febNonLeapYear = createTimestamp(2023, 2, 28, 23, 59, 59);
    auto marchFirst = addSeconds(febNonLeapYear, 1);
    EXPECT_EQ(formatTimestamp(marchFirst, "%Y-%m-%d %H:%M:%S"), "2023-03-01 00:00:00");
}

} // namespace testing
} // namespace equipment_tracker
// </test_code>