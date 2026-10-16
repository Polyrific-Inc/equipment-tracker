// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/position.h"
#include <chrono>
#include <thread>
#include <cmath>

// Define M_PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace equipment_tracker {
namespace {

// Test fixture for Position tests
class PositionTest : public ::testing::Test {
protected:
    // Helper method to create a timestamp for a specific time
    static Timestamp createTimestamp(int year, int month, int day, 
                                    int hour, int minute, int second) {
        std::tm timeinfo = {};
        timeinfo.tm_year = year - 1900;  // Years since 1900
        timeinfo.tm_mon = month - 1;     // Months since January (0-11)
        timeinfo.tm_mday = day;          // Day of the month (1-31)
        timeinfo.tm_hour = hour;         // Hours (0-23)
        timeinfo.tm_min = minute;        // Minutes (0-59)
        timeinfo.tm_sec = second;        // Seconds (0-59)
        
        std::time_t time_t_value;
#ifdef _WIN32
        time_t_value = _mkgmtime(&timeinfo);
#else
        timeinfo.tm_isdst = -1;  // Let the system determine DST
        time_t_value = timegm(&timeinfo);
#endif
        
        return std::chrono::system_clock::from_time_t(time_t_value);
    }
};

// Test constructor and getters
TEST_F(PositionTest, ConstructorAndGetters) {
    // Create a specific timestamp for testing
    auto timestamp = createTimestamp(2023, 5, 15, 10, 30, 0);
    
    // Create a position with specific values
    Position position(40.7128, -74.0060, 10.5, 3.0, timestamp);
    
    // Verify getters return the expected values
    EXPECT_DOUBLE_EQ(40.7128, position.getLatitude());
    EXPECT_DOUBLE_EQ(-74.0060, position.getLongitude());
    EXPECT_DOUBLE_EQ(10.5, position.getAltitude());
    EXPECT_DOUBLE_EQ(3.0, position.getAccuracy());
    EXPECT_EQ(timestamp, position.getTimestamp());
}

// Test setters
TEST_F(PositionTest, Setters) {
    Position position;
    
    // Use setters to modify the position
    position.setLatitude(37.7749);
    position.setLongitude(-122.4194);
    position.setAltitude(15.2);
    position.setAccuracy(4.5);
    
    auto timestamp = createTimestamp(2023, 6, 20, 14, 45, 30);
    position.setTimestamp(timestamp);
    
    // Verify the values were set correctly
    EXPECT_DOUBLE_EQ(37.7749, position.getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194, position.getLongitude());
    EXPECT_DOUBLE_EQ(15.2, position.getAltitude());
    EXPECT_DOUBLE_EQ(4.5, position.getAccuracy());
    EXPECT_EQ(timestamp, position.getTimestamp());
}

// Test default constructor
TEST_F(PositionTest, DefaultConstructor) {
    Position position;
    
    // Default values should be set
    EXPECT_DOUBLE_EQ(0.0, position.getLatitude());
    EXPECT_DOUBLE_EQ(0.0, position.getLongitude());
    EXPECT_DOUBLE_EQ(0.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(DEFAULT_POSITION_ACCURACY, position.getAccuracy());
    
    // Timestamp should be close to current time
    auto now = getCurrentTimestamp();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(
        now - position.getTimestamp()).count();
    
    // Should be within a few seconds
    EXPECT_LT(std::abs(diff), 5);
}

// Test builder pattern
TEST_F(PositionTest, Builder) {
    auto timestamp = createTimestamp(2023, 7, 10, 8, 15, 45);
    
    // Use builder to create a position
    Position position = Position::builder()
        .withLatitude(51.5074)
        .withLongitude(-0.1278)
        .withAltitude(25.0)
        .withAccuracy(1.5)
        .withTimestamp(timestamp)
        .build();
    
    // Verify the values were set correctly
    EXPECT_DOUBLE_EQ(51.5074, position.getLatitude());
    EXPECT_DOUBLE_EQ(-0.1278, position.getLongitude());
    EXPECT_DOUBLE_EQ(25.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(1.5, position.getAccuracy());
    EXPECT_EQ(timestamp, position.getTimestamp());
}

// Test distance calculation
TEST_F(PositionTest, DistanceTo) {
    // New York City coordinates
    Position nyc(40.7128, -74.0060);
    
    // Los Angeles coordinates
    Position la(34.0522, -118.2437);
    
    // Distance between NYC and LA is approximately 3935 km
    double distance = nyc.distanceTo(la);
    
    // Allow for some floating-point precision differences
    EXPECT_NEAR(3935000.0, distance, 5000.0);
    
    // Distance to self should be 0
    EXPECT_DOUBLE_EQ(0.0, nyc.distanceTo(nyc));
    
    // Test with points that are very close
    Position nyc2(40.7129, -74.0061);  // Very close to NYC
    double shortDistance = nyc.distanceTo(nyc2);
    
    // Should be a small distance (less than 50 meters)
    EXPECT_LT(shortDistance, 50.0);
    EXPECT_GT(shortDistance, 0.0);
}

// Test toString method
TEST_F(PositionTest, ToString) {
    // Create a position with a fixed timestamp for consistent testing
    auto timestamp = createTimestamp(2023, 8, 25, 12, 30, 45);
    Position position(48.8566, 2.3522, 35.5, 2.0, timestamp);
    
    // Get the string representation
    std::string posStr = position.toString();
    
    // Check that the string contains the expected values
    EXPECT_THAT(posStr, ::testing::HasSubstr("lat=48.856600"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("lon=2.352200"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("alt=35.50"));
    EXPECT_THAT(posStr, ::testing::HasSubstr("acc=2.00"));
    
    // The time part is platform-dependent due to localization,
    // so we just check that it contains a date and time format
    EXPECT_THAT(posStr, ::testing::HasSubstr("time="));
    EXPECT_THAT(posStr, ::testing::ContainsRegex("\\d{4}-\\d{2}-\\d{2}"));
}

// Test edge cases for distance calculation
TEST_F(PositionTest, DistanceEdgeCases) {
    // Test antipodal points (opposite sides of Earth)
    Position p1(0.0, 0.0);
    Position p2(0.0, 180.0);
    
    // Distance should be approximately half the Earth's circumference
    double distance = p1.distanceTo(p2);
    double expectedDistance = M_PI * EARTH_RADIUS_METERS;
    
    EXPECT_NEAR(expectedDistance, distance, 1.0);
    
    // Test points at the poles
    Position northPole(90.0, 0.0);
    Position southPole(-90.0, 0.0);
    
    // Distance should be approximately Earth's diameter / 2
    distance = northPole.distanceTo(southPole);
    expectedDistance = M_PI * EARTH_RADIUS_METERS;
    
    EXPECT_NEAR(expectedDistance, distance, 1.0);
}

// Test with invalid coordinates
TEST_F(PositionTest, InvalidCoordinates) {
    // Create positions with extreme coordinates
    Position extremeLat(200.0, 0.0);  // Invalid latitude
    Position extremeLon(0.0, 200.0);  // Invalid longitude
    Position normal(0.0, 0.0);
    
    // The distance calculation should still work mathematically
    // even with invalid coordinates
    double distance = extremeLat.distanceTo(normal);
    EXPECT_FALSE(std::isnan(distance));
    
    distance = extremeLon.distanceTo(normal);
    EXPECT_FALSE(std::isnan(distance));
}

}  // namespace
}  // namespace equipment_tracker
// </test_code>