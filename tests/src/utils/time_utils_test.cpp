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
    
    std::time_t time_c = std::mktime(&timeinfo);
    Timestamp timestamp = std::chrono::system_clock::from_time_t(time_c);
    
    // Test different format strings
    EXPECT_EQ(formatTimestamp(timestamp, "%Y-%m-%d"), "2023-05-15");
    EXPECT_EQ(formatTimestamp(timestamp, "%H:%M:%S"), "10:30:45");
    EXPECT_EQ(formatTimestamp(timestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:45");
    EXPECT_EQ(formatTimestamp(timestamp, "%a, %d %b %Y"), "Mon, 15 May 2023");
}

TEST_F(TimestampUtilsTest, ParseTimestamp) {
    // Test parsing with different format strings
    Timestamp timestamp1 = parseTimestamp("2023-05-15", "%Y-%m-%d");
    EXPECT_EQ(formatTimestamp(timestamp1, "%Y-%m-%d"), "2023-05-15");
    
    Timestamp timestamp2 = parseTimestamp("10:30:45", "%H:%M:%S");
    EXPECT_EQ(formatTimestamp(timestamp2, "%H:%M:%S"), "10:30:45");
    
    Timestamp timestamp3 = parseTimestamp("2023-05-15 10:30:45", "%Y-%m-%d %H:%M:%S");
    EXPECT_EQ(formatTimestamp(timestamp3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:45");
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
    Timestamp t2 = parseTimestamp("2023-05-10 00:00:00", "%Y-%m-%d %H:%M:%S");
    
    EXPECT_EQ(timestampDiffDays(t1, t2), 5);
    EXPECT_EQ(timestampDiffDays(t2, t1), -5);
    
    // Test with same timestamp
    EXPECT_EQ(timestampDiffDays(t1, t1), 0);
}

TEST_F(TimestampUtilsTest, AddSeconds) {
    Timestamp t = parseTimestamp("2023-05-15 10:30:15", "%Y-%m-%d %H:%M:%S");
    
    Timestamp t1 = addSeconds(t, 30);
    EXPECT_EQ(formatTimestamp(t1, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:45");
    
    Timestamp t2 = addSeconds(t, -15);
    EXPECT_EQ(formatTimestamp(t2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:00");
    
    // Test with zero seconds
    Timestamp t3 = addSeconds(t, 0);
    EXPECT_EQ(formatTimestamp(t3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:15");
}

TEST_F(TimestampUtilsTest, AddMinutes) {
    Timestamp t = parseTimestamp("2023-05-15 10:30:15", "%Y-%m-%d %H:%M:%S");
    
    Timestamp t1 = addMinutes(t, 30);
    EXPECT_EQ(formatTimestamp(t1, "%Y-%m-%d %H:%M:%S"), "2023-05-15 11:00:15");
    
    Timestamp t2 = addMinutes(t, -15);
    EXPECT_EQ(formatTimestamp(t2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:15:15");
    
    // Test with zero minutes
    Timestamp t3 = addMinutes(t, 0);
    EXPECT_EQ(formatTimestamp(t3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:15");
}

TEST_F(TimestampUtilsTest, AddHours) {
    Timestamp t = parseTimestamp("2023-05-15 10:30:15", "%Y-%m-%d %H:%M:%S");
    
    Timestamp t1 = addHours(t, 3);
    EXPECT_EQ(formatTimestamp(t1, "%Y-%m-%d %H:%M:%S"), "2023-05-15 13:30:15");
    
    Timestamp t2 = addHours(t, -5);
    EXPECT_EQ(formatTimestamp(t2, "%Y-%m-%d %H:%M:%S"), "2023-05-15 05:30:15");
    
    // Test with zero hours
    Timestamp t3 = addHours(t, 0);
    EXPECT_EQ(formatTimestamp(t3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:15");
    
    // Test day change
    Timestamp t4 = addHours(t, 24);
    EXPECT_EQ(formatTimestamp(t4, "%Y-%m-%d %H:%M:%S"), "2023-05-16 10:30:15");
}

TEST_F(TimestampUtilsTest, AddDays) {
    Timestamp t = parseTimestamp("2023-05-15 10:30:15", "%Y-%m-%d %H:%M:%S");
    
    Timestamp t1 = addDays(t, 5);
    EXPECT_EQ(formatTimestamp(t1, "%Y-%m-%d %H:%M:%S"), "2023-05-20 10:30:15");
    
    Timestamp t2 = addDays(t, -10);
    EXPECT_EQ(formatTimestamp(t2, "%Y-%m-%d %H:%M:%S"), "2023-05-05 10:30:15");
    
    // Test with zero days
    Timestamp t3 = addDays(t, 0);
    EXPECT_EQ(formatTimestamp(t3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 10:30:15");
    
    // Test month change
    Timestamp t4 = addDays(t, 20);
    EXPECT_EQ(formatTimestamp(t4, "%Y-%m-%d %H:%M:%S"), "2023-06-04 10:30:15");
}

TEST_F(TimestampUtilsTest, EdgeCases) {
    // Test with very large time differences
    Timestamp past = parseTimestamp("1970-01-01 00:00:00", "%Y-%m-%d %H:%M:%S");
    Timestamp future = parseTimestamp("2100-01-01 00:00:00", "%Y-%m-%d %H:%M:%S");
    
    // These should be large positive numbers
    EXPECT_GT(timestampDiffSeconds(future, past), 0);
    EXPECT_GT(timestampDiffMinutes(future, past), 0);
    EXPECT_GT(timestampDiffHours(future, past), 0);
    EXPECT_GT(timestampDiffDays(future, past), 0);
    
    // These should be large negative numbers
    EXPECT_LT(timestampDiffSeconds(past, future), 0);
    EXPECT_LT(timestampDiffMinutes(past, future), 0);
    EXPECT_LT(timestampDiffHours(past, future), 0);
    EXPECT_LT(timestampDiffDays(past, future), 0);
    
    // Test with very large time additions
    Timestamp t = parseTimestamp("2023-05-15 10:30:15", "%Y-%m-%d %H:%M:%S");
    
    Timestamp t1 = addDays(t, 365 * 10); // Add 10 years
    EXPECT_EQ(formatTimestamp(t1, "%Y"), "2033");
    
    Timestamp t2 = addDays(t, -365 * 10); // Subtract 10 years
    EXPECT_EQ(formatTimestamp(t2, "%Y"), "2013");
}

} // namespace testing
} // namespace equipment_tracker

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>