// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/utils/time_utils.h"
#include "equipment_tracker/utils/types.h"
#include <chrono>
#include <thread>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace equipment_tracker {
namespace testing {

class TimeUtilsTest : public ::testing::Test {
protected:
    // Helper function to create a timestamp for a specific date/time
    Timestamp createTimestamp(int year, int month, int day, int hour, int minute, int second) {
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
    // Get current timestamp
    auto timestamp = getCurrentTimestamp();
    
    // Get current time for comparison
    auto now = std::chrono::system_clock::now();
    
    // The difference should be very small (less than 1 second)
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - timestamp).count();
    EXPECT_LT(std::abs(diff), 1000);
}

TEST_F(TimeUtilsTest, FormatTimestamp) {
    // Create a fixed timestamp for testing (2023-05-15 14:30:45)
    auto timestamp = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Test different format strings
    EXPECT_EQ(formatTimestamp(timestamp, "%Y-%m-%d"), "2023-05-15");
    EXPECT_EQ(formatTimestamp(timestamp, "%H:%M:%S"), "14:30:45");
    EXPECT_EQ(formatTimestamp(timestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
    EXPECT_EQ(formatTimestamp(timestamp, "%a, %d %b %Y"), "Mon, 15 May 2023");
}

TEST_F(TimeUtilsTest, ParseTimestamp) {
    // Test parsing with different format strings
    auto timestamp1 = parseTimestamp("2023-05-15", "%Y-%m-%d");
    auto timestamp2 = parseTimestamp("14:30:45", "%H:%M:%S");
    auto timestamp3 = parseTimestamp("2023-05-15 14:30:45", "%Y-%m-%d %H:%M:%S");
    
    // Create expected timestamps for comparison
    // Note: When only date is provided, time is set to 00:00:00
    auto expected1 = createTimestamp(2023, 5, 15, 0, 0, 0);
    // Note: When only time is provided, date is set to the epoch start date plus local timezone offset
    // This is hard to test precisely, so we'll skip exact comparison for timestamp2
    auto expected3 = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Compare timestamps
    EXPECT_EQ(formatTimestamp(timestamp1, "%Y-%m-%d %H:%M:%S"), formatTimestamp(expected1, "%Y-%m-%d %H:%M:%S"));
    EXPECT_EQ(formatTimestamp(timestamp3, "%Y-%m-%d %H:%M:%S"), formatTimestamp(expected3, "%Y-%m-%d %H:%M:%S"));
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
    EXPECT_EQ(timestampDiffDays(t1, t3), 4);  // 4 days and 18 hours
}

TEST_F(TimeUtilsTest, AddSeconds) {
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
    auto result4 = addSeconds(base, 3600);  // 1 hour
    EXPECT_EQ(formatTimestamp(result4, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:30:45");
}

TEST_F(TimeUtilsTest, AddMinutes) {
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
    auto result4 = addMinutes(base, 60);  // 1 hour
    EXPECT_EQ(formatTimestamp(result4, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:30:45");
}

TEST_F(TimeUtilsTest, AddHours) {
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
    
    // Add hours that cross day boundary
    auto result4 = addHours(base, 12);
    EXPECT_EQ(formatTimestamp(result4, "%Y-%m-%d %H:%M:%S"), "2023-05-16 02:30:45");
}

TEST_F(TimeUtilsTest, AddDays) {
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
    
    // Add days that cross month boundary
    auto result4 = addDays(base, 20);
    EXPECT_EQ(formatTimestamp(result4, "%Y-%m-%d %H:%M:%S"), "2023-06-04 14:30:45");
    
    // Add days that cross year boundary
    auto dec31 = createTimestamp(2023, 12, 31, 14, 30, 45);
    auto result5 = addDays(dec31, 1);
    EXPECT_EQ(formatTimestamp(result5, "%Y-%m-%d %H:%M:%S"), "2024-01-01 14:30:45");
}

} // namespace testing
} // namespace equipment_tracker
// </test_code>