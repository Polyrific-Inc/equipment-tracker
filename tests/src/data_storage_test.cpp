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
#define MKGMTIME _mkgmtime
#else
#define MKGMTIME timegm
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
        Equipment equipment(id, EquipmentType::Drone, "Test Drone");
        equipment.setStatus(EquipmentStatus::Available);
        
        // Add a position
        auto now = std::chrono::system_clock::now();
        Position position(37.7749, -122.4194, 10.0, 5.0, now);
        equipment.setLastPosition(position);
        
        return equipment;
    }

    // Helper method to create a test position
    Position createTestPosition(double lat = 37.7749, double lon = -122.4194, 
                               double alt = 10.0, double acc = 5.0) {
        auto now = std::chrono::system_clock::now();
        return Position(lat, lon, alt, acc, now);
    }
};

TEST_F(DataStorageTest, InitializeCreatesDirectories) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Check that the directories were created
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "positions"));
}

TEST_F(DataStorageTest, InitializeIdempotent) {
    ASSERT_TRUE(data_storage_->initialize());
    ASSERT_TRUE(data_storage_->initialize()); // Second call should also succeed
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
    updated.setName("Updated Drone");
    updated.setStatus(EquipmentStatus::InUse);
    ASSERT_TRUE(data_storage_->updateEquipment(updated));
    
    // Load the equipment
    auto loaded = data_storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    // Verify the loaded equipment matches the updated version
    EXPECT_EQ(loaded->getName(), "Updated Drone");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::InUse);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Save a position for the equipment
    Position position = createTestPosition();
    ASSERT_TRUE(data_storage_->savePosition(equipment.getId(), position));
    
    // Delete the equipment
    ASSERT_TRUE(data_storage_->deleteEquipment(equipment.getId()));
    
    // Verify the equipment is gone
    auto loaded = data_storage_->loadEquipment(equipment.getId());
    EXPECT_FALSE(loaded.has_value());
    
    // Verify the position directory is gone
    std::string position_dir = test_db_path_.string() + "/positions/" + equipment.getId();
    EXPECT_FALSE(std::filesystem::exists(position_dir));
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create positions at different times
    auto now = std::chrono::system_clock::now();
    auto one_hour_ago = now - std::chrono::hours(1);
    auto two_hours_ago = now - std::chrono::hours(2);
    
    Position pos1(37.7749, -122.4194, 10.0, 5.0, two_hours_ago);
    Position pos2(37.7750, -122.4195, 11.0, 4.0, one_hour_ago);
    Position pos3(37.7751, -122.4196, 12.0, 3.0, now);
    
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
    
    // Get position history with time range
    auto filtered_history = data_storage_->getPositionHistory(
        equipment.getId(), one_hour_ago - std::chrono::minutes(5), now + std::chrono::minutes(5));
    ASSERT_EQ(filtered_history.size(), 2);
    EXPECT_NEAR(filtered_history[0].getLatitude(), pos2.getLatitude(), 0.0001);
    EXPECT_NEAR(filtered_history[1].getLatitude(), pos3.getLatitude(), 0.0001);
}

TEST_F(DataStorageTest, GetAllEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save multiple equipment
    Equipment eq1 = createTestEquipment("eq1");
    Equipment eq2 = createTestEquipment("eq2");
    Equipment eq3 = createTestEquipment("eq3");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Get all equipment
    auto all = data_storage_->getAllEquipment();
    ASSERT_EQ(all.size(), 3);
    
    // Verify all equipment are present
    std::vector<std::string> ids;
    for (const auto& eq : all) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("eq1", "eq2", "eq3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different statuses
    Equipment eq1("eq1", EquipmentType::Drone, "Drone 1");
    eq1.setStatus(EquipmentStatus::Available);
    
    Equipment eq2("eq2", EquipmentType::Drone, "Drone 2");
    eq2.setStatus(EquipmentStatus::InUse);
    
    Equipment eq3("eq3", EquipmentType::Drone, "Drone 3");
    eq3.setStatus(EquipmentStatus::Available);
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find equipment by status
    auto available = data_storage_->findEquipmentByStatus(EquipmentStatus::Available);
    ASSERT_EQ(available.size(), 2);
    
    std::vector<std::string> ids;
    for (const auto& eq : available) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("eq1", "eq3"));
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different types
    Equipment eq1("eq1", EquipmentType::Drone, "Drone 1");
    Equipment eq2("eq2", EquipmentType::Vehicle, "Vehicle 1");
    Equipment eq3("eq3", EquipmentType::Drone, "Drone 2");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find equipment by type
    auto drones = data_storage_->findEquipmentByType(EquipmentType::Drone);
    ASSERT_EQ(drones.size(), 2);
    
    std::vector<std::string> ids;
    for (const auto& eq : drones) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("eq1", "eq3"));
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment with different positions
    Equipment eq1("eq1", EquipmentType::Drone, "Drone 1");
    eq1.setLastPosition(Position(37.7749, -122.4194, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment eq2("eq2", EquipmentType::Drone, "Drone 2");
    eq2.setLastPosition(Position(37.3382, -121.8863, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment eq3("eq3", EquipmentType::Drone, "Drone 3");
    eq3.setLastPosition(Position(37.7749, -122.4195, 10.0, 5.0, std::chrono::system_clock::now()));
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find equipment in area
    auto inArea = data_storage_->findEquipmentInArea(37.7740, -122.4200, 37.7760, -122.4190);
    ASSERT_EQ(inArea.size(), 2);
    
    std::vector<std::string> ids;
    for (const auto& eq : inArea) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("eq1", "eq3"));
}

TEST_F(DataStorageTest, ExecuteQuery) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // This is just testing the placeholder implementation
    testing::internal::CaptureStdout();
    bool result = data_storage_->executeQuery("SELECT * FROM equipment");
    std::string output = testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(result);
    EXPECT_THAT(output, ::testing::HasSubstr("Query: SELECT * FROM equipment"));
}

TEST_F(DataStorageTest, SaveEquipmentWithoutInitialize) {
    // Don't call initialize() explicitly
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Verify the equipment was saved
    auto loaded = data_storage_->loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getId(), equipment.getId());
}

TEST_F(DataStorageTest, SavePositionWithoutInitialize) {
    // Don't call initialize() explicitly
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    Position position = createTestPosition();
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), position));
    
    // Verify the position was saved
    auto history = data_storage_->getPositionHistory(equipment.getId());
    ASSERT_EQ(history.size(), 1);
    EXPECT_NEAR(history[0].getLatitude(), position.getLatitude(), 0.0001);
}

TEST_F(DataStorageTest, EquipmentWithoutPosition) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create equipment without position
    Equipment equipment("eq1", EquipmentType::Drone, "Drone without position");
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Load the equipment
    auto loaded = data_storage_->loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded.has_value());
    
    // Verify no position is present
    EXPECT_FALSE(loaded->getLastPosition().has_value());
}

TEST_F(DataStorageTest, ConcurrentAccess) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create multiple threads that save and load equipment
    std::vector<std::thread> threads;
    const int num_threads = 10;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i]() {
            std::string id = "eq" + std::to_string(i);
            Equipment equipment(id, EquipmentType::Drone, "Drone " + std::to_string(i));
            EXPECT_TRUE(data_storage_->saveEquipment(equipment));
            
            auto loaded = data_storage_->loadEquipment(id);
            EXPECT_TRUE(loaded.has_value());
            if (loaded) {
                EXPECT_EQ(loaded->getId(), id);
            }
        });
    }
    
    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all equipment were saved
    auto all = data_storage_->getAllEquipment();
    EXPECT_EQ(all.size(), num_threads);
}

} // namespace equipment_tracker
// </test_code>