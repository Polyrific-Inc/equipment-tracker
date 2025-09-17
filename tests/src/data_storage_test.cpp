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
        
        // Create the data storage with the test path
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

TEST_F(DataStorageTest, InitializeCreatesDirectories) {
    // Verify that initialize creates the necessary directories
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "positions"));
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    // Create test equipment
    Equipment equipment = createTestEquipment("equip1", EquipmentType::Forklift, "Test Forklift");
    equipment.setStatus(EquipmentStatus::Active);
    
    // Save the equipment
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Load the equipment
    auto loaded_equipment = data_storage_->loadEquipment("equip1");
    
    // Verify the loaded equipment matches the original
    ASSERT_TRUE(loaded_equipment.has_value());
    EXPECT_EQ(loaded_equipment->getId(), "equip1");
    EXPECT_EQ(loaded_equipment->getName(), "Test Forklift");
    EXPECT_EQ(loaded_equipment->getType(), EquipmentType::Forklift);
    EXPECT_EQ(loaded_equipment->getStatus(), EquipmentStatus::Active);
}

TEST_F(DataStorageTest, SaveAndLoadEquipmentWithPosition) {
    // Create test equipment with position
    Equipment equipment = createTestEquipment("equip2");
    Position position = createTestPosition(37.7749, -122.4194);
    equipment.setLastPosition(position);
    
    // Save the equipment
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Load the equipment
    auto loaded_equipment = data_storage_->loadEquipment("equip2");
    
    // Verify the loaded equipment has the correct position
    ASSERT_TRUE(loaded_equipment.has_value());
    auto loaded_position = loaded_equipment->getLastPosition();
    ASSERT_TRUE(loaded_position.has_value());
    EXPECT_NEAR(loaded_position->getLatitude(), 37.7749, 0.0001);
    EXPECT_NEAR(loaded_position->getLongitude(), -122.4194, 0.0001);
    EXPECT_NEAR(loaded_position->getAltitude(), 10.0, 0.0001);
    EXPECT_NEAR(loaded_position->getAccuracy(), 2.0, 0.0001);
}

TEST_F(DataStorageTest, UpdateEquipment) {
    // Create and save test equipment
    Equipment equipment = createTestEquipment("equip3");
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Update the equipment
    equipment.setName("Updated Name");
    equipment.setStatus(EquipmentStatus::Maintenance);
    EXPECT_TRUE(data_storage_->updateEquipment(equipment));
    
    // Load the equipment and verify updates
    auto loaded_equipment = data_storage_->loadEquipment("equip3");
    ASSERT_TRUE(loaded_equipment.has_value());
    EXPECT_EQ(loaded_equipment->getName(), "Updated Name");
    EXPECT_EQ(loaded_equipment->getStatus(), EquipmentStatus::Maintenance);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    // Create and save test equipment
    Equipment equipment = createTestEquipment("equip4");
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Verify equipment exists
    EXPECT_TRUE(data_storage_->loadEquipment("equip4").has_value());
    
    // Delete the equipment
    EXPECT_TRUE(data_storage_->deleteEquipment("equip4"));
    
    // Verify equipment no longer exists
    EXPECT_FALSE(data_storage_->loadEquipment("equip4").has_value());
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    // Create test equipment
    Equipment equipment = createTestEquipment("equip5");
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create test positions with different timestamps
    auto now = getCurrentTimestamp();
    auto one_hour_ago = now - std::chrono::hours(1);
    auto two_hours_ago = now - std::chrono::hours(2);
    
    Position pos1(40.7128, -74.0060, 10.0, 2.0, two_hours_ago);
    Position pos2(40.7129, -74.0061, 11.0, 2.1, one_hour_ago);
    Position pos3(40.7130, -74.0062, 12.0, 2.2, now);
    
    // Save positions
    EXPECT_TRUE(data_storage_->savePosition("equip5", pos1));
    EXPECT_TRUE(data_storage_->savePosition("equip5", pos2));
    EXPECT_TRUE(data_storage_->savePosition("equip5", pos3));
    
    // Get position history for the full time range
    auto history = data_storage_->getPositionHistory(
        "equip5", 
        two_hours_ago - std::chrono::minutes(1), 
        now + std::chrono::minutes(1)
    );
    
    // Verify all positions are retrieved
    ASSERT_EQ(history.size(), 3);
    
    // Positions should be sorted by timestamp
    EXPECT_NEAR(history[0].getLatitude(), 40.7128, 0.0001);
    EXPECT_NEAR(history[1].getLatitude(), 40.7129, 0.0001);
    EXPECT_NEAR(history[2].getLatitude(), 40.7130, 0.0001);
    
    // Get position history for a limited time range
    auto limited_history = data_storage_->getPositionHistory(
        "equip5", 
        one_hour_ago - std::chrono::minutes(1), 
        now + std::chrono::minutes(1)
    );
    
    // Verify only the positions within the time range are retrieved
    ASSERT_EQ(limited_history.size(), 2);
    EXPECT_NEAR(limited_history[0].getLatitude(), 40.7129, 0.0001);
    EXPECT_NEAR(limited_history[1].getLatitude(), 40.7130, 0.0001);
}

TEST_F(DataStorageTest, GetAllEquipment) {
    // Create and save multiple equipment
    Equipment equip1 = createTestEquipment("equip6", EquipmentType::Forklift, "Forklift 1");
    Equipment equip2 = createTestEquipment("equip7", EquipmentType::Crane, "Crane 1");
    Equipment equip3 = createTestEquipment("equip8", EquipmentType::Bulldozer, "Bulldozer 1");
    
    EXPECT_TRUE(data_storage_->saveEquipment(equip1));
    EXPECT_TRUE(data_storage_->saveEquipment(equip2));
    EXPECT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Get all equipment
    auto all_equipment = data_storage_->getAllEquipment();
    
    // Verify all equipment are retrieved
    ASSERT_EQ(all_equipment.size(), 3);
    
    // Verify equipment IDs (order may vary)
    std::vector<std::string> ids;
    for (const auto& equip : all_equipment) {
        ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("equip6", "equip7", "equip8"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    // Create and save equipment with different statuses
    Equipment equip1 = createTestEquipment("equip9", EquipmentType::Forklift, "Forklift 2");
    equip1.setStatus(EquipmentStatus::Active);
    
    Equipment equip2 = createTestEquipment("equip10", EquipmentType::Crane, "Crane 2");
    equip2.setStatus(EquipmentStatus::Maintenance);
    
    Equipment equip3 = createTestEquipment("equip11", EquipmentType::Bulldozer, "Bulldozer 2");
    equip3.setStatus(EquipmentStatus::Active);
    
    EXPECT_TRUE(data_storage_->saveEquipment(equip1));
    EXPECT_TRUE(data_storage_->saveEquipment(equip2));
    EXPECT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Find equipment by status
    auto active_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Active);
    auto maintenance_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    auto inactive_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Inactive);
    
    // Verify results
    ASSERT_EQ(active_equipment.size(), 2);
    ASSERT_EQ(maintenance_equipment.size(), 1);
    ASSERT_EQ(inactive_equipment.size(), 0);
    
    // Verify equipment IDs for active equipment (order may vary)
    std::vector<std::string> active_ids;
    for (const auto& equip : active_equipment) {
        active_ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(active_ids, ::testing::UnorderedElementsAre("equip9", "equip11"));
    
    // Verify equipment ID for maintenance equipment
    EXPECT_EQ(maintenance_equipment[0].getId(), "equip10");
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    // Create and save equipment with different types
    Equipment equip1 = createTestEquipment("equip12", EquipmentType::Forklift, "Forklift 3");
    Equipment equip2 = createTestEquipment("equip13", EquipmentType::Crane, "Crane 3");
    Equipment equip3 = createTestEquipment("equip14", EquipmentType::Forklift, "Forklift 4");
    
    EXPECT_TRUE(data_storage_->saveEquipment(equip1));
    EXPECT_TRUE(data_storage_->saveEquipment(equip2));
    EXPECT_TRUE(data_storage_->saveEquipment(equip3));
    
    // Find equipment by type
    auto forklifts = data_storage_->findEquipmentByType(EquipmentType::Forklift);
    auto cranes = data_storage_->findEquipmentByType(EquipmentType::Crane);
    auto bulldozers = data_storage_->findEquipmentByType(EquipmentType::Bulldozer);
    
    // Verify results
    ASSERT_EQ(forklifts.size(), 2);
    ASSERT_EQ(cranes.size(), 1);
    ASSERT_EQ(bulldozers.size(), 0);
    
    // Verify equipment IDs for forklifts (order may vary)
    std::vector<std::string> forklift_ids;
    for (const auto& equip : forklifts) {
        forklift_ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(forklift_ids, ::testing::UnorderedElementsAre("equip12", "equip14"));
    
    // Verify equipment ID for cranes
    EXPECT_EQ(cranes[0].getId(), "equip13");
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    // Create equipment with positions in different areas
    Equipment equip1 = createTestEquipment("equip15");
    Equipment equip2 = createTestEquipment("equip16");
    Equipment equip3 = createTestEquipment("equip17");
    Equipment equip4 = createTestEquipment("equip18"); // No position
    
    // Set positions
    equip1.setLastPosition(Position(37.7749, -122.4194)); // San Francisco
    equip2.setLastPosition(Position(34.0522, -118.2437)); // Los Angeles
    equip3.setLastPosition(Position(40.7128, -74.0060));  // New York
    
    EXPECT_TRUE(data_storage_->saveEquipment(equip1));
    EXPECT_TRUE(data_storage_->saveEquipment(equip2));
    EXPECT_TRUE(data_storage_->saveEquipment(equip3));
    EXPECT_TRUE(data_storage_->saveEquipment(equip4));
    
    // Find equipment in West Coast area (covers SF and LA)
    auto west_coast = data_storage_->findEquipmentInArea(
        33.0, -125.0,  // Southwest corner
        38.0, -118.0   // Northeast corner
    );
    
    // Find equipment in East Coast area (covers NY)
    auto east_coast = data_storage_->findEquipmentInArea(
        39.0, -75.0,   // Southwest corner
        41.0, -73.0    // Northeast corner
    );
    
    // Find equipment in central US (should be empty)
    auto central = data_storage_->findEquipmentInArea(
        35.0, -100.0,  // Southwest corner
        40.0, -90.0    // Northeast corner
    );
    
    // Verify results
    ASSERT_EQ(west_coast.size(), 2);
    ASSERT_EQ(east_coast.size(), 1);
    ASSERT_EQ(central.size(), 0);
    
    // Verify equipment IDs for west coast (order may vary)
    std::vector<std::string> west_coast_ids;
    for (const auto& equip : west_coast) {
        west_coast_ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(west_coast_ids, ::testing::UnorderedElementsAre("equip15", "equip16"));
    
    // Verify equipment ID for east coast
    EXPECT_EQ(east_coast[0].getId(), "equip17");
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    // Try to load equipment that doesn't exist
    auto equipment = data_storage_->loadEquipment("non_existent_id");
    
    // Verify that no equipment is returned
    EXPECT_FALSE(equipment.has_value());
}

TEST_F(DataStorageTest, GetPositionHistoryForNonExistentEquipment) {
    // Try to get position history for equipment that doesn't exist
    auto history = data_storage_->getPositionHistory("non_existent_id");
    
    // Verify that an empty vector is returned
    EXPECT_TRUE(history.empty());
}

TEST_F(DataStorageTest, DeleteNonExistentEquipment) {
    // Try to delete equipment that doesn't exist
    // This should succeed (or at least not fail) since the end result is the same
    EXPECT_TRUE(data_storage_->deleteEquipment("non_existent_id"));
}

TEST_F(DataStorageTest, InitializeMultipleTimes) {
    // Initialize should be idempotent
    EXPECT_TRUE(data_storage_->initialize());
    EXPECT_TRUE(data_storage_->initialize());
    EXPECT_TRUE(data_storage_->initialize());
}

TEST_F(DataStorageTest, ConcurrentOperations) {
    // Create test equipment
    Equipment equipment = createTestEquipment("equip_concurrent");
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Run concurrent operations
    std::vector<std::thread> threads;
    
    // Thread 1: Update equipment
    threads.emplace_back([this]() {
        Equipment equip = createTestEquipment("equip_concurrent");
        equip.setName("Updated by thread 1");
        EXPECT_TRUE(data_storage_->updateEquipment(equip));
    });
    
    // Thread 2: Load equipment
    threads.emplace_back([this]() {
        auto loaded = data_storage_->loadEquipment("equip_concurrent");
        EXPECT_TRUE(loaded.has_value());
    });
    
    // Thread 3: Save position
    threads.emplace_back([this]() {
        Position pos = createTestPosition(42.0, -71.0);
        EXPECT_TRUE(data_storage_->savePosition("equip_concurrent", pos));
    });
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify the equipment was updated
    auto final_equipment = data_storage_->loadEquipment("equip_concurrent");
    ASSERT_TRUE(final_equipment.has_value());
    EXPECT_EQ(final_equipment->getName(), "Updated by thread 1");
    
    // Verify position was saved
    auto history = data_storage_->getPositionHistory("equip_concurrent");
    EXPECT_FALSE(history.empty());
}

} // namespace
} // namespace equipment_tracker
// </test_code>