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

using namespace equipment_tracker;
using ::testing::HasSubstr;

class DataStorageTest : public ::testing::Test {
protected:
    std::string test_db_path;

    void SetUp() override {
        // Create a unique temporary directory for each test
        test_db_path = std::filesystem::temp_directory_path().string() + "/equipment_tracker_test_" + 
                       std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    }

    void TearDown() override {
        // Clean up the test directory
        if (std::filesystem::exists(test_db_path)) {
            std::filesystem::remove_all(test_db_path);
        }
    }

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

    // Helper method to verify equipment equality
    void verifyEquipment(const Equipment& expected, const Equipment& actual) {
        EXPECT_EQ(expected.getId(), actual.getId());
        EXPECT_EQ(expected.getName(), actual.getName());
        EXPECT_EQ(expected.getType(), actual.getType());
        EXPECT_EQ(expected.getStatus(), actual.getStatus());
        
        auto expectedPos = expected.getLastPosition();
        auto actualPos = actual.getLastPosition();
        
        if (expectedPos && actualPos) {
            EXPECT_NEAR(expectedPos->getLatitude(), actualPos->getLatitude(), 0.0001);
            EXPECT_NEAR(expectedPos->getLongitude(), actualPos->getLongitude(), 0.0001);
            EXPECT_NEAR(expectedPos->getAltitude(), actualPos->getAltitude(), 0.0001);
            EXPECT_NEAR(expectedPos->getAccuracy(), actualPos->getAccuracy(), 0.0001);
            
            // Compare timestamps with some tolerance
            auto expected_time = std::chrono::system_clock::to_time_t(expectedPos->getTimestamp());
            auto actual_time = std::chrono::system_clock::to_time_t(actualPos->getTimestamp());
            EXPECT_NEAR(expected_time, actual_time, 2); // Allow 2 seconds difference
        } else {
            EXPECT_EQ(expectedPos.has_value(), actualPos.has_value());
        }
    }
};

TEST_F(DataStorageTest, Initialize) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Check that the database directories were created
    EXPECT_TRUE(std::filesystem::exists(test_db_path));
    EXPECT_TRUE(std::filesystem::exists(test_db_path + "/equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path + "/positions"));
    
    // Initializing again should return true
    EXPECT_TRUE(storage.initialize());
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(storage.saveEquipment(equipment));
    
    auto loaded = storage.loadEquipment(equipment.getId());
    EXPECT_TRUE(loaded.has_value());
    
    if (loaded) {
        verifyEquipment(equipment, loaded.value());
    }
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    auto loaded = storage.loadEquipment("nonexistent");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(storage.saveEquipment(equipment));
    
    // Update equipment
    equipment.setName("Updated Name");
    equipment.setStatus(EquipmentStatus::InUse);
    EXPECT_TRUE(storage.updateEquipment(equipment));
    
    auto loaded = storage.loadEquipment(equipment.getId());
    EXPECT_TRUE(loaded.has_value());
    
    if (loaded) {
        EXPECT_EQ("Updated Name", loaded->getName());
        EXPECT_EQ(EquipmentStatus::InUse, loaded->getStatus());
    }
}

TEST_F(DataStorageTest, DeleteEquipment) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(storage.saveEquipment(equipment));
    
    // Verify it exists
    EXPECT_TRUE(storage.loadEquipment(equipment.getId()).has_value());
    
    // Delete it
    EXPECT_TRUE(storage.deleteEquipment(equipment.getId()));
    
    // Verify it's gone
    EXPECT_FALSE(storage.loadEquipment(equipment.getId()).has_value());
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    std::string id = "test123";
    
    // Create positions at different times
    auto now = std::chrono::system_clock::now();
    auto one_hour_ago = now - std::chrono::hours(1);
    auto two_hours_ago = now - std::chrono::hours(2);
    
    Position pos1(37.7749, -122.4194, 10.0, 5.0, two_hours_ago);
    Position pos2(37.7750, -122.4195, 11.0, 4.0, one_hour_ago);
    Position pos3(37.7751, -122.4196, 12.0, 3.0, now);
    
    // Save positions
    EXPECT_TRUE(storage.savePosition(id, pos1));
    EXPECT_TRUE(storage.savePosition(id, pos2));
    EXPECT_TRUE(storage.savePosition(id, pos3));
    
    // Get all positions
    auto three_hours_ago = now - std::chrono::hours(3);
    auto one_hour_from_now = now + std::chrono::hours(1);
    auto positions = storage.getPositionHistory(id, three_hours_ago, one_hour_from_now);
    
    EXPECT_EQ(3, positions.size());
    
    // Positions should be sorted by timestamp (oldest first)
    if (positions.size() >= 3) {
        EXPECT_NEAR(pos1.getLatitude(), positions[0].getLatitude(), 0.0001);
        EXPECT_NEAR(pos2.getLatitude(), positions[1].getLatitude(), 0.0001);
        EXPECT_NEAR(pos3.getLatitude(), positions[2].getLatitude(), 0.0001);
    }
    
    // Test time range filtering
    auto filtered_positions = storage.getPositionHistory(id, one_hour_ago - std::chrono::minutes(5), 
                                                        one_hour_ago + std::chrono::minutes(5));
    EXPECT_EQ(1, filtered_positions.size());
    
    if (!filtered_positions.empty()) {
        EXPECT_NEAR(pos2.getLatitude(), filtered_positions[0].getLatitude(), 0.0001);
    }
}

TEST_F(DataStorageTest, GetAllEquipment) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Create and save multiple equipment
    Equipment eq1("id1", EquipmentType::Vehicle, "Equipment 1");
    Equipment eq2("id2", EquipmentType::Tool, "Equipment 2");
    Equipment eq3("id3", EquipmentType::Other, "Equipment 3");
    
    EXPECT_TRUE(storage.saveEquipment(eq1));
    EXPECT_TRUE(storage.saveEquipment(eq2));
    EXPECT_TRUE(storage.saveEquipment(eq3));
    
    auto all_equipment = storage.getAllEquipment();
    EXPECT_EQ(3, all_equipment.size());
    
    // Verify all equipment are retrieved
    bool found_eq1 = false, found_eq2 = false, found_eq3 = false;
    
    for (const auto& eq : all_equipment) {
        if (eq.getId() == "id1") found_eq1 = true;
        if (eq.getId() == "id2") found_eq2 = true;
        if (eq.getId() == "id3") found_eq3 = true;
    }
    
    EXPECT_TRUE(found_eq1);
    EXPECT_TRUE(found_eq2);
    EXPECT_TRUE(found_eq3);
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Create equipment with different statuses
    Equipment eq1("id1", EquipmentType::Vehicle, "Equipment 1");
    eq1.setStatus(EquipmentStatus::Available);
    
    Equipment eq2("id2", EquipmentType::Tool, "Equipment 2");
    eq2.setStatus(EquipmentStatus::InUse);
    
    Equipment eq3("id3", EquipmentType::Other, "Equipment 3");
    eq3.setStatus(EquipmentStatus::Available);
    
    EXPECT_TRUE(storage.saveEquipment(eq1));
    EXPECT_TRUE(storage.saveEquipment(eq2));
    EXPECT_TRUE(storage.saveEquipment(eq3));
    
    // Find available equipment
    auto available = storage.findEquipmentByStatus(EquipmentStatus::Available);
    EXPECT_EQ(2, available.size());
    
    // Find in-use equipment
    auto in_use = storage.findEquipmentByStatus(EquipmentStatus::InUse);
    EXPECT_EQ(1, in_use.size());
    
    if (!in_use.empty()) {
        EXPECT_EQ("id2", in_use[0].getId());
    }
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Create equipment with different types
    Equipment eq1("id1", EquipmentType::Vehicle, "Equipment 1");
    Equipment eq2("id2", EquipmentType::Tool, "Equipment 2");
    Equipment eq3("id3", EquipmentType::Vehicle, "Equipment 3");
    
    EXPECT_TRUE(storage.saveEquipment(eq1));
    EXPECT_TRUE(storage.saveEquipment(eq2));
    EXPECT_TRUE(storage.saveEquipment(eq3));
    
    // Find vehicles
    auto vehicles = storage.findEquipmentByType(EquipmentType::Vehicle);
    EXPECT_EQ(2, vehicles.size());
    
    // Find tools
    auto tools = storage.findEquipmentByType(EquipmentType::Tool);
    EXPECT_EQ(1, tools.size());
    
    if (!tools.empty()) {
        EXPECT_EQ("id2", tools[0].getId());
    }
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Create equipment with different positions
    Equipment eq1("id1", EquipmentType::Vehicle, "Equipment 1");
    eq1.setLastPosition(Position(37.7749, -122.4194, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment eq2("id2", EquipmentType::Tool, "Equipment 2");
    eq2.setLastPosition(Position(40.7128, -74.0060, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment eq3("id3", EquipmentType::Vehicle, "Equipment 3");
    eq3.setLastPosition(Position(37.7750, -122.4195, 10.0, 5.0, std::chrono::system_clock::now()));
    
    Equipment eq4("id4", EquipmentType::Other, "Equipment 4");
    // No position for eq4
    
    EXPECT_TRUE(storage.saveEquipment(eq1));
    EXPECT_TRUE(storage.saveEquipment(eq2));
    EXPECT_TRUE(storage.saveEquipment(eq3));
    EXPECT_TRUE(storage.saveEquipment(eq4));
    
    // Find equipment in San Francisco area
    auto sf_equipment = storage.findEquipmentInArea(37.7, -122.5, 37.8, -122.4);
    EXPECT_EQ(2, sf_equipment.size());
    
    // Find equipment in New York area
    auto ny_equipment = storage.findEquipmentInArea(40.7, -74.1, 40.8, -74.0);
    EXPECT_EQ(1, ny_equipment.size());
    
    if (!ny_equipment.empty()) {
        EXPECT_EQ("id2", ny_equipment[0].getId());
    }
}

TEST_F(DataStorageTest, ExecuteQuery) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Redirect cout to capture output
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());
    
    EXPECT_TRUE(storage.executeQuery("SELECT * FROM equipment"));
    
    // Restore cout
    std::cout.rdbuf(old);
    
    // Verify output contains the query
    EXPECT_THAT(buffer.str(), HasSubstr("SELECT * FROM equipment"));
}

TEST_F(DataStorageTest, NonExistentDirectory) {
    // Test with a path that doesn't exist and requires multiple directory creation
    std::string deep_path = test_db_path + "/deep/nested/path";
    DataStorage storage(deep_path);
    
    EXPECT_TRUE(storage.initialize());
    EXPECT_TRUE(std::filesystem::exists(deep_path));
    EXPECT_TRUE(std::filesystem::exists(deep_path + "/equipment"));
    EXPECT_TRUE(std::filesystem::exists(deep_path + "/positions"));
}

TEST_F(DataStorageTest, ConcurrentAccess) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Create multiple threads that save and load equipment
    std::vector<std::thread> threads;
    const int num_threads = 5;
    const int operations_per_thread = 10;
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&storage, i, operations_per_thread]() {
            for (int j = 0; j < operations_per_thread; j++) {
                std::string id = "thread" + std::to_string(i) + "_op" + std::to_string(j);
                Equipment eq(id, EquipmentType::Vehicle, "Thread Test");
                
                // Save equipment
                EXPECT_TRUE(storage.saveEquipment(eq));
                
                // Load equipment
                auto loaded = storage.loadEquipment(id);
                EXPECT_TRUE(loaded.has_value());
                
                if (loaded) {
                    EXPECT_EQ(id, loaded->getId());
                }
            }
        });
    }
    
    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all equipment were saved
    auto all_equipment = storage.getAllEquipment();
    EXPECT_EQ(num_threads * operations_per_thread, all_equipment.size());
}
// </test_code>