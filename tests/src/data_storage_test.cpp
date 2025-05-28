// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <chrono>
#include <thread>
#include "equipment_tracker/data_storage.h"

// Platform-specific time functions
#ifdef _WIN32
#define MKGMTIME _mkgmtime
#else
#define MKGMTIME timegm
#endif

// Test fixture for DataStorage tests
class DataStorageTest : public ::testing::Test {
protected:
    std::string test_db_path;
    
    void SetUp() override {
        // Create a unique temporary directory for each test
        test_db_path = std::filesystem::temp_directory_path().string() + "/equipment_tracker_test_" + 
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
    
    // Helper to create a timestamp
    std::chrono::system_clock::time_point createTimestamp(int year, int month, int day, 
                                                         int hour = 0, int min = 0, int sec = 0) {
        struct tm timeinfo = {};
        timeinfo.tm_year = year - 1900;
        timeinfo.tm_mon = month - 1;
        timeinfo.tm_mday = day;
        timeinfo.tm_hour = hour;
        timeinfo.tm_min = min;
        timeinfo.tm_sec = sec;
        
        time_t time = MKGMTIME(&timeinfo);
        return std::chrono::system_clock::from_time_t(time);
    }
    
    // Helper to create a test equipment
    equipment_tracker::Equipment createTestEquipment(const std::string& id = "test123") {
        equipment_tracker::Equipment equipment(id, equipment_tracker::EquipmentType::Vehicle, "Test Equipment");
        equipment.setStatus(equipment_tracker::EquipmentStatus::Available);
        
        // Add a position
        equipment_tracker::Position position(37.7749, -122.4194, 10.0, 5.0, 
                                            std::chrono::system_clock::now());
        equipment.setLastPosition(position);
        
        return equipment;
    }
};

// Test initialization
TEST_F(DataStorageTest, InitializeCreatesDirectories) {
    equipment_tracker::DataStorage storage(test_db_path);
    
    EXPECT_TRUE(storage.initialize());
    
    // Check that directories were created
    EXPECT_TRUE(std::filesystem::exists(test_db_path));
    EXPECT_TRUE(std::filesystem::exists(test_db_path + "/equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path + "/positions"));
}

// Test saving and loading equipment
TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    equipment_tracker::DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    auto equipment = createTestEquipment();
    EXPECT_TRUE(storage.saveEquipment(equipment));
    
    // Load the equipment back
    auto loaded = storage.loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded.has_value());
    
    // Verify properties
    EXPECT_EQ(loaded->getId(), equipment.getId());
    EXPECT_EQ(loaded->getName(), equipment.getName());
    EXPECT_EQ(loaded->getType(), equipment.getType());
    EXPECT_EQ(loaded->getStatus(), equipment.getStatus());
    
    // Verify position
    auto original_pos = equipment.getLastPosition();
    auto loaded_pos = loaded->getLastPosition();
    ASSERT_TRUE(original_pos.has_value());
    ASSERT_TRUE(loaded_pos.has_value());
    
    EXPECT_DOUBLE_EQ(loaded_pos->getLatitude(), original_pos->getLatitude());
    EXPECT_DOUBLE_EQ(loaded_pos->getLongitude(), original_pos->getLongitude());
    EXPECT_DOUBLE_EQ(loaded_pos->getAltitude(), original_pos->getAltitude());
    EXPECT_DOUBLE_EQ(loaded_pos->getAccuracy(), original_pos->getAccuracy());
}

// Test loading non-existent equipment
TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    equipment_tracker::DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    auto loaded = storage.loadEquipment("nonexistent");
    EXPECT_FALSE(loaded.has_value());
}

// Test updating equipment
TEST_F(DataStorageTest, UpdateEquipment) {
    equipment_tracker::DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    auto equipment = createTestEquipment();
    ASSERT_TRUE(storage.saveEquipment(equipment));
    
    // Update equipment
    equipment.setName("Updated Name");
    equipment.setStatus(equipment_tracker::EquipmentStatus::InUse);
    EXPECT_TRUE(storage.updateEquipment(equipment));
    
    // Load and verify
    auto loaded = storage.loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getName(), "Updated Name");
    EXPECT_EQ(loaded->getStatus(), equipment_tracker::EquipmentStatus::InUse);
}

// Test deleting equipment
TEST_F(DataStorageTest, DeleteEquipment) {
    equipment_tracker::DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    auto equipment = createTestEquipment();
    ASSERT_TRUE(storage.saveEquipment(equipment));
    
    // Delete equipment
    EXPECT_TRUE(storage.deleteEquipment(equipment.getId()));
    
    // Verify it's gone
    auto loaded = storage.loadEquipment(equipment.getId());
    EXPECT_FALSE(loaded.has_value());
}

// Test saving and retrieving position history
TEST_F(DataStorageTest, SaveAndRetrievePositionHistory) {
    equipment_tracker::DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    std::string equipment_id = "test123";
    
    // Create positions with different timestamps
    auto timestamp1 = createTimestamp(2023, 1, 1, 10, 0, 0);
    auto timestamp2 = createTimestamp(2023, 1, 1, 11, 0, 0);
    auto timestamp3 = createTimestamp(2023, 1, 1, 12, 0, 0);
    
    equipment_tracker::Position pos1(37.7749, -122.4194, 10.0, 5.0, timestamp1);
    equipment_tracker::Position pos2(37.7750, -122.4195, 11.0, 6.0, timestamp2);
    equipment_tracker::Position pos3(37.7751, -122.4196, 12.0, 7.0, timestamp3);
    
    // Save positions
    EXPECT_TRUE(storage.savePosition(equipment_id, pos1));
    EXPECT_TRUE(storage.savePosition(equipment_id, pos2));
    EXPECT_TRUE(storage.savePosition(equipment_id, pos3));
    
    // Retrieve all positions
    auto positions = storage.getPositionHistory(
        equipment_id, 
        createTimestamp(2023, 1, 1, 0, 0, 0),
        createTimestamp(2023, 1, 2, 0, 0, 0)
    );
    
    ASSERT_EQ(positions.size(), 3);
    
    // Positions should be sorted by timestamp
    EXPECT_DOUBLE_EQ(positions[0].getLatitude(), pos1.getLatitude());
    EXPECT_DOUBLE_EQ(positions[1].getLatitude(), pos2.getLatitude());
    EXPECT_DOUBLE_EQ(positions[2].getLatitude(), pos3.getLatitude());
    
    // Test time range filtering
    auto filtered_positions = storage.getPositionHistory(
        equipment_id,
        createTimestamp(2023, 1, 1, 10, 30, 0),
        createTimestamp(2023, 1, 1, 11, 30, 0)
    );
    
    ASSERT_EQ(filtered_positions.size(), 1);
    EXPECT_DOUBLE_EQ(filtered_positions[0].getLatitude(), pos2.getLatitude());
}

// Test getting all equipment
TEST_F(DataStorageTest, GetAllEquipment) {
    equipment_tracker::DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    // Create and save multiple equipment
    auto equipment1 = createTestEquipment("test1");
    auto equipment2 = createTestEquipment("test2");
    auto equipment3 = createTestEquipment("test3");
    
    ASSERT_TRUE(storage.saveEquipment(equipment1));
    ASSERT_TRUE(storage.saveEquipment(equipment2));
    ASSERT_TRUE(storage.saveEquipment(equipment3));
    
    // Get all equipment
    auto all_equipment = storage.getAllEquipment();
    ASSERT_EQ(all_equipment.size(), 3);
    
    // Verify IDs (order may vary)
    std::vector<std::string> ids;
    for (const auto& equipment : all_equipment) {
        ids.push_back(equipment.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("test1", "test2", "test3"));
}

// Test finding equipment by status
TEST_F(DataStorageTest, FindEquipmentByStatus) {
    equipment_tracker::DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    // Create equipment with different statuses
    auto equipment1 = createTestEquipment("test1");
    equipment1.setStatus(equipment_tracker::EquipmentStatus::Available);
    
    auto equipment2 = createTestEquipment("test2");
    equipment2.setStatus(equipment_tracker::EquipmentStatus::InUse);
    
    auto equipment3 = createTestEquipment("test3");
    equipment3.setStatus(equipment_tracker::EquipmentStatus::Available);
    
    ASSERT_TRUE(storage.saveEquipment(equipment1));
    ASSERT_TRUE(storage.saveEquipment(equipment2));
    ASSERT_TRUE(storage.saveEquipment(equipment3));
    
    // Find available equipment
    auto available = storage.findEquipmentByStatus(equipment_tracker::EquipmentStatus::Available);
    ASSERT_EQ(available.size(), 2);
    
    // Verify IDs (order may vary)
    std::vector<std::string> ids;
    for (const auto& equipment : available) {
        ids.push_back(equipment.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("test1", "test3"));
    
    // Find in-use equipment
    auto in_use = storage.findEquipmentByStatus(equipment_tracker::EquipmentStatus::InUse);
    ASSERT_EQ(in_use.size(), 1);
    EXPECT_EQ(in_use[0].getId(), "test2");
}

// Test finding equipment by type
TEST_F(DataStorageTest, FindEquipmentByType) {
    equipment_tracker::DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    // Create equipment with different types
    auto equipment1 = createTestEquipment("test1");
    equipment1.setType(equipment_tracker::EquipmentType::Vehicle);
    
    auto equipment2 = createTestEquipment("test2");
    equipment2.setType(equipment_tracker::EquipmentType::Tool);
    
    auto equipment3 = createTestEquipment("test3");
    equipment3.setType(equipment_tracker::EquipmentType::Vehicle);
    
    ASSERT_TRUE(storage.saveEquipment(equipment1));
    ASSERT_TRUE(storage.saveEquipment(equipment2));
    ASSERT_TRUE(storage.saveEquipment(equipment3));
    
    // Find vehicles
    auto vehicles = storage.findEquipmentByType(equipment_tracker::EquipmentType::Vehicle);
    ASSERT_EQ(vehicles.size(), 2);
    
    // Verify IDs (order may vary)
    std::vector<std::string> ids;
    for (const auto& equipment : vehicles) {
        ids.push_back(equipment.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("test1", "test3"));
    
    // Find tools
    auto tools = storage.findEquipmentByType(equipment_tracker::EquipmentType::Tool);
    ASSERT_EQ(tools.size(), 1);
    EXPECT_EQ(tools[0].getId(), "test2");
}

// Test finding equipment in area
TEST_F(DataStorageTest, FindEquipmentInArea) {
    equipment_tracker::DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    // Create equipment at different locations
    auto equipment1 = createTestEquipment("test1");
    equipment1.setLastPosition(equipment_tracker::Position(37.7749, -122.4194, 10.0, 5.0, std::chrono::system_clock::now()));
    
    auto equipment2 = createTestEquipment("test2");
    equipment2.setLastPosition(equipment_tracker::Position(40.7128, -74.0060, 10.0, 5.0, std::chrono::system_clock::now()));
    
    auto equipment3 = createTestEquipment("test3");
    equipment3.setLastPosition(equipment_tracker::Position(37.7750, -122.4195, 10.0, 5.0, std::chrono::system_clock::now()));
    
    ASSERT_TRUE(storage.saveEquipment(equipment1));
    ASSERT_TRUE(storage.saveEquipment(equipment2));
    ASSERT_TRUE(storage.saveEquipment(equipment3));
    
    // Find equipment in San Francisco area
    auto sf_equipment = storage.findEquipmentInArea(37.7, -122.5, 37.8, -122.4);
    ASSERT_EQ(sf_equipment.size(), 2);
    
    // Verify IDs (order may vary)
    std::vector<std::string> ids;
    for (const auto& equipment : sf_equipment) {
        ids.push_back(equipment.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("test1", "test3"));
    
    // Find equipment in New York area
    auto ny_equipment = storage.findEquipmentInArea(40.7, -74.1, 40.8, -74.0);
    ASSERT_EQ(ny_equipment.size(), 1);
    EXPECT_EQ(ny_equipment[0].getId(), "test2");
}

// Test concurrent access
TEST_F(DataStorageTest, ConcurrentAccess) {
    equipment_tracker::DataStorage storage(test_db_path);
    ASSERT_TRUE(storage.initialize());
    
    // Create and save test equipment
    auto equipment = createTestEquipment();
    ASSERT_TRUE(storage.saveEquipment(equipment));
    
    // Create threads that read and write concurrently
    std::vector<std::thread> threads;
    const int num_threads = 10;
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&storage, i, &equipment]() {
            if (i % 2 == 0) {
                // Even threads read
                auto loaded = storage.loadEquipment(equipment.getId());
                EXPECT_TRUE(loaded.has_value());
            } else {
                // Odd threads write
                equipment_tracker::Position pos(
                    37.7749 + (i * 0.001), 
                    -122.4194 + (i * 0.001), 
                    10.0, 
                    5.0, 
                    std::chrono::system_clock::now()
                );
                EXPECT_TRUE(storage.savePosition(equipment.getId(), pos));
            }
        });
    }
    
    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify equipment still exists
    auto loaded = storage.loadEquipment(equipment.getId());
    EXPECT_TRUE(loaded.has_value());
}

// Test error handling for invalid paths
TEST_F(DataStorageTest, InvalidPathHandling) {
    // Use an invalid path that can't be created
    std::string invalid_path = "/nonexistent/directory/that/cannot/be/created";
    
    equipment_tracker::DataStorage storage(invalid_path);
    
    // Initialize should fail gracefully
    EXPECT_FALSE(storage.initialize());
    
    // Operations should fail gracefully
    auto equipment = createTestEquipment();
    EXPECT_FALSE(storage.saveEquipment(equipment));
    EXPECT_FALSE(storage.loadEquipment(equipment.getId()).has_value());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>