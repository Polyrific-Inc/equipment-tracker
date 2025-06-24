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

// Platform-specific time handling
#ifdef _WIN32
#define PLATFORM_SPECIFIC_TIME_HANDLING 1
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
    // Timestamp should be set to current time, can't test exact value
}

TEST_F(PositionTest, ParameterizedConstructor) {
    Position position(37.7749, -122.4194, 10.0, 2.0);
    EXPECT_DOUBLE_EQ(37.7749, position.getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194, position.getLongitude());
    EXPECT_DOUBLE_EQ(10.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(2.0, position.getAccuracy());
}

TEST_F(PositionTest, BuilderPattern) {
    Position position = Position::builder()
                            .withLatitude(34.0522)
                            .withLongitude(-118.2437)
                            .withAltitude(50.0)
                            .withAccuracy(1.5)
                            .build();
    
    EXPECT_DOUBLE_EQ(34.0522, position.getLatitude());
    EXPECT_DOUBLE_EQ(-118.2437, position.getLongitude());
    EXPECT_DOUBLE_EQ(50.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(1.5, position.getAccuracy());
}

TEST_F(PositionTest, SettersAndGetters) {
    Position position;
    
    position.setLatitude(37.7749);
    position.setLongitude(-122.4194);
    position.setAltitude(10.0);
    position.setAccuracy(2.0);
    
    EXPECT_DOUBLE_EQ(37.7749, position.getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194, position.getLongitude());
    EXPECT_DOUBLE_EQ(10.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(2.0, position.getAccuracy());
    
    Timestamp now = getCurrentTimestamp();
    position.setTimestamp(now);
    EXPECT_EQ(now, position.getTimestamp());
}

TEST_F(PositionTest, DistanceCalculation) {
    // The distance between San Francisco and Los Angeles is approximately 559 km
    // But we'll allow for some floating point variance
    double distance = sanFrancisco.distanceTo(losAngeles);
    EXPECT_NEAR(559000.0, distance, 1000.0); // Within 1 km of expected
    
    // Distance should be the same in reverse
    double reverseDistance = losAngeles.distanceTo(sanFrancisco);
    EXPECT_DOUBLE_EQ(distance, reverseDistance);
    
    // Distance to self should be 0
    EXPECT_DOUBLE_EQ(0.0, sanFrancisco.distanceTo(sanFrancisco));
}

TEST_F(PositionTest, ToStringOutput) {
    // We can't test exact string output due to timestamp formatting differences
    // but we can check that the string contains the expected coordinates
    std::string posStr = sanFrancisco.toString();
    EXPECT_THAT(posStr, HasSubstr("37.7749"));
    EXPECT_THAT(posStr, HasSubstr("-122.4194"));
    EXPECT_THAT(posStr, HasSubstr("10"));
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
    EXPECT_EQ(EquipmentStatus::Active, forklift->getStatus()); // Default status
}

TEST_F(EquipmentTest, CopyConstructorAndAssignment) {
    // Set a position to test copying
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
    Equipment forkliftAssigned("CRANE-001", EquipmentType::Crane, "Test Crane");
    forkliftAssigned = *forklift;
    EXPECT_EQ(forklift->getId(), forkliftAssigned.getId());
    EXPECT_EQ(forklift->getType(), forkliftAssigned.getType());
    EXPECT_EQ(forklift->getName(), forkliftAssigned.getName());
}

TEST_F(EquipmentTest, MoveConstructorAndAssignment) {
    // Set a position to test moving
    forklift->setLastPosition(sanFrancisco);
    
    // Capture ID for later comparison
    std::string originalId = forklift->getId();
    
    // Test move constructor
    Equipment forkliftMoved(std::move(*forklift));
    EXPECT_EQ(originalId, forkliftMoved.getId());
    EXPECT_EQ(EquipmentType::Forklift, forkliftMoved.getType());
    EXPECT_EQ("Warehouse Forklift 1", forkliftMoved.getName());
    
    auto movedPos = forkliftMoved.getLastPosition();
    ASSERT_TRUE(movedPos.has_value());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLatitude(), movedPos->getLatitude());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLongitude(), movedPos->getLongitude());
    
    // Recreate forklift for move assignment test
    forklift = std::make_unique<Equipment>("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    forklift->setLastPosition(sanFrancisco);
    originalId = forklift->getId();
    
    // Test move assignment
    Equipment forkliftAssigned("CRANE-001", EquipmentType::Crane, "Test Crane");
    forkliftAssigned = std::move(*forklift);
    EXPECT_EQ(originalId, forkliftAssigned.getId());
    EXPECT_EQ(EquipmentType::Forklift, forkliftAssigned.getType());
    EXPECT_EQ("Warehouse Forklift 1", forkliftAssigned.getName());
}

TEST_F(EquipmentTest, SettersAndGetters) {
    forklift->setName("Updated Forklift Name");
    EXPECT_EQ("Updated Forklift Name", forklift->getName());
    
    forklift->setStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(EquipmentStatus::Maintenance, forklift->getStatus());
    
    // Test position setting and getting
    EXPECT_FALSE(forklift->getLastPosition().has_value());
    
    forklift->setLastPosition(sanFrancisco);
    auto position = forklift->getLastPosition();
    ASSERT_TRUE(position.has_value());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLatitude(), position->getLatitude());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLongitude(), position->getLongitude());
    EXPECT_DOUBLE_EQ(sanFrancisco.getAltitude(), position->getAltitude());
}

TEST_F(EquipmentTest, PositionHistory) {
    // Initially history should be empty
    EXPECT_TRUE(forklift->getPositionHistory().empty());
    
    // Record positions
    forklift->recordPosition(sanFrancisco);
    forklift->recordPosition(losAngeles);
    
    // Check history
    auto history = forklift->getPositionHistory();
    ASSERT_EQ(2, history.size());
    
    // Check positions in history (should be in order of recording)
    EXPECT_DOUBLE_EQ(sanFrancisco.getLatitude(), history[0].getLatitude());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLongitude(), history[0].getLongitude());
    
    EXPECT_DOUBLE_EQ(losAngeles.getLatitude(), history[1].getLatitude());
    EXPECT_DOUBLE_EQ(losAngeles.getLongitude(), history[1].getLongitude());
    
    // Test clear history
    forklift->clearPositionHistory();
    EXPECT_TRUE(forklift->getPositionHistory().empty());
}

TEST_F(EquipmentTest, HistorySizeLimit) {
    // Record more positions than the default history size limit
    for (size_t i = 0; i < DEFAULT_MAX_HISTORY_SIZE + 10; ++i) {
        Position pos(37.7749 + (i * 0.001), -122.4194 + (i * 0.001), 10.0);
        forklift->recordPosition(pos);
    }
    
    // History should be limited to DEFAULT_MAX_HISTORY_SIZE
    auto history = forklift->getPositionHistory();
    EXPECT_EQ(DEFAULT_MAX_HISTORY_SIZE, history.size());
}

TEST_F(EquipmentTest, IsMoving) {
    // Without positions, should not be moving
    EXPECT_FALSE(forklift->isMoving());
    
    // Record a position
    forklift->recordPosition(sanFrancisco);
    
    // With only one position, should not be moving
    EXPECT_FALSE(forklift->isMoving());
    
    // Record a position very close to the first one (not moving)
    Position nearbyPosition(37.7749 + 0.00001, -122.4194 + 0.00001, 10.0);
    forklift->recordPosition(nearbyPosition);
    
    // Should still not be moving (positions too close)
    EXPECT_FALSE(forklift->isMoving());
    
    // Record a position far from the previous one (moving)
    // Wait a bit to ensure timestamps are different
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    forklift->recordPosition(losAngeles);
    
    // Should be moving now
    EXPECT_TRUE(forklift->isMoving());
}

TEST_F(EquipmentTest, ToStringOutput) {
    forklift->setLastPosition(sanFrancisco);
    
    std::string equipStr = forklift->toString();
    EXPECT_THAT(equipStr, HasSubstr("FORKLIFT-001"));
    EXPECT_THAT(equipStr, HasSubstr("Warehouse Forklift 1"));
    EXPECT_THAT(equipStr, HasSubstr("Forklift"));
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
    Timestamp before = std::chrono::system_clock::now();
    Timestamp current = getCurrentTimestamp();
    Timestamp after = std::chrono::system_clock::now();
    
    // Current timestamp should be between before and after
    EXPECT_LE(before, current);
    EXPECT_LE(current, after);
}

TEST_F(TimeUtilsTest, TimestampDiffCalculations) {
    // Create timestamps 1 hour apart
    Timestamp t1 = testTime;
    Timestamp t2 = addHours(t1, 1);
    
    EXPECT_EQ(3600, timestampDiffSeconds(t2, t1));
    EXPECT_EQ(60, timestampDiffMinutes(t2, t1));
    EXPECT_EQ(1, timestampDiffHours(t2, t1));
    EXPECT_EQ(0, timestampDiffDays(t2, t1));
    
    // Test negative differences (t1 later than t2)
    EXPECT_EQ(-3600, timestampDiffSeconds(t1, t2));
    EXPECT_EQ(-60, timestampDiffMinutes(t1, t2));
    EXPECT_EQ(-1, timestampDiffHours(t1, t2));
    EXPECT_EQ(0, timestampDiffDays(t1, t2));
    
    // Test day difference
    Timestamp t3 = addDays(t1, 2);
    EXPECT_EQ(2, timestampDiffDays(t3, t1));
    EXPECT_EQ(48, timestampDiffHours(t3, t1));
}

TEST_F(TimeUtilsTest, TimestampAddition) {
    Timestamp t1 = testTime;
    
    // Test adding seconds
    Timestamp t2 = addSeconds(t1, 30);
    EXPECT_EQ(30, timestampDiffSeconds(t2, t1));
    
    // Test adding minutes
    Timestamp t3 = addMinutes(t1, 45);
    EXPECT_EQ(45, timestampDiffMinutes(t3, t1));
    
    // Test adding hours
    Timestamp t4 = addHours(t1, 5);
    EXPECT_EQ(5, timestampDiffHours(t4, t1));
    
    // Test adding days
    Timestamp t5 = addDays(t1, 3);
    EXPECT_EQ(3, timestampDiffDays(t5, t1));
}

TEST_F(TimeUtilsTest, TimestampFormatAndParse) {
    // Format the timestamp
    std::string formatted = formatTimestamp(testTime, "%Y-%m-%d %H:%M:%S");
    
    // Parse it back
    Timestamp parsed = parseTimestamp(formatted, "%Y-%m-%d %H:%M:%S");
    
    // Due to second-level precision in the format string, the timestamps might differ by less than a second
    // So we check if they're within 1 second of each other
    EXPECT_LE(std::abs(timestampDiffSeconds(testTime, parsed)), 1);
}
// </test_code>