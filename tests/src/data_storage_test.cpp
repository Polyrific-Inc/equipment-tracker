// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include "equipment_tracker/data_storage.h"

// Platform-specific time functions
#ifdef _WIN32
#define timegm _mkgmtime
#endif

namespace equipment_tracker {

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
    
    // Helper to create a test equipment
    Equipment createTestEquipment(const std::string& id = "test123") {
        Equipment equipment(id, EquipmentType::Forklift, "Test Forklift");
        equipment.setStatus(EquipmentStatus::Active);
        
        // Add a position
        Position pos(37.7749, -122.4194, 10.0, 2.0);
        equipment.setLastPosition(pos);
        
        return equipment;
    }
    
    // Helper to create a test position
    Position createTestPosition(double lat = 37.7749, double lon = -122.4194) {
        return Position(lat, lon, 10.0, 2.0);
    }
};

// Test initialization
TEST_F(DataStorageTest, Initialize) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Check that directories were created
    EXPECT_TRUE(std::filesystem::exists(test_db_path));
    EXPECT_TRUE(std::filesystem::exists(test_db_path + "/equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path + "/positions"));
}

// Test saving and loading equipment
TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    DataStorage storage(test_db_path);
    Equipment original = createTestEquipment();
    
    // Save equipment
    EXPECT_TRUE(storage.saveEquipment(original));
    
    // Load equipment
    auto loaded = storage.loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    // Verify properties
    EXPECT_EQ(loaded->getId(), original.getId());
    EXPECT_EQ(loaded->getName(), original.getName());
    EXPECT_EQ(static_cast<int>(loaded->getType()), static_cast<int>(original.getType()));
    EXPECT_EQ(static_cast<int>(loaded->getStatus()), static_cast<int>(original.getStatus()));
    
    // Verify position
    auto original_pos = original.getLastPosition();
    auto loaded_pos = loaded->getLastPosition();
    ASSERT_TRUE(original_pos.has_value());
    ASSERT_TRUE(loaded_pos.has_value());
    
    EXPECT_DOUBLE_EQ(loaded_pos->getLatitude(), original_pos->getLatitude());
    EXPECT_DOUBLE_EQ(loaded_pos->getLongitude(), original_pos->getLongitude());
    EXPECT_DOUBLE_EQ(loaded_pos->getAltitude(), original_pos->getAltitude());
    EXPECT_DOUBLE_EQ(loaded_pos->getAccuracy(), original_pos->getAccuracy());
}

// Test updating equipment
TEST_F(DataStorageTest, UpdateEquipment) {
    DataStorage storage(test_db_path);
    Equipment equipment = createTestEquipment();
    
    // Save equipment
    EXPECT_TRUE(storage.saveEquipment(equipment));
    
    // Update equipment
    equipment.setName("Updated Forklift");
    equipment.setStatus(EquipmentStatus::Maintenance);
    EXPECT_TRUE(storage.updateEquipment(equipment));
    
    // Load equipment
    auto loaded = storage.loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded.has_value());
    
    // Verify updated properties
    EXPECT_EQ(loaded->getName(), "Updated Forklift");
    EXPECT_EQ(static_cast<int>(loaded->getStatus()), static_cast<int>(EquipmentStatus::Maintenance));
}

// Test deleting equipment
TEST_F(DataStorageTest, DeleteEquipment) {
    DataStorage storage(test_db_path);
    Equipment equipment = createTestEquipment();
    
    // Save equipment
    EXPECT_TRUE(storage.saveEquipment(equipment));
    
    // Delete equipment
    EXPECT_TRUE(storage.deleteEquipment(equipment.getId()));
    
    // Try to load deleted equipment
    auto loaded = storage.loadEquipment(equipment.getId());
    EXPECT_FALSE(loaded.has_value());
}

// Test saving and retrieving position history
TEST_F(DataStorageTest, PositionHistory) {
    DataStorage storage(test_db_path);
    std::string id = "test123";
    
    // Create positions with different timestamps
    auto now = std::chrono::system_clock::now();
    Position pos1(37.7749, -122.4194, 10.0, 2.0, now - std::chrono::hours(2));
    Position pos2(37.7750, -122.4195, 11.0, 2.1, now - std::chrono::hours(1));
    Position pos3(37.7751, -122.4196, 12.0, 2.2, now);
    
    // Save positions
    EXPECT_TRUE(storage.savePosition(id, pos1));
    EXPECT_TRUE(storage.savePosition(id, pos2));
    EXPECT_TRUE(storage.savePosition(id, pos3));
    
    // Get all position history
    auto history = storage.getPositionHistory(id, 
        now - std::chrono::hours(3), 
        now + std::chrono::hours(1));
    
    EXPECT_EQ(history.size(), 3);
    
    // Get limited position history
    auto recent_history = storage.getPositionHistory(id, 
        now - std::chrono::minutes(90), 
        now + std::chrono::hours(1));
    
    EXPECT_EQ(recent_history.size(), 2);
    
    // Verify the most recent position
    if (!recent_history.empty()) {
        EXPECT_NEAR(recent_history.back().getLatitude(), 37.7751, 0.0001);
        EXPECT_NEAR(recent_history.back().getLongitude(), -122.4196, 0.0001);
    }
}

// Test getting all equipment
TEST_F(DataStorageTest, GetAllEquipment) {
    DataStorage storage(test_db_path);
    
    // Create and save multiple equipment
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    
    EXPECT_TRUE(storage.saveEquipment(eq1));
    EXPECT_TRUE(storage.saveEquipment(eq2));
    EXPECT_TRUE(storage.saveEquipment(eq3));
    
    // Get all equipment
    auto all = storage.getAllEquipment();
    EXPECT_EQ(all.size(), 3);
    
    // Verify IDs are present
    std::vector<std::string> ids;
    for (const auto& eq : all) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("id1", "id2", "id3"));
}

// Test finding equipment by status
TEST_F(DataStorageTest, FindEquipmentByStatus) {
    DataStorage storage(test_db_path);
    
    // Create equipment with different statuses
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    eq1.setStatus(EquipmentStatus::Active);
    
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    eq2.setStatus(EquipmentStatus::Maintenance);
    
    Equipment eq3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setStatus(EquipmentStatus::Active);
    
    EXPECT_TRUE(storage.saveEquipment(eq1));
    EXPECT_TRUE(storage.saveEquipment(eq2));
    EXPECT_TRUE(storage.saveEquipment(eq3));
    
    // Find active equipment
    auto active = storage.findEquipmentByStatus(EquipmentStatus::Active);
    EXPECT_EQ(active.size(), 2);
    
    // Find maintenance equipment
    auto maintenance = storage.findEquipmentByStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(maintenance.size(), 1);
    EXPECT_EQ(maintenance[0].getId(), "id2");
}

// Test finding equipment by type
TEST_F(DataStorageTest, FindEquipmentByType) {
    DataStorage storage(test_db_path);
    
    // Create equipment with different types
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("id3", EquipmentType::Forklift, "Forklift 2");
    
    EXPECT_TRUE(storage.saveEquipment(eq1));
    EXPECT_TRUE(storage.saveEquipment(eq2));
    EXPECT_TRUE(storage.saveEquipment(eq3));
    
    // Find forklifts
    auto forklifts = storage.findEquipmentByType(EquipmentType::Forklift);
    EXPECT_EQ(forklifts.size(), 2);
    
    // Find cranes
    auto cranes = storage.findEquipmentByType(EquipmentType::Crane);
    EXPECT_EQ(cranes.size(), 1);
    EXPECT_EQ(cranes[0].getId(), "id2");
}

// Test finding equipment in area
TEST_F(DataStorageTest, FindEquipmentInArea) {
    DataStorage storage(test_db_path);
    
    // Create equipment with different positions
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    eq1.setLastPosition(Position(37.7749, -122.4194));
    
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    eq2.setLastPosition(Position(37.3382, -121.8863));
    
    Equipment eq3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setLastPosition(Position(37.7749, -122.4195));
    
    EXPECT_TRUE(storage.saveEquipment(eq1));
    EXPECT_TRUE(storage.saveEquipment(eq2));
    EXPECT_TRUE(storage.saveEquipment(eq3));
    
    // Find equipment in San Francisco area
    auto sf_equipment = storage.findEquipmentInArea(
        37.7, -122.5,  // Southwest corner
        37.8, -122.4   // Northeast corner
    );
    
    EXPECT_EQ(sf_equipment.size(), 2);
    
    // Find equipment in San Jose area
    auto sj_equipment = storage.findEquipmentInArea(
        37.3, -122.0,  // Southwest corner
        37.4, -121.8   // Northeast corner
    );
    
    EXPECT_EQ(sj_equipment.size(), 1);
    EXPECT_EQ(sj_equipment[0].getId(), "id2");
}

// Test error handling for non-existent equipment
TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    DataStorage storage(test_db_path);
    
    // Try to load non-existent equipment
    auto loaded = storage.loadEquipment("nonexistent");
    EXPECT_FALSE(loaded.has_value());
}

// Test concurrent access
TEST_F(DataStorageTest, ConcurrentAccess) {
    DataStorage storage(test_db_path);
    
    // Create and save test equipment
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(storage.saveEquipment(equipment));
    
    // Create threads that read and write concurrently
    std::vector<std::thread> threads;
    
    // Thread for reading
    threads.emplace_back([&storage, &equipment]() {
        for (int i = 0; i < 10; i++) {
            auto loaded = storage.loadEquipment(equipment.getId());
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    
    // Thread for writing
    threads.emplace_back([&storage, &equipment]() {
        for (int i = 0; i < 10; i++) {
            equipment.setName("Updated " + std::to_string(i));
            storage.updateEquipment(equipment);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    
    // Join threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify equipment was updated
    auto loaded = storage.loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_THAT(loaded->getName(), ::testing::HasSubstr("Updated"));
}

// Test handling of invalid database path
TEST_F(DataStorageTest, InvalidDatabasePath) {
    // Create a path that should be invalid (a file that exists but isn't a directory)
    std::string invalid_path = test_db_path + "/invalid_file";
    std::filesystem::create_directories(test_db_path);
    std::ofstream file(invalid_path);
    file << "This is not a directory";
    file.close();
    
    // Try to initialize with the invalid path
    DataStorage storage(invalid_path);
    
    // This should fail gracefully without crashing
    EXPECT_FALSE(storage.initialize());
}

} // namespace equipment_tracker
// </test_code>