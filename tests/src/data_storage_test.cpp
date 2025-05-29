// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <ctime>
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
    Equipment original = createTestEquipment();
    
    // Save the equipment
    ASSERT_TRUE(data_storage_->saveEquipment(original));
    
    // Load the equipment
    auto loaded = data_storage_->loadEquipment(original.getId());
    
    // Verify the loaded equipment matches the original
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getId(), original.getId());
    EXPECT_EQ(loaded->getName(), original.getName());
    EXPECT_EQ(static_cast<int>(loaded->getType()), static_cast<int>(original.getType()));
    EXPECT_EQ(static_cast<int>(loaded->getStatus()), static_cast<int>(original.getStatus()));
    
    // Check position data
    auto original_pos = original.getLastPosition();
    auto loaded_pos = loaded->getLastPosition();
    
    ASSERT_TRUE(original_pos.has_value());
    ASSERT_TRUE(loaded_pos.has_value());
    
    EXPECT_NEAR(loaded_pos->getLatitude(), original_pos->getLatitude(), 0.0001);
    EXPECT_NEAR(loaded_pos->getLongitude(), original_pos->getLongitude(), 0.0001);
    EXPECT_NEAR(loaded_pos->getAltitude(), original_pos->getAltitude(), 0.0001);
    EXPECT_NEAR(loaded_pos->getAccuracy(), original_pos->getAccuracy(), 0.0001);
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    auto result = data_storage_->loadEquipment("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    // Create and save equipment
    Equipment original = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(original));
    
    // Modify the equipment
    Equipment updated = createTestEquipment();
    updated.setName("Updated Forklift");
    updated.setStatus(EquipmentStatus::Maintenance);
    
    // Update the equipment
    ASSERT_TRUE(data_storage_->updateEquipment(updated));
    
    // Load the equipment and verify changes
    auto loaded = data_storage_->loadEquipment(updated.getId());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getName(), "Updated Forklift");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::Maintenance);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    // Create and save equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Verify it exists
    ASSERT_TRUE(data_storage_->loadEquipment(equipment.getId()).has_value());
    
    // Delete the equipment
    ASSERT_TRUE(data_storage_->deleteEquipment(equipment.getId()));
    
    // Verify it no longer exists
    EXPECT_FALSE(data_storage_->loadEquipment(equipment.getId()).has_value());
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    // Create equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create positions with different timestamps
    auto now = std::chrono::system_clock::now();
    Position pos1(37.7749, -122.4194, 10.0, 2.0, now - std::chrono::hours(2));
    Position pos2(37.7750, -122.4195, 11.0, 2.1, now - std::chrono::hours(1));
    Position pos3(37.7751, -122.4196, 12.0, 2.2, now);
    
    // Save positions
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos1));
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos2));
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), pos3));
    
    // Get all position history
    auto history = data_storage_->getPositionHistory(equipment.getId());
    ASSERT_EQ(history.size(), 3);
    
    // Verify positions are in chronological order
    EXPECT_NEAR(history[0].getLatitude(), pos1.getLatitude(), 0.0001);
    EXPECT_NEAR(history[1].getLatitude(), pos2.getLatitude(), 0.0001);
    EXPECT_NEAR(history[2].getLatitude(), pos3.getLatitude(), 0.0001);
    
    // Test time range filtering
    auto filtered_history = data_storage_->getPositionHistory(
        equipment.getId(), 
        now - std::chrono::minutes(90), 
        now - std::chrono::minutes(30)
    );
    
    ASSERT_EQ(filtered_history.size(), 1);
    EXPECT_NEAR(filtered_history[0].getLatitude(), pos2.getLatitude(), 0.0001);
}

TEST_F(DataStorageTest, GetAllEquipment) {
    // Create and save multiple equipment
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Get all equipment
    auto all = data_storage_->getAllEquipment();
    ASSERT_EQ(all.size(), 3);
    
    // Verify all equipment are present (order may vary)
    std::vector<std::string> ids;
    for (const auto& eq : all) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("id1", "id2", "id3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    // Create equipment with different statuses
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    eq1.setStatus(EquipmentStatus::Active);
    
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    eq2.setStatus(EquipmentStatus::Maintenance);
    
    Equipment eq3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setStatus(EquipmentStatus::Active);
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find active equipment
    auto active = data_storage_->findEquipmentByStatus(EquipmentStatus::Active);
    ASSERT_EQ(active.size(), 2);
    
    // Find maintenance equipment
    auto maintenance = data_storage_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    ASSERT_EQ(maintenance.size(), 1);
    EXPECT_EQ(maintenance[0].getId(), "id2");
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    // Create equipment with different types
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("id3", EquipmentType::Forklift, "Forklift 2");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find forklifts
    auto forklifts = data_storage_->findEquipmentByType(EquipmentType::Forklift);
    ASSERT_EQ(forklifts.size(), 2);
    
    // Find cranes
    auto cranes = data_storage_->findEquipmentByType(EquipmentType::Crane);
    ASSERT_EQ(cranes.size(), 1);
    EXPECT_EQ(cranes[0].getId(), "id2");
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    // Create equipment with different positions
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    eq1.setLastPosition(Position(37.7749, -122.4194)); // San Francisco
    
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    eq2.setLastPosition(Position(34.0522, -118.2437)); // Los Angeles
    
    Equipment eq3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setLastPosition(Position(37.8715, -122.2730)); // Berkeley
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find equipment in Bay Area
    auto bay_area = data_storage_->findEquipmentInArea(37.0, -123.0, 38.0, -122.0);
    ASSERT_EQ(bay_area.size(), 2);
    
    // Find equipment in Southern California
    auto socal = data_storage_->findEquipmentInArea(33.0, -119.0, 35.0, -118.0);
    ASSERT_EQ(socal.size(), 1);
    EXPECT_EQ(socal[0].getId(), "id2");
}

TEST_F(DataStorageTest, HandleEmptyDatabase) {
    // Test operations on an empty database
    EXPECT_TRUE(data_storage_->getAllEquipment().empty());
    EXPECT_TRUE(data_storage_->findEquipmentByStatus(EquipmentStatus::Active).empty());
    EXPECT_TRUE(data_storage_->findEquipmentByType(EquipmentType::Forklift).empty());
    EXPECT_TRUE(data_storage_->findEquipmentInArea(0, 0, 1, 1).empty());
    EXPECT_TRUE(data_storage_->getPositionHistory("nonexistent").empty());
}

TEST_F(DataStorageTest, ConcurrentAccess) {
    // Test concurrent access to the data storage
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create threads that read and write concurrently
    std::thread writer([&]() {
        for (int i = 0; i < 10; i++) {
            Position pos(37.7749 + i * 0.001, -122.4194 + i * 0.001);
            data_storage_->savePosition(equipment.getId(), pos);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    std::thread reader([&]() {
        for (int i = 0; i < 10; i++) {
            data_storage_->loadEquipment(equipment.getId());
            data_storage_->getPositionHistory(equipment.getId());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    writer.join();
    reader.join();
    
    // Verify data is still accessible
    auto loaded = data_storage_->loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded.has_value());
    
    auto history = data_storage_->getPositionHistory(equipment.getId());
    EXPECT_GE(history.size(), 1);
}

TEST_F(DataStorageTest, EquipmentWithoutPosition) {
    // Create equipment without a position
    Equipment equipment("noloc", EquipmentType::Other, "No Location");
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Load and verify
    auto loaded = data_storage_->loadEquipment("noloc");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_FALSE(loaded->getLastPosition().has_value());
    
    // Should not be found in area searches
    auto in_area = data_storage_->findEquipmentInArea(0, 0, 90, 90);
    EXPECT_EQ(in_area.size(), 0);
}

TEST_F(DataStorageTest, DeleteNonExistentEquipment) {
    // Deleting non-existent equipment should still return true
    EXPECT_TRUE(data_storage_->deleteEquipment("nonexistent"));
}

TEST_F(DataStorageTest, SavePositionForNonExistentEquipment) {
    // Should be able to save positions even if equipment doesn't exist yet
    Position pos = createTestPosition();
    EXPECT_TRUE(data_storage_->savePosition("future_equipment", pos));
    
    // Position history should be retrievable
    auto history = data_storage_->getPositionHistory("future_equipment");
    ASSERT_EQ(history.size(), 1);
}
// </test_code>