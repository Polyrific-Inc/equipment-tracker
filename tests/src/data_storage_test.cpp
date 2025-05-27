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
        
        // Create a fresh test directory
        std::filesystem::create_directory(test_db_path_);
        
        // Initialize the data storage with the test path
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
        Equipment equipment(id, EquipmentType::Vehicle, "Test Vehicle");
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

TEST_F(DataStorageTest, InitializeCreatesDirectories) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Check that the database directories were created
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "positions"));
}

TEST_F(DataStorageTest, InitializeIdempotent) {
    ASSERT_TRUE(data_storage_->initialize());
    ASSERT_TRUE(data_storage_->initialize()); // Should return true on second call
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment original = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(original));
    
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
    
    // Timestamps might have slight differences due to serialization/deserialization
    auto original_time = std::chrono::system_clock::to_time_t(original.getLastPosition()->getTimestamp());
    auto loaded_time = std::chrono::system_clock::to_time_t(loaded->getLastPosition()->getTimestamp());
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
    Equipment updated = original;
    updated.setName("Updated Name");
    updated.setStatus(EquipmentStatus::InUse);
    
    ASSERT_TRUE(data_storage_->updateEquipment(updated));
    
    // Load and verify the update
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
    
    std::string equipmentId = "test123";
    
    // Create positions at different times
    struct tm timeinfo = {};
    timeinfo.tm_year = 120;  // 2020
    timeinfo.tm_mon = 0;     // January
    timeinfo.tm_mday = 1;    // 1st
    timeinfo.tm_hour = 12;   // 12:00
    
    time_t base_time = MKGMTIME(&timeinfo);
    
    Position pos1 = createPosition(37.7749, -122.4194, 10.0, 5.0, base_time);
    Position pos2 = createPosition(37.7750, -122.4195, 11.0, 4.0, base_time + 3600);
    Position pos3 = createPosition(37.7751, -122.4196, 12.0, 3.0, base_time + 7200);
    
    // Save positions
    ASSERT_TRUE(data_storage_->savePosition(equipmentId, pos1));
    ASSERT_TRUE(data_storage_->savePosition(equipmentId, pos2));
    ASSERT_TRUE(data_storage_->savePosition(equipmentId, pos3));
    
    // Get all positions
    auto history = data_storage_->getPositionHistory(
        equipmentId,
        std::chrono::system_clock::from_time_t(base_time - 1),
        std::chrono::system_clock::from_time_t(base_time + 7201)
    );
    
    ASSERT_EQ(history.size(), 3);
    
    // Verify positions are in chronological order
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), pos1.getLatitude());
    EXPECT_DOUBLE_EQ(history[1].getLatitude(), pos2.getLatitude());
    EXPECT_DOUBLE_EQ(history[2].getLatitude(), pos3.getLatitude());
    
    // Get positions in a specific time range
    auto filtered_history = data_storage_->getPositionHistory(
        equipmentId,
        std::chrono::system_clock::from_time_t(base_time + 1),
        std::chrono::system_clock::from_time_t(base_time + 3601)
    );
    
    ASSERT_EQ(filtered_history.size(), 1);
    EXPECT_DOUBLE_EQ(filtered_history[0].getLatitude(), pos2.getLatitude());
}

TEST_F(DataStorageTest, GetAllEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save multiple equipment
    Equipment eq1("id1", EquipmentType::Vehicle, "Vehicle 1");
    Equipment eq2("id2", EquipmentType::Tool, "Tool 1");
    Equipment eq3("id3", EquipmentType::Vehicle, "Vehicle 2");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Get all equipment
    auto all = data_storage_->getAllEquipment();
    
    ASSERT_EQ(all.size(), 3);
    
    // Verify all equipment are retrieved
    std::vector<std::string> ids;
    for (const auto& eq : all) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("id1", "id2", "id3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different statuses
    Equipment eq1("id1", EquipmentType::Vehicle, "Vehicle 1");
    eq1.setStatus(EquipmentStatus::Available);
    
    Equipment eq2("id2", EquipmentType::Tool, "Tool 1");
    eq2.setStatus(EquipmentStatus::InUse);
    
    Equipment eq3("id3", EquipmentType::Vehicle, "Vehicle 2");
    eq3.setStatus(EquipmentStatus::Available);
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find equipment by status
    auto available = data_storage_->findEquipmentByStatus(EquipmentStatus::Available);
    ASSERT_EQ(available.size(), 2);
    
    std::vector<std::string> ids;
    for (const auto& eq : available) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("id1", "id3"));
    
    // Find equipment with a different status
    auto in_use = data_storage_->findEquipmentByStatus(EquipmentStatus::InUse);
    ASSERT_EQ(in_use.size(), 1);
    EXPECT_EQ(in_use[0].getId(), "id2");
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different types
    Equipment eq1("id1", EquipmentType::Vehicle, "Vehicle 1");
    Equipment eq2("id2", EquipmentType::Tool, "Tool 1");
    Equipment eq3("id3", EquipmentType::Vehicle, "Vehicle 2");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find equipment by type
    auto vehicles = data_storage_->findEquipmentByType(EquipmentType::Vehicle);
    ASSERT_EQ(vehicles.size(), 2);
    
    std::vector<std::string> ids;
    for (const auto& eq : vehicles) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("id1", "id3"));
    
    // Find equipment with a different type
    auto tools = data_storage_->findEquipmentByType(EquipmentType::Tool);
    ASSERT_EQ(tools.size(), 1);
    EXPECT_EQ(tools[0].getId(), "id2");
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different positions
    Equipment eq1("id1", EquipmentType::Vehicle, "Vehicle 1");
    eq1.setLastPosition(Position(37.7749, -122.4194, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment eq2("id2", EquipmentType::Tool, "Tool 1");
    eq2.setLastPosition(Position(40.7128, -74.0060, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment eq3("id3", EquipmentType::Vehicle, "Vehicle 2");
    eq3.setLastPosition(Position(37.7750, -122.4195, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment eq4("id4", EquipmentType::Other, "No Position");
    // No position set for eq4
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    ASSERT_TRUE(data_storage_->saveEquipment(eq4));
    
    // Find equipment in San Francisco area
    auto sf_area = data_storage_->findEquipmentInArea(37.7, -122.5, 37.8, -122.4);
    ASSERT_EQ(sf_area.size(), 2);
    
    std::vector<std::string> ids;
    for (const auto& eq : sf_area) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("id1", "id3"));
    
    // Find equipment in New York area
    auto ny_area = data_storage_->findEquipmentInArea(40.7, -74.1, 40.8, -74.0);
    ASSERT_EQ(ny_area.size(), 1);
    EXPECT_EQ(ny_area[0].getId(), "id2");
    
    // Find equipment in an empty area
    auto empty_area = data_storage_->findEquipmentInArea(0.0, 0.0, 1.0, 1.0);
    EXPECT_EQ(empty_area.size(), 0);
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
    // Don't call initialize() first
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Verify it was saved
    auto loaded = data_storage_->loadEquipment(equipment.getId());
    EXPECT_TRUE(loaded.has_value());
}

TEST_F(DataStorageTest, LoadEquipmentWithoutInitialization) {
    // Save equipment first
    ASSERT_TRUE(data_storage_->initialize());
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create a new data storage instance without initializing
    auto new_storage = std::make_unique<DataStorage>(test_db_path_.string());
    
    // Should still be able to load
    auto loaded = new_storage->loadEquipment(equipment.getId());
    EXPECT_TRUE(loaded.has_value());
}

TEST_F(DataStorageTest, SavePositionWithoutInitialization) {
    // Don't call initialize() first
    std::string equipmentId = "test123";
    Position position(37.7749, -122.4194, 10.0, 5.0, std::chrono::system_clock::now());
    
    EXPECT_TRUE(data_storage_->savePosition(equipmentId, position));
    
    // Verify it was saved by checking the directory structure
    std::filesystem::path position_dir = test_db_path_ / "positions" / equipmentId;
    EXPECT_TRUE(std::filesystem::exists(position_dir));
    EXPECT_GT(std::distance(std::filesystem::directory_iterator(position_dir), 
                           std::filesystem::directory_iterator()), 0);
}

}  // namespace equipment_tracker
// </test_code>