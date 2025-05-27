// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <ctime>
#include <chrono>
#include <thread>
#include "equipment_tracker/data_storage.h"

// Platform-specific time handling
#ifdef _WIN32
#define timegm _mkgmtime
#endif

namespace equipment_tracker {

class DataStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for testing
        test_db_path_ = std::filesystem::temp_directory_path() / "equipment_tracker_test";
        
        // Clean up any existing test directory
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove_all(test_db_path_);
        }
        
        // Create the data storage with the test path
        data_storage_ = std::make_unique<DataStorage>(test_db_path_.string());
    }

    void TearDown() override {
        // Clean up the test directory
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove_all(test_db_path_);
        }
    }

    // Helper method to create a test equipment
    Equipment createTestEquipment(const std::string& id = "test123") {
        Equipment equipment(id, EquipmentType::Vehicle, "Test Equipment");
        equipment.setStatus(EquipmentStatus::Available);
        
        // Add a position
        auto now = std::chrono::system_clock::now();
        Position position(37.7749, -122.4194, 10.0, 5.0, now);
        equipment.setLastPosition(position);
        
        return equipment;
    }

    // Helper method to create a position
    Position createTestPosition(double lat = 37.7749, double lon = -122.4194, 
                               double alt = 10.0, double acc = 5.0) {
        auto now = std::chrono::system_clock::now();
        return Position(lat, lon, alt, acc, now);
    }

    std::filesystem::path test_db_path_;
    std::unique_ptr<DataStorage> data_storage_;
};

TEST_F(DataStorageTest, Initialize) {
    EXPECT_TRUE(data_storage_->initialize());
    
    // Check that the database directories were created
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "positions"));
    
    // Initialize again should return true
    EXPECT_TRUE(data_storage_->initialize());
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment original = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(original));
    
    // Load the equipment
    auto loaded = data_storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    // Verify the loaded equipment matches the original
    EXPECT_EQ(loaded->getId(), original.getId());
    EXPECT_EQ(loaded->getName(), original.getName());
    EXPECT_EQ(loaded->getType(), original.getType());
    EXPECT_EQ(loaded->getStatus(), original.getStatus());
    
    // Verify position data
    ASSERT_TRUE(loaded->getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(loaded->getLastPosition()->getLatitude(), original.getLastPosition()->getLatitude());
    EXPECT_DOUBLE_EQ(loaded->getLastPosition()->getLongitude(), original.getLastPosition()->getLongitude());
    EXPECT_DOUBLE_EQ(loaded->getLastPosition()->getAltitude(), original.getLastPosition()->getAltitude());
    EXPECT_DOUBLE_EQ(loaded->getLastPosition()->getAccuracy(), original.getLastPosition()->getAccuracy());
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Try to load non-existent equipment
    auto loaded = data_storage_->loadEquipment("nonexistent");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment original = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(original));
    
    // Update the equipment
    Equipment updated = original;
    updated.setName("Updated Name");
    updated.setStatus(EquipmentStatus::InUse);
    EXPECT_TRUE(data_storage_->updateEquipment(updated));
    
    // Load the equipment and verify updates
    auto loaded = data_storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getName(), "Updated Name");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::InUse);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Save a position for the equipment
    Position position = createTestPosition();
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), position));
    
    // Delete the equipment
    EXPECT_TRUE(data_storage_->deleteEquipment(equipment.getId()));
    
    // Verify equipment is deleted
    auto loaded = data_storage_->loadEquipment(equipment.getId());
    EXPECT_FALSE(loaded.has_value());
    
    // Verify position directory is deleted
    std::string position_dir = (test_db_path_ / "positions" / equipment.getId()).string();
    EXPECT_FALSE(std::filesystem::exists(position_dir));
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create test equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create and save positions with different timestamps
    auto now = std::chrono::system_clock::now();
    auto one_hour_ago = now - std::chrono::hours(1);
    auto two_hours_ago = now - std::chrono::hours(2);
    
    Position pos1(37.7749, -122.4194, 10.0, 5.0, two_hours_ago);
    Position pos2(37.7750, -122.4195, 11.0, 6.0, one_hour_ago);
    Position pos3(37.7751, -122.4196, 12.0, 7.0, now);
    
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos1));
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos2));
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos3));
    
    // Get position history for the full time range
    auto three_hours_ago = now - std::chrono::hours(3);
    auto one_hour_future = now + std::chrono::hours(1);
    
    auto history = data_storage_->getPositionHistory(
        equipment.getId(), three_hours_ago, one_hour_future);
    
    // Verify we got all three positions
    ASSERT_EQ(history.size(), 3);
    
    // Verify positions are sorted by timestamp
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), pos1.getLatitude());
    EXPECT_DOUBLE_EQ(history[1].getLatitude(), pos2.getLatitude());
    EXPECT_DOUBLE_EQ(history[2].getLatitude(), pos3.getLatitude());
    
    // Get position history for a limited time range
    auto ninety_mins_ago = now - std::chrono::minutes(90);
    auto thirty_mins_ago = now - std::chrono::minutes(30);
    
    history = data_storage_->getPositionHistory(
        equipment.getId(), ninety_mins_ago, thirty_mins_ago);
    
    // Verify we got only the middle position
    ASSERT_EQ(history.size(), 1);
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), pos2.getLatitude());
}

TEST_F(DataStorageTest, GetAllEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save multiple equipment
    Equipment equip1 = createTestEquipment("equip1");
    Equipment equip2 = createTestEquipment("equip2");
    Equipment equip3 = createTestEquipment("equip3");
    
    ASSERT_TRUE(data_storage_->saveEquipment(equip1));
    ASSERT_TRUE(data_storage_->saveEquipment(equip2));
    ASSERT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Get all equipment
    auto all_equipment = data_storage_->getAllEquipment();
    
    // Verify we got all three equipment
    ASSERT_EQ(all_equipment.size(), 3);
    
    // Verify the equipment IDs
    std::vector<std::string> ids;
    for (const auto& equip : all_equipment) {
        ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("equip1", "equip2", "equip3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different statuses
    Equipment equip1 = createTestEquipment("equip1");
    equip1.setStatus(EquipmentStatus::Available);
    
    Equipment equip2 = createTestEquipment("equip2");
    equip2.setStatus(EquipmentStatus::InUse);
    
    Equipment equip3 = createTestEquipment("equip3");
    equip3.setStatus(EquipmentStatus::Available);
    
    ASSERT_TRUE(data_storage_->saveEquipment(equip1));
    ASSERT_TRUE(data_storage_->saveEquipment(equip2));
    ASSERT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Find equipment by status
    auto available_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Available);
    
    // Verify we got the two available equipment
    ASSERT_EQ(available_equipment.size(), 2);
    
    // Verify the equipment IDs
    std::vector<std::string> ids;
    for (const auto& equip : available_equipment) {
        ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("equip1", "equip3"));
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different types
    Equipment equip1 = createTestEquipment("equip1");
    equip1.setType(EquipmentType::Vehicle);
    
    Equipment equip2 = createTestEquipment("equip2");
    equip2.setType(EquipmentType::Tool);
    
    Equipment equip3 = createTestEquipment("equip3");
    equip3.setType(EquipmentType::Vehicle);
    
    ASSERT_TRUE(data_storage_->saveEquipment(equip1));
    ASSERT_TRUE(data_storage_->saveEquipment(equip2));
    ASSERT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Find equipment by type
    auto vehicle_equipment = data_storage_->findEquipmentByType(EquipmentType::Vehicle);
    
    // Verify we got the two vehicle equipment
    ASSERT_EQ(vehicle_equipment.size(), 2);
    
    // Verify the equipment IDs
    std::vector<std::string> ids;
    for (const auto& equip : vehicle_equipment) {
        ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("equip1", "equip3"));
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different positions
    Equipment equip1 = createTestEquipment("equip1");
    equip1.setLastPosition(Position(37.7749, -122.4194, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment equip2 = createTestEquipment("equip2");
    equip2.setLastPosition(Position(40.7128, -74.0060, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment equip3 = createTestEquipment("equip3");
    equip3.setLastPosition(Position(37.3382, -121.8863, 10.0, 5.0, std::chrono::system_clock::now()));
    
    ASSERT_TRUE(data_storage_->saveEquipment(equip1));
    ASSERT_TRUE(data_storage_->saveEquipment(equip2));
    ASSERT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Find equipment in San Francisco Bay Area
    auto bay_area_equipment = data_storage_->findEquipmentInArea(
        37.0, -123.0, 38.0, -121.0);
    
    // Verify we got the two equipment in the Bay Area
    ASSERT_EQ(bay_area_equipment.size(), 2);
    
    // Verify the equipment IDs
    std::vector<std::string> ids;
    for (const auto& equip : bay_area_equipment) {
        ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("equip1", "equip3"));
}

TEST_F(DataStorageTest, ExecuteQuery) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Test the executeQuery method (which is a placeholder in the implementation)
    testing::internal::CaptureStdout();
    bool result = data_storage_->executeQuery("SELECT * FROM equipment");
    std::string output = testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(result);
    EXPECT_THAT(output, ::testing::HasSubstr("Query: SELECT * FROM equipment"));
}

TEST_F(DataStorageTest, SaveEquipmentWithoutInitialize) {
    // Don't call initialize() explicitly
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Verify the equipment was saved
    auto loaded = data_storage_->loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getId(), equipment.getId());
}

TEST_F(DataStorageTest, SavePositionWithoutInitialize) {
    // Don't call initialize() explicitly
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    Position position = createTestPosition();
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), position));
    
    // Verify the position was saved
    auto history = data_storage_->getPositionHistory(
        equipment.getId(), 
        std::chrono::system_clock::now() - std::chrono::hours(1),
        std::chrono::system_clock::now() + std::chrono::hours(1));
    
    ASSERT_EQ(history.size(), 1);
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), position.getLatitude());
}

TEST_F(DataStorageTest, GetPositionHistoryForNonExistentEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Get position history for non-existent equipment
    auto history = data_storage_->getPositionHistory(
        "nonexistent", 
        std::chrono::system_clock::now() - std::chrono::hours(1),
        std::chrono::system_clock::now());
    
    // Verify we got an empty vector
    EXPECT_TRUE(history.empty());
}

TEST_F(DataStorageTest, GetAllEquipmentFromEmptyDatabase) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Get all equipment from empty database
    auto all_equipment = data_storage_->getAllEquipment();
    
    // Verify we got an empty vector
    EXPECT_TRUE(all_equipment.empty());
}

}  // namespace equipment_tracker
// </test_code>