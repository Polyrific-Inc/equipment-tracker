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
using namespace testing;

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

TEST_F(PositionTest, ConstructorSetsCorrectValues) {
    const double lat = 37.7749;
    const double lon = -122.4194;
    const double alt = 10.0;
    
    Position position(lat, lon, alt);
    
    EXPECT_DOUBLE_EQ(position.getLatitude(), lat);
    EXPECT_DOUBLE_EQ(position.getLongitude(), lon);
    EXPECT_DOUBLE_EQ(position.getAltitude(), alt);
    EXPECT_DOUBLE_EQ(position.getAccuracy(), 0.0); // Default accuracy
}

TEST_F(PositionTest, BuilderPatternSetsCorrectValues) {
    const double lat = 34.0522;
    const double lon = -118.2437;
    const double alt = 50.0;
    const double accuracy = 1.5;
    
    Position position = Position::builder()
                            .withLatitude(lat)
                            .withLongitude(lon)
                            .withAltitude(alt)
                            .withAccuracy(accuracy)
                            .build();
    
    EXPECT_DOUBLE_EQ(position.getLatitude(), lat);
    EXPECT_DOUBLE_EQ(position.getLongitude(), lon);
    EXPECT_DOUBLE_EQ(position.getAltitude(), alt);
    EXPECT_DOUBLE_EQ(position.getAccuracy(), accuracy);
}

TEST_F(PositionTest, ToStringContainsCoordinates) {
    Position position(37.7749, -122.4194, 10.0);
    
    std::string posStr = position.toString();
    
    EXPECT_THAT(posStr, HasSubstr("37.7749"));
    EXPECT_THAT(posStr, HasSubstr("-122.4194"));
    EXPECT_THAT(posStr, HasSubstr("10"));
}

TEST_F(PositionTest, DistanceCalculationIsAccurate) {
    // San Francisco
    Position sf(37.7749, -122.4194, 10.0);
    // Los Angeles
    Position la(34.0522, -118.2437, 50.0);
    
    double distance = sf.distanceTo(la);
    
    // The distance between SF and LA is approximately 559 km
    // Due to different implementations of haversine formula and floating-point precision,
    // we use a reasonable tolerance
    EXPECT_NEAR(distance / 1000.0, 559.0, 10.0);
}

TEST_F(PositionTest, DistanceToSelfIsZero) {
    Position position(37.7749, -122.4194, 10.0);
    
    double distance = position.distanceTo(position);
    
    EXPECT_NEAR(distance, 0.0, 0.001);
}

TEST_F(PositionTest, DistanceWithNorthPole) {
    Position sf(37.7749, -122.4194, 10.0);
    Position northPole(90.0, 0.0, 0.0);
    
    double distance = sf.distanceTo(northPole);
    
    // Distance from SF to North Pole is approximately 5,765 km
    EXPECT_NEAR(distance / 1000.0, 5765.0, 100.0);
}

TEST_F(EquipmentTest, ConstructorSetsCorrectValues) {
    const std::string id = "FORKLIFT-001";
    const EquipmentType type = EquipmentType::Forklift;
    const std::string description = "Warehouse Forklift 1";
    
    Equipment equipment(id, type, description);
    
    EXPECT_EQ(equipment.getId(), id);
    EXPECT_EQ(equipment.getType(), type);
    EXPECT_EQ(equipment.getDescription(), description);
    EXPECT_EQ(equipment.getLastPosition(), nullptr);
}

TEST_F(EquipmentTest, SetAndGetLastPosition) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    Position position(37.7749, -122.4194, 10.0);
    
    forklift.setLastPosition(position);
    auto lastPosition = forklift.getLastPosition();
    
    ASSERT_NE(lastPosition, nullptr);
    EXPECT_DOUBLE_EQ(lastPosition->getLatitude(), position.getLatitude());
    EXPECT_DOUBLE_EQ(lastPosition->getLongitude(), position.getLongitude());
    EXPECT_DOUBLE_EQ(lastPosition->getAltitude(), position.getAltitude());
}

TEST_F(EquipmentTest, RecordPositionAddsToHistory) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    Position position1(37.7749, -122.4194, 10.0);
    Position position2(37.7750, -122.4195, 11.0);
    
    forklift.recordPosition(position1);
    forklift.recordPosition(position2);
    
    auto history = forklift.getPositionHistory();
    
    EXPECT_EQ(history.size(), 2);
    
    // Check last position is updated
    auto lastPosition = forklift.getLastPosition();
    ASSERT_NE(lastPosition, nullptr);
    EXPECT_DOUBLE_EQ(lastPosition->getLatitude(), position2.getLatitude());
}

TEST_F(EquipmentTest, IsMovingReturnsTrueWhenMoving) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    // Record positions with significant distance between them
    Position position1(37.7749, -122.4194, 10.0);
    // Wait to ensure timestamps are different
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Record first position
    forklift.recordPosition(position1);
    
    // Wait to ensure timestamps are different
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Record second position with significant movement
    Position position2(37.7849, -122.4294, 10.0); // Moved ~1.1 km
    forklift.recordPosition(position2);
    
    EXPECT_TRUE(forklift.isMoving());
}

TEST_F(EquipmentTest, IsMovingReturnsFalseWhenStationary) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    // Record positions with minimal distance between them
    Position position1(37.7749, -122.4194, 10.0);
    // Wait to ensure timestamps are different
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Record first position
    forklift.recordPosition(position1);
    
    // Wait to ensure timestamps are different
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Record second position with minimal movement
    Position position2(37.77491, -122.41941, 10.0); // Moved very little
    forklift.recordPosition(position2);
    
    EXPECT_FALSE(forklift.isMoving());
}

TEST_F(EquipmentTest, ToStringContainsEquipmentInfo) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    std::string equipStr = forklift.toString();
    
    EXPECT_THAT(equipStr, HasSubstr("FORKLIFT-001"));
    EXPECT_THAT(equipStr, HasSubstr("Forklift"));
    EXPECT_THAT(equipStr, HasSubstr("Warehouse Forklift 1"));
}

TEST_F(EquipmentTest, PositionHistoryMaintainsOrder) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    // Record multiple positions
    for (int i = 0; i < 5; ++i) {
        Position pos(37.7749 + i * 0.001, -122.4194 + i * 0.002, 10.0 + i);
        forklift.recordPosition(pos);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    auto history = forklift.getPositionHistory();
    
    EXPECT_EQ(history.size(), 5);
    
    // Check positions are in chronological order (newest first or oldest first, depending on implementation)
    // This test assumes newest first, adjust if your implementation differs
    for (size_t i = 1; i < history.size(); ++i) {
        EXPECT_GE(history[i-1].timestamp, history[i].timestamp);
    }
}

TEST_F(EquipmentTest, EmptyPositionHistoryWhenNoPositionsRecorded) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    auto history = forklift.getPositionHistory();
    
    EXPECT_TRUE(history.empty());
}

// Additional tests for edge cases

TEST_F(PositionTest, DistanceWithInvalidCoordinates) {
    Position validPos(37.7749, -122.4194, 10.0);
    Position invalidLat(91.0, -122.4194, 10.0); // Invalid latitude > 90
    Position invalidLon(37.7749, -181.0, 10.0); // Invalid longitude < -180
    
    // Depending on implementation, this might throw an exception or return a special value
    // Adjust the test according to your implementation
    EXPECT_NO_THROW(validPos.distanceTo(invalidLat));
    EXPECT_NO_THROW(validPos.distanceTo(invalidLon));
}

TEST_F(EquipmentTest, RecordPositionWithSameTimestamp) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    // Create two positions
    Position pos1(37.7749, -122.4194, 10.0);
    Position pos2(37.7750, -122.4195, 11.0);
    
    // Record both positions
    forklift.recordPosition(pos1);
    forklift.recordPosition(pos2);
    
    auto history = forklift.getPositionHistory();
    
    // Expect both positions to be recorded, even if they have similar timestamps
    EXPECT_EQ(history.size(), 2);
}
// </test_code>