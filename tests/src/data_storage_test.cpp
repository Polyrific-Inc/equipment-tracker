// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include "equipment_tracker/data_storage.h"

// Platform-specific time handling
#ifdef _WIN32
#define timegm _mkgmtime
#endif

using namespace equipment_tracker;
using ::testing::HasSubstr;

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
    Equipment createTestEquipment(const std::string& id = "test123") {
        Equipment equipment(id, EquipmentType::Forklift, "Test Forklift");
        equipment.setStatus(EquipmentStatus::Active);
        
        // Add a position
        Position pos(37.7749, -122.4194, 10.0, 2.0);
        equipment.setLastPosition(pos);
        
        return equipment;
    }

    // Helper method to create a test position
    Position createTestPosition(double lat = 37.7749, double lon = -122.4194, 
                               double alt = 10.0, double acc = 2.0) {
        return Position(lat, lon, alt, acc);
    }

    std::filesystem::path test_db_path_;
    std::unique_ptr<DataStorage> data_storage_;
};

TEST_F(DataStorageTest, InitializeCreatesDirectories) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Check that the directories were created
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "positions"));
}

TEST_F(DataStorageTest, InitializeIsIdempotent) {
    ASSERT_TRUE(data_storage_->initialize());
    ASSERT_TRUE(data_storage_->initialize()); // Should still return true
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
    EXPECT_EQ(static_cast<int>(loaded->getType()), static_cast<int>(original.getType()));
    EXPECT_EQ(static_cast<int>(loaded->getStatus()), static_cast<int>(original.getStatus()));
    
    // Verify position
    auto original_pos = original.getLastPosition();
    auto loaded_pos = loaded->getLastPosition();
    ASSERT_TRUE(loaded_pos.has_value());
    ASSERT_TRUE(original_pos.has_value());
    
    EXPECT_NEAR(loaded_pos->getLatitude(), original_pos->getLatitude(), 0.0001);
    EXPECT_NEAR(loaded_pos->getLongitude(), original_pos->getLongitude(), 0.0001);
    EXPECT_NEAR(loaded_pos->getAltitude(), original_pos->getAltitude(), 0.0001);
    EXPECT_NEAR(loaded_pos->getAccuracy(), original_pos->getAccuracy(), 0.0001);
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
    Equipment updated = createTestEquipment();
    updated.setName("Updated Forklift");
    updated.setStatus(EquipmentStatus::Maintenance);
    
    ASSERT_TRUE(data_storage_->updateEquipment(updated));
    
    // Load the equipment
    auto loaded = data_storage_->loadEquipment(updated.getId());
    ASSERT_TRUE(loaded.has_value());
    
    // Verify the loaded equipment has the updated values
    EXPECT_EQ(loaded->getName(), "Updated Forklift");
    EXPECT_EQ(static_cast<int>(loaded->getStatus()), static_cast<int>(EquipmentStatus::Maintenance));
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
    
    // Create positions with different timestamps
    auto now = std::chrono::system_clock::now();
    auto one_hour_ago = now - std::chrono::hours(1);
    auto two_hours_ago = now - std::chrono::hours(2);
    
    Position pos1(37.7749, -122.4194, 10.0, 2.0, now);
    Position pos2(37.7750, -122.4195, 11.0, 2.1, one_hour_ago);
    Position pos3(37.7751, -122.4196, 12.0, 2.2, two_hours_ago);
    
    // Save positions
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos1));
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos2));
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos3));
    
    // Get all position history
    auto history = data_storage_->getPositionHistory(equipment.getId(), two_hours_ago, now);
    EXPECT_EQ(history.size(), 3);
    
    // Get partial history
    auto recent_history = data_storage_->getPositionHistory(equipment.getId(), one_hour_ago, now);
    EXPECT_EQ(recent_history.size(), 2);
    
    // Verify the positions are sorted by timestamp
    if (history.size() >= 3) {
        // The positions should be sorted from oldest to newest
        EXPECT_LE(history[0].getTimestamp(), history[1].getTimestamp());
        EXPECT_LE(history[1].getTimestamp(), history[2].getTimestamp());
        
        // Verify position data
        EXPECT_NEAR(history[0].getLatitude(), 37.7751, 0.0001);
        EXPECT_NEAR(history[1].getLatitude(), 37.7750, 0.0001);
        EXPECT_NEAR(history[2].getLatitude(), 37.7749, 0.0001);
    }
}

TEST_F(DataStorageTest, GetAllEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save multiple equipment
    Equipment eq1("eq1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("eq2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("eq3", EquipmentType::Bulldozer, "Bulldozer 1");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Get all equipment
    auto all_equipment = data_storage_->getAllEquipment();
    EXPECT_EQ(all_equipment.size(), 3);
    
    // Verify the equipment IDs
    std::vector<std::string> ids;
    for (const auto& eq : all_equipment) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("eq1", "eq2", "eq3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save equipment with different statuses
    Equipment eq1("eq1", EquipmentType::Forklift, "Forklift 1");
    eq1.setStatus(EquipmentStatus::Active);
    
    Equipment eq2("eq2", EquipmentType::Crane, "Crane 1");
    eq2.setStatus(EquipmentStatus::Maintenance);
    
    Equipment eq3("eq3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setStatus(EquipmentStatus::Active);
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find active equipment
    auto active_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Active);
    EXPECT_EQ(active_equipment.size(), 2);
    
    // Find maintenance equipment
    auto maintenance_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(maintenance_equipment.size(), 1);
    
    // Find inactive equipment (should be none)
    auto inactive_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Inactive);
    EXPECT_EQ(inactive_equipment.size(), 0);
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save equipment with different types
    Equipment eq1("eq1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("eq2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("eq3", EquipmentType::Forklift, "Forklift 2");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find forklifts
    auto forklifts = data_storage_->findEquipmentByType(EquipmentType::Forklift);
    EXPECT_EQ(forklifts.size(), 2);
    
    // Find cranes
    auto cranes = data_storage_->findEquipmentByType(EquipmentType::Crane);
    EXPECT_EQ(cranes.size(), 1);
    
    // Find bulldozers (should be none)
    auto bulldozers = data_storage_->findEquipmentByType(EquipmentType::Bulldozer);
    EXPECT_EQ(bulldozers.size(), 0);
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save equipment with different positions
    Equipment eq1("eq1", EquipmentType::Forklift, "Forklift 1");
    eq1.setLastPosition(Position(37.7749, -122.4194)); // San Francisco
    
    Equipment eq2("eq2", EquipmentType::Crane, "Crane 1");
    eq2.setLastPosition(Position(34.0522, -118.2437)); // Los Angeles
    
    Equipment eq3("eq3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setLastPosition(Position(40.7128, -74.0060)); // New York
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find equipment in West Coast area
    auto west_coast = data_storage_->findEquipmentInArea(33.0, -125.0, 38.0, -118.0);
    EXPECT_EQ(west_coast.size(), 2);
    
    // Find equipment in East Coast area
    auto east_coast = data_storage_->findEquipmentInArea(39.0, -75.0, 41.0, -73.0);
    EXPECT_EQ(east_coast.size(), 1);
    
    // Find equipment in a small area around San Francisco
    auto sf_area = data_storage_->findEquipmentInArea(37.7, -122.5, 37.8, -122.4);
    EXPECT_EQ(sf_area.size(), 1);
    
    // Find equipment in an area with no equipment
    auto no_equipment_area = data_storage_->findEquipmentInArea(45.0, -90.0, 46.0, -89.0);
    EXPECT_EQ(no_equipment_area.size(), 0);
}

TEST_F(DataStorageTest, SaveEquipmentWithoutInitialization) {
    // Don't call initialize() explicitly
    Equipment equipment = createTestEquipment();
    
    // Should initialize automatically when saving
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Verify the equipment was saved
    auto loaded = data_storage_->loadEquipment(equipment.getId());
    EXPECT_TRUE(loaded.has_value());
}

TEST_F(DataStorageTest, SavePositionWithoutInitialization) {
    // Don't call initialize() explicitly
    std::string equipment_id = "test123";
    Position position = createTestPosition();
    
    // Should initialize automatically when saving
    EXPECT_TRUE(data_storage_->savePosition(equipment_id, position));
    
    // Verify the position was saved
    auto history = data_storage_->getPositionHistory(equipment_id);
    EXPECT_EQ(history.size(), 1);
}

TEST_F(DataStorageTest, GetPositionHistoryEmptyResult) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Get history for non-existent equipment
    auto history = data_storage_->getPositionHistory("nonexistent");
    EXPECT_TRUE(history.empty());
}

TEST_F(DataStorageTest, GetPositionHistoryWithTimeRange) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create positions with different timestamps
    auto now = std::chrono::system_clock::now();
    auto one_day_ago = now - std::chrono::hours(24);
    auto two_days_ago = now - std::chrono::hours(48);
    auto three_days_ago = now - std::chrono::hours(72);
    
    Position pos1(37.7749, -122.4194, 10.0, 2.0, now);
    Position pos2(37.7750, -122.4195, 11.0, 2.1, one_day_ago);
    Position pos3(37.7751, -122.4196, 12.0, 2.2, two_days_ago);
    Position pos4(37.7752, -122.4197, 13.0, 2.3, three_days_ago);
    
    // Save positions
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos1));
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos2));
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos3));
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos4));
    
    // Get history for a specific time range (between one and two days ago)
    auto history = data_storage_->getPositionHistory(
        equipment.getId(), two_days_ago, one_day_ago);
    
    EXPECT_EQ(history.size(), 2);
    
    // Verify the positions are within the time range
    for (const auto& pos : history) {
        auto timestamp = pos.getTimestamp();
        EXPECT_LE(two_days_ago, timestamp);
        EXPECT_LE(timestamp, one_day_ago);
    }
}

TEST_F(DataStorageTest, DeleteNonExistentEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Delete non-existent equipment should still return true
    EXPECT_TRUE(data_storage_->deleteEquipment("nonexistent"));
}
// </test_code>