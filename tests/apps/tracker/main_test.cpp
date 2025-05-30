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


// Mock for getCurrentTimestamp to make tests deterministic
class TimeUtilsMock {
public:
    static Timestamp mockCurrentTime;
    
    static void setMockTime(const Timestamp& time) {
        mockCurrentTime = time;
    }
    
    static Timestamp advanceTimeBy(std::chrono::milliseconds duration) {
        mockCurrentTime += duration;
        return mockCurrentTime;
    }
};

Timestamp TimeUtilsMock::mockCurrentTime = std::chrono::system_clock::now();

// Position Tests
class PositionTest : public Test {
protected:
    void SetUp() override {
        // Set a fixed timestamp for tests
        TimeUtilsMock::setMockTime(std::chrono::system_clock::now());
    }
};

TEST_F(PositionTest, DefaultConstructor) {

    Position position;
    EXPECT_DOUBLE_EQ(0.0, position.getLatitude());
    EXPECT_DOUBLE_EQ(0.0, position.getLongitude());
    EXPECT_DOUBLE_EQ(0.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(DEFAULT_POSITION_ACCURACY, position.getAccuracy());
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

TEST_F(PositionTest, Setters) {
    Position position;
    
    position.setLatitude(40.7128);
    position.setLongitude(-74.0060);
    position.setAltitude(15.0);
    position.setAccuracy(3.0);
    
    EXPECT_DOUBLE_EQ(40.7128, position.getLatitude());
    EXPECT_DOUBLE_EQ(-74.0060, position.getLongitude());
    EXPECT_DOUBLE_EQ(15.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(3.0, position.getAccuracy());
}

TEST_F(PositionTest, DistanceCalculation) {
    // San Francisco
    Position sf(37.7749, -122.4194, 10.0);
    // Los Angeles
    Position la(34.0522, -118.2437, 50.0);
    
    // Expected distance is approximately 559.12 km (347.42 miles)
    double distance = sf.distanceTo(la);
    
    // Use a reasonable tolerance for floating point comparison
    EXPECT_NEAR(559120.0, distance, 1000.0); // Within 1km of expected
}

TEST_F(PositionTest, DistanceToSamePosition) {
    Position position(37.7749, -122.4194, 10.0);
    double distance = position.distanceTo(position);
    EXPECT_DOUBLE_EQ(0.0, distance);
}

TEST_F(PositionTest, ToStringContainsCoordinates) {
    Position position(37.7749, -122.4194, 10.0);
    std::string posStr = position.toString();
    
    EXPECT_THAT(posStr, HasSubstr("37.7749"));
    EXPECT_THAT(posStr, HasSubstr("-122.4194"));
    EXPECT_THAT(posStr, HasSubstr("10"));
}

// Equipment Tests
class EquipmentTest : public Test {
protected:
    void SetUp() override {
        // Set a fixed timestamp for tests
        TimeUtilsMock::setMockTime(std::chrono::system_clock::now());
    }
    
    // Helper to create a standard equipment instance
    std::unique_ptr<Equipment> createTestEquipment() {
        return std::make_unique<Equipment>("TEST-001", EquipmentType::Forklift, "Test Forklift");
    }
    
    // Helper to create a position
    Position createTestPosition(double lat = 37.7749, double lon = -122.4194, double alt = 10.0) {
        return Position(lat, lon, alt);
    }
};

TEST_F(EquipmentTest, Constructor) {
    Equipment equipment("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    EXPECT_EQ("FORKLIFT-001", equipment.getId());
    EXPECT_EQ(EquipmentType::Forklift, equipment.getType());
    EXPECT_EQ("Warehouse Forklift 1", equipment.getName());
    EXPECT_EQ(EquipmentStatus::Active, equipment.getStatus()); // Default status
    EXPECT_FALSE(equipment.getLastPosition().has_value()); // No position by default
}

TEST_F(EquipmentTest, MoveConstructor) {
    Equipment original("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    original.setStatus(EquipmentStatus::Maintenance);
    
    // Set a position
    Position pos = createTestPosition();
    original.setLastPosition(pos);
    
    // Move construct
    Equipment moved(std::move(original));
    
    EXPECT_EQ("FORKLIFT-001", moved.getId());
    EXPECT_EQ(EquipmentType::Forklift, moved.getType());
    EXPECT_EQ("Warehouse Forklift 1", moved.getName());
    EXPECT_EQ(EquipmentStatus::Maintenance, moved.getStatus());
    
    // Check position was moved
    ASSERT_TRUE(moved.getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(37.7749, moved.getLastPosition()->getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194, moved.getLastPosition()->getLongitude());
}

TEST_F(EquipmentTest, MoveAssignment) {
    Equipment original("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    Equipment target("CRANE-001", EquipmentType::Crane, "Tower Crane 1");
    
    // Set a position
    Position pos = createTestPosition();
    original.setLastPosition(pos);
    
    // Move assign
    target = std::move(original);
    
    EXPECT_EQ("FORKLIFT-001", target.getId());
    EXPECT_EQ(EquipmentType::Forklift, target.getType());
    EXPECT_EQ("Warehouse Forklift 1", target.getName());
    
    // Check position was moved
    ASSERT_TRUE(target.getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(37.7749, target.getLastPosition()->getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194, target.getLastPosition()->getLongitude());
}

TEST_F(EquipmentTest, SettersAndGetters) {
    auto equipment = createTestEquipment();
    
    equipment->setName("Updated Forklift");
    equipment->setStatus(EquipmentStatus::Maintenance);
    
    EXPECT_EQ("Updated Forklift", equipment->getName());
    EXPECT_EQ(EquipmentStatus::Maintenance, equipment->getStatus());
}

TEST_F(EquipmentTest, PositionManagement) {
    auto equipment = createTestEquipment();
    
    // Initially no position
    EXPECT_FALSE(equipment->getLastPosition().has_value());
    
    // Set position
    Position pos1 = createTestPosition();
    equipment->setLastPosition(pos1);
    
    // Check position was set
    ASSERT_TRUE(equipment->getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(37.7749, equipment->getLastPosition()->getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194, equipment->getLastPosition()->getLongitude());
    
    // Update position
    Position pos2 = createTestPosition(37.8, -122.5, 15.0);
    equipment->setLastPosition(pos2);
    
    // Check position was updated
    ASSERT_TRUE(equipment->getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(37.8, equipment->getLastPosition()->getLatitude());
    EXPECT_DOUBLE_EQ(-122.5, equipment->getLastPosition()->getLongitude());
    EXPECT_DOUBLE_EQ(15.0, equipment->getLastPosition()->getAltitude());
}

TEST_F(EquipmentTest, PositionHistory) {
    auto equipment = createTestEquipment();
    
    // Initially empty history
    EXPECT_TRUE(equipment->getPositionHistory().empty());
    
    // Record positions
    for (int i = 0; i < 5; ++i) {
        Position pos = createTestPosition(37.7749 + i * 0.001, -122.4194 + i * 0.002, 10.0 + i);
        equipment->recordPosition(pos);
        
        // Advance time for each position
        TimeUtilsMock::advanceTimeBy(std::chrono::seconds(1));
    }
    
    // Check history size
    auto history = equipment->getPositionHistory();
    EXPECT_EQ(5, history.size());
    
    // Check positions are in correct order (newest first)
    EXPECT_DOUBLE_EQ(37.7749 + 4 * 0.001, history[0].getLatitude());
    EXPECT_DOUBLE_EQ(37.7749 + 3 * 0.001, history[1].getLatitude());
    EXPECT_DOUBLE_EQ(37.7749, history[4].getLatitude());
    
    // Clear history
    equipment->clearPositionHistory();
    EXPECT_TRUE(equipment->getPositionHistory().empty());
}

TEST_F(EquipmentTest, HistorySizeLimit) {
    auto equipment = createTestEquipment();
    
    // Record more positions than the default history size
    for (size_t i = 0; i < DEFAULT_MAX_HISTORY_SIZE + 10; ++i) {
        Position pos = createTestPosition(37.7749 + i * 0.001, -122.4194 + i * 0.002, 10.0 + i);
        equipment->recordPosition(pos);
        
        // Advance time for each position
        TimeUtilsMock::advanceTimeBy(std::chrono::seconds(1));
    }
    
    // Check history size is limited to DEFAULT_MAX_HISTORY_SIZE
    auto history = equipment->getPositionHistory();
    EXPECT_EQ(DEFAULT_MAX_HISTORY_SIZE, history.size());
    
    // Check the oldest entries were removed
    // The newest entry should have the highest latitude offset
    double expectedLatitude = 37.7749 + (DEFAULT_MAX_HISTORY_SIZE + 10 - 1) * 0.001;
    EXPECT_DOUBLE_EQ(expectedLatitude, history[0].getLatitude());
}

TEST_F(EquipmentTest, IsMoving) {
    auto equipment = createTestEquipment();
    
    // No position yet, should not be moving
    EXPECT_FALSE(equipment->isMoving());
    
    // Record first position
    Position pos1 = createTestPosition();
    equipment->recordPosition(pos1);
    
    // Only one position, should not be moving
    EXPECT_FALSE(equipment->isMoving());
    
    // Record second position with significant movement
    TimeUtilsMock::advanceTimeBy(std::chrono::seconds(10));
    Position pos2 = createTestPosition(37.7749 + 0.01, -122.4194 + 0.01, 10.0);
    equipment->recordPosition(pos2);
    
    // Should be moving (positions are far enough apart)
    EXPECT_TRUE(equipment->isMoving());
    
    // Record third position with minimal movement
    TimeUtilsMock::advanceTimeBy(std::chrono::seconds(10));
    Position pos3 = createTestPosition(37.7749 + 0.01, -122.4194 + 0.01, 10.0);
    equipment->recordPosition(pos3);
    
    // Should not be moving (positions are too close)
    EXPECT_FALSE(equipment->isMoving());
}

TEST_F(EquipmentTest, ToString) {
    Equipment equipment("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    std::string equipStr = equipment.toString();
    
    EXPECT_THAT(equipStr, HasSubstr("FORKLIFT-001"));
    EXPECT_THAT(equipStr, HasSubstr("Warehouse Forklift 1"));
    EXPECT_THAT(equipStr, HasSubstr("Forklift"));
}

// Time Utils Tests
class TimeUtilsTest : public Test {
protected:
    void SetUp() override {
        // Set a fixed timestamp for tests
        TimeUtilsMock::setMockTime(std::chrono::system_clock::now());
    }
};

TEST_F(TimeUtilsTest, FormatTimestamp) {
    auto now = getCurrentTimestamp();
    std::string formatted = formatTimestamp(now, "%Y-%m-%d");
    
    // Check format is correct (YYYY-MM-DD)
    EXPECT_THAT(formatted, MatchesRegex("[0-9]{4}-[0-9]{2}-[0-9]{2}"));
}

TEST_F(TimeUtilsTest, ParseTimestamp) {
    std::string dateStr = "2023-05-15 14:30:00";
    auto timestamp = parseTimestamp(dateStr, "%Y-%m-%d %H:%M:%S");
    
    // Format it back to verify
    std::string formatted = formatTimestamp(timestamp, "%Y-%m-%d %H:%M:%S");
    EXPECT_EQ(dateStr, formatted);
}

TEST_F(TimeUtilsTest, TimestampDifference) {
    auto now = getCurrentTimestamp();
    auto later = addSeconds(now, 3665); // 1 hour, 1 minute, 5 seconds
    
    EXPECT_EQ(3665, timestampDiffSeconds(later, now));
    EXPECT_EQ(61, timestampDiffMinutes(later, now));
    EXPECT_EQ(1, timestampDiffHours(later, now));
    EXPECT_EQ(0, timestampDiffDays(later, now));
    
    auto muchLater = addDays(now, 2);
    EXPECT_EQ(2, timestampDiffDays(muchLater, now));
}

TEST_F(TimeUtilsTest, AddTime) {
    auto now = getCurrentTimestamp();
    
    auto later1 = addSeconds(now, 30);
    EXPECT_EQ(30, timestampDiffSeconds(later1, now));
    
    auto later2 = addMinutes(now, 45);
    EXPECT_EQ(45, timestampDiffMinutes(later2, now));
    
    auto later3 = addHours(now, 3);
    EXPECT_EQ(3, timestampDiffHours(later3, now));
    
    auto later4 = addDays(now, 7);
    EXPECT_EQ(7, timestampDiffDays(later4, now));
}
// </test_code>