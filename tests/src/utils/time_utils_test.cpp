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
        timeinfo.tm_year = year - 1900;  // Years since 1900
        timeinfo.tm_mon = month - 1;     // Months since January (0-11)
        timeinfo.tm_mday = day;          // Day of the month (1-31)
        timeinfo.tm_hour = hour;         // Hours (0-23)
        timeinfo.tm_min = minute;        // Minutes (0-59)
        timeinfo.tm_sec = second;        // Seconds (0-59)
        
        std::time_t time = std::mktime(&timeinfo);
        return std::chrono::system_clock::from_time_t(time);
    }
};

TEST_F(TimeUtilsTest, GetCurrentTimestamp) {
    // Get current timestamp
    auto timestamp = getCurrentTimestamp();
    
    // Get current time using system_clock for comparison
    auto now = std::chrono::system_clock::now();
    
    // The difference should be very small (less than 100ms)
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - timestamp).count();
    EXPECT_LT(std::abs(diff), 100);
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
    // Parse a timestamp string
    auto timestamp = parseTimestamp("2023-05-15 14:30:45", "%Y-%m-%d %H:%M:%S");
    
    // Format it back to verify
    std::string formatted = formatTimestamp(timestamp, "%Y-%m-%d %H:%M:%S");
    EXPECT_EQ(formatted, "2023-05-15 14:30:45");
    
    // Test with different format
    timestamp = parseTimestamp("15/05/2023 14:30", "%d/%m/%Y %H:%M");
    formatted = formatTimestamp(timestamp, "%Y-%m-%d %H:%M:%S");
    EXPECT_EQ(formatted, "2023-05-15 14:30:00");
}

TEST_F(TimeUtilsTest, TimestampDiffSeconds) {
    auto t1 = createTimestamp(2023, 5, 15, 14, 30, 45);
    auto t2 = createTimestamp(2023, 5, 15, 14, 30, 15);
    
    // t1 is 30 seconds later than t2
    EXPECT_EQ(timestampDiffSeconds(t1, t2), 30);
    // t2 is 30 seconds earlier than t1
    EXPECT_EQ(timestampDiffSeconds(t2, t1), -30);
    
    // Same timestamp should have zero difference
    EXPECT_EQ(timestampDiffSeconds(t1, t1), 0);
}

TEST_F(TimeUtilsTest, TimestampDiffMinutes) {
    auto t1 = createTimestamp(2023, 5, 15, 14, 45, 0);
    auto t2 = createTimestamp(2023, 5, 15, 14, 15, 0);
    
    // t1 is 30 minutes later than t2
    EXPECT_EQ(timestampDiffMinutes(t1, t2), 30);
    // t2 is 30 minutes earlier than t1
    EXPECT_EQ(timestampDiffMinutes(t2, t1), -30);
    
    // Test with seconds that don't affect the minute count
    auto t3 = createTimestamp(2023, 5, 15, 14, 45, 30);
    auto t4 = createTimestamp(2023, 5, 15, 14, 15, 45);
    EXPECT_EQ(timestampDiffMinutes(t3, t4), 29);
}

TEST_F(TimeUtilsTest, TimestampDiffHours) {
    auto t1 = createTimestamp(2023, 5, 15, 18, 0, 0);
    auto t2 = createTimestamp(2023, 5, 15, 12, 0, 0);
    
    // t1 is 6 hours later than t2
    EXPECT_EQ(timestampDiffHours(t1, t2), 6);
    // t2 is 6 hours earlier than t1
    EXPECT_EQ(timestampDiffHours(t2, t1), -6);
    
    // Test with minutes that don't affect the hour count
    auto t3 = createTimestamp(2023, 5, 15, 18, 30, 0);
    auto t4 = createTimestamp(2023, 5, 15, 12, 45, 0);
    EXPECT_EQ(timestampDiffHours(t3, t4), 5);
}

TEST_F(TimeUtilsTest, TimestampDiffDays) {
    auto t1 = createTimestamp(2023, 5, 20, 12, 0, 0);
    auto t2 = createTimestamp(2023, 5, 15, 12, 0, 0);
    
    // t1 is 5 days later than t2
    EXPECT_EQ(timestampDiffDays(t1, t2), 5);
    // t2 is 5 days earlier than t1
    EXPECT_EQ(timestampDiffDays(t2, t1), -5);
    
    // Test with hours that affect the day count
    auto t3 = createTimestamp(2023, 5, 20, 18, 0, 0);
    auto t4 = createTimestamp(2023, 5, 15, 6, 0, 0);
    EXPECT_EQ(timestampDiffDays(t3, t4), 5);
}

TEST_F(TimeUtilsTest, AddSeconds) {
    auto timestamp = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Add 30 seconds
    auto newTimestamp = addSeconds(timestamp, 30);
    EXPECT_EQ(formatTimestamp(newTimestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:31:15");
    
    // Add negative seconds (subtract)
    newTimestamp = addSeconds(timestamp, -30);
    EXPECT_EQ(formatTimestamp(newTimestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:15");
    
    // Add large number of seconds
    newTimestamp = addSeconds(timestamp, 3600);  // 1 hour
    EXPECT_EQ(formatTimestamp(newTimestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:30:45");
}

TEST_F(TimeUtilsTest, AddMinutes) {
    auto timestamp = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Add 30 minutes
    auto newTimestamp = addMinutes(timestamp, 30);
    EXPECT_EQ(formatTimestamp(newTimestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:00:45");
    
    // Add negative minutes (subtract)
    newTimestamp = addMinutes(timestamp, -30);
    EXPECT_EQ(formatTimestamp(newTimestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:00:45");
    
    // Add large number of minutes
    newTimestamp = addMinutes(timestamp, 1440);  // 24 hours
    EXPECT_EQ(formatTimestamp(newTimestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-16 14:30:45");
}

TEST_F(TimeUtilsTest, AddHours) {
    auto timestamp = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Add 5 hours
    auto newTimestamp = addHours(timestamp, 5);
    EXPECT_EQ(formatTimestamp(newTimestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-15 19:30:45");
    
    // Add negative hours (subtract)
    newTimestamp = addHours(timestamp, -5);
    EXPECT_EQ(formatTimestamp(newTimestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-15 09:30:45");
    
    // Add hours that cross day boundary
    newTimestamp = addHours(timestamp, 12);
    EXPECT_EQ(formatTimestamp(newTimestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-16 02:30:45");
}

TEST_F(TimeUtilsTest, AddDays) {
    auto timestamp = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Add 5 days
    auto newTimestamp = addDays(timestamp, 5);
    EXPECT_EQ(formatTimestamp(newTimestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-20 14:30:45");
    
    // Add negative days (subtract)
    newTimestamp = addDays(timestamp, -5);
    EXPECT_EQ(formatTimestamp(newTimestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-10 14:30:45");
    
    // Add days that cross month boundary
    newTimestamp = addDays(timestamp, 20);
    EXPECT_EQ(formatTimestamp(newTimestamp, "%Y-%m-%d %H:%M:%S"), "2023-06-04 14:30:45");
    
    // Add days that cross year boundary
    timestamp = createTimestamp(2023, 12, 25, 14, 30, 45);
    newTimestamp = addDays(timestamp, 10);
    EXPECT_EQ(formatTimestamp(newTimestamp, "%Y-%m-%d %H:%M:%S"), "2024-01-04 14:30:45");
}

} // namespace testing
} // namespace equipment_tracker
// </test_code>