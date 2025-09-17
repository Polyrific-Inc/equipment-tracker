// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/position.h"
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/utils/time_utils.h"
#include <chrono>
#include <thread>
#include <cmath>

using namespace equipment_tracker;
using namespace testing;

// Cross-platform compatibility
#ifdef _WIN32
#define M_PI 3.14159265358979323846
#endif

class PositionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Default test positions
        sanFrancisco = Position(37.7749, -122.4194, 10.0);
        losAngeles = Position(34.0522, -118.2437, 50.0, 1.5);
    }

    Position sanFrancisco;
    Position losAngeles;
};

TEST_F(PositionTest, DefaultConstructor) {
    Position position;
    EXPECT_DOUBLE_EQ(0.0, position.getLatitude());
    EXPECT_DOUBLE_EQ(0.0, position.getLongitude());
    EXPECT_DOUBLE_EQ(0.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(DEFAULT_POSITION_ACCURACY, position.getAccuracy());
    EXPECT_NE(Timestamp(), position.getTimestamp());
}

TEST_F(PositionTest, ParameterizedConstructor) {
    Position position(10.0, 20.0, 30.0, 5.0);
    EXPECT_DOUBLE_EQ(10.0, position.getLatitude());
    EXPECT_DOUBLE_EQ(20.0, position.getLongitude());
    EXPECT_DOUBLE_EQ(30.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(5.0, position.getAccuracy());
    EXPECT_NE(Timestamp(), position.getTimestamp());
}

TEST_F(PositionTest, SettersAndGetters) {
    Position position;
    
    position.setLatitude(45.0);
    position.setLongitude(90.0);
    position.setAltitude(100.0);
    position.setAccuracy(3.5);
    
    Timestamp customTime = std::chrono::system_clock::now();
    position.setTimestamp(customTime);
    
    EXPECT_DOUBLE_EQ(45.0, position.getLatitude());
    EXPECT_DOUBLE_EQ(90.0, position.getLongitude());
    EXPECT_DOUBLE_EQ(100.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(3.5, position.getAccuracy());
    EXPECT_EQ(customTime, position.getTimestamp());
}

TEST_F(PositionTest, BuilderPattern) {
    Timestamp customTime = std::chrono::system_clock::now();
    
    Position position = Position::builder()
        .withLatitude(51.5074)
        .withLongitude(-0.1278)
        .withAltitude(25.0)
        .withAccuracy(2.0)
        .withTimestamp(customTime)
        .build();
    
    EXPECT_DOUBLE_EQ(51.5074, position.getLatitude());
    EXPECT_DOUBLE_EQ(-0.1278, position.getLongitude());
    EXPECT_DOUBLE_EQ(25.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(2.0, position.getAccuracy());
    EXPECT_EQ(customTime, position.getTimestamp());
}

TEST_F(PositionTest, DistanceCalculation) {
    // The distance between San Francisco and Los Angeles is approximately 559 km
    // We'll allow for some floating point imprecision with a tolerance of 1000 meters
    double distance = sanFrancisco.distanceTo(losAngeles);
    EXPECT_NEAR(559000.0, distance, 1000.0);
    
    // Distance should be symmetric
    double reverseDistance = losAngeles.distanceTo(sanFrancisco);
    EXPECT_DOUBLE_EQ(distance, reverseDistance);
    
    // Distance to self should be 0
    EXPECT_DOUBLE_EQ(0.0, sanFrancisco.distanceTo(sanFrancisco));
}

TEST_F(PositionTest, ToStringOutput) {
    std::string positionStr = sanFrancisco.toString();
    EXPECT_THAT(positionStr, HasSubstr("37.7749"));
    EXPECT_THAT(positionStr, HasSubstr("-122.419"));
    EXPECT_THAT(positionStr, HasSubstr("10"));
}

class EquipmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        forklift = std::make_unique<Equipment>("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
        sanFrancisco = Position(37.7749, -122.4194, 10.0);
        losAngeles = Position(34.0522, -118.2437, 50.0, 1.5);
    }

    std::unique_ptr<Equipment> forklift;
    Position sanFrancisco;
    Position losAngeles;
};

TEST_F(EquipmentTest, Constructor) {
    EXPECT_EQ("FORKLIFT-001", forklift->getId());
    EXPECT_EQ(EquipmentType::Forklift, forklift->getType());
    EXPECT_EQ("Warehouse Forklift 1", forklift->getName());
    EXPECT_EQ(EquipmentStatus::Active, forklift->getStatus());
    EXPECT_FALSE(forklift->getLastPosition().has_value());
}

TEST_F(EquipmentTest, CopyConstructorAndAssignment) {
    // Set position to test copying
    forklift->setLastPosition(sanFrancisco);
    
    // Test copy constructor
    Equipment forkliftCopy(*forklift);
    EXPECT_EQ(forklift->getId(), forkliftCopy.getId());
    EXPECT_EQ(forklift->getType(), forkliftCopy.getType());
    EXPECT_EQ(forklift->getName(), forkliftCopy.getName());
    EXPECT_EQ(forklift->getStatus(), forkliftCopy.getStatus());
    
    auto originalPos = forklift->getLastPosition();
    auto copiedPos = forkliftCopy.getLastPosition();
    ASSERT_TRUE(originalPos.has_value());
    ASSERT_TRUE(copiedPos.has_value());
    EXPECT_DOUBLE_EQ(originalPos->getLatitude(), copiedPos->getLatitude());
    EXPECT_DOUBLE_EQ(originalPos->getLongitude(), copiedPos->getLongitude());
    
    // Test copy assignment
    Equipment forkliftAssign("TEMP-ID", EquipmentType::Crane, "Temp");
    forkliftAssign = *forklift;
    EXPECT_EQ(forklift->getId(), forkliftAssign.getId());
    EXPECT_EQ(forklift->getType(), forkliftAssign.getType());
    EXPECT_EQ(forklift->getName(), forkliftAssign.getName());
}

TEST_F(EquipmentTest, MoveConstructorAndAssignment) {
    // Set position to test moving
    forklift->setLastPosition(sanFrancisco);
    
    // Test move constructor
    Equipment forkliftMoved(std::move(*forklift));
    EXPECT_EQ("FORKLIFT-001", forkliftMoved.getId());
    EXPECT_EQ(EquipmentType::Forklift, forkliftMoved.getType());
    EXPECT_EQ("Warehouse Forklift 1", forkliftMoved.getName());
    
    auto movedPos = forkliftMoved.getLastPosition();
    ASSERT_TRUE(movedPos.has_value());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLatitude(), movedPos->getLatitude());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLongitude(), movedPos->getLongitude());
    
    // Recreate forklift for move assignment test
    forklift = std::make_unique<Equipment>("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    forklift->setLastPosition(sanFrancisco);
    
    // Test move assignment
    Equipment forkliftAssign("TEMP-ID", EquipmentType::Crane, "Temp");
    forkliftAssign = std::move(*forklift);
    EXPECT_EQ("FORKLIFT-001", forkliftAssign.getId());
    EXPECT_EQ(EquipmentType::Forklift, forkliftAssign.getType());
    EXPECT_EQ("Warehouse Forklift 1", forkliftAssign.getName());
}

TEST_F(EquipmentTest, SettersAndGetters) {
    forklift->setName("New Forklift Name");
    forklift->setStatus(EquipmentStatus::Maintenance);
    
    EXPECT_EQ("New Forklift Name", forklift->getName());
    EXPECT_EQ(EquipmentStatus::Maintenance, forklift->getStatus());
}

TEST_F(EquipmentTest, PositionManagement) {
    // Initially no position
    EXPECT_FALSE(forklift->getLastPosition().has_value());
    
    // Set position
    forklift->setLastPosition(sanFrancisco);
    auto lastPosition = forklift->getLastPosition();
    ASSERT_TRUE(lastPosition.has_value());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLatitude(), lastPosition->getLatitude());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLongitude(), lastPosition->getLongitude());
    
    // Update position
    forklift->setLastPosition(losAngeles);
    lastPosition = forklift->getLastPosition();
    ASSERT_TRUE(lastPosition.has_value());
    EXPECT_DOUBLE_EQ(losAngeles.getLatitude(), lastPosition->getLatitude());
    EXPECT_DOUBLE_EQ(losAngeles.getLongitude(), lastPosition->getLongitude());
}

TEST_F(EquipmentTest, PositionHistory) {
    // Initially empty history
    EXPECT_TRUE(forklift->getPositionHistory().empty());
    
    // Record positions
    forklift->recordPosition(sanFrancisco);
    forklift->recordPosition(losAngeles);
    
    // Check history
    auto history = forklift->getPositionHistory();
    ASSERT_EQ(2, history.size());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLatitude(), history[0].getLatitude());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLongitude(), history[0].getLongitude());
    EXPECT_DOUBLE_EQ(losAngeles.getLatitude(), history[1].getLatitude());
    EXPECT_DOUBLE_EQ(losAngeles.getLongitude(), history[1].getLongitude());
    
    // Clear history
    forklift->clearPositionHistory();
    EXPECT_TRUE(forklift->getPositionHistory().empty());
}

TEST_F(EquipmentTest, HistorySizeLimit) {
    // Record more positions than the default history size
    for (size_t i = 0; i < DEFAULT_MAX_HISTORY_SIZE + 10; ++i) {
        Position pos(37.7749 + i * 0.001, -122.4194 + i * 0.001, 10.0);
        forklift->recordPosition(pos);
    }
    
    // Check that history size is limited
    auto history = forklift->getPositionHistory();
    EXPECT_EQ(DEFAULT_MAX_HISTORY_SIZE, history.size());
}

TEST_F(EquipmentTest, MovementDetection) {
    // No movement with less than 2 positions
    forklift->clearPositionHistory();
    EXPECT_FALSE(forklift->isMoving());
    
    // Record stationary positions (very close together)
    Position pos1(37.7749, -122.4194, 10.0);
    forklift->recordPosition(pos1);
    
    // Still not enough positions to determine movement
    EXPECT_FALSE(forklift->isMoving());
    
    // Add another position very close to the first (not moving)
    Position pos2(37.77491, -122.41941, 10.0);
    forklift->recordPosition(pos2);
    
    // With very small movement and default threshold, should not be considered moving
    EXPECT_FALSE(forklift->isMoving());
    
    // Add a position with significant movement
    Position pos3(37.7760, -122.4210, 10.0);
    forklift->recordPosition(pos3);
    
    // Now should be considered moving
    EXPECT_TRUE(forklift->isMoving());
}

TEST_F(EquipmentTest, ToStringOutput) {
    forklift->setLastPosition(sanFrancisco);
    std::string equipmentStr = forklift->toString();
    
    EXPECT_THAT(equipmentStr, HasSubstr("FORKLIFT-001"));
    EXPECT_THAT(equipmentStr, HasSubstr("Warehouse Forklift 1"));
    EXPECT_THAT(equipmentStr, HasSubstr("Forklift"));
}

class TimeUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a fixed timestamp for testing
        testTime = std::chrono::system_clock::now();
    }

    Timestamp testTime;
};

TEST_F(TimeUtilsTest, GetCurrentTimestamp) {
    Timestamp now = getCurrentTimestamp();
    Timestamp systemNow = std::chrono::system_clock::now();
    
    // Current timestamp should be close to system time
    // Allow 100ms difference to account for test execution time
    int64_t diffMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        systemNow - now).count();
    EXPECT_LT(std::abs(diffMs), 100);
}

TEST_F(TimeUtilsTest, TimestampFormatting) {
    // Format timestamp with different formats
    std::string formatted = formatTimestamp(testTime, "%Y-%m-%d");
    
    // Basic validation - should have year, month, day separated by hyphens
    EXPECT_THAT(formatted, MatchesRegex("\\d{4}-\\d{2}-\\d{2}"));
    
    // Test time formatting
    std::string timeFormatted = formatTimestamp(testTime, "%H:%M:%S");
    EXPECT_THAT(timeFormatted, MatchesRegex("\\d{2}:\\d{2}:\\d{2}"));
}

TEST_F(TimeUtilsTest, TimestampParsing) {
    // Format a timestamp, then parse it back
    std::string formatted = formatTimestamp(testTime, "%Y-%m-%d %H:%M:%S");
    Timestamp parsed = parseTimestamp(formatted, "%Y-%m-%d %H:%M:%S");
    
    // The parsed time should be close to the original
    // We lose sub-second precision in the format/parse cycle
    int64_t diffSeconds = std::abs(timestampDiffSeconds(testTime, parsed));
    EXPECT_LT(diffSeconds, 1);
}

TEST_F(TimeUtilsTest, TimestampDifference) {
    // Create timestamps 1 hour apart
    Timestamp later = addHours(testTime, 1);
    
    EXPECT_EQ(3600, timestampDiffSeconds(later, testTime));
    EXPECT_EQ(60, timestampDiffMinutes(later, testTime));
    EXPECT_EQ(1, timestampDiffHours(later, testTime));
    EXPECT_EQ(0, timestampDiffDays(later, testTime));
    
    // Test negative differences (earlier - later)
    EXPECT_EQ(-3600, timestampDiffSeconds(testTime, later));
    
    // Test day difference
    Timestamp muchLater = addDays(testTime, 2);
    EXPECT_EQ(2, timestampDiffDays(muchLater, testTime));
}

TEST_F(TimeUtilsTest, TimestampAddition) {
    // Test adding time
    Timestamp plus1Sec = addSeconds(testTime, 1);
    EXPECT_EQ(1, timestampDiffSeconds(plus1Sec, testTime));
    
    Timestamp plus5Min = addMinutes(testTime, 5);
    EXPECT_EQ(5, timestampDiffMinutes(plus5Min, testTime));
    
    Timestamp plus2Hours = addHours(testTime, 2);
    EXPECT_EQ(2, timestampDiffHours(plus2Hours, testTime));
    
    Timestamp plus3Days = addDays(testTime, 3);
    EXPECT_EQ(3, timestampDiffDays(plus3Days, testTime));
    
    // Test negative addition (subtraction)
    Timestamp minus1Hour = addHours(testTime, -1);
    EXPECT_EQ(-1, timestampDiffHours(minus1Hour, testTime));
}
// </test_code>