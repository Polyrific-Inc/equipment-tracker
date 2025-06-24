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

// Platform-specific definitions for cross-platform compatibility
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
    EXPECT_LE(std::chrono::system_clock::now() - position.getTimestamp(), std::chrono::seconds(1));
}

TEST_F(PositionTest, ParameterizedConstructor) {
    double lat = 37.7749;
    double lon = -122.4194;
    double alt = 10.0;
    double acc = 2.0;
    auto now = getCurrentTimestamp();
    
    Position position(lat, lon, alt, acc, now);
    
    EXPECT_DOUBLE_EQ(lat, position.getLatitude());
    EXPECT_DOUBLE_EQ(lon, position.getLongitude());
    EXPECT_DOUBLE_EQ(alt, position.getAltitude());
    EXPECT_DOUBLE_EQ(acc, position.getAccuracy());
    EXPECT_EQ(now, position.getTimestamp());
}

TEST_F(PositionTest, BuilderPattern) {
    auto now = getCurrentTimestamp();
    Position position = Position::builder()
                            .withLatitude(34.0522)
                            .withLongitude(-118.2437)
                            .withAltitude(50.0)
                            .withAccuracy(1.5)
                            .withTimestamp(now)
                            .build();
    
    EXPECT_DOUBLE_EQ(34.0522, position.getLatitude());
    EXPECT_DOUBLE_EQ(-118.2437, position.getLongitude());
    EXPECT_DOUBLE_EQ(50.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(1.5, position.getAccuracy());
    EXPECT_EQ(now, position.getTimestamp());
}

TEST_F(PositionTest, Setters) {
    Position position;
    
    position.setLatitude(37.7749);
    position.setLongitude(-122.4194);
    position.setAltitude(10.0);
    position.setAccuracy(2.0);
    auto now = getCurrentTimestamp();
    position.setTimestamp(now);
    
    EXPECT_DOUBLE_EQ(37.7749, position.getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194, position.getLongitude());
    EXPECT_DOUBLE_EQ(10.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(2.0, position.getAccuracy());
    EXPECT_EQ(now, position.getTimestamp());
}

TEST_F(PositionTest, DistanceCalculation) {
    // The distance between San Francisco and Los Angeles is approximately 559 km
    // We'll allow for some floating point imprecision with a tolerance of 1 km
    double distanceInMeters = sanFrancisco.distanceTo(losAngeles);
    EXPECT_NEAR(559000.0, distanceInMeters, 1000.0);
    
    // Distance should be the same in reverse
    double reverseDistance = losAngeles.distanceTo(sanFrancisco);
    EXPECT_DOUBLE_EQ(distanceInMeters, reverseDistance);
    
    // Distance to self should be 0
    EXPECT_DOUBLE_EQ(0.0, sanFrancisco.distanceTo(sanFrancisco));
}

TEST_F(PositionTest, ToStringOutput) {
    // Test that toString contains the expected information
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
    EXPECT_EQ(EquipmentStatus::Active, forklift->getStatus()); // Default status should be Active
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

TEST_F(EquipmentTest, CopyAssignment) {
    forklift->setLastPosition(sanFrancisco);
    Equipment anotherForklift("FORKLIFT-002", EquipmentType::Forklift, "Another Forklift");
    
    anotherForklift = *forklift;
    
    EXPECT_EQ(forklift->getId(), anotherForklift.getId());
    EXPECT_EQ(forklift->getType(), anotherForklift.getType());
    EXPECT_EQ(forklift->getName(), anotherForklift.getName());
    
    auto originalPos = forklift->getLastPosition();
    auto copiedPos = anotherForklift.getLastPosition();
    
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

TEST_F(EquipmentTest, MoveAssignment) {
    forklift->setLastPosition(sanFrancisco);
    std::string originalId = forklift->getId();
    EquipmentType originalType = forklift->getType();
    std::string originalName = forklift->getName();
    
    Equipment anotherForklift("FORKLIFT-002", EquipmentType::Forklift, "Another Forklift");
    anotherForklift = std::move(*forklift);
    
    EXPECT_EQ(originalId, anotherForklift.getId());
    EXPECT_EQ(originalType, anotherForklift.getType());
    EXPECT_EQ(originalName, anotherForklift.getName());
    
    auto movedPos = anotherForklift.getLastPosition();
    ASSERT_TRUE(movedPos.has_value());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLatitude(), movedPos->getLatitude());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLongitude(), movedPos->getLongitude());
}

TEST_F(EquipmentTest, Setters) {
    forklift->setName("New Forklift Name");
    forklift->setStatus(EquipmentStatus::Maintenance);
    forklift->setLastPosition(sanFrancisco);
    
    EXPECT_EQ("New Forklift Name", forklift->getName());
    EXPECT_EQ(EquipmentStatus::Maintenance, forklift->getStatus());
    
    auto pos = forklift->getLastPosition();
    ASSERT_TRUE(pos.has_value());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLatitude(), pos->getLatitude());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLongitude(), pos->getLongitude());
}

TEST_F(EquipmentTest, PositionHistory) {
    // Initially, history should be empty
    EXPECT_TRUE(forklift->getPositionHistory().empty());
    
    // Record positions
    forklift->recordPosition(sanFrancisco);
    forklift->recordPosition(losAngeles);
    
    // Check history size
    auto history = forklift->getPositionHistory();
    EXPECT_EQ(2, history.size());
    
    // Check history contents
    EXPECT_DOUBLE_EQ(sanFrancisco.getLatitude(), history[0].getLatitude());
    EXPECT_DOUBLE_EQ(sanFrancisco.getLongitude(), history[0].getLongitude());
    EXPECT_DOUBLE_EQ(losAngeles.getLatitude(), history[1].getLatitude());
    EXPECT_DOUBLE_EQ(losAngeles.getLongitude(), history[1].getLongitude());
    
    // Clear history
    forklift->clearPositionHistory();
    EXPECT_TRUE(forklift->getPositionHistory().empty());
}

TEST_F(EquipmentTest, HistorySizeLimit) {
    // Record more positions than the default history size limit
    for (size_t i = 0; i < DEFAULT_MAX_HISTORY_SIZE + 10; ++i) {
        Position pos(37.7749 + i * 0.001, -122.4194 + i * 0.001);
        forklift->recordPosition(pos);
    }
    
    // History size should be limited to DEFAULT_MAX_HISTORY_SIZE
    auto history = forklift->getPositionHistory();
    EXPECT_EQ(DEFAULT_MAX_HISTORY_SIZE, history.size());
}

TEST_F(EquipmentTest, IsMoving) {
    // Initially, equipment should not be moving (no positions)
    EXPECT_FALSE(forklift->isMoving());
    
    // Record a single position - still not moving
    forklift->recordPosition(sanFrancisco);
    EXPECT_FALSE(forklift->isMoving());
    
    // Record a position very close to the first one - should not be moving
    Position nearbyPosition(37.7749 + 0.0001, -122.4194 + 0.0001);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    forklift->recordPosition(nearbyPosition);
    EXPECT_FALSE(forklift->isMoving());
    
    // Record a position far away - should be moving
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    forklift->recordPosition(losAngeles);
    EXPECT_TRUE(forklift->isMoving());
}

TEST_F(EquipmentTest, ToString) {
    forklift->setLastPosition(sanFrancisco);
    std::string equipStr = forklift->toString();
    
    EXPECT_THAT(equipStr, HasSubstr("FORKLIFT-001"));
    EXPECT_THAT(equipStr, HasSubstr("Warehouse Forklift 1"));
    EXPECT_THAT(equipStr, HasSubstr("Forklift"));
}

class TimeUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        testTime = std::chrono::system_clock::now();
    }

    Timestamp testTime;
};

TEST_F(TimeUtilsTest, GetCurrentTimestamp) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = getCurrentTimestamp();
    
    // The timestamps should be very close (within 1 second)
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(
        now - timestamp).count();
    EXPECT_LE(std::abs(diff), 1);
}

TEST_F(TimeUtilsTest, TimestampDiffSeconds) {
    auto later = addSeconds(testTime, 120);
    EXPECT_EQ(120, timestampDiffSeconds(later, testTime));
    EXPECT_EQ(-120, timestampDiffSeconds(testTime, later));
}

TEST_F(TimeUtilsTest, TimestampDiffMinutes) {
    auto later = addMinutes(testTime, 60);
    EXPECT_EQ(60, timestampDiffMinutes(later, testTime));
    EXPECT_EQ(-60, timestampDiffMinutes(testTime, later));
}

TEST_F(TimeUtilsTest, TimestampDiffHours) {
    auto later = addHours(testTime, 5);
    EXPECT_EQ(5, timestampDiffHours(later, testTime));
    EXPECT_EQ(-5, timestampDiffHours(testTime, later));
}

TEST_F(TimeUtilsTest, TimestampDiffDays) {
    auto later = addDays(testTime, 7);
    EXPECT_EQ(7, timestampDiffDays(later, testTime));
    EXPECT_EQ(-7, timestampDiffDays(testTime, later));
}

TEST_F(TimeUtilsTest, AddSeconds) {
    auto later = addSeconds(testTime, 30);
    EXPECT_EQ(30, timestampDiffSeconds(later, testTime));
}

TEST_F(TimeUtilsTest, AddMinutes) {
    auto later = addMinutes(testTime, 15);
    EXPECT_EQ(15 * 60, timestampDiffSeconds(later, testTime));
}

TEST_F(TimeUtilsTest, AddHours) {
    auto later = addHours(testTime, 2);
    EXPECT_EQ(2 * 60 * 60, timestampDiffSeconds(later, testTime));
}

TEST_F(TimeUtilsTest, AddDays) {
    auto later = addDays(testTime, 3);
    EXPECT_EQ(3 * 24 * 60 * 60, timestampDiffSeconds(later, testTime));
}

TEST_F(TimeUtilsTest, FormatAndParseTimestamp) {
    // This test is platform-dependent, so we'll use a simple format
    std::string format = "%Y-%m-%d %H:%M:%S";
    std::string formatted = formatTimestamp(testTime, format);
    
    // Parse the formatted string back to a timestamp
    auto parsedTime = parseTimestamp(formatted, format);
    
    // The timestamps should be very close (within 1 second due to truncation)
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(
        testTime - parsedTime).count();
    EXPECT_LE(std::abs(diff), 1);
}
// </test_code>