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
    Position createPosition(double lat, double lon, double alt, double acc, 
                           std::chrono::system_clock::time_point time) {
        return Position(lat, lon, alt, acc, time);
    }
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
    
    // Timestamps might not be exactly equal due to serialization/deserialization
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
    
    // Modify the equipment
    original.setName("Updated Name");
    original.setStatus(EquipmentStatus::InUse);
    
    ASSERT_TRUE(data_storage_->updateEquipment(original));
    
    auto loaded = data_storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    EXPECT_EQ(loaded->getName(), "Updated Name");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::InUse);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Verify it exists
    EXPECT_TRUE(data_storage_->loadEquipment(equipment.getId()).has_value());
    
    // Delete it
    ASSERT_TRUE(data_storage_->deleteEquipment(equipment.getId()));
    
    // Verify it's gone
    EXPECT_FALSE(data_storage_->loadEquipment(equipment.getId()).has_value());
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    ASSERT_TRUE(data_storage_->initialize());
    
    std::string id = "test123";
    auto now = std::chrono::system_clock::now();
    
    // Create positions at different times
    Position pos1(37.7749, -122.4194, 10.0, 5.0, now - std::chrono::hours(2));
    Position pos2(37.7750, -122.4195, 11.0, 6.0, now - std::chrono::hours(1));
    Position pos3(37.7751, -122.4196, 12.0, 7.0, now);
    
    // Save positions
    ASSERT_TRUE(data_storage_->savePosition(id, pos1));
    ASSERT_TRUE(data_storage_->savePosition(id, pos2));
    ASSERT_TRUE(data_storage_->savePosition(id, pos3));
    
    // Get all positions
    auto start_time = now - std::chrono::hours(3);
    auto end_time = now + std::chrono::hours(1);
    auto positions = data_storage_->getPositionHistory(id, start_time, end_time);
    
    ASSERT_EQ(positions.size(), 3);
    
    // Positions should be sorted by timestamp
    EXPECT_DOUBLE_EQ(positions[0].getLatitude(), pos1.getLatitude());
    EXPECT_DOUBLE_EQ(positions[1].getLatitude(), pos2.getLatitude());
    EXPECT_DOUBLE_EQ(positions[2].getLatitude(), pos3.getLatitude());
    
    // Test time range filtering
    auto filtered_positions = data_storage_->getPositionHistory(
        id, now - std::chrono::minutes(90), now);
    
    ASSERT_EQ(filtered_positions.size(), 2);
    EXPECT_DOUBLE_EQ(filtered_positions[0].getLatitude(), pos2.getLatitude());
    EXPECT_DOUBLE_EQ(filtered_positions[1].getLatitude(), pos3.getLatitude());
}

TEST_F(DataStorageTest, GetAllEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save multiple equipment
    Equipment equip1("id1", EquipmentType::Vehicle, "Equipment 1");
    Equipment equip2("id2", EquipmentType::Tool, "Equipment 2");
    Equipment equip3("id3", EquipmentType::Vehicle, "Equipment 3");
    
    ASSERT_TRUE(data_storage_->saveEquipment(equip1));
    ASSERT_TRUE(data_storage_->saveEquipment(equip2));
    ASSERT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Get all equipment
    auto all_equipment = data_storage_->getAllEquipment();
    
    ASSERT_EQ(all_equipment.size(), 3);
    
    // Check if all equipment are retrieved
    bool found1 = false, found2 = false, found3 = false;
    
    for (const auto& equip : all_equipment) {
        if (equip.getId() == "id1") found1 = true;
        if (equip.getId() == "id2") found2 = true;
        if (equip.getId() == "id3") found3 = true;
    }
    
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
    EXPECT_TRUE(found3);
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different statuses
    Equipment equip1("id1", EquipmentType::Vehicle, "Equipment 1");
    equip1.setStatus(EquipmentStatus::Available);
    
    Equipment equip2("id2", EquipmentType::Tool, "Equipment 2");
    equip2.setStatus(EquipmentStatus::InUse);
    
    Equipment equip3("id3", EquipmentType::Vehicle, "Equipment 3");
    equip3.setStatus(EquipmentStatus::Available);
    
    ASSERT_TRUE(data_storage_->saveEquipment(equip1));
    ASSERT_TRUE(data_storage_->saveEquipment(equip2));
    ASSERT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Find equipment by status
    auto available_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Available);
    ASSERT_EQ(available_equipment.size(), 2);
    
    auto in_use_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::InUse);
    ASSERT_EQ(in_use_equipment.size(), 1);
    EXPECT_EQ(in_use_equipment[0].getId(), "id2");
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different types
    Equipment equip1("id1", EquipmentType::Vehicle, "Equipment 1");
    Equipment equip2("id2", EquipmentType::Tool, "Equipment 2");
    Equipment equip3("id3", EquipmentType::Vehicle, "Equipment 3");
    
    ASSERT_TRUE(data_storage_->saveEquipment(equip1));
    ASSERT_TRUE(data_storage_->saveEquipment(equip2));
    ASSERT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Find equipment by type
    auto vehicles = data_storage_->findEquipmentByType(EquipmentType::Vehicle);
    ASSERT_EQ(vehicles.size(), 2);
    
    auto tools = data_storage_->findEquipmentByType(EquipmentType::Tool);
    ASSERT_EQ(tools.size(), 1);
    EXPECT_EQ(tools[0].getId(), "id2");
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different positions
    Equipment equip1("id1", EquipmentType::Vehicle, "Equipment 1");
    equip1.setLastPosition(Position(37.7749, -122.4194, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment equip2("id2", EquipmentType::Tool, "Equipment 2");
    equip2.setLastPosition(Position(40.7128, -74.0060, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment equip3("id3", EquipmentType::Vehicle, "Equipment 3");
    equip3.setLastPosition(Position(37.7750, -122.4195, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment equip4("id4", EquipmentType::Other, "Equipment 4");
    // No position for equip4
    
    ASSERT_TRUE(data_storage_->saveEquipment(equip1));
    ASSERT_TRUE(data_storage_->saveEquipment(equip2));
    ASSERT_TRUE(data_storage_->saveEquipment(equip3));
    ASSERT_TRUE(data_storage_->saveEquipment(equip4));
    
    // Find equipment in San Francisco area
    auto sf_equipment = data_storage_->findEquipmentInArea(37.7, -122.5, 37.8, -122.4);
    ASSERT_EQ(sf_equipment.size(), 2);
    
    // Find equipment in New York area
    auto ny_equipment = data_storage_->findEquipmentInArea(40.7, -74.1, 40.8, -74.0);
    ASSERT_EQ(ny_equipment.size(), 1);
    EXPECT_EQ(ny_equipment[0].getId(), "id2");
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

TEST_F(DataStorageTest, SaveEquipmentWithoutInitialization) {
    // Don't call initialize() explicitly
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Verify it was saved
    auto loaded = data_storage_->loadEquipment(equipment.getId());
    EXPECT_TRUE(loaded.has_value());
}

TEST_F(DataStorageTest, SavePositionWithoutInitialization) {
    // Don't call initialize() explicitly
    std::string id = "test123";
    Position position(37.7749, -122.4194, 10.0, 5.0, std::chrono::system_clock::now());
    
    EXPECT_TRUE(data_storage_->savePosition(id, position));
    
    // Verify it was saved
    auto start_time = std::chrono::system_clock::now() - std::chrono::hours(1);
    auto end_time = std::chrono::system_clock::now() + std::chrono::hours(1);
    auto positions = data_storage_->getPositionHistory(id, start_time, end_time);
    
    ASSERT_EQ(positions.size(), 1);
    EXPECT_DOUBLE_EQ(positions[0].getLatitude(), position.getLatitude());
}

} // namespace equipment_tracker
// </test_code>