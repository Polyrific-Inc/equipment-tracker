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
    Timestamp createTimestamp(int year, int month, int day, 
                             int hour = 0, int minute = 0, int second = 0) {
        std::tm timeinfo = {};
        timeinfo.tm_year = year - 1900; // Years since 1900
        timeinfo.tm_mon = month - 1;    // Months since January (0-11)
        timeinfo.tm_mday = day;         // Day of the month (1-31)
        timeinfo.tm_hour = hour;        // Hours (0-23)
        timeinfo.tm_min = minute;       // Minutes (0-59)
        timeinfo.tm_sec = second;       // Seconds (0-59)
        timeinfo.tm_isdst = -1;         // Let system determine DST

        std::time_t time = std::mktime(&timeinfo);
        return std::chrono::system_clock::from_time_t(time);
    }
};

TEST_F(TimeUtilsTest, GetCurrentTimestamp) {
    // Test that getCurrentTimestamp returns a valid timestamp
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
    
    // Compare timestamps with a small tolerance for potential platform differences
    EXPECT_NEAR(timestampDiffSeconds(expected, parsed1), 0, 1);
    EXPECT_NEAR(timestampDiffSeconds(expected, parsed2), 0, 1);
}

TEST_F(TimeUtilsTest, TimestampDiffSeconds) {
    auto t1 = createTimestamp(2023, 5, 15, 14, 30, 45);
    auto t2 = createTimestamp(2023, 5, 15, 14, 30, 15);
    
    EXPECT_EQ(timestampDiffSeconds(t1, t2), 30);
    EXPECT_EQ(timestampDiffSeconds(t2, t1), -30);
}

TEST_F(TimeUtilsTest, TimestampDiffMinutes) {
    auto t1 = createTimestamp(2023, 5, 15, 14, 45, 0);
    auto t2 = createTimestamp(2023, 5, 15, 14, 15, 0);
    
    EXPECT_EQ(timestampDiffMinutes(t1, t2), 30);
    EXPECT_EQ(timestampDiffMinutes(t2, t1), -30);
    
    // Test with seconds that don't align with minute boundaries
    auto t3 = createTimestamp(2023, 5, 15, 14, 45, 30);
    auto t4 = createTimestamp(2023, 5, 15, 14, 15, 15);
    
    EXPECT_EQ(timestampDiffMinutes(t3, t4), 30);
}

TEST_F(TimeUtilsTest, TimestampDiffHours) {
    auto t1 = createTimestamp(2023, 5, 15, 18, 0, 0);
    auto t2 = createTimestamp(2023, 5, 15, 12, 0, 0);
    
    EXPECT_EQ(timestampDiffHours(t1, t2), 6);
    EXPECT_EQ(timestampDiffHours(t2, t1), -6);
    
    // Test with minutes that don't align with hour boundaries
    auto t3 = createTimestamp(2023, 5, 15, 18, 30, 0);
    auto t4 = createTimestamp(2023, 5, 15, 12, 15, 0);
    
    EXPECT_EQ(timestampDiffHours(t3, t4), 6);
}

TEST_F(TimeUtilsTest, TimestampDiffDays) {
    auto t1 = createTimestamp(2023, 5, 20, 12, 0, 0);
    auto t2 = createTimestamp(2023, 5, 15, 12, 0, 0);
    
    EXPECT_EQ(timestampDiffDays(t1, t2), 5);
    EXPECT_EQ(timestampDiffDays(t2, t1), -5);
    
    // Test with hours that don't align with day boundaries
    auto t3 = createTimestamp(2023, 5, 20, 18, 0, 0);
    auto t4 = createTimestamp(2023, 5, 15, 6, 0, 0);
    
    // 5 days and 12 hours = 5.5 days, but we expect integer division
    EXPECT_EQ(timestampDiffDays(t3, t4), 5);
}

TEST_F(TimeUtilsTest, AddSeconds) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    auto result1 = addSeconds(base, 30);
    EXPECT_EQ(formatTimestamp(result1, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:31:15");
    
    auto result2 = addSeconds(base, -30);
    EXPECT_EQ(formatTimestamp(result2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:15");
    
    // Test adding seconds that cross minute boundary
    auto result3 = addSeconds(base, 20);
    EXPECT_EQ(formatTimestamp(result3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:31:05");
}

TEST_F(TimeUtilsTest, AddMinutes) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    auto result1 = addMinutes(base, 45);
    EXPECT_EQ(formatTimestamp(result1, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:15:45");
    
    auto result2 = addMinutes(base, -45);
    EXPECT_EQ(formatTimestamp(result2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 13:45:45");
    
    // Test adding minutes that cross hour boundary
    auto result3 = addMinutes(base, 35);
    EXPECT_EQ(formatTimestamp(result3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:05:45");
}

TEST_F(TimeUtilsTest, AddHours) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    auto result1 = addHours(base, 12);
    EXPECT_EQ(formatTimestamp(result1, "%Y-%m-%d %H:%M:%S"), "2023-05-16 02:30:45");
    
    auto result2 = addHours(base, -12);
    EXPECT_EQ(formatTimestamp(result2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 02:30:45");
    
    // Test adding hours that cross day boundary
    auto result3 = addHours(base, 10);
    EXPECT_EQ(formatTimestamp(result3, "%Y-%m-%d %H:%M:%S"), "2023-05-16 00:30:45");
}

TEST_F(TimeUtilsTest, AddDays) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    auto result1 = addDays(base, 5);
    EXPECT_EQ(formatTimestamp(result1, "%Y-%m-%d %H:%M:%S"), "2023-05-20 14:30:45");
    
    auto result2 = addDays(base, -5);
    EXPECT_EQ(formatTimestamp(result2, "%Y-%m-%d %H:%M:%S"), "2023-05-10 14:30:45");
    
    // Test adding days that cross month boundary
    auto result3 = addDays(base, 20);
    EXPECT_EQ(formatTimestamp(result3, "%Y-%m-%d %H:%M:%S"), "2023-06-04 14:30:45");
    
    // Test adding days that cross year boundary
    auto yearEnd = createTimestamp(2023, 12, 25, 12, 0, 0);
    auto nextYear = addDays(yearEnd, 10);
    EXPECT_EQ(formatTimestamp(nextYear, "%Y-%m-%d %H:%M:%S"), "2024-01-04 12:00:00");
}

} // namespace testing
} // namespace equipment_tracker
// </test_code>