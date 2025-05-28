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
#define GMTIME(t, tm) gmtime_s(tm, t)
#else
#define GMTIME(t, tm) gmtime_r(t, tm)
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
        Equipment equipment(id, EquipmentType::Drone, "Test Drone");
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
    
    // Check that the database directories were created
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "positions"));
}

TEST_F(DataStorageTest, InitializeIdempotent) {
    ASSERT_TRUE(data_storage_->initialize());
    ASSERT_TRUE(data_storage_->initialize()); // Should still return true on second call
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment original = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(original));
    
    // Load the equipment
    auto loaded = data_storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    // Verify the loaded equipment matches the original
    EXPECT_EQ(loaded->getId(), original.getId());
    EXPECT_EQ(loaded->getName(), original.getName());
    EXPECT_EQ(loaded->getType(), original.getType());
    EXPECT_EQ(loaded->getStatus(), original.getStatus());
    
    // Check position data
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
    updated.setName("Updated Drone");
    updated.setStatus(EquipmentStatus::InUse);
    
    ASSERT_TRUE(data_storage_->updateEquipment(updated));
    
    // Load the equipment
    auto loaded = data_storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    // Verify the loaded equipment has the updated values
    EXPECT_EQ(loaded->getName(), "Updated Drone");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::InUse);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Verify it exists
    EXPECT_TRUE(data_storage_->loadEquipment(equipment.getId()).has_value());
    
    // Delete the equipment
    ASSERT_TRUE(data_storage_->deleteEquipment(equipment.getId()));
    
    // Verify it no longer exists
    EXPECT_FALSE(data_storage_->loadEquipment(equipment.getId()).has_value());
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create positions at different times
    auto now = std::chrono::system_clock::now();
    auto one_hour_ago = now - std::chrono::hours(1);
    auto two_hours_ago = now - std::chrono::hours(2);
    
    Position pos1(37.7749, -122.4194, 10.0, 5.0, two_hours_ago);
    Position pos2(37.7750, -122.4195, 11.0, 4.0, one_hour_ago);
    Position pos3(37.7751, -122.4196, 12.0, 3.0, now);
    
    // Save positions
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos1));
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos2));
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos3));
    
    // Get position history for the entire time range
    auto history = data_storage_->getPositionHistory(
        equipment.getId(), 
        two_hours_ago - std::chrono::minutes(1), 
        now + std::chrono::minutes(1)
    );
    
    // Verify we got all three positions
    ASSERT_EQ(history.size(), 3);
    
    // Verify positions are in chronological order
    EXPECT_NEAR(history[0].getLatitude(), pos1.getLatitude(), 0.0001);
    EXPECT_NEAR(history[1].getLatitude(), pos2.getLatitude(), 0.0001);
    EXPECT_NEAR(history[2].getLatitude(), pos3.getLatitude(), 0.0001);
    
    // Get position history for a partial time range
    auto partial_history = data_storage_->getPositionHistory(
        equipment.getId(),
        one_hour_ago - std::chrono::minutes(1),
        now + std::chrono::minutes(1)
    );
    
    // Verify we got only the last two positions
    ASSERT_EQ(partial_history.size(), 2);
    EXPECT_NEAR(partial_history[0].getLatitude(), pos2.getLatitude(), 0.0001);
    EXPECT_NEAR(partial_history[1].getLatitude(), pos3.getLatitude(), 0.0001);
}

TEST_F(DataStorageTest, GetAllEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save multiple equipment
    Equipment eq1 = createTestEquipment("eq1");
    Equipment eq2 = createTestEquipment("eq2");
    Equipment eq3 = createTestEquipment("eq3");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Get all equipment
    auto all_equipment = data_storage_->getAllEquipment();
    
    // Verify we got all three equipment
    ASSERT_EQ(all_equipment.size(), 3);
    
    // Verify the equipment IDs
    std::vector<std::string> ids;
    for (const auto& eq : all_equipment) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("eq1", "eq2", "eq3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different statuses
    Equipment eq1("eq1", EquipmentType::Drone, "Drone 1");
    eq1.setStatus(EquipmentStatus::Available);
    
    Equipment eq2("eq2", EquipmentType::Drone, "Drone 2");
    eq2.setStatus(EquipmentStatus::InUse);
    
    Equipment eq3("eq3", EquipmentType::Drone, "Drone 3");
    eq3.setStatus(EquipmentStatus::Available);
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find equipment by status
    auto available = data_storage_->findEquipmentByStatus(EquipmentStatus::Available);
    auto in_use = data_storage_->findEquipmentByStatus(EquipmentStatus::InUse);
    auto maintenance = data_storage_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    
    // Verify results
    ASSERT_EQ(available.size(), 2);
    ASSERT_EQ(in_use.size(), 1);
    ASSERT_EQ(maintenance.size(), 0);
    
    // Check IDs of available equipment
    std::vector<std::string> available_ids;
    for (const auto& eq : available) {
        available_ids.push_back(eq.getId());
    }
    EXPECT_THAT(available_ids, ::testing::UnorderedElementsAre("eq1", "eq3"));
    
    // Check ID of in-use equipment
    EXPECT_EQ(in_use[0].getId(), "eq2");
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different types
    Equipment eq1("eq1", EquipmentType::Drone, "Drone 1");
    Equipment eq2("eq2", EquipmentType::Vehicle, "Vehicle 1");
    Equipment eq3("eq3", EquipmentType::Drone, "Drone 2");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find equipment by type
    auto drones = data_storage_->findEquipmentByType(EquipmentType::Drone);
    auto vehicles = data_storage_->findEquipmentByType(EquipmentType::Vehicle);
    auto other = data_storage_->findEquipmentByType(EquipmentType::Other);
    
    // Verify results
    ASSERT_EQ(drones.size(), 2);
    ASSERT_EQ(vehicles.size(), 1);
    ASSERT_EQ(other.size(), 0);
    
    // Check IDs of drones
    std::vector<std::string> drone_ids;
    for (const auto& eq : drones) {
        drone_ids.push_back(eq.getId());
    }
    EXPECT_THAT(drone_ids, ::testing::UnorderedElementsAre("eq1", "eq3"));
    
    // Check ID of vehicle
    EXPECT_EQ(vehicles[0].getId(), "eq2");
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different positions
    Equipment eq1("eq1", EquipmentType::Drone, "Drone 1");
    eq1.setLastPosition(Position(37.7749, -122.4194, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment eq2("eq2", EquipmentType::Drone, "Drone 2");
    eq2.setLastPosition(Position(40.7128, -74.0060, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment eq3("eq3", EquipmentType::Drone, "Drone 3");
    eq3.setLastPosition(Position(37.3382, -121.8863, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment eq4("eq4", EquipmentType::Drone, "Drone 4");
    // No position set for eq4
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    ASSERT_TRUE(data_storage_->saveEquipment(eq4));
    
    // Find equipment in San Francisco Bay Area
    auto bay_area = data_storage_->findEquipmentInArea(37.0, -123.0, 38.0, -121.0);
    
    // Verify results
    ASSERT_EQ(bay_area.size(), 2);
    
    // Check IDs of equipment in the Bay Area
    std::vector<std::string> bay_area_ids;
    for (const auto& eq : bay_area) {
        bay_area_ids.push_back(eq.getId());
    }
    EXPECT_THAT(bay_area_ids, ::testing::UnorderedElementsAre("eq1", "eq3"));
    
    // Find equipment in a different area (New York)
    auto ny_area = data_storage_->findEquipmentInArea(40.0, -75.0, 41.0, -73.0);
    
    // Verify results
    ASSERT_EQ(ny_area.size(), 1);
    EXPECT_EQ(ny_area[0].getId(), "eq2");
    
    // Find equipment in an area with no equipment
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

} // namespace equipment_tracker
// </test_code>