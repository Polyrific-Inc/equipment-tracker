// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sstream>
#include <chrono>
#include <cmath>
#include <thread>
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/utils/constants.h"

// Mock Position class for testing
namespace equipment_tracker {

class MockPosition : public Position {
public:
    MockPosition(double lat = 0.0, double lon = 0.0, std::chrono::system_clock::time_point time = std::chrono::system_clock::now())
        : Position(lat, lon, time) {}
    
    MOCK_METHOD(double, distanceTo, (const Position&), (const, override));
    MOCK_METHOD(std::chrono::system_clock::time_point, getTimestamp, (), (const, override));
};

class EquipmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for all tests
    }
    
    void TearDown() override {
        // Common cleanup for all tests
    }
};

TEST_F(EquipmentTest, ConstructorInitializesCorrectly) {
    Equipment equipment("E123", EquipmentType::Forklift, "Test Forklift");
    
    EXPECT_EQ(equipment.getId(), "E123");
    EXPECT_EQ(equipment.getType(), EquipmentType::Forklift);
    EXPECT_EQ(equipment.getName(), "Test Forklift");
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Inactive);
    EXPECT_FALSE(equipment.getLastPosition().has_value());
    EXPECT_TRUE(equipment.getPositionHistory().empty());
}

TEST_F(EquipmentTest, MoveConstructorWorks) {
    Equipment original("E123", EquipmentType::Forklift, "Test Forklift");
    original.setStatus(EquipmentStatus::Active);
    
    Position pos(10.0, 20.0);
    original.recordPosition(pos);
    
    Equipment moved(std::move(original));
    
    EXPECT_EQ(moved.getId(), "E123");
    EXPECT_EQ(moved.getType(), EquipmentType::Forklift);
    EXPECT_EQ(moved.getName(), "Test Forklift");
    EXPECT_EQ(moved.getStatus(), EquipmentStatus::Active);
    EXPECT_TRUE(moved.getLastPosition().has_value());
    EXPECT_EQ(moved.getPositionHistory().size(), 1);
    
    // Original should be in an "unknown" state
    EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
}

TEST_F(EquipmentTest, MoveAssignmentWorks) {
    Equipment original("E123", EquipmentType::Forklift, "Test Forklift");
    original.setStatus(EquipmentStatus::Active);
    
    Position pos(10.0, 20.0);
    original.recordPosition(pos);
    
    Equipment target("E456", EquipmentType::Crane, "Test Crane");
    target = std::move(original);
    
    EXPECT_EQ(target.getId(), "E123");
    EXPECT_EQ(target.getType(), EquipmentType::Forklift);
    EXPECT_EQ(target.getName(), "Test Forklift");
    EXPECT_EQ(target.getStatus(), EquipmentStatus::Active);
    EXPECT_TRUE(target.getLastPosition().has_value());
    EXPECT_EQ(target.getPositionHistory().size(), 1);
    
    // Original should be in an "unknown" state
    EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
}

TEST_F(EquipmentTest, SelfAssignmentDoesNothing) {
    Equipment equipment("E123", EquipmentType::Forklift, "Test Forklift");
    equipment.setStatus(EquipmentStatus::Active);
    
    Position pos(10.0, 20.0);
    equipment.recordPosition(pos);
    
    // Self-assignment
    equipment = std::move(equipment);
    
    EXPECT_EQ(equipment.getId(), "E123");
    EXPECT_EQ(equipment.getType(), EquipmentType::Forklift);
    EXPECT_EQ(equipment.getName(), "Test Forklift");
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
}

TEST_F(EquipmentTest, SetAndGetStatus) {
    Equipment equipment("E123", EquipmentType::Forklift, "Test Forklift");
    
    equipment.setStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Maintenance);
    
    equipment.setStatus(EquipmentStatus::Active);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
}

TEST_F(EquipmentTest, SetAndGetName) {
    Equipment equipment("E123", EquipmentType::Forklift, "Test Forklift");
    
    equipment.setName("New Name");
    EXPECT_EQ(equipment.getName(), "New Name");
}

TEST_F(EquipmentTest, SetAndGetLastPosition) {
    Equipment equipment("E123", EquipmentType::Forklift, "Test Forklift");
    
    EXPECT_FALSE(equipment.getLastPosition().has_value());
    
    Position pos(10.0, 20.0);
    equipment.setLastPosition(pos);
    
    auto lastPos = equipment.getLastPosition();
    EXPECT_TRUE(lastPos.has_value());
    EXPECT_DOUBLE_EQ(lastPos->getLatitude(), 10.0);
    EXPECT_DOUBLE_EQ(lastPos->getLongitude(), 20.0);
}

TEST_F(EquipmentTest, RecordPositionUpdatesLastPositionAndHistory) {
    Equipment equipment("E123", EquipmentType::Forklift, "Test Forklift");
    
    Position pos1(10.0, 20.0);
    equipment.recordPosition(pos1);
    
    auto lastPos = equipment.getLastPosition();
    EXPECT_TRUE(lastPos.has_value());
    EXPECT_DOUBLE_EQ(lastPos->getLatitude(), 10.0);
    EXPECT_DOUBLE_EQ(lastPos->getLongitude(), 20.0);
    
    auto history = equipment.getPositionHistory();
    EXPECT_EQ(history.size(), 1);
    
    Position pos2(11.0, 21.0);
    equipment.recordPosition(pos2);
    
    lastPos = equipment.getLastPosition();
    EXPECT_DOUBLE_EQ(lastPos->getLatitude(), 11.0);
    EXPECT_DOUBLE_EQ(lastPos->getLongitude(), 21.0);
    
    history = equipment.getPositionHistory();
    EXPECT_EQ(history.size(), 2);
    
    // Check status is updated to Active
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
}

TEST_F(EquipmentTest, PositionHistorySizeLimit) {
    Equipment equipment("E123", EquipmentType::Forklift, "Test Forklift");
    
    // Set max history size to a small value for testing
    equipment.setMaxHistorySize(3);
    
    // Add 4 positions
    for (int i = 0; i < 4; i++) {
        Position pos(i, i);
        equipment.recordPosition(pos);
    }
    
    auto history = equipment.getPositionHistory();
    EXPECT_EQ(history.size(), 3);  // Should be limited to 3
    
    // First position should be removed, so history should start with position (1,1)
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), 1.0);
    EXPECT_DOUBLE_EQ(history[0].getLongitude(), 1.0);
}

TEST_F(EquipmentTest, ClearPositionHistory) {
    Equipment equipment("E123", EquipmentType::Forklift, "Test Forklift");
    
    Position pos1(10.0, 20.0);
    Position pos2(11.0, 21.0);
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    
    EXPECT_EQ(equipment.getPositionHistory().size(), 2);
    
    equipment.clearPositionHistory();
    
    EXPECT_EQ(equipment.getPositionHistory().size(), 0);
    // Last position should still be available
    EXPECT_TRUE(equipment.getLastPosition().has_value());
}

TEST_F(EquipmentTest, IsMovingReturnsFalseWithInsufficientData) {
    Equipment equipment("E123", EquipmentType::Forklift, "Test Forklift");
    
    // No positions
    EXPECT_FALSE(equipment.isMoving());
    
    // Only one position
    Position pos(10.0, 20.0);
    equipment.recordPosition(pos);
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, IsMovingDetectsMovement) {
    using ::testing::Return;
    using namespace std::chrono;
    
    Equipment equipment("E123", EquipmentType::Forklift, "Test Forklift");
    
    auto now = system_clock::now();
    auto later = now + seconds(10);
    
    // Create positions with known timestamps
    MockPosition pos1(10.0, 20.0, now);
    MockPosition pos2(10.1, 20.1, later);
    
    // Set up expectations for the mock
    EXPECT_CALL(pos1, getTimestamp()).WillRepeatedly(Return(now));
    EXPECT_CALL(pos2, getTimestamp()).WillRepeatedly(Return(later));
    
    // Set up distance calculation to return 100 meters
    // This should result in a speed of 10 m/s (100m / 10s)
    EXPECT_CALL(pos2, distanceTo(::testing::_)).WillRepeatedly(Return(100.0));
    
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    
    // If MOVEMENT_SPEED_THRESHOLD is less than 10 m/s, isMoving should return true
    EXPECT_TRUE(equipment.isMoving());
}

TEST_F(EquipmentTest, IsMovingHandlesSmallTimeDifference) {
    using ::testing::Return;
    using namespace std::chrono;
    
    Equipment equipment("E123", EquipmentType::Forklift, "Test Forklift");
    
    auto now = system_clock::now();
    auto almostNow = now + milliseconds(100);  // Only 0.1 seconds later
    
    MockPosition pos1(10.0, 20.0, now);
    MockPosition pos2(10.1, 20.1, almostNow);
    
    EXPECT_CALL(pos1, getTimestamp()).WillRepeatedly(Return(now));
    EXPECT_CALL(pos2, getTimestamp()).WillRepeatedly(Return(almostNow));
    
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    
    // Time difference is less than 1 second, should return false to avoid division by zero
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, ToStringFormatsCorrectly) {
    Equipment equipment("E123", EquipmentType::Forklift, "Test Forklift");
    
    std::string result = equipment.toString();
    
    EXPECT_THAT(result, ::testing::HasSubstr("Equipment(id=E123"));
    EXPECT_THAT(result, ::testing::HasSubstr("name=Test Forklift"));
    EXPECT_THAT(result, ::testing::HasSubstr("type=Forklift"));
    EXPECT_THAT(result, ::testing::HasSubstr("status=Inactive"));
    
    equipment.setStatus(EquipmentStatus::Maintenance);
    result = equipment.toString();
    EXPECT_THAT(result, ::testing::HasSubstr("status=Maintenance"));
    
    Equipment crane("C456", EquipmentType::Crane, "Test Crane");
    result = crane.toString();
    EXPECT_THAT(result, ::testing::HasSubstr("type=Crane"));
    
    Equipment bulldozer("B789", EquipmentType::Bulldozer, "Test Bulldozer");
    result = bulldozer.toString();
    EXPECT_THAT(result, ::testing::HasSubstr("type=Bulldozer"));
    
    Equipment excavator("E101", EquipmentType::Excavator, "Test Excavator");
    result = excavator.toString();
    EXPECT_THAT(result, ::testing::HasSubstr("type=Excavator"));
    
    Equipment truck("T202", EquipmentType::Truck, "Test Truck");
    result = truck.toString();
    EXPECT_THAT(result, ::testing::HasSubstr("type=Truck"));
    
    Equipment other("O303", static_cast<EquipmentType>(99), "Test Other");
    result = other.toString();
    EXPECT_THAT(result, ::testing::HasSubstr("type=Other"));
    
    other.setStatus(static_cast<EquipmentStatus>(99));
    result = other.toString();
    EXPECT_THAT(result, ::testing::HasSubstr("status=Unknown"));
}

} // namespace equipment_tracker
// </test_code>