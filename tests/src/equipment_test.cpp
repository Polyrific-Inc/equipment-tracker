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

TEST_F(EquipmentTest, ConstructorInitializesPropertiesCorrectly) {
    Equipment equipment("EQ123", EquipmentType::Forklift, "Test Forklift");
    
    EXPECT_EQ(equipment.getId(), "EQ123");
    EXPECT_EQ(equipment.getType(), EquipmentType::Forklift);
    EXPECT_EQ(equipment.getName(), "Test Forklift");
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Inactive);
    EXPECT_FALSE(equipment.getLastPosition().has_value());
}

TEST_F(EquipmentTest, MoveConstructorTransfersOwnership) {
    Equipment original("EQ123", EquipmentType::Forklift, "Test Forklift");
    original.setStatus(EquipmentStatus::Active);
    
    Position pos(37.7749, -122.4194, 10.0);
    original.setLastPosition(pos);
    
    Equipment moved(std::move(original));
    
    EXPECT_EQ(moved.getId(), "EQ123");
    EXPECT_EQ(moved.getType(), EquipmentType::Forklift);
    EXPECT_EQ(moved.getName(), "Test Forklift");
    EXPECT_EQ(moved.getStatus(), EquipmentStatus::Active);
    
    auto movedPos = moved.getLastPosition();
    ASSERT_TRUE(movedPos.has_value());
    EXPECT_NEAR(movedPos->getLatitude(), 37.7749, 0.0001);
    EXPECT_NEAR(movedPos->getLongitude(), -122.4194, 0.0001);
    
    // Original should be reset
    EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
}

TEST_F(EquipmentTest, MoveAssignmentTransfersOwnership) {
    Equipment original("EQ123", EquipmentType::Forklift, "Test Forklift");
    original.setStatus(EquipmentStatus::Active);
    
    Position pos(37.7749, -122.4194, 10.0);
    original.setLastPosition(pos);
    
    Equipment target("EQ456", EquipmentType::Crane, "Test Crane");
    target = std::move(original);
    
    EXPECT_EQ(target.getId(), "EQ123");
    EXPECT_EQ(target.getType(), EquipmentType::Forklift);
    EXPECT_EQ(target.getName(), "Test Forklift");
    EXPECT_EQ(target.getStatus(), EquipmentStatus::Active);
    
    auto targetPos = target.getLastPosition();
    ASSERT_TRUE(targetPos.has_value());
    EXPECT_NEAR(targetPos->getLatitude(), 37.7749, 0.0001);
    EXPECT_NEAR(targetPos->getLongitude(), -122.4194, 0.0001);
    
    // Original should be reset
    EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
}

TEST_F(EquipmentTest, SelfAssignmentDoesNotAffectState) {
    Equipment equipment("EQ123", EquipmentType::Forklift, "Test Forklift");
    equipment.setStatus(EquipmentStatus::Active);
    
    equipment = std::move(equipment); // Self-assignment
    
    EXPECT_EQ(equipment.getId(), "EQ123");
    EXPECT_EQ(equipment.getType(), EquipmentType::Forklift);
    EXPECT_EQ(equipment.getName(), "Test Forklift");
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
}

TEST_F(EquipmentTest, GetLastPositionReturnsEmptyWhenNotSet) {
    Equipment equipment("EQ123", EquipmentType::Forklift, "Test Forklift");
    EXPECT_FALSE(equipment.getLastPosition().has_value());
}

TEST_F(EquipmentTest, SetAndGetLastPosition) {
    Equipment equipment("EQ123", EquipmentType::Forklift, "Test Forklift");
    Position pos(37.7749, -122.4194, 10.0);
    
    equipment.setLastPosition(pos);
    
    auto retrievedPos = equipment.getLastPosition();
    ASSERT_TRUE(retrievedPos.has_value());
    EXPECT_NEAR(retrievedPos->getLatitude(), 37.7749, 0.0001);
    EXPECT_NEAR(retrievedPos->getLongitude(), -122.4194, 0.0001);
    EXPECT_NEAR(retrievedPos->getAltitude(), 10.0, 0.0001);
}

TEST_F(EquipmentTest, SetAndGetStatus) {
    Equipment equipment("EQ123", EquipmentType::Forklift, "Test Forklift");
    
    equipment.setStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Maintenance);
    
    equipment.setStatus(EquipmentStatus::Active);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
}

TEST_F(EquipmentTest, SetAndGetName) {
    Equipment equipment("EQ123", EquipmentType::Forklift, "Test Forklift");
    
    equipment.setName("New Forklift Name");
    EXPECT_EQ(equipment.getName(), "New Forklift Name");
}

TEST_F(EquipmentTest, RecordPositionUpdatesLastPositionAndHistory) {
    Equipment equipment("EQ123", EquipmentType::Forklift, "Test Forklift");
    Position pos1(37.7749, -122.4194, 10.0);
    Position pos2(37.7750, -122.4195, 11.0);
    
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    
    // Check last position
    auto lastPos = equipment.getLastPosition();
    ASSERT_TRUE(lastPos.has_value());
    EXPECT_NEAR(lastPos->getLatitude(), 37.7750, 0.0001);
    EXPECT_NEAR(lastPos->getLongitude(), -122.4195, 0.0001);
    
    // Check history
    auto history = equipment.getPositionHistory();
    ASSERT_EQ(history.size(), 2);
    EXPECT_NEAR(history[0].getLatitude(), 37.7749, 0.0001);
    EXPECT_NEAR(history[1].getLatitude(), 37.7750, 0.0001);
    
    // Check status is updated to active
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
}

TEST_F(EquipmentTest, PositionHistorySizeIsLimited) {
    Equipment equipment("EQ123", EquipmentType::Forklift, "Test Forklift");
    
    // Set max history size to a small value for testing
    equipment.setMaxHistorySize(3);
    
    // Add more positions than the max size
    for (int i = 0; i < 5; i++) {
        Position pos(37.7749 + i * 0.001, -122.4194 + i * 0.001, 10.0 + i);
        equipment.recordPosition(pos);
    }
    
    auto history = equipment.getPositionHistory();
    ASSERT_EQ(history.size(), 3);
    
    // Verify we have the 3 most recent positions
    EXPECT_NEAR(history[0].getLatitude(), 37.7749 + 2 * 0.001, 0.0001);
    EXPECT_NEAR(history[1].getLatitude(), 37.7749 + 3 * 0.001, 0.0001);
    EXPECT_NEAR(history[2].getLatitude(), 37.7749 + 4 * 0.001, 0.0001);
}

TEST_F(EquipmentTest, ClearPositionHistoryRemovesAllPositions) {
    Equipment equipment("EQ123", EquipmentType::Forklift, "Test Forklift");
    
    // Add some positions
    for (int i = 0; i < 3; i++) {
        Position pos(37.7749 + i * 0.001, -122.4194 + i * 0.001, 10.0 + i);
        equipment.recordPosition(pos);
    }
    
    ASSERT_EQ(equipment.getPositionHistory().size(), 3);
    
    equipment.clearPositionHistory();
    
    EXPECT_EQ(equipment.getPositionHistory().size(), 0);
    
    // Last position should still be available
    EXPECT_TRUE(equipment.getLastPosition().has_value());
}

TEST_F(EquipmentTest, IsMovingReturnsFalseWithLessThanTwoPositions) {
    Equipment equipment("EQ123", EquipmentType::Forklift, "Test Forklift");
    
    // No positions
    EXPECT_FALSE(equipment.isMoving());
    
    // One position
    Position pos(37.7749, -122.4194, 10.0);
    equipment.recordPosition(pos);
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, IsMovingDetectsMovementCorrectly) {
    Equipment equipment("EQ123", EquipmentType::Forklift, "Test Forklift");
    
    // Create two positions with enough distance and time to be considered moving
    auto now = std::chrono::system_clock::now();
    Position pos1(37.7749, -122.4194, 10.0, now - std::chrono::seconds(10));
    Position pos2(37.7759, -122.4204, 10.0, now); // ~100m away after 10 seconds = 10m/s
    
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    
    EXPECT_TRUE(equipment.isMoving());
    
    // Create two positions that are too close to be considered moving
    Equipment equipment2("EQ456", EquipmentType::Forklift, "Test Forklift 2");
    Position pos3(37.7749, -122.4194, 10.0, now - std::chrono::seconds(10));
    Position pos4(37.77491, -122.41941, 10.0, now); // Very small movement
    
    equipment2.recordPosition(pos3);
    equipment2.recordPosition(pos4);
    
    EXPECT_FALSE(equipment2.isMoving());
}

TEST_F(EquipmentTest, IsMovingHandlesSmallTimeDifference) {
    Equipment equipment("EQ123", EquipmentType::Forklift, "Test Forklift");
    
    // Create two positions with very small time difference
    auto now = std::chrono::system_clock::now();
    Position pos1(37.7749, -122.4194, 10.0, now);
    Position pos2(37.7759, -122.4204, 10.0, now); // Same timestamp
    
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    
    // Should return false to avoid division by zero
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, ToStringFormatsEquipmentInfoCorrectly) {
    Equipment equipment("EQ123", EquipmentType::Forklift, "Test Forklift");
    equipment.setStatus(EquipmentStatus::Active);
    
    std::string result = equipment.toString();
    
    EXPECT_THAT(result, ::testing::HasSubstr("Equipment(id=EQ123"));
    EXPECT_THAT(result, ::testing::HasSubstr("name=Test Forklift"));
    EXPECT_THAT(result, ::testing::HasSubstr("type=Forklift"));
    EXPECT_THAT(result, ::testing::HasSubstr("status=Active"));
}

TEST_F(EquipmentTest, ToStringHandlesAllEquipmentTypes) {
    Equipment forklift("EQ1", EquipmentType::Forklift, "Forklift");
    Equipment crane("EQ2", EquipmentType::Crane, "Crane");
    Equipment bulldozer("EQ3", EquipmentType::Bulldozer, "Bulldozer");
    Equipment excavator("EQ4", EquipmentType::Excavator, "Excavator");
    Equipment truck("EQ5", EquipmentType::Truck, "Truck");
    Equipment other("EQ6", static_cast<EquipmentType>(99), "Other"); // Unknown type
    
    EXPECT_THAT(forklift.toString(), ::testing::HasSubstr("type=Forklift"));
    EXPECT_THAT(crane.toString(), ::testing::HasSubstr("type=Crane"));
    EXPECT_THAT(bulldozer.toString(), ::testing::HasSubstr("type=Bulldozer"));
    EXPECT_THAT(excavator.toString(), ::testing::HasSubstr("type=Excavator"));
    EXPECT_THAT(truck.toString(), ::testing::HasSubstr("type=Truck"));
    EXPECT_THAT(other.toString(), ::testing::HasSubstr("type=Other"));
}

TEST_F(EquipmentTest, ToStringHandlesAllEquipmentStatuses) {
    Equipment active("EQ1", EquipmentType::Forklift, "Active");
    active.setStatus(EquipmentStatus::Active);
    
    Equipment inactive("EQ2", EquipmentType::Forklift, "Inactive");
    inactive.setStatus(EquipmentStatus::Inactive);
    
    Equipment maintenance("EQ3", EquipmentType::Forklift, "Maintenance");
    maintenance.setStatus(EquipmentStatus::Maintenance);
    
    Equipment unknown("EQ4", EquipmentType::Forklift, "Unknown");
    unknown.setStatus(static_cast<EquipmentStatus>(99)); // Unknown status
    
    EXPECT_THAT(active.toString(), ::testing::HasSubstr("status=Active"));
    EXPECT_THAT(inactive.toString(), ::testing::HasSubstr("status=Inactive"));
    EXPECT_THAT(maintenance.toString(), ::testing::HasSubstr("status=Maintenance"));
    EXPECT_THAT(unknown.toString(), ::testing::HasSubstr("status=Unknown"));
}

} // namespace equipment_tracker
// </test_code>