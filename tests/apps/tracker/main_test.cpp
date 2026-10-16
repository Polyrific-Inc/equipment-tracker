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
    auto now = getCurrentTimestamp();
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
                            .withLatitude(45.0)
                            .withLongitude(-75.0)
                            .withAltitude(100.0)
                            .withAccuracy(3.5)
                            .withTimestamp(timestamp)
                            .build();
    
    EXPECT_DOUBLE_EQ(45.0, position.getLatitude());
    EXPECT_DOUBLE_EQ(-75.0, position.getLongitude());
    EXPECT_DOUBLE_EQ(100.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(3.5, position.getAccuracy());
    EXPECT_EQ(timestamp, position.getTimestamp());
}

TEST_F(PositionTest, Setters) {
    Position position;
    position.setLatitude(35.0);
    position.setLongitude(-105.0);
    position.setAltitude(200.0);
    position.setAccuracy(4.0);
    
    auto timestamp = getCurrentTimestamp();
    position.setTimestamp(timestamp);
    
    EXPECT_DOUBLE_EQ(35.0, position.getLatitude());
    EXPECT_DOUBLE_EQ(-105.0, position.getLongitude());
    EXPECT_DOUBLE_EQ(200.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(4.0, position.getAccuracy());
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
    EquipmentType originalType = forklift->getType();
    std::string originalName = forklift->getName();
    
    Equipment movedForklift(std::move(*forklift));
    
    EXPECT_EQ(originalId, movedForklift.getId());
    EXPECT_EQ(originalType, movedForklift.getType());
    EXPECT_EQ(originalName, movedForklift.getName());
    
    auto movedPos = movedForklift.getLastPosition();
    ASSERT_TRUE(movedPos.has_value());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLatitude(), movedPos->getLatitude());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLongitude(), movedPos->getLongitude());
}

TEST_F(EquipmentTest, CopyAssignment) {
    forklift->setLastPosition(sanFrancisco);
    Equipment otherForklift("CRANE-001", EquipmentType::Crane, "Tower Crane");
    
    otherForklift = *forklift;
    
    EXPECT_EQ(forklift->getId(), otherForklift.getId());
    EXPECT_EQ(forklift->getType(), otherForklift.getType());
    EXPECT_EQ(forklift->getName(), otherForklift.getName());
    
    auto originalPos = forklift->getLastPosition();
    auto copiedPos = otherForklift.getLastPosition();
    
    ASSERT_TRUE(originalPos.has_value());
    ASSERT_TRUE(copiedPos.has_value());
    EXPECT_DOUBLE_EQ(originalPos->getLatitude(), copiedPos->getLatitude());
    EXPECT_DOUBLE_EQ(originalPos->getLongitude(), copiedPos->getLongitude());
}

TEST_F(EquipmentTest, MoveAssignment) {
    forklift->setLastPosition(sanFrancisco);
    std::string originalId = forklift->getId();
    EquipmentType originalType = forklift->getType();
    std::string originalName = forklift->getName();
    
    Equipment otherForklift("CRANE-001", EquipmentType::Crane, "Tower Crane");
    otherForklift = std::move(*forklift);
    
    EXPECT_EQ(originalId, otherForklift.getId());
    EXPECT_EQ(originalType, otherForklift.getType());
    EXPECT_EQ(originalName, otherForklift.getName());
    
    auto movedPos = otherForklift.getLastPosition();
    ASSERT_TRUE(movedPos.has_value());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLatitude(), movedPos->getLatitude());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLongitude(), movedPos->getLongitude());
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
    auto pos = forklift->getLastPosition();
    ASSERT_TRUE(pos.has_value());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLatitude(), pos->getLatitude());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLongitude(), pos->getLongitude());
    
    // Update position
    forklift->setLastPosition(losAngeles);
    pos = forklift->getLastPosition();
    ASSERT_TRUE(pos.has_value());
    EXPECT_DOUBLE_EQ(losAngeles.getLatitude(), pos->getLatitude());
    EXPECT_DOUBLE_EQ(losAngeles.getLongitude(), pos->getLongitude());
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
    
    // History should be in chronological order (oldest first)
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
    
    // The oldest entries should have been removed
    EXPECT_GT(history[0].getLatitude(), 37.7749);
}

TEST_F(EquipmentTest, IsMovingDetection) {
    // No movement initially (no positions)
    EXPECT_FALSE(forklift->isMoving());
    
    // Record a single position
    forklift->recordPosition(sanFrancisco);
    EXPECT_FALSE(forklift->isMoving()); // Still not moving (need at least 2 positions)
    
    // Record a second position with significant distance
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    forklift->recordPosition(losAngeles);
    EXPECT_TRUE(forklift->isMoving()); // Should detect movement
    
    // Record positions close together
    Position nearbyPos1(37.7749, -122.4194, 10.0);
    Position nearbyPos2(37.7749 + 0.0000001, -122.4194 + 0.0000001, 10.0);
    
    Equipment stationary("STATIONARY-001", EquipmentType::Other, "Stationary Equipment");
    stationary.recordPosition(nearbyPos1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    stationary.recordPosition(nearbyPos2);
    
    EXPECT_FALSE(stationary.isMoving()); // Should not detect movement for tiny changes
}

TEST_F(EquipmentTest, ToStringOutput) {
    forklift->setLastPosition(sanFrancisco);
    std::string info = forklift->toString();
    
    EXPECT_THAT(info, HasSubstr("FORKLIFT-001"));
    EXPECT_THAT(info, HasSubstr("Warehouse Forklift 1"));
    EXPECT_THAT(info, HasSubstr("Forklift"));
    EXPECT_THAT(info, HasSubstr("Active"));
}

class TimeUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a fixed timestamp for testing
        auto now = std::chrono::system_clock::now();
        testTimestamp = now;
    }
    
    Timestamp testTimestamp;
};

TEST_F(TimeUtilsTest, GetCurrentTimestamp) {
    auto before = std::chrono::system_clock::now();
    auto current = getCurrentTimestamp();
    auto after = std::chrono::system_clock::now();
    
    // Current timestamp should be between before and after
    EXPECT_LE(before, current);
    EXPECT_LE(current, after);
}

TEST_F(TimeUtilsTest, TimestampFormatting) {
    // Basic format test
    std::string formatted = formatTimestamp(testTimestamp, "%Y-%m-%d");
    
    // Should match YYYY-MM-DD pattern
    EXPECT_THAT(formatted, MatchesRegex("\\d{4}-\\d{2}-\\d{2}"));
    
    // Test with time components
    std::string formattedWithTime = formatTimestamp(testTimestamp, "%Y-%m-%d %H:%M:%S");
    EXPECT_THAT(formattedWithTime, MatchesRegex("\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}"));
}

TEST_F(TimeUtilsTest, TimestampParsing) {
    // Format a timestamp, then parse it back
    std::string formatted = formatTimestamp(testTimestamp, "%Y-%m-%d %H:%M:%S");
    auto parsedTimestamp = parseTimestamp(formatted, "%Y-%m-%d %H:%M:%S");
    
    // The timestamps should be very close (within 1 second due to precision loss)
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(
        testTimestamp - parsedTimestamp).count();
    EXPECT_LT(std::abs(diff), 1);
}

TEST_F(TimeUtilsTest, TimestampDifference) {
    // Create timestamps 1 hour apart
    auto laterTimestamp = addHours(testTimestamp, 1);
    
    EXPECT_EQ(3600, timestampDiffSeconds(laterTimestamp, testTimestamp));
    EXPECT_EQ(60, timestampDiffMinutes(laterTimestamp, testTimestamp));
    EXPECT_EQ(1, timestampDiffHours(laterTimestamp, testTimestamp));
    EXPECT_EQ(0, timestampDiffDays(laterTimestamp, testTimestamp));
    
    // Create timestamps 2 days apart
    auto muchLaterTimestamp = addDays(testTimestamp, 2);
    
    EXPECT_EQ(2 * 24 * 3600, timestampDiffSeconds(muchLaterTimestamp, testTimestamp));
    EXPECT_EQ(2 * 24 * 60, timestampDiffMinutes(muchLaterTimestamp, testTimestamp));
    EXPECT_EQ(2 * 24, timestampDiffHours(muchLaterTimestamp, testTimestamp));
    EXPECT_EQ(2, timestampDiffDays(muchLaterTimestamp, testTimestamp));
    
    // Negative differences (earlier - later)
    EXPECT_EQ(-3600, timestampDiffSeconds(testTimestamp, laterTimestamp));
    EXPECT_EQ(-60, timestampDiffMinutes(testTimestamp, laterTimestamp));
    EXPECT_EQ(-1, timestampDiffHours(testTimestamp, laterTimestamp));
}

TEST_F(TimeUtilsTest, TimestampAddition) {
    // Add seconds
    auto plusSeconds = addSeconds(testTimestamp, 30);
    EXPECT_EQ(30, timestampDiffSeconds(plusSeconds, testTimestamp));
    
    // Add minutes
    auto plusMinutes = addMinutes(testTimestamp, 15);
    EXPECT_EQ(15 * 60, timestampDiffSeconds(plusMinutes, testTimestamp));
    
    // Add hours
    auto plusHours = addHours(testTimestamp, 5);
    EXPECT_EQ(5 * 3600, timestampDiffSeconds(plusHours, testTimestamp));
    
    // Add days
    auto plusDays = addDays(testTimestamp, 3);
    EXPECT_EQ(3 * 24 * 3600, timestampDiffSeconds(plusDays, testTimestamp));
    
    // Negative additions
    auto minusHours = addHours(testTimestamp, -2);
    EXPECT_EQ(-2 * 3600, timestampDiffSeconds(minusHours, testTimestamp));
}
// </test_code>