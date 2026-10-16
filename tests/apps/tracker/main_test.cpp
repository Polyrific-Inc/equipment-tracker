// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/position.h"
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/utils/time_utils.h"
#include <chrono>
#include <thread>
#include <cmath>

namespace equipment_tracker {
namespace {

// Constants for testing
constexpr double LATITUDE_SF = 37.7749;
constexpr double LONGITUDE_SF = -122.4194;
constexpr double LATITUDE_LA = 34.0522;
constexpr double LONGITUDE_LA = -118.2437;
constexpr double ALTITUDE_1 = 10.0;
constexpr double ALTITUDE_2 = 50.0;
constexpr double ACCURACY_1 = 2.5; // Default accuracy
constexpr double ACCURACY_2 = 1.5;
constexpr double DISTANCE_TOLERANCE = 1000.0; // 1km tolerance for distance calculations
constexpr double EXPECTED_SF_LA_DISTANCE = 559000.0; // ~559km between SF and LA

class PositionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test positions
        position_sf_ = Position(LATITUDE_SF, LONGITUDE_SF, ALTITUDE_1);
        position_la_ = Position::builder()
                           .withLatitude(LATITUDE_LA)
                           .withLongitude(LONGITUDE_LA)
                           .withAltitude(ALTITUDE_2)
                           .withAccuracy(ACCURACY_2)
                           .build();
    }

    Position position_sf_;
    Position position_la_;
};

TEST_F(PositionTest, DefaultConstructor) {
    Position position;
    EXPECT_DOUBLE_EQ(0.0, position.getLatitude());
    EXPECT_DOUBLE_EQ(0.0, position.getLongitude());
    EXPECT_DOUBLE_EQ(0.0, position.getAltitude());
    EXPECT_DOUBLE_EQ(DEFAULT_POSITION_ACCURACY, position.getAccuracy());
    // Timestamp should be close to now
    auto now = getCurrentTimestamp();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(
        now - position.getTimestamp()).count();
    EXPECT_LT(std::abs(diff), 2); // Within 2 seconds
}

TEST_F(PositionTest, ParameterizedConstructor) {
    EXPECT_DOUBLE_EQ(LATITUDE_SF, position_sf_.getLatitude());
    EXPECT_DOUBLE_EQ(LONGITUDE_SF, position_sf_.getLongitude());
    EXPECT_DOUBLE_EQ(ALTITUDE_1, position_sf_.getAltitude());
    EXPECT_DOUBLE_EQ(DEFAULT_POSITION_ACCURACY, position_sf_.getAccuracy());
}

TEST_F(PositionTest, BuilderPattern) {
    EXPECT_DOUBLE_EQ(LATITUDE_LA, position_la_.getLatitude());
    EXPECT_DOUBLE_EQ(LONGITUDE_LA, position_la_.getLongitude());
    EXPECT_DOUBLE_EQ(ALTITUDE_2, position_la_.getAltitude());
    EXPECT_DOUBLE_EQ(ACCURACY_2, position_la_.getAccuracy());
}

TEST_F(PositionTest, Setters) {
    Position position;
    position.setLatitude(LATITUDE_SF);
    position.setLongitude(LONGITUDE_SF);
    position.setAltitude(ALTITUDE_1);
    position.setAccuracy(ACCURACY_2);
    
    Timestamp custom_time = getCurrentTimestamp() - std::chrono::hours(1);
    position.setTimestamp(custom_time);
    
    EXPECT_DOUBLE_EQ(LATITUDE_SF, position.getLatitude());
    EXPECT_DOUBLE_EQ(LONGITUDE_SF, position.getLongitude());
    EXPECT_DOUBLE_EQ(ALTITUDE_1, position.getAltitude());
    EXPECT_DOUBLE_EQ(ACCURACY_2, position.getAccuracy());
    EXPECT_EQ(custom_time, position.getTimestamp());
}

TEST_F(PositionTest, DistanceCalculation) {
    double distance = position_sf_.distanceTo(position_la_);
    EXPECT_NEAR(EXPECTED_SF_LA_DISTANCE, distance, DISTANCE_TOLERANCE);
    
    // Distance should be symmetric
    double reverse_distance = position_la_.distanceTo(position_sf_);
    EXPECT_DOUBLE_EQ(distance, reverse_distance);
    
    // Distance to self should be 0
    EXPECT_DOUBLE_EQ(0.0, position_sf_.distanceTo(position_sf_));
}

TEST_F(PositionTest, ToStringOutput) {
    std::string sf_string = position_sf_.toString();
    EXPECT_THAT(sf_string, ::testing::HasSubstr(std::to_string(LATITUDE_SF)));
    EXPECT_THAT(sf_string, ::testing::HasSubstr(std::to_string(LONGITUDE_SF)));
    EXPECT_THAT(sf_string, ::testing::HasSubstr(std::to_string(ALTITUDE_1)));
}

class EquipmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        forklift_ = std::make_unique<Equipment>("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
        position_sf_ = Position(LATITUDE_SF, LONGITUDE_SF, ALTITUDE_1);
        position_la_ = Position(LATITUDE_LA, LONGITUDE_LA, ALTITUDE_2);
    }

    std::unique_ptr<Equipment> forklift_;
    Position position_sf_;
    Position position_la_;
};

TEST_F(EquipmentTest, Constructor) {
    EXPECT_EQ("FORKLIFT-001", forklift_->getId());
    EXPECT_EQ(EquipmentType::Forklift, forklift_->getType());
    EXPECT_EQ("Warehouse Forklift 1", forklift_->getName());
    EXPECT_EQ(EquipmentStatus::Active, forklift_->getStatus()); // Default status
    EXPECT_FALSE(forklift_->getLastPosition().has_value()); // No position initially
}

TEST_F(EquipmentTest, CopyConstructorAndAssignment) {
    forklift_->setLastPosition(position_sf_);
    
    // Test copy constructor
    Equipment forklift_copy(*forklift_);
    EXPECT_EQ(forklift_->getId(), forklift_copy.getId());
    EXPECT_EQ(forklift_->getType(), forklift_copy.getType());
    EXPECT_EQ(forklift_->getName(), forklift_copy.getName());
    EXPECT_EQ(forklift_->getStatus(), forklift_copy.getStatus());
    
    auto original_pos = forklift_->getLastPosition();
    auto copy_pos = forklift_copy.getLastPosition();
    ASSERT_TRUE(original_pos.has_value());
    ASSERT_TRUE(copy_pos.has_value());
    EXPECT_DOUBLE_EQ(original_pos->getLatitude(), copy_pos->getLatitude());
    
    // Test copy assignment
    Equipment forklift_assign("OTHER-001", EquipmentType::Crane, "Other Equipment");
    forklift_assign = *forklift_;
    EXPECT_EQ(forklift_->getId(), forklift_assign.getId());
    EXPECT_EQ(forklift_->getType(), forklift_assign.getType());
}

TEST_F(EquipmentTest, MoveConstructorAndAssignment) {
    forklift_->setLastPosition(position_sf_);
    
    // Test move constructor
    Equipment forklift_move(std::move(*forklift_));
    EXPECT_EQ("FORKLIFT-001", forklift_move.getId());
    EXPECT_EQ(EquipmentType::Forklift, forklift_move.getType());
    EXPECT_EQ("Warehouse Forklift 1", forklift_move.getName());
    
    auto move_pos = forklift_move.getLastPosition();
    ASSERT_TRUE(move_pos.has_value());
    EXPECT_DOUBLE_EQ(LATITUDE_SF, move_pos->getLatitude());
    
    // Recreate forklift_ since it was moved from
    forklift_ = std::make_unique<Equipment>("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");
    forklift_->setLastPosition(position_sf_);
    
    // Test move assignment
    Equipment forklift_move_assign("OTHER-001", EquipmentType::Crane, "Other Equipment");
    forklift_move_assign = std::move(*forklift_);
    EXPECT_EQ("FORKLIFT-001", forklift_move_assign.getId());
    EXPECT_EQ(EquipmentType::Forklift, forklift_move_assign.getType());
}

TEST_F(EquipmentTest, SettersAndGetters) {
    // Test setters
    forklift_->setName("Updated Forklift Name");
    forklift_->setStatus(EquipmentStatus::Maintenance);
    forklift_->setLastPosition(position_sf_);
    
    // Test getters
    EXPECT_EQ("Updated Forklift Name", forklift_->getName());
    EXPECT_EQ(EquipmentStatus::Maintenance, forklift_->getStatus());
    
    auto position = forklift_->getLastPosition();
    ASSERT_TRUE(position.has_value());
    EXPECT_DOUBLE_EQ(LATITUDE_SF, position->getLatitude());
    EXPECT_DOUBLE_EQ(LONGITUDE_SF, position->getLongitude());
    EXPECT_DOUBLE_EQ(ALTITUDE_1, position->getAltitude());
}

TEST_F(EquipmentTest, PositionHistory) {
    // Initially empty
    EXPECT_TRUE(forklift_->getPositionHistory().empty());
    
    // Record positions
    forklift_->recordPosition(position_sf_);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    forklift_->recordPosition(position_la_);
    
    // Check history
    auto history = forklift_->getPositionHistory();
    EXPECT_EQ(2, history.size());
    EXPECT_DOUBLE_EQ(LATITUDE_SF, history[0].getLatitude());
    EXPECT_DOUBLE_EQ(LATITUDE_LA, history[1].getLatitude());
    
    // Check last position is updated
    auto last_pos = forklift_->getLastPosition();
    ASSERT_TRUE(last_pos.has_value());
    EXPECT_DOUBLE_EQ(LATITUDE_LA, last_pos->getLatitude());
    
    // Clear history
    forklift_->clearPositionHistory();
    EXPECT_TRUE(forklift_->getPositionHistory().empty());
    
    // Last position should still be available
    last_pos = forklift_->getLastPosition();
    ASSERT_TRUE(last_pos.has_value());
    EXPECT_DOUBLE_EQ(LATITUDE_LA, last_pos->getLatitude());
}

TEST_F(EquipmentTest, IsMoving) {
    // No position yet, should not be moving
    EXPECT_FALSE(forklift_->isMoving());
    
    // Single position, should not be moving
    forklift_->recordPosition(position_sf_);
    EXPECT_FALSE(forklift_->isMoving());
    
    // Record positions with small movement (below threshold)
    Position position_near_sf(LATITUDE_SF + 0.00001, LONGITUDE_SF + 0.00001, ALTITUDE_1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    forklift_->recordPosition(position_near_sf);
    
    // Record position with significant movement
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    forklift_->recordPosition(position_la_);
    
    // Should be moving due to significant position change
    EXPECT_TRUE(forklift_->isMoving());
}

TEST_F(EquipmentTest, MaxHistorySize) {
    // Record more positions than the default max history size
    for (size_t i = 0; i < DEFAULT_MAX_HISTORY_SIZE + 10; ++i) {
        Position pos(LATITUDE_SF + i * 0.001, LONGITUDE_SF + i * 0.001, ALTITUDE_1 + i);
        forklift_->recordPosition(pos);
    }
    
    // History should be limited to max size
    auto history = forklift_->getPositionHistory();
    EXPECT_EQ(DEFAULT_MAX_HISTORY_SIZE, history.size());
    
    // The oldest entries should have been removed
    EXPECT_GT(history[0].getLatitude(), LATITUDE_SF);
}

TEST_F(EquipmentTest, ToStringOutput) {
    forklift_->setLastPosition(position_sf_);
    std::string equipment_string = forklift_->toString();
    
    EXPECT_THAT(equipment_string, ::testing::HasSubstr("FORKLIFT-001"));
    EXPECT_THAT(equipment_string, ::testing::HasSubstr("Warehouse Forklift 1"));
    EXPECT_THAT(equipment_string, ::testing::HasSubstr("Forklift"));
}

} // namespace
} // namespace equipment_tracker
// </test_code>