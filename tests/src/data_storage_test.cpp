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
namespace testing {

class DataStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for testing
        test_db_path_ = std::filesystem::temp_directory_path() / "equipment_tracker_test";
        
        // Clean up any existing test directory
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove_all(test_db_path_);
        }
        
        // Create a fresh data storage instance
        data_storage_ = std::make_unique<DataStorage>(test_db_path_.string());
    }

    void TearDown() override {
        // Clean up the test directory
        data_storage_.reset();
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove_all(test_db_path_);
        }
    }

    // Helper method to create a test equipment
    Equipment createTestEquipment(const std::string& id = "test123", 
                                 EquipmentType type = EquipmentType::Forklift,
                                 const std::string& name = "Test Equipment") {
        Equipment equipment(id, type, name);
        equipment.setStatus(EquipmentStatus::Active);
        return equipment;
    }

    // Helper method to create a test position
    Position createTestPosition(double lat = 37.7749, 
                               double lon = -122.4194, 
                               double alt = 10.0,
                               double accuracy = 2.0,
                               Timestamp timestamp = getCurrentTimestamp()) {
        return Position(lat, lon, alt, accuracy, timestamp);
    }

    std::filesystem::path test_db_path_;
    std::unique_ptr<DataStorage> data_storage_;
};

TEST_F(DataStorageTest, InitializeCreatesDirectories) {
    // Initialize the data storage
    EXPECT_TRUE(data_storage_->initialize());
    
    // Check that the directories were created
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "positions"));
    
    // Calling initialize again should return true
    EXPECT_TRUE(data_storage_->initialize());
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    // Initialize the data storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create test equipment
    Equipment equipment = createTestEquipment();
    
    // Save the equipment
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Load the equipment
    auto loaded_equipment = data_storage_->loadEquipment(equipment.getId());
    
    // Check that the equipment was loaded correctly
    ASSERT_TRUE(loaded_equipment.has_value());
    EXPECT_EQ(loaded_equipment->getId(), equipment.getId());
    EXPECT_EQ(loaded_equipment->getName(), equipment.getName());
    EXPECT_EQ(static_cast<int>(loaded_equipment->getType()), static_cast<int>(equipment.getType()));
    EXPECT_EQ(static_cast<int>(loaded_equipment->getStatus()), static_cast<int>(equipment.getStatus()));
}

TEST_F(DataStorageTest, SaveAndLoadEquipmentWithPosition) {
    // Initialize the data storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create test equipment with position
    Equipment equipment = createTestEquipment();
    Position position = createTestPosition();
    equipment.setLastPosition(position);
    
    // Save the equipment
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Load the equipment
    auto loaded_equipment = data_storage_->loadEquipment(equipment.getId());
    
    // Check that the equipment was loaded correctly with position
    ASSERT_TRUE(loaded_equipment.has_value());
    ASSERT_TRUE(loaded_equipment->getLastPosition().has_value());
    
    auto loaded_position = loaded_equipment->getLastPosition().value();
    EXPECT_DOUBLE_EQ(loaded_position.getLatitude(), position.getLatitude());
    EXPECT_DOUBLE_EQ(loaded_position.getLongitude(), position.getLongitude());
    EXPECT_DOUBLE_EQ(loaded_position.getAltitude(), position.getAltitude());
    EXPECT_DOUBLE_EQ(loaded_position.getAccuracy(), position.getAccuracy());
    
    // Timestamps should be the same when converted to time_t
    auto loaded_time = std::chrono::system_clock::to_time_t(loaded_position.getTimestamp());
    auto original_time = std::chrono::system_clock::to_time_t(position.getTimestamp());
    EXPECT_EQ(loaded_time, original_time);
}

TEST_F(DataStorageTest, UpdateEquipment) {
    // Initialize the data storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Update the equipment
    equipment.setName("Updated Name");
    equipment.setStatus(EquipmentStatus::Maintenance);
    
    // Save the updated equipment
    EXPECT_TRUE(data_storage_->updateEquipment(equipment));
    
    // Load the equipment
    auto loaded_equipment = data_storage_->loadEquipment(equipment.getId());
    
    // Check that the equipment was updated correctly
    ASSERT_TRUE(loaded_equipment.has_value());
    EXPECT_EQ(loaded_equipment->getName(), "Updated Name");
    EXPECT_EQ(loaded_equipment->getStatus(), EquipmentStatus::Maintenance);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    // Initialize the data storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Save a position for the equipment
    Position position = createTestPosition();
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), position));
    
    // Delete the equipment
    EXPECT_TRUE(data_storage_->deleteEquipment(equipment.getId()));
    
    // Try to load the equipment
    auto loaded_equipment = data_storage_->loadEquipment(equipment.getId());
    
    // Check that the equipment was deleted
    EXPECT_FALSE(loaded_equipment.has_value());
    
    // Check that the position directory was deleted
    std::string position_dir = test_db_path_.string() + "/positions/" + equipment.getId();
    EXPECT_FALSE(std::filesystem::exists(position_dir));
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    // Initialize the data storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create test equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create test positions with different timestamps
    auto now = getCurrentTimestamp();
    auto one_hour_ago = now - std::chrono::hours(1);
    auto two_hours_ago = now - std::chrono::hours(2);
    
    Position pos1 = createTestPosition(37.7749, -122.4194, 10.0, 2.0, two_hours_ago);
    Position pos2 = createTestPosition(37.7750, -122.4195, 11.0, 2.1, one_hour_ago);
    Position pos3 = createTestPosition(37.7751, -122.4196, 12.0, 2.2, now);
    
    // Save positions
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos1));
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos2));
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos3));
    
    // Get position history for the full time range
    auto history = data_storage_->getPositionHistory(
        equipment.getId(), 
        two_hours_ago - std::chrono::minutes(5), 
        now + std::chrono::minutes(5)
    );
    
    // Check that all positions were retrieved
    ASSERT_EQ(history.size(), 3);
    
    // Positions should be sorted by timestamp
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), pos1.getLatitude());
    EXPECT_DOUBLE_EQ(history[1].getLatitude(), pos2.getLatitude());
    EXPECT_DOUBLE_EQ(history[2].getLatitude(), pos3.getLatitude());
    
    // Get position history for a limited time range
    auto limited_history = data_storage_->getPositionHistory(
        equipment.getId(), 
        one_hour_ago - std::chrono::minutes(5), 
        now + std::chrono::minutes(5)
    );
    
    // Check that only the positions within the time range were retrieved
    ASSERT_EQ(limited_history.size(), 2);
    EXPECT_DOUBLE_EQ(limited_history[0].getLatitude(), pos2.getLatitude());
    EXPECT_DOUBLE_EQ(limited_history[1].getLatitude(), pos3.getLatitude());
}

TEST_F(DataStorageTest, GetAllEquipment) {
    // Initialize the data storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save multiple equipment
    Equipment eq1 = createTestEquipment("eq1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2 = createTestEquipment("eq2", EquipmentType::Crane, "Crane 1");
    Equipment eq3 = createTestEquipment("eq3", EquipmentType::Bulldozer, "Bulldozer 1");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Get all equipment
    auto all_equipment = data_storage_->getAllEquipment();
    
    // Check that all equipment were retrieved
    ASSERT_EQ(all_equipment.size(), 3);
    
    // Check that the equipment IDs match (order may vary)
    std::vector<std::string> ids;
    for (const auto& eq : all_equipment) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("eq1", "eq2", "eq3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    // Initialize the data storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save equipment with different statuses
    Equipment eq1 = createTestEquipment("eq1", EquipmentType::Forklift, "Forklift 1");
    eq1.setStatus(EquipmentStatus::Active);
    
    Equipment eq2 = createTestEquipment("eq2", EquipmentType::Crane, "Crane 1");
    eq2.setStatus(EquipmentStatus::Maintenance);
    
    Equipment eq3 = createTestEquipment("eq3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setStatus(EquipmentStatus::Active);
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find equipment by status
    auto active_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Active);
    auto maintenance_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    auto inactive_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Inactive);
    
    // Check that the correct equipment were found
    ASSERT_EQ(active_equipment.size(), 2);
    ASSERT_EQ(maintenance_equipment.size(), 1);
    ASSERT_EQ(inactive_equipment.size(), 0);
    
    // Check the IDs of the active equipment
    std::vector<std::string> active_ids;
    for (const auto& eq : active_equipment) {
        active_ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(active_ids, ::testing::UnorderedElementsAre("eq1", "eq3"));
    
    // Check the ID of the maintenance equipment
    EXPECT_EQ(maintenance_equipment[0].getId(), "eq2");
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    // Initialize the data storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save equipment with different types
    Equipment eq1 = createTestEquipment("eq1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2 = createTestEquipment("eq2", EquipmentType::Crane, "Crane 1");
    Equipment eq3 = createTestEquipment("eq3", EquipmentType::Forklift, "Forklift 2");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find equipment by type
    auto forklifts = data_storage_->findEquipmentByType(EquipmentType::Forklift);
    auto cranes = data_storage_->findEquipmentByType(EquipmentType::Crane);
    auto bulldozers = data_storage_->findEquipmentByType(EquipmentType::Bulldozer);
    
    // Check that the correct equipment were found
    ASSERT_EQ(forklifts.size(), 2);
    ASSERT_EQ(cranes.size(), 1);
    ASSERT_EQ(bulldozers.size(), 0);
    
    // Check the IDs of the forklifts
    std::vector<std::string> forklift_ids;
    for (const auto& eq : forklifts) {
        forklift_ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(forklift_ids, ::testing::UnorderedElementsAre("eq1", "eq3"));
    
    // Check the ID of the crane
    EXPECT_EQ(cranes[0].getId(), "eq2");
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    // Initialize the data storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different positions
    Equipment eq1 = createTestEquipment("eq1", EquipmentType::Forklift, "Forklift 1");
    eq1.setLastPosition(createTestPosition(37.7749, -122.4194)); // San Francisco
    
    Equipment eq2 = createTestEquipment("eq2", EquipmentType::Crane, "Crane 1");
    eq2.setLastPosition(createTestPosition(34.0522, -118.2437)); // Los Angeles
    
    Equipment eq3 = createTestEquipment("eq3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setLastPosition(createTestPosition(37.7833, -122.4167)); // Also San Francisco
    
    Equipment eq4 = createTestEquipment("eq4", EquipmentType::Truck, "Truck 1");
    // No position for eq4
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    ASSERT_TRUE(data_storage_->saveEquipment(eq4));
    
    // Define a bounding box around San Francisco
    double lat1 = 37.70;
    double lon1 = -122.50;
    double lat2 = 37.80;
    double lon2 = -122.40;
    
    // Find equipment in the San Francisco area
    auto sf_equipment = data_storage_->findEquipmentInArea(lat1, lon1, lat2, lon2);
    
    // Check that the correct equipment were found
    ASSERT_EQ(sf_equipment.size(), 2);
    
    // Check the IDs of the equipment in San Francisco
    std::vector<std::string> sf_ids;
    for (const auto& eq : sf_equipment) {
        sf_ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(sf_ids, ::testing::UnorderedElementsAre("eq1", "eq3"));
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    // Initialize the data storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Try to load non-existent equipment
    auto loaded_equipment = data_storage_->loadEquipment("non_existent_id");
    
    // Check that no equipment was loaded
    EXPECT_FALSE(loaded_equipment.has_value());
}

TEST_F(DataStorageTest, GetPositionHistoryForNonExistentEquipment) {
    // Initialize the data storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Try to get position history for non-existent equipment
    auto history = data_storage_->getPositionHistory("non_existent_id");
    
    // Check that no positions were retrieved
    EXPECT_TRUE(history.empty());
}

TEST_F(DataStorageTest, InitializeWithInvalidPath) {
    // Create a data storage with an invalid path
    DataStorage invalid_storage("/invalid/path/that/should/not/exist");
    
    // Try to initialize
    EXPECT_TRUE(invalid_storage.initialize());
    
    // The initialization should create the directories
    EXPECT_TRUE(std::filesystem::exists("/invalid/path/that/should/not/exist"));
    
    // Clean up
    std::filesystem::remove_all("/invalid/path/that/should/not/exist");
}

} // namespace testing
} // namespace equipment_tracker
// </test_code>