// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>
#include <optional>
#include <thread>
#include <chrono>
#include "utils/types.h"
#include "position.h"
#include "equipment.h"

// Mock Position class if needed
#ifndef POSITION_H
class Position {
public:
    Position(double lat, double lon, double elevation, uint64_t timestamp)
        : latitude(lat), longitude(lon), elevation(elevation), timestamp(timestamp) {}
    
    double getLatitude() const { return latitude; }
    double getLongitude() const { return longitude; }
    double getElevation() const { return elevation; }
    uint64_t getTimestamp() const { return timestamp; }
    
    std::string toString() const { 
        return "Position(" + std::to_string(latitude) + ", " + 
               std::to_string(longitude) + ", " + 
               std::to_string(elevation) + ")"; 
    }
    
private:
    double latitude;
    double longitude;
    double elevation;
    uint64_t timestamp;
};
#endif

// Mock types if needed
#ifndef TYPES_H
enum class EquipmentType { BULLDOZER, EXCAVATOR, CRANE, LOADER };
enum class EquipmentStatus { ACTIVE, INACTIVE, MAINTENANCE, UNKNOWN };
using EquipmentId = std::string;
constexpr size_t DEFAULT_MAX_HISTORY_SIZE = 100;
#endif

namespace equipment_tracker {

class EquipmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for all tests
    }
    
    // Helper method to create a test equipment
    Equipment createTestEquipment() {
        return Equipment("EQ123", EquipmentType::BULLDOZER, "Test Bulldozer");
    }
    
    // Helper method to create a test position
    Position createTestPosition(double lat = 40.7128, double lon = -74.0060, 
                               double elev = 10.0, uint64_t time = 1609459200) {
        return Position(lat, lon, elev, time);
    }
};

TEST_F(EquipmentTest, ConstructorInitializesCorrectly) {
    Equipment eq("EQ123", EquipmentType::BULLDOZER, "Test Bulldozer");
    
    EXPECT_EQ("EQ123", eq.getId());
    EXPECT_EQ(EquipmentType::BULLDOZER, eq.getType());
    EXPECT_EQ("Test Bulldozer", eq.getName());
}

TEST_F(EquipmentTest, MoveConstructorWorks) {
    Equipment original("EQ123", EquipmentType::BULLDOZER, "Test Bulldozer");
    original.setStatus(EquipmentStatus::ACTIVE);
    
    Equipment moved(std::move(original));
    
    EXPECT_EQ("EQ123", moved.getId());
    EXPECT_EQ(EquipmentType::BULLDOZER, moved.getType());
    EXPECT_EQ("Test Bulldozer", moved.getName());
    EXPECT_EQ(EquipmentStatus::ACTIVE, moved.getStatus());
}

TEST_F(EquipmentTest, MoveAssignmentWorks) {
    Equipment original("EQ123", EquipmentType::BULLDOZER, "Test Bulldozer");
    original.setStatus(EquipmentStatus::ACTIVE);
    
    Equipment moved("EQ456", EquipmentType::CRANE, "Test Crane");
    moved = std::move(original);
    
    EXPECT_EQ("EQ123", moved.getId());
    EXPECT_EQ(EquipmentType::BULLDOZER, moved.getType());
    EXPECT_EQ("Test Bulldozer", moved.getName());
    EXPECT_EQ(EquipmentStatus::ACTIVE, moved.getStatus());
}

TEST_F(EquipmentTest, SettersAndGettersWork) {
    Equipment eq = createTestEquipment();
    
    eq.setStatus(EquipmentStatus::MAINTENANCE);
    EXPECT_EQ(EquipmentStatus::MAINTENANCE, eq.getStatus());
    
    eq.setName("Updated Bulldozer");
    EXPECT_EQ("Updated Bulldozer", eq.getName());
}

TEST_F(EquipmentTest, PositionManagementWorks) {
    Equipment eq = createTestEquipment();
    Position pos = createTestPosition();
    
    // Initially no position
    EXPECT_FALSE(eq.getLastPosition().has_value());
    
    // Set and get last position
    eq.setLastPosition(pos);
    auto lastPos = eq.getLastPosition();
    ASSERT_TRUE(lastPos.has_value());
    EXPECT_DOUBLE_EQ(pos.getLatitude(), lastPos->getLatitude());
    EXPECT_DOUBLE_EQ(pos.getLongitude(), lastPos->getLongitude());
    
    // Position history should be empty initially
    EXPECT_TRUE(eq.getPositionHistory().empty());
    
    // Record position and check history
    eq.recordPosition(pos);
    auto history = eq.getPositionHistory();
    ASSERT_EQ(1, history.size());
    EXPECT_DOUBLE_EQ(pos.getLatitude(), history[0].getLatitude());
    
    // Clear history
    eq.clearPositionHistory();
    EXPECT_TRUE(eq.getPositionHistory().empty());
}

TEST_F(EquipmentTest, PositionHistoryLimitsSize) {
    Equipment eq = createTestEquipment();
    
    // Add more positions than the default max history size
    for (size_t i = 0; i < DEFAULT_MAX_HISTORY_SIZE + 10; i++) {
        Position pos = createTestPosition(40.0 + i * 0.01, -74.0, 10.0, 1609459200 + i);
        eq.recordPosition(pos);
    }
    
    // History should be limited to DEFAULT_MAX_HISTORY_SIZE
    EXPECT_EQ(DEFAULT_MAX_HISTORY_SIZE, eq.getPositionHistory().size());
}

TEST_F(EquipmentTest, IsMovingWorks) {
    Equipment eq = createTestEquipment();
    
    // No position, should not be moving
    EXPECT_FALSE(eq.isMoving());
    
    // Add a position, still should not be moving with just one position
    Position pos1 = createTestPosition(40.0, -74.0, 10.0, 1609459200);
    eq.recordPosition(pos1);
    EXPECT_FALSE(eq.isMoving());
    
    // Add a different position with later timestamp, should be moving
    Position pos2 = createTestPosition(40.1, -74.1, 10.0, 1609459300);
    eq.recordPosition(pos2);
    EXPECT_TRUE(eq.isMoving());
    
    // Add same position with later timestamp, should not be moving
    Position pos3 = createTestPosition(40.1, -74.1, 10.0, 1609459400);
    eq.recordPosition(pos3);
    EXPECT_FALSE(eq.isMoving());
}

TEST_F(EquipmentTest, ToStringReturnsExpectedFormat) {
    Equipment eq = createTestEquipment();
    eq.setStatus(EquipmentStatus::ACTIVE);
    
    std::string result = eq.toString();
    
    EXPECT_THAT(result, ::testing::HasSubstr("EQ123"));
    EXPECT_THAT(result, ::testing::HasSubstr("Test Bulldozer"));
    EXPECT_THAT(result, ::testing::HasSubstr("BULLDOZER"));
    EXPECT_THAT(result, ::testing::HasSubstr("ACTIVE"));
}

TEST_F(EquipmentTest, ThreadSafetyForPositionAccess) {
    Equipment eq = createTestEquipment();
    
    // Create multiple threads that update and read the position
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&eq, i]() {
            Position pos = Position(40.0 + i * 0.1, -74.0 + i * 0.1, 10.0, 1609459200 + i);
            eq.setLastPosition(pos);
            eq.recordPosition(pos);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            eq.getLastPosition();
            eq.getPositionHistory();
        });
    }
    
    // Join all threads
    for (auto& t : threads) {
        t.join();
    }
    
    // If we got here without crashes or exceptions, the test passes
    SUCCEED();
}

} // namespace equipment_tracker
// </test_code>