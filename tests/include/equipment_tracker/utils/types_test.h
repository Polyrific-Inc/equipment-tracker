// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <chrono>
#include <functional>
#include <thread>

// Include the header file being tested
#include "equipment_tracker.h" // Assuming this is the name of the main_file

namespace {

class EquipmentTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Teardown code if needed
    }
};

// Test the getCurrentTimestamp function
TEST_F(EquipmentTrackerTest, GetCurrentTimestampReturnsCurrentTime) {
    // Get the current time before calling the function
    auto beforeTime = std::chrono::system_clock::now();
    
    // Call the function
    auto timestamp = equipment_tracker::getCurrentTimestamp();
    
    // Get the current time after calling the function
    auto afterTime = std::chrono::system_clock::now();
    
    // The timestamp should be between beforeTime and afterTime
    EXPECT_GE(timestamp, beforeTime);
    EXPECT_LE(timestamp, afterTime);
}

// Test the type aliases
TEST_F(EquipmentTrackerTest, TypeAliasesWork) {
    // Test EquipmentId
    equipment_tracker::EquipmentId id = "FORK-123";
    EXPECT_EQ(id, "FORK-123");
    
    // Test Timestamp
    equipment_tracker::Timestamp now = std::chrono::system_clock::now();
    EXPECT_EQ(now, now); // Simple equality check
    
    // Test PositionCallback
    bool callbackCalled = false;
    equipment_tracker::PositionCallback callback = 
        [&callbackCalled](double latitude, double longitude, double altitude, equipment_tracker::Timestamp timestamp) {
            callbackCalled = true;
            EXPECT_NEAR(latitude, 45.0, 0.001);
            EXPECT_NEAR(longitude, -75.0, 0.001);
            EXPECT_NEAR(altitude, 100.0, 0.001);
            EXPECT_NE(timestamp, equipment_tracker::Timestamp{});
        };
    
    // Call the callback
    callback(45.0, -75.0, 100.0, std::chrono::system_clock::now());
    EXPECT_TRUE(callbackCalled);
}

// Test the EquipmentType enum
TEST_F(EquipmentTrackerTest, EquipmentTypeEnumValues) {
    // Test that we can assign and compare enum values
    equipment_tracker::EquipmentType type1 = equipment_tracker::EquipmentType::Forklift;
    equipment_tracker::EquipmentType type2 = equipment_tracker::EquipmentType::Crane;
    equipment_tracker::EquipmentType type3 = equipment_tracker::EquipmentType::Bulldozer;
    equipment_tracker::EquipmentType type4 = equipment_tracker::EquipmentType::Excavator;
    equipment_tracker::EquipmentType type5 = equipment_tracker::EquipmentType::Truck;
    equipment_tracker::EquipmentType type6 = equipment_tracker::EquipmentType::Other;
    
    // Verify they are different
    EXPECT_NE(type1, type2);
    EXPECT_NE(type1, type3);
    EXPECT_NE(type1, type4);
    EXPECT_NE(type1, type5);
    EXPECT_NE(type1, type6);
    
    EXPECT_NE(type2, type3);
    EXPECT_NE(type2, type4);
    EXPECT_NE(type2, type5);
    EXPECT_NE(type2, type6);
    
    EXPECT_NE(type3, type4);
    EXPECT_NE(type3, type5);
    EXPECT_NE(type3, type6);
    
    EXPECT_NE(type4, type5);
    EXPECT_NE(type4, type6);
    
    EXPECT_NE(type5, type6);
}

// Test the EquipmentStatus enum
TEST_F(EquipmentTrackerTest, EquipmentStatusEnumValues) {
    // Test that we can assign and compare enum values
    equipment_tracker::EquipmentStatus status1 = equipment_tracker::EquipmentStatus::Active;
    equipment_tracker::EquipmentStatus status2 = equipment_tracker::EquipmentStatus::Inactive;
    equipment_tracker::EquipmentStatus status3 = equipment_tracker::EquipmentStatus::Maintenance;
    equipment_tracker::EquipmentStatus status4 = equipment_tracker::EquipmentStatus::Unknown;
    
    // Verify they are different
    EXPECT_NE(status1, status2);
    EXPECT_NE(status1, status3);
    EXPECT_NE(status1, status4);
    
    EXPECT_NE(status2, status3);
    EXPECT_NE(status2, status4);
    
    EXPECT_NE(status3, status4);
}

// Test timestamp comparison
TEST_F(EquipmentTrackerTest, TimestampComparison) {
    auto now = equipment_tracker::getCurrentTimestamp();
    
    // Sleep for a short time to ensure timestamps are different
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    
    auto later = equipment_tracker::getCurrentTimestamp();
    
    EXPECT_LT(now, later);
}

}  // namespace

// Implementation of getCurrentTimestamp for testing
// This would normally be in a separate .cpp file
namespace equipment_tracker {
    Timestamp getCurrentTimestamp() {
        return std::chrono::system_clock::now();
    }
}
// </test_code>