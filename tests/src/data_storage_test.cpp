// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <chrono>
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

// Test initialization
TEST_F(DataStorageTest, Initialize) {
    EXPECT_TRUE(data_storage_->initialize());
    
    // Verify that the directory structure was created
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "positions"));
}

// Test saving and loading equipment
TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment original = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(original));
    
    // Load the equipment and verify it matches
    auto loaded = data_storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getId(), original.getId());
    EXPECT_EQ(loaded->getName(), original.getName());
    EXPECT_EQ(static_cast<int>(loaded->getType()), static_cast<int>(original.getType()));
    EXPECT_EQ(static_cast<int>(loaded->getStatus()), static_cast<int>(original.getStatus()));
}

// Test saving equipment with position
TEST_F(DataStorageTest, SaveAndLoadEquipmentWithPosition) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with position
    Equipment original = createTestEquipment();
    Position pos = createTestPosition();
    original.setLastPosition(pos);
    
    // Save and load
    EXPECT_TRUE(data_storage_->saveEquipment(original));
    auto loaded = data_storage_->loadEquipment(original.getId());
    
    // Verify position was saved and loaded correctly
    ASSERT_TRUE(loaded.has_value());
    ASSERT_TRUE(loaded->getLastPosition().has_value());
    
    auto loaded_pos = loaded->getLastPosition().value();
    EXPECT_DOUBLE_EQ(loaded_pos.getLatitude(), pos.getLatitude());
    EXPECT_DOUBLE_EQ(loaded_pos.getLongitude(), pos.getLongitude());
    EXPECT_DOUBLE_EQ(loaded_pos.getAltitude(), pos.getAltitude());
    EXPECT_DOUBLE_EQ(loaded_pos.getAccuracy(), pos.getAccuracy());
    
    // Timestamps might not be exactly equal due to serialization/deserialization
    // so we check if they're close enough (within 1 second)
    auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(
        loaded_pos.getTimestamp() - pos.getTimestamp()).count();
    EXPECT_LE(std::abs(time_diff), 1);
}

// Test updating equipment
TEST_F(DataStorageTest, UpdateEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment original = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(original));
    
    // Update equipment
    Equipment updated = original;
    updated.setName("Updated Forklift");
    updated.setStatus(EquipmentStatus::Maintenance);
    
    EXPECT_TRUE(data_storage_->updateEquipment(updated));
    
    // Load and verify updates
    auto loaded = data_storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getName(), "Updated Forklift");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::Maintenance);
}

// Test deleting equipment
TEST_F(DataStorageTest, DeleteEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment original = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(original));
    
    // Verify it exists
    EXPECT_TRUE(data_storage_->loadEquipment(original.getId()).has_value());
    
    // Delete it
    EXPECT_TRUE(data_storage_->deleteEquipment(original.getId()));
    
    // Verify it's gone
    EXPECT_FALSE(data_storage_->loadEquipment(original.getId()).has_value());
}

// Test saving and retrieving position history
TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create positions with different timestamps
    auto now = getCurrentTimestamp();
    auto one_hour_ago = now - std::chrono::hours(1);
    auto two_hours_ago = now - std::chrono::hours(2);
    
    Position pos1(37.7749, -122.4194, 10.0, 5.0, two_hours_ago);
    Position pos2(37.7750, -122.4195, 11.0, 5.0, one_hour_ago);
    Position pos3(37.7751, -122.4196, 12.0, 5.0, now);
    
    // Save positions
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos1));
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos2));
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos3));
    
    // Get all position history
    auto history = data_storage_->getPositionHistory(equipment.getId());
    EXPECT_EQ(history.size(), 3);
    
    // Get position history with time range
    auto filtered_history = data_storage_->getPositionHistory(
        equipment.getId(), one_hour_ago - std::chrono::minutes(5), now + std::chrono::minutes(5));
    EXPECT_EQ(filtered_history.size(), 2);
    
    // Verify positions are ordered by timestamp
    if (filtered_history.size() >= 2) {
        EXPECT_LE(filtered_history[0].getTimestamp(), filtered_history[1].getTimestamp());
    }
}

// Test getting all equipment
TEST_F(DataStorageTest, GetAllEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save multiple equipment
    Equipment eq1("test-eq-1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("test-eq-2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("test-eq-3", EquipmentType::Bulldozer, "Bulldozer 1");
    
    EXPECT_TRUE(data_storage_->saveEquipment(eq1));
    EXPECT_TRUE(data_storage_->saveEquipment(eq2));
    EXPECT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Get all equipment
    auto all_equipment = data_storage_->getAllEquipment();
    EXPECT_EQ(all_equipment.size(), 3);
    
    // Verify all equipment are retrieved
    std::vector<std::string> ids;
    for (const auto& eq : all_equipment) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("test-eq-1", "test-eq-2", "test-eq-3"));
}

// Test finding equipment by status
TEST_F(DataStorageTest, FindEquipmentByStatus) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different statuses
    Equipment eq1("test-eq-1", EquipmentType::Forklift, "Forklift 1");
    eq1.setStatus(EquipmentStatus::Active);
    
    Equipment eq2("test-eq-2", EquipmentType::Crane, "Crane 1");
    eq2.setStatus(EquipmentStatus::Maintenance);
    
    Equipment eq3("test-eq-3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setStatus(EquipmentStatus::Active);
    
    EXPECT_TRUE(data_storage_->saveEquipment(eq1));
    EXPECT_TRUE(data_storage_->saveEquipment(eq2));
    EXPECT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find active equipment
    auto active_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Active);
    EXPECT_EQ(active_equipment.size(), 2);
    
    // Find maintenance equipment
    auto maintenance_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(maintenance_equipment.size(), 1);
    EXPECT_EQ(maintenance_equipment[0].getId(), "test-eq-2");
}

// Test finding equipment by type
TEST_F(DataStorageTest, FindEquipmentByType) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different types
    Equipment eq1("test-eq-1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("test-eq-2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("test-eq-3", EquipmentType::Forklift, "Forklift 2");
    
    EXPECT_TRUE(data_storage_->saveEquipment(eq1));
    EXPECT_TRUE(data_storage_->saveEquipment(eq2));
    EXPECT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find forklifts
    auto forklifts = data_storage_->findEquipmentByType(EquipmentType::Forklift);
    EXPECT_EQ(forklifts.size(), 2);
    
    // Find cranes
    auto cranes = data_storage_->findEquipmentByType(EquipmentType::Crane);
    EXPECT_EQ(cranes.size(), 1);
    EXPECT_EQ(cranes[0].getId(), "test-eq-2");
}

// Test finding equipment in area
TEST_F(DataStorageTest, FindEquipmentInArea) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different positions
    Equipment eq1("test-eq-1", EquipmentType::Forklift, "Forklift 1");
    eq1.setLastPosition(Position(37.7749, -122.4194, 10.0, 5.0));
    
    Equipment eq2("test-eq-2", EquipmentType::Crane, "Crane 1");
    eq2.setLastPosition(Position(37.3382, -121.8863, 10.0, 5.0));
    
    Equipment eq3("test-eq-3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setLastPosition(Position(37.7750, -122.4195, 10.0, 5.0));
    
    EXPECT_TRUE(data_storage_->saveEquipment(eq1));
    EXPECT_TRUE(data_storage_->saveEquipment(eq2));
    EXPECT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find equipment in San Francisco area
    auto sf_equipment = data_storage_->findEquipmentInArea(37.7, -122.5, 37.8, -122.4);
    EXPECT_EQ(sf_equipment.size(), 2);
    
    // Find equipment in San Jose area
    auto sj_equipment = data_storage_->findEquipmentInArea(37.3, -122.0, 37.4, -121.8);
    EXPECT_EQ(sj_equipment.size(), 1);
    EXPECT_EQ(sj_equipment[0].getId(), "test-eq-2");
}

// Test error handling for non-existent equipment
TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Try to load non-existent equipment
    auto result = data_storage_->loadEquipment("non-existent-id");
    EXPECT_FALSE(result.has_value());
}

// Test error handling for invalid directory
TEST_F(DataStorageTest, InvalidDirectory) {
    // Create a data storage with an invalid path (contains characters not allowed in filenames)
    DataStorage invalid_storage("/invalid/path/with/illegal/characters/*/");
    
    // Initialization should fail or handle the error gracefully
    EXPECT_FALSE(invalid_storage.initialize());
}

// Test concurrent access
TEST_F(DataStorageTest, ConcurrentAccess) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create test equipment
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create a thread that repeatedly loads the equipment
    std::atomic<bool> stop_flag(false);
    std::atomic<int> load_count(0);
    
    std::thread reader_thread([&]() {
        while (!stop_flag) {
            auto loaded = data_storage_->loadEquipment(equipment.getId());
            if (loaded.has_value()) {
                load_count++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    // In the main thread, update the equipment multiple times
    for (int i = 0; i < 10; i++) {
        Equipment updated = equipment;
        updated.setName("Updated Forklift " + std::to_string(i));
        EXPECT_TRUE(data_storage_->updateEquipment(updated));
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    // Stop the reader thread
    stop_flag = true;
    reader_thread.join();
    
    // Verify that the reader thread was able to load the equipment multiple times
    EXPECT_GT(load_count, 0);
}

} // namespace equipment_tracker
// </test_code>