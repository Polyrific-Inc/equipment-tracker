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
    // We use a tolerance because the exact calculation may vary slightly by platform
    EXPECT_NEAR(559000.0, distance, 5000.0);  // Within 5km tolerance
}

TEST_F(PositionTest, ToString) {
    Position position(37.7749, -122.4194, 10.0, 2.5);
    std::string posStr = position.toString();
    
    EXPECT_THAT(posStr, HasSubstr("37.7749"));
    EXPECT_THAT(posStr, HasSubstr("-122.4194"));
    EXPECT_THAT(posStr, HasSubstr("10"));
    EXPECT_THAT(posStr, HasSubstr("2.5"));
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
    // This depends on how the Position class is implemented
    // It might clamp values, throw exceptions, or have other behavior
    EXPECT_TRUE(invalidLat.getLatitude() <= 90.0 && invalidLat.getLatitude() >= -90.0);
    EXPECT_TRUE(invalidLon.getLongitude() <= 180.0 && invalidLon.getLongitude() >= -180.0);
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
    
    ASSERT_TRUE(forklift.getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(37.7749, forklift.getLastPosition()->getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194, forklift.getLastPosition()->getLongitude());
    EXPECT_DOUBLE_EQ(10.0, forklift.getLastPosition()->getAltitude());
}

TEST_F(EquipmentTest, RecordPosition) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    Position position1(37.7749, -122.4194, 10.0);
    Position position2(37.7750, -122.4195, 11.0);
    
    forklift.recordPosition(position1);
    forklift.recordPosition(position2);
    
    auto history = forklift.getPositionHistory();
    EXPECT_EQ(2, history.size());
    
    ASSERT_TRUE(forklift.getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(37.7750, forklift.getLastPosition()->getLatitude());
    EXPECT_DOUBLE_EQ(-122.4195, forklift.getLastPosition()->getLongitude());
}

TEST_F(EquipmentTest, IsMoving) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    // Record positions that are close together
    Position position1(37.7749, -122.4194, 10.0);
    Position position2(37.7749, -122.4194, 10.0);  // Same position
    
    forklift.recordPosition(position1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    forklift.recordPosition(position2);
    
    // Should not be moving since positions are identical
    EXPECT_FALSE(forklift.isMoving());
    
    // Record a position that's significantly different
    Position position3(37.7759, -122.4204, 15.0);  // Moved position
    forklift.recordPosition(position3);
    
    // Should be moving now
    EXPECT_TRUE(forklift.isMoving());
}

TEST_F(EquipmentTest, ToString) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    Position position(37.7749, -122.4194, 10.0);
    forklift.setLastPosition(position);
    
    std::string equipStr = forklift.toString();
    
    EXPECT_THAT(equipStr, HasSubstr("FORKLIFT-001"));
    EXPECT_THAT(equipStr, HasSubstr("Warehouse Forklift 1"));
    // The string representation might include the equipment type and position
    // depending on the implementation
}

TEST_F(EquipmentTest, PositionHistory) {
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    
    // Record multiple positions
    for (int i = 0; i < 5; ++i) {
        Position newPos(
            37.7749 + i * 0.001,
            -122.4194 + i * 0.002,
            10.0 + i);
        forklift.recordPosition(newPos);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    auto history = forklift.getPositionHistory();
    EXPECT_EQ(5, history.size());
    
    // Check that positions are stored in chronological order
    for (size_t i = 1; i < history.size(); ++i) {
        EXPECT_TRUE(history[i].timestamp > history[i-1].timestamp);
    }
    
    // Check the first and last positions
    EXPECT_DOUBLE_EQ(37.7749, history[0].position.getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194, history[0].position.getLongitude());
    
    EXPECT_DOUBLE_EQ(37.7749 + 4 * 0.001, history[4].position.getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194 + 4 * 0.002, history[4].position.getLongitude());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>