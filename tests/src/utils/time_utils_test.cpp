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
namespace testing {

class TimestampUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set a fixed timezone for consistent test results
#ifdef _WIN32
        _putenv("TZ=UTC");
#else
        setenv("TZ", "UTC", 1);
#endif
        tzset();
    }

    void TearDown() override {
        // Reset timezone
#ifdef _WIN32
        _putenv("TZ=");
#else
        unsetenv("TZ");
#endif
        tzset();
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
    // Create a fixed timestamp for testing (2023-05-15 10:30:45)
    std::tm timeinfo = {};
    timeinfo.tm_year = 2023 - 1900;
    timeinfo.tm_mon = 5 - 1;
    timeinfo.tm_mday = 15;
    timeinfo.tm_hour = 10;
    timeinfo.tm_min = 30;
    timeinfo.tm_sec = 45;
    
    std::time_t time_c = std::mktime(&timeinfo);
    Timestamp timestamp = std::chrono::system_clock::from_time_t(time_c);
    
    // Test various formats
    EXPECT_EQ(formatTimestamp(timestamp, "%Y-%m-%d"), "2023-05-15");
    EXPECT_EQ(formatTimestamp(timestamp, "%H:%M:%S"), "10:30:45");
    EXPECT_EQ(formatTimestamp(timestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:45");
    EXPECT_EQ(formatTimestamp(timestamp, "%a, %d %b %Y"), "Mon, 15 May 2023");
}

TEST_F(TimestampUtilsTest, ParseTimestamp) {
    // Parse a timestamp string
    Timestamp timestamp = parseTimestamp("2023-05-15 10:30:45", "%Y-%m-%d %H:%M:%S");
    
    // Format it back to verify
    std::string formatted = formatTimestamp(timestamp, "%Y-%m-%d %H:%M:%S");
    EXPECT_EQ(formatted, "2023-05-15 10:30:45");
    
    // Test with different format
    timestamp = parseTimestamp("15/05/2023 10:30", "%d/%m/%Y %H:%M");
    formatted = formatTimestamp(timestamp, "%d/%m/%Y %H:%M");
    EXPECT_EQ(formatted, "15/05/2023 10:30");
}

TEST_F(TimestampUtilsTest, TimestampDiffSeconds) {
    Timestamp t1 = parseTimestamp("2023-05-15 10:30:45", "%Y-%m-%d %H:%M:%S");
    Timestamp t2 = parseTimestamp("2023-05-15 10:30:15", "%Y-%m-%d %H:%M:%S");
    
    EXPECT_EQ(timestampDiffSeconds(t1, t2), 30);
    EXPECT_EQ(timestampDiffSeconds(t2, t1), -30);
    
    // Same timestamp should return 0
    EXPECT_EQ(timestampDiffSeconds(t1, t1), 0);
}

TEST_F(TimestampUtilsTest, TimestampDiffMinutes) {
    Timestamp t1 = parseTimestamp("2023-05-15 10:45:00", "%Y-%m-%d %H:%M:%S");
    Timestamp t2 = parseTimestamp("2023-05-15 10:15:00", "%Y-%m-%d %H:%M:%S");
    
    EXPECT_EQ(timestampDiffMinutes(t1, t2), 30);
    EXPECT_EQ(timestampDiffMinutes(t2, t1), -30);
    
    // Test with seconds that should be truncated
    Timestamp t3 = parseTimestamp("2023-05-15 10:45:30", "%Y-%m-%d %H:%M:%S");
    Timestamp t4 = parseTimestamp("2023-05-15 10:15:45", "%Y-%m-%d %H:%M:%S");
    
    EXPECT_EQ(timestampDiffMinutes(t3, t4), 29);
}

TEST_F(TimestampUtilsTest, TimestampDiffHours) {
    Timestamp t1 = parseTimestamp("2023-05-15 15:00:00", "%Y-%m-%d %H:%M:%S");
    Timestamp t2 = parseTimestamp("2023-05-15 10:00:00", "%Y-%m-%d %H:%M:%S");
    
    EXPECT_EQ(timestampDiffHours(t1, t2), 5);
    EXPECT_EQ(timestampDiffHours(t2, t1), -5);
    
    // Test with minutes that should be truncated
    Timestamp t3 = parseTimestamp("2023-05-15 15:45:00", "%Y-%m-%d %H:%M:%S");
    Timestamp t4 = parseTimestamp("2023-05-15 10:15:00", "%Y-%m-%d %H:%M:%S");
    
    EXPECT_EQ(timestampDiffHours(t3, t4), 5);
}

TEST_F(TimestampUtilsTest, TimestampDiffDays) {
    Timestamp t1 = parseTimestamp("2023-05-20 12:00:00", "%Y-%m-%d %H:%M:%S");
    Timestamp t2 = parseTimestamp("2023-05-15 12:00:00", "%Y-%m-%d %H:%M:%S");
    
    EXPECT_EQ(timestampDiffDays(t1, t2), 5);
    EXPECT_EQ(timestampDiffDays(t2, t1), -5);
    
    // Test with hours that should be truncated
    Timestamp t3 = parseTimestamp("2023-05-20 18:00:00", "%Y-%m-%d %H:%M:%S");
    Timestamp t4 = parseTimestamp("2023-05-15 06:00:00", "%Y-%m-%d %H:%M:%S");
    
    // 5 days and 12 hours = 5.5 days, but integer division gives 5
    EXPECT_EQ(timestampDiffDays(t3, t4), 5);
}

TEST_F(TimestampUtilsTest, AddSeconds) {
    Timestamp t1 = parseTimestamp("2023-05-15 10:30:15", "%Y-%m-%d %H:%M:%S");
    
    Timestamp t2 = addSeconds(t1, 30);
    EXPECT_EQ(formatTimestamp(t2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:45");
    
    // Test negative seconds
    Timestamp t3 = addSeconds(t1, -15);
    EXPECT_EQ(formatTimestamp(t3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:00");
    
    // Test adding 0 seconds
    Timestamp t4 = addSeconds(t1, 0);
    EXPECT_EQ(formatTimestamp(t4, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:15");
}

TEST_F(TimestampUtilsTest, AddMinutes) {
    Timestamp t1 = parseTimestamp("2023-05-15 10:30:15", "%Y-%m-%d %H:%M:%S");
    
    Timestamp t2 = addMinutes(t1, 30);
    EXPECT_EQ(formatTimestamp(t2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 11:00:15");
    
    // Test negative minutes
    Timestamp t3 = addMinutes(t1, -45);
    EXPECT_EQ(formatTimestamp(t3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 09:45:15");
    
    // Test crossing day boundary
    Timestamp t4 = parseTimestamp("2023-05-15 23:45:00", "%Y-%m-%d %H:%M:%S");
    Timestamp t5 = addMinutes(t4, 30);
    EXPECT_EQ(formatTimestamp(t5, "%Y-%m-%d %H:%M:%S"), "2023-05-16 00:15:00");
}

TEST_F(TimestampUtilsTest, AddHours) {
    Timestamp t1 = parseTimestamp("2023-05-15 10:30:15", "%Y-%m-%d %H:%M:%S");
    
    Timestamp t2 = addHours(t1, 5);
    EXPECT_EQ(formatTimestamp(t2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:30:15");
    
    // Test negative hours
    Timestamp t3 = addHours(t1, -12);
    EXPECT_EQ(formatTimestamp(t3, "%Y-%m-%d %H:%M:%S"), "2023-05-14 22:30:15");
    
    // Test crossing month boundary
    Timestamp t4 = parseTimestamp("2023-05-31 20:00:00", "%Y-%m-%d %H:%M:%S");
    Timestamp t5 = addHours(t4, 6);
    EXPECT_EQ(formatTimestamp(t5, "%Y-%m-%d %H:%M:%S"), "2023-06-01 02:00:00");
}

TEST_F(TimestampUtilsTest, AddDays) {
    Timestamp t1 = parseTimestamp("2023-05-15 10:30:15", "%Y-%m-%d %H:%M:%S");
    
    Timestamp t2 = addDays(t1, 5);
    EXPECT_EQ(formatTimestamp(t2, "%Y-%m-%d %H:%M:%S"), "2023-05-20 10:30:15");
    
    // Test negative days
    Timestamp t3 = addDays(t1, -7);
    EXPECT_EQ(formatTimestamp(t3, "%Y-%m-%d %H:%M:%S"), "2023-05-08 10:30:15");
    
    // Test crossing year boundary
    Timestamp t4 = parseTimestamp("2023-12-29 10:30:15", "%Y-%m-%d %H:%M:%S");
    Timestamp t5 = addDays(t4, 5);
    EXPECT_EQ(formatTimestamp(t5, "%Y-%m-%d %H:%M:%S"), "2024-01-03 10:30:15");
}

TEST_F(TimestampUtilsTest, ParseInvalidTimestamp) {
    // Test with invalid date format
    Timestamp t1 = parseTimestamp("invalid-date", "%Y-%m-%d");
    
    // The behavior is implementation-defined, but we can at least check that it doesn't crash
    // and returns some time_point (typically epoch or some default value)
    auto time_t = std::chrono::system_clock::to_time_t(t1);
    EXPECT_NE(time_t, 0); // Just verify we got some value
}

TEST_F(TimestampUtilsTest, FormatEmptyTimestamp) {
    // Default-constructed timestamp (typically epoch)
    Timestamp t1;
    
    // Should format to some string without crashing
    std::string result = formatTimestamp(t1, "%Y-%m-%d %H:%M:%S");
    EXPECT_FALSE(result.empty());
}

} // namespace testing
} // namespace equipment_tracker
// </test_code>