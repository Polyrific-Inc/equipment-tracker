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
        
        // Initialize the data storage with the test directory
        storage_ = std::make_unique<DataStorage>(test_db_path_.string());
    }

    void TearDown() override {
        // Clean up the test directory
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove_all(test_db_path_);
        }
    }

    std::filesystem::path test_db_path_;
    std::unique_ptr<DataStorage> storage_;
    
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
    EXPECT_TRUE(storage_->initialize());
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "positions"));
}

TEST_F(DataStorageTest, InitializeIsIdempotent) {
    EXPECT_TRUE(storage_->initialize());
    EXPECT_TRUE(storage_->initialize());  // Second call should also return true
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    ASSERT_TRUE(storage_->initialize());
    
    Equipment original = createTestEquipment();
    EXPECT_TRUE(storage_->saveEquipment(original));
    
    auto loaded = storage_->loadEquipment(original.getId());
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
    
    // Compare timestamps (allowing for small differences due to serialization)
    auto original_time = std::chrono::system_clock::to_time_t(original_pos->getTimestamp());
    auto loaded_time = std::chrono::system_clock::to_time_t(loaded_pos->getTimestamp());
    EXPECT_EQ(loaded_time, original_time);
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    ASSERT_TRUE(storage_->initialize());
    
    auto loaded = storage_->loadEquipment("nonexistent");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    ASSERT_TRUE(storage_->initialize());
    
    Equipment original = createTestEquipment();
    EXPECT_TRUE(storage_->saveEquipment(original));
    
    // Modify the equipment
    Equipment updated = original;
    updated.setName("Updated Drone");
    updated.setStatus(EquipmentStatus::InUse);
    
    EXPECT_TRUE(storage_->updateEquipment(updated));
    
    auto loaded = storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    EXPECT_EQ(loaded->getName(), "Updated Drone");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::InUse);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    ASSERT_TRUE(storage_->initialize());
    
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(storage_->saveEquipment(equipment));
    
    // Save a position to create position history
    auto now = std::chrono::system_clock::now();
    Position position(37.7749, -122.4194, 10.0, 5.0, now);
    EXPECT_TRUE(storage_->savePosition(equipment.getId(), position));
    
    // Verify equipment exists
    EXPECT_TRUE(storage_->loadEquipment(equipment.getId()).has_value());
    
    // Delete the equipment
    EXPECT_TRUE(storage_->deleteEquipment(equipment.getId()));
    
    // Verify equipment no longer exists
    EXPECT_FALSE(storage_->loadEquipment(equipment.getId()).has_value());
    
    // Verify position directory is also deleted
    std::string history_dir = (test_db_path_ / "positions" / equipment.getId()).string();
    EXPECT_FALSE(std::filesystem::exists(history_dir));
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    ASSERT_TRUE(storage_->initialize());
    
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(storage_->saveEquipment(equipment));
    
    // Create positions at different times
    auto now = std::chrono::system_clock::now();
    auto time1 = now - std::chrono::hours(2);
    auto time2 = now - std::chrono::hours(1);
    auto time3 = now;
    
    Position pos1(37.7749, -122.4194, 10.0, 5.0, time1);
    Position pos2(37.7750, -122.4195, 11.0, 4.0, time2);
    Position pos3(37.7751, -122.4196, 12.0, 3.0, time3);
    
    // Save positions
    EXPECT_TRUE(storage_->savePosition(equipment.getId(), pos1));
    EXPECT_TRUE(storage_->savePosition(equipment.getId(), pos2));
    EXPECT_TRUE(storage_->savePosition(equipment.getId(), pos3));
    
    // Get all position history
    auto history = storage_->getPositionHistory(equipment.getId());
    EXPECT_EQ(history.size(), 3);
    
    // Positions should be sorted by timestamp
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), pos1.getLatitude());
    EXPECT_DOUBLE_EQ(history[1].getLatitude(), pos2.getLatitude());
    EXPECT_DOUBLE_EQ(history[2].getLatitude(), pos3.getLatitude());
    
    // Get position history with time range
    auto filtered_history = storage_->getPositionHistory(
        equipment.getId(), time1, time2);
    EXPECT_EQ(filtered_history.size(), 2);
    EXPECT_DOUBLE_EQ(filtered_history[0].getLatitude(), pos1.getLatitude());
    EXPECT_DOUBLE_EQ(filtered_history[1].getLatitude(), pos2.getLatitude());
}

TEST_F(DataStorageTest, GetAllEquipment) {
    ASSERT_TRUE(storage_->initialize());
    
    // Create and save multiple equipment
    Equipment equip1("drone1", EquipmentType::Drone, "Drone 1");
    Equipment equip2("robot1", EquipmentType::Robot, "Robot 1");
    Equipment equip3("vehicle1", EquipmentType::Vehicle, "Vehicle 1");
    
    EXPECT_TRUE(storage_->saveEquipment(equip1));
    EXPECT_TRUE(storage_->saveEquipment(equip2));
    EXPECT_TRUE(storage_->saveEquipment(equip3));
    
    // Get all equipment
    auto all_equipment = storage_->getAllEquipment();
    EXPECT_EQ(all_equipment.size(), 3);
    
    // Check if all equipment are retrieved
    bool found_drone = false, found_robot = false, found_vehicle = false;
    for (const auto& equip : all_equipment) {
        if (equip.getId() == "drone1") found_drone = true;
        if (equip.getId() == "robot1") found_robot = true;
        if (equip.getId() == "vehicle1") found_vehicle = true;
    }
    
    EXPECT_TRUE(found_drone);
    EXPECT_TRUE(found_robot);
    EXPECT_TRUE(found_vehicle);
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    ASSERT_TRUE(storage_->initialize());
    
    // Create equipment with different statuses
    Equipment equip1("drone1", EquipmentType::Drone, "Drone 1");
    equip1.setStatus(EquipmentStatus::Available);
    
    Equipment equip2("robot1", EquipmentType::Robot, "Robot 1");
    equip2.setStatus(EquipmentStatus::InUse);
    
    Equipment equip3("vehicle1", EquipmentType::Vehicle, "Vehicle 1");
    equip3.setStatus(EquipmentStatus::Available);
    
    EXPECT_TRUE(storage_->saveEquipment(equip1));
    EXPECT_TRUE(storage_->saveEquipment(equip2));
    EXPECT_TRUE(storage_->saveEquipment(equip3));
    
    // Find equipment by status
    auto available = storage_->findEquipmentByStatus(EquipmentStatus::Available);
    EXPECT_EQ(available.size(), 2);
    
    auto in_use = storage_->findEquipmentByStatus(EquipmentStatus::InUse);
    EXPECT_EQ(in_use.size(), 1);
    EXPECT_EQ(in_use[0].getId(), "robot1");
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    ASSERT_TRUE(storage_->initialize());
    
    // Create equipment with different types
    Equipment equip1("drone1", EquipmentType::Drone, "Drone 1");
    Equipment equip2("drone2", EquipmentType::Drone, "Drone 2");
    Equipment equip3("robot1", EquipmentType::Robot, "Robot 1");
    
    EXPECT_TRUE(storage_->saveEquipment(equip1));
    EXPECT_TRUE(storage_->saveEquipment(equip2));
    EXPECT_TRUE(storage_->saveEquipment(equip3));
    
    // Find equipment by type
    auto drones = storage_->findEquipmentByType(EquipmentType::Drone);
    EXPECT_EQ(drones.size(), 2);
    
    auto robots = storage_->findEquipmentByType(EquipmentType::Robot);
    EXPECT_EQ(robots.size(), 1);
    EXPECT_EQ(robots[0].getId(), "robot1");
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    ASSERT_TRUE(storage_->initialize());
    
    // Create equipment with different positions
    Equipment equip1("drone1", EquipmentType::Drone, "Drone 1");
    auto now = std::chrono::system_clock::now();
    Position pos1(37.7749, -122.4194, 10.0, 5.0, now);
    equip1.setLastPosition(pos1);
    
    Equipment equip2("drone2", EquipmentType::Drone, "Drone 2");
    Position pos2(37.3382, -121.8863, 10.0, 5.0, now);
    equip2.setLastPosition(pos2);
    
    Equipment equip3("robot1", EquipmentType::Robot, "Robot 1");
    Position pos3(37.7749, -122.4194, 10.0, 5.0, now);
    equip3.setLastPosition(pos3);
    
    EXPECT_TRUE(storage_->saveEquipment(equip1));
    EXPECT_TRUE(storage_->saveEquipment(equip2));
    EXPECT_TRUE(storage_->saveEquipment(equip3));
    
    // Find equipment in San Francisco area
    auto sf_area = storage_->findEquipmentInArea(37.7, -122.5, 37.8, -122.4);
    EXPECT_EQ(sf_area.size(), 2);
    
    // Find equipment in San Jose area
    auto sj_area = storage_->findEquipmentInArea(37.3, -121.9, 37.4, -121.8);
    EXPECT_EQ(sj_area.size(), 1);
    EXPECT_EQ(sj_area[0].getId(), "drone2");
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

TEST_F(DataStorageTest, SaveEquipmentWithoutInitialization) {
    // Don't call initialize() explicitly
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(storage_->saveEquipment(equipment));
    
    // Verify the equipment was saved
    auto loaded = storage_->loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getId(), equipment.getId());
}

TEST_F(DataStorageTest, LoadEquipmentWithoutInitialization) {
    // Save equipment first
    ASSERT_TRUE(storage_->initialize());
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(storage_->saveEquipment(equipment));
    
    // Create a new storage instance without explicit initialization
    auto new_storage = std::make_unique<DataStorage>(test_db_path_.string());
    
    // Try to load equipment
    auto loaded = new_storage->loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getId(), equipment.getId());
}

TEST_F(DataStorageTest, SavePositionWithoutInitialization) {
    // Don't call initialize() explicitly
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(storage_->saveEquipment(equipment));
    
    auto now = std::chrono::system_clock::now();
    Position position(37.7749, -122.4194, 10.0, 5.0, now);
    
    EXPECT_TRUE(storage_->savePosition(equipment.getId(), position));
    
    // Verify the position was saved
    auto history = storage_->getPositionHistory(equipment.getId());
    ASSERT_EQ(history.size(), 1);
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), position.getLatitude());
}

TEST_F(DataStorageTest, GetPositionHistoryForNonExistentEquipment) {
    ASSERT_TRUE(storage_->initialize());
    
    auto history = storage_->getPositionHistory("nonexistent");
    EXPECT_TRUE(history.empty());
}

TEST_F(DataStorageTest, GetAllEquipmentFromEmptyDatabase) {
    ASSERT_TRUE(storage_->initialize());
    
    auto all_equipment = storage_->getAllEquipment();
    EXPECT_TRUE(all_equipment.empty());
}

TEST_F(DataStorageTest, FindEquipmentByStatusWithNoMatches) {
    ASSERT_TRUE(storage_->initialize());
    
    Equipment equip("drone1", EquipmentType::Drone, "Drone 1");
    equip.setStatus(EquipmentStatus::Available);
    EXPECT_TRUE(storage_->saveEquipment(equip));
    
    auto maintenance = storage_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    EXPECT_TRUE(maintenance.empty());
}

TEST_F(DataStorageTest, FindEquipmentByTypeWithNoMatches) {
    ASSERT_TRUE(storage_->initialize());
    
    Equipment equip("drone1", EquipmentType::Drone, "Drone 1");
    EXPECT_TRUE(storage_->saveEquipment(equip));
    
    auto vehicles = storage_->findEquipmentByType(EquipmentType::Vehicle);
    EXPECT_TRUE(vehicles.empty());
}

TEST_F(DataStorageTest, FindEquipmentInAreaWithNoMatches) {
    ASSERT_TRUE(storage_->initialize());
    
    Equipment equip("drone1", EquipmentType::Drone, "Drone 1");
    auto now = std::chrono::system_clock::now();
    Position pos(37.7749, -122.4194, 10.0, 5.0, now);
    equip.setLastPosition(pos);
    EXPECT_TRUE(storage_->saveEquipment(equip));
    
    // Area far from the equipment's position
    auto result = storage_->findEquipmentInArea(40.7, -74.0, 40.8, -73.9);
    EXPECT_TRUE(result.empty());
}

TEST_F(DataStorageTest, FindEquipmentInAreaWithNoPosition) {
    ASSERT_TRUE(storage_->initialize());
    
    Equipment equip("drone1", EquipmentType::Drone, "Drone 1");
    // No position set
    EXPECT_TRUE(storage_->saveEquipment(equip));
    
    auto result = storage_->findEquipmentInArea(37.7, -122.5, 37.8, -122.4);
    EXPECT_TRUE(result.empty());
}

}  // namespace equipment_tracker
// </test_code>