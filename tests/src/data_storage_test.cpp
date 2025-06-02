// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <thread>
#include "equipment_tracker/data_storage.h"
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/types.h"
#include "equipment_tracker/utils/constants.h"

namespace equipment_tracker {

// Test fixture for DataStorage tests
class DataStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for testing
        test_db_path_ = std::filesystem::temp_directory_path() / "equipment_tracker_test";
        
        // Clean up any existing test directory
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove_all(test_db_path_);
        }
        
        // Create the data storage with the test path
        data_storage_ = std::make_unique<DataStorage>(test_db_path_.string());
    }

    void TearDown() override {
        // Clean up the test directory
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove_all(test_db_path_);
        }
    }

    // Helper method to create a test equipment
    Equipment createTestEquipment(const std::string& id = "test_id", 
                                 EquipmentType type = EquipmentType::Forklift,
                                 const std::string& name = "Test Equipment") {
        return Equipment(id, type, name);
    }

    // Helper method to create a test position
    Position createTestPosition(double lat = 40.7128, 
                               double lon = -74.0060, 
                               double alt = 10.0,
                               double accuracy = 2.0,
                               Timestamp timestamp = getCurrentTimestamp()) {
        return Position(lat, lon, alt, accuracy, timestamp);
    }

    std::filesystem::path test_db_path_;
    std::unique_ptr<DataStorage> data_storage_;
};

// Test initialization
TEST_F(DataStorageTest, Initialize) {
    EXPECT_TRUE(data_storage_->initialize());
    
    // Verify that the database directories were created
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "positions"));
}

// Test saving and loading equipment
TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create test equipment
    Equipment equipment = createTestEquipment("equip1", EquipmentType::Forklift, "Test Forklift");
    equipment.setStatus(EquipmentStatus::Active);
    
    // Save the equipment
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Load the equipment
    auto loaded_equipment = data_storage_->loadEquipment("equip1");
    ASSERT_TRUE(loaded_equipment.has_value());
    
    // Verify the loaded equipment matches the original
    EXPECT_EQ(loaded_equipment->getId(), "equip1");
    EXPECT_EQ(loaded_equipment->getName(), "Test Forklift");
    EXPECT_EQ(loaded_equipment->getType(), EquipmentType::Forklift);
    EXPECT_EQ(loaded_equipment->getStatus(), EquipmentStatus::Active);
}

// Test loading non-existent equipment
TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Try to load non-existent equipment
    auto loaded_equipment = data_storage_->loadEquipment("non_existent");
    EXPECT_FALSE(loaded_equipment.has_value());
}

// Test updating equipment
TEST_F(DataStorageTest, UpdateEquipment) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment equipment = createTestEquipment("equip1", EquipmentType::Forklift, "Test Forklift");
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Update the equipment
    equipment.setName("Updated Forklift");
    equipment.setStatus(EquipmentStatus::Maintenance);
    EXPECT_TRUE(data_storage_->updateEquipment(equipment));
    
    // Load the equipment and verify updates
    auto loaded_equipment = data_storage_->loadEquipment("equip1");
    ASSERT_TRUE(loaded_equipment.has_value());
    EXPECT_EQ(loaded_equipment->getName(), "Updated Forklift");
    EXPECT_EQ(loaded_equipment->getStatus(), EquipmentStatus::Maintenance);
}

// Test deleting equipment
TEST_F(DataStorageTest, DeleteEquipment) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment equipment = createTestEquipment("equip1");
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Delete the equipment
    EXPECT_TRUE(data_storage_->deleteEquipment("equip1"));
    
    // Verify the equipment was deleted
    auto loaded_equipment = data_storage_->loadEquipment("equip1");
    EXPECT_FALSE(loaded_equipment.has_value());
}

// Test saving and loading equipment with position
TEST_F(DataStorageTest, SaveAndLoadEquipmentWithPosition) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create test equipment with position
    Equipment equipment = createTestEquipment("equip1");
    Position position = createTestPosition(40.7128, -74.0060);
    equipment.setLastPosition(position);
    
    // Save the equipment
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Load the equipment
    auto loaded_equipment = data_storage_->loadEquipment("equip1");
    ASSERT_TRUE(loaded_equipment.has_value());
    
    // Verify the position was loaded correctly
    auto loaded_position = loaded_equipment->getLastPosition();
    ASSERT_TRUE(loaded_position.has_value());
    EXPECT_NEAR(loaded_position->getLatitude(), 40.7128, 1e-10);
    EXPECT_NEAR(loaded_position->getLongitude(), -74.0060, 1e-10);
    EXPECT_NEAR(loaded_position->getAltitude(), 10.0, 1e-10);
    EXPECT_NEAR(loaded_position->getAccuracy(), 2.0, 1e-10);
}

// Test saving and retrieving position history
TEST_F(DataStorageTest, SaveAndRetrievePositionHistory) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment ID
    std::string equipment_id = "equip1";
    
    // Create test positions with different timestamps
    auto now = getCurrentTimestamp();
    auto one_hour_ago = now - std::chrono::hours(1);
    auto two_hours_ago = now - std::chrono::hours(2);
    
    Position pos1(40.7128, -74.0060, 10.0, 2.0, two_hours_ago);
    Position pos2(40.7129, -74.0061, 11.0, 2.1, one_hour_ago);
    Position pos3(40.7130, -74.0062, 12.0, 2.2, now);
    
    // Save positions
    EXPECT_TRUE(data_storage_->savePosition(equipment_id, pos1));
    EXPECT_TRUE(data_storage_->savePosition(equipment_id, pos2));
    EXPECT_TRUE(data_storage_->savePosition(equipment_id, pos3));
    
    // Retrieve all position history
    auto history = data_storage_->getPositionHistory(equipment_id, two_hours_ago, now);
    EXPECT_EQ(history.size(), 3);
    
    // Verify positions are in chronological order
    if (history.size() >= 3) {
        EXPECT_NEAR(history[0].getLatitude(), 40.7128, 1e-10);
        EXPECT_NEAR(history[1].getLatitude(), 40.7129, 1e-10);
        EXPECT_NEAR(history[2].getLatitude(), 40.7130, 1e-10);
    }
    
    // Retrieve partial history
    auto recent_history = data_storage_->getPositionHistory(equipment_id, one_hour_ago, now);
    EXPECT_EQ(recent_history.size(), 2);
    
    if (recent_history.size() >= 2) {
        EXPECT_NEAR(recent_history[0].getLatitude(), 40.7129, 1e-10);
        EXPECT_NEAR(recent_history[1].getLatitude(), 40.7130, 1e-10);
    }
}

// Test getting all equipment
TEST_F(DataStorageTest, GetAllEquipment) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save multiple equipment
    Equipment equip1("equip1", EquipmentType::Forklift, "Forklift 1");
    Equipment equip2("equip2", EquipmentType::Crane, "Crane 1");
    Equipment equip3("equip3", EquipmentType::Bulldozer, "Bulldozer 1");
    
    ASSERT_TRUE(data_storage_->saveEquipment(equip1));
    ASSERT_TRUE(data_storage_->saveEquipment(equip2));
    ASSERT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Get all equipment
    auto all_equipment = data_storage_->getAllEquipment();
    EXPECT_EQ(all_equipment.size(), 3);
    
    // Verify equipment IDs
    std::vector<std::string> ids;
    for (const auto& equip : all_equipment) {
        ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("equip1", "equip2", "equip3"));
}

// Test finding equipment by status
TEST_F(DataStorageTest, FindEquipmentByStatus) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save equipment with different statuses
    Equipment equip1("equip1", EquipmentType::Forklift, "Forklift 1");
    equip1.setStatus(EquipmentStatus::Active);
    
    Equipment equip2("equip2", EquipmentType::Crane, "Crane 1");
    equip2.setStatus(EquipmentStatus::Maintenance);
    
    Equipment equip3("equip3", EquipmentType::Bulldozer, "Bulldozer 1");
    equip3.setStatus(EquipmentStatus::Active);
    
    ASSERT_TRUE(data_storage_->saveEquipment(equip1));
    ASSERT_TRUE(data_storage_->saveEquipment(equip2));
    ASSERT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Find equipment by status
    auto active_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Active);
    EXPECT_EQ(active_equipment.size(), 2);
    
    auto maintenance_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(maintenance_equipment.size(), 1);
    
    auto inactive_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Inactive);
    EXPECT_EQ(inactive_equipment.size(), 0);
}

// Test finding equipment by type
TEST_F(DataStorageTest, FindEquipmentByType) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save equipment with different types
    Equipment equip1("equip1", EquipmentType::Forklift, "Forklift 1");
    Equipment equip2("equip2", EquipmentType::Crane, "Crane 1");
    Equipment equip3("equip3", EquipmentType::Forklift, "Forklift 2");
    
    ASSERT_TRUE(data_storage_->saveEquipment(equip1));
    ASSERT_TRUE(data_storage_->saveEquipment(equip2));
    ASSERT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Find equipment by type
    auto forklifts = data_storage_->findEquipmentByType(EquipmentType::Forklift);
    EXPECT_EQ(forklifts.size(), 2);
    
    auto cranes = data_storage_->findEquipmentByType(EquipmentType::Crane);
    EXPECT_EQ(cranes.size(), 1);
    
    auto bulldozers = data_storage_->findEquipmentByType(EquipmentType::Bulldozer);
    EXPECT_EQ(bulldozers.size(), 0);
}

// Test finding equipment in area
TEST_F(DataStorageTest, FindEquipmentInArea) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different positions
    Equipment equip1("equip1", EquipmentType::Forklift, "Forklift 1");
    equip1.setLastPosition(Position(40.7128, -74.0060)); // New York
    
    Equipment equip2("equip2", EquipmentType::Crane, "Crane 1");
    equip2.setLastPosition(Position(34.0522, -118.2437)); // Los Angeles
    
    Equipment equip3("equip3", EquipmentType::Bulldozer, "Bulldozer 1");
    equip3.setLastPosition(Position(41.8781, -87.6298)); // Chicago
    
    Equipment equip4("equip4", EquipmentType::Truck, "Truck 1");
    // No position set
    
    ASSERT_TRUE(data_storage_->saveEquipment(equip1));
    ASSERT_TRUE(data_storage_->saveEquipment(equip2));
    ASSERT_TRUE(data_storage_->saveEquipment(equip3));
    ASSERT_TRUE(data_storage_->saveEquipment(equip4));
    
    // Find equipment in East Coast area
    auto east_coast = data_storage_->findEquipmentInArea(39.0, -75.0, 42.0, -73.0);
    EXPECT_EQ(east_coast.size(), 1);
    if (!east_coast.empty()) {
        EXPECT_EQ(east_coast[0].getId(), "equip1");
    }
    
    // Find equipment in Midwest area
    auto midwest = data_storage_->findEquipmentInArea(40.0, -89.0, 43.0, -86.0);
    EXPECT_EQ(midwest.size(), 1);
    if (!midwest.empty()) {
        EXPECT_EQ(midwest[0].getId(), "equip3");
    }
    
    // Find equipment in West Coast area
    auto west_coast = data_storage_->findEquipmentInArea(33.0, -119.0, 35.0, -118.0);
    EXPECT_EQ(west_coast.size(), 1);
    if (!west_coast.empty()) {
        EXPECT_EQ(west_coast[0].getId(), "equip2");
    }
    
    // Find equipment in an area with no equipment
    auto no_equipment = data_storage_->findEquipmentInArea(45.0, -120.0, 47.0, -118.0);
    EXPECT_EQ(no_equipment.size(), 0);
}

// Test concurrent operations
TEST_F(DataStorageTest, ConcurrentOperations) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create test equipment
    Equipment equipment = createTestEquipment("equip1");
    
    // Create threads for concurrent operations
    std::thread save_thread([&]() {
        for (int i = 0; i < 10; i++) {
            equipment.setName("Equipment " + std::to_string(i));
            EXPECT_TRUE(data_storage_->saveEquipment(equipment));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    std::thread load_thread([&]() {
        for (int i = 0; i < 10; i++) {
            auto loaded = data_storage_->loadEquipment("equip1");
            if (loaded.has_value()) {
                // Just verify we can load it, name might change
                EXPECT_FALSE(loaded->getName().empty());
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // Wait for threads to complete
    save_thread.join();
    load_thread.join();
    
    // Verify the equipment was saved
    auto loaded_equipment = data_storage_->loadEquipment("equip1");
    ASSERT_TRUE(loaded_equipment.has_value());
    EXPECT_EQ(loaded_equipment->getName(), "Equipment 9");
}

// Test error handling for invalid paths
TEST_F(DataStorageTest, InvalidPathHandling) {
    // Create a data storage with an invalid path
    #ifdef _WIN32
    DataStorage invalid_storage("Z:\\non\\existent\\path\\that\\should\\fail");
    #else
    DataStorage invalid_storage("/root/non/existent/path/that/should/fail");
    #endif
    
    // Initialization should fail gracefully
    EXPECT_FALSE(invalid_storage.initialize());
}

// Test database initialization when already initialized
TEST_F(DataStorageTest, InitializeWhenAlreadyInitialized) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Initialize again should return true without error
    EXPECT_TRUE(data_storage_->initialize());
}

} // namespace equipment_tracker
// </test_code>