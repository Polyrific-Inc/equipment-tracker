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

// Define constants if not available in test environment
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
    Position pos(40.7128, -74.0060);
    original.setLastPosition(pos);
    original.setStatus(EquipmentStatus::Active);
    
    // Move construct
    Equipment moved(std::move(original));
    
    // Check moved equipment has correct values
    EXPECT_EQ(moved.getId(), "EQ456");
    EXPECT_EQ(moved.getType(), EquipmentType::Bulldozer);
    EXPECT_EQ(moved.getName(), "Original Bulldozer");
    EXPECT_EQ(moved.getStatus(), EquipmentStatus::Active);
    
    // Check last position was moved
    ASSERT_TRUE(moved.getLastPosition().has_value());
    EXPECT_NEAR(moved.getLastPosition()->getLatitude(), 40.7128, 0.0001);
    EXPECT_NEAR(moved.getLastPosition()->getLongitude(), -74.0060, 0.0001);
    
    // Check original was reset
    EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
}

TEST_F(EquipmentTest, MoveAssignmentWorks) {
    // Setup original equipment
    Equipment original("EQ789", EquipmentType::Excavator, "Original Excavator");
    Position pos(34.0522, -118.2437);
    original.setLastPosition(pos);
    original.setStatus(EquipmentStatus::Maintenance);
    
    // Setup target equipment
    Equipment target("EQ999", EquipmentType::Truck, "Target Truck");
    
    // Move assign
    target = std::move(original);
    
    // Check target has correct values
    EXPECT_EQ(target.getId(), "EQ789");
    EXPECT_EQ(target.getType(), EquipmentType::Excavator);
    EXPECT_EQ(target.getName(), "Original Excavator");
    EXPECT_EQ(target.getStatus(), EquipmentStatus::Maintenance);
    
    // Check last position was moved
    ASSERT_TRUE(target.getLastPosition().has_value());
    EXPECT_NEAR(target.getLastPosition()->getLatitude(), 34.0522, 0.0001);
    EXPECT_NEAR(target.getLastPosition()->getLongitude(), -118.2437, 0.0001);
    
    // Check original was reset
    EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
}

TEST_F(EquipmentTest, SelfAssignmentHandledCorrectly) {
    // Setup equipment
    Equipment eq("EQ111", EquipmentType::Forklift, "Self Assign Test");
    Position pos(41.8781, -87.6298);
    eq.setLastPosition(pos);
    
    // Self-assignment
    eq = std::move(eq);
    
    // Check equipment still has correct values
    EXPECT_EQ(eq.getId(), "EQ111");
    EXPECT_EQ(eq.getType(), EquipmentType::Forklift);
    EXPECT_EQ(eq.getName(), "Self Assign Test");
    
    // Check last position is still there
    ASSERT_TRUE(eq.getLastPosition().has_value());
    EXPECT_NEAR(eq.getLastPosition()->getLatitude(), 41.8781, 0.0001);
    EXPECT_NEAR(eq.getLastPosition()->getLongitude(), -87.6298, 0.0001);
}

TEST_F(EquipmentTest, GettersAndSettersWork) {
    // Test initial values
    EXPECT_EQ(equipment.getId(), "EQ001");
    EXPECT_EQ(equipment.getType(), EquipmentType::Forklift);
    EXPECT_EQ(equipment.getName(), "Test Forklift");
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Inactive);
    
    // Test setters
    equipment.setName("Updated Forklift");
    equipment.setStatus(EquipmentStatus::Maintenance);
    
    // Verify changes
    EXPECT_EQ(equipment.getName(), "Updated Forklift");
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Maintenance);
}

TEST_F(EquipmentTest, PositionManagementWorks) {
    // Initially no position
    EXPECT_FALSE(equipment.getLastPosition().has_value());
    
    // Set last position
    Position pos1(37.7749, -122.4194);
    equipment.setLastPosition(pos1);
    
    // Check last position is set
    ASSERT_TRUE(equipment.getLastPosition().has_value());
    EXPECT_NEAR(equipment.getLastPosition()->getLatitude(), 37.7749, 0.0001);
    EXPECT_NEAR(equipment.getLastPosition()->getLongitude(), -122.4194, 0.0001);
    
    // Position history should still be empty
    EXPECT_TRUE(equipment.getPositionHistory().empty());
    
    // Record a position
    Position pos2(37.7750, -122.4195);
    equipment.recordPosition(pos2);
    
    // Check last position is updated
    ASSERT_TRUE(equipment.getLastPosition().has_value());
    EXPECT_NEAR(equipment.getLastPosition()->getLatitude(), 37.7750, 0.0001);
    EXPECT_NEAR(equipment.getLastPosition()->getLongitude(), -122.4195, 0.0001);
    
    // Position history should have one entry
    ASSERT_EQ(equipment.getPositionHistory().size(), 1);
    EXPECT_NEAR(equipment.getPositionHistory()[0].getLatitude(), 37.7750, 0.0001);
    EXPECT_NEAR(equipment.getPositionHistory()[0].getLongitude(), -122.4195, 0.0001);
    
    // Status should be active after recording position
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
    
    // Clear position history
    equipment.clearPositionHistory();
    EXPECT_TRUE(equipment.getPositionHistory().empty());
    
    // Last position should still be available
    ASSERT_TRUE(equipment.getLastPosition().has_value());
}

TEST_F(EquipmentTest, PositionHistorySizeLimit) {
    // Create equipment with small history size for testing
    Equipment eq("EQ222", EquipmentType::Truck, "History Test");
    eq.setMaxHistorySize(3);
    
    // Record multiple positions
    for (int i = 0; i < 5; i++) {
        Position pos(40.0 + i * 0.1, -74.0 + i * 0.1);
        eq.recordPosition(pos);
    }
    
    // Check history size is limited
    ASSERT_EQ(eq.getPositionHistory().size(), 3);
    
    // Check the oldest positions were removed
    auto history = eq.getPositionHistory();
    EXPECT_NEAR(history[0].getLatitude(), 40.2, 0.0001);
    EXPECT_NEAR(history[1].getLatitude(), 40.3, 0.0001);
    EXPECT_NEAR(history[2].getLatitude(), 40.4, 0.0001);
}

TEST_F(EquipmentTest, IsMovingDetectsMovement) {
    // Create equipment for testing
    Equipment eq("EQ333", EquipmentType::Forklift, "Movement Test");
    
    // No positions, should not be moving
    EXPECT_FALSE(eq.isMoving());
    
    // One position, should not be moving
    Position pos1(42.3601, -71.0589);
    eq.recordPosition(pos1);
    EXPECT_FALSE(eq.isMoving());
    
    // Add second position with small time difference, should not be moving
    auto now = std::chrono::system_clock::now();
    Position pos2(42.3602, -71.0590, now);
    eq.recordPosition(pos2);
    EXPECT_FALSE(eq.isMoving());
    
    // Add third position with sufficient time and distance to be moving
    // Create a position 100 meters away, 10 seconds later
    auto later = now + std::chrono::seconds(10);
    Position pos3(42.3610, -71.0600, later); // Approximately 100m away
    eq.recordPosition(pos3);
    
    // Should be moving (speed > threshold)
    EXPECT_TRUE(eq.isMoving());
    
    // Add fourth position with insufficient movement
    auto much_later = later + std::chrono::seconds(100);
    Position pos4(42.3611, -71.0601, much_later); // Small movement over long time
    eq.recordPosition(pos4);
    
    // Should not be moving (speed < threshold)
    EXPECT_FALSE(eq.isMoving());
}

TEST_F(EquipmentTest, ToStringFormatsCorrectly) {
    // Test different equipment types and statuses
    Equipment eq1("EQ444", EquipmentType::Forklift, "Forklift Test");
    eq1.setStatus(EquipmentStatus::Active);
    EXPECT_THAT(eq1.toString(), ::testing::HasSubstr("id=EQ444"));
    EXPECT_THAT(eq1.toString(), ::testing::HasSubstr("name=Forklift Test"));
    EXPECT_THAT(eq1.toString(), ::testing::HasSubstr("type=Forklift"));
    EXPECT_THAT(eq1.toString(), ::testing::HasSubstr("status=Active"));
    
    Equipment eq2("EQ555", EquipmentType::Crane, "Crane Test");
    eq2.setStatus(EquipmentStatus::Maintenance);
    EXPECT_THAT(eq2.toString(), ::testing::HasSubstr("type=Crane"));
    EXPECT_THAT(eq2.toString(), ::testing::HasSubstr("status=Maintenance"));
    
    Equipment eq3("EQ666", EquipmentType::Bulldozer, "Bulldozer Test");
    eq3.setStatus(EquipmentStatus::Inactive);
    EXPECT_THAT(eq3.toString(), ::testing::HasSubstr("type=Bulldozer"));
    EXPECT_THAT(eq3.toString(), ::testing::HasSubstr("status=Inactive"));
    
    Equipment eq4("EQ777", EquipmentType::Excavator, "Excavator Test");
    EXPECT_THAT(eq4.toString(), ::testing::HasSubstr("type=Excavator"));
    
    Equipment eq5("EQ888", EquipmentType::Truck, "Truck Test");
    EXPECT_THAT(eq5.toString(), ::testing::HasSubstr("type=Truck"));
    
    // Test unknown status
    Equipment eq6("EQ999", EquipmentType::Forklift, "Unknown Status");
    eq6.setStatus(static_cast<EquipmentStatus>(999)); // Invalid status
    EXPECT_THAT(eq6.toString(), ::testing::HasSubstr("status=Unknown"));
}

} // namespace equipment_tracker
// </test_code>