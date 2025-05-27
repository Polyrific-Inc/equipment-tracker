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

// Platform-specific time functions
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
        
        // Create a fresh test directory
        std::filesystem::create_directory(test_db_path_);
        
        // Initialize the data storage with the test directory
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

    // Helper method to create a position at a specific time
    Position createPosition(double lat, double lon, double alt, double acc, 
                           std::chrono::system_clock::time_point time) {
        return Position(lat, lon, alt, acc, time);
    }

    std::filesystem::path test_db_path_;
    std::unique_ptr<DataStorage> data_storage_;
};

TEST_F(DataStorageTest, InitializeCreatesDirectories) {
    ASSERT_TRUE(data_storage_->initialize());
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "positions"));
}

TEST_F(DataStorageTest, InitializeIsIdempotent) {
    ASSERT_TRUE(data_storage_->initialize());
    ASSERT_TRUE(data_storage_->initialize());
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment original = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(original));
    
    auto loaded = data_storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    EXPECT_EQ(loaded->getId(), original.getId());
    EXPECT_EQ(loaded->getName(), original.getName());
    EXPECT_EQ(loaded->getType(), original.getType());
    EXPECT_EQ(loaded->getStatus(), original.getStatus());
    
    auto original_pos = original.getLastPosition();
    auto loaded_pos = loaded->getLastPosition();
    
    ASSERT_TRUE(original_pos.has_value());
    ASSERT_TRUE(loaded_pos.has_value());
    
    EXPECT_DOUBLE_EQ(loaded_pos->getLatitude(), original_pos->getLatitude());
    EXPECT_DOUBLE_EQ(loaded_pos->getLongitude(), original_pos->getLongitude());
    EXPECT_DOUBLE_EQ(loaded_pos->getAltitude(), original_pos->getAltitude());
    EXPECT_DOUBLE_EQ(loaded_pos->getAccuracy(), original_pos->getAccuracy());
    
    // Timestamps might have slight differences due to serialization/deserialization
    auto original_time = std::chrono::system_clock::to_time_t(original_pos->getTimestamp());
    auto loaded_time = std::chrono::system_clock::to_time_t(loaded_pos->getTimestamp());
    EXPECT_EQ(loaded_time, original_time);
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    auto loaded = data_storage_->loadEquipment("nonexistent");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment original = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(original));
    
    // Update equipment
    Equipment updated = original;
    updated.setName("Updated Name");
    updated.setStatus(EquipmentStatus::InUse);
    
    ASSERT_TRUE(data_storage_->updateEquipment(updated));
    
    auto loaded = data_storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    EXPECT_EQ(loaded->getName(), "Updated Name");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::InUse);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Save a position to create position history
    auto position = equipment.getLastPosition().value();
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), position));
    
    // Delete the equipment
    ASSERT_TRUE(data_storage_->deleteEquipment(equipment.getId()));
    
    // Verify it's gone
    auto loaded = data_storage_->loadEquipment(equipment.getId());
    EXPECT_FALSE(loaded.has_value());
    
    // Verify position directory is gone
    std::string history_dir = test_db_path_.string() + "/positions/" + equipment.getId();
    EXPECT_FALSE(std::filesystem::exists(history_dir));
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create positions at different times
    auto now = std::chrono::system_clock::now();
    auto hour_ago = now - std::chrono::hours(1);
    auto two_hours_ago = now - std::chrono::hours(2);
    auto three_hours_ago = now - std::chrono::hours(3);
    
    Position pos1(37.7749, -122.4194, 10.0, 5.0, three_hours_ago);
    Position pos2(37.7750, -122.4195, 11.0, 4.0, two_hours_ago);
    Position pos3(37.7751, -122.4196, 12.0, 3.0, hour_ago);
    Position pos4(37.7752, -122.4197, 13.0, 2.0, now);
    
    // Save positions
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos1));
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos2));
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos3));
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos4));
    
    // Get all position history
    auto history = data_storage_->getPositionHistory(equipment.getId());
    EXPECT_EQ(history.size(), 4);
    
    // Get position history with time range
    auto filtered_history = data_storage_->getPositionHistory(
        equipment.getId(), two_hours_ago, hour_ago);
    EXPECT_EQ(filtered_history.size(), 2);
    
    // Verify positions are in chronological order
    if (filtered_history.size() >= 2) {
        EXPECT_LE(
            std::chrono::system_clock::to_time_t(filtered_history[0].getTimestamp()),
            std::chrono::system_clock::to_time_t(filtered_history[1].getTimestamp())
        );
    }
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
    EXPECT_EQ(all_equipment.size(), 3);
    
    // Verify all equipment are loaded
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
    auto available = data_storage_->findEquipmentByStatus(EquipmentStatus::Available);
    EXPECT_EQ(available.size(), 2);
    
    auto in_use = data_storage_->findEquipmentByStatus(EquipmentStatus::InUse);
    EXPECT_EQ(in_use.size(), 1);
    
    auto maintenance = data_storage_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(maintenance.size(), 0);
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
    auto vehicles = data_storage_->findEquipmentByType(EquipmentType::Vehicle);
    EXPECT_EQ(vehicles.size(), 2);
    
    auto tools = data_storage_->findEquipmentByType(EquipmentType::Tool);
    EXPECT_EQ(tools.size(), 1);
    
    auto electronics = data_storage_->findEquipmentByType(EquipmentType::Electronics);
    EXPECT_EQ(electronics.size(), 0);
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment at different locations
    Equipment equip1 = createTestEquipment("equip1");
    equip1.setLastPosition(Position(37.7749, -122.4194, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment equip2 = createTestEquipment("equip2");
    equip2.setLastPosition(Position(37.3382, -121.8863, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment equip3 = createTestEquipment("equip3");
    equip3.setLastPosition(Position(37.7749, -122.4194, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment equip4 = createTestEquipment("equip4");
    // No position set for equip4
    
    ASSERT_TRUE(data_storage_->saveEquipment(equip1));
    ASSERT_TRUE(data_storage_->saveEquipment(equip2));
    ASSERT_TRUE(data_storage_->saveEquipment(equip3));
    ASSERT_TRUE(data_storage_->saveEquipment(equip4));
    
    // Find equipment in San Francisco area
    auto sf_equipment = data_storage_->findEquipmentInArea(37.7, -122.5, 37.8, -122.4);
    EXPECT_EQ(sf_equipment.size(), 2);
    
    // Find equipment in San Jose area
    auto sj_equipment = data_storage_->findEquipmentInArea(37.3, -121.9, 37.4, -121.8);
    EXPECT_EQ(sj_equipment.size(), 1);
    
    // Find equipment in a region with no equipment
    auto no_equipment = data_storage_->findEquipmentInArea(38.0, -123.0, 38.1, -122.9);
    EXPECT_EQ(no_equipment.size(), 0);
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