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
    Timestamp createTimestamp(int year, int month, int day, int hour, int minute, int second) {
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
    
    // Compare timestamps by formatting them to the same string
    EXPECT_EQ(formatTimestamp(parsed1, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
    EXPECT_EQ(formatTimestamp(parsed2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
    
    // Test with just date
    auto dateOnly = parseTimestamp("2023-05-15", "%Y-%m-%d");
    EXPECT_EQ(formatTimestamp(dateOnly, "%Y-%m-%d"), "2023-05-15");
    
    // Test with just time
    auto timeOnly = parseTimestamp("14:30:45", "%H:%M:%S");
    EXPECT_EQ(formatTimestamp(timeOnly, "%H:%M:%S"), "14:30:45");
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
    
    // Test with same timestamp
    EXPECT_EQ(timestampDiffMinutes(t1, t1), 0);
    
    // Test with seconds that don't make a full minute
    auto t3 = createTimestamp(2023, 5, 15, 14, 15, 30);
    EXPECT_EQ(timestampDiffMinutes(t1, t3), 29);
}

TEST_F(TimeUtilsTest, TimestampDiffHours) {
    auto t1 = createTimestamp(2023, 5, 15, 18, 0, 0);
    auto t2 = createTimestamp(2023, 5, 15, 12, 0, 0);
    
    EXPECT_EQ(timestampDiffHours(t1, t2), 6);
    EXPECT_EQ(timestampDiffHours(t2, t1), -6);
    
    // Test with same timestamp
    EXPECT_EQ(timestampDiffHours(t1, t1), 0);
    
    // Test with minutes that don't make a full hour
    auto t3 = createTimestamp(2023, 5, 15, 12, 30, 0);
    EXPECT_EQ(timestampDiffHours(t1, t3), 5);
}

TEST_F(TimeUtilsTest, TimestampDiffDays) {
    auto t1 = createTimestamp(2023, 5, 20, 12, 0, 0);
    auto t2 = createTimestamp(2023, 5, 15, 12, 0, 0);
    
    EXPECT_EQ(timestampDiffDays(t1, t2), 5);
    EXPECT_EQ(timestampDiffDays(t2, t1), -5);
    
    // Test with same timestamp
    EXPECT_EQ(timestampDiffDays(t1, t1), 0);
    
    // Test with hours that don't make a full day
    auto t3 = createTimestamp(2023, 5, 15, 18, 0, 0);
    EXPECT_EQ(timestampDiffDays(t1, t3), 4);
}

TEST_F(TimeUtilsTest, AddSeconds) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    auto result1 = addSeconds(base, 30);
    EXPECT_EQ(formatTimestamp(result1, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:31:15");
    
    auto result2 = addSeconds(base, -30);
    EXPECT_EQ(formatTimestamp(result2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:15");
    
    auto result3 = addSeconds(base, 0);
    EXPECT_EQ(formatTimestamp(result3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
}

TEST_F(TimeUtilsTest, AddMinutes) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    auto result1 = addMinutes(base, 30);
    EXPECT_EQ(formatTimestamp(result1, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:00:45");
    
    auto result2 = addMinutes(base, -30);
    EXPECT_EQ(formatTimestamp(result2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:00:45");
    
    auto result3 = addMinutes(base, 0);
    EXPECT_EQ(formatTimestamp(result3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
}

TEST_F(TimeUtilsTest, AddHours) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    auto result1 = addHours(base, 12);
    EXPECT_EQ(formatTimestamp(result1, "%Y-%m-%d %H:%M:%S"), "2023-05-16 02:30:45");
    
    auto result2 = addHours(base, -12);
    EXPECT_EQ(formatTimestamp(result2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 02:30:45");
    
    auto result3 = addHours(base, 0);
    EXPECT_EQ(formatTimestamp(result3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
}

TEST_F(TimeUtilsTest, AddDays) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    auto result1 = addDays(base, 5);
    EXPECT_EQ(formatTimestamp(result1, "%Y-%m-%d %H:%M:%S"), "2023-05-20 14:30:45");
    
    auto result2 = addDays(base, -5);
    EXPECT_EQ(formatTimestamp(result2, "%Y-%m-%d %H:%M:%S"), "2023-05-10 14:30:45");
    
    auto result3 = addDays(base, 0);
    EXPECT_EQ(formatTimestamp(result3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
    
    // Test month rollover
    auto monthEnd = createTimestamp(2023, 5, 31, 12, 0, 0);
    auto nextMonth = addDays(monthEnd, 1);
    EXPECT_EQ(formatTimestamp(nextMonth, "%Y-%m-%d %H:%M:%S"), "2023-06-01 12:00:00");
    
    // Test year rollover
    auto yearEnd = createTimestamp(2023, 12, 31, 12, 0, 0);
    auto nextYear = addDays(yearEnd, 1);
    EXPECT_EQ(formatTimestamp(nextYear, "%Y-%m-%d %H:%M:%S"), "2024-01-01 12:00:00");
}

TEST_F(TimeUtilsTest, RoundTripConversion) {
    // Test that formatting and parsing a timestamp gives the original timestamp
    auto original = createTimestamp(2023, 5, 15, 14, 30, 45);
    std::string formatted = formatTimestamp(original, "%Y-%m-%d %H:%M:%S");
    auto parsed = parseTimestamp(formatted, "%Y-%m-%d %H:%M:%S");
    
    // Compare the timestamps by formatting them to strings
    EXPECT_EQ(formatTimestamp(original, "%Y-%m-%d %H:%M:%S"), 
              formatTimestamp(parsed, "%Y-%m-%d %H:%M:%S"));
}

} // namespace testing
} // namespace equipment_tracker
// </test_code>