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
#define MKGMTIME _mkgmtime
#else
#define MKGMTIME timegm
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

    std::filesystem::path test_db_path_;
    std::unique_ptr<DataStorage> data_storage_;

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

    // Helper method to create a position at a specific time
    Position createPosition(double lat, double lon, double alt, double acc, time_t timestamp) {
        return Position(lat, lon, alt, acc, std::chrono::system_clock::from_time_t(timestamp));
    }
};

TEST_F(DataStorageTest, Initialize) {
    EXPECT_TRUE(data_storage_->initialize());
    
    // Check that the directories were created
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "positions"));
    
    // Initialize again should return true
    EXPECT_TRUE(data_storage_->initialize());
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save equipment
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
    
    // Verify position
    ASSERT_TRUE(loaded->getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(loaded->getLastPosition()->getLatitude(), original.getLastPosition()->getLatitude());
    EXPECT_DOUBLE_EQ(loaded->getLastPosition()->getLongitude(), original.getLastPosition()->getLongitude());
    EXPECT_DOUBLE_EQ(loaded->getLastPosition()->getAltitude(), original.getLastPosition()->getAltitude());
    EXPECT_DOUBLE_EQ(loaded->getLastPosition()->getAccuracy(), original.getLastPosition()->getAccuracy());
    
    // Timestamps might have slight differences due to serialization/deserialization
    auto original_time = std::chrono::system_clock::to_time_t(original.getLastPosition()->getTimestamp());
    auto loaded_time = std::chrono::system_clock::to_time_t(loaded->getLastPosition()->getTimestamp());
    EXPECT_EQ(loaded_time, original_time);
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Try to load non-existent equipment
    auto loaded = data_storage_->loadEquipment("nonexistent");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save equipment
    Equipment original = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(original));
    
    // Update equipment
    Equipment updated = original;
    updated.setName("Updated Name");
    updated.setStatus(EquipmentStatus::InUse);
    EXPECT_TRUE(data_storage_->updateEquipment(updated));
    
    // Load the equipment
    auto loaded = data_storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    // Verify the loaded equipment matches the updated version
    EXPECT_EQ(loaded->getName(), "Updated Name");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::InUse);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save equipment
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Verify it exists
    EXPECT_TRUE(data_storage_->loadEquipment(equipment.getId()).has_value());
    
    // Delete the equipment
    EXPECT_TRUE(data_storage_->deleteEquipment(equipment.getId()));
    
    // Verify it no longer exists
    EXPECT_FALSE(data_storage_->loadEquipment(equipment.getId()).has_value());
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create positions at different times
    std::string id = equipment.getId();
    time_t now = std::time(nullptr);
    
    Position pos1 = createPosition(37.7749, -122.4194, 10.0, 5.0, now - 3600); // 1 hour ago
    Position pos2 = createPosition(37.7750, -122.4195, 11.0, 4.0, now - 1800); // 30 minutes ago
    Position pos3 = createPosition(37.7751, -122.4196, 12.0, 3.0, now);        // now
    
    // Save positions
    EXPECT_TRUE(data_storage_->savePosition(id, pos1));
    EXPECT_TRUE(data_storage_->savePosition(id, pos2));
    EXPECT_TRUE(data_storage_->savePosition(id, pos3));
    
    // Get position history for the full time range
    auto history = data_storage_->getPositionHistory(
        id, 
        std::chrono::system_clock::from_time_t(now - 7200),  // 2 hours ago
        std::chrono::system_clock::from_time_t(now + 3600)   // 1 hour from now
    );
    
    // Verify all positions are returned
    ASSERT_EQ(history.size(), 3);
    
    // Verify positions are in chronological order
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), pos1.getLatitude());
    EXPECT_DOUBLE_EQ(history[1].getLatitude(), pos2.getLatitude());
    EXPECT_DOUBLE_EQ(history[2].getLatitude(), pos3.getLatitude());
    
    // Get position history for a limited time range
    auto limited_history = data_storage_->getPositionHistory(
        id, 
        std::chrono::system_clock::from_time_t(now - 2000),  // 33.3 minutes ago
        std::chrono::system_clock::from_time_t(now - 1000)   // 16.7 minutes ago
    );
    
    // Verify only the middle position is returned
    ASSERT_EQ(limited_history.size(), 1);
    EXPECT_DOUBLE_EQ(limited_history[0].getLatitude(), pos2.getLatitude());
}

TEST_F(DataStorageTest, GetAllEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save multiple equipment
    Equipment equip1 = createTestEquipment("equip1");
    Equipment equip2 = createTestEquipment("equip2");
    Equipment equip3 = createTestEquipment("equip3");
    
    EXPECT_TRUE(data_storage_->saveEquipment(equip1));
    EXPECT_TRUE(data_storage_->saveEquipment(equip2));
    EXPECT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Get all equipment
    auto all_equipment = data_storage_->getAllEquipment();
    
    // Verify all equipment are returned
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
    
    EXPECT_TRUE(data_storage_->saveEquipment(equip1));
    EXPECT_TRUE(data_storage_->saveEquipment(equip2));
    EXPECT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Find equipment by status
    auto available = data_storage_->findEquipmentByStatus(EquipmentStatus::Available);
    auto in_use = data_storage_->findEquipmentByStatus(EquipmentStatus::InUse);
    auto maintenance = data_storage_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    
    // Verify results
    ASSERT_EQ(available.size(), 2);
    ASSERT_EQ(in_use.size(), 1);
    ASSERT_EQ(maintenance.size(), 0);
    
    // Verify the equipment IDs
    std::vector<std::string> available_ids;
    for (const auto& equip : available) {
        available_ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(available_ids, ::testing::UnorderedElementsAre("equip1", "equip3"));
    EXPECT_EQ(in_use[0].getId(), "equip2");
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
    
    EXPECT_TRUE(data_storage_->saveEquipment(equip1));
    EXPECT_TRUE(data_storage_->saveEquipment(equip2));
    EXPECT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Find equipment by type
    auto vehicles = data_storage_->findEquipmentByType(EquipmentType::Vehicle);
    auto tools = data_storage_->findEquipmentByType(EquipmentType::Tool);
    auto other = data_storage_->findEquipmentByType(EquipmentType::Other);
    
    // Verify results
    ASSERT_EQ(vehicles.size(), 2);
    ASSERT_EQ(tools.size(), 1);
    ASSERT_EQ(other.size(), 0);
    
    // Verify the equipment IDs
    std::vector<std::string> vehicle_ids;
    for (const auto& equip : vehicles) {
        vehicle_ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(vehicle_ids, ::testing::UnorderedElementsAre("equip1", "equip3"));
    EXPECT_EQ(tools[0].getId(), "equip2");
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment at different locations
    Equipment equip1 = createTestEquipment("equip1");
    equip1.setLastPosition(Position(37.7749, -122.4194, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment equip2 = createTestEquipment("equip2");
    equip2.setLastPosition(Position(40.7128, -74.0060, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment equip3 = createTestEquipment("equip3");
    equip3.setLastPosition(Position(37.3382, -121.8863, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment equip4 = createTestEquipment("equip4");
    // No position set for equip4
    
    EXPECT_TRUE(data_storage_->saveEquipment(equip1));
    EXPECT_TRUE(data_storage_->saveEquipment(equip2));
    EXPECT_TRUE(data_storage_->saveEquipment(equip3));
    EXPECT_TRUE(data_storage_->saveEquipment(equip4));
    
    // Find equipment in San Francisco Bay Area
    auto bay_area = data_storage_->findEquipmentInArea(37.0, -123.0, 38.0, -121.0);
    
    // Verify results
    ASSERT_EQ(bay_area.size(), 2);
    
    // Verify the equipment IDs
    std::vector<std::string> bay_area_ids;
    for (const auto& equip : bay_area) {
        bay_area_ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(bay_area_ids, ::testing::UnorderedElementsAre("equip1", "equip3"));
    
    // Find equipment in a smaller area (just San Francisco)
    auto sf_only = data_storage_->findEquipmentInArea(37.7, -122.5, 37.8, -122.4);
    
    // Verify results
    ASSERT_EQ(sf_only.size(), 1);
    EXPECT_EQ(sf_only[0].getId(), "equip1");
    
    // Find equipment in an area with no equipment
    auto empty_area = data_storage_->findEquipmentInArea(0.0, 0.0, 1.0, 1.0);
    EXPECT_TRUE(empty_area.empty());
}

TEST_F(DataStorageTest, ExecuteQuery) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // This is just testing the placeholder implementation
    testing::internal::CaptureStdout();
    bool result = data_storage_->executeQuery("SELECT * FROM equipment");
    std::string output = testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(result);
    EXPECT_THAT(output, ::testing::HasSubstr("Query: SELECT * FROM equipment"));
}

} // namespace equipment_tracker
// </test_code>