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
        
        // Create the storage with the test path
        storage_ = std::make_unique<DataStorage>(test_db_path_.string());
    }

    void TearDown() override {
        // Clean up after the test
        storage_.reset();
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
        Position position(40.7128, -74.0060, 10.0, 5.0, now);
        equipment.setLastPosition(position);
        
        return equipment;
    }

    // Helper method to create a position at a specific time
    Position createPosition(double lat, double lon, double alt, double acc, 
                           const std::chrono::system_clock::time_point& time) {
        return Position(lat, lon, alt, acc, time);
    }

    std::filesystem::path test_db_path_;
    std::unique_ptr<DataStorage> storage_;
};

TEST_F(DataStorageTest, InitializeCreatesDirectories) {
    ASSERT_TRUE(storage_->initialize());
    
    // Check that the directories were created
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "positions"));
}

TEST_F(DataStorageTest, InitializeIsIdempotent) {
    ASSERT_TRUE(storage_->initialize());
    ASSERT_TRUE(storage_->initialize());  // Should still return true
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    ASSERT_TRUE(storage_->initialize());
    
    // Create and save equipment
    Equipment original = createTestEquipment();
    ASSERT_TRUE(storage_->saveEquipment(original));
    
    // Load the equipment
    auto loaded = storage_->loadEquipment(original.getId());
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
    
    // Timestamps might have slight differences due to serialization/deserialization
    auto original_time = std::chrono::system_clock::to_time_t(original.getLastPosition()->getTimestamp());
    auto loaded_time = std::chrono::system_clock::to_time_t(loaded->getLastPosition()->getTimestamp());
    EXPECT_EQ(loaded_time, original_time);
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    ASSERT_TRUE(storage_->initialize());
    
    // Try to load non-existent equipment
    auto loaded = storage_->loadEquipment("nonexistent");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    ASSERT_TRUE(storage_->initialize());
    
    // Create and save equipment
    Equipment original = createTestEquipment();
    ASSERT_TRUE(storage_->saveEquipment(original));
    
    // Update the equipment
    Equipment updated = original;
    updated.setName("Updated Name");
    updated.setStatus(EquipmentStatus::InUse);
    
    // Save the updated equipment
    ASSERT_TRUE(storage_->updateEquipment(updated));
    
    // Load the equipment
    auto loaded = storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    // Verify the loaded equipment matches the updated version
    EXPECT_EQ(loaded->getName(), "Updated Name");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::InUse);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    ASSERT_TRUE(storage_->initialize());
    
    // Create and save equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(storage_->saveEquipment(equipment));
    
    // Save a position for this equipment
    auto position = equipment.getLastPosition().value();
    ASSERT_TRUE(storage_->savePosition(equipment.getId(), position));
    
    // Delete the equipment
    ASSERT_TRUE(storage_->deleteEquipment(equipment.getId()));
    
    // Try to load the deleted equipment
    auto loaded = storage_->loadEquipment(equipment.getId());
    EXPECT_FALSE(loaded.has_value());
    
    // Verify the position directory is also deleted
    std::string position_dir = (test_db_path_ / "positions" / equipment.getId()).string();
    EXPECT_FALSE(std::filesystem::exists(position_dir));
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    ASSERT_TRUE(storage_->initialize());
    
    // Create equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(storage_->saveEquipment(equipment));
    
    // Create positions at different times
    auto now = std::chrono::system_clock::now();
    auto one_hour_ago = now - std::chrono::hours(1);
    auto two_hours_ago = now - std::chrono::hours(2);
    
    Position pos1(40.7128, -74.0060, 10.0, 5.0, two_hours_ago);
    Position pos2(40.7129, -74.0061, 11.0, 4.0, one_hour_ago);
    Position pos3(40.7130, -74.0062, 12.0, 3.0, now);
    
    // Save positions
    ASSERT_TRUE(storage_->savePosition(equipment.getId(), pos1));
    ASSERT_TRUE(storage_->savePosition(equipment.getId(), pos2));
    ASSERT_TRUE(storage_->savePosition(equipment.getId(), pos3));
    
    // Get position history for the entire time range
    auto history = storage_->getPositionHistory(
        equipment.getId(), two_hours_ago, now);
    
    // Verify we got all positions
    ASSERT_EQ(history.size(), 3);
    
    // Positions should be sorted by timestamp
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), pos1.getLatitude());
    EXPECT_DOUBLE_EQ(history[1].getLatitude(), pos2.getLatitude());
    EXPECT_DOUBLE_EQ(history[2].getLatitude(), pos3.getLatitude());
    
    // Get position history for a partial time range
    auto partial_history = storage_->getPositionHistory(
        equipment.getId(), one_hour_ago, now);
    
    // Verify we got only the positions in the time range
    ASSERT_EQ(partial_history.size(), 2);
    EXPECT_DOUBLE_EQ(partial_history[0].getLatitude(), pos2.getLatitude());
    EXPECT_DOUBLE_EQ(partial_history[1].getLatitude(), pos3.getLatitude());
}

TEST_F(DataStorageTest, GetAllEquipment) {
    ASSERT_TRUE(storage_->initialize());
    
    // Create and save multiple equipment
    Equipment equip1 = createTestEquipment("equip1");
    Equipment equip2 = createTestEquipment("equip2");
    Equipment equip3 = createTestEquipment("equip3");
    
    ASSERT_TRUE(storage_->saveEquipment(equip1));
    ASSERT_TRUE(storage_->saveEquipment(equip2));
    ASSERT_TRUE(storage_->saveEquipment(equip3));
    
    // Get all equipment
    auto all_equipment = storage_->getAllEquipment();
    
    // Verify we got all equipment
    ASSERT_EQ(all_equipment.size(), 3);
    
    // Verify the IDs match (order may vary)
    std::vector<std::string> ids;
    for (const auto& equip : all_equipment) {
        ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("equip1", "equip2", "equip3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    ASSERT_TRUE(storage_->initialize());
    
    // Create equipment with different statuses
    Equipment equip1 = createTestEquipment("equip1");
    equip1.setStatus(EquipmentStatus::Available);
    
    Equipment equip2 = createTestEquipment("equip2");
    equip2.setStatus(EquipmentStatus::InUse);
    
    Equipment equip3 = createTestEquipment("equip3");
    equip3.setStatus(EquipmentStatus::Available);
    
    ASSERT_TRUE(storage_->saveEquipment(equip1));
    ASSERT_TRUE(storage_->saveEquipment(equip2));
    ASSERT_TRUE(storage_->saveEquipment(equip3));
    
    // Find equipment by status
    auto available = storage_->findEquipmentByStatus(EquipmentStatus::Available);
    auto in_use = storage_->findEquipmentByStatus(EquipmentStatus::InUse);
    auto maintenance = storage_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    
    // Verify results
    ASSERT_EQ(available.size(), 2);
    ASSERT_EQ(in_use.size(), 1);
    ASSERT_EQ(maintenance.size(), 0);
    
    // Verify the correct equipment was found
    std::vector<std::string> available_ids;
    for (const auto& equip : available) {
        available_ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(available_ids, ::testing::UnorderedElementsAre("equip1", "equip3"));
    EXPECT_EQ(in_use[0].getId(), "equip2");
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    ASSERT_TRUE(storage_->initialize());
    
    // Create equipment with different types
    Equipment equip1 = createTestEquipment("equip1");
    equip1.setType(EquipmentType::Vehicle);
    
    Equipment equip2 = createTestEquipment("equip2");
    equip2.setType(EquipmentType::Tool);
    
    Equipment equip3 = createTestEquipment("equip3");
    equip3.setType(EquipmentType::Vehicle);
    
    ASSERT_TRUE(storage_->saveEquipment(equip1));
    ASSERT_TRUE(storage_->saveEquipment(equip2));
    ASSERT_TRUE(storage_->saveEquipment(equip3));
    
    // Find equipment by type
    auto vehicles = storage_->findEquipmentByType(EquipmentType::Vehicle);
    auto tools = storage_->findEquipmentByType(EquipmentType::Tool);
    auto other = storage_->findEquipmentByType(EquipmentType::Other);
    
    // Verify results
    ASSERT_EQ(vehicles.size(), 2);
    ASSERT_EQ(tools.size(), 1);
    ASSERT_EQ(other.size(), 0);
    
    // Verify the correct equipment was found
    std::vector<std::string> vehicle_ids;
    for (const auto& equip : vehicles) {
        vehicle_ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(vehicle_ids, ::testing::UnorderedElementsAre("equip1", "equip3"));
    EXPECT_EQ(tools[0].getId(), "equip2");
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    ASSERT_TRUE(storage_->initialize());
    
    // Create equipment at different locations
    Equipment equip1 = createTestEquipment("equip1");
    equip1.setLastPosition(Position(40.7128, -74.0060, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment equip2 = createTestEquipment("equip2");
    equip2.setLastPosition(Position(37.7749, -122.4194, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment equip3 = createTestEquipment("equip3");
    equip3.setLastPosition(Position(40.7300, -74.0200, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment equip4 = createTestEquipment("equip4");
    // No position for equip4
    
    ASSERT_TRUE(storage_->saveEquipment(equip1));
    ASSERT_TRUE(storage_->saveEquipment(equip2));
    ASSERT_TRUE(storage_->saveEquipment(equip3));
    ASSERT_TRUE(storage_->saveEquipment(equip4));
    
    // Find equipment in NYC area
    auto nyc_area = storage_->findEquipmentInArea(40.70, -74.05, 40.75, -74.00);
    
    // Verify results
    ASSERT_EQ(nyc_area.size(), 1);
    EXPECT_EQ(nyc_area[0].getId(), "equip1");
    
    // Find equipment in a larger area
    auto larger_area = storage_->findEquipmentInArea(40.70, -74.05, 40.75, -74.00);
    ASSERT_EQ(larger_area.size(), 1);
    
    // Find equipment in an area with no equipment
    auto empty_area = storage_->findEquipmentInArea(41.0, -75.0, 42.0, -76.0);
    ASSERT_EQ(empty_area.size(), 0);
}

TEST_F(DataStorageTest, ExecuteQuery) {
    ASSERT_TRUE(storage_->initialize());
    
    // This is just testing the placeholder implementation
    testing::internal::CaptureStdout();
    bool result = storage_->executeQuery("SELECT * FROM equipment");
    std::string output = testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(result);
    EXPECT_THAT(output, ::testing::HasSubstr("Query: SELECT * FROM equipment"));
}

} // namespace equipment_tracker
// </test_code>