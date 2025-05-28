// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <chrono>
#include <thread>
#include "equipment_tracker/position.h"
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/utils/time_utils.h"

using namespace equipment_tracker;
using ::testing::HasSubstr;

class PositionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for position tests
    }
};

class EquipmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for equipment tests
    }
};

// Position Tests
TEST_F(PositionTest, DefaultConstructor) {
    Position position;
    EXPECT_DOUBLE_EQ(0.0, position.getLatitude());
    EXPECT_DOUBLE_EQ(0.0, position.getLongitude());
    EXPECT_DOUBLE_EQ(0.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(0.0, position.getAccuracy());
}

TEST_F(PositionTest, ParameterizedConstructor) {
    Position position(37.7749, -122.4194, 10.0);
    EXPECT_DOUBLE_EQ(37.7749, position.getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194, position.getLongitude());
    EXPECT_DOUBLE_EQ(10.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(0.0, position.getAccuracy());  // Default accuracy
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

TEST_F(PositionTest, DistanceCalculation) {
    Position sanFrancisco(37.7749, -122.4194, 10.0);
    Position losAngeles(34.0522, -118.2437, 50.0);
    
    double distance = sanFrancisco.distanceTo(losAngeles);
    
    // The distance between SF and LA is approximately 559 km
    // Using a tolerance of 1 km for platform differences
    EXPECT_NEAR(559000.0, distance, 1000.0);
}

TEST_F(PositionTest, DistanceToSelf) {
    Position position(37.7749, -122.4194, 10.0);
    double distance = position.distanceTo(position);
    EXPECT_DOUBLE_EQ(0.0, distance);
}

TEST_F(PositionTest, ToString) {
    Position position(37.7749, -122.4194, 10.0, 2.5);
    std::string posStr = position.toString();
    
    EXPECT_THAT(posStr, HasSubstr("37.7749"));
    EXPECT_THAT(posStr, HasSubstr("-122.4194"));
    EXPECT_THAT(posStr, HasSubstr("10"));
    EXPECT_THAT(posStr, HasSubstr("2.5"));
}

TEST_F(PositionTest, SettersAndGetters) {
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

TEST_F(PositionTest, InvalidCoordinates) {
    // Test with invalid latitude (out of range -90 to 90)
    Position position1 = Position::builder()
                             .withLatitude(100.0)
                             .withLongitude(-118.2437)
                             .build();
    
    // The implementation should clamp or handle invalid values
    EXPECT_LE(position1.getLatitude(), 90.0);
    EXPECT_GE(position1.getLatitude(), -90.0);
    
    // Test with invalid longitude (out of range -180 to 180)
    Position position2 = Position::builder()
                             .withLatitude(34.0522)
                             .withLongitude(200.0)
                             .build();
    
    // The implementation should clamp or handle invalid values
    EXPECT_LE(position2.getLongitude(), 180.0);
    EXPECT_GE(position2.getLongitude(), -180.0);
}

// Equipment Tests
TEST_F(EquipmentTest, Constructor) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    EXPECT_EQ("FORKLIFT-001", forklift.getId());
    EXPECT_EQ(EquipmentType::Forklift, forklift.getType());
    EXPECT_EQ("Warehouse Forklift 1", forklift.getName());
}

TEST_F(EquipmentTest, SetAndGetLastPosition) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    Position position(37.7749, -122.4194, 10.0);
    
    // Initially, there should be no position
    EXPECT_FALSE(forklift.getLastPosition().has_value());
    
    // Set position
    forklift.setLastPosition(position);
    
    // Check if position was set correctly
    auto lastPosition = forklift.getLastPosition();
    ASSERT_TRUE(lastPosition.has_value());
    EXPECT_DOUBLE_EQ(37.7749, lastPosition->getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194, lastPosition->getLongitude());
    EXPECT_DOUBLE_EQ(10.0, lastPosition->getAltitude());
}

TEST_F(EquipmentTest, RecordPosition) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    Position position(37.7749, -122.4194, 10.0);
    
    // Record position
    forklift.recordPosition(position);
    
    // Check if position was recorded
    auto lastPosition = forklift.getLastPosition();
    ASSERT_TRUE(lastPosition.has_value());
    EXPECT_DOUBLE_EQ(37.7749, lastPosition->getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194, lastPosition->getLongitude());
    EXPECT_DOUBLE_EQ(10.0, lastPosition->getAltitude());
    
    // Check position history
    auto history = forklift.getPositionHistory();
    EXPECT_EQ(1, history.size());
}

TEST_F(EquipmentTest, PositionHistory) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    // Record multiple positions
    for (int i = 0; i < 5; ++i) {
        Position position(37.7749 + i * 0.001, -122.4194 + i * 0.002, 10.0 + i);
        forklift.recordPosition(position);
        
        // Add a small delay to ensure different timestamps
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Check position history
    auto history = forklift.getPositionHistory();
    EXPECT_EQ(5, history.size());
    
    // Check that positions are stored in chronological order
    for (size_t i = 1; i < history.size(); ++i) {
        EXPECT_GT(history[i].timestamp, history[i-1].timestamp);
    }
    
    // Check the last position matches the last recorded position
    auto lastPosition = forklift.getLastPosition();
    ASSERT_TRUE(lastPosition.has_value());
    EXPECT_DOUBLE_EQ(37.7749 + 4 * 0.001, lastPosition->getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194 + 4 * 0.002, lastPosition->getLongitude());
    EXPECT_DOUBLE_EQ(10.0 + 4, lastPosition->getAltitude());
}

TEST_F(EquipmentTest, IsMoving) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    // Initially, equipment should not be moving (no positions)
    EXPECT_FALSE(forklift.isMoving());
    
    // Record a position
    Position position1(37.7749, -122.4194, 10.0);
    forklift.recordPosition(position1);
    
    // Still not moving (only one position)
    EXPECT_FALSE(forklift.isMoving());
    
    // Record a second position with significant distance
    Position position2(37.7849, -122.4294, 15.0);  // Moved ~1.4 km
    forklift.recordPosition(position2);
    
    // Now it should be moving
    EXPECT_TRUE(forklift.isMoving());
    
    // Record a third position with minimal movement
    Position position3(37.7849, -122.4294, 15.1);  // Only altitude changed slightly
    forklift.recordPosition(position3);
    
    // Depending on the implementation, this might be considered not moving
    // This test might need adjustment based on the actual implementation
}

TEST_F(EquipmentTest, ToString) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    Position position(37.7749, -122.4194, 10.0);
    forklift.setLastPosition(position);
    
    std::string equipStr = forklift.toString();
    
    EXPECT_THAT(equipStr, HasSubstr("FORKLIFT-001"));
    EXPECT_THAT(equipStr, HasSubstr("Warehouse Forklift 1"));
    EXPECT_THAT(equipStr, HasSubstr("Forklift"));  // Equipment type
}

TEST_F(EquipmentTest, SettersAndGetters) {
    Equipment equipment("CRANE-001", EquipmentType::Crane, "Tower Crane");
    
    equipment.setName("Mobile Crane");
    equipment.setType(EquipmentType::MobileCrane);
    
    EXPECT_EQ("CRANE-001", equipment.getId());  // ID should not change
    EXPECT_EQ("Mobile Crane", equipment.getName());
    EXPECT_EQ(EquipmentType::MobileCrane, equipment.getType());
}

// Main function that runs all the tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>