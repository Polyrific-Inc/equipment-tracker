// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <cstdio>
#include "equipment_tracker/data_storage.h"
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/types.h"
#include "equipment_tracker/utils/constants.h"

namespace equipment_tracker {

class DataStorageTest : public ::testing::Test {
protected:
    std::string test_db_path;

    void SetUp() override {
        // Create a unique temporary directory for each test
        test_db_path = "test_db_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        
        // Ensure the directory doesn't exist
        if (std::filesystem::exists(test_db_path)) {
            std::filesystem::remove_all(test_db_path);
        }
    }

    void TearDown() override {
        // Clean up the test directory
        if (std::filesystem::exists(test_db_path)) {
            std::filesystem::remove_all(test_db_path);
        }
    }

    // Helper method to create a test equipment
    Equipment createTestEquipment(const std::string& id = "test_id") {
        Equipment equipment(id, EquipmentType::Forklift, "Test Equipment");
        equipment.setStatus(EquipmentStatus::Active);
        
        // Add a position
        Position position(37.7749, -122.4194, 10.0, 5.0);
        equipment.setLastPosition(position);
        
        return equipment;
    }
};

TEST_F(DataStorageTest, InitializeCreatesDirectories) {
    DataStorage storage(test_db_path);
    
    ASSERT_TRUE(storage.initialize());
    
    // Check that the directories were created
    EXPECT_TRUE(std::filesystem::exists(test_db_path));
    EXPECT_TRUE(std::filesystem::exists(test_db_path + "/equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path + "/positions"));
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    // Create and save equipment
    Equipment original = createTestEquipment();
    ASSERT_TRUE(storage.saveEquipment(original));
    
    // Load the equipment
    auto loaded = storage.loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    // Verify the loaded equipment matches the original
    EXPECT_EQ(loaded->getId(), original.getId());
    EXPECT_EQ(loaded->getName(), original.getName());
    EXPECT_EQ(static_cast<int>(loaded->getType()), static_cast<int>(original.getType()));
    EXPECT_EQ(static_cast<int>(loaded->getStatus()), static_cast<int>(original.getStatus()));
    
    // Verify position data
    auto original_pos = original.getLastPosition();
    auto loaded_pos = loaded->getLastPosition();
    ASSERT_TRUE(loaded_pos.has_value());
    ASSERT_TRUE(original_pos.has_value());
    
    EXPECT_DOUBLE_EQ(loaded_pos->getLatitude(), original_pos->getLatitude());
    EXPECT_DOUBLE_EQ(loaded_pos->getLongitude(), original_pos->getLongitude());
    EXPECT_DOUBLE_EQ(loaded_pos->getAltitude(), original_pos->getAltitude());
    EXPECT_DOUBLE_EQ(loaded_pos->getAccuracy(), original_pos->getAccuracy());
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    // Try to load non-existent equipment
    auto loaded = storage.loadEquipment("non_existent_id");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    // Create and save equipment
    Equipment original = createTestEquipment();
    ASSERT_TRUE(storage.saveEquipment(original));
    
    // Modify and update the equipment
    Equipment updated = original;
    updated.setName("Updated Name");
    updated.setStatus(EquipmentStatus::Maintenance);
    
    Position new_position(38.8951, -77.0364, 20.0, 3.0);
    updated.setLastPosition(new_position);
    
    ASSERT_TRUE(storage.updateEquipment(updated));
    
    // Load the updated equipment
    auto loaded = storage.loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    // Verify the updated values
    EXPECT_EQ(loaded->getName(), "Updated Name");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::Maintenance);
    
    auto loaded_pos = loaded->getLastPosition();
    ASSERT_TRUE(loaded_pos.has_value());
    EXPECT_DOUBLE_EQ(loaded_pos->getLatitude(), 38.8951);
    EXPECT_DOUBLE_EQ(loaded_pos->getLongitude(), -77.0364);
    EXPECT_DOUBLE_EQ(loaded_pos->getAltitude(), 20.0);
    EXPECT_DOUBLE_EQ(loaded_pos->getAccuracy(), 3.0);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    // Create and save equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(storage.saveEquipment(equipment));
    
    // Verify it exists
    EXPECT_TRUE(storage.loadEquipment(equipment.getId()).has_value());
    
    // Delete the equipment
    ASSERT_TRUE(storage.deleteEquipment(equipment.getId()));
    
    // Verify it no longer exists
    EXPECT_FALSE(storage.loadEquipment(equipment.getId()).has_value());
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    // Create equipment
    std::string equipment_id = "test_equipment";
    
    // Create positions with different timestamps
    auto now = std::chrono::system_clock::now();
    Position pos1(37.7749, -122.4194, 10.0, 5.0, now - std::chrono::hours(2));
    Position pos2(37.7750, -122.4195, 11.0, 4.0, now - std::chrono::hours(1));
    Position pos3(37.7751, -122.4196, 12.0, 3.0, now);
    
    // Save positions
    ASSERT_TRUE(storage.savePosition(equipment_id, pos1));
    ASSERT_TRUE(storage.savePosition(equipment_id, pos2));
    ASSERT_TRUE(storage.savePosition(equipment_id, pos3));
    
    // Get all position history
    auto history = storage.getPositionHistory(equipment_id);
    ASSERT_EQ(history.size(), 3);
    
    // Positions should be sorted by timestamp
    EXPECT_NEAR(history[0].getLatitude(), pos1.getLatitude(), 0.0001);
    EXPECT_NEAR(history[1].getLatitude(), pos2.getLatitude(), 0.0001);
    EXPECT_NEAR(history[2].getLatitude(), pos3.getLatitude(), 0.0001);
    
    // Get position history with time range
    auto filtered_history = storage.getPositionHistory(
        equipment_id, 
        now - std::chrono::minutes(90), 
        now - std::chrono::minutes(30)
    );
    
    ASSERT_EQ(filtered_history.size(), 1);
    EXPECT_NEAR(filtered_history[0].getLatitude(), pos2.getLatitude(), 0.0001);
}

TEST_F(DataStorageTest, GetAllEquipment) {
    DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    // Create and save multiple equipment
    Equipment eq1("id1", EquipmentType::Forklift, "Equipment 1");
    Equipment eq2("id2", EquipmentType::Crane, "Equipment 2");
    Equipment eq3("id3", EquipmentType::Bulldozer, "Equipment 3");
    
    ASSERT_TRUE(storage.saveEquipment(eq1));
    ASSERT_TRUE(storage.saveEquipment(eq2));
    ASSERT_TRUE(storage.saveEquipment(eq3));
    
    // Get all equipment
    auto all_equipment = storage.getAllEquipment();
    ASSERT_EQ(all_equipment.size(), 3);
    
    // Verify all equipment were loaded
    std::vector<std::string> ids;
    for (const auto& eq : all_equipment) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("id1", "id2", "id3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    // Create equipment with different statuses
    Equipment eq1("id1", EquipmentType::Forklift, "Equipment 1");
    eq1.setStatus(EquipmentStatus::Active);
    
    Equipment eq2("id2", EquipmentType::Crane, "Equipment 2");
    eq2.setStatus(EquipmentStatus::Maintenance);
    
    Equipment eq3("id3", EquipmentType::Bulldozer, "Equipment 3");
    eq3.setStatus(EquipmentStatus::Active);
    
    ASSERT_TRUE(storage.saveEquipment(eq1));
    ASSERT_TRUE(storage.saveEquipment(eq2));
    ASSERT_TRUE(storage.saveEquipment(eq3));
    
    // Find equipment by status
    auto active_equipment = storage.findEquipmentByStatus(EquipmentStatus::Active);
    ASSERT_EQ(active_equipment.size(), 2);
    
    std::vector<std::string> active_ids;
    for (const auto& eq : active_equipment) {
        active_ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(active_ids, ::testing::UnorderedElementsAre("id1", "id3"));
    
    // Find equipment with a different status
    auto maintenance_equipment = storage.findEquipmentByStatus(EquipmentStatus::Maintenance);
    ASSERT_EQ(maintenance_equipment.size(), 1);
    EXPECT_EQ(maintenance_equipment[0].getId(), "id2");
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    // Create equipment with different types
    Equipment eq1("id1", EquipmentType::Forklift, "Equipment 1");
    Equipment eq2("id2", EquipmentType::Crane, "Equipment 2");
    Equipment eq3("id3", EquipmentType::Forklift, "Equipment 3");
    
    ASSERT_TRUE(storage.saveEquipment(eq1));
    ASSERT_TRUE(storage.saveEquipment(eq2));
    ASSERT_TRUE(storage.saveEquipment(eq3));
    
    // Find equipment by type
    auto forklifts = storage.findEquipmentByType(EquipmentType::Forklift);
    ASSERT_EQ(forklifts.size(), 2);
    
    std::vector<std::string> forklift_ids;
    for (const auto& eq : forklifts) {
        forklift_ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(forklift_ids, ::testing::UnorderedElementsAre("id1", "id3"));
    
    // Find equipment with a different type
    auto cranes = storage.findEquipmentByType(EquipmentType::Crane);
    ASSERT_EQ(cranes.size(), 1);
    EXPECT_EQ(cranes[0].getId(), "id2");
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    // Create equipment with different positions
    Equipment eq1("id1", EquipmentType::Forklift, "Equipment 1");
    eq1.setLastPosition(Position(37.7749, -122.4194)); // San Francisco
    
    Equipment eq2("id2", EquipmentType::Crane, "Equipment 2");
    eq2.setLastPosition(Position(34.0522, -118.2437)); // Los Angeles
    
    Equipment eq3("id3", EquipmentType::Bulldozer, "Equipment 3");
    eq3.setLastPosition(Position(37.8044, -122.2712)); // Oakland
    
    Equipment eq4("id4", EquipmentType::Truck, "Equipment 4");
    // No position set
    
    ASSERT_TRUE(storage.saveEquipment(eq1));
    ASSERT_TRUE(storage.saveEquipment(eq2));
    ASSERT_TRUE(storage.saveEquipment(eq3));
    ASSERT_TRUE(storage.saveEquipment(eq4));
    
    // Find equipment in the Bay Area
    auto bay_area_equipment = storage.findEquipmentInArea(
        37.5, -123.0, // Southwest corner
        38.0, -122.0  // Northeast corner
    );
    
    ASSERT_EQ(bay_area_equipment.size(), 2);
    
    std::vector<std::string> bay_area_ids;
    for (const auto& eq : bay_area_equipment) {
        bay_area_ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(bay_area_ids, ::testing::UnorderedElementsAre("id1", "id3"));
    
    // Find equipment in Southern California
    auto socal_equipment = storage.findEquipmentInArea(
        33.5, -119.0, // Southwest corner
        34.5, -118.0  // Northeast corner
    );
    
    ASSERT_EQ(socal_equipment.size(), 1);
    EXPECT_EQ(socal_equipment[0].getId(), "id2");
}

TEST_F(DataStorageTest, InitializeWithNonExistentDirectory) {
    // Use a nested directory that doesn't exist
    std::string nested_path = test_db_path + "/nested/db";
    DataStorage storage(nested_path);
    
    ASSERT_TRUE(storage.initialize());
    
    // Check that the directories were created
    EXPECT_TRUE(std::filesystem::exists(nested_path));
    EXPECT_TRUE(std::filesystem::exists(nested_path + "/equipment"));
    EXPECT_TRUE(std::filesystem::exists(nested_path + "/positions"));
}

TEST_F(DataStorageTest, SaveEquipmentWithoutInitialization) {
    DataStorage storage(test_db_path);
    
    // Save equipment without explicit initialization
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(storage.saveEquipment(equipment));
    
    // Verify it was saved correctly
    auto loaded = storage.loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getId(), equipment.getId());
}

TEST_F(DataStorageTest, LoadEquipmentWithoutInitialization) {
    DataStorage storage(test_db_path);
    
    // First save equipment to ensure it exists
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(storage.saveEquipment(equipment));
    
    // Create a new storage instance without initialization
    DataStorage new_storage(test_db_path);
    
    // Load equipment without explicit initialization
    auto loaded = new_storage.loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getId(), equipment.getId());
}

TEST_F(DataStorageTest, SavePositionWithoutInitialization) {
    DataStorage storage(test_db_path);
    
    // Save position without explicit initialization
    std::string equipment_id = "test_equipment";
    Position position(37.7749, -122.4194);
    
    ASSERT_TRUE(storage.savePosition(equipment_id, position));
    
    // Verify it was saved correctly
    auto history = storage.getPositionHistory(equipment_id);
    ASSERT_EQ(history.size(), 1);
    EXPECT_NEAR(history[0].getLatitude(), position.getLatitude(), 0.0001);
}

TEST_F(DataStorageTest, GetPositionHistoryWithEmptyRange) {
    DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    std::string equipment_id = "test_equipment";
    
    // Create positions with different timestamps
    auto now = std::chrono::system_clock::now();
    Position pos1(37.7749, -122.4194, 10.0, 5.0, now - std::chrono::hours(2));
    Position pos2(37.7750, -122.4195, 11.0, 4.0, now - std::chrono::hours(1));
    
    // Save positions
    ASSERT_TRUE(storage.savePosition(equipment_id, pos1));
    ASSERT_TRUE(storage.savePosition(equipment_id, pos2));
    
    // Get position history with empty time range (should return all positions)
    auto history = storage.getPositionHistory(equipment_id, Timestamp(), Timestamp());
    
    // Should return all positions
    ASSERT_EQ(history.size(), 2);
}

TEST_F(DataStorageTest, GetPositionHistoryForNonExistentEquipment) {
    DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    // Get position history for non-existent equipment
    auto history = storage.getPositionHistory("non_existent_id");
    
    // Should return an empty vector
    EXPECT_TRUE(history.empty());
}

TEST_F(DataStorageTest, FindEquipmentWithNoResults) {
    DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    // Create some equipment
    Equipment eq1("id1", EquipmentType::Forklift, "Equipment 1");
    eq1.setStatus(EquipmentStatus::Active);
    
    ASSERT_TRUE(storage.saveEquipment(eq1));
    
    // Find equipment with a status that doesn't exist
    auto inactive_equipment = storage.findEquipmentByStatus(EquipmentStatus::Inactive);
    EXPECT_TRUE(inactive_equipment.empty());
    
    // Find equipment with a type that doesn't exist
    auto excavators = storage.findEquipmentByType(EquipmentType::Excavator);
    EXPECT_TRUE(excavators.empty());
    
    // Find equipment in an area with no equipment
    auto remote_equipment = storage.findEquipmentInArea(0.0, 0.0, 1.0, 1.0);
    EXPECT_TRUE(remote_equipment.empty());
}

}  // namespace equipment_tracker
// </test_code>