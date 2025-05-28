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
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    EXPECT_EQ(equipment.getId(), "EQ001");
    EXPECT_EQ(equipment.getType(), EquipmentType::Forklift);
    EXPECT_EQ(equipment.getName(), "Test Forklift");
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Inactive);
    EXPECT_FALSE(equipment.getLastPosition().has_value());
    EXPECT_TRUE(equipment.getPositionHistory().empty());
}

TEST_F(EquipmentTest, MoveConstructorWorksCorrectly) {
    Equipment original("EQ001", EquipmentType::Forklift, "Test Forklift");
    original.setStatus(EquipmentStatus::Active);
    
    Position pos(10.0, 20.0);
    original.recordPosition(pos);
    
    Equipment moved(std::move(original));
    
    EXPECT_EQ(moved.getId(), "EQ001");
    EXPECT_EQ(moved.getType(), EquipmentType::Forklift);
    EXPECT_EQ(moved.getName(), "Test Forklift");
    EXPECT_EQ(moved.getStatus(), EquipmentStatus::Active);
    EXPECT_TRUE(moved.getLastPosition().has_value());
    EXPECT_EQ(moved.getPositionHistory().size(), 1);
    
    // Original should be in an unknown state
    EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
}

TEST_F(EquipmentTest, MoveAssignmentWorksCorrectly) {
    Equipment original("EQ001", EquipmentType::Forklift, "Test Forklift");
    original.setStatus(EquipmentStatus::Active);
    
    Position pos(10.0, 20.0);
    original.recordPosition(pos);
    
    Equipment target("EQ002", EquipmentType::Crane, "Test Crane");
    target = std::move(original);
    
    EXPECT_EQ(target.getId(), "EQ001");
    EXPECT_EQ(target.getType(), EquipmentType::Forklift);
    EXPECT_EQ(target.getName(), "Test Forklift");
    EXPECT_EQ(target.getStatus(), EquipmentStatus::Active);
    EXPECT_TRUE(target.getLastPosition().has_value());
    EXPECT_EQ(target.getPositionHistory().size(), 1);
    
    // Original should be in an unknown state
    EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
}

TEST_F(EquipmentTest, SelfMoveAssignmentDoesNothing) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    equipment.setStatus(EquipmentStatus::Active);
    
    Position pos(10.0, 20.0);
    equipment.recordPosition(pos);
    
    equipment = std::move(equipment); // Self-assignment
    
    EXPECT_EQ(equipment.getId(), "EQ001");
    EXPECT_EQ(equipment.getType(), EquipmentType::Forklift);
    EXPECT_EQ(equipment.getName(), "Test Forklift");
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
    EXPECT_TRUE(equipment.getLastPosition().has_value());
    EXPECT_EQ(equipment.getPositionHistory().size(), 1);
}

TEST_F(EquipmentTest, GetLastPositionReturnsCorrectValue) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Initially, last position should be empty
    EXPECT_FALSE(equipment.getLastPosition().has_value());
    
    // Set a position
    Position pos(10.0, 20.0);
    equipment.setLastPosition(pos);
    
    // Now last position should be set
    auto lastPos = equipment.getLastPosition();
    ASSERT_TRUE(lastPos.has_value());
    EXPECT_DOUBLE_EQ(lastPos->getLatitude(), 10.0);
    EXPECT_DOUBLE_EQ(lastPos->getLongitude(), 20.0);
}

TEST_F(EquipmentTest, SetStatusChangesStatus) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Inactive);
    
    equipment.setStatus(EquipmentStatus::Active);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
    
    equipment.setStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Maintenance);
}

TEST_F(EquipmentTest, SetNameChangesName) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    EXPECT_EQ(equipment.getName(), "Test Forklift");
    
    equipment.setName("New Name");
    EXPECT_EQ(equipment.getName(), "New Name");
}

TEST_F(EquipmentTest, RecordPositionUpdatesHistoryAndStatus) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Initially, history should be empty
    EXPECT_TRUE(equipment.getPositionHistory().empty());
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Inactive);
    
    // Record a position
    Position pos1(10.0, 20.0);
    equipment.recordPosition(pos1);
    
    // Check history and status
    auto history = equipment.getPositionHistory();
    ASSERT_EQ(history.size(), 1);
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), 10.0);
    EXPECT_DOUBLE_EQ(history[0].getLongitude(), 20.0);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
    
    // Record another position
    Position pos2(11.0, 21.0);
    equipment.recordPosition(pos2);
    
    // Check updated history
    history = equipment.getPositionHistory();
    ASSERT_EQ(history.size(), 2);
    EXPECT_DOUBLE_EQ(history[1].getLatitude(), 11.0);
    EXPECT_DOUBLE_EQ(history[1].getLongitude(), 21.0);
}

TEST_F(EquipmentTest, PositionHistorySizeIsLimited) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Set max history size to a small value for testing
    equipment.setMaxHistorySize(3);
    
    // Record more positions than the max size
    for (int i = 0; i < 5; i++) {
        Position pos(i, i);
        equipment.recordPosition(pos);
    }
    
    // Check that history size is limited
    auto history = equipment.getPositionHistory();
    ASSERT_EQ(history.size(), 3);
    
    // Check that oldest positions were removed
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), 2.0);
    EXPECT_DOUBLE_EQ(history[1].getLatitude(), 3.0);
    EXPECT_DOUBLE_EQ(history[2].getLatitude(), 4.0);
}

TEST_F(EquipmentTest, ClearPositionHistoryWorks) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Add some positions
    for (int i = 0; i < 3; i++) {
        Position pos(i, i);
        equipment.recordPosition(pos);
    }
    
    EXPECT_EQ(equipment.getPositionHistory().size(), 3);
    
    // Clear history
    equipment.clearPositionHistory();
    
    // Check that history is empty
    EXPECT_TRUE(equipment.getPositionHistory().empty());
    
    // Last position should still be available
    EXPECT_TRUE(equipment.getLastPosition().has_value());
}

TEST_F(EquipmentTest, IsMovingReturnsFalseWithInsufficientData) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
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
    
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Create positions with specific timestamps
    auto now = system_clock::now();
    MockPosition pos1(10.0, 20.0, now - seconds(10));
    MockPosition pos2(11.0, 21.0, now);
    
    // Set up mock expectations
    EXPECT_CALL(pos1, getTimestamp()).WillRepeatedly(Return(now - seconds(10)));
    EXPECT_CALL(pos2, getTimestamp()).WillRepeatedly(Return(now));
    
    // Distance that would indicate movement (greater than threshold)
    // Assuming MOVEMENT_SPEED_THRESHOLD is defined in constants.h
    double distance = MOVEMENT_SPEED_THRESHOLD * 10 + 1.0; // 10 seconds * threshold + buffer
    EXPECT_CALL(pos2, distanceTo(testing::_)).WillRepeatedly(Return(distance));
    
    // Record positions
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    
    // Should detect movement
    EXPECT_TRUE(equipment.isMoving());
}

TEST_F(EquipmentTest, IsMovingReturnsFalseForStationaryEquipment) {
    using ::testing::Return;
    using namespace std::chrono;
    
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Create positions with specific timestamps
    auto now = system_clock::now();
    MockPosition pos1(10.0, 20.0, now - seconds(10));
    MockPosition pos2(10.001, 20.001, now);
    
    // Set up mock expectations
    EXPECT_CALL(pos1, getTimestamp()).WillRepeatedly(Return(now - seconds(10)));
    EXPECT_CALL(pos2, getTimestamp()).WillRepeatedly(Return(now));
    
    // Distance that would indicate no movement (less than threshold)
    double distance = MOVEMENT_SPEED_THRESHOLD * 10 - 1.0; // 10 seconds * threshold - buffer
    EXPECT_CALL(pos2, distanceTo(testing::_)).WillRepeatedly(Return(distance));
    
    // Record positions
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    
    // Should not detect movement
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, IsMovingHandlesSmallTimeDifference) {
    using ::testing::Return;
    using namespace std::chrono;
    
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Create positions with very close timestamps
    auto now = system_clock::now();
    MockPosition pos1(10.0, 20.0, now);
    MockPosition pos2(11.0, 21.0, now + milliseconds(100)); // Only 100ms difference
    
    // Set up mock expectations
    EXPECT_CALL(pos1, getTimestamp()).WillRepeatedly(Return(now));
    EXPECT_CALL(pos2, getTimestamp()).WillRepeatedly(Return(now + milliseconds(100)));
    
    // Record positions
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    
    // Should not detect movement due to small time difference
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, ToStringReturnsCorrectFormat) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    equipment.setStatus(EquipmentStatus::Active);
    
    std::string result = equipment.toString();
    
    EXPECT_THAT(result, ::testing::HasSubstr("Equipment(id=EQ001"));
    EXPECT_THAT(result, ::testing::HasSubstr("name=Test Forklift"));
    EXPECT_THAT(result, ::testing::HasSubstr("type=Forklift"));
    EXPECT_THAT(result, ::testing::HasSubstr("status=Active"));
}

TEST_F(EquipmentTest, ToStringHandlesAllEquipmentTypes) {
    Equipment forklift("EQ001", EquipmentType::Forklift, "Test");
    Equipment crane("EQ002", EquipmentType::Crane, "Test");
    Equipment bulldozer("EQ003", EquipmentType::Bulldozer, "Test");
    Equipment excavator("EQ004", EquipmentType::Excavator, "Test");
    Equipment truck("EQ005", EquipmentType::Truck, "Test");
    Equipment other("EQ006", static_cast<EquipmentType>(99), "Test"); // Unknown type
    
    EXPECT_THAT(forklift.toString(), ::testing::HasSubstr("type=Forklift"));
    EXPECT_THAT(crane.toString(), ::testing::HasSubstr("type=Crane"));
    EXPECT_THAT(bulldozer.toString(), ::testing::HasSubstr("type=Bulldozer"));
    EXPECT_THAT(excavator.toString(), ::testing::HasSubstr("type=Excavator"));
    EXPECT_THAT(truck.toString(), ::testing::HasSubstr("type=Truck"));
    EXPECT_THAT(other.toString(), ::testing::HasSubstr("type=Other"));
}

TEST_F(EquipmentTest, ToStringHandlesAllStatusTypes) {
    Equipment active("EQ001", EquipmentType::Forklift, "Test");
    Equipment inactive("EQ002", EquipmentType::Forklift, "Test");
    Equipment maintenance("EQ003", EquipmentType::Forklift, "Test");
    Equipment unknown("EQ004", EquipmentType::Forklift, "Test");
    
    active.setStatus(EquipmentStatus::Active);
    inactive.setStatus(EquipmentStatus::Inactive);
    maintenance.setStatus(EquipmentStatus::Maintenance);
    unknown.setStatus(static_cast<EquipmentStatus>(99)); // Unknown status
    
    EXPECT_THAT(active.toString(), ::testing::HasSubstr("status=Active"));
    EXPECT_THAT(inactive.toString(), ::testing::HasSubstr("status=Inactive"));
    EXPECT_THAT(maintenance.toString(), ::testing::HasSubstr("status=Maintenance"));
    EXPECT_THAT(unknown.toString(), ::testing::HasSubstr("status=Unknown"));
}

} // namespace equipment_tracker
// </test_code>