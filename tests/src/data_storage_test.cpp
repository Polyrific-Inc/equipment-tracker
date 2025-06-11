// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include "equipment_tracker/data_storage.h"
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/types.h"
#include "equipment_tracker/utils/constants.h"

namespace equipment_tracker {
namespace {

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
        
        // Create the data storage with test path
        data_storage_ = std::make_unique<DataStorage>(test_db_path_.string());
        data_storage_->initialize();
    }

    void TearDown() override {
        // Clean up the test directory
        data_storage_.reset();
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove_all(test_db_path_);
        }
    }

    // Helper method to create a test equipment
    Equipment createTestEquipment(const std::string& id = "test_equipment") {
        Equipment equipment(id, EquipmentType::Forklift, "Test Forklift");
        equipment.setStatus(EquipmentStatus::Active);
        return equipment;
    }

    // Helper method to create a test position
    Position createTestPosition(double lat = 40.7128, double lon = -74.0060) {
        return Position(lat, lon, 10.0, 5.0, getCurrentTimestamp());
    }

    std::filesystem::path test_db_path_;
    std::unique_ptr<DataStorage> data_storage_;
};

TEST_F(DataStorageTest, InitializeCreatesDirectories) {
    // Verify that initialize creates the necessary directories
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "positions"));
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    // Create test equipment
    Equipment original = createTestEquipment();
    
    // Save equipment
    EXPECT_TRUE(data_storage_->saveEquipment(original));
    
    // Load equipment
    auto loaded = data_storage_->loadEquipment(original.getId());
    
    // Verify loaded equipment matches original
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getId(), original.getId());
    EXPECT_EQ(loaded->getName(), original.getName());
    EXPECT_EQ(static_cast<int>(loaded->getType()), static_cast<int>(original.getType()));
    EXPECT_EQ(static_cast<int>(loaded->getStatus()), static_cast<int>(original.getStatus()));
}

TEST_F(DataStorageTest, SaveAndLoadEquipmentWithPosition) {
    // Create test equipment with position
    Equipment original = createTestEquipment();
    Position position = createTestPosition();
    original.setLastPosition(position);
    
    // Save equipment
    EXPECT_TRUE(data_storage_->saveEquipment(original));
    
    // Load equipment
    auto loaded = data_storage_->loadEquipment(original.getId());
    
    // Verify loaded equipment has position
    ASSERT_TRUE(loaded.has_value());
    ASSERT_TRUE(loaded->getLastPosition().has_value());
    
    // Compare position values
    auto loaded_pos = loaded->getLastPosition().value();
    EXPECT_DOUBLE_EQ(loaded_pos.getLatitude(), position.getLatitude());
    EXPECT_DOUBLE_EQ(loaded_pos.getLongitude(), position.getLongitude());
    EXPECT_DOUBLE_EQ(loaded_pos.getAltitude(), position.getAltitude());
    EXPECT_DOUBLE_EQ(loaded_pos.getAccuracy(), position.getAccuracy());
    
    // Timestamps might not be exactly equal due to serialization/deserialization
    // So we check if they're close enough (within 1 second)
    auto original_time = std::chrono::system_clock::to_time_t(position.getTimestamp());
    auto loaded_time = std::chrono::system_clock::to_time_t(loaded_pos.getTimestamp());
    EXPECT_NEAR(loaded_time, original_time, 1);
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    // Try to load non-existent equipment
    auto result = data_storage_->loadEquipment("non_existent_id");
    
    // Verify result is empty
    EXPECT_FALSE(result.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    // Create and save test equipment
    Equipment original = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(original));
    
    // Update equipment
    Equipment updated = original;
    updated.setName("Updated Name");
    updated.setStatus(EquipmentStatus::Maintenance);
    
    // Save updated equipment
    EXPECT_TRUE(data_storage_->updateEquipment(updated));
    
    // Load equipment
    auto loaded = data_storage_->loadEquipment(original.getId());
    
    // Verify loaded equipment has updated values
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getName(), "Updated Name");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::Maintenance);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    // Create and save test equipment
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Verify equipment exists
    EXPECT_TRUE(data_storage_->loadEquipment(equipment.getId()).has_value());
    
    // Delete equipment
    EXPECT_TRUE(data_storage_->deleteEquipment(equipment.getId()));
    
    // Verify equipment no longer exists
    EXPECT_FALSE(data_storage_->loadEquipment(equipment.getId()).has_value());
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    // Create test equipment
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create test positions
    Position pos1(40.7128, -74.0060, 10.0, 5.0, getCurrentTimestamp());
    
    // Wait a bit to ensure different timestamps
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    Position pos2(40.7129, -74.0061, 11.0, 4.0, getCurrentTimestamp());
    
    // Wait a bit to ensure different timestamps
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    Position pos3(40.7130, -74.0062, 12.0, 3.0, getCurrentTimestamp());
    
    // Save positions
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos1));
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos2));
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos3));
    
    // Get position history
    auto history = data_storage_->getPositionHistory(equipment.getId());
    
    // Verify history contains all positions
    ASSERT_EQ(history.size(), 3);
    
    // Positions should be sorted by timestamp (oldest first)
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), pos1.getLatitude());
    EXPECT_DOUBLE_EQ(history[1].getLatitude(), pos2.getLatitude());
    EXPECT_DOUBLE_EQ(history[2].getLatitude(), pos3.getLatitude());
}

TEST_F(DataStorageTest, GetPositionHistoryWithTimeRange) {
    // Create test equipment
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create positions with specific timestamps
    auto now = getCurrentTimestamp();
    auto time1 = now - std::chrono::hours(3);
    auto time2 = now - std::chrono::hours(2);
    auto time3 = now - std::chrono::hours(1);
    
    Position pos1(40.7128, -74.0060, 10.0, 5.0, time1);
    Position pos2(40.7129, -74.0061, 11.0, 4.0, time2);
    Position pos3(40.7130, -74.0062, 12.0, 3.0, time3);
    
    // Save positions
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos1));
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos2));
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos3));
    
    // Get position history with time range (only middle position)
    auto start_time = time1 + std::chrono::minutes(30);
    auto end_time = time2 + std::chrono::minutes(30);
    auto history = data_storage_->getPositionHistory(equipment.getId(), start_time, end_time);
    
    // Verify history contains only the middle position
    ASSERT_EQ(history.size(), 1);
    EXPECT_DOUBLE_EQ(history[0].getLatitude(), pos2.getLatitude());
}

TEST_F(DataStorageTest, GetAllEquipment) {
    // Create and save multiple equipment
    Equipment equip1("id1", EquipmentType::Forklift, "Forklift 1");
    Equipment equip2("id2", EquipmentType::Crane, "Crane 1");
    Equipment equip3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    
    EXPECT_TRUE(data_storage_->saveEquipment(equip1));
    EXPECT_TRUE(data_storage_->saveEquipment(equip2));
    EXPECT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Get all equipment
    auto all_equipment = data_storage_->getAllEquipment();
    
    // Verify all equipment are returned
    ASSERT_EQ(all_equipment.size(), 3);
    
    // Check if all IDs are present (order may vary)
    std::vector<std::string> ids;
    for (const auto& equip : all_equipment) {
        ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("id1", "id2", "id3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    // Create equipment with different statuses
    Equipment equip1("id1", EquipmentType::Forklift, "Forklift 1");
    equip1.setStatus(EquipmentStatus::Active);
    
    Equipment equip2("id2", EquipmentType::Crane, "Crane 1");
    equip2.setStatus(EquipmentStatus::Maintenance);
    
    Equipment equip3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    equip3.setStatus(EquipmentStatus::Active);
    
    // Save equipment
    EXPECT_TRUE(data_storage_->saveEquipment(equip1));
    EXPECT_TRUE(data_storage_->saveEquipment(equip2));
    EXPECT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Find equipment by status
    auto active_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Active);
    
    // Verify correct equipment are returned
    ASSERT_EQ(active_equipment.size(), 2);
    
    // Check if correct IDs are present (order may vary)
    std::vector<std::string> ids;
    for (const auto& equip : active_equipment) {
        ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("id1", "id3"));
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    // Create equipment with different types
    Equipment equip1("id1", EquipmentType::Forklift, "Forklift 1");
    Equipment equip2("id2", EquipmentType::Crane, "Crane 1");
    Equipment equip3("id3", EquipmentType::Forklift, "Forklift 2");
    
    // Save equipment
    EXPECT_TRUE(data_storage_->saveEquipment(equip1));
    EXPECT_TRUE(data_storage_->saveEquipment(equip2));
    EXPECT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Find equipment by type
    auto forklifts = data_storage_->findEquipmentByType(EquipmentType::Forklift);
    
    // Verify correct equipment are returned
    ASSERT_EQ(forklifts.size(), 2);
    
    // Check if correct IDs are present (order may vary)
    std::vector<std::string> ids;
    for (const auto& equip : forklifts) {
        ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("id1", "id3"));
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    // Create equipment with different positions
    Equipment equip1("id1", EquipmentType::Forklift, "Forklift 1");
    equip1.setLastPosition(Position(40.7128, -74.0060)); // NYC
    
    Equipment equip2("id2", EquipmentType::Crane, "Crane 1");
    equip2.setLastPosition(Position(34.0522, -118.2437)); // LA
    
    Equipment equip3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    equip3.setLastPosition(Position(41.8781, -87.6298)); // Chicago
    
    // Save equipment
    EXPECT_TRUE(data_storage_->saveEquipment(equip1));
    EXPECT_TRUE(data_storage_->saveEquipment(equip2));
    EXPECT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Define area around NYC
    double lat1 = 40.0;
    double lon1 = -75.0;
    double lat2 = 41.0;
    double lon2 = -73.0;
    
    // Find equipment in area
    auto equipment_in_area = data_storage_->findEquipmentInArea(lat1, lon1, lat2, lon2);
    
    // Verify only NYC equipment is returned
    ASSERT_EQ(equipment_in_area.size(), 1);
    EXPECT_EQ(equipment_in_area[0].getId(), "id1");
}

TEST_F(DataStorageTest, InitializeMultipleTimes) {
    // Initialize should be idempotent
    EXPECT_TRUE(data_storage_->initialize());
    EXPECT_TRUE(data_storage_->initialize());
    EXPECT_TRUE(data_storage_->initialize());
    
    // Should still be able to save and load equipment
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    EXPECT_TRUE(data_storage_->loadEquipment(equipment.getId()).has_value());
}

TEST_F(DataStorageTest, GetPositionHistoryForNonExistentEquipment) {
    // Get position history for non-existent equipment
    auto history = data_storage_->getPositionHistory("non_existent_id");
    
    // Verify result is empty
    EXPECT_TRUE(history.empty());
}

TEST_F(DataStorageTest, DeleteNonExistentEquipment) {
    // Delete non-existent equipment should still return true
    EXPECT_TRUE(data_storage_->deleteEquipment("non_existent_id"));
}

} // namespace
} // namespace equipment_tracker
// </test_code>