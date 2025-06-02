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
    
    // Convert to time_t for comparison to avoid precision issues
    auto expected_time_t = std::chrono::system_clock::to_time_t(expected);
    auto parsed1_time_t = std::chrono::system_clock::to_time_t(parsed1);
    auto parsed2_time_t = std::chrono::system_clock::to_time_t(parsed2);
    
    EXPECT_EQ(expected_time_t, parsed1_time_t);
    EXPECT_EQ(expected_time_t, parsed2_time_t);
}

TEST_F(TimeUtilsTest, TimestampDiffSeconds) {
    auto t1 = createTimestamp(2023, 5, 15, 14, 30, 45);
    auto t2 = createTimestamp(2023, 5, 15, 14, 30, 15);
    
    EXPECT_EQ(timestampDiffSeconds(t1, t2), 30);
    EXPECT_EQ(timestampDiffSeconds(t2, t1), -30);
    
    // Test with zero difference
    auto t3 = createTimestamp(2023, 5, 15, 14, 30, 45);
    EXPECT_EQ(timestampDiffSeconds(t1, t3), 0);
}

TEST_F(TimeUtilsTest, TimestampDiffMinutes) {
    auto t1 = createTimestamp(2023, 5, 15, 14, 45, 0);
    auto t2 = createTimestamp(2023, 5, 15, 14, 15, 0);
    
    EXPECT_EQ(timestampDiffMinutes(t1, t2), 30);
    EXPECT_EQ(timestampDiffMinutes(t2, t1), -30);
    
    // Test with partial minutes
    auto t3 = createTimestamp(2023, 5, 15, 14, 45, 30);
    auto t4 = createTimestamp(2023, 5, 15, 14, 15, 15);
    EXPECT_EQ(timestampDiffMinutes(t3, t4), 30); // Should truncate to whole minutes
}

TEST_F(TimeUtilsTest, TimestampDiffHours) {
    auto t1 = createTimestamp(2023, 5, 15, 18, 0, 0);
    auto t2 = createTimestamp(2023, 5, 15, 12, 0, 0);
    
    EXPECT_EQ(timestampDiffHours(t1, t2), 6);
    EXPECT_EQ(timestampDiffHours(t2, t1), -6);
    
    // Test with partial hours
    auto t3 = createTimestamp(2023, 5, 15, 18, 30, 0);
    auto t4 = createTimestamp(2023, 5, 15, 12, 15, 0);
    EXPECT_EQ(timestampDiffHours(t3, t4), 6); // Should truncate to whole hours
}

TEST_F(TimeUtilsTest, TimestampDiffDays) {
    auto t1 = createTimestamp(2023, 5, 20, 12, 0, 0);
    auto t2 = createTimestamp(2023, 5, 15, 12, 0, 0);
    
    EXPECT_EQ(timestampDiffDays(t1, t2), 5);
    EXPECT_EQ(timestampDiffDays(t2, t1), -5);
    
    // Test with partial days
    auto t3 = createTimestamp(2023, 5, 20, 18, 0, 0);
    auto t4 = createTimestamp(2023, 5, 15, 6, 0, 0);
    EXPECT_EQ(timestampDiffDays(t3, t4), 5); // Should be 5 days and 12 hours, but truncated to 5 days
}

TEST_F(TimeUtilsTest, AddSeconds) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Add positive seconds
    auto result1 = addSeconds(base, 30);
    EXPECT_EQ(timestampDiffSeconds(result1, base), 30);
    
    // Add negative seconds
    auto result2 = addSeconds(base, -15);
    EXPECT_EQ(timestampDiffSeconds(base, result2), 15);
    
    // Add zero seconds
    auto result3 = addSeconds(base, 0);
    EXPECT_EQ(timestampDiffSeconds(result3, base), 0);
    
    // Add large number of seconds
    auto result4 = addSeconds(base, 3600); // 1 hour
    EXPECT_EQ(timestampDiffSeconds(result4, base), 3600);
}

TEST_F(TimeUtilsTest, AddMinutes) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Add positive minutes
    auto result1 = addMinutes(base, 30);
    EXPECT_EQ(timestampDiffMinutes(result1, base), 30);
    
    // Add negative minutes
    auto result2 = addMinutes(base, -15);
    EXPECT_EQ(timestampDiffMinutes(base, result2), 15);
    
    // Add zero minutes
    auto result3 = addMinutes(base, 0);
    EXPECT_EQ(timestampDiffMinutes(result3, base), 0);
    
    // Add large number of minutes
    auto result4 = addMinutes(base, 120); // 2 hours
    EXPECT_EQ(timestampDiffMinutes(result4, base), 120);
}

TEST_F(TimeUtilsTest, AddHours) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Add positive hours
    auto result1 = addHours(base, 5);
    EXPECT_EQ(timestampDiffHours(result1, base), 5);
    
    // Add negative hours
    auto result2 = addHours(base, -3);
    EXPECT_EQ(timestampDiffHours(base, result2), 3);
    
    // Add zero hours
    auto result3 = addHours(base, 0);
    EXPECT_EQ(timestampDiffHours(result3, base), 0);
    
    // Add large number of hours
    auto result4 = addHours(base, 48); // 2 days
    EXPECT_EQ(timestampDiffHours(result4, base), 48);
}

TEST_F(TimeUtilsTest, AddDays) {
    auto base = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Add positive days
    auto result1 = addDays(base, 5);
    EXPECT_EQ(timestampDiffDays(result1, base), 5);
    
    // Add negative days
    auto result2 = addDays(base, -3);
    EXPECT_EQ(timestampDiffDays(base, result2), 3);
    
    // Add zero days
    auto result3 = addDays(base, 0);
    EXPECT_EQ(timestampDiffDays(result3, base), 0);
    
    // Add large number of days
    auto result4 = addDays(base, 30); // 1 month (approximately)
    EXPECT_EQ(timestampDiffDays(result4, base), 30);
}

TEST_F(TimeUtilsTest, RoundTripFormatParse) {
    // Test that formatting and then parsing gives the original timestamp
    auto original = createTimestamp(2023, 5, 15, 14, 30, 45);
    std::string formatted = formatTimestamp(original, "%Y-%m-%d %H:%M:%S");
    auto parsed = parseTimestamp(formatted, "%Y-%m-%d %H:%M:%S");
    
    // Convert to time_t for comparison to avoid precision issues
    auto original_time_t = std::chrono::system_clock::to_time_t(original);
    auto parsed_time_t = std::chrono::system_clock::to_time_t(parsed);
    
    EXPECT_EQ(original_time_t, parsed_time_t);
}

} // namespace testing
} // namespace equipment_tracker
// </test_code>