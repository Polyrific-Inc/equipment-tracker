// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/utils/time_utils.h"
#include "equipment_tracker/utils/types.h"
#include <chrono>
#include <ctime>
#include <thread>

namespace {

class TimeUtilsTest : public ::testing::Test {
protected:
    // Helper function to create a timestamp for a specific date/time
    equipment_tracker::Timestamp createTimestamp(int year, int month, int day, 
                                                int hour, int minute, int second) {
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
    auto timestamp1 = equipment_tracker::getCurrentTimestamp();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto timestamp2 = equipment_tracker::getCurrentTimestamp();
    
    // Verify that time is advancing
    EXPECT_LT(timestamp1, timestamp2);
}

TEST_F(TimeUtilsTest, FormatTimestamp) {
    // Create a fixed timestamp for testing (2023-05-15 14:30:45)
    auto timestamp = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    // Test different format strings
    EXPECT_EQ(equipment_tracker::formatTimestamp(timestamp, "%Y-%m-%d"), "2023-05-15");
    EXPECT_EQ(equipment_tracker::formatTimestamp(timestamp, "%H:%M:%S"), "14:30:45");
    EXPECT_EQ(equipment_tracker::formatTimestamp(timestamp, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
    EXPECT_EQ(equipment_tracker::formatTimestamp(timestamp, "%a, %b %d, %Y"), "Mon, May 15, 2023");
}

TEST_F(TimeUtilsTest, ParseTimestamp) {
    // Test parsing with different format strings
    auto timestamp1 = equipment_tracker::parseTimestamp("2023-05-15", "%Y-%m-%d");
    auto timestamp2 = equipment_tracker::parseTimestamp("14:30:45", "%H:%M:%S");
    auto timestamp3 = equipment_tracker::parseTimestamp("2023-05-15 14:30:45", "%Y-%m-%d %H:%M:%S");
    
    // Verify the parsed timestamps by formatting them back
    EXPECT_EQ(equipment_tracker::formatTimestamp(timestamp1, "%Y-%m-%d"), "2023-05-15");
    EXPECT_THAT(equipment_tracker::formatTimestamp(timestamp2, "%H:%M:%S"), ::testing::HasSubstr("14:30:45"));
    EXPECT_EQ(equipment_tracker::formatTimestamp(timestamp3, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
}

TEST_F(TimeUtilsTest, TimestampDiffSeconds) {
    auto t1 = createTimestamp(2023, 5, 15, 14, 30, 45);
    auto t2 = createTimestamp(2023, 5, 15, 14, 30, 15);
    
    EXPECT_EQ(equipment_tracker::timestampDiffSeconds(t1, t2), 30);
    EXPECT_EQ(equipment_tracker::timestampDiffSeconds(t2, t1), -30);
    
    // Test with same timestamp
    EXPECT_EQ(equipment_tracker::timestampDiffSeconds(t1, t1), 0);
}

TEST_F(TimeUtilsTest, TimestampDiffMinutes) {
    auto t1 = createTimestamp(2023, 5, 15, 14, 45, 0);
    auto t2 = createTimestamp(2023, 5, 15, 14, 15, 0);
    
    EXPECT_EQ(equipment_tracker::timestampDiffMinutes(t1, t2), 30);
    EXPECT_EQ(equipment_tracker::timestampDiffMinutes(t2, t1), -30);
    
    // Test with same timestamp
    EXPECT_EQ(equipment_tracker::timestampDiffMinutes(t1, t1), 0);
    
    // Test with partial minutes
    auto t3 = createTimestamp(2023, 5, 15, 14, 45, 30);
    EXPECT_EQ(equipment_tracker::timestampDiffMinutes(t3, t2), 30);
}

TEST_F(TimeUtilsTest, TimestampDiffHours) {
    auto t1 = createTimestamp(2023, 5, 15, 18, 0, 0);
    auto t2 = createTimestamp(2023, 5, 15, 12, 0, 0);
    
    EXPECT_EQ(equipment_tracker::timestampDiffHours(t1, t2), 6);
    EXPECT_EQ(equipment_tracker::timestampDiffHours(t2, t1), -6);
    
    // Test with same timestamp
    EXPECT_EQ(equipment_tracker::timestampDiffHours(t1, t1), 0);
    
    // Test with partial hours
    auto t3 = createTimestamp(2023, 5, 15, 18, 30, 0);
    EXPECT_EQ(equipment_tracker::timestampDiffHours(t3, t2), 6);
}

TEST_F(TimeUtilsTest, TimestampDiffDays) {
    auto t1 = createTimestamp(2023, 5, 20, 12, 0, 0);
    auto t2 = createTimestamp(2023, 5, 15, 12, 0, 0);
    
    EXPECT_EQ(equipment_tracker::timestampDiffDays(t1, t2), 5);
    EXPECT_EQ(equipment_tracker::timestampDiffDays(t2, t1), -5);
    
    // Test with same timestamp
    EXPECT_EQ(equipment_tracker::timestampDiffDays(t1, t1), 0);
    
    // Test with partial days
    auto t3 = createTimestamp(2023, 5, 20, 18, 0, 0);
    auto t4 = createTimestamp(2023, 5, 15, 6, 0, 0);
    EXPECT_EQ(equipment_tracker::timestampDiffDays(t3, t4), 5);
}

TEST_F(TimeUtilsTest, AddSeconds) {
    auto timestamp = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    auto result = equipment_tracker::addSeconds(timestamp, 30);
    EXPECT_EQ(equipment_tracker::formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:31:15");
    
    // Test with negative value
    result = equipment_tracker::addSeconds(timestamp, -30);
    EXPECT_EQ(equipment_tracker::formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:15");
    
    // Test with zero
    result = equipment_tracker::addSeconds(timestamp, 0);
    EXPECT_EQ(equipment_tracker::formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
    
    // Test with large value
    result = equipment_tracker::addSeconds(timestamp, 3600);
    EXPECT_EQ(equipment_tracker::formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:30:45");
}

TEST_F(TimeUtilsTest, AddMinutes) {
    auto timestamp = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    auto result = equipment_tracker::addMinutes(timestamp, 30);
    EXPECT_EQ(equipment_tracker::formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:00:45");
    
    // Test with negative value
    result = equipment_tracker::addMinutes(timestamp, -30);
    EXPECT_EQ(equipment_tracker::formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:00:45");
    
    // Test with zero
    result = equipment_tracker::addMinutes(timestamp, 0);
    EXPECT_EQ(equipment_tracker::formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
    
    // Test with large value
    result = equipment_tracker::addMinutes(timestamp, 60);
    EXPECT_EQ(equipment_tracker::formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 15:30:45");
}

TEST_F(TimeUtilsTest, AddHours) {
    auto timestamp = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    auto result = equipment_tracker::addHours(timestamp, 5);
    EXPECT_EQ(equipment_tracker::formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 19:30:45");
    
    // Test with negative value
    result = equipment_tracker::addHours(timestamp, -5);
    EXPECT_EQ(equipment_tracker::formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 09:30:45");
    
    // Test with zero
    result = equipment_tracker::addHours(timestamp, 0);
    EXPECT_EQ(equipment_tracker::formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
    
    // Test with large value crossing day boundary
    result = equipment_tracker::addHours(timestamp, 12);
    EXPECT_EQ(equipment_tracker::formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-16 02:30:45");
}

TEST_F(TimeUtilsTest, AddDays) {
    auto timestamp = createTimestamp(2023, 5, 15, 14, 30, 45);
    
    auto result = equipment_tracker::addDays(timestamp, 5);
    EXPECT_EQ(equipment_tracker::formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-20 14:30:45");
    
    // Test with negative value
    result = equipment_tracker::addDays(timestamp, -5);
    EXPECT_EQ(equipment_tracker::formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-10 14:30:45");
    
    // Test with zero
    result = equipment_tracker::addDays(timestamp, 0);
    EXPECT_EQ(equipment_tracker::formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-05-15 14:30:45");
    
    // Test with large value crossing month boundary
    result = equipment_tracker::addDays(timestamp, 20);
    EXPECT_EQ(equipment_tracker::formatTimestamp(result, "%Y-%m-%d %H:%M:%S"), "2023-06-04 14:30:45");
}

}  // namespace
// </test_code>