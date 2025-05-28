// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include "equipment_tracker/utils/types.h"

namespace equipment_tracker
{
    // Forward declarations for functions being tested
    Timestamp getCurrentTimestamp();
    std::string formatTimestamp(const Timestamp &timestamp, const std::string &format);
    Timestamp parseTimestamp(const std::string &str, const std::string &format);
    int64_t timestampDiffSeconds(const Timestamp &t1, const Timestamp &t2);
    int64_t timestampDiffMinutes(const Timestamp &t1, const Timestamp &t2);
    int64_t timestampDiffHours(const Timestamp &t1, const Timestamp &t2);
    int64_t timestampDiffDays(const Timestamp &t1, const Timestamp &t2);
    Timestamp addSeconds(const Timestamp &timestamp, int64_t seconds);
    Timestamp addMinutes(const Timestamp &timestamp, int64_t minutes);
    Timestamp addHours(const Timestamp &timestamp, int64_t hours);
    Timestamp addDays(const Timestamp &timestamp, int64_t days);
}

class TimestampUtilsTest : public ::testing::Test {
protected:
    // Reference timestamp for testing (2023-05-15 10:30:45)
    equipment_tracker::Timestamp reference_time;
    
    void SetUp() override {
        std::tm tm = {};
        tm.tm_year = 2023 - 1900;  // Years since 1900
        tm.tm_mon = 5 - 1;         // Months since January (0-11)
        tm.tm_mday = 15;           // Day of the month (1-31)
        tm.tm_hour = 10;           // Hours (0-23)
        tm.tm_min = 30;            // Minutes (0-59)
        tm.tm_sec = 45;            // Seconds (0-59)
        
        std::time_t time_t = std::mktime(&tm);
        reference_time = std::chrono::system_clock::from_time_t(time_t);
    }
};

TEST_F(TimestampUtilsTest, GetCurrentTimestamp) {
    auto before = std::chrono::system_clock::now();
    auto current = equipment_tracker::getCurrentTimestamp();
    auto after = std::chrono::system_clock::now();
    
    // Current timestamp should be between before and after
    EXPECT_LE(before, current);
    EXPECT_LE(current, after);
}

TEST_F(TimestampUtilsTest, FormatTimestamp) {
    // Test with various formats
    EXPECT_EQ("2023-05-15", equipment_tracker::formatTimestamp(reference_time, "%Y-%m-%d"));
    EXPECT_EQ("10:30:45", equipment_tracker::formatTimestamp(reference_time, "%H:%M:%S"));
    EXPECT_EQ("May 15, 2023", equipment_tracker::formatTimestamp(reference_time, "%B %d, %Y"));
    
    // Test with empty format
    EXPECT_EQ("", equipment_tracker::formatTimestamp(reference_time, ""));
}

TEST_F(TimestampUtilsTest, ParseTimestamp) {
    // Test parsing with various formats
    auto parsed1 = equipment_tracker::parseTimestamp("2023-05-15", "%Y-%m-%d");
    auto parsed2 = equipment_tracker::parseTimestamp("10:30:45", "%H:%M:%S");
    auto parsed3 = equipment_tracker::parseTimestamp("2023-05-15 10:30:45", "%Y-%m-%d %H:%M:%S");
    
    // Compare formatted strings to verify parsing
    EXPECT_EQ("2023-05-15", equipment_tracker::formatTimestamp(parsed1, "%Y-%m-%d"));
    EXPECT_EQ("10:30:45", equipment_tracker::formatTimestamp(parsed2, "%H:%M:%S"));
    EXPECT_EQ("2023-05-15 10:30:45", equipment_tracker::formatTimestamp(parsed3, "%Y-%m-%d %H:%M:%S"));
    
    // Test invalid format (should not crash)
    auto invalid = equipment_tracker::parseTimestamp("invalid date", "%Y-%m-%d");
    // The result is implementation-defined, so we don't assert specific values
}

TEST_F(TimestampUtilsTest, TimestampDiffSeconds) {
    auto t1 = reference_time;
    auto t2 = equipment_tracker::addSeconds(t1, 30);
    auto t3 = equipment_tracker::addSeconds(t1, -45);
    
    EXPECT_EQ(-30, equipment_tracker::timestampDiffSeconds(t1, t2));
    EXPECT_EQ(30, equipment_tracker::timestampDiffSeconds(t2, t1));
    EXPECT_EQ(45, equipment_tracker::timestampDiffSeconds(t1, t3));
    EXPECT_EQ(0, equipment_tracker::timestampDiffSeconds(t1, t1));
}

TEST_F(TimestampUtilsTest, TimestampDiffMinutes) {
    auto t1 = reference_time;
    auto t2 = equipment_tracker::addMinutes(t1, 15);
    auto t3 = equipment_tracker::addMinutes(t1, -30);
    
    EXPECT_EQ(-15, equipment_tracker::timestampDiffMinutes(t1, t2));
    EXPECT_EQ(15, equipment_tracker::timestampDiffMinutes(t2, t1));
    EXPECT_EQ(30, equipment_tracker::timestampDiffMinutes(t1, t3));
    EXPECT_EQ(0, equipment_tracker::timestampDiffMinutes(t1, t1));
}

TEST_F(TimestampUtilsTest, TimestampDiffHours) {
    auto t1 = reference_time;
    auto t2 = equipment_tracker::addHours(t1, 5);
    auto t3 = equipment_tracker::addHours(t1, -12);
    
    EXPECT_EQ(-5, equipment_tracker::timestampDiffHours(t1, t2));
    EXPECT_EQ(5, equipment_tracker::timestampDiffHours(t2, t1));
    EXPECT_EQ(12, equipment_tracker::timestampDiffHours(t1, t3));
    EXPECT_EQ(0, equipment_tracker::timestampDiffHours(t1, t1));
}

TEST_F(TimestampUtilsTest, TimestampDiffDays) {
    auto t1 = reference_time;
    auto t2 = equipment_tracker::addDays(t1, 3);
    auto t3 = equipment_tracker::addDays(t1, -7);
    
    EXPECT_EQ(-3, equipment_tracker::timestampDiffDays(t1, t2));
    EXPECT_EQ(3, equipment_tracker::timestampDiffDays(t2, t1));
    EXPECT_EQ(7, equipment_tracker::timestampDiffDays(t1, t3));
    EXPECT_EQ(0, equipment_tracker::timestampDiffDays(t1, t1));
}

TEST_F(TimestampUtilsTest, AddSeconds) {
    auto t1 = reference_time;
    auto t2 = equipment_tracker::addSeconds(t1, 30);
    auto t3 = equipment_tracker::addSeconds(t1, -45);
    
    EXPECT_EQ(30, equipment_tracker::timestampDiffSeconds(t2, t1));
    EXPECT_EQ(-45, equipment_tracker::timestampDiffSeconds(t3, t1));
    
    // Test adding zero seconds
    auto t4 = equipment_tracker::addSeconds(t1, 0);
    EXPECT_EQ(0, equipment_tracker::timestampDiffSeconds(t4, t1));
    
    // Test large values
    auto t5 = equipment_tracker::addSeconds(t1, 1000000);
    EXPECT_EQ(1000000, equipment_tracker::timestampDiffSeconds(t5, t1));
}

TEST_F(TimestampUtilsTest, AddMinutes) {
    auto t1 = reference_time;
    auto t2 = equipment_tracker::addMinutes(t1, 15);
    auto t3 = equipment_tracker::addMinutes(t1, -30);
    
    EXPECT_EQ(15, equipment_tracker::timestampDiffMinutes(t2, t1));
    EXPECT_EQ(-30, equipment_tracker::timestampDiffMinutes(t3, t1));
    
    // Test adding zero minutes
    auto t4 = equipment_tracker::addMinutes(t1, 0);
    EXPECT_EQ(0, equipment_tracker::timestampDiffMinutes(t4, t1));
    
    // Test large values
    auto t5 = equipment_tracker::addMinutes(t1, 10000);
    EXPECT_EQ(10000, equipment_tracker::timestampDiffMinutes(t5, t1));
}

TEST_F(TimestampUtilsTest, AddHours) {
    auto t1 = reference_time;
    auto t2 = equipment_tracker::addHours(t1, 5);
    auto t3 = equipment_tracker::addHours(t1, -12);
    
    EXPECT_EQ(5, equipment_tracker::timestampDiffHours(t2, t1));
    EXPECT_EQ(-12, equipment_tracker::timestampDiffHours(t3, t1));
    
    // Test adding zero hours
    auto t4 = equipment_tracker::addHours(t1, 0);
    EXPECT_EQ(0, equipment_tracker::timestampDiffHours(t4, t1));
    
    // Test large values
    auto t5 = equipment_tracker::addHours(t1, 1000);
    EXPECT_EQ(1000, equipment_tracker::timestampDiffHours(t5, t1));
}

TEST_F(TimestampUtilsTest, AddDays) {
    auto t1 = reference_time;
    auto t2 = equipment_tracker::addDays(t1, 3);
    auto t3 = equipment_tracker::addDays(t1, -7);
    
    EXPECT_EQ(3, equipment_tracker::timestampDiffDays(t2, t1));
    EXPECT_EQ(-7, equipment_tracker::timestampDiffDays(t3, t1));
    
    // Test adding zero days
    auto t4 = equipment_tracker::addDays(t1, 0);
    EXPECT_EQ(0, equipment_tracker::timestampDiffDays(t4, t1));
    
    // Test large values
    auto t5 = equipment_tracker::addDays(t1, 365);
    EXPECT_EQ(365, equipment_tracker::timestampDiffDays(t5, t1));
}

TEST_F(TimestampUtilsTest, RoundTripFormatParse) {
    auto original = reference_time;
    std::string formatted = equipment_tracker::formatTimestamp(original, "%Y-%m-%d %H:%M:%S");
    auto parsed = equipment_tracker::parseTimestamp(formatted, "%Y-%m-%d %H:%M:%S");
    
    // Due to potential timezone issues and precision loss, we check if they're close
    auto diff = std::abs(equipment_tracker::timestampDiffSeconds(original, parsed));
    EXPECT_LE(diff, 1); // Allow 1 second difference due to rounding
}
// </test_code>