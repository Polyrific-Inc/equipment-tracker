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
    MockPosition(double lat = 0.0, double lon = 0.0) {
        latitude_ = lat;
        longitude_ = lon;
        timestamp_ = std::chrono::system_clock::now();
    }
    
    MockPosition(double lat, double lon, std::chrono::system_clock::time_point time) {
        latitude_ = lat;
        longitude_ = lon;
        timestamp_ = time;
    }
    
    double distanceTo(const Position& other) const override {
        // Simple mock implementation for testing
        return 100.0; // Return fixed distance for predictable testing
    }
    
    std::chrono::system_clock::time_point getTimestamp() const {
        return timestamp_;
    }
    
private:
    double latitude_;
    double longitude_;
    std::chrono::system_clock::time_point timestamp_;
};

class EquipmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up common test data
    }
    
    void TearDown() override {
        // Clean up after each test
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

TEST_F(EquipmentTest, MoveConstructorWorks) {
    Equipment original("EQ001", EquipmentType::Forklift, "Test Forklift");
    original.setStatus(EquipmentStatus::Active);
    
    MockPosition pos(10.0, 20.0);
    original.recordPosition(pos);
    
    // Move construct
    Equipment moved(std::move(original));
    
    // Check moved-to object
    EXPECT_EQ(moved.getId(), "EQ001");
    EXPECT_EQ(moved.getType(), EquipmentType::Forklift);
    EXPECT_EQ(moved.getName(), "Test Forklift");
    EXPECT_EQ(moved.getStatus(), EquipmentStatus::Active);
    EXPECT_TRUE(moved.getLastPosition().has_value());
    EXPECT_EQ(moved.getPositionHistory().size(), 1);
    
    // Check moved-from object (should be in a valid but unspecified state)
    EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
}

TEST_F(EquipmentTest, MoveAssignmentWorks) {
    Equipment original("EQ001", EquipmentType::Forklift, "Test Forklift");
    original.setStatus(EquipmentStatus::Active);
    
    MockPosition pos(10.0, 20.0);
    original.recordPosition(pos);
    
    Equipment target("EQ002", EquipmentType::Crane, "Test Crane");
    
    // Move assign
    target = std::move(original);
    
    // Check moved-to object
    EXPECT_EQ(target.getId(), "EQ001");
    EXPECT_EQ(target.getType(), EquipmentType::Forklift);
    EXPECT_EQ(target.getName(), "Test Forklift");
    EXPECT_EQ(target.getStatus(), EquipmentStatus::Active);
    EXPECT_TRUE(target.getLastPosition().has_value());
    EXPECT_EQ(target.getPositionHistory().size(), 1);
    
    // Check moved-from object (should be in a valid but unspecified state)
    EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
}

TEST_F(EquipmentTest, SelfMoveAssignmentHandledCorrectly) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    equipment.setStatus(EquipmentStatus::Active);
    
    // Self-assignment should be a no-op
    equipment = std::move(equipment);
    
    EXPECT_EQ(equipment.getId(), "EQ001");
    EXPECT_EQ(equipment.getType(), EquipmentType::Forklift);
    EXPECT_EQ(equipment.getName(), "Test Forklift");
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
}

TEST_F(EquipmentTest, SetAndGetStatus) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    equipment.setStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Maintenance);
    
    equipment.setStatus(EquipmentStatus::Active);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
}

TEST_F(EquipmentTest, SetAndGetName) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    equipment.setName("New Name");
    EXPECT_EQ(equipment.getName(), "New Name");
}

TEST_F(EquipmentTest, SetAndGetLastPosition) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Initially no position
    EXPECT_FALSE(equipment.getLastPosition().has_value());
    
    MockPosition pos(10.0, 20.0);
    equipment.setLastPosition(pos);
    
    // Now should have a position
    EXPECT_TRUE(equipment.getLastPosition().has_value());
}

TEST_F(EquipmentTest, RecordPositionUpdatesLastPositionAndHistory) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    MockPosition pos1(10.0, 20.0);
    equipment.recordPosition(pos1);
    
    // Check last position
    EXPECT_TRUE(equipment.getLastPosition().has_value());
    
    // Check history
    auto history = equipment.getPositionHistory();
    EXPECT_EQ(history.size(), 1);
    
    // Check status update
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
    
    // Add another position
    MockPosition pos2(11.0, 21.0);
    equipment.recordPosition(pos2);
    
    // Check history again
    history = equipment.getPositionHistory();
    EXPECT_EQ(history.size(), 2);
}

TEST_F(EquipmentTest, PositionHistorySizeLimit) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Set a small max history size for testing
    equipment.setMaxHistorySize(3);
    
    // Add 5 positions
    for (int i = 0; i < 5; i++) {
        MockPosition pos(10.0 + i, 20.0 + i);
        equipment.recordPosition(pos);
    }
    
    // History should be limited to 3 positions
    auto history = equipment.getPositionHistory();
    EXPECT_EQ(history.size(), 3);
    
    // The oldest positions should have been removed
    // We can't directly check coordinates since we're using mocks,
    // but we can verify the size constraint is enforced
}

TEST_F(EquipmentTest, ClearPositionHistory) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Add some positions
    for (int i = 0; i < 3; i++) {
        MockPosition pos(10.0 + i, 20.0 + i);
        equipment.recordPosition(pos);
    }
    
    // Verify positions were added
    EXPECT_EQ(equipment.getPositionHistory().size(), 3);
    
    // Clear history
    equipment.clearPositionHistory();
    
    // Verify history is empty
    EXPECT_TRUE(equipment.getPositionHistory().empty());
    
    // Last position should still be available
    EXPECT_TRUE(equipment.getLastPosition().has_value());
}

TEST_F(EquipmentTest, IsMovingWithInsufficientPositions) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // No positions
    EXPECT_FALSE(equipment.isMoving());
    
    // Only one position
    MockPosition pos(10.0, 20.0);
    equipment.recordPosition(pos);
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, IsMovingWithMovement) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Add first position
    auto now = std::chrono::system_clock::now();
    MockPosition pos1(10.0, 20.0, now);
    equipment.recordPosition(pos1);
    
    // Add second position 10 seconds later
    // The mock distanceTo returns 100.0, so speed will be 100/10 = 10 m/s
    MockPosition pos2(11.0, 21.0, now + std::chrono::seconds(10));
    equipment.recordPosition(pos2);
    
    // Should be moving if speed > MOVEMENT_SPEED_THRESHOLD
    if (10.0 > MOVEMENT_SPEED_THRESHOLD) {
        EXPECT_TRUE(equipment.isMoving());
    } else {
        EXPECT_FALSE(equipment.isMoving());
    }
}

TEST_F(EquipmentTest, IsMovingWithSmallTimeDifference) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Add positions with very small time difference
    auto now = std::chrono::system_clock::now();
    MockPosition pos1(10.0, 20.0, now);
    equipment.recordPosition(pos1);
    
    // Less than 1 second difference
    MockPosition pos2(11.0, 21.0, now + std::chrono::milliseconds(500));
    equipment.recordPosition(pos2);
    
    // Should not be moving due to small time difference
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, ToStringFormatting) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    equipment.setStatus(EquipmentStatus::Active);
    
    std::string str = equipment.toString();
    
    // Check that the string contains all the expected parts
    EXPECT_THAT(str, ::testing::HasSubstr("Equipment(id=EQ001"));
    EXPECT_THAT(str, ::testing::HasSubstr("name=Test Forklift"));
    EXPECT_THAT(str, ::testing::HasSubstr("type=Forklift"));
    EXPECT_THAT(str, ::testing::HasSubstr("status=Active"));
}

TEST_F(EquipmentTest, ToStringWithDifferentTypes) {
    Equipment crane("EQ002", EquipmentType::Crane, "Test Crane");
    Equipment bulldozer("EQ003", EquipmentType::Bulldozer, "Test Bulldozer");
    Equipment excavator("EQ004", EquipmentType::Excavator, "Test Excavator");
    Equipment truck("EQ005", EquipmentType::Truck, "Test Truck");
    Equipment other("EQ006", static_cast<EquipmentType>(99), "Test Other");
    
    EXPECT_THAT(crane.toString(), ::testing::HasSubstr("type=Crane"));
    EXPECT_THAT(bulldozer.toString(), ::testing::HasSubstr("type=Bulldozer"));
    EXPECT_THAT(excavator.toString(), ::testing::HasSubstr("type=Excavator"));
    EXPECT_THAT(truck.toString(), ::testing::HasSubstr("type=Truck"));
    EXPECT_THAT(other.toString(), ::testing::HasSubstr("type=Other"));
}

TEST_F(EquipmentTest, ToStringWithDifferentStatuses) {
    Equipment active("EQ001", EquipmentType::Forklift, "Test Active");
    active.setStatus(EquipmentStatus::Active);
    
    Equipment inactive("EQ002", EquipmentType::Forklift, "Test Inactive");
    inactive.setStatus(EquipmentStatus::Inactive);
    
    Equipment maintenance("EQ003", EquipmentType::Forklift, "Test Maintenance");
    maintenance.setStatus(EquipmentStatus::Maintenance);
    
    Equipment unknown("EQ004", EquipmentType::Forklift, "Test Unknown");
    unknown.setStatus(static_cast<EquipmentStatus>(99));
    
    EXPECT_THAT(active.toString(), ::testing::HasSubstr("status=Active"));
    EXPECT_THAT(inactive.toString(), ::testing::HasSubstr("status=Inactive"));
    EXPECT_THAT(maintenance.toString(), ::testing::HasSubstr("status=Maintenance"));
    EXPECT_THAT(unknown.toString(), ::testing::HasSubstr("status=Unknown"));
}

} // namespace equipment_tracker
// </test_code>