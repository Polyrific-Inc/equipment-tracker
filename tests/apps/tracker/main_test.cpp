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

TEST_F(PositionTest, ToString) {
    Position position(37.7749, -122.4194, 10.0);
    std::string posStr = position.toString();
    
    EXPECT_THAT(posStr, HasSubstr("37.7749"));
    EXPECT_THAT(posStr, HasSubstr("-122.4194"));
    EXPECT_THAT(posStr, HasSubstr("10"));
}

TEST_F(PositionTest, DistanceTo) {
    Position sf(37.7749, -122.4194, 10.0);
    Position la(34.0522, -118.2437, 50.0);
    
    double distance = sf.distanceTo(la);
    
    // The distance between SF and LA is approximately 559 km
    // Using a tolerance of 1 km due to different calculation methods
    EXPECT_NEAR(559000.0, distance, 1000.0);
}

TEST_F(PositionTest, DistanceToSamePosition) {
    Position position(37.7749, -122.4194, 10.0);
    double distance = position.distanceTo(position);
    EXPECT_DOUBLE_EQ(0.0, distance);
}

TEST_F(PositionTest, DistanceWithInvalidCoordinates) {
    Position position1(200.0, -122.4194, 10.0);  // Invalid latitude
    Position position2(37.7749, -200.0, 10.0);   // Invalid longitude
    Position valid(37.7749, -122.4194, 10.0);
    
    // Expect reasonable behavior with invalid coordinates
    double distance1 = position1.distanceTo(valid);
    double distance2 = valid.distanceTo(position2);
    
    // The actual values might vary, but they should be finite
    EXPECT_FALSE(std::isnan(distance1));
    EXPECT_FALSE(std::isnan(distance2));
}

// Equipment Tests
TEST_F(EquipmentTest, Constructor) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    EXPECT_EQ("FORKLIFT-001", forklift.getId());
    EXPECT_EQ(EquipmentType::Forklift, forklift.getType());
    EXPECT_EQ("Warehouse Forklift 1", forklift.getDescription());
}

TEST_F(EquipmentTest, SetAndGetLastPosition) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    Position position(37.7749, -122.4194, 10.0);
    
    // Initially, no position
    EXPECT_FALSE(forklift.getLastPosition().has_value());
    
    // Set position
    forklift.setLastPosition(position);
    
    // Check if position is set correctly
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
    
    // Check if position is recorded
    auto lastPosition = forklift.getLastPosition();
    ASSERT_TRUE(lastPosition.has_value());
    EXPECT_DOUBLE_EQ(37.7749, lastPosition->getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194, lastPosition->getLongitude());
    EXPECT_DOUBLE_EQ(10.0, lastPosition->getAltitude());
    
    // Check position history
    auto history = forklift.getPositionHistory();
    EXPECT_EQ(1, history.size());
}

TEST_F(EquipmentTest, RecordMultiplePositions) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    // Record multiple positions
    for (int i = 0; i < 5; ++i) {
        Position position(37.7749 + i * 0.001, -122.4194 + i * 0.002, 10.0 + i);
        forklift.recordPosition(position);
        
        // Small delay to ensure different timestamps
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Check position history
    auto history = forklift.getPositionHistory();
    EXPECT_EQ(5, history.size());
    
    // Check last position
    auto lastPosition = forklift.getLastPosition();
    ASSERT_TRUE(lastPosition.has_value());
    EXPECT_DOUBLE_EQ(37.7749 + 4 * 0.001, lastPosition->getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194 + 4 * 0.002, lastPosition->getLongitude());
    EXPECT_DOUBLE_EQ(10.0 + 4, lastPosition->getAltitude());
}

TEST_F(EquipmentTest, IsMoving) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    // Initially, not moving (no positions)
    EXPECT_FALSE(forklift.isMoving());
    
    // Record a position
    Position position1(37.7749, -122.4194, 10.0);
    forklift.recordPosition(position1);
    
    // Still not moving (only one position)
    EXPECT_FALSE(forklift.isMoving());
    
    // Record another position with significant distance
    Position position2(37.7849, -122.4294, 10.0);  // About 1.4 km away
    forklift.recordPosition(position2);
    
    // Now it should be moving
    EXPECT_TRUE(forklift.isMoving());
    
    // Record another position at the same location
    forklift.recordPosition(position2);
    
    // Should still be considered moving based on history
    EXPECT_TRUE(forklift.isMoving());
}

TEST_F(EquipmentTest, ToString) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    Position position(37.7749, -122.4194, 10.0);
    forklift.setLastPosition(position);
    
    std::string equipStr = forklift.toString();
    
    EXPECT_THAT(equipStr, HasSubstr("FORKLIFT-001"));
    EXPECT_THAT(equipStr, HasSubstr("Warehouse Forklift 1"));
    EXPECT_THAT(equipStr, HasSubstr("Forklift"));
}

TEST_F(EquipmentTest, GetPositionHistory) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    // Initially, history should be empty
    EXPECT_TRUE(forklift.getPositionHistory().empty());
    
    // Record positions
    Position position1(37.7749, -122.4194, 10.0);
    Position position2(37.7750, -122.4195, 11.0);
    
    forklift.recordPosition(position1);
    forklift.recordPosition(position2);
    
    // Check history
    auto history = forklift.getPositionHistory();
    EXPECT_EQ(2, history.size());
    
    // Check order (newest first or oldest first, depending on implementation)
    // This test assumes newest first, adjust if needed
    EXPECT_DOUBLE_EQ(37.7750, history[0].getLatitude());
    EXPECT_DOUBLE_EQ(37.7749, history[1].getLatitude());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>