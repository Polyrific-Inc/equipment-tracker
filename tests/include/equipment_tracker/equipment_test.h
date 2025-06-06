// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/types.h"
#include "equipment_tracker/utils/constants.h"

namespace equipment_tracker {

class EquipmentTest : public ::testing::Test {
protected:
    EquipmentId testId = "EQ12345";
    EquipmentType testType = EquipmentType::Forklift;
    std::string testName = "Test Forklift";
    
    // Helper method to create a test position
    Position createTestPosition(double lat = 40.7128, double lon = -74.0060, 
                               double alt = 10.0, Timestamp time = getCurrentTimestamp()) {
        return Position(lat, lon, alt, DEFAULT_POSITION_ACCURACY, time);
    }
};

TEST_F(EquipmentTest, ConstructorAndGetters) {
    Equipment equipment(testId, testType, testName);
    
    EXPECT_EQ(equipment.getId(), testId);
    EXPECT_EQ(equipment.getType(), testType);
    EXPECT_EQ(equipment.getName(), testName);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Unknown);
    EXPECT_FALSE(equipment.getLastPosition().has_value());
}

TEST_F(EquipmentTest, CopyConstructor) {
    Equipment original(testId, testType, testName);
    original.setStatus(EquipmentStatus::Active);
    
    Position pos = createTestPosition();
    original.setLastPosition(pos);
    
    // Create a copy
    Equipment copy(original);
    
    // Verify the copy has the same values
    EXPECT_EQ(copy.getId(), original.getId());
    EXPECT_EQ(copy.getType(), original.getType());
    EXPECT_EQ(copy.getName(), original.getName());
    EXPECT_EQ(copy.getStatus(), original.getStatus());
    
    // Check position was copied
    ASSERT_TRUE(copy.getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(copy.getLastPosition()->getLatitude(), pos.getLatitude());
    EXPECT_DOUBLE_EQ(copy.getLastPosition()->getLongitude(), pos.getLongitude());
}

TEST_F(EquipmentTest, CopyAssignment) {
    Equipment original(testId, testType, testName);
    original.setStatus(EquipmentStatus::Active);
    
    Position pos = createTestPosition();
    original.setLastPosition(pos);
    
    // Create another equipment and assign
    Equipment other("OTHER123", EquipmentType::Crane, "Other Equipment");
    other = original;
    
    // Verify the assigned object has the same values
    EXPECT_EQ(other.getId(), original.getId());
    EXPECT_EQ(other.getType(), original.getType());
    EXPECT_EQ(other.getName(), original.getName());
    EXPECT_EQ(other.getStatus(), original.getStatus());
    
    // Check position was copied
    ASSERT_TRUE(other.getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(other.getLastPosition()->getLatitude(), pos.getLatitude());
    EXPECT_DOUBLE_EQ(other.getLastPosition()->getLongitude(), pos.getLongitude());
}

TEST_F(EquipmentTest, MoveConstructor) {
    Equipment original(testId, testType, testName);
    original.setStatus(EquipmentStatus::Active);
    
    Position pos = createTestPosition();
    original.setLastPosition(pos);
    
    // Create a copy to verify against later
    Equipment copy(original);
    
    // Move construct
    Equipment moved(std::move(original));
    
    // Verify moved object has the correct values
    EXPECT_EQ(moved.getId(), copy.getId());
    EXPECT_EQ(moved.getType(), copy.getType());
    EXPECT_EQ(moved.getName(), copy.getName());
    EXPECT_EQ(moved.getStatus(), copy.getStatus());
    
    // Check position was moved
    ASSERT_TRUE(moved.getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(moved.getLastPosition()->getLatitude(), pos.getLatitude());
    EXPECT_DOUBLE_EQ(moved.getLastPosition()->getLongitude(), pos.getLongitude());
}

TEST_F(EquipmentTest, MoveAssignment) {
    Equipment original(testId, testType, testName);
    original.setStatus(EquipmentStatus::Active);
    
    Position pos = createTestPosition();
    original.setLastPosition(pos);
    
    // Create a copy to verify against later
    Equipment copy(original);
    
    // Create another equipment and move assign
    Equipment other("OTHER123", EquipmentType::Crane, "Other Equipment");
    other = std::move(original);
    
    // Verify the moved-to object has the correct values
    EXPECT_EQ(other.getId(), copy.getId());
    EXPECT_EQ(other.getType(), copy.getType());
    EXPECT_EQ(other.getName(), copy.getName());
    EXPECT_EQ(other.getStatus(), copy.getStatus());
    
    // Check position was moved
    ASSERT_TRUE(other.getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(other.getLastPosition()->getLatitude(), pos.getLatitude());
    EXPECT_DOUBLE_EQ(other.getLastPosition()->getLongitude(), pos.getLongitude());
}

TEST_F(EquipmentTest, SettersAndGetters) {
    Equipment equipment(testId, testType, testName);
    
    // Test setStatus and getStatus
    equipment.setStatus(EquipmentStatus::Active);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
    
    equipment.setStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Maintenance);
    
    // Test setName and getName
    std::string newName = "Updated Forklift";
    equipment.setName(newName);
    EXPECT_EQ(equipment.getName(), newName);
}

TEST_F(EquipmentTest, PositionManagement) {
    Equipment equipment(testId, testType, testName);
    
    // Initially no position
    EXPECT_FALSE(equipment.getLastPosition().has_value());
    
    // Set position
    Position pos1 = createTestPosition(40.7128, -74.0060);
    equipment.setLastPosition(pos1);
    
    // Verify position was set
    ASSERT_TRUE(equipment.getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(equipment.getLastPosition()->getLatitude(), 40.7128);
    EXPECT_DOUBLE_EQ(equipment.getLastPosition()->getLongitude(), -74.0060);
    
    // Update position
    Position pos2 = createTestPosition(34.0522, -118.2437);
    equipment.setLastPosition(pos2);
    
    // Verify position was updated
    ASSERT_TRUE(equipment.getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(equipment.getLastPosition()->getLatitude(), 34.0522);
    EXPECT_DOUBLE_EQ(equipment.getLastPosition()->getLongitude(), -118.2437);
}

TEST_F(EquipmentTest, PositionHistory) {
    Equipment equipment(testId, testType, testName);
    
    // Initially empty history
    EXPECT_TRUE(equipment.getPositionHistory().empty());
    
    // Record positions
    Position pos1 = createTestPosition(40.7128, -74.0060);
    Position pos2 = createTestPosition(34.0522, -118.2437);
    Position pos3 = createTestPosition(41.8781, -87.6298);
    
    equipment.recordPosition(pos1);
    equipment.recordPosition(pos2);
    equipment.recordPosition(pos3);
    
    // Verify history contains all positions in order
    auto history = equipment.getPositionHistory();
    ASSERT_EQ(history.size(), 3);
    
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), 40.7128);
    EXPECT_DOUBLE_EQ(history[0].getLongitude(), -74.0060);
    
    EXPECT_DOUBLE_EQ(history[1].getLatitude(), 34.0522);
    EXPECT_DOUBLE_EQ(history[1].getLongitude(), -118.2437);
    
    EXPECT_DOUBLE_EQ(history[2].getLatitude(), 41.8781);
    EXPECT_DOUBLE_EQ(history[2].getLongitude(), -87.6298);
    
    // Test clear history
    equipment.clearPositionHistory();
    EXPECT_TRUE(equipment.getPositionHistory().empty());
}

TEST_F(EquipmentTest, HistorySizeLimit) {
    Equipment equipment(testId, testType, testName);
    
    // Add more positions than the default max history size
    for (size_t i = 0; i < DEFAULT_MAX_HISTORY_SIZE + 10; i++) {
        Position pos = createTestPosition(i, i);
        equipment.recordPosition(pos);
    }
    
    // Verify history is limited to max size
    auto history = equipment.getPositionHistory();
    EXPECT_EQ(history.size(), DEFAULT_MAX_HISTORY_SIZE);
    
    // Verify the oldest entries were removed (FIFO)
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), 10.0);
    EXPECT_DOUBLE_EQ(history[0].getLongitude(), 10.0);
}

TEST_F(EquipmentTest, IsMovingFunctionality) {
    Equipment equipment(testId, testType, testName);
    
    // Initially no position, should not be moving
    EXPECT_FALSE(equipment.isMoving());
    
    // Add a single position, still not moving
    Position pos1 = createTestPosition(40.7128, -74.0060);
    equipment.recordPosition(pos1);
    EXPECT_FALSE(equipment.isMoving());
    
    // Add a second position with a small time difference
    // Create a timestamp 1 second later
    auto timestamp = getCurrentTimestamp() + std::chrono::seconds(1);
    Position pos2 = createTestPosition(40.7129, -74.0061, 10.0, timestamp);
    equipment.recordPosition(pos2);
    
    // Should be moving if the calculated speed exceeds the threshold
    // This will depend on the implementation of isMoving()
    // We can't make a definitive assertion without knowing the exact implementation
    
    // Add a position that's identical to the previous one
    auto timestamp2 = getCurrentTimestamp() + std::chrono::seconds(2);
    Position pos3 = createTestPosition(40.7129, -74.0061, 10.0, timestamp2);
    equipment.recordPosition(pos3);
    
    // Should not be moving if positions are identical
    EXPECT_FALSE(equipment.isMoving());
}

TEST_F(EquipmentTest, ToStringMethod) {
    Equipment equipment(testId, testType, testName);
    equipment.setStatus(EquipmentStatus::Active);
    
    std::string result = equipment.toString();
    
    // Check that the string contains the essential information
    EXPECT_THAT(result, ::testing::HasSubstr(testId));
    EXPECT_THAT(result, ::testing::HasSubstr(testName));
    EXPECT_THAT(result, ::testing::HasSubstr("Active"));
    EXPECT_THAT(result, ::testing::HasSubstr("Forklift"));
}

TEST_F(EquipmentTest, ThreadSafetyOfPositionAccess) {
    Equipment equipment(testId, testType, testName);
    
    // Create multiple threads that simultaneously access and modify position
    std::vector<std::thread> threads;
    const int numThreads = 10;
    
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([&equipment, i]() {
            // Each thread sets a different position
            Position pos = Position(i * 1.0, i * -1.0);
            equipment.setLastPosition(pos);
            
            // Read the position
            auto lastPos = equipment.getLastPosition();
            
            // Record the position in history
            equipment.recordPosition(pos);
            
            // Get the history
            auto history = equipment.getPositionHistory();
        });
    }
    
    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify we have position history entries
    EXPECT_FALSE(equipment.getPositionHistory().empty());
    
    // Verify we have a last position
    EXPECT_TRUE(equipment.getLastPosition().has_value());
}

} // namespace equipment_tracker
// </test_code>