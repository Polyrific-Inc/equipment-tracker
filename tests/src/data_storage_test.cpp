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
    
    // Create and save equipment
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
    
    // Create and save equipment
    Equipment original = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(original));
    
    // Update the equipment
    Equipment updated = original;
    updated.setName("Updated Name");
    updated.setStatus(EquipmentStatus::InUse);
    ASSERT_TRUE(data_storage_->updateEquipment(updated));
    
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
    auto hour_ago = now - std::chrono::hours(1);
    auto two_hours_ago = now - std::chrono::hours(2);
    
    Position pos1(37.7749, -122.4194, 10.0, 5.0, two_hours_ago);
    Position pos2(37.7750, -122.4195, 11.0, 4.0, hour_ago);
    Position pos3(37.7751, -122.4196, 12.0, 3.0, now);
    
    // Save positions
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos1));
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos2));
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos3));
    
    // Get position history for the full time range
    auto history = data_storage_->getPositionHistory(
        equipment.getId(), two_hours_ago, now);
    
    // Verify we got all positions
    ASSERT_EQ(history.size(), 3);
    
    // Verify positions are in chronological order
    EXPECT_NEAR(history[0].getLatitude(), pos1.getLatitude(), 0.0001);
    EXPECT_NEAR(history[1].getLatitude(), pos2.getLatitude(), 0.0001);
    EXPECT_NEAR(history[2].getLatitude(), pos3.getLatitude(), 0.0001);
    
    // Get position history for a partial time range
    auto partial_history = data_storage_->getPositionHistory(
        equipment.getId(), hour_ago, now);
    
    // Verify we got only the positions in that time range
    ASSERT_EQ(partial_history.size(), 2);
    EXPECT_NEAR(partial_history[0].getLatitude(), pos2.getLatitude(), 0.0001);
    EXPECT_NEAR(partial_history[1].getLatitude(), pos3.getLatitude(), 0.0001);
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
    
    // Verify we got all equipment
    ASSERT_EQ(all_equipment.size(), 3);
    
    // Verify the IDs match what we expect
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
    auto in_use = data_storage_->findEquipmentByStatus(EquipmentStatus::InUse);
    auto maintenance = data_storage_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    
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
    auto tools = data_storage_->findEquipmentByType(EquipmentType::Tool);
    auto other = data_storage_->findEquipmentByType(EquipmentType::Other);
    
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
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment at different locations
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
    auto bay_area = data_storage_->findEquipmentInArea(37.0, -123.0, 38.0, -121.0);
    
    // Verify results
    ASSERT_EQ(bay_area.size(), 2);
    
    // Verify the correct equipment was found
    std::vector<std::string> bay_area_ids;
    for (const auto& equip : bay_area) {
        bay_area_ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(bay_area_ids, ::testing::UnorderedElementsAre("equip1", "equip3"));
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

TEST_F(DataStorageTest, EquipmentWithoutPosition) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment without a position
    Equipment equipment("no_pos", EquipmentType::Tool, "No Position");
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Load the equipment
    auto loaded = data_storage_->loadEquipment("no_pos");
    ASSERT_TRUE(loaded.has_value());
    
    // Verify no position is set
    EXPECT_FALSE(loaded->getLastPosition().has_value());
    
    // Verify it's not found in area searches
    auto in_area = data_storage_->findEquipmentInArea(0.0, 0.0, 90.0, 180.0);
    EXPECT_EQ(in_area.size(), 0);
}

TEST_F(DataStorageTest, GetPositionHistoryEmptyRange) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Get position history for a time range with no positions
    auto now = std::chrono::system_clock::now();
    auto future = now + std::chrono::hours(1);
    
    auto history = data_storage_->getPositionHistory(equipment.getId(), now, future);
    
    // Verify we got no positions
    EXPECT_EQ(history.size(), 0);
}

TEST_F(DataStorageTest, GetPositionHistoryNonExistentEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Get position history for non-existent equipment
    auto now = std::chrono::system_clock::now();
    auto history = data_storage_->getPositionHistory("nonexistent", now, now);
    
    // Verify we got no positions
    EXPECT_EQ(history.size(), 0);
}

} // namespace equipment_tracker

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>