// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/utils/time_utils.h"
#include "equipment_tracker/utils/types.h"
#include <chrono>
#include <ctime>
#include <thread>

namespace {
    // Helper function to create a timestamp from a specific date/time
    equipment_tracker::Timestamp makeTimestamp(int year, int month, int day, 
                                              int hour, int minute, int second) {
        std::tm tm = {};
        tm.tm_year = year - 1900;  // Years since 1900
        tm.tm_mon = month - 1;     // Months are 0-based
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        
        std::time_t time = std::mktime(&tm);
        return std::chrono::system_clock::from_time_t(time);
    }
}

class TimeUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create two timestamps 1 day, 2 hours, 3 minutes, and 4 seconds apart
        timestamp1 = makeTimestamp(2023, 1, 1, 12, 0, 0);
        timestamp2 = makeTimestamp(2023, 1, 2, 14, 3, 4);
    }

    equipment_tracker::Timestamp timestamp1;
    equipment_tracker::Timestamp timestamp2;
};

TEST_F(TimeUtilsTest, GetCurrentTimestamp) {
    auto timestamp = equipment_tracker::getCurrentTimestamp();
    auto now = std::chrono::system_clock::now();
    
    // The timestamps should be very close (within 1 second)
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - timestamp).count();
    EXPECT_LE(std::abs(diff), 1);
}

TEST_F(TimeUtilsTest, FormatTimestamp) {
    auto timestamp = makeTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Test different format strings
    EXPECT_EQ(equipment_tracker::formatTimestamp(timestamp, "%Y-%m-%d"), "2023-05-15");
    EXPECT_EQ(equipment_tracker::formatTimestamp(timestamp, "%H:%M:%S"), "14:30:45");
    EXPECT_EQ(equipment_tracker::formatTimestamp(timestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
}

TEST_F(TimeUtilsTest, ParseTimestamp) {
    std::string dateStr = "2023-05-15 14:30:45";
    std::string format = "%Y-%m-%d %H:%M:%S";
    
    auto timestamp = equipment_tracker::parseTimestamp(dateStr, format);
    
    // Verify by formatting it back
    EXPECT_EQ(equipment_tracker::formatTimestamp(timestamp, format), dateStr);
}

TEST_F(TimeUtilsTest, TimestampDiffSeconds) {
    // timestamp2 is later than timestamp1
    EXPECT_EQ(equipment_tracker::timestampDiffSeconds(timestamp2, timestamp1), 
              24*60*60 + 2*60*60 + 3*60 + 4);
    
    // Negative difference when order is reversed
    EXPECT_EQ(equipment_tracker::timestampDiffSeconds(timestamp1, timestamp2), 
              -(24*60*60 + 2*60*60 + 3*60 + 4));
    
    // Zero difference for same timestamp
    EXPECT_EQ(equipment_tracker::timestampDiffSeconds(timestamp1, timestamp1), 0);
}

TEST_F(TimeUtilsTest, TimestampDiffMinutes) {
    // timestamp2 is later than timestamp1
    EXPECT_EQ(equipment_tracker::timestampDiffMinutes(timestamp2, timestamp1), 
              24*60 + 2*60 + 3);
    
    // Negative difference when order is reversed
    EXPECT_EQ(equipment_tracker::timestampDiffMinutes(timestamp1, timestamp2), 
              -(24*60 + 2*60 + 3));
    
    // Zero difference for same timestamp
    EXPECT_EQ(equipment_tracker::timestampDiffMinutes(timestamp1, timestamp1), 0);
}

TEST_F(TimeUtilsTest, TimestampDiffHours) {
    // timestamp2 is later than timestamp1
    EXPECT_EQ(equipment_tracker::timestampDiffHours(timestamp2, timestamp1), 24 + 2);
    
    // Negative difference when order is reversed
    EXPECT_EQ(equipment_tracker::timestampDiffHours(timestamp1, timestamp2), -(24 + 2));
    
    // Zero difference for same timestamp
    EXPECT_EQ(equipment_tracker::timestampDiffHours(timestamp1, timestamp1), 0);
}

TEST_F(TimeUtilsTest, TimestampDiffDays) {
    // timestamp2 is later than timestamp1
    EXPECT_EQ(equipment_tracker::timestampDiffDays(timestamp2, timestamp1), 1);
    
    // Negative difference when order is reversed
    EXPECT_EQ(equipment_tracker::timestampDiffDays(timestamp1, timestamp2), -1);
    
    // Zero difference for same timestamp
    EXPECT_EQ(equipment_tracker::timestampDiffDays(timestamp1, timestamp1), 0);
    
    // Test with exactly 48 hours difference
    auto timestamp3 = makeTimestamp(2023, 1, 3, 12, 0, 0);
    EXPECT_EQ(equipment_tracker::timestampDiffDays(timestamp3, timestamp1), 2);
}

TEST_F(TimeUtilsTest, AddSeconds) {
    auto result = equipment_tracker::addSeconds(timestamp1, 10);
    EXPECT_EQ(equipment_tracker::timestampDiffSeconds(result, timestamp1), 10);
    
    // Test with negative value
    result = equipment_tracker::addSeconds(timestamp1, -10);
    EXPECT_EQ(equipment_tracker::timestampDiffSeconds(result, timestamp1), -10);
}

TEST_F(TimeUtilsTest, AddMinutes) {
    auto result = equipment_tracker::addMinutes(timestamp1, 10);
    EXPECT_EQ(equipment_tracker::timestampDiffMinutes(result, timestamp1), 10);
    
    // Test with negative value
    result = equipment_tracker::addMinutes(timestamp1, -10);
    EXPECT_EQ(equipment_tracker::timestampDiffMinutes(result, timestamp1), -10);
}

TEST_F(TimeUtilsTest, AddHours) {
    auto result = equipment_tracker::addHours(timestamp1, 10);
    EXPECT_EQ(equipment_tracker::timestampDiffHours(result, timestamp1), 10);
    
    // Test with negative value
    result = equipment_tracker::addHours(timestamp1, -10);
    EXPECT_EQ(equipment_tracker::timestampDiffHours(result, timestamp1), -10);
}

TEST_F(TimeUtilsTest, AddDays) {
    auto result = equipment_tracker::addDays(timestamp1, 10);
    EXPECT_EQ(equipment_tracker::timestampDiffDays(result, timestamp1), 10);
    
    // Test with negative value
    result = equipment_tracker::addDays(timestamp1, -10);
    EXPECT_EQ(equipment_tracker::timestampDiffDays(result, timestamp1), -10);
}

TEST_F(TimeUtilsTest, EdgeCases) {
    // Test with very large time differences
    auto farFuture = equipment_tracker::addDays(timestamp1, 10000);
    auto farPast = equipment_tracker::addDays(timestamp1, -10000);
    
    EXPECT_EQ(equipment_tracker::timestampDiffDays(farFuture, timestamp1), 10000);
    EXPECT_EQ(equipment_tracker::timestampDiffDays(farPast, timestamp1), -10000);
    
    // Test with zero values
    EXPECT_EQ(equipment_tracker::addSeconds(timestamp1, 0), timestamp1);
    EXPECT_EQ(equipment_tracker::addMinutes(timestamp1, 0), timestamp1);
    EXPECT_EQ(equipment_tracker::addHours(timestamp1, 0), timestamp1);
    EXPECT_EQ(equipment_tracker::addDays(timestamp1, 0), timestamp1);
}

TEST_F(TimeUtilsTest, ParseInvalidTimestamp) {
    // Test parsing an invalid timestamp
    std::string invalidDateStr = "invalid-date";
    std::string format = "%Y-%m-%d";
    
    // The behavior is implementation-defined, but we can at least verify it doesn't crash
    auto timestamp = equipment_tracker::parseTimestamp(invalidDateStr, format);
    
    // The result should be a valid timestamp, though not necessarily what we expect
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::years>(now - timestamp).count();
    
    // The difference should be reasonable (not thousands of years)
    EXPECT_LE(std::abs(diff), 200);
}
// </test_code>