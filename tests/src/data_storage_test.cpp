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
        Equipment equipment(id, EquipmentType::Drone, "Test Drone");
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
    EXPECT_TRUE(data_storage_->initialize());
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "positions"));
}

TEST_F(DataStorageTest, InitializeIsIdempotent) {
    EXPECT_TRUE(data_storage_->initialize());
    EXPECT_TRUE(data_storage_->initialize()); // Second call should also return true
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    Equipment original = createTestEquipment();
    
    // Save the equipment
    EXPECT_TRUE(data_storage_->saveEquipment(original));
    
    // Load the equipment
    auto loaded = data_storage_->loadEquipment(original.getId());
    
    // Verify the loaded equipment matches the original
    ASSERT_TRUE(loaded.has_value());
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
    EXPECT_EQ(original_time, loaded_time);
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    auto result = data_storage_->loadEquipment("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    Equipment original = createTestEquipment();
    
    // Save the equipment
    EXPECT_TRUE(data_storage_->saveEquipment(original));
    
    // Modify and update
    Equipment updated = original;
    updated.setName("Updated Drone");
    updated.setStatus(EquipmentStatus::InUse);
    
    EXPECT_TRUE(data_storage_->updateEquipment(updated));
    
    // Load and verify
    auto loaded = data_storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getName(), "Updated Drone");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::InUse);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    Equipment equipment = createTestEquipment();
    
    // Save the equipment
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Verify it exists
    EXPECT_TRUE(data_storage_->loadEquipment(equipment.getId()).has_value());
    
    // Delete it
    EXPECT_TRUE(data_storage_->deleteEquipment(equipment.getId()));
    
    // Verify it's gone
    EXPECT_FALSE(data_storage_->loadEquipment(equipment.getId()).has_value());
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    std::string id = "test123";
    
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
    EXPECT_TRUE(data_storage_->savePosition(id, pos1));
    EXPECT_TRUE(data_storage_->savePosition(id, pos2));
    EXPECT_TRUE(data_storage_->savePosition(id, pos3));
    
    // Get position history for the full range
    auto history = data_storage_->getPositionHistory(
        id, 
        std::chrono::system_clock::from_time_t(base_time - 1),
        std::chrono::system_clock::from_time_t(base_time + 7201)
    );
    
    // Verify all positions are returned
    ASSERT_EQ(history.size(), 3);
    
    // Positions should be sorted by timestamp
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), 37.7749);
    EXPECT_DOUBLE_EQ(history[1].getLatitude(), 37.7750);
    EXPECT_DOUBLE_EQ(history[2].getLatitude(), 37.7751);
    
    // Get position history for a partial range
    auto partial_history = data_storage_->getPositionHistory(
        id, 
        std::chrono::system_clock::from_time_t(base_time + 1),
        std::chrono::system_clock::from_time_t(base_time + 3601)
    );
    
    // Verify only the middle position is returned
    ASSERT_EQ(partial_history.size(), 1);
    EXPECT_DOUBLE_EQ(partial_history[0].getLatitude(), 37.7750);
}

TEST_F(DataStorageTest, GetAllEquipment) {
    // Create and save multiple equipment
    Equipment eq1 = createTestEquipment("eq1");
    Equipment eq2 = createTestEquipment("eq2");
    Equipment eq3 = createTestEquipment("eq3");
    
    EXPECT_TRUE(data_storage_->saveEquipment(eq1));
    EXPECT_TRUE(data_storage_->saveEquipment(eq2));
    EXPECT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Get all equipment
    auto all = data_storage_->getAllEquipment();
    
    // Verify all equipment are returned
    ASSERT_EQ(all.size(), 3);
    
    // Verify IDs (order may vary)
    std::vector<std::string> ids;
    for (const auto& eq : all) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("eq1", "eq2", "eq3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    // Create equipment with different statuses
    Equipment eq1("eq1", EquipmentType::Drone, "Drone 1");
    eq1.setStatus(EquipmentStatus::Available);
    
    Equipment eq2("eq2", EquipmentType::Drone, "Drone 2");
    eq2.setStatus(EquipmentStatus::InUse);
    
    Equipment eq3("eq3", EquipmentType::Drone, "Drone 3");
    eq3.setStatus(EquipmentStatus::Available);
    
    // Save all equipment
    EXPECT_TRUE(data_storage_->saveEquipment(eq1));
    EXPECT_TRUE(data_storage_->saveEquipment(eq2));
    EXPECT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find equipment by status
    auto available = data_storage_->findEquipmentByStatus(EquipmentStatus::Available);
    auto in_use = data_storage_->findEquipmentByStatus(EquipmentStatus::InUse);
    auto maintenance = data_storage_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    
    // Verify results
    ASSERT_EQ(available.size(), 2);
    ASSERT_EQ(in_use.size(), 1);
    ASSERT_EQ(maintenance.size(), 0);
    
    // Verify IDs of available equipment
    std::vector<std::string> available_ids;
    for (const auto& eq : available) {
        available_ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(available_ids, ::testing::UnorderedElementsAre("eq1", "eq3"));
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    // Create equipment with different types
    Equipment eq1("eq1", EquipmentType::Drone, "Drone");
    Equipment eq2("eq2", EquipmentType::Vehicle, "Vehicle");
    Equipment eq3("eq3", EquipmentType::Drone, "Another Drone");
    
    // Save all equipment
    EXPECT_TRUE(data_storage_->saveEquipment(eq1));
    EXPECT_TRUE(data_storage_->saveEquipment(eq2));
    EXPECT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find equipment by type
    auto drones = data_storage_->findEquipmentByType(EquipmentType::Drone);
    auto vehicles = data_storage_->findEquipmentByType(EquipmentType::Vehicle);
    auto other = data_storage_->findEquipmentByType(EquipmentType::Other);
    
    // Verify results
    ASSERT_EQ(drones.size(), 2);
    ASSERT_EQ(vehicles.size(), 1);
    ASSERT_EQ(other.size(), 0);
    
    // Verify IDs of drones
    std::vector<std::string> drone_ids;
    for (const auto& eq : drones) {
        drone_ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(drone_ids, ::testing::UnorderedElementsAre("eq1", "eq3"));
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    // Create equipment with different positions
    Equipment eq1("eq1", EquipmentType::Drone, "Drone 1");
    eq1.setLastPosition(Position(37.7749, -122.4194, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment eq2("eq2", EquipmentType::Drone, "Drone 2");
    eq2.setLastPosition(Position(40.7128, -74.0060, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment eq3("eq3", EquipmentType::Drone, "Drone 3");
    eq3.setLastPosition(Position(37.3382, -121.8863, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment eq4("eq4", EquipmentType::Drone, "Drone 4");
    // No position for eq4
    
    // Save all equipment
    EXPECT_TRUE(data_storage_->saveEquipment(eq1));
    EXPECT_TRUE(data_storage_->saveEquipment(eq2));
    EXPECT_TRUE(data_storage_->saveEquipment(eq3));
    EXPECT_TRUE(data_storage_->saveEquipment(eq4));
    
    // Find equipment in San Francisco area
    auto sf_area = data_storage_->findEquipmentInArea(37.7, -122.5, 37.8, -122.4);
    
    // Verify results
    ASSERT_EQ(sf_area.size(), 1);
    EXPECT_EQ(sf_area[0].getId(), "eq1");
    
    // Find equipment in a larger California area
    auto ca_area = data_storage_->findEquipmentInArea(36.0, -123.0, 38.0, -121.0);
    
    // Verify results
    ASSERT_EQ(ca_area.size(), 2);
    
    std::vector<std::string> ca_ids;
    for (const auto& eq : ca_area) {
        ca_ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ca_ids, ::testing::UnorderedElementsAre("eq1", "eq3"));
}

TEST_F(DataStorageTest, ExecuteQuery) {
    // This is just testing the placeholder implementation
    testing::internal::CaptureStdout();
    bool result = data_storage_->executeQuery("SELECT * FROM equipment");
    std::string output = testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(result);
    EXPECT_THAT(output, ::testing::HasSubstr("Query: SELECT * FROM equipment"));
}

} // namespace equipment_tracker
// </test_code>