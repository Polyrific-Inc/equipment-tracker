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
    // Can't test exact timestamp, but it should be close to now
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(
        now - position.getTimestamp()).count();
    EXPECT_LT(std::abs(diff), 2); // Within 2 seconds
}

TEST_F(PositionTest, ParameterizedConstructor) {
    Position position(10.0, 20.0, 30.0, 5.0);
    EXPECT_DOUBLE_EQ(10.0, position.getLatitude());
    EXPECT_DOUBLE_EQ(20.0, position.getLongitude());
    EXPECT_DOUBLE_EQ(30.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(5.0, position.getAccuracy());
}

TEST_F(PositionTest, BuilderPattern) {
    auto timestamp = getCurrentTimestamp();
    Position position = Position::builder()
                            .withLatitude(15.0)
                            .withLongitude(25.0)
                            .withAltitude(35.0)
                            .withAccuracy(4.5)
                            .withTimestamp(timestamp)
                            .build();
    
    EXPECT_DOUBLE_EQ(15.0, position.getLatitude());
    EXPECT_DOUBLE_EQ(25.0, position.getLongitude());
    EXPECT_DOUBLE_EQ(35.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(4.5, position.getAccuracy());
    EXPECT_EQ(timestamp, position.getTimestamp());
}

TEST_F(PositionTest, Setters) {
    Position position;
    position.setLatitude(45.0);
    position.setLongitude(-75.0);
    position.setAltitude(100.0);
    position.setAccuracy(3.0);
    
    auto timestamp = getCurrentTimestamp();
    position.setTimestamp(timestamp);
    
    EXPECT_DOUBLE_EQ(45.0, position.getLatitude());
    EXPECT_DOUBLE_EQ(-75.0, position.getLongitude());
    EXPECT_DOUBLE_EQ(100.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(3.0, position.getAccuracy());
    EXPECT_EQ(timestamp, position.getTimestamp());
}

TEST_F(PositionTest, DistanceCalculation) {
    // The distance between San Francisco and Los Angeles is approximately 559 km
    // But we'll allow for some variation due to different calculation methods
    double distance = sanFrancisco.distanceTo(losAngeles);
    EXPECT_NEAR(559000.0, distance, 10000.0); // Within 10 km
    
    // Distance should be the same in reverse
    double reverseDistance = losAngeles.distanceTo(sanFrancisco);
    EXPECT_DOUBLE_EQ(distance, reverseDistance);
    
    // Distance to self should be 0
    EXPECT_DOUBLE_EQ(0.0, sanFrancisco.distanceTo(sanFrancisco));
}

TEST_F(PositionTest, ToStringOutput) {
    std::string sfString = sanFrancisco.toString();
    EXPECT_THAT(sfString, HasSubstr("37.7749"));
    EXPECT_THAT(sfString, HasSubstr("-122.4194"));
    EXPECT_THAT(sfString, HasSubstr("10"));
    
    std::string laString = losAngeles.toString();
    EXPECT_THAT(laString, HasSubstr("34.0522"));
    EXPECT_THAT(laString, HasSubstr("-118.2437"));
    EXPECT_THAT(laString, HasSubstr("50"));
    EXPECT_THAT(laString, HasSubstr("1.5")); // Accuracy
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
    EXPECT_FALSE(forklift->getLastPosition().has_value()); // No position initially
}

TEST_F(EquipmentTest, CopyConstructor) {
    forklift->setLastPosition(sanFrancisco);
    Equipment copiedForklift(*forklift);
    
    EXPECT_EQ(forklift->getId(), copiedForklift.getId());
    EXPECT_EQ(forklift->getType(), copiedForklift.getType());
    EXPECT_EQ(forklift->getName(), copiedForklift.getName());
    EXPECT_EQ(forklift->getStatus(), copiedForklift.getStatus());
    
    auto originalPos = forklift->getLastPosition();
    auto copiedPos = copiedForklift.getLastPosition();
    
    ASSERT_TRUE(originalPos.has_value());
    ASSERT_TRUE(copiedPos.has_value());
    EXPECT_DOUBLE_EQ(originalPos->getLatitude(), copiedPos->getLatitude());
    EXPECT_DOUBLE_EQ(originalPos->getLongitude(), copiedPos->getLongitude());
}

TEST_F(EquipmentTest, MoveConstructor) {
    forklift->setLastPosition(sanFrancisco);
    std::string originalId = forklift->getId();
    
    Equipment movedForklift(std::move(*forklift));
    
    EXPECT_EQ(originalId, movedForklift.getId());
    EXPECT_EQ(EquipmentType::Forklift, movedForklift.getType());
    
    auto movedPos = movedForklift.getLastPosition();
    ASSERT_TRUE(movedPos.has_value());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLatitude(), movedPos->getLatitude());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLongitude(), movedPos->getLongitude());
}

TEST_F(EquipmentTest, CopyAssignment) {
    forklift->setLastPosition(sanFrancisco);
    
    Equipment anotherForklift("FORKLIFT-002", EquipmentType::Forklift, "Another Forklift");
    anotherForklift = *forklift;
    
    EXPECT_EQ(forklift->getId(), anotherForklift.getId());
    EXPECT_EQ(forklift->getName(), anotherForklift.getName());
    
    auto originalPos = forklift->getLastPosition();
    auto copiedPos = anotherForklift.getLastPosition();
    
    ASSERT_TRUE(originalPos.has_value());
    ASSERT_TRUE(copiedPos.has_value());
    EXPECT_DOUBLE_EQ(originalPos->getLatitude(), copiedPos->getLatitude());
    EXPECT_DOUBLE_EQ(originalPos->getLongitude(), copiedPos->getLongitude());
}

TEST_F(EquipmentTest, MoveAssignment) {
    forklift->setLastPosition(sanFrancisco);
    std::string originalId = forklift->getId();
    
    Equipment anotherForklift("FORKLIFT-002", EquipmentType::Forklift, "Another Forklift");
    anotherForklift = std::move(*forklift);
    
    EXPECT_EQ(originalId, anotherForklift.getId());
    
    auto movedPos = anotherForklift.getLastPosition();
    ASSERT_TRUE(movedPos.has_value());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLatitude(), movedPos->getLatitude());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLongitude(), movedPos->getLongitude());
}

TEST_F(EquipmentTest, SettersAndGetters) {
    forklift->setName("Updated Forklift Name");
    forklift->setStatus(EquipmentStatus::Maintenance);
    
    EXPECT_EQ("Updated Forklift Name", forklift->getName());
    EXPECT_EQ(EquipmentStatus::Maintenance, forklift->getStatus());
}

TEST_F(EquipmentTest, PositionManagement) {
    // Initially no position
    EXPECT_FALSE(forklift->getLastPosition().has_value());
    
    // Set position
    forklift->setLastPosition(sanFrancisco);
    auto lastPos = forklift->getLastPosition();
    ASSERT_TRUE(lastPos.has_value());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLatitude(), lastPos->getLatitude());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLongitude(), lastPos->getLongitude());
    
    // Update position
    forklift->setLastPosition(losAngeles);
    lastPos = forklift->getLastPosition();
    ASSERT_TRUE(lastPos.has_value());
    EXPECT_DOUBLE_EQ(losAngeles.getLatitude(), lastPos->getLatitude());
    EXPECT_DOUBLE_EQ(losAngeles.getLongitude(), lastPos->getLongitude());
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
    
    // Check positions in history (should be in order of recording)
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
        Position pos(37.7749 + i * 0.001, -122.4194 + i * 0.001);
        forklift->recordPosition(pos);
    }
    
    // History should be limited to DEFAULT_MAX_HISTORY_SIZE
    auto history = forklift->getPositionHistory();
    EXPECT_EQ(DEFAULT_MAX_HISTORY_SIZE, history.size());
}

TEST_F(EquipmentTest, IsMovingDetection) {
    // Initially not moving (no positions)
    EXPECT_FALSE(forklift->isMoving());
    
    // Record a single position - still not moving
    forklift->recordPosition(sanFrancisco);
    EXPECT_FALSE(forklift->isMoving());
    
    // Record a position very close to the first one - should not be considered moving
    Position nearbyPosition(37.7749 + 0.00001, -122.4194 + 0.00001); // Very small change
    forklift->recordPosition(nearbyPosition);
    EXPECT_FALSE(forklift->isMoving());
    
    // Record a position far from the previous one - should be considered moving
    forklift->recordPosition(losAngeles);
    EXPECT_TRUE(forklift->isMoving());
}

TEST_F(EquipmentTest, ToStringOutput) {
    forklift->setLastPosition(sanFrancisco);
    std::string equipmentString = forklift->toString();
    
    EXPECT_THAT(equipmentString, HasSubstr("FORKLIFT-001"));
    EXPECT_THAT(equipmentString, HasSubstr("Warehouse Forklift 1"));
    EXPECT_THAT(equipmentString, HasSubstr("Forklift"));
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
    auto now = std::chrono::system_clock::now();
    auto timestamp = getCurrentTimestamp();
    
    // The timestamps should be close to each other
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(
        now - timestamp).count();
    EXPECT_LT(std::abs(diff), 2); // Within 2 seconds
}

TEST_F(TimeUtilsTest, TimestampFormatting) {
    // Format the timestamp with a specific format
    std::string formatted = formatTimestamp(testTime, "%Y-%m-%d");
    
    // Check that the format is correct (YYYY-MM-DD)
    EXPECT_THAT(formatted, MatchesRegex("\\d{4}-\\d{2}-\\d{2}"));
}

TEST_F(TimeUtilsTest, TimestampParsing) {
    // Format the timestamp to a string
    std::string timeStr = formatTimestamp(testTime, "%Y-%m-%d %H:%M:%S");
    
    // Parse it back to a timestamp
    auto parsedTime = parseTimestamp(timeStr, "%Y-%m-%d %H:%M:%S");
    
    // The timestamps should be equal (within seconds precision)
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(
        testTime - parsedTime).count();
    EXPECT_EQ(0, diff);
}

TEST_F(TimeUtilsTest, TimestampDifference) {
    // Create timestamps with known differences
    auto laterTime = addSeconds(testTime, 3600); // 1 hour later
    
    EXPECT_EQ(3600, timestampDiffSeconds(laterTime, testTime));
    EXPECT_EQ(60, timestampDiffMinutes(laterTime, testTime));
    EXPECT_EQ(1, timestampDiffHours(laterTime, testTime));
    EXPECT_EQ(0, timestampDiffDays(laterTime, testTime));
    
    // Test negative differences
    EXPECT_EQ(-3600, timestampDiffSeconds(testTime, laterTime));
    EXPECT_EQ(-60, timestampDiffMinutes(testTime, laterTime));
    EXPECT_EQ(-1, timestampDiffHours(testTime, laterTime));
    EXPECT_EQ(0, timestampDiffDays(testTime, laterTime));
}

TEST_F(TimeUtilsTest, TimestampAddition) {
    // Test adding time
    auto oneHourLater = addHours(testTime, 1);
    auto oneDayLater = addDays(testTime, 1);
    
    EXPECT_EQ(3600, timestampDiffSeconds(oneHourLater, testTime));
    EXPECT_EQ(86400, timestampDiffSeconds(oneDayLater, testTime));
    
    // Test adding negative time (subtraction)
    auto oneHourEarlier = addHours(testTime, -1);
    auto oneDayEarlier = addDays(testTime, -1);
    
    EXPECT_EQ(-3600, timestampDiffSeconds(oneHourEarlier, testTime));
    EXPECT_EQ(-86400, timestampDiffSeconds(oneDayEarlier, testTime));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>