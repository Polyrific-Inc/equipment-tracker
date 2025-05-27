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

// Cross-platform compatibility
#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

class DataStorageTest : public ::testing::Test {
protected:
    std::string test_db_path;
    
    void SetUp() override {
        // Create a unique temporary directory for each test
        test_db_path = std::filesystem::temp_directory_path().string() + 
                      PATH_SEPARATOR + "equipment_tracker_test_" + 
                      std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        
        // Ensure the directory doesn't exist
        if (std::filesystem::exists(test_db_path)) {
            std::filesystem::remove_all(test_db_path);
        }
    }
    
    void TearDown() override {
        // Clean up the test directory
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
    
    equipment_tracker::Equipment createTestEquipment(const std::string& id = "test-123") {
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
    ASSERT_TRUE(std::filesystem::exists(test_db_path + PATH_SEPARATOR + "equipment"));
    ASSERT_TRUE(std::filesystem::exists(test_db_path + PATH_SEPARATOR + "positions"));
}

TEST_F(DataStorageTest, InitializeIdempotent) {
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
    
    auto loaded = storage.loadEquipment("non-existent-id");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    equipment_tracker::DataStorage storage(test_db_path);
    auto equipment = createTestEquipment();
    
    ASSERT_TRUE(storage.saveEquipment(equipment));
    
    // Update equipment
    equipment.setName("Updated Name");
    equipment.setStatus(equipment_tracker::EquipmentStatus::InUse);
    
    ASSERT_TRUE(storage.updateEquipment(equipment));
    
    auto loaded = storage.loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getName(), "Updated Name");
    EXPECT_EQ(loaded->getStatus(), equipment_tracker::EquipmentStatus::InUse);
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
    std::string equipment_id = "test-123";
    
    // Create positions with different timestamps
    auto now = std::chrono::system_clock::now();
    auto pos1 = equipment_tracker::Position(40.7128, -74.0060, 10.0, 5.0, now - std::chrono::hours(2));
    auto pos2 = equipment_tracker::Position(40.7129, -74.0061, 11.0, 6.0, now - std::chrono::hours(1));
    auto pos3 = equipment_tracker::Position(40.7130, -74.0062, 12.0, 7.0, now);
    
    // Save positions
    ASSERT_TRUE(storage.savePosition(equipment_id, pos1));
    ASSERT_TRUE(storage.savePosition(equipment_id, pos2));
    ASSERT_TRUE(storage.savePosition(equipment_id, pos3));
    
    // Get all position history
    auto history = storage.getPositionHistory(
        equipment_id, 
        now - std::chrono::hours(3), 
        now + std::chrono::hours(1)
    );
    
    ASSERT_EQ(history.size(), 3);
    
    // Get limited position history
    auto recent_history = storage.getPositionHistory(
        equipment_id, 
        now - std::chrono::minutes(90), 
        now + std::chrono::hours(1)
    );
    
    ASSERT_EQ(recent_history.size(), 2);
    
    // Verify the most recent position
    EXPECT_DOUBLE_EQ(recent_history[1].getLatitude(), pos3.getLatitude());
    EXPECT_DOUBLE_EQ(recent_history[1].getLongitude(), pos3.getLongitude());
}

TEST_F(DataStorageTest, GetAllEquipment) {
    equipment_tracker::DataStorage storage(test_db_path);
    
    auto equipment1 = createTestEquipment("test-1");
    auto equipment2 = createTestEquipment("test-2");
    auto equipment3 = createTestEquipment("test-3");
    
    ASSERT_TRUE(storage.saveEquipment(equipment1));
    ASSERT_TRUE(storage.saveEquipment(equipment2));
    ASSERT_TRUE(storage.saveEquipment(equipment3));
    
    auto all_equipment = storage.getAllEquipment();
    ASSERT_EQ(all_equipment.size(), 3);
    
    // Verify all equipment IDs are present
    std::vector<std::string> ids;
    for (const auto& eq : all_equipment) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("test-1", "test-2", "test-3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    equipment_tracker::DataStorage storage(test_db_path);
    
    auto equipment1 = createTestEquipment("test-1");
    equipment1.setStatus(equipment_tracker::EquipmentStatus::Available);
    
    auto equipment2 = createTestEquipment("test-2");
    equipment2.setStatus(equipment_tracker::EquipmentStatus::InUse);
    
    auto equipment3 = createTestEquipment("test-3");
    equipment3.setStatus(equipment_tracker::EquipmentStatus::Available);
    
    ASSERT_TRUE(storage.saveEquipment(equipment1));
    ASSERT_TRUE(storage.saveEquipment(equipment2));
    ASSERT_TRUE(storage.saveEquipment(equipment3));
    
    auto available_equipment = storage.findEquipmentByStatus(equipment_tracker::EquipmentStatus::Available);
    ASSERT_EQ(available_equipment.size(), 2);
    
    auto in_use_equipment = storage.findEquipmentByStatus(equipment_tracker::EquipmentStatus::InUse);
    ASSERT_EQ(in_use_equipment.size(), 1);
    EXPECT_EQ(in_use_equipment[0].getId(), "test-2");
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    equipment_tracker::DataStorage storage(test_db_path);
    
    auto equipment1 = createTestEquipment("test-1");
    equipment1.setType(equipment_tracker::EquipmentType::Vehicle);
    
    auto equipment2 = createTestEquipment("test-2");
    equipment2.setType(equipment_tracker::EquipmentType::Tool);
    
    auto equipment3 = createTestEquipment("test-3");
    equipment3.setType(equipment_tracker::EquipmentType::Vehicle);
    
    ASSERT_TRUE(storage.saveEquipment(equipment1));
    ASSERT_TRUE(storage.saveEquipment(equipment2));
    ASSERT_TRUE(storage.saveEquipment(equipment3));
    
    auto vehicles = storage.findEquipmentByType(equipment_tracker::EquipmentType::Vehicle);
    ASSERT_EQ(vehicles.size(), 2);
    
    auto tools = storage.findEquipmentByType(equipment_tracker::EquipmentType::Tool);
    ASSERT_EQ(tools.size(), 1);
    EXPECT_EQ(tools[0].getId(), "test-2");
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    equipment_tracker::DataStorage storage(test_db_path);
    
    auto equipment1 = createTestEquipment("test-1");
    equipment1.setLastPosition(equipment_tracker::Position(40.7128, -74.0060, 10.0, 5.0, std::chrono::system_clock::now()));
    
    auto equipment2 = createTestEquipment("test-2");
    equipment2.setLastPosition(equipment_tracker::Position(41.8781, -87.6298, 10.0, 5.0, std::chrono::system_clock::now()));
    
    auto equipment3 = createTestEquipment("test-3");
    equipment3.setLastPosition(equipment_tracker::Position(40.7300, -74.0200, 10.0, 5.0, std::chrono::system_clock::now()));
    
    ASSERT_TRUE(storage.saveEquipment(equipment1));
    ASSERT_TRUE(storage.saveEquipment(equipment2));
    ASSERT_TRUE(storage.saveEquipment(equipment3));
    
    // Search in New York area
    auto ny_equipment = storage.findEquipmentInArea(40.70, -74.05, 40.75, -74.00);
    ASSERT_EQ(ny_equipment.size(), 1);
    EXPECT_EQ(ny_equipment[0].getId(), "test-1");
    
    // Wider search area
    auto wider_area = storage.findEquipmentInArea(40.70, -74.05, 40.75, -74.00);
    ASSERT_EQ(wider_area.size(), 1);
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

TEST_F(DataStorageTest, EquipmentWithNoPosition) {
    equipment_tracker::DataStorage storage(test_db_path);
    
    // Create equipment without position
    equipment_tracker::Equipment equipment("test-no-pos", equipment_tracker::EquipmentType::Vehicle, "No Position");
    
    ASSERT_TRUE(storage.saveEquipment(equipment));
    
    auto loaded = storage.loadEquipment("test-no-pos");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_FALSE(loaded->getLastPosition().has_value());
}

TEST_F(DataStorageTest, EmptyPositionHistory) {
    equipment_tracker::DataStorage storage(test_db_path);
    std::string equipment_id = "test-no-history";
    
    auto now = std::chrono::system_clock::now();
    auto history = storage.getPositionHistory(
        equipment_id, 
        now - std::chrono::hours(1), 
        now
    );
    
    EXPECT_TRUE(history.empty());
}

TEST_F(DataStorageTest, EmptyEquipmentList) {
    equipment_tracker::DataStorage storage(test_db_path);
    storage.initialize();
    
    auto all_equipment = storage.getAllEquipment();
    EXPECT_TRUE(all_equipment.empty());
    
    auto available_equipment = storage.findEquipmentByStatus(equipment_tracker::EquipmentStatus::Available);
    EXPECT_TRUE(available_equipment.empty());
    
    auto vehicles = storage.findEquipmentByType(equipment_tracker::EquipmentType::Vehicle);
    EXPECT_TRUE(vehicles.empty());
    
    auto area_equipment = storage.findEquipmentInArea(40.70, -74.05, 40.75, -74.00);
    EXPECT_TRUE(area_equipment.empty());
}
// </test_code>