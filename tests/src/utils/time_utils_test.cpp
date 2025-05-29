// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include "equipment_tracker/utils/types.h"
#include "equipment_tracker/utils/time_utils.h"

using namespace equipment_tracker;
using namespace testing;

class TimeUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set a fixed timestamp for testing
        fixedTime = std::chrono::system_clock::from_time_t(1609459200); // 2021-01-01 00:00:00 UTC
    }

    Timestamp fixedTime;
};

TEST_F(TimeUtilsTest, GetCurrentTimestamp) {
    // Get current timestamp
    Timestamp now = getCurrentTimestamp();
    
    // Get current time using system_clock
    Timestamp systemNow = std::chrono::system_clock::now();
    
    // The difference should be very small (less than 1 second)
    int64_t diffMs = std::chrono::duration_cast<std::chrono::milliseconds>(systemNow - now).count();
    EXPECT_LT(std::abs(diffMs), 1000);
}

TEST_F(TimeUtilsTest, FormatTimestamp) {
    // Test with different formats
    EXPECT_THAT(formatTimestamp(fixedTime, "%Y-%m-%d"), HasSubstr("2021-01-01"));
    
    // Test with time format
    std::string formattedTime = formatTimestamp(fixedTime, "%H:%M:%S");
    // We can't test exact time due to timezone differences, but we can check format
    EXPECT_THAT(formattedTime, MatchesRegex("[0-9]{2}:[0-9]{2}:[0-9]{2}"));
    
    // Test with full datetime format
    std::string fullFormat = formatTimestamp(fixedTime, "%Y-%m-%d %H:%M:%S");
    EXPECT_THAT(fullFormat, MatchesRegex("[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}"));
}

TEST_F(TimeUtilsTest, ParseTimestamp) {
    // Create a timestamp string
    std::string timestampStr = "2021-02-15 14:30:45";
    std::string format = "%Y-%m-%d %H:%M:%S";
    
    // Parse the timestamp
    Timestamp parsedTime = parseTimestamp(timestampStr, format);
    
    // Format it back to string to verify
    std::string formattedTime = formatTimestamp(parsedTime, format);
    
    // They should match
    EXPECT_EQ(formattedTime, timestampStr);
}

TEST_F(TimeUtilsTest, TimestampDiffSeconds) {
    // Create two timestamps 10 seconds apart
    Timestamp t1 = fixedTime;
    Timestamp t2 = t1 + std::chrono::seconds(10);
    
    // Test positive difference
    EXPECT_EQ(timestampDiffSeconds(t2, t1), 10);
    
    // Test negative difference
    EXPECT_EQ(timestampDiffSeconds(t1, t2), -10);
    
    // Test zero difference
    EXPECT_EQ(timestampDiffSeconds(t1, t1), 0);
}

TEST_F(TimeUtilsTest, TimestampDiffMinutes) {
    // Create two timestamps 10 minutes apart
    Timestamp t1 = fixedTime;
    Timestamp t2 = t1 + std::chrono::minutes(10);
    
    // Test positive difference
    EXPECT_EQ(timestampDiffMinutes(t2, t1), 10);
    
    // Test negative difference
    EXPECT_EQ(timestampDiffMinutes(t1, t2), -10);
    
    // Test zero difference
    EXPECT_EQ(timestampDiffMinutes(t1, t1), 0);
}

TEST_F(TimeUtilsTest, TimestampDiffHours) {
    // Create two timestamps 5 hours apart
    Timestamp t1 = fixedTime;
    Timestamp t2 = t1 + std::chrono::hours(5);
    
    // Test positive difference
    EXPECT_EQ(timestampDiffHours(t2, t1), 5);
    
    // Test negative difference
    EXPECT_EQ(timestampDiffHours(t1, t2), -5);
    
    // Test zero difference
    EXPECT_EQ(timestampDiffHours(t1, t1), 0);
}

TEST_F(TimeUtilsTest, TimestampDiffDays) {
    // Create two timestamps 3 days apart
    Timestamp t1 = fixedTime;
    Timestamp t2 = t1 + std::chrono::hours(3 * 24);
    
    // Test positive difference
    EXPECT_EQ(timestampDiffDays(t2, t1), 3);
    
    // Test negative difference
    EXPECT_EQ(timestampDiffDays(t1, t2), -3);
    
    // Test zero difference
    EXPECT_EQ(timestampDiffDays(t1, t1), 0);
}

TEST_F(TimeUtilsTest, AddSeconds) {
    // Add 30 seconds
    Timestamp result = addSeconds(fixedTime, 30);
    EXPECT_EQ(timestampDiffSeconds(result, fixedTime), 30);
    
    // Add negative seconds (subtract)
    result = addSeconds(fixedTime, -15);
    EXPECT_EQ(timestampDiffSeconds(result, fixedTime), -15);
    
    // Add zero seconds
    result = addSeconds(fixedTime, 0);
    EXPECT_EQ(timestampDiffSeconds(result, fixedTime), 0);
}

TEST_F(TimeUtilsTest, AddMinutes) {
    // Add 45 minutes
    Timestamp result = addMinutes(fixedTime, 45);
    EXPECT_EQ(timestampDiffMinutes(result, fixedTime), 45);
    
    // Add negative minutes (subtract)
    result = addMinutes(fixedTime, -20);
    EXPECT_EQ(timestampDiffMinutes(result, fixedTime), -20);
    
    // Add zero minutes
    result = addMinutes(fixedTime, 0);
    EXPECT_EQ(timestampDiffMinutes(result, fixedTime), 0);
}

TEST_F(TimeUtilsTest, AddHours) {
    // Add 12 hours
    Timestamp result = addHours(fixedTime, 12);
    EXPECT_EQ(timestampDiffHours(result, fixedTime), 12);
    
    // Add negative hours (subtract)
    result = addHours(fixedTime, -6);
    EXPECT_EQ(timestampDiffHours(result, fixedTime), -6);
    
    // Add zero hours
    result = addHours(fixedTime, 0);
    EXPECT_EQ(timestampDiffHours(result, fixedTime), 0);
}

TEST_F(TimeUtilsTest, AddDays) {
    // Add 7 days
    Timestamp result = addDays(fixedTime, 7);
    EXPECT_EQ(timestampDiffDays(result, fixedTime), 7);
    
    // Add negative days (subtract)
    result = addDays(fixedTime, -3);
    EXPECT_EQ(timestampDiffDays(result, fixedTime), -3);
    
    // Add zero days
    result = addDays(fixedTime, 0);
    EXPECT_EQ(timestampDiffDays(result, fixedTime), 0);
}

TEST_F(TimeUtilsTest, EdgeCases) {
    // Test with large values
    Timestamp farFuture = addDays(fixedTime, 10000);
    EXPECT_EQ(timestampDiffDays(farFuture, fixedTime), 10000);
    
    // Test with large negative values
    Timestamp farPast = addDays(fixedTime, -10000);
    EXPECT_EQ(timestampDiffDays(farPast, fixedTime), -10000);
    
    // Test with maximum int64_t value that doesn't cause overflow
    int64_t maxSafeSeconds = std::numeric_limits<int64_t>::max() / 1000;
    Timestamp maxTime = addSeconds(fixedTime, maxSafeSeconds);
    EXPECT_GT(timestampDiffSeconds(maxTime, fixedTime), 0);
}

TEST_F(TimeUtilsTest, ParseInvalidTimestamp) {
    // Test with invalid format
    std::string invalidStr = "not-a-date";
    Timestamp invalidTime = parseTimestamp(invalidStr, "%Y-%m-%d");
    
    // The result should be epoch time or close to it
    time_t epochTime = std::chrono::system_clock::to_time_t(invalidTime);
    EXPECT_NEAR(epochTime, 0, 86400); // Within a day of epoch
}
// </test_code>