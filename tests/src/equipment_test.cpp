// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sstream>
#include <chrono>
#include <cmath>
#include <thread>
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/utils/constants.h"

// Mock Position class if not available in test environment
#ifndef POSITION_MOCK_DEFINED
#define POSITION_MOCK_DEFINED

namespace equipment_tracker {
class Position {
public:
    Position(double lat = 0.0, double lon = 0.0, double alt = 0.0)
        : latitude_(lat), longitude_(lon), altitude_(alt), 
          timestamp_(std::chrono::system_clock::now()) {}
    
    Position(double lat, double lon, double alt, std::chrono::system_clock::time_point timestamp)
        : latitude_(lat), longitude_(lon), altitude_(alt), timestamp_(timestamp) {}
    
    double getLatitude() const { return latitude_; }
    double getLongitude() const { return longitude_; }
    double getAltitude() const { return altitude_; }
    std::chrono::system_clock::time_point getTimestamp() const { return timestamp_; }
    
    double distanceTo(const Position& other) const {
        // Haversine formula for distance calculation
        const double R = 6371000.0; // Earth radius in meters
        double lat1 = latitude_ * M_PI / 180.0;
        double lat2 = other.latitude_ * M_PI / 180.0;
        double dLat = (other.latitude_ - latitude_) * M_PI / 180.0;
        double dLon = (other.longitude_ - longitude_) * M_PI / 180.0;
        
        double a = sin(dLat/2) * sin(dLat/2) +
                  cos(lat1) * cos(lat2) * 
                  sin(dLon/2) * sin(dLon/2);
        double c = 2 * atan2(sqrt(a), sqrt(1-a));
        return R * c;
    }
    
private:
    double latitude_;
    double longitude_;
    double altitude_;
    std::chrono::system_clock::time_point timestamp_;
};
}

#endif

// Define constants if not available in test environment
#ifndef MOVEMENT_SPEED_THRESHOLD
#define MOVEMENT_SPEED_THRESHOLD 0.5 // meters per second
#endif

namespace equipment_tracker {

class EquipmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for tests
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
    
    Position pos(37.7749, -122.4194, 10.0);
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
    
    Position pos(37.7749, -122.4194, 10.0);
    original.recordPosition(pos);
    
    Equipment moved("EQ002", EquipmentType::Crane, "Test Crane");
    moved = std::move(original);
    
    EXPECT_EQ(moved.getId(), "EQ001");
    EXPECT_EQ(moved.getType(), EquipmentType::Forklift);
    EXPECT_EQ(moved.getName(), "Test Forklift");
    EXPECT_EQ(moved.getStatus(), EquipmentStatus::Active);
    EXPECT_TRUE(moved.getLastPosition().has_value());
    EXPECT_EQ(moved.getPositionHistory().size(), 1);
    
    // Original should be in an unknown state
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

TEST_F(EquipmentTest, GetLastPositionReturnsCorrectValue) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Initially, last position should be empty
    EXPECT_FALSE(equipment.getLastPosition().has_value());
    
    // Set a position
    Position pos(37.7749, -122.4194, 10.0);
    equipment.setLastPosition(pos);
    
    // Now last position should have a value
    ASSERT_TRUE(equipment.getLastPosition().has_value());
    EXPECT_NEAR(equipment.getLastPosition()->getLatitude(), 37.7749, 0.0001);
    EXPECT_NEAR(equipment.getLastPosition()->getLongitude(), -122.4194, 0.0001);
    EXPECT_NEAR(equipment.getLastPosition()->getAltitude(), 10.0, 0.0001);
}

TEST_F(EquipmentTest, SetStatusChangesStatus) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Initial status should be Inactive
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Inactive);
    
    // Change status
    equipment.setStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Maintenance);
    
    equipment.setStatus(EquipmentStatus::Active);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
}

TEST_F(EquipmentTest, SetNameChangesName) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Initial name
    EXPECT_EQ(equipment.getName(), "Test Forklift");
    
    // Change name
    equipment.setName("New Forklift Name");
    EXPECT_EQ(equipment.getName(), "New Forklift Name");
}

TEST_F(EquipmentTest, RecordPositionUpdatesHistoryAndStatus) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Initially, status should be Inactive
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Inactive);
    
    // Record a position
    Position pos1(37.7749, -122.4194, 10.0);
    equipment.recordPosition(pos1);
    
    // Status should change to Active
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
    
    // Last position should be updated
    ASSERT_TRUE(equipment.getLastPosition().has_value());
    EXPECT_NEAR(equipment.getLastPosition()->getLatitude(), 37.7749, 0.0001);
    
    // Position history should have one entry
    EXPECT_EQ(equipment.getPositionHistory().size(), 1);
    
    // Record another position
    Position pos2(37.7750, -122.4195, 11.0);
    equipment.recordPosition(pos2);
    
    // Position history should have two entries
    EXPECT_EQ(equipment.getPositionHistory().size(), 2);
    
    // Last position should be updated to the new position
    ASSERT_TRUE(equipment.getLastPosition().has_value());
    EXPECT_NEAR(equipment.getLastPosition()->getLatitude(), 37.7750, 0.0001);
}

TEST_F(EquipmentTest, PositionHistorySizeIsLimited) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Set max history size to a small value for testing
    equipment.setMaxHistorySize(3);
    
    // Record more positions than the max history size
    for (int i = 0; i < 5; i++) {
        Position pos(37.7749 + i * 0.001, -122.4194 + i * 0.001, 10.0 + i);
        equipment.recordPosition(pos);
    }
    
    // History size should be limited to max
    EXPECT_EQ(equipment.getPositionHistory().size(), 3);
    
    // The oldest positions should be removed
    auto history = equipment.getPositionHistory();
    EXPECT_NEAR(history[0].getLatitude(), 37.7749 + 2 * 0.001, 0.0001);
    EXPECT_NEAR(history[1].getLatitude(), 37.7749 + 3 * 0.001, 0.0001);
    EXPECT_NEAR(history[2].getLatitude(), 37.7749 + 4 * 0.001, 0.0001);
}

TEST_F(EquipmentTest, ClearPositionHistoryWorks) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Record some positions
    for (int i = 0; i < 3; i++) {
        Position pos(37.7749 + i * 0.001, -122.4194 + i * 0.001, 10.0 + i);
        equipment.recordPosition(pos);
    }
    
    // History should have entries
    EXPECT_EQ(equipment.getPositionHistory().size(), 3);
    
    // Clear history
    equipment.clearPositionHistory();
    
    // History should be empty
    EXPECT_TRUE(equipment.getPositionHistory().empty());
    
    // Last position should still be available
    EXPECT_TRUE(equipment.getLastPosition().has_value());
}

TEST_F(EquipmentTest, IsMovingReturnsFalseWithInsufficientData) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // With no positions, should not be moving
    EXPECT_FALSE(equipment.isMoving());
    
    // With only one position, should not be moving
    Position pos(37.7749, -122.4194, 10.0);
    equipment.recordPosition(pos);
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, IsMovingDetectsMovement) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Create timestamps with sufficient time difference
    auto now = std::chrono::system_clock::now();
    auto earlier = now - std::chrono::seconds(10);
    
    // Record positions that are far enough apart to indicate movement
    // (assuming MOVEMENT_SPEED_THRESHOLD is 0.5 m/s)
    // These positions are ~111 meters apart
    Position pos1(37.7749, -122.4194, 10.0, earlier);
    Position pos2(37.7750, -122.4194, 10.0, now); // ~111m north in 10 seconds = ~11.1 m/s
    
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    
    // Should detect movement
    EXPECT_TRUE(equipment.isMoving());
}

TEST_F(EquipmentTest, IsMovingReturnsFalseForSlowMovement) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Create timestamps with sufficient time difference
    auto now = std::chrono::system_clock::now();
    auto earlier = now - std::chrono::seconds(10);
    
    // Record positions that are close enough to indicate no significant movement
    // (assuming MOVEMENT_SPEED_THRESHOLD is 0.5 m/s)
    // These positions are ~1.11 meters apart
    Position pos1(37.7749, -122.4194, 10.0, earlier);
    Position pos2(37.77491, -122.4194, 10.0, now); // ~1.11m north in 10 seconds = ~0.111 m/s
    
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    
    // Should not detect movement (below threshold)
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, IsMovingHandlesSmallTimeDifference) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    
    // Create timestamps with very small time difference
    auto now = std::chrono::system_clock::now();
    auto almostNow = now - std::chrono::milliseconds(500); // 0.5 seconds
    
    // Record positions
    Position pos1(37.7749, -122.4194, 10.0, almostNow);
    Position pos2(37.7750, -122.4194, 10.0, now); // ~111m difference
    
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    
    // Should handle small time difference gracefully
    // The time difference is less than 1 second, so it should return false
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
    // Test each equipment type
    Equipment forklift("EQ001", EquipmentType::Forklift, "Test Equipment");
    EXPECT_THAT(forklift.toString(), ::testing::HasSubstr("type=Forklift"));
    
    Equipment crane("EQ002", EquipmentType::Crane, "Test Equipment");
    EXPECT_THAT(crane.toString(), ::testing::HasSubstr("type=Crane"));
    
    Equipment bulldozer("EQ003", EquipmentType::Bulldozer, "Test Equipment");
    EXPECT_THAT(bulldozer.toString(), ::testing::HasSubstr("type=Bulldozer"));
    
    Equipment excavator("EQ004", EquipmentType::Excavator, "Test Equipment");
    EXPECT_THAT(excavator.toString(), ::testing::HasSubstr("type=Excavator"));
    
    Equipment truck("EQ005", EquipmentType::Truck, "Test Equipment");
    EXPECT_THAT(truck.toString(), ::testing::HasSubstr("type=Truck"));
    
    // Test unknown type (assuming 99 is not a valid enum value)
    Equipment other("EQ006", static_cast<EquipmentType>(99), "Test Equipment");
    EXPECT_THAT(other.toString(), ::testing::HasSubstr("type=Other"));
}

TEST_F(EquipmentTest, ToStringHandlesAllStatusTypes) {
    Equipment equipment("EQ001", EquipmentType::Forklift, "Test Equipment");
    
    equipment.setStatus(EquipmentStatus::Active);
    EXPECT_THAT(equipment.toString(), ::testing::HasSubstr("status=Active"));
    
    equipment.setStatus(EquipmentStatus::Inactive);
    EXPECT_THAT(equipment.toString(), ::testing::HasSubstr("status=Inactive"));
    
    equipment.setStatus(EquipmentStatus::Maintenance);
    EXPECT_THAT(equipment.toString(), ::testing::HasSubstr("status=Maintenance"));
    
    // Test unknown status (assuming 99 is not a valid enum value)
    equipment.setStatus(static_cast<EquipmentStatus>(99));
    EXPECT_THAT(equipment.toString(), ::testing::HasSubstr("status=Unknown"));
}

} // namespace equipment_tracker
// </test_code>