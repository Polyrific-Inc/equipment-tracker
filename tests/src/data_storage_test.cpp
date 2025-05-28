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

class DataStorageTest : public ::testing::Test {
protected:
    std::string test_db_path;
    
    void SetUp() override {
        // Create a unique temporary directory for each test
        test_db_path = std::filesystem::temp_directory_path().string() + "/equipment_tracker_test_" + 
                       std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    }
    
    void TearDown() override {
        // Clean up test directory after each test
        if (std::filesystem::exists(test_db_path)) {
            std::filesystem::remove_all(test_db_path);
        }
    }
    
    equipment_tracker::Position createTestPosition(double lat = 40.7128, double lon = -74.0060) {
        return equipment_tracker::Position(
            lat, lon, 10.0, 5.0, 
            std::chrono::system_clock::now()
        );
    }
    
    equipment_tracker::Equipment createTestEquipment(const std::string& id = "test123") {
        equipment_tracker::Equipment equipment(
            id, 
            equipment_tracker::EquipmentType::Vehicle, 
            "Test Equipment"
        );
        equipment.setStatus(equipment_tracker::EquipmentStatus::Available);
        equipment.setLastPosition(createTestPosition());
        return equipment;
    }
};

TEST_F(DataStorageTest, InitializeCreatesDirectories) {
    equipment_tracker::DataStorage storage(test_db_path);
    
    ASSERT_TRUE(storage.initialize());
    ASSERT_TRUE(std::filesystem::exists(test_db_path));
    ASSERT_TRUE(std::filesystem::exists(test_db_path + "/equipment"));
    ASSERT_TRUE(std::filesystem::exists(test_db_path + "/positions"));
}

TEST_F(DataStorageTest, InitializeIsIdempotent) {
    equipment_tracker::DataStorage storage(test_db_path);
    
    ASSERT_TRUE(storage.initialize());
    ASSERT_TRUE(storage.initialize()); // Second call should also return true
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    equipment_tracker::DataStorage storage(test_db_path);
    auto equipment = createTestEquipment();
    
    ASSERT_TRUE(storage.saveEquipment(equipment));
    
    auto loaded = storage.loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getId(), equipment.getId());
    EXPECT_EQ(loaded->getName(), equipment.getName());
    EXPECT_EQ(loaded->getType(), equipment.getType());
    EXPECT_EQ(loaded->getStatus(), equipment.getStatus());
    
    auto original_pos = equipment.getLastPosition();
    auto loaded_pos = loaded->getLastPosition();
    ASSERT_TRUE(original_pos.has_value());
    ASSERT_TRUE(loaded_pos.has_value());
    EXPECT_DOUBLE_EQ(loaded_pos->getLatitude(), original_pos->getLatitude());
    EXPECT_DOUBLE_EQ(loaded_pos->getLongitude(), original_pos->getLongitude());
    EXPECT_DOUBLE_EQ(loaded_pos->getAltitude(), original_pos->getAltitude());
    EXPECT_DOUBLE_EQ(loaded_pos->getAccuracy(), original_pos->getAccuracy());
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    equipment_tracker::DataStorage storage(test_db_path);
    storage.initialize();
    
    auto loaded = storage.loadEquipment("nonexistent");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    equipment_tracker::DataStorage storage(test_db_path);
    auto equipment = createTestEquipment();
    
    ASSERT_TRUE(storage.saveEquipment(equipment));
    
    // Update equipment
    equipment.setName("Updated Name");
    equipment.setStatus(equipment_tracker::EquipmentStatus::InUse);
    equipment.setLastPosition(createTestPosition(41.0, -75.0));
    
    ASSERT_TRUE(storage.updateEquipment(equipment));
    
    // Load and verify updates
    auto loaded = storage.loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getName(), "Updated Name");
    EXPECT_EQ(loaded->getStatus(), equipment_tracker::EquipmentStatus::InUse);
    
    auto pos = loaded->getLastPosition();
    ASSERT_TRUE(pos.has_value());
    EXPECT_DOUBLE_EQ(pos->getLatitude(), 41.0);
    EXPECT_DOUBLE_EQ(pos->getLongitude(), -75.0);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    equipment_tracker::DataStorage storage(test_db_path);
    auto equipment = createTestEquipment();
    
    ASSERT_TRUE(storage.saveEquipment(equipment));
    ASSERT_TRUE(storage.deleteEquipment(equipment.getId()));
    
    auto loaded = storage.loadEquipment(equipment.getId());
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    equipment_tracker::DataStorage storage(test_db_path);
    std::string id = "test123";
    
    // Create positions at different times
    auto now = std::chrono::system_clock::now();
    auto pos1 = equipment_tracker::Position(40.0, -74.0, 10.0, 5.0, now - std::chrono::hours(2));
    auto pos2 = equipment_tracker::Position(40.1, -74.1, 11.0, 6.0, now - std::chrono::hours(1));
    auto pos3 = equipment_tracker::Position(40.2, -74.2, 12.0, 7.0, now);
    
    // Save positions
    ASSERT_TRUE(storage.savePosition(id, pos1));
    ASSERT_TRUE(storage.savePosition(id, pos2));
    ASSERT_TRUE(storage.savePosition(id, pos3));
    
    // Get all position history
    auto history = storage.getPositionHistory(
        id, 
        now - std::chrono::hours(3), 
        now + std::chrono::hours(1)
    );
    
    ASSERT_EQ(history.size(), 3);
    
    // Get partial history
    auto partial_history = storage.getPositionHistory(
        id, 
        now - std::chrono::hours(1) - std::chrono::minutes(30), 
        now + std::chrono::hours(1)
    );
    
    ASSERT_EQ(partial_history.size(), 2);
}

TEST_F(DataStorageTest, GetAllEquipment) {
    equipment_tracker::DataStorage storage(test_db_path);
    
    // Create and save multiple equipment
    auto equip1 = createTestEquipment("equip1");
    auto equip2 = createTestEquipment("equip2");
    auto equip3 = createTestEquipment("equip3");
    
    ASSERT_TRUE(storage.saveEquipment(equip1));
    ASSERT_TRUE(storage.saveEquipment(equip2));
    ASSERT_TRUE(storage.saveEquipment(equip3));
    
    // Get all equipment
    auto all = storage.getAllEquipment();
    ASSERT_EQ(all.size(), 3);
    
    // Verify IDs are present
    std::vector<std::string> ids;
    for (const auto& equip : all) {
        ids.push_back(equip.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("equip1", "equip2", "equip3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    equipment_tracker::DataStorage storage(test_db_path);
    
    // Create equipment with different statuses
    auto equip1 = createTestEquipment("equip1");
    equip1.setStatus(equipment_tracker::EquipmentStatus::Available);
    
    auto equip2 = createTestEquipment("equip2");
    equip2.setStatus(equipment_tracker::EquipmentStatus::InUse);
    
    auto equip3 = createTestEquipment("equip3");
    equip3.setStatus(equipment_tracker::EquipmentStatus::Available);
    
    ASSERT_TRUE(storage.saveEquipment(equip1));
    ASSERT_TRUE(storage.saveEquipment(equip2));
    ASSERT_TRUE(storage.saveEquipment(equip3));
    
    // Find by status
    auto available = storage.findEquipmentByStatus(equipment_tracker::EquipmentStatus::Available);
    ASSERT_EQ(available.size(), 2);
    
    auto in_use = storage.findEquipmentByStatus(equipment_tracker::EquipmentStatus::InUse);
    ASSERT_EQ(in_use.size(), 1);
    EXPECT_EQ(in_use[0].getId(), "equip2");
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    equipment_tracker::DataStorage storage(test_db_path);
    
    // Create equipment with different types
    auto equip1 = createTestEquipment("equip1");
    equip1.setType(equipment_tracker::EquipmentType::Vehicle);
    
    auto equip2 = createTestEquipment("equip2");
    equip2.setType(equipment_tracker::EquipmentType::Tool);
    
    auto equip3 = createTestEquipment("equip3");
    equip3.setType(equipment_tracker::EquipmentType::Vehicle);
    
    ASSERT_TRUE(storage.saveEquipment(equip1));
    ASSERT_TRUE(storage.saveEquipment(equip2));
    ASSERT_TRUE(storage.saveEquipment(equip3));
    
    // Find by type
    auto vehicles = storage.findEquipmentByType(equipment_tracker::EquipmentType::Vehicle);
    ASSERT_EQ(vehicles.size(), 2);
    
    auto tools = storage.findEquipmentByType(equipment_tracker::EquipmentType::Tool);
    ASSERT_EQ(tools.size(), 1);
    EXPECT_EQ(tools[0].getId(), "equip2");
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    equipment_tracker::DataStorage storage(test_db_path);
    
    // Create equipment at different locations
    auto equip1 = createTestEquipment("equip1");
    equip1.setLastPosition(equipment_tracker::Position(40.0, -74.0, 10.0, 5.0, std::chrono::system_clock::now()));
    
    auto equip2 = createTestEquipment("equip2");
    equip2.setLastPosition(equipment_tracker::Position(41.0, -75.0, 10.0, 5.0, std::chrono::system_clock::now()));
    
    auto equip3 = createTestEquipment("equip3");
    equip3.setLastPosition(equipment_tracker::Position(42.0, -76.0, 10.0, 5.0, std::chrono::system_clock::now()));
    
    ASSERT_TRUE(storage.saveEquipment(equip1));
    ASSERT_TRUE(storage.saveEquipment(equip2));
    ASSERT_TRUE(storage.saveEquipment(equip3));
    
    // Find in area
    auto area1 = storage.findEquipmentInArea(39.5, -74.5, 40.5, -73.5);
    ASSERT_EQ(area1.size(), 1);
    EXPECT_EQ(area1[0].getId(), "equip1");
    
    auto area2 = storage.findEquipmentInArea(39.0, -77.0, 43.0, -73.0);
    ASSERT_EQ(area2.size(), 3);
}

TEST_F(DataStorageTest, ExecuteQuery) {
    equipment_tracker::DataStorage storage(test_db_path);
    
    // This is just testing the placeholder implementation
    testing::internal::CaptureStdout();
    bool result = storage.executeQuery("SELECT * FROM equipment");
    std::string output = testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(result);
    EXPECT_THAT(output, ::testing::HasSubstr("Query: SELECT * FROM equipment"));
}

TEST_F(DataStorageTest, SaveEquipmentWithoutInitialization) {
    equipment_tracker::DataStorage storage(test_db_path);
    auto equipment = createTestEquipment();
    
    // Should initialize automatically
    ASSERT_TRUE(storage.saveEquipment(equipment));
    ASSERT_TRUE(std::filesystem::exists(test_db_path));
}

TEST_F(DataStorageTest, LoadEquipmentWithoutInitialization) {
    equipment_tracker::DataStorage storage(test_db_path);
    auto equipment = createTestEquipment();
    
    ASSERT_TRUE(storage.saveEquipment(equipment));
    
    // Create a new storage instance (uninitialized)
    equipment_tracker::DataStorage new_storage(test_db_path);
    
    // Should initialize automatically
    auto loaded = new_storage.loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded.has_value());
}

TEST_F(DataStorageTest, GetPositionHistoryForNonExistentEquipment) {
    equipment_tracker::DataStorage storage(test_db_path);
    storage.initialize();
    
    auto now = std::chrono::system_clock::now();
    auto history = storage.getPositionHistory(
        "nonexistent", 
        now - std::chrono::hours(1), 
        now
    );
    
    EXPECT_TRUE(history.empty());
}
// </test_code>