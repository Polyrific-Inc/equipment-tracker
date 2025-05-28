// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sstream>
#include <chrono>
#include <cmath>
#include <thread>
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/utils/constants.h"

// Mock Position class if it's not available in the test environment
#ifndef POSITION_MOCK_DEFINED
#define POSITION_MOCK_DEFINED

namespace equipment_tracker {
class Position {
public:
    Position(double lat = 0.0, double lon = 0.0, std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : lat_(lat), lon_(lon), timestamp_(timestamp) {}
    
    double getLatitude() const { return lat_; }
    double getLongitude() const { return lon_; }
    std::chrono::system_clock::time_point getTimestamp() const { return timestamp_; }
    
    double distanceTo(const Position& other) const {
        // Haversine formula for distance calculation
        const double R = 6371000.0; // Earth radius in meters
        double lat1 = lat_ * M_PI / 180.0;
        double lat2 = other.lat_ * M_PI / 180.0;
        double dLat = (other.lat_ - lat_) * M_PI / 180.0;
        double dLon = (other.lon_ - lon_) * M_PI / 180.0;
        
        double a = sin(dLat/2) * sin(dLat/2) +
                  cos(lat1) * cos(lat2) * 
                  sin(dLon/2) * sin(dLon/2);
        double c = 2 * atan2(sqrt(a), sqrt(1-a));
        return R * c;
    }
    
private:
    double lat_;
    double lon_;
    std::chrono::system_clock::time_point timestamp_;
};
}

#endif

// Define constants if not available in the test environment
#ifndef MOVEMENT_SPEED_THRESHOLD
#define MOVEMENT_SPEED_THRESHOLD 0.5 // m/s
#endif

namespace equipment_tracker {

class EquipmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for tests
    }
};

TEST_F(EquipmentTest, ConstructorInitializesCorrectly) {
    Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");
    
    EXPECT_EQ(equipment.getId(), "123");
    EXPECT_EQ(equipment.getType(), EquipmentType::Forklift);
    EXPECT_EQ(equipment.getName(), "Test Forklift");
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Inactive);
    EXPECT_FALSE(equipment.getLastPosition().has_value());
    EXPECT_TRUE(equipment.getPositionHistory().empty());
}

TEST_F(EquipmentTest, MoveConstructorWorks) {
    Equipment original("123", EquipmentType::Forklift, "Test Forklift");
    original.setStatus(EquipmentStatus::Active);
    
    Position pos(37.7749, -122.4194);
    original.recordPosition(pos);
    
    // Move construct
    Equipment moved(std::move(original));
    
    // Check moved-to object
    EXPECT_EQ(moved.getId(), "123");
    EXPECT_EQ(moved.getType(), EquipmentType::Forklift);
    EXPECT_EQ(moved.getName(), "Test Forklift");
    EXPECT_EQ(moved.getStatus(), EquipmentStatus::Active);
    EXPECT_TRUE(moved.getLastPosition().has_value());
    EXPECT_EQ(moved.getPositionHistory().size(), 1);
    
    // Check moved-from object (should be in a valid but unspecified state)
    EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
}

TEST_F(EquipmentTest, MoveAssignmentWorks) {
    Equipment original("123", EquipmentType::Forklift, "Test Forklift");
    original.setStatus(EquipmentStatus::Active);
    
    Position pos(37.7749, -122.4194);
    original.recordPosition(pos);
    
    Equipment target("456", EquipmentType::Crane, "Test Crane");
    
    // Move assign
    target = std::move(original);
    
    // Check moved-to object
    EXPECT_EQ(target.getId(), "123");
    EXPECT_EQ(target.getType(), EquipmentType::Forklift);
    EXPECT_EQ(target.getName(), "Test Forklift");
    EXPECT_EQ(target.getStatus(), EquipmentStatus::Active);
    EXPECT_TRUE(target.getLastPosition().has_value());
    EXPECT_EQ(target.getPositionHistory().size(), 1);
    
    // Check moved-from object
    EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
}

TEST_F(EquipmentTest, SelfMoveAssignmentDoesNothing) {
    Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");
    equipment.setStatus(EquipmentStatus::Active);
    
    // Self-assignment should be a no-op
    equipment = std::move(equipment);
    
    EXPECT_EQ(equipment.getId(), "123");
    EXPECT_EQ(equipment.getType(), EquipmentType::Forklift);
    EXPECT_EQ(equipment.getName(), "Test Forklift");
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
}

TEST_F(EquipmentTest, SetAndGetStatus) {
    Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");
    
    equipment.setStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Maintenance);
    
    equipment.setStatus(EquipmentStatus::Active);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
}

TEST_F(EquipmentTest, SetAndGetName) {
    Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");
    
    equipment.setName("New Name");
    EXPECT_EQ(equipment.getName(), "New Name");
}

TEST_F(EquipmentTest, SetAndGetLastPosition) {
    Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");
    
    Position pos(37.7749, -122.4194);
    equipment.setLastPosition(pos);
    
    auto lastPos = equipment.getLastPosition();
    ASSERT_TRUE(lastPos.has_value());
    EXPECT_NEAR(lastPos->getLatitude(), 37.7749, 0.0001);
    EXPECT_NEAR(lastPos->getLongitude(), -122.4194, 0.0001);
}

TEST_F(EquipmentTest, RecordPositionUpdatesLastPositionAndHistory) {
    Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");
    
    Position pos1(37.7749, -122.4194);
    equipment.recordPosition(pos1);
    
    // Check last position
    auto lastPos = equipment.getLastPosition();
    ASSERT_TRUE(lastPos.has_value());
    EXPECT_NEAR(lastPos->getLatitude(), 37.7749, 0.0001);
    EXPECT_NEAR(lastPos->getLongitude(), -122.4194, 0.0001);
    
    // Check history
    auto history = equipment.getPositionHistory();
    ASSERT_EQ(history.size(), 1);
    EXPECT_NEAR(history[0].getLatitude(), 37.7749, 0.0001);
    EXPECT_NEAR(history[0].getLongitude(), -122.4194, 0.0001);
    
    // Check status is updated to active
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
    
    // Add another position
    Position pos2(37.7750, -122.4195);
    equipment.recordPosition(pos2);
    
    // Check updated last position
    lastPos = equipment.getLastPosition();
    ASSERT_TRUE(lastPos.has_value());
    EXPECT_NEAR(lastPos->getLatitude(), 37.7750, 0.0001);
    EXPECT_NEAR(lastPos->getLongitude(), -122.4195, 0.0001);
    
    // Check updated history
    history = equipment.getPositionHistory();
    ASSERT_EQ(history.size(), 2);
    EXPECT_NEAR(history[1].getLatitude(), 37.7750, 0.0001);
    EXPECT_NEAR(history[1].getLongitude(), -122.4195, 0.0001);
}

TEST_F(EquipmentTest, PositionHistorySizeIsLimited) {
    Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");
    
    // Record more positions than the default max history size
    const int maxHistorySize = 100; // Assuming default is 100
    for (int i = 0; i < maxHistorySize + 10; i++) {
        Position pos(37.7749 + i * 0.0001, -122.4194 + i * 0.0001);
        equipment.recordPosition(pos);
    }
    
    // Check history size is limited
    auto history = equipment.getPositionHistory();
    EXPECT_EQ(history.size(), maxHistorySize);
    
    // Check the oldest entries were removed
    EXPECT_NEAR(history[0].getLatitude(), 37.7749 + 10 * 0.0001, 0.0001);
    EXPECT_NEAR(history[0].getLongitude(), -122.4194 + 10 * 0.0001, 0.0001);
}

TEST_F(EquipmentTest, ClearPositionHistory) {
    Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");
    
    // Add some positions
    for (int i = 0; i < 5; i++) {
        Position pos(37.7749 + i * 0.0001, -122.4194 + i * 0.0001);
        equipment.recordPosition(pos);
    }
    
    // Verify positions were added
    EXPECT_EQ(equipment.getPositionHistory().size(), 5);
    
    // Clear history
    equipment.clearPositionHistory();
    
    // Verify history is empty
    EXPECT_TRUE(equipment.getPositionHistory().empty());
    
    // Last position should still be available
    EXPECT_TRUE(equipment.getLastPosition().has_value());
}

TEST_F(EquipmentTest, IsMovingReturnsFalseWithInsufficientData) {
    Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");
    
    // No positions
    EXPECT_FALSE(equipment.isMoving());
    
    // Only one position
    Position pos(37.7749, -122.4194);
    equipment.recordPosition(pos);
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, IsMovingDetectsMovement) {
    Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");
    
    // Add first position
    auto now = std::chrono::system_clock::now();
    Position pos1(37.7749, -122.4194, now);
    equipment.recordPosition(pos1);
    
    // Add second position 10 seconds later with significant movement
    // (about 111 meters per 0.001 degree at the equator)
    Position pos2(37.7749 + 0.001, -122.4194, now + std::chrono::seconds(10));
    equipment.recordPosition(pos2);
    
    // Should detect movement (speed > threshold)
    EXPECT_TRUE(equipment.isMoving());
}

TEST_F(EquipmentTest, IsMovingReturnsFalseForSlowMovement) {
    Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");
    
    // Add first position
    auto now = std::chrono::system_clock::now();
    Position pos1(37.7749, -122.4194, now);
    equipment.recordPosition(pos1);
    
    // Add second position 100 seconds later with minimal movement
    // Small movement that results in speed below threshold
    Position pos2(37.7749 + 0.00001, -122.4194, now + std::chrono::seconds(100));
    equipment.recordPosition(pos2);
    
    // Should not detect movement (speed < threshold)
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, IsMovingHandlesSmallTimeDifference) {
    Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");
    
    // Add first position
    auto now = std::chrono::system_clock::now();
    Position pos1(37.7749, -122.4194, now);
    equipment.recordPosition(pos1);
    
    // Add second position with almost same timestamp
    Position pos2(37.7749 + 0.001, -122.4194, now + std::chrono::milliseconds(500));
    equipment.recordPosition(pos2);
    
    // Should return false for very small time differences to avoid division by zero
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, ToStringReturnsCorrectFormat) {
    Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");
    equipment.setStatus(EquipmentStatus::Active);
    
    std::string result = equipment.toString();
    
    EXPECT_THAT(result, ::testing::HasSubstr("Equipment(id=123"));
    EXPECT_THAT(result, ::testing::HasSubstr("name=Test Forklift"));
    EXPECT_THAT(result, ::testing::HasSubstr("type=Forklift"));
    EXPECT_THAT(result, ::testing::HasSubstr("status=Active"));
}

TEST_F(EquipmentTest, ToStringHandlesAllEquipmentTypes) {
    Equipment forklift("1", EquipmentType::Forklift, "Forklift");
    Equipment crane("2", EquipmentType::Crane, "Crane");
    Equipment bulldozer("3", EquipmentType::Bulldozer, "Bulldozer");
    Equipment excavator("4", EquipmentType::Excavator, "Excavator");
    Equipment truck("5", EquipmentType::Truck, "Truck");
    Equipment other("6", static_cast<EquipmentType>(99), "Other");
    
    EXPECT_THAT(forklift.toString(), ::testing::HasSubstr("type=Forklift"));
    EXPECT_THAT(crane.toString(), ::testing::HasSubstr("type=Crane"));
    EXPECT_THAT(bulldozer.toString(), ::testing::HasSubstr("type=Bulldozer"));
    EXPECT_THAT(excavator.toString(), ::testing::HasSubstr("type=Excavator"));
    EXPECT_THAT(truck.toString(), ::testing::HasSubstr("type=Truck"));
    EXPECT_THAT(other.toString(), ::testing::HasSubstr("type=Other"));
}

TEST_F(EquipmentTest, ToStringHandlesAllStatusTypes) {
    Equipment active("1", EquipmentType::Forklift, "Active");
    Equipment inactive("2", EquipmentType::Forklift, "Inactive");
    Equipment maintenance("3", EquipmentType::Forklift, "Maintenance");
    Equipment unknown("4", EquipmentType::Forklift, "Unknown");
    
    active.setStatus(EquipmentStatus::Active);
    inactive.setStatus(EquipmentStatus::Inactive);
    maintenance.setStatus(EquipmentStatus::Maintenance);
    unknown.setStatus(static_cast<EquipmentStatus>(99));
    
    EXPECT_THAT(active.toString(), ::testing::HasSubstr("status=Active"));
    EXPECT_THAT(inactive.toString(), ::testing::HasSubstr("status=Inactive"));
    EXPECT_THAT(maintenance.toString(), ::testing::HasSubstr("status=Maintenance"));
    EXPECT_THAT(unknown.toString(), ::testing::HasSubstr("status=Unknown"));
}

} // namespace equipment_tracker
// </test_code>