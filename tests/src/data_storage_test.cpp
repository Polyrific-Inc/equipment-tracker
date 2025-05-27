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
#define timegm _mkgmtime
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
        Equipment equipment(id, EquipmentType::Vehicle, "Test Equipment");
        equipment.setStatus(EquipmentStatus::Available);
        
        // Add a position
        auto now = std::chrono::system_clock::now();
        Position position(37.7749, -122.4194, 10.0, 5.0, now);
        equipment.setLastPosition(position);
        
        return equipment;
    }
    
    // Helper method to create multiple test positions
    std::vector<Position> createTestPositions(int count) {
        std::vector<Position> positions;
        auto now = std::chrono::system_clock::now();
        
        for (int i = 0; i < count; i++) {
            // Create positions with increasing timestamps
            auto timestamp = now + std::chrono::minutes(i);
            positions.emplace_back(
                37.7749 + (i * 0.001),  // Slightly different latitude each time
                -122.4194 + (i * 0.001), // Slightly different longitude each time
                10.0 + i,                // Increasing altitude
                5.0,                     // Constant accuracy
                timestamp
            );
        }
        
        return positions;
    }
};

TEST_F(DataStorageTest, InitializeCreatesDirectories) {
    EXPECT_TRUE(data_storage_->initialize());
    
    // Check that the database directories were created
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "positions"));
}

TEST_F(DataStorageTest, InitializeIsIdempotent) {
    EXPECT_TRUE(data_storage_->initialize());
    EXPECT_TRUE(data_storage_->initialize()); // Second call should also return true
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    EXPECT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment original = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(original));
    
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
    
    // Timestamps might have slight differences due to serialization/deserialization
    auto original_time = std::chrono::system_clock::to_time_t(original.getLastPosition()->getTimestamp());
    auto loaded_time = std::chrono::system_clock::to_time_t(loaded->getLastPosition()->getTimestamp());
    EXPECT_EQ(loaded_time, original_time);
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    EXPECT_TRUE(data_storage_->initialize());
    
    // Try to load non-existent equipment
    auto loaded = data_storage_->loadEquipment("nonexistent");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    EXPECT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment original = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(original));
    
    // Update the equipment
    Equipment updated = original;
    updated.setName("Updated Name");
    updated.setStatus(EquipmentStatus::InUse);
    
    // Save the updated equipment
    EXPECT_TRUE(data_storage_->updateEquipment(updated));
    
    // Load the equipment
    auto loaded = data_storage_->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    // Verify the loaded equipment has the updated values
    EXPECT_EQ(loaded->getName(), "Updated Name");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::InUse);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    EXPECT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Verify it exists
    EXPECT_TRUE(data_storage_->loadEquipment(equipment.getId()).has_value());
    
    // Delete the equipment
    EXPECT_TRUE(data_storage_->deleteEquipment(equipment.getId()));
    
    // Verify it no longer exists
    EXPECT_FALSE(data_storage_->loadEquipment(equipment.getId()).has_value());
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    EXPECT_TRUE(data_storage_->initialize());
    
    // Create test equipment
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create test positions
    auto positions = createTestPositions(5);
    
    // Save each position
    for (const auto& pos : positions) {
        EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos));
    }
    
    // Get position history
    auto start_time = std::chrono::system_clock::now() - std::chrono::hours(1);
    auto end_time = std::chrono::system_clock::now() + std::chrono::hours(1);
    auto history = data_storage_->getPositionHistory(equipment.getId(), start_time, end_time);
    
    // Verify we got all positions
    EXPECT_EQ(history.size(), positions.size());
    
    // Verify positions are in chronological order
    for (size_t i = 1; i < history.size(); i++) {
        EXPECT_LE(
            std::chrono::system_clock::to_time_t(history[i-1].getTimestamp()),
            std::chrono::system_clock::to_time_t(history[i].getTimestamp())
        );
    }
    
    // Verify position data
    for (size_t i = 0; i < history.size(); i++) {
        EXPECT_DOUBLE_EQ(history[i].getLatitude(), positions[i].getLatitude());
        EXPECT_DOUBLE_EQ(history[i].getLongitude(), positions[i].getLongitude());
        EXPECT_DOUBLE_EQ(history[i].getAltitude(), positions[i].getAltitude());
        EXPECT_DOUBLE_EQ(history[i].getAccuracy(), positions[i].getAccuracy());
        
        // Timestamps might have slight differences due to serialization/deserialization
        auto original_time = std::chrono::system_clock::to_time_t(positions[i].getTimestamp());
        auto loaded_time = std::chrono::system_clock::to_time_t(history[i].getTimestamp());
        EXPECT_EQ(loaded_time, original_time);
    }
}

TEST_F(DataStorageTest, GetPositionHistoryWithTimeRange) {
    EXPECT_TRUE(data_storage_->initialize());
    
    // Create test equipment
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create test positions with timestamps 10 minutes apart
    auto positions = createTestPositions(10);
    
    // Save each position
    for (const auto& pos : positions) {
        EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos));
    }
    
    // Get position history for middle time range (positions 3-6)
    auto start_time = positions[3].getTimestamp();
    auto end_time = positions[6].getTimestamp();
    auto history = data_storage_->getPositionHistory(equipment.getId(), start_time, end_time);
    
    // Verify we got the expected number of positions
    EXPECT_EQ(history.size(), 4); // Positions 3, 4, 5, 6
}

TEST_F(DataStorageTest, GetAllEquipment) {
    EXPECT_TRUE(data_storage_->initialize());
    
    // Create and save multiple equipment items
    Equipment equipment1("test1", EquipmentType::Vehicle, "Equipment 1");
    Equipment equipment2("test2", EquipmentType::Tool, "Equipment 2");
    Equipment equipment3("test3", EquipmentType::Other, "Equipment 3");
    
    EXPECT_TRUE(data_storage_->saveEquipment(equipment1));
    EXPECT_TRUE(data_storage_->saveEquipment(equipment2));
    EXPECT_TRUE(data_storage_->saveEquipment(equipment3));
    
    // Get all equipment
    auto all_equipment = data_storage_->getAllEquipment();
    
    // Verify we got all equipment
    EXPECT_EQ(all_equipment.size(), 3);
    
    // Verify equipment IDs
    std::vector<std::string> ids;
    for (const auto& equipment : all_equipment) {
        ids.push_back(equipment.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("test1", "test2", "test3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    EXPECT_TRUE(data_storage_->initialize());
    
    // Create and save equipment with different statuses
    Equipment equipment1("test1", EquipmentType::Vehicle, "Equipment 1");
    equipment1.setStatus(EquipmentStatus::Available);
    
    Equipment equipment2("test2", EquipmentType::Tool, "Equipment 2");
    equipment2.setStatus(EquipmentStatus::InUse);
    
    Equipment equipment3("test3", EquipmentType::Other, "Equipment 3");
    equipment3.setStatus(EquipmentStatus::Available);
    
    EXPECT_TRUE(data_storage_->saveEquipment(equipment1));
    EXPECT_TRUE(data_storage_->saveEquipment(equipment2));
    EXPECT_TRUE(data_storage_->saveEquipment(equipment3));
    
    // Find equipment by status
    auto available_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Available);
    
    // Verify we got the expected equipment
    EXPECT_EQ(available_equipment.size(), 2);
    
    // Verify equipment IDs
    std::vector<std::string> ids;
    for (const auto& equipment : available_equipment) {
        ids.push_back(equipment.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("test1", "test3"));
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    EXPECT_TRUE(data_storage_->initialize());
    
    // Create and save equipment with different types
    Equipment equipment1("test1", EquipmentType::Vehicle, "Equipment 1");
    Equipment equipment2("test2", EquipmentType::Tool, "Equipment 2");
    Equipment equipment3("test3", EquipmentType::Vehicle, "Equipment 3");
    
    EXPECT_TRUE(data_storage_->saveEquipment(equipment1));
    EXPECT_TRUE(data_storage_->saveEquipment(equipment2));
    EXPECT_TRUE(data_storage_->saveEquipment(equipment3));
    
    // Find equipment by type
    auto vehicle_equipment = data_storage_->findEquipmentByType(EquipmentType::Vehicle);
    
    // Verify we got the expected equipment
    EXPECT_EQ(vehicle_equipment.size(), 2);
    
    // Verify equipment IDs
    std::vector<std::string> ids;
    for (const auto& equipment : vehicle_equipment) {
        ids.push_back(equipment.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("test1", "test3"));
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    EXPECT_TRUE(data_storage_->initialize());
    
    // Create and save equipment with different positions
    Equipment equipment1("test1", EquipmentType::Vehicle, "Equipment 1");
    equipment1.setLastPosition(Position(37.7749, -122.4194, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment equipment2("test2", EquipmentType::Tool, "Equipment 2");
    equipment2.setLastPosition(Position(40.7128, -74.0060, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment equipment3("test3", EquipmentType::Vehicle, "Equipment 3");
    equipment3.setLastPosition(Position(37.3382, -121.8863, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment equipment4("test4", EquipmentType::Other, "Equipment 4");
    // No position for equipment4
    
    EXPECT_TRUE(data_storage_->saveEquipment(equipment1));
    EXPECT_TRUE(data_storage_->saveEquipment(equipment2));
    EXPECT_TRUE(data_storage_->saveEquipment(equipment3));
    EXPECT_TRUE(data_storage_->saveEquipment(equipment4));
    
    // Find equipment in San Francisco Bay Area
    auto bay_area_equipment = data_storage_->findEquipmentInArea(37.0, -123.0, 38.0, -121.0);
    
    // Verify we got the expected equipment
    EXPECT_EQ(bay_area_equipment.size(), 2);
    
    // Verify equipment IDs
    std::vector<std::string> ids;
    for (const auto& equipment : bay_area_equipment) {
        ids.push_back(equipment.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("test1", "test3"));
}

TEST_F(DataStorageTest, ExecuteQuery) {
    EXPECT_TRUE(data_storage_->initialize());
    
    // This is just testing the placeholder implementation
    testing::internal::CaptureStdout();
    bool result = data_storage_->executeQuery("SELECT * FROM equipment");
    std::string output = testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(result);
    EXPECT_THAT(output, ::testing::HasSubstr("Query: SELECT * FROM equipment"));
}

} // namespace equipment_tracker
// </test_code>