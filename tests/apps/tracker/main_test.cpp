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
    const double LATITUDE_SF = 37.7749;
    const double LONGITUDE_SF = -122.4194;
    const double ALTITUDE_SF = 10.0;
    
    const double LATITUDE_LA = 34.0522;
    const double LONGITUDE_LA = -118.2437;
    const double ALTITUDE_LA = 50.0;
    const double ACCURACY_LA = 1.5;
    
    // Tolerance for floating point comparisons
    const double EPSILON = 0.0001;
};

TEST_F(PositionTest, ConstructorSetsCorrectValues) {
    Position position(LATITUDE_SF, LONGITUDE_SF, ALTITUDE_SF);
    
    EXPECT_NEAR(LATITUDE_SF, position.getLatitude(), EPSILON);
    EXPECT_NEAR(LONGITUDE_SF, position.getLongitude(), EPSILON);
    EXPECT_NEAR(ALTITUDE_SF, position.getAltitude(), EPSILON);
    EXPECT_NEAR(0.0, position.getAccuracy(), EPSILON); // Default accuracy
}

TEST_F(PositionTest, BuilderPatternSetsCorrectValues) {
    Position position = Position::builder()
                            .withLatitude(LATITUDE_LA)
                            .withLongitude(LONGITUDE_LA)
                            .withAltitude(ALTITUDE_LA)
                            .withAccuracy(ACCURACY_LA)
                            .build();
    
    EXPECT_NEAR(LATITUDE_LA, position.getLatitude(), EPSILON);
    EXPECT_NEAR(LONGITUDE_LA, position.getLongitude(), EPSILON);
    EXPECT_NEAR(ALTITUDE_LA, position.getAltitude(), EPSILON);
    EXPECT_NEAR(ACCURACY_LA, position.getAccuracy(), EPSILON);
}

TEST_F(PositionTest, ToStringContainsCoordinates) {
    Position position(LATITUDE_SF, LONGITUDE_SF, ALTITUDE_SF);
    std::string positionStr = position.toString();
    
    EXPECT_THAT(positionStr, HasSubstr(std::to_string(LATITUDE_SF)));
    EXPECT_THAT(positionStr, HasSubstr(std::to_string(LONGITUDE_SF)));
    EXPECT_THAT(positionStr, HasSubstr(std::to_string(ALTITUDE_SF)));
}

TEST_F(PositionTest, DistanceCalculationIsAccurate) {
    Position sanFrancisco(LATITUDE_SF, LONGITUDE_SF, ALTITUDE_SF);
    Position losAngeles(LATITUDE_LA, LONGITUDE_LA, ALTITUDE_LA);
    
    double distance = sanFrancisco.distanceTo(losAngeles);
    
    // The distance between SF and LA is approximately 559 km
    // But we'll use a wider tolerance due to different calculation methods
    const double EXPECTED_DISTANCE_KM = 559.0;
    EXPECT_NEAR(EXPECTED_DISTANCE_KM * 1000, distance, 10000); // 10km tolerance
}

TEST_F(PositionTest, DistanceToSelfIsZero) {
    Position position(LATITUDE_SF, LONGITUDE_SF, ALTITUDE_SF);
    double distance = position.distanceTo(position);
    EXPECT_NEAR(0.0, distance, EPSILON);
}

class EquipmentTest : public ::testing::Test {
protected:
    const std::string EQUIPMENT_ID = "FORKLIFT-001";
    const EquipmentType EQUIPMENT_TYPE = EquipmentType::Forklift;
    const std::string EQUIPMENT_NAME = "Warehouse Forklift 1";
    
    Position testPosition{37.7749, -122.4194, 10.0};
    
    void SetUp() override {
        equipment = std::make_unique<Equipment>(EQUIPMENT_ID, EQUIPMENT_TYPE, EQUIPMENT_NAME);
    }
    
    std::unique_ptr<Equipment> equipment;
};

TEST_F(EquipmentTest, ConstructorSetsCorrectValues) {
    EXPECT_EQ(EQUIPMENT_ID, equipment->getId());
    EXPECT_EQ(EQUIPMENT_TYPE, equipment->getType());
    EXPECT_EQ(EQUIPMENT_NAME, equipment->getName());
    EXPECT_FALSE(equipment->getLastPosition().has_value());
}

TEST_F(EquipmentTest, SetLastPositionUpdatesPosition) {
    equipment->setLastPosition(testPosition);
    
    auto lastPosition = equipment->getLastPosition();
    ASSERT_TRUE(lastPosition.has_value());
    EXPECT_NEAR(testPosition.getLatitude(), lastPosition->getLatitude(), 0.0001);
    EXPECT_NEAR(testPosition.getLongitude(), lastPosition->getLongitude(), 0.0001);
}

TEST_F(EquipmentTest, ToStringContainsEquipmentInfo) {
    std::string equipmentStr = equipment->toString();
    
    EXPECT_THAT(equipmentStr, HasSubstr(EQUIPMENT_ID));
    EXPECT_THAT(equipmentStr, HasSubstr(EQUIPMENT_NAME));
    // Should check for equipment type string representation
}

TEST_F(EquipmentTest, RecordPositionAddsToHistory) {
    // Record a few positions
    for (int i = 0; i < 3; ++i) {
        Position pos(37.7749 + i * 0.001, -122.4194 + i * 0.002, 10.0 + i);
        equipment->recordPosition(pos);
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Ensure timestamps differ
    }
    
    auto history = equipment->getPositionHistory();
    EXPECT_EQ(3, history.size());
    
    // Check that positions are stored in chronological order
    for (size_t i = 1; i < history.size(); ++i) {
        EXPECT_TRUE(history[i].timestamp > history[i-1].timestamp);
    }
}

TEST_F(EquipmentTest, IsMovingDetectsMovement) {
    // Record stationary positions
    for (int i = 0; i < 3; ++i) {
        equipment->recordPosition(testPosition);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_FALSE(equipment->isMoving());
    
    // Record moving positions
    for (int i = 0; i < 3; ++i) {
        Position pos(37.7749 + i * 0.01, -122.4194 + i * 0.01, 10.0);
        equipment->recordPosition(pos);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(equipment->isMoving());
}

TEST_F(EquipmentTest, EmptyHistoryIsNotMoving) {
    EXPECT_FALSE(equipment->isMoving());
}

TEST_F(EquipmentTest, SinglePositionIsNotMoving) {
    equipment->recordPosition(testPosition);
    EXPECT_FALSE(equipment->isMoving());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>