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

// Helper function to create a timestamp with a specified offset from now
Timestamp createTimestampWithOffset(int seconds_offset) {
    return std::chrono::system_clock::now() + std::chrono::seconds(seconds_offset);
}

class EquipmentTest : public ::testing::Test {
protected:
    EquipmentId test_id = "EQ12345";
    EquipmentType test_type = EquipmentType::Forklift;
    std::string test_name = "Test Forklift";
    
    // Create a test position
    Position createTestPosition(double lat = 37.7749, double lon = -122.4194, 
                               double alt = 10.0, int time_offset = 0) {
        return Position(lat, lon, alt, DEFAULT_POSITION_ACCURACY, 
                       createTimestampWithOffset(time_offset));
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
    
    Position pos = createTestPosition();
    original.recordPosition(pos);
    
    Equipment copy(original);
    
    EXPECT_EQ(copy.getId(), original.getId());
    EXPECT_EQ(copy.getType(), original.getType());
    EXPECT_EQ(copy.getName(), original.getName());
    EXPECT_EQ(copy.getStatus(), original.getStatus());
    
    auto original_pos = original.getLastPosition();
    auto copy_pos = copy.getLastPosition();
    
    ASSERT_TRUE(original_pos.has_value());
    ASSERT_TRUE(copy_pos.has_value());
    
    EXPECT_DOUBLE_EQ(original_pos->getLatitude(), copy_pos->getLatitude());
    EXPECT_DOUBLE_EQ(original_pos->getLongitude(), copy_pos->getLongitude());
    
    EXPECT_EQ(original.getPositionHistory().size(), copy.getPositionHistory().size());
}

TEST_F(EquipmentTest, CopyAssignmentWorks) {
    Equipment original(test_id, test_type, test_name);
    original.setStatus(EquipmentStatus::Active);
    
    Position pos = createTestPosition();
    original.recordPosition(pos);
    
    Equipment copy("OTHER", EquipmentType::Crane, "Other Equipment");
    copy = original;
    
    EXPECT_EQ(copy.getId(), original.getId());
    EXPECT_EQ(copy.getType(), original.getType());
    EXPECT_EQ(copy.getName(), original.getName());
    EXPECT_EQ(copy.getStatus(), original.getStatus());
    
    auto original_pos = original.getLastPosition();
    auto copy_pos = copy.getLastPosition();
    
    ASSERT_TRUE(original_pos.has_value());
    ASSERT_TRUE(copy_pos.has_value());
    
    EXPECT_DOUBLE_EQ(original_pos->getLatitude(), copy_pos->getLatitude());
    EXPECT_DOUBLE_EQ(original_pos->getLongitude(), copy_pos->getLongitude());
    
    EXPECT_EQ(original.getPositionHistory().size(), copy.getPositionHistory().size());
}

TEST_F(EquipmentTest, MoveConstructorWorks) {
    Equipment original(test_id, test_type, test_name);
    original.setStatus(EquipmentStatus::Active);
    
    Position pos = createTestPosition();
    original.recordPosition(pos);
    
    auto original_pos = original.getLastPosition();
    ASSERT_TRUE(original_pos.has_value());
    double original_lat = original_pos->getLatitude();
    double original_lon = original_pos->getLongitude();
    
    Equipment moved(std::move(original));
    
    // Check moved-to object has correct data
    EXPECT_EQ(moved.getId(), test_id);
    EXPECT_EQ(moved.getType(), test_type);
    EXPECT_EQ(moved.getName(), test_name);
    EXPECT_EQ(moved.getStatus(), EquipmentStatus::Active);
    
    auto moved_pos = moved.getLastPosition();
    ASSERT_TRUE(moved_pos.has_value());
    EXPECT_DOUBLE_EQ(moved_pos->getLatitude(), original_lat);
    EXPECT_DOUBLE_EQ(moved_pos->getLongitude(), original_lon);
    
    // Check moved-from object is in a valid but unspecified state
    EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
}

TEST_F(EquipmentTest, MoveAssignmentWorks) {
    Equipment original(test_id, test_type, test_name);
    original.setStatus(EquipmentStatus::Active);
    
    Position pos = createTestPosition();
    original.recordPosition(pos);
    
    auto original_pos = original.getLastPosition();
    ASSERT_TRUE(original_pos.has_value());
    double original_lat = original_pos->getLatitude();
    double original_lon = original_pos->getLongitude();
    
    Equipment moved("OTHER", EquipmentType::Crane, "Other Equipment");
    moved = std::move(original);
    
    // Check moved-to object has correct data
    EXPECT_EQ(moved.getId(), test_id);
    EXPECT_EQ(moved.getType(), test_type);
    EXPECT_EQ(moved.getName(), test_name);
    EXPECT_EQ(moved.getStatus(), EquipmentStatus::Active);
    
    auto moved_pos = moved.getLastPosition();
    ASSERT_TRUE(moved_pos.has_value());
    EXPECT_DOUBLE_EQ(moved_pos->getLatitude(), original_lat);
    EXPECT_DOUBLE_EQ(moved_pos->getLongitude(), original_lon);
    
    // Check moved-from object is in a valid but unspecified state
    EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
}

TEST_F(EquipmentTest, SelfAssignmentHandledCorrectly) {
    Equipment equipment(test_id, test_type, test_name);
    equipment.setStatus(EquipmentStatus::Active);
    
    Position pos = createTestPosition();
    equipment.recordPosition(pos);
    
    // Self copy assignment
    equipment = equipment;
    
    EXPECT_EQ(equipment.getId(), test_id);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
    ASSERT_TRUE(equipment.getLastPosition().has_value());
    
    // Self move assignment
    equipment = std::move(equipment);
    
    EXPECT_EQ(equipment.getId(), test_id);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
    ASSERT_TRUE(equipment.getLastPosition().has_value());
}

TEST_F(EquipmentTest, SettersWorkCorrectly) {
    Equipment equipment(test_id, test_type, test_name);
    
    // Test setStatus
    equipment.setStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Maintenance);
    
    // Test setName
    std::string new_name = "Updated Forklift";
    equipment.setName(new_name);
    EXPECT_EQ(equipment.getName(), new_name);
    
    // Test setLastPosition
    Position pos = createTestPosition();
    equipment.setLastPosition(pos);
    
    auto last_pos = equipment.getLastPosition();
    ASSERT_TRUE(last_pos.has_value());
    EXPECT_DOUBLE_EQ(last_pos->getLatitude(), pos.getLatitude());
    EXPECT_DOUBLE_EQ(last_pos->getLongitude(), pos.getLongitude());
    EXPECT_DOUBLE_EQ(last_pos->getAltitude(), pos.getAltitude());
}

TEST_F(EquipmentTest, RecordPositionUpdatesHistory) {
    Equipment equipment(test_id, test_type, test_name);
    
    // Record multiple positions
    Position pos1 = createTestPosition(37.7749, -122.4194);
    Position pos2 = createTestPosition(37.7750, -122.4195);
    Position pos3 = createTestPosition(37.7751, -122.4196);
    
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    equipment.recordPosition(pos3);
    
    // Check last position
    auto last_pos = equipment.getLastPosition();
    ASSERT_TRUE(last_pos.has_value());
    EXPECT_DOUBLE_EQ(last_pos->getLatitude(), pos3.getLatitude());
    EXPECT_DOUBLE_EQ(last_pos->getLongitude(), pos3.getLongitude());
    
    // Check history
    auto history = equipment.getPositionHistory();
    ASSERT_EQ(history.size(), 3);
    
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), pos1.getLatitude());
    EXPECT_DOUBLE_EQ(history[1].getLatitude(), pos2.getLatitude());
    EXPECT_DOUBLE_EQ(history[2].getLatitude(), pos3.getLatitude());
    
    // Check status is updated to Active
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
}

TEST_F(EquipmentTest, HistorySizeIsLimited) {
    Equipment equipment(test_id, test_type, test_name);
    
    // Record more positions than the default max history size
    for (size_t i = 0; i < DEFAULT_MAX_HISTORY_SIZE + 10; i++) {
        Position pos = createTestPosition(37.7749 + i * 0.0001, -122.4194 + i * 0.0001);
        equipment.recordPosition(pos);
    }
    
    // Check history size is limited
    auto history = equipment.getPositionHistory();
    EXPECT_EQ(history.size(), DEFAULT_MAX_HISTORY_SIZE);
    
    // Check the oldest entries were removed (FIFO)
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), 37.7749 + 10 * 0.0001);
}

TEST_F(EquipmentTest, ClearPositionHistoryWorks) {
    Equipment equipment(test_id, test_type, test_name);
    
    // Record some positions
    for (int i = 0; i < 5; i++) {
        Position pos = createTestPosition(37.7749 + i * 0.0001, -122.4194 + i * 0.0001);
        equipment.recordPosition(pos);
    }
    
    EXPECT_EQ(equipment.getPositionHistory().size(), 5);
    
    // Clear history
    equipment.clearPositionHistory();
    
    // Check history is empty
    EXPECT_TRUE(equipment.getPositionHistory().empty());
    
    // Last position should still be available
    EXPECT_TRUE(equipment.getLastPosition().has_value());
}

TEST_F(EquipmentTest, IsMovingDetectsMovement) {
    Equipment equipment(test_id, test_type, test_name);
    
    // Not moving with less than 2 positions
    EXPECT_FALSE(equipment.isMoving());
    
    // Add first position
    Position pos1 = createTestPosition(37.7749, -122.4194, 10.0, -10);
    equipment.recordPosition(pos1);
    EXPECT_FALSE(equipment.isMoving());
    
    // Add second position with significant movement
    // Calculate a position that's far enough to exceed the movement threshold
    // Assuming MOVEMENT_SPEED_THRESHOLD is 0.5 m/s and 5 seconds have passed
    // We need to move more than 2.5 meters
    // At latitude 37.7749, moving 0.0001 degrees is roughly 11 meters
    Position pos2 = createTestPosition(37.7750, -122.4194, 10.0, 0);
    equipment.recordPosition(pos2);
    
    // Should be moving (positions are far enough apart)
    EXPECT_TRUE(equipment.isMoving());
    
    // Test with positions that are too close in time
    Equipment equipment2(test_id, test_type, test_name);
    
    // Add positions with very small time difference
    Position pos3 = createTestPosition(37.7749, -122.4194, 10.0, 0);
    equipment2.recordPosition(pos3);
    
    // Add second position immediately (less than 1 second apart)
    Position pos4 = createTestPosition(37.7750, -122.4194, 10.0, 0);
    equipment2.recordPosition(pos4);
    
    // Should not be moving (time difference too small)
    EXPECT_FALSE(equipment2.isMoving());
}

TEST_F(EquipmentTest, ToStringFormatsCorrectly) {
    Equipment equipment(test_id, test_type, test_name);
    
    std::string str = equipment.toString();
    
    // Check that the string contains all the expected parts
    EXPECT_THAT(str, ::testing::HasSubstr(test_id));
    EXPECT_THAT(str, ::testing::HasSubstr(test_name));
    EXPECT_THAT(str, ::testing::HasSubstr("Forklift"));
    EXPECT_THAT(str, ::testing::HasSubstr("Inactive"));
    
    // Change status and check again
    equipment.setStatus(EquipmentStatus::Maintenance);
    str = equipment.toString();
    EXPECT_THAT(str, ::testing::HasSubstr("Maintenance"));
    
    // Test with different equipment type
    Equipment crane("CRANE1", EquipmentType::Crane, "Test Crane");
    str = crane.toString();
    EXPECT_THAT(str, ::testing::HasSubstr("Crane"));
    
    // Test with unknown status
    crane.setStatus(EquipmentStatus::Unknown);
    str = crane.toString();
    EXPECT_THAT(str, ::testing::HasSubstr("Unknown"));
}

TEST_F(EquipmentTest, ThreadSafetyOfPositionAccess) {
    Equipment equipment(test_id, test_type, test_name);
    
    // Create a large number of positions to record
    const int num_positions = 100;
    std::vector<Position> positions;
    
    for (int i = 0; i < num_positions; i++) {
        positions.push_back(createTestPosition(37.7749 + i * 0.0001, -122.4194 + i * 0.0001));
    }
    
    // Create threads to record positions and read history concurrently
    std::thread writer([&]() {
        for (const auto& pos : positions) {
            equipment.recordPosition(pos);
        }
    });
    
    std::thread reader([&]() {
        for (int i = 0; i < num_positions; i++) {
            auto history = equipment.getPositionHistory();
            auto last_pos = equipment.getLastPosition();
            // Just accessing the data is enough to test thread safety
        }
    });
    
    writer.join();
    reader.join();
    
    // If we got here without crashing or deadlocking, the test passes
    // Also verify the final state is correct
    EXPECT_EQ(equipment.getPositionHistory().size(), num_positions);
    ASSERT_TRUE(equipment.getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(equipment.getLastPosition()->getLatitude(), 37.7749 + (num_positions - 1) * 0.0001);
}

} // namespace equipment_tracker
// </test_code>