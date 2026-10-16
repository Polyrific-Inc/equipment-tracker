// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <chrono>
#include <thread>
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
        
        // Create a fresh test directory
        std::filesystem::create_directory(test_db_path_);
        
        // Initialize data storage with test path
        data_storage_ = std::make_unique<DataStorage>(test_db_path_.string());
    }

    void TearDown() override {
        // Clean up the test directory
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove_all(test_db_path_);
        }
    }

    // Helper method to create a test equipment
    Equipment createTestEquipment(const std::string& id = "test-equipment-1") {
        Equipment equipment(id, EquipmentType::Forklift, "Test Forklift");
        equipment.setStatus(EquipmentStatus::Active);
        return equipment;
    }

    // Helper method to create a test position
    Position createTestPosition(double lat = 37.7749, double lon = -122.4194) {
        return Position(lat, lon, 10.0, 5.0, getCurrentTimestamp());
    }

    std::filesystem::path test_db_path_;
    std::unique_ptr<DataStorage> data_storage_;
};

TEST_F(DataStorageTest, Initialize) {
    // Test initialization
    EXPECT_TRUE(data_storage_->initialize());
    
    // Verify that the database directories were created
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "positions"));
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment original = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(original));
    
    // Load the equipment
    auto loaded = data_storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    // Verify equipment properties
    EXPECT_EQ(loaded->getId(), original.getId());
    EXPECT_EQ(loaded->getName(), original.getName());
    EXPECT_EQ(static_cast<int>(loaded->getType()), static_cast<int>(original.getType()));
    EXPECT_EQ(static_cast<int>(loaded->getStatus()), static_cast<int>(original.getStatus()));
}

TEST_F(DataStorageTest, SaveAndLoadEquipmentWithPosition) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create test equipment with position
    Equipment original = createTestEquipment();
    Position position = createTestPosition();
    original.setLastPosition(position);
    
    // Save the equipment
    EXPECT_TRUE(data_storage_->saveEquipment(original));
    
    // Load the equipment
    auto loaded = data_storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    // Verify position data
    auto loaded_position = loaded->getLastPosition();
    ASSERT_TRUE(loaded_position.has_value());
    EXPECT_DOUBLE_EQ(loaded_position->getLatitude(), position.getLatitude());
    EXPECT_DOUBLE_EQ(loaded_position->getLongitude(), position.getLongitude());
    EXPECT_DOUBLE_EQ(loaded_position->getAltitude(), position.getAltitude());
    EXPECT_DOUBLE_EQ(loaded_position->getAccuracy(), position.getAccuracy());
    
    // Timestamps might not match exactly due to serialization/deserialization
    auto loaded_time = std::chrono::system_clock::to_time_t(loaded_position->getTimestamp());
    auto original_time = std::chrono::system_clock::to_time_t(position.getTimestamp());
    EXPECT_EQ(loaded_time, original_time);
}

TEST_F(DataStorageTest, UpdateEquipment) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment original = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(original));
    
    // Update equipment properties
    Equipment updated = original;
    updated.setName("Updated Forklift");
    updated.setStatus(EquipmentStatus::Maintenance);
    
    // Save the updated equipment
    EXPECT_TRUE(data_storage_->updateEquipment(updated));
    
    // Load the equipment
    auto loaded = data_storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    // Verify updated properties
    EXPECT_EQ(loaded->getName(), "Updated Forklift");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::Maintenance);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment original = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(original));
    
    // Verify equipment exists
    ASSERT_TRUE(data_storage_->loadEquipment(original.getId()).has_value());
    
    // Delete the equipment
    EXPECT_TRUE(data_storage_->deleteEquipment(original.getId()));
    
    // Verify equipment no longer exists
    EXPECT_FALSE(data_storage_->loadEquipment(original.getId()).has_value());
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create test equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create and save multiple positions
    Position pos1(37.7749, -122.4194, 10.0, 5.0, 
                 std::chrono::system_clock::from_time_t(1600000000));
    Position pos2(37.7750, -122.4195, 11.0, 4.0, 
                 std::chrono::system_clock::from_time_t(1600001000));
    Position pos3(37.7751, -122.4196, 12.0, 3.0, 
                 std::chrono::system_clock::from_time_t(1600002000));
    
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos1));
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos2));
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos3));
    
    // Get position history for the full time range
    auto history = data_storage_->getPositionHistory(
        equipment.getId(),
        std::chrono::system_clock::from_time_t(1600000000),
        std::chrono::system_clock::from_time_t(1600002000)
    );
    
    // Verify all positions were retrieved
    ASSERT_EQ(history.size(), 3);
    
    // Verify positions are in chronological order
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), pos1.getLatitude());
    EXPECT_DOUBLE_EQ(history[1].getLatitude(), pos2.getLatitude());
    EXPECT_DOUBLE_EQ(history[2].getLatitude(), pos3.getLatitude());
    
    // Get position history for a partial time range
    auto partial_history = data_storage_->getPositionHistory(
        equipment.getId(),
        std::chrono::system_clock::from_time_t(1600000500),
        std::chrono::system_clock::from_time_t(1600001500)
    );
    
    // Verify only the middle position was retrieved
    ASSERT_EQ(partial_history.size(), 1);
    EXPECT_DOUBLE_EQ(partial_history[0].getLatitude(), pos2.getLatitude());
}

TEST_F(DataStorageTest, GetAllEquipment) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save multiple equipment
    Equipment eq1("test-eq-1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("test-eq-2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("test-eq-3", EquipmentType::Bulldozer, "Bulldozer 1");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Get all equipment
    auto all_equipment = data_storage_->getAllEquipment();
    
    // Verify all equipment were retrieved
    ASSERT_EQ(all_equipment.size(), 3);
    
    // Verify equipment IDs (order may vary)
    std::vector<std::string> ids;
    for (const auto& eq : all_equipment) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("test-eq-1", "test-eq-2", "test-eq-3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save equipment with different statuses
    Equipment eq1("test-eq-1", EquipmentType::Forklift, "Forklift 1");
    eq1.setStatus(EquipmentStatus::Active);
    
    Equipment eq2("test-eq-2", EquipmentType::Crane, "Crane 1");
    eq2.setStatus(EquipmentStatus::Maintenance);
    
    Equipment eq3("test-eq-3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setStatus(EquipmentStatus::Active);
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find equipment by status
    auto active_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Active);
    auto maintenance_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    auto inactive_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Inactive);
    
    // Verify results
    ASSERT_EQ(active_equipment.size(), 2);
    ASSERT_EQ(maintenance_equipment.size(), 1);
    ASSERT_EQ(inactive_equipment.size(), 0);
    
    // Verify active equipment IDs
    std::vector<std::string> active_ids;
    for (const auto& eq : active_equipment) {
        active_ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(active_ids, ::testing::UnorderedElementsAre("test-eq-1", "test-eq-3"));
    
    // Verify maintenance equipment ID
    EXPECT_EQ(maintenance_equipment[0].getId(), "test-eq-2");
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save equipment with different types
    Equipment eq1("test-eq-1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("test-eq-2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("test-eq-3", EquipmentType::Forklift, "Forklift 2");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find equipment by type
    auto forklifts = data_storage_->findEquipmentByType(EquipmentType::Forklift);
    auto cranes = data_storage_->findEquipmentByType(EquipmentType::Crane);
    auto bulldozers = data_storage_->findEquipmentByType(EquipmentType::Bulldozer);
    
    // Verify results
    ASSERT_EQ(forklifts.size(), 2);
    ASSERT_EQ(cranes.size(), 1);
    ASSERT_EQ(bulldozers.size(), 0);
    
    // Verify forklift IDs
    std::vector<std::string> forklift_ids;
    for (const auto& eq : forklifts) {
        forklift_ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(forklift_ids, ::testing::UnorderedElementsAre("test-eq-1", "test-eq-3"));
    
    // Verify crane ID
    EXPECT_EQ(cranes[0].getId(), "test-eq-2");
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different positions
    Equipment eq1("test-eq-1", EquipmentType::Forklift, "Forklift 1");
    eq1.setLastPosition(Position(37.7749, -122.4194, 10.0, 5.0, getCurrentTimestamp()));
    
    Equipment eq2("test-eq-2", EquipmentType::Crane, "Crane 1");
    eq2.setLastPosition(Position(37.8000, -122.4000, 10.0, 5.0, getCurrentTimestamp()));
    
    Equipment eq3("test-eq-3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setLastPosition(Position(38.0000, -123.0000, 10.0, 5.0, getCurrentTimestamp()));
    
    Equipment eq4("test-eq-4", EquipmentType::Truck, "Truck 1");
    // No position for eq4
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    ASSERT_TRUE(data_storage_->saveEquipment(eq4));
    
    // Find equipment in area
    auto equipment_in_area = data_storage_->findEquipmentInArea(
        37.7700, -122.4200,
        37.8100, -122.3900
    );
    
    // Verify results
    ASSERT_EQ(equipment_in_area.size(), 2);
    
    // Verify equipment IDs
    std::vector<std::string> area_ids;
    for (const auto& eq : equipment_in_area) {
        area_ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(area_ids, ::testing::UnorderedElementsAre("test-eq-1", "test-eq-2"));
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Try to load non-existent equipment
    auto equipment = data_storage_->loadEquipment("non-existent-id");
    
    // Verify result is empty
    EXPECT_FALSE(equipment.has_value());
}

TEST_F(DataStorageTest, GetPositionHistoryForNonExistentEquipment) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Try to get position history for non-existent equipment
    auto history = data_storage_->getPositionHistory("non-existent-id");
    
    // Verify result is empty
    EXPECT_TRUE(history.empty());
}

TEST_F(DataStorageTest, ConcurrentAccess) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create test equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create multiple threads to access the data storage concurrently
    std::vector<std::thread> threads;
    const int num_threads = 10;
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([this, &equipment, i]() {
            // Each thread performs a different operation
            switch (i % 5) {
                case 0:
                    // Load equipment
                    data_storage_->loadEquipment(equipment.getId());
                    break;
                case 1:
                    // Save position
                    Position pos = createTestPosition(37.7749 + i * 0.0001, -122.4194 + i * 0.0001);
                    data_storage_->savePosition(equipment.getId(), pos);
                    break;
                case 2:
                    // Get position history
                    data_storage_->getPositionHistory(equipment.getId());
                    break;
                case 3:
                    // Update equipment
                    Equipment updated = equipment;
                    updated.setName("Updated " + std::to_string(i));
                    data_storage_->updateEquipment(updated);
                    break;
                case 4:
                    // Get all equipment
                    data_storage_->getAllEquipment();
                    break;
            }
        });
    }
    
    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify the database is still accessible
    EXPECT_TRUE(data_storage_->loadEquipment(equipment.getId()).has_value());
}

TEST_F(DataStorageTest, InitializeWithInvalidPath) {
    // Create data storage with invalid path
    DataStorage invalid_storage("/invalid/path/that/does/not/exist");
    
    // Initialization should still succeed (it creates the directory)
    EXPECT_TRUE(invalid_storage.initialize());
    
    // Clean up
    std::filesystem::remove_all("/invalid/path/that/does/not/exist");
}

TEST_F(DataStorageTest, ReinitializeExistingDatabase) {
    // Initialize the database
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Reinitialize the database
    EXPECT_TRUE(data_storage_->initialize());
    
    // Verify equipment still exists
    EXPECT_TRUE(data_storage_->loadEquipment(equipment.getId()).has_value());
}

} // namespace testing
} // namespace equipment_tracker
// </test_code>