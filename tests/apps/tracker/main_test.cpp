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
    // Using a tolerance of 1 km for the test
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

TEST_F(PositionTest, InvalidLatitude) {
    Position position;
    
    // Latitude should be between -90 and 90
    position.setLatitude(100.0);
    EXPECT_DOUBLE_EQ(90.0, position.getLatitude());
    
    position.setLatitude(-100.0);
    EXPECT_DOUBLE_EQ(-90.0, position.getLatitude());
}

TEST_F(PositionTest, InvalidLongitude) {
    Position position;
    
    // Longitude should be between -180 and 180
    position.setLongitude(200.0);
    EXPECT_DOUBLE_EQ(180.0, position.getLongitude());
    
    position.setLongitude(-200.0);
    EXPECT_DOUBLE_EQ(-180.0, position.getLongitude());
}

// Equipment Tests
TEST_F(EquipmentTest, Constructor) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    EXPECT_EQ("FORKLIFT-001", forklift.getId());
    EXPECT_EQ(EquipmentType::Forklift, forklift.getType());
    EXPECT_EQ("Warehouse Forklift 1", forklift.getName());
    EXPECT_FALSE(forklift.getLastPosition().has_value());
}

TEST_F(EquipmentTest, SetAndGetPosition) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    Position position(37.7749, -122.4194, 10.0);
    
    forklift.setLastPosition(position);
    
    EXPECT_TRUE(forklift.getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(37.7749, forklift.getLastPosition()->getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194, forklift.getLastPosition()->getLongitude());
    EXPECT_DOUBLE_EQ(10.0, forklift.getLastPosition()->getAltitude());
}

TEST_F(EquipmentTest, RecordPosition) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    Position position(37.7749, -122.4194, 10.0);
    
    forklift.recordPosition(position);
    
    EXPECT_TRUE(forklift.getLastPosition().has_value());
    EXPECT_EQ(1, forklift.getPositionHistory().size());
}

TEST_F(EquipmentTest, PositionHistory) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    // Record multiple positions
    for (int i = 0; i < 5; ++i) {
        Position position(37.7749 + i * 0.001, -122.4194 + i * 0.002, 10.0 + i);
        forklift.recordPosition(position);
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Small delay to ensure different timestamps
    }
    
    auto history = forklift.getPositionHistory();
    EXPECT_EQ(5, history.size());
    
    // Check that positions are stored in chronological order
    for (size_t i = 1; i < history.size(); ++i) {
        EXPECT_GT(history[i].timestamp, history[i-1].timestamp);
    }
}

TEST_F(EquipmentTest, IsMoving) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    // No positions recorded yet
    EXPECT_FALSE(forklift.isMoving());
    
    // Record a position
    Position position1(37.7749, -122.4194, 10.0);
    forklift.recordPosition(position1);
    
    // Still not moving with only one position
    EXPECT_FALSE(forklift.isMoving());
    
    // Record a position far enough away to indicate movement
    Position position2(37.7849, -122.4294, 10.0);  // About 1.4 km away
    forklift.recordPosition(position2);
    
    // Should be considered moving now
    EXPECT_TRUE(forklift.isMoving());
    
    // Record a position very close to the last one
    Position position3(37.7849, -122.4294, 10.0);  // Same position
    forklift.recordPosition(position3);
    
    // Should not be considered moving anymore
    EXPECT_FALSE(forklift.isMoving());
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

TEST_F(EquipmentTest, SettersAndGetters) {
    Equipment equipment("CRANE-001", EquipmentType::Crane, "Tower Crane");
    
    equipment.setId("CRANE-002");
    equipment.setName("Mobile Crane");
    equipment.setType(EquipmentType::MobileCrane);
    
    EXPECT_EQ("CRANE-002", equipment.getId());
    EXPECT_EQ("Mobile Crane", equipment.getName());
    EXPECT_EQ(EquipmentType::MobileCrane, equipment.getType());
}

TEST_F(EquipmentTest, ClearPositionHistory) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    // Record multiple positions
    for (int i = 0; i < 5; ++i) {
        Position position(37.7749 + i * 0.001, -122.4194 + i * 0.002, 10.0 + i);
        forklift.recordPosition(position);
    }
    
    EXPECT_EQ(5, forklift.getPositionHistory().size());
    
    forklift.clearPositionHistory();
    
    EXPECT_EQ(0, forklift.getPositionHistory().size());
    EXPECT_FALSE(forklift.getLastPosition().has_value());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>