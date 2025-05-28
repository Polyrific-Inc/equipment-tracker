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
    // Using a tolerance of 1 km due to different calculation methods
    EXPECT_NEAR(559000.0, distance, 1000.0);
}

TEST_F(PositionTest, ToStringContainsCoordinates) {
    Position position(37.7749, -122.4194, 10.0);
    std::string posStr = position.toString();
    
    EXPECT_THAT(posStr, HasSubstr("37.7749"));
    EXPECT_THAT(posStr, HasSubstr("-122.4194"));
    EXPECT_THAT(posStr, HasSubstr("10"));
}

TEST_F(PositionTest, InvalidCoordinates) {
    // Test with invalid latitude (outside -90 to 90 range)
    Position invalidLat = Position::builder()
                              .withLatitude(100.0)
                              .withLongitude(0.0)
                              .build();
    
    // Test with invalid longitude (outside -180 to 180 range)
    Position invalidLon = Position::builder()
                              .withLatitude(0.0)
                              .withLongitude(200.0)
                              .build();
    
    // Check if the implementation handles invalid coordinates
    // This depends on the actual implementation - might clamp or reject
    EXPECT_TRUE(invalidLat.getLatitude() <= 90.0 && invalidLat.getLatitude() >= -90.0);
    EXPECT_TRUE(invalidLon.getLongitude() <= 180.0 && invalidLon.getLongitude() >= -180.0);
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
    Position position1(37.7749, -122.4194, 10.0);
    Position position2(37.7750, -122.4195, 11.0);
    
    // Record positions
    forklift.recordPosition(position1);
    forklift.recordPosition(position2);
    
    // Check last position
    auto lastPosition = forklift.getLastPosition();
    ASSERT_TRUE(lastPosition.has_value());
    EXPECT_DOUBLE_EQ(37.7750, lastPosition->getLatitude());
    EXPECT_DOUBLE_EQ(-122.4195, lastPosition->getLongitude());
    
    // Check position history
    auto history = forklift.getPositionHistory();
    EXPECT_EQ(2, history.size());
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
    Position position2(37.7759, -122.4204, 11.0);  // ~100m away
    forklift.recordPosition(position2);
    
    // Now it should be moving
    EXPECT_TRUE(forklift.isMoving());
    
    // Wait for movement timeout (if implemented)
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Record same position again (no movement)
    forklift.recordPosition(position2);
    
    // Might still be moving depending on implementation
    // This depends on how isMoving() is implemented (time-based or just position change)
}

TEST_F(EquipmentTest, ToStringContainsInfo) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    std::string equipStr = forklift.toString();
    
    EXPECT_THAT(equipStr, HasSubstr("FORKLIFT-001"));
    EXPECT_THAT(equipStr, HasSubstr("Warehouse Forklift 1"));
    // Should also contain type information
    EXPECT_THAT(equipStr, HasSubstr("Forklift"));
}

TEST_F(EquipmentTest, PositionHistoryLimit) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    // Record many positions to test if there's a history limit
    for (int i = 0; i < 100; ++i) {
        Position pos(37.7749 + i * 0.001, -122.4194 + i * 0.001, 10.0 + i);
        forklift.recordPosition(pos);
    }
    
    // Check history size - this depends on implementation
    // Most implementations would have some limit to prevent unlimited memory usage
    auto history = forklift.getPositionHistory();
    EXPECT_GT(history.size(), 0);
    
    // If there's a known limit, we could test for that specific value
    // EXPECT_EQ(expectedLimit, history.size());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>