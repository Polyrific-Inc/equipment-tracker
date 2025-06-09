// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/utils/constants.h"
#include "equipment_tracker/utils/types.h"
#include "equipment_tracker/position.h"

namespace equipment_tracker {

// Helper function to create a timestamp with a specific offset from now
Timestamp createTimestampWithOffset(int seconds_offset) {
    return std::chrono::system_clock::now() + std::chrono::seconds(seconds_offset);
}

class EquipmentTest : public ::testing::Test {
protected:
    EquipmentId test_id = "EQ12345";
    EquipmentType test_type = EquipmentType::Forklift;
    std::string test_name = "Test Forklift";
    
    // Create test positions
    Position createTestPosition(double lat, double lon, double alt = 0.0, 
                               Timestamp time = getCurrentTimestamp()) {
        return Position(lat, lon, alt, DEFAULT_POSITION_ACCURACY, time);
    }
};

TEST_F(EquipmentTest, ConstructorInitializesCorrectly) {
    Equipment equipment(test_id, test_type, test_name);
    
    EXPECT_EQ(equipment.getId(), test_id);
    EXPECT_EQ(equipment.getType(), test_type);
    EXPECT_EQ(equipment.getName(), test_name);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Inactive);
    EXPECT_FALSE(equipment.getLastPosition().has_value());
    EXPECT_TRUE(equipment.getPositionHistory().empty());
}

TEST_F(EquipmentTest, CopyConstructorWorks) {
    Equipment original(test_id, test_type, test_name);
    original.setStatus(EquipmentStatus::Active);
    
    Position pos = createTestPosition(10.0, 20.0);
    original.recordPosition(pos);
    
    // Create a copy
    Equipment copy(original);
    
    // Verify all properties were copied
    EXPECT_EQ(copy.getId(), original.getId());
    EXPECT_EQ(copy.getType(), original.getType());
    EXPECT_EQ(copy.getName(), original.getName());
    EXPECT_EQ(copy.getStatus(), original.getStatus());
    
    // Check position data
    ASSERT_TRUE(copy.getLastPosition().has_value());
    EXPECT_EQ(copy.getLastPosition()->getLatitude(), pos.getLatitude());
    EXPECT_EQ(copy.getLastPosition()->getLongitude(), pos.getLongitude());
    
    // Check position history
    auto history = copy.getPositionHistory();
    ASSERT_EQ(history.size(), 1);
    EXPECT_EQ(history[0].getLatitude(), pos.getLatitude());
    EXPECT_EQ(history[0].getLongitude(), pos.getLongitude());
}

TEST_F(EquipmentTest, CopyAssignmentWorks) {
    Equipment original(test_id, test_type, test_name);
    original.setStatus(EquipmentStatus::Active);
    
    Position pos = createTestPosition(10.0, 20.0);
    original.recordPosition(pos);
    
    // Create another equipment and assign
    Equipment copy("OTHER", EquipmentType::Crane, "Other Equipment");
    copy = original;
    
    // Verify all properties were copied
    EXPECT_EQ(copy.getId(), original.getId());
    EXPECT_EQ(copy.getType(), original.getType());
    EXPECT_EQ(copy.getName(), original.getName());
    EXPECT_EQ(copy.getStatus(), original.getStatus());
    
    // Check position data
    ASSERT_TRUE(copy.getLastPosition().has_value());
    EXPECT_EQ(copy.getLastPosition()->getLatitude(), pos.getLatitude());
    EXPECT_EQ(copy.getLastPosition()->getLongitude(), pos.getLongitude());
}

TEST_F(EquipmentTest, MoveConstructorWorks) {
    Equipment original(test_id, test_type, test_name);
    original.setStatus(EquipmentStatus::Active);
    
    Position pos = createTestPosition(10.0, 20.0);
    original.recordPosition(pos);
    
    // Store original values for comparison
    std::string orig_id = original.getId();
    EquipmentType orig_type = original.getType();
    std::string orig_name = original.getName();
    EquipmentStatus orig_status = original.getStatus();
    
    // Create using move constructor
    Equipment moved(std::move(original));
    
    // Verify all properties were moved
    EXPECT_EQ(moved.getId(), orig_id);
    EXPECT_EQ(moved.getType(), orig_type);
    EXPECT_EQ(moved.getName(), orig_name);
    EXPECT_EQ(moved.getStatus(), orig_status);
    
    // Check position data
    ASSERT_TRUE(moved.getLastPosition().has_value());
    EXPECT_EQ(moved.getLastPosition()->getLatitude(), pos.getLatitude());
    EXPECT_EQ(moved.getLastPosition()->getLongitude(), pos.getLongitude());
    
    // Original should be in "unknown" state
    EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
}

TEST_F(EquipmentTest, MoveAssignmentWorks) {
    Equipment original(test_id, test_type, test_name);
    original.setStatus(EquipmentStatus::Active);
    
    Position pos = createTestPosition(10.0, 20.0);
    original.recordPosition(pos);
    
    // Store original values for comparison
    std::string orig_id = original.getId();
    EquipmentType orig_type = original.getType();
    std::string orig_name = original.getName();
    EquipmentStatus orig_status = original.getStatus();
    
    // Create another equipment and move-assign
    Equipment moved("OTHER", EquipmentType::Crane, "Other Equipment");
    moved = std::move(original);
    
    // Verify all properties were moved
    EXPECT_EQ(moved.getId(), orig_id);
    EXPECT_EQ(moved.getType(), orig_type);
    EXPECT_EQ(moved.getName(), orig_name);
    EXPECT_EQ(moved.getStatus(), orig_status);
    
    // Check position data
    ASSERT_TRUE(moved.getLastPosition().has_value());
    EXPECT_EQ(moved.getLastPosition()->getLatitude(), pos.getLatitude());
    EXPECT_EQ(moved.getLastPosition()->getLongitude(), pos.getLongitude());
    
    // Original should be in "unknown" state
    EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
}

TEST_F(EquipmentTest, SettersWorkCorrectly) {
    Equipment equipment(test_id, test_type, test_name);
    
    // Test setStatus
    equipment.setStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Maintenance);
    
    // Test setName
    std::string new_name = "Updated Name";
    equipment.setName(new_name);
    EXPECT_EQ(equipment.getName(), new_name);
    
    // Test setLastPosition
    Position pos = createTestPosition(10.0, 20.0);
    equipment.setLastPosition(pos);
    ASSERT_TRUE(equipment.getLastPosition().has_value());
    EXPECT_EQ(equipment.getLastPosition()->getLatitude(), pos.getLatitude());
    EXPECT_EQ(equipment.getLastPosition()->getLongitude(), pos.getLongitude());
    
    // Verify position history is not affected by setLastPosition
    EXPECT_TRUE(equipment.getPositionHistory().empty());
}

TEST_F(EquipmentTest, RecordPositionUpdatesHistoryAndLastPosition) {
    Equipment equipment(test_id, test_type, test_name);
    
    // Record a position
    Position pos1 = createTestPosition(10.0, 20.0);
    equipment.recordPosition(pos1);
    
    // Check last position
    ASSERT_TRUE(equipment.getLastPosition().has_value());
    EXPECT_EQ(equipment.getLastPosition()->getLatitude(), pos1.getLatitude());
    EXPECT_EQ(equipment.getLastPosition()->getLongitude(), pos1.getLongitude());
    
    // Check position history
    auto history = equipment.getPositionHistory();
    ASSERT_EQ(history.size(), 1);
    EXPECT_EQ(history[0].getLatitude(), pos1.getLatitude());
    EXPECT_EQ(history[0].getLongitude(), pos1.getLongitude());
    
    // Check status is updated to Active
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
    
    // Record another position
    Position pos2 = createTestPosition(11.0, 21.0);
    equipment.recordPosition(pos2);
    
    // Check last position is updated
    ASSERT_TRUE(equipment.getLastPosition().has_value());
    EXPECT_EQ(equipment.getLastPosition()->getLatitude(), pos2.getLatitude());
    EXPECT_EQ(equipment.getLastPosition()->getLongitude(), pos2.getLongitude());
    
    // Check position history has both positions
    history = equipment.getPositionHistory();
    ASSERT_EQ(history.size(), 2);
    EXPECT_EQ(history[0].getLatitude(), pos1.getLatitude());
    EXPECT_EQ(history[1].getLatitude(), pos2.getLatitude());
}

TEST_F(EquipmentTest, PositionHistorySizeIsLimited) {
    Equipment equipment(test_id, test_type, test_name);
    
    // Record more positions than the max history size
    for (size_t i = 0; i < DEFAULT_MAX_HISTORY_SIZE + 10; i++) {
        Position pos = createTestPosition(static_cast<double>(i), static_cast<double>(i));
        equipment.recordPosition(pos);
    }
    
    // Check history size is limited
    auto history = equipment.getPositionHistory();
    EXPECT_EQ(history.size(), DEFAULT_MAX_HISTORY_SIZE);
    
    // Check that oldest positions were removed (FIFO)
    EXPECT_EQ(history[0].getLatitude(), 10.0);
    EXPECT_EQ(history[0].getLongitude(), 10.0);
    
    // Check that newest positions are present
    EXPECT_EQ(history.back().getLatitude(), DEFAULT_MAX_HISTORY_SIZE + 9);
    EXPECT_EQ(history.back().getLongitude(), DEFAULT_MAX_HISTORY_SIZE + 9);
}

TEST_F(EquipmentTest, ClearPositionHistoryWorks) {
    Equipment equipment(test_id, test_type, test_name);
    
    // Record some positions
    for (int i = 0; i < 5; i++) {
        Position pos = createTestPosition(static_cast<double>(i), static_cast<double>(i));
        equipment.recordPosition(pos);
    }
    
    // Verify history has positions
    EXPECT_EQ(equipment.getPositionHistory().size(), 5);
    
    // Clear history
    equipment.clearPositionHistory();
    
    // Verify history is empty
    EXPECT_TRUE(equipment.getPositionHistory().empty());
    
    // Last position should still be available
    ASSERT_TRUE(equipment.getLastPosition().has_value());
    EXPECT_EQ(equipment.getLastPosition()->getLatitude(), 4.0);
    EXPECT_EQ(equipment.getLastPosition()->getLongitude(), 4.0);
}

TEST_F(EquipmentTest, IsMovingReturnsFalseWithInsufficientHistory) {
    Equipment equipment(test_id, test_type, test_name);
    
    // No positions
    EXPECT_FALSE(equipment.isMoving());
    
    // Only one position
    Position pos = createTestPosition(10.0, 20.0);
    equipment.recordPosition(pos);
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, IsMovingDetectsMovementCorrectly) {
    Equipment equipment(test_id, test_type, test_name);
    
    // Record two positions with significant distance and time difference
    Timestamp time1 = createTimestampWithOffset(-10); // 10 seconds ago
    Position pos1 = createTestPosition(10.0, 20.0, 0.0, time1);
    equipment.recordPosition(pos1);
    
    // Second position 100 meters away, 5 seconds later
    // This should result in a speed of 20 m/s, which is above the threshold
    Timestamp time2 = createTimestampWithOffset(-5); // 5 seconds ago
    Position pos2 = createTestPosition(10.001, 20.0, 0.0, time2); // ~100m north
    equipment.recordPosition(pos2);
    
    EXPECT_TRUE(equipment.isMoving());
}

TEST_F(EquipmentTest, IsMovingReturnsFalseForSlowMovement) {
    Equipment equipment(test_id, test_type, test_name);
    
    // Record two positions with small distance and large time difference
    Timestamp time1 = createTimestampWithOffset(-10); // 10 seconds ago
    Position pos1 = createTestPosition(10.0, 20.0, 0.0, time1);
    equipment.recordPosition(pos1);
    
    // Second position very close, 5 seconds later
    // This should result in a speed below the threshold
    Timestamp time2 = createTimestampWithOffset(-5); // 5 seconds ago
    Position pos2 = createTestPosition(10.000001, 20.0, 0.0, time2); // Very small movement
    equipment.recordPosition(pos2);
    
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, IsMovingHandlesSmallTimeDifference) {
    Equipment equipment(test_id, test_type, test_name);
    
    // Record two positions with almost same timestamp
    Timestamp time1 = getCurrentTimestamp();
    Position pos1 = createTestPosition(10.0, 20.0, 0.0, time1);
    equipment.recordPosition(pos1);
    
    // Second position with same timestamp (or very close)
    Position pos2 = createTestPosition(10.001, 20.0, 0.0, time1);
    equipment.recordPosition(pos2);
    
    // Should return false to avoid division by zero
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, ToStringFormatsCorrectly) {
    Equipment equipment(test_id, test_type, test_name);
    
    std::string result = equipment.toString();
    
    // Check that the string contains all the expected parts
    EXPECT_THAT(result, ::testing::HasSubstr(test_id));
    EXPECT_THAT(result, ::testing::HasSubstr(test_name));
    EXPECT_THAT(result, ::testing::HasSubstr("Forklift"));
    EXPECT_THAT(result, ::testing::HasSubstr("Inactive"));
}

TEST_F(EquipmentTest, ToStringHandlesAllEquipmentTypes) {
    // Test each equipment type
    std::vector<std::pair<EquipmentType, std::string>> types = {
        {EquipmentType::Forklift, "Forklift"},
        {EquipmentType::Crane, "Crane"},
        {EquipmentType::Bulldozer, "Bulldozer"},
        {EquipmentType::Excavator, "Excavator"},
        {EquipmentType::Truck, "Truck"},
        {EquipmentType::Other, "Other"}
    };
    
    for (const auto& [type, expected_str] : types) {
        Equipment equipment(test_id, type, test_name);
        std::string result = equipment.toString();
        EXPECT_THAT(result, ::testing::HasSubstr(expected_str));
    }
}

TEST_F(EquipmentTest, ToStringHandlesAllStatusTypes) {
    // Test each status type
    std::vector<std::pair<EquipmentStatus, std::string>> statuses = {
        {EquipmentStatus::Active, "Active"},
        {EquipmentStatus::Inactive, "Inactive"},
        {EquipmentStatus::Maintenance, "Maintenance"},
        {EquipmentStatus::Unknown, "Unknown"}
    };
    
    for (const auto& [status, expected_str] : statuses) {
        Equipment equipment(test_id, test_type, test_name);
        equipment.setStatus(status);
        std::string result = equipment.toString();
        EXPECT_THAT(result, ::testing::HasSubstr(expected_str));
    }
}

} // namespace equipment_tracker
// </test_code>