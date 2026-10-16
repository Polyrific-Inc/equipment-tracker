// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/utils/types.h"
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/constants.h"
#include <chrono>
#include <thread>

namespace equipment_tracker {

class EquipmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test equipment instance
        testEquipment = new Equipment("EQ123", EquipmentType::Forklift, "Test Forklift");
    }

    void TearDown() override {
        delete testEquipment;
    }

    Equipment* testEquipment;

    // Helper method to create a test position
    Position createTestPosition(double lat = 40.7128, double lon = -74.0060, double alt = 10.0) {
        return Position(lat, lon, alt);
    }
};

TEST_F(EquipmentTest, ConstructorSetsInitialValues) {
    EXPECT_EQ("EQ123", testEquipment->getId());
    EXPECT_EQ(EquipmentType::Forklift, testEquipment->getType());
    EXPECT_EQ("Test Forklift", testEquipment->getName());
    EXPECT_EQ(EquipmentStatus::Unknown, testEquipment->getStatus());
    EXPECT_FALSE(testEquipment->getLastPosition().has_value());
}

TEST_F(EquipmentTest, CopyConstructorWorks) {
    // Set some values to test
    testEquipment->setStatus(EquipmentStatus::Active);
    Position pos = createTestPosition();
    testEquipment->setLastPosition(pos);
    
    // Create a copy
    Equipment copy(*testEquipment);
    
    // Verify the copy has the same values
    EXPECT_EQ(testEquipment->getId(), copy.getId());
    EXPECT_EQ(testEquipment->getType(), copy.getType());
    EXPECT_EQ(testEquipment->getName(), copy.getName());
    EXPECT_EQ(testEquipment->getStatus(), copy.getStatus());
    
    // Check position is copied
    ASSERT_TRUE(copy.getLastPosition().has_value());
    EXPECT_NEAR(pos.getLatitude(), copy.getLastPosition()->getLatitude(), 0.0001);
    EXPECT_NEAR(pos.getLongitude(), copy.getLastPosition()->getLongitude(), 0.0001);
}

TEST_F(EquipmentTest, CopyAssignmentWorks) {
    // Create a second equipment with different values
    Equipment other("EQ456", EquipmentType::Crane, "Test Crane");
    other.setStatus(EquipmentStatus::Maintenance);
    
    // Assign the test equipment to the other
    other = *testEquipment;
    
    // Verify the values were copied
    EXPECT_EQ(testEquipment->getId(), other.getId());
    EXPECT_EQ(testEquipment->getType(), other.getType());
    EXPECT_EQ(testEquipment->getName(), other.getName());
    EXPECT_EQ(testEquipment->getStatus(), other.getStatus());
}

TEST_F(EquipmentTest, MoveConstructorWorks) {
    // Set up the source equipment
    testEquipment->setStatus(EquipmentStatus::Active);
    Position pos = createTestPosition();
    testEquipment->setLastPosition(pos);
    
    // Store original values for comparison
    std::string id = testEquipment->getId();
    EquipmentType type = testEquipment->getType();
    std::string name = testEquipment->getName();
    EquipmentStatus status = testEquipment->getStatus();
    
    // Move construct
    Equipment moved(std::move(*testEquipment));
    
    // Verify moved equipment has the correct values
    EXPECT_EQ(id, moved.getId());
    EXPECT_EQ(type, moved.getType());
    EXPECT_EQ(name, moved.getName());
    EXPECT_EQ(status, moved.getStatus());
    
    // Check position was moved
    ASSERT_TRUE(moved.getLastPosition().has_value());
    EXPECT_NEAR(pos.getLatitude(), moved.getLastPosition()->getLatitude(), 0.0001);
    EXPECT_NEAR(pos.getLongitude(), moved.getLastPosition()->getLongitude(), 0.0001);
}

TEST_F(EquipmentTest, MoveAssignmentWorks) {
    // Set up the source equipment
    testEquipment->setStatus(EquipmentStatus::Active);
    Position pos = createTestPosition();
    testEquipment->setLastPosition(pos);
    
    // Store original values for comparison
    std::string id = testEquipment->getId();
    EquipmentType type = testEquipment->getType();
    std::string name = testEquipment->getName();
    EquipmentStatus status = testEquipment->getStatus();
    
    // Create target equipment
    Equipment target("EQ999", EquipmentType::Other, "Target Equipment");
    
    // Move assign
    target = std::move(*testEquipment);
    
    // Verify target has the moved values
    EXPECT_EQ(id, target.getId());
    EXPECT_EQ(type, target.getType());
    EXPECT_EQ(name, target.getName());
    EXPECT_EQ(status, target.getStatus());
    
    // Check position was moved
    ASSERT_TRUE(target.getLastPosition().has_value());
    EXPECT_NEAR(pos.getLatitude(), target.getLastPosition()->getLatitude(), 0.0001);
    EXPECT_NEAR(pos.getLongitude(), target.getLastPosition()->getLongitude(), 0.0001);
}

TEST_F(EquipmentTest, SetAndGetStatus) {
    // Test setting and getting status
    testEquipment->setStatus(EquipmentStatus::Active);
    EXPECT_EQ(EquipmentStatus::Active, testEquipment->getStatus());
    
    testEquipment->setStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(EquipmentStatus::Maintenance, testEquipment->getStatus());
}

TEST_F(EquipmentTest, SetAndGetName) {
    // Test setting and getting name
    testEquipment->setName("Updated Forklift");
    EXPECT_EQ("Updated Forklift", testEquipment->getName());
    
    // Test with empty name
    testEquipment->setName("");
    EXPECT_EQ("", testEquipment->getName());
}

TEST_F(EquipmentTest, SetAndGetLastPosition) {
    // Create a position
    Position pos = createTestPosition();
    
    // Initially, there should be no position
    EXPECT_FALSE(testEquipment->getLastPosition().has_value());
    
    // Set and verify position
    testEquipment->setLastPosition(pos);
    ASSERT_TRUE(testEquipment->getLastPosition().has_value());
    EXPECT_NEAR(pos.getLatitude(), testEquipment->getLastPosition()->getLatitude(), 0.0001);
    EXPECT_NEAR(pos.getLongitude(), testEquipment->getLastPosition()->getLongitude(), 0.0001);
    EXPECT_NEAR(pos.getAltitude(), testEquipment->getLastPosition()->getAltitude(), 0.0001);
}

TEST_F(EquipmentTest, RecordPositionAndGetHistory) {
    // Create test positions
    Position pos1 = createTestPosition(40.7128, -74.0060, 10.0);
    Position pos2 = createTestPosition(40.7129, -74.0061, 11.0);
    Position pos3 = createTestPosition(40.7130, -74.0062, 12.0);
    
    // Record positions
    testEquipment->recordPosition(pos1);
    testEquipment->recordPosition(pos2);
    testEquipment->recordPosition(pos3);
    
    // Get history and verify
    std::vector<Position> history = testEquipment->getPositionHistory();
    ASSERT_EQ(3, history.size());
    
    // Check positions in order (most recent first)
    EXPECT_NEAR(pos3.getLatitude(), history[0].getLatitude(), 0.0001);
    EXPECT_NEAR(pos2.getLatitude(), history[1].getLatitude(), 0.0001);
    EXPECT_NEAR(pos1.getLatitude(), history[2].getLatitude(), 0.0001);
    
    // Last position should be the most recent
    ASSERT_TRUE(testEquipment->getLastPosition().has_value());
    EXPECT_NEAR(pos3.getLatitude(), testEquipment->getLastPosition()->getLatitude(), 0.0001);
}

TEST_F(EquipmentTest, ClearPositionHistory) {
    // Add some positions
    testEquipment->recordPosition(createTestPosition(40.7128, -74.0060));
    testEquipment->recordPosition(createTestPosition(40.7129, -74.0061));
    
    // Verify positions were added
    EXPECT_EQ(2, testEquipment->getPositionHistory().size());
    EXPECT_TRUE(testEquipment->getLastPosition().has_value());
    
    // Clear history
    testEquipment->clearPositionHistory();
    
    // Verify history is cleared but last position remains
    EXPECT_EQ(0, testEquipment->getPositionHistory().size());
    EXPECT_TRUE(testEquipment->getLastPosition().has_value());
}

TEST_F(EquipmentTest, IsMovingReturnsFalseWithNoPositions) {
    // With no positions, equipment should not be moving
    EXPECT_FALSE(testEquipment->isMoving());
}

TEST_F(EquipmentTest, IsMovingReturnsFalseWithOnePosition) {
    // With only one position, equipment should not be moving
    testEquipment->recordPosition(createTestPosition());
    EXPECT_FALSE(testEquipment->isMoving());
}

TEST_F(EquipmentTest, IsMovingDetectsMovement) {
    // Create positions with significant distance
    Position pos1 = createTestPosition(40.7128, -74.0060);
    
    // Record first position
    testEquipment->recordPosition(pos1);
    
    // Sleep to ensure timestamp difference
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Create a position that would indicate movement above threshold
    Position pos2 = createTestPosition(40.7138, -74.0070); // Moved significantly
    
    // Record second position
    testEquipment->recordPosition(pos2);
    
    // Should detect movement
    EXPECT_TRUE(testEquipment->isMoving());
}

TEST_F(EquipmentTest, IsMovingReturnsFalseForStationaryEquipment) {
    // Create positions with minimal distance
    Position pos1 = createTestPosition(40.7128, -74.0060);
    
    // Record first position
    testEquipment->recordPosition(pos1);
    
    // Sleep to ensure timestamp difference
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create a position very close to the first one
    Position pos2 = createTestPosition(40.71281, -74.00601); // Moved very slightly
    
    // Record second position
    testEquipment->recordPosition(pos2);
    
    // Should not detect movement (below threshold)
    EXPECT_FALSE(testEquipment->isMoving());
}

TEST_F(EquipmentTest, ToStringReturnsFormattedString) {
    // Set up equipment with position
    testEquipment->setStatus(EquipmentStatus::Active);
    testEquipment->setLastPosition(createTestPosition(40.7128, -74.0060));
    
    // Get string representation
    std::string result = testEquipment->toString();
    
    // Check that the string contains expected information
    EXPECT_THAT(result, ::testing::HasSubstr("EQ123"));
    EXPECT_THAT(result, ::testing::HasSubstr("Test Forklift"));
    EXPECT_THAT(result, ::testing::HasSubstr("Forklift"));
    EXPECT_THAT(result, ::testing::HasSubstr("Active"));
    EXPECT_THAT(result, ::testing::HasSubstr("40.7128"));
    EXPECT_THAT(result, ::testing::HasSubstr("-74.006"));
}

TEST_F(EquipmentTest, HistoryLimitedToMaxSize) {
    // Add more positions than the default max history size
    for (size_t i = 0; i < DEFAULT_MAX_HISTORY_SIZE + 10; i++) {
        testEquipment->recordPosition(createTestPosition(40.0 + i * 0.001, -74.0 + i * 0.001));
    }
    
    // Verify history size is limited to max
    EXPECT_EQ(DEFAULT_MAX_HISTORY_SIZE, testEquipment->getPositionHistory().size());
    
    // Verify the most recent positions are kept (most recent first)
    std::vector<Position> history = testEquipment->getPositionHistory();
    EXPECT_NEAR(40.0 + (DEFAULT_MAX_HISTORY_SIZE + 9) * 0.001, history[0].getLatitude(), 0.0001);
    EXPECT_NEAR(40.0 + (DEFAULT_MAX_HISTORY_SIZE + 8) * 0.001, history[1].getLatitude(), 0.0001);
}

TEST_F(EquipmentTest, ThreadSafetyOfPositionAccess) {
    // This test verifies that concurrent access to position data is thread-safe
    // by running multiple threads that set and get positions
    
    const int numThreads = 10;
    const int operationsPerThread = 100;
    
    std::vector<std::thread> threads;
    
    // Launch threads that set and get positions
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([this, i, operationsPerThread]() {
            for (int j = 0; j < operationsPerThread; j++) {
                // Set position
                Position pos = createTestPosition(40.0 + i * 0.1 + j * 0.001, 
                                                -74.0 + i * 0.1 + j * 0.001);
                testEquipment->setLastPosition(pos);
                
                // Record position
                testEquipment->recordPosition(pos);
                
                // Get position
                auto lastPos = testEquipment->getLastPosition();
                
                // Get history
                auto history = testEquipment->getPositionHistory();
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // If we get here without crashes or exceptions, the test passes
    SUCCEED();
}

} // namespace equipment_tracker
// </test_code>