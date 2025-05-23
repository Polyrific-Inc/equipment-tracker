// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sstream>
#include <chrono>
#include <cmath>
#include <thread>
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/utils/constants.h"

// Mock Position class if not available in the test environment
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
#define MOVEMENT_SPEED_THRESHOLD 0.5 // meters per second
#endif

namespace equipment_tracker {

class EquipmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a default equipment for testing
        equipment = Equipment("EQ001", EquipmentType::Forklift, "Test Forklift");
    }
    
    Equipment equipment;
};

TEST_F(EquipmentTest, ConstructorInitializesCorrectly) {
    Equipment eq("EQ123", EquipmentType::Crane, "Test Crane");
    
    EXPECT_EQ(eq.getId(), "EQ123");
    EXPECT_EQ(eq.getType(), EquipmentType::Crane);
    EXPECT_EQ(eq.getName(), "Test Crane");
    EXPECT_EQ(eq.getStatus(), EquipmentStatus::Inactive);
    EXPECT_FALSE(eq.getLastPosition().has_value());
}

TEST_F(EquipmentTest, MoveConstructorWorks) {
    // Setup original equipment
    Equipment original("EQ456", EquipmentType::Bulldozer, "Original Bulldozer");
    Position pos(10.0, 20.0);
    original.recordPosition(pos);
    original.setStatus(EquipmentStatus::Active);
    
    // Move construct
    Equipment moved(std::move(original));
    
    // Check moved equipment has correct values
    EXPECT_EQ(moved.getId(), "EQ456");
    EXPECT_EQ(moved.getType(), EquipmentType::Bulldozer);
    EXPECT_EQ(moved.getName(), "Original Bulldozer");
    EXPECT_EQ(moved.getStatus(), EquipmentStatus::Active);
    
    // Check position was moved
    ASSERT_TRUE(moved.getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(moved.getLastPosition()->getLatitude(), 10.0);
    EXPECT_DOUBLE_EQ(moved.getLastPosition()->getLongitude(), 20.0);
    
    // Check original equipment was reset
    EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
    EXPECT_FALSE(original.getLastPosition().has_value());
}

TEST_F(EquipmentTest, MoveAssignmentWorks) {
    // Setup original equipment
    Equipment original("EQ789", EquipmentType::Excavator, "Original Excavator");
    Position pos(30.0, 40.0);
    original.recordPosition(pos);
    original.setStatus(EquipmentStatus::Maintenance);
    
    // Setup target equipment
    Equipment target("EQ999", EquipmentType::Truck, "Target Truck");
    
    // Move assign
    target = std::move(original);
    
    // Check target equipment has correct values
    EXPECT_EQ(target.getId(), "EQ789");
    EXPECT_EQ(target.getType(), EquipmentType::Excavator);
    EXPECT_EQ(target.getName(), "Original Excavator");
    EXPECT_EQ(target.getStatus(), EquipmentStatus::Maintenance);
    
    // Check position was moved
    ASSERT_TRUE(target.getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(target.getLastPosition()->getLatitude(), 30.0);
    EXPECT_DOUBLE_EQ(target.getLastPosition()->getLongitude(), 40.0);
    
    // Check original equipment was reset
    EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
    EXPECT_FALSE(original.getLastPosition().has_value());
}

TEST_F(EquipmentTest, SelfMoveAssignmentIsHandled) {
    // Setup equipment
    Equipment eq("EQ111", EquipmentType::Forklift, "Self Move Test");
    Position pos(50.0, 60.0);
    eq.recordPosition(pos);
    eq.setStatus(EquipmentStatus::Active);
    
    // Self move assign
    eq = std::move(eq);
    
    // Check equipment still has correct values
    EXPECT_EQ(eq.getId(), "EQ111");
    EXPECT_EQ(eq.getType(), EquipmentType::Forklift);
    EXPECT_EQ(eq.getName(), "Self Move Test");
    EXPECT_EQ(eq.getStatus(), EquipmentStatus::Active);
    
    // Check position is still there
    ASSERT_TRUE(eq.getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(eq.getLastPosition()->getLatitude(), 50.0);
    EXPECT_DOUBLE_EQ(eq.getLastPosition()->getLongitude(), 60.0);
}

TEST_F(EquipmentTest, GettersReturnCorrectValues) {
    EXPECT_EQ(equipment.getId(), "EQ001");
    EXPECT_EQ(equipment.getType(), EquipmentType::Forklift);
    EXPECT_EQ(equipment.getName(), "Test Forklift");
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Inactive);
}

TEST_F(EquipmentTest, SettersUpdateValues) {
    equipment.setName("Updated Forklift");
    equipment.setStatus(EquipmentStatus::Maintenance);
    
    EXPECT_EQ(equipment.getName(), "Updated Forklift");
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Maintenance);
}

TEST_F(EquipmentTest, SetLastPositionWorks) {
    Position pos(12.34, 56.78);
    equipment.setLastPosition(pos);
    
    auto lastPos = equipment.getLastPosition();
    ASSERT_TRUE(lastPos.has_value());
    EXPECT_DOUBLE_EQ(lastPos->getLatitude(), 12.34);
    EXPECT_DOUBLE_EQ(lastPos->getLongitude(), 56.78);
}

TEST_F(EquipmentTest, RecordPositionUpdatesLastPositionAndHistory) {
    Position pos1(10.0, 20.0);
    Position pos2(11.0, 21.0);
    
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    
    // Check last position
    auto lastPos = equipment.getLastPosition();
    ASSERT_TRUE(lastPos.has_value());
    EXPECT_DOUBLE_EQ(lastPos->getLatitude(), 11.0);
    EXPECT_DOUBLE_EQ(lastPos->getLongitude(), 21.0);
    
    // Check history
    auto history = equipment.getPositionHistory();
    ASSERT_EQ(history.size(), 2);
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), 10.0);
    EXPECT_DOUBLE_EQ(history[0].getLongitude(), 20.0);
    EXPECT_DOUBLE_EQ(history[1].getLatitude(), 11.0);
    EXPECT_DOUBLE_EQ(history[1].getLongitude(), 21.0);
    
    // Check status is updated to active
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
}

TEST_F(EquipmentTest, PositionHistorySizeIsLimited) {
    // Set a small max history size for testing
    equipment.setMaxHistorySize(3);
    
    // Record more positions than the max size
    for (int i = 0; i < 5; i++) {
        Position pos(i * 1.0, i * 2.0);
        equipment.recordPosition(pos);
    }
    
    // Check history size is limited
    auto history = equipment.getPositionHistory();
    ASSERT_EQ(history.size(), 3);
    
    // Check the oldest positions were removed
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), 2.0);
    EXPECT_DOUBLE_EQ(history[0].getLongitude(), 4.0);
    EXPECT_DOUBLE_EQ(history[1].getLatitude(), 3.0);
    EXPECT_DOUBLE_EQ(history[1].getLongitude(), 6.0);
    EXPECT_DOUBLE_EQ(history[2].getLatitude(), 4.0);
    EXPECT_DOUBLE_EQ(history[2].getLongitude(), 8.0);
}

TEST_F(EquipmentTest, ClearPositionHistoryWorks) {
    // Record some positions
    Position pos1(10.0, 20.0);
    Position pos2(11.0, 21.0);
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    
    // Clear history
    equipment.clearPositionHistory();
    
    // Check history is empty
    auto history = equipment.getPositionHistory();
    EXPECT_TRUE(history.empty());
    
    // Last position should still be available
    auto lastPos = equipment.getLastPosition();
    ASSERT_TRUE(lastPos.has_value());
    EXPECT_DOUBLE_EQ(lastPos->getLatitude(), 11.0);
    EXPECT_DOUBLE_EQ(lastPos->getLongitude(), 21.0);
}

TEST_F(EquipmentTest, IsMovingReturnsFalseWithInsufficientPositions) {
    // No positions
    EXPECT_FALSE(equipment.isMoving());
    
    // Only one position
    Position pos(10.0, 20.0);
    equipment.recordPosition(pos);
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, IsMovingDetectsMovement) {
    // Create positions with timestamps 10 seconds apart
    auto now = std::chrono::system_clock::now();
    Position pos1(10.0, 20.0, now - std::chrono::seconds(10));
    Position pos2(10.01, 20.01, now); // Small movement, ~1.5km distance
    
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    
    // Should be moving (speed > threshold)
    EXPECT_TRUE(equipment.isMoving());
}

TEST_F(EquipmentTest, IsMovingDetectsStationary) {
    // Create positions with timestamps 10 seconds apart
    auto now = std::chrono::system_clock::now();
    Position pos1(10.0, 20.0, now - std::chrono::seconds(10));
    Position pos2(10.000001, 20.000001, now); // Tiny movement, ~0.15m distance
    
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    
    // Should not be moving (speed < threshold)
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, IsMovingHandlesSmallTimeDifference) {
    // Create positions with timestamps less than 1 second apart
    auto now = std::chrono::system_clock::now();
    Position pos1(10.0, 20.0, now - std::chrono::milliseconds(500));
    Position pos2(10.01, 20.01, now); // Would be fast movement
    
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    
    // Should return false to avoid division by zero
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, ToStringFormatsCorrectly) {
    equipment.setStatus(EquipmentStatus::Active);
    std::string result = equipment.toString();
    
    EXPECT_THAT(result, ::testing::HasSubstr("id=EQ001"));
    EXPECT_THAT(result, ::testing::HasSubstr("name=Test Forklift"));
    EXPECT_THAT(result, ::testing::HasSubstr("type=Forklift"));
    EXPECT_THAT(result, ::testing::HasSubstr("status=Active"));
}

TEST_F(EquipmentTest, ToStringHandlesAllEquipmentTypes) {
    Equipment crane("C1", EquipmentType::Crane, "Crane");
    Equipment bulldozer("B1", EquipmentType::Bulldozer, "Bulldozer");
    Equipment excavator("E1", EquipmentType::Excavator, "Excavator");
    Equipment truck("T1", EquipmentType::Truck, "Truck");
    Equipment other("O1", static_cast<EquipmentType>(99), "Other"); // Unknown type
    
    EXPECT_THAT(crane.toString(), ::testing::HasSubstr("type=Crane"));
    EXPECT_THAT(bulldozer.toString(), ::testing::HasSubstr("type=Bulldozer"));
    EXPECT_THAT(excavator.toString(), ::testing::HasSubstr("type=Excavator"));
    EXPECT_THAT(truck.toString(), ::testing::HasSubstr("type=Truck"));
    EXPECT_THAT(other.toString(), ::testing::HasSubstr("type=Other"));
}

TEST_F(EquipmentTest, ToStringHandlesAllStatusTypes) {
    Equipment active("A1", EquipmentType::Forklift, "Active");
    active.setStatus(EquipmentStatus::Active);
    
    Equipment inactive("I1", EquipmentType::Forklift, "Inactive");
    inactive.setStatus(EquipmentStatus::Inactive);
    
    Equipment maintenance("M1", EquipmentType::Forklift, "Maintenance");
    maintenance.setStatus(EquipmentStatus::Maintenance);
    
    Equipment unknown("U1", EquipmentType::Forklift, "Unknown");
    unknown.setStatus(static_cast<EquipmentStatus>(99)); // Unknown status
    
    EXPECT_THAT(active.toString(), ::testing::HasSubstr("status=Active"));
    EXPECT_THAT(inactive.toString(), ::testing::HasSubstr("status=Inactive"));
    EXPECT_THAT(maintenance.toString(), ::testing::HasSubstr("status=Maintenance"));
    EXPECT_THAT(unknown.toString(), ::testing::HasSubstr("status=Unknown"));
}

} // namespace equipment_tracker
// </test_code>