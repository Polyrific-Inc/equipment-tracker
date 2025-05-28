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
    EXPECT_TRUE(data_storage_->initialize());  // Second call should also return true
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    Equipment original = createTestEquipment();
    
    // Save the equipment
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
    auto result = data_storage_->loadEquipment("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    // Create and save initial equipment
    Equipment original = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(original));
    
    // Modify the equipment
    Equipment updated = original;
    updated.setName("Updated Drone");
    updated.setStatus(EquipmentStatus::InUse);
    
    // Update the equipment
    EXPECT_TRUE(data_storage_->updateEquipment(updated));
    
    // Load the equipment and verify changes
    auto loaded = data_storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getName(), "Updated Drone");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::InUse);
}

TEST_F(DataStorageTest, DeleteEquipment) {
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
    // Create equipment
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create positions at different times
    std::vector<Position> positions;
    auto now = std::chrono::system_clock::now();
    auto hour_ago = now - std::chrono::hours(1);
    auto two_hours_ago = now - std::chrono::hours(2);
    
    positions.push_back(Position(37.7749, -122.4194, 10.0, 5.0, two_hours_ago));
    positions.push_back(Position(37.7750, -122.4195, 15.0, 4.0, hour_ago));
    positions.push_back(Position(37.7751, -122.4196, 20.0, 3.0, now));
    
    // Save positions
    for (const auto& pos : positions) {
        EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos));
    }
    
    // Get position history for the entire time range
    auto history = data_storage_->getPositionHistory(
        equipment.getId(),
        two_hours_ago - std::chrono::minutes(1),
        now + std::chrono::minutes(1)
    );
    
    // Verify all positions are retrieved
    ASSERT_EQ(history.size(), 3);
    
    // Verify positions are in chronological order
    EXPECT_NEAR(history[0].getLatitude(), 37.7749, 0.0001);
    EXPECT_NEAR(history[1].getLatitude(), 37.7750, 0.0001);
    EXPECT_NEAR(history[2].getLatitude(), 37.7751, 0.0001);
    
    // Get position history for a limited time range
    auto limited_history = data_storage_->getPositionHistory(
        equipment.getId(),
        hour_ago - std::chrono::minutes(1),
        now + std::chrono::minutes(1)
    );
    
    // Verify only the positions in the time range are retrieved
    ASSERT_EQ(limited_history.size(), 2);
    EXPECT_NEAR(limited_history[0].getLatitude(), 37.7750, 0.0001);
    EXPECT_NEAR(limited_history[1].getLatitude(), 37.7751, 0.0001);
}

TEST_F(DataStorageTest, GetAllEquipment) {
    // Create and save multiple equipment
    Equipment drone = Equipment("drone1", EquipmentType::Drone, "Drone 1");
    Equipment vehicle = Equipment("vehicle1", EquipmentType::Vehicle, "Vehicle 1");
    Equipment tool = Equipment("tool1", EquipmentType::Tool, "Tool 1");
    
    EXPECT_TRUE(data_storage_->saveEquipment(drone));
    EXPECT_TRUE(data_storage_->saveEquipment(vehicle));
    EXPECT_TRUE(data_storage_->saveEquipment(tool));
    
    // Get all equipment
    auto all = data_storage_->getAllEquipment();
    
    // Verify all equipment are retrieved
    ASSERT_EQ(all.size(), 3);
    
    // Verify the equipment IDs are correct (order may vary)
    std::vector<std::string> ids;
    for (const auto& eq : all) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("drone1", "vehicle1", "tool1"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    // Create equipment with different statuses
    Equipment available = Equipment("eq1", EquipmentType::Drone, "Equipment 1");
    available.setStatus(EquipmentStatus::Available);
    
    Equipment in_use = Equipment("eq2", EquipmentType::Vehicle, "Equipment 2");
    in_use.setStatus(EquipmentStatus::InUse);
    
    Equipment maintenance = Equipment("eq3", EquipmentType::Tool, "Equipment 3");
    maintenance.setStatus(EquipmentStatus::Maintenance);
    
    // Save all equipment
    EXPECT_TRUE(data_storage_->saveEquipment(available));
    EXPECT_TRUE(data_storage_->saveEquipment(in_use));
    EXPECT_TRUE(data_storage_->saveEquipment(maintenance));
    
    // Find equipment by status
    auto available_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Available);
    ASSERT_EQ(available_equipment.size(), 1);
    EXPECT_EQ(available_equipment[0].getId(), "eq1");
    
    auto in_use_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::InUse);
    ASSERT_EQ(in_use_equipment.size(), 1);
    EXPECT_EQ(in_use_equipment[0].getId(), "eq2");
    
    auto maintenance_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    ASSERT_EQ(maintenance_equipment.size(), 1);
    EXPECT_EQ(maintenance_equipment[0].getId(), "eq3");
    
    // Find equipment with a status that doesn't exist
    auto nonexistent = data_storage_->findEquipmentByStatus(EquipmentStatus::Decommissioned);
    EXPECT_EQ(nonexistent.size(), 0);
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    // Create equipment with different types
    Equipment drone = Equipment("drone1", EquipmentType::Drone, "Drone 1");
    Equipment vehicle = Equipment("vehicle1", EquipmentType::Vehicle, "Vehicle 1");
    Equipment tool = Equipment("tool1", EquipmentType::Tool, "Tool 1");
    
    // Save all equipment
    EXPECT_TRUE(data_storage_->saveEquipment(drone));
    EXPECT_TRUE(data_storage_->saveEquipment(vehicle));
    EXPECT_TRUE(data_storage_->saveEquipment(tool));
    
    // Find equipment by type
    auto drones = data_storage_->findEquipmentByType(EquipmentType::Drone);
    ASSERT_EQ(drones.size(), 1);
    EXPECT_EQ(drones[0].getId(), "drone1");
    
    auto vehicles = data_storage_->findEquipmentByType(EquipmentType::Vehicle);
    ASSERT_EQ(vehicles.size(), 1);
    EXPECT_EQ(vehicles[0].getId(), "vehicle1");
    
    auto tools = data_storage_->findEquipmentByType(EquipmentType::Tool);
    ASSERT_EQ(tools.size(), 1);
    EXPECT_EQ(tools[0].getId(), "tool1");
    
    // Find equipment with a type that doesn't exist
    auto nonexistent = data_storage_->findEquipmentByType(EquipmentType::Other);
    EXPECT_EQ(nonexistent.size(), 0);
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    // Create equipment with different positions
    Equipment eq1 = Equipment("eq1", EquipmentType::Drone, "Equipment 1");
    Equipment eq2 = Equipment("eq2", EquipmentType::Drone, "Equipment 2");
    Equipment eq3 = Equipment("eq3", EquipmentType::Drone, "Equipment 3");
    Equipment eq4 = Equipment("eq4", EquipmentType::Drone, "Equipment 4");
    
    // Set positions
    auto now = std::chrono::system_clock::now();
    eq1.setLastPosition(Position(37.7749, -122.4194, 10.0, 5.0, now)); // San Francisco
    eq2.setLastPosition(Position(34.0522, -118.2437, 10.0, 5.0, now)); // Los Angeles
    eq3.setLastPosition(Position(40.7128, -74.0060, 10.0, 5.0, now));  // New York
    // eq4 has no position
    
    // Save all equipment
    EXPECT_TRUE(data_storage_->saveEquipment(eq1));
    EXPECT_TRUE(data_storage_->saveEquipment(eq2));
    EXPECT_TRUE(data_storage_->saveEquipment(eq3));
    EXPECT_TRUE(data_storage_->saveEquipment(eq4));
    
    // Find equipment in West Coast area (covers SF and LA)
    auto west_coast = data_storage_->findEquipmentInArea(33.0, -125.0, 39.0, -118.0);
    ASSERT_EQ(west_coast.size(), 2);
    
    // Find equipment in SF area only
    auto sf_area = data_storage_->findEquipmentInArea(37.7, -122.5, 37.8, -122.4);
    ASSERT_EQ(sf_area.size(), 1);
    EXPECT_EQ(sf_area[0].getId(), "eq1");
    
    // Find equipment in an area with no equipment
    auto empty_area = data_storage_->findEquipmentInArea(45.0, -120.0, 46.0, -119.0);
    EXPECT_EQ(empty_area.size(), 0);
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