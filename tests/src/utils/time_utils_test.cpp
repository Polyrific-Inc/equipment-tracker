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
    // Helper method to create a timestamp for a specific date/time
    Timestamp createTimestamp(int year, int month, int day, 
                             int hour = 0, int minute = 0, int second = 0) {
        std::tm tm = {};
        tm.tm_year = year - 1900;  // Years since 1900
        tm.tm_mon = month - 1;     // Months since January (0-11)
        tm.tm_mday = day;          // Day of the month (1-31)
        tm.tm_hour = hour;         // Hours (0-23)
        tm.tm_min = minute;        // Minutes (0-59)
        tm.tm_sec = second;        // Seconds (0-59)
        
        std::time_t time = std::mktime(&tm);
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
    auto timestamp1 = parseTimestamp("2023-05-15", "%Y-%m-%d");
    auto expected1 = createTimestamp(2023, 5, 15);
    
    auto timestamp2 = parseTimestamp("2023-05-15 14:30:45", "%Y-%m-%d %H:%M:%S");
    auto expected2 = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Compare timestamps by formatting them back to strings for reliable comparison
    EXPECT_EQ(formatTimestamp(timestamp1, "%Y-%m-%d"), "2023-05-15");
    EXPECT_EQ(formatTimestamp(timestamp2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
}

TEST_F(TimeUtilsTest, TimestampDiffSeconds) {
    auto t1 = createTimestamp(2023, 5, 15, 14, 30, 45);
    auto t2 = createTimestamp(2023, 5, 15, 14, 30, 15);
    
    // t1 is 30 seconds after t2
    EXPECT_EQ(timestampDiffSeconds(t1, t2), 30);
    // t2 is 30 seconds before t1
    EXPECT_EQ(timestampDiffSeconds(t2, t1), -30);
    // Same timestamp should have 0 difference
    EXPECT_EQ(timestampDiffSeconds(t1, t1), 0);
}

TEST_F(TimeUtilsTest, TimestampDiffMinutes) {
    auto t1 = createTimestamp(2023, 5, 15, 14, 45, 0);
    auto t2 = createTimestamp(2023, 5, 15, 14, 15, 0);
    
    // t1 is 30 minutes after t2
    EXPECT_EQ(timestampDiffMinutes(t1, t2), 30);
    // t2 is 30 minutes before t1
    EXPECT_EQ(timestampDiffMinutes(t2, t1), -30);
    // Same timestamp should have 0 difference
    EXPECT_EQ(timestampDiffMinutes(t1, t1), 0);
}

TEST_F(TimeUtilsTest, TimestampDiffHours) {
    auto t1 = createTimestamp(2023, 5, 15, 18, 0, 0);
    auto t2 = createTimestamp(2023, 5, 15, 12, 0, 0);
    
    // t1 is 6 hours after t2
    EXPECT_EQ(timestampDiffHours(t1, t2), 6);
    // t2 is 6 hours before t1
    EXPECT_EQ(timestampDiffHours(t2, t1), -6);
    // Same timestamp should have 0 difference
    EXPECT_EQ(timestampDiffHours(t1, t1), 0);
}

TEST_F(TimeUtilsTest, TimestampDiffDays) {
    auto t1 = createTimestamp(2023, 5, 20, 12, 0, 0);
    auto t2 = createTimestamp(2023, 5, 15, 12, 0, 0);
    
    // t1 is 5 days after t2
    EXPECT_EQ(timestampDiffDays(t1, t2), 5);
    // t2 is 5 days before t1
    EXPECT_EQ(timestampDiffDays(t2, t1), -5);
    // Same timestamp should have 0 difference
    EXPECT_EQ(timestampDiffDays(t1, t1), 0);
}

TEST_F(TimeUtilsTest, AddSeconds) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Add 30 seconds
    auto result = addSeconds(base, 30);
    EXPECT_EQ(formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:31:15");
    
    // Add negative seconds (subtract)
    result = addSeconds(base, -30);
    EXPECT_EQ(formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:15");
    
    // Add 0 seconds (no change)
    result = addSeconds(base, 0);
    EXPECT_EQ(formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
}

TEST_F(TimeUtilsTest, AddMinutes) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Add 30 minutes
    auto result = addMinutes(base, 30);
    EXPECT_EQ(formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:00:45");
    
    // Add negative minutes (subtract)
    result = addMinutes(base, -30);
    EXPECT_EQ(formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:00:45");
    
    // Add 0 minutes (no change)
    result = addMinutes(base, 0);
    EXPECT_EQ(formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
}

TEST_F(TimeUtilsTest, AddHours) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Add 5 hours
    auto result = addHours(base, 5);
    EXPECT_EQ(formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 19:30:45");
    
    // Add 12 hours (crosses day boundary)
    result = addHours(base, 12);
    EXPECT_EQ(formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-16 02:30:45");
    
    // Add negative hours (subtract)
    result = addHours(base, -5);
    EXPECT_EQ(formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 09:30:45");
    
    // Add 0 hours (no change)
    result = addHours(base, 0);
    EXPECT_EQ(formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
}

TEST_F(TimeUtilsTest, AddDays) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Add 5 days
    auto result = addDays(base, 5);
    EXPECT_EQ(formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-20 14:30:45");
    
    // Add 20 days (crosses month boundary)
    result = addDays(base, 20);
    EXPECT_EQ(formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-06-04 14:30:45");
    
    // Add negative days (subtract)
    result = addDays(base, -5);
    EXPECT_EQ(formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-10 14:30:45");
    
    // Add 0 days (no change)
    result = addDays(base, 0);
    EXPECT_EQ(formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
}

TEST_F(TimeUtilsTest, EdgeCases) {
    // Test with very large time differences
    auto past = createTimestamp(1970, 1, 1);
    auto future = createTimestamp(2100, 1, 1);
    
    // Large time differences should still work
    EXPECT_GT(timestampDiffDays(future, past), 0);
    EXPECT_LT(timestampDiffDays(past, future), 0);
    
    // Test with very large additions
    auto base = createTimestamp(2023, 5, 15);
    auto result = addDays(base, 365 * 10); // Add 10 years worth of days
    EXPECT_EQ(formatTimestamp(result, "%Y"), "2033");
}

} // namespace testing
} // namespace equipment_tracker
// </test_code>