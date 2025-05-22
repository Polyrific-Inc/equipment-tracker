// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <chrono>
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/constants.h"

namespace equipment_tracker {

class PositionTest : public ::testing::Test {
protected:
    // Helper method to create a timestamp for a specific date/time
    static std::chrono::system_clock::time_point createTimestamp(int year, int month, int day, 
                                                                int hour, int minute, int second) {
        std::tm timeinfo = {};
        timeinfo.tm_year = year - 1900;  // Years since 1900
        timeinfo.tm_mon = month - 1;     // Months since January (0-11)
        timeinfo.tm_mday = day;          // Day of the month (1-31)
        timeinfo.tm_hour = hour;         // Hours (0-23)
        timeinfo.tm_min = minute;        // Minutes (0-59)
        timeinfo.tm_sec = second;        // Seconds (0-59)
        timeinfo.tm_isdst = -1;          // Determine DST automatically

        std::time_t time_t;
#ifdef _WIN32
        time_t = _mkgmtime(&timeinfo);
#else
        time_t = timegm(&timeinfo);
#endif
        return std::chrono::system_clock::from_time_t(time_t);
    }
};

TEST_F(PositionTest, ConstructorInitializesAllFields) {
    // Arrange
    double latitude = 37.7749;
    double longitude = -122.4194;
    double altitude = 10.5;
    double accuracy = 5.0;
    auto timestamp = std::chrono::system_clock::now();
    
    // Act
    Position position(latitude, longitude, altitude, accuracy, timestamp);
    
    // Assert
    EXPECT_DOUBLE_EQ(latitude, position.getLatitude());
    EXPECT_DOUBLE_EQ(longitude, position.getLongitude());
    EXPECT_DOUBLE_EQ(altitude, position.getAltitude());
    EXPECT_DOUBLE_EQ(accuracy, position.getAccuracy());
    EXPECT_EQ(timestamp, position.getTimestamp());
}

TEST_F(PositionTest, DistanceToSamePosition) {
    // Arrange
    Position position(37.7749, -122.4194, 10.5, 5.0, std::chrono::system_clock::now());
    
    // Act
    double distance = position.distanceTo(position);
    
    // Assert
    EXPECT_NEAR(0.0, distance, 0.001);
}

TEST_F(PositionTest, DistanceToDifferentPosition) {
    // Arrange
    Position sf(37.7749, -122.4194, 10.5, 5.0, std::chrono::system_clock::now());
    Position ny(40.7128, -74.0060, 10.0, 5.0, std::chrono::system_clock::now());
    
    // Act
    double distance = sf.distanceTo(ny);
    
    // Assert - Distance between SF and NY is approximately 4130 km
    EXPECT_NEAR(4130000.0, distance, 5000.0);
}

TEST_F(PositionTest, DistanceToNearbyPosition) {
    // Arrange - Two positions 100 meters apart
    Position pos1(37.7749, -122.4194, 10.5, 5.0, std::chrono::system_clock::now());
    // Move approximately 100 meters north
    Position pos2(37.7758, -122.4194, 10.5, 5.0, std::chrono::system_clock::now());
    
    // Act
    double distance = pos1.distanceTo(pos2);
    
    // Assert
    EXPECT_NEAR(100.0, distance, 5.0);
}

TEST_F(PositionTest, DistanceToAntipodes) {
    // Arrange - Antipodal points (opposite sides of Earth)
    Position pos1(0.0, 0.0, 0.0, 5.0, std::chrono::system_clock::now());
    Position pos2(0.0, 180.0, 0.0, 5.0, std::chrono::system_clock::now());
    
    // Act
    double distance = pos1.distanceTo(pos2);
    
    // Assert - Should be approximately half the Earth's circumference
    EXPECT_NEAR(M_PI * EARTH_RADIUS_METERS, distance, 1.0);
}

TEST_F(PositionTest, ToStringFormatsCorrectly) {
    // Arrange
    auto timestamp = createTimestamp(2023, 5, 15, 14, 30, 0);
    Position position(37.774900, -122.419400, 10.50, 5.00, timestamp);
    
    // Act
    std::string result = position.toString();
    
    // Assert
    EXPECT_THAT(result, ::testing::HasSubstr("lat=37.774900"));
    EXPECT_THAT(result, ::testing::HasSubstr("lon=-122.419400"));
    EXPECT_THAT(result, ::testing::HasSubstr("alt=10.50m"));
    EXPECT_THAT(result, ::testing::HasSubstr("acc=5.00m"));
    EXPECT_THAT(result, ::testing::HasSubstr("2023-05-15"));
}

TEST_F(PositionTest, ToStringHandlesZeroValues) {
    // Arrange
    auto timestamp = createTimestamp(2023, 5, 15, 14, 30, 0);
    Position position(0.0, 0.0, 0.0, 0.0, timestamp);
    
    // Act
    std::string result = position.toString();
    
    // Assert
    EXPECT_THAT(result, ::testing::HasSubstr("lat=0.000000"));
    EXPECT_THAT(result, ::testing::HasSubstr("lon=0.000000"));
    EXPECT_THAT(result, ::testing::HasSubstr("alt=0.00m"));
    EXPECT_THAT(result, ::testing::HasSubstr("acc=0.00m"));
}

TEST_F(PositionTest, ToStringHandlesNegativeValues) {
    // Arrange
    auto timestamp = createTimestamp(2023, 5, 15, 14, 30, 0);
    Position position(-37.774900, -122.419400, -10.50, 5.00, timestamp);
    
    // Act
    std::string result = position.toString();
    
    // Assert
    EXPECT_THAT(result, ::testing::HasSubstr("lat=-37.774900"));
    EXPECT_THAT(result, ::testing::HasSubstr("lon=-122.419400"));
    EXPECT_THAT(result, ::testing::HasSubstr("alt=-10.50m"));
}

TEST_F(PositionTest, DistanceToEdgeCases) {
    // Arrange
    auto timestamp = std::chrono::system_clock::now();
    Position northPole(90.0, 0.0, 0.0, 0.0, timestamp);
    Position southPole(-90.0, 0.0, 0.0, 0.0, timestamp);
    Position equator(0.0, 0.0, 0.0, 0.0, timestamp);
    
    // Act & Assert
    // Distance from North Pole to South Pole should be approximately Earth's circumference / 2
    EXPECT_NEAR(M_PI * EARTH_RADIUS_METERS, northPole.distanceTo(southPole), 1.0);
    
    // Distance from North Pole to Equator should be approximately Earth's circumference / 4
    EXPECT_NEAR(M_PI * EARTH_RADIUS_METERS / 2, northPole.distanceTo(equator), 1.0);
}

} // namespace equipment_tracker
// </test_code>