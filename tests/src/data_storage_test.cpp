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
TEST_F(DataStorageTest, InitializeCreatesDirectories) {
    DataStorage storage(test_db_path);
    
    EXPECT_TRUE(storage.initialize());
    EXPECT_TRUE(std::filesystem::exists(test_db_path));
    EXPECT_TRUE(std::filesystem::exists(test_db_path + "/equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path + "/positions"));
}

// Test saving and loading equipment
TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    DataStorage storage(test_db_path);
    storage.initialize();
    
    Equipment original = createTestEquipment();
    EXPECT_TRUE(storage.saveEquipment(original));
    
    auto loaded = storage.loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    EXPECT_EQ(loaded->getId(), original.getId());
    EXPECT_EQ(loaded->getName(), original.getName());
    EXPECT_EQ(static_cast<int>(loaded->getType()), static_cast<int>(original.getType()));
    EXPECT_EQ(static_cast<int>(loaded->getStatus()), static_cast<int>(original.getStatus()));
    
    // Check position
    auto original_pos = original.getLastPosition();
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
    DataStorage storage(test_db_path);
    storage.initialize();
    
    auto loaded = storage.loadEquipment("nonexistent");
    EXPECT_FALSE(loaded.has_value());
}

// Test updating equipment
TEST_F(DataStorageTest, UpdateEquipment) {
    DataStorage storage(test_db_path);
    storage.initialize();
    
    Equipment original = createTestEquipment();
    EXPECT_TRUE(storage.saveEquipment(original));
    
    // Modify and update
    Equipment updated = original;
    updated.setName("Updated Forklift");
    updated.setStatus(EquipmentStatus::Maintenance);
    
    EXPECT_TRUE(storage.updateEquipment(updated));
    
    // Load and verify
    auto loaded = storage.loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    EXPECT_EQ(loaded->getName(), "Updated Forklift");
    EXPECT_EQ(static_cast<int>(loaded->getStatus()), static_cast<int>(EquipmentStatus::Maintenance));
}

// Test deleting equipment
TEST_F(DataStorageTest, DeleteEquipment) {
    DataStorage storage(test_db_path);
    storage.initialize();
    
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(storage.saveEquipment(equipment));
    
    // Save a position
    Position pos = createTestPosition();
    EXPECT_TRUE(storage.savePosition(equipment.getId(), pos));
    
    // Delete
    EXPECT_TRUE(storage.deleteEquipment(equipment.getId()));
    
    // Verify it's gone
    auto loaded = storage.loadEquipment(equipment.getId());
    EXPECT_FALSE(loaded.has_value());
    
    // Verify position directory is gone
    std::string position_dir = test_db_path + "/positions/" + equipment.getId();
    EXPECT_FALSE(std::filesystem::exists(position_dir));
}

// Test saving and retrieving position history
TEST_F(DataStorageTest, SaveAndRetrievePositionHistory) {
    DataStorage storage(test_db_path);
    storage.initialize();
    
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
    
    // Retrieve all positions
    auto positions = storage.getPositionHistory(id);
    EXPECT_EQ(positions.size(), 3);
    
    // Positions should be sorted by timestamp
    ASSERT_GE(positions.size(), 3);
    EXPECT_NEAR(positions[0].getLatitude(), pos1.getLatitude(), 0.0001);
    EXPECT_NEAR(positions[1].getLatitude(), pos2.getLatitude(), 0.0001);
    EXPECT_NEAR(positions[2].getLatitude(), pos3.getLatitude(), 0.0001);
    
    // Test time range filtering
    auto filtered = storage.getPositionHistory(
        id, 
        now - std::chrono::minutes(90), 
        now - std::chrono::minutes(30)
    );
    
    EXPECT_EQ(filtered.size(), 1);
    ASSERT_GE(filtered.size(), 1);
    EXPECT_NEAR(filtered[0].getLatitude(), pos2.getLatitude(), 0.0001);
}

// Test getting all equipment
TEST_F(DataStorageTest, GetAllEquipment) {
    DataStorage storage(test_db_path);
    storage.initialize();
    
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
    
    // Verify IDs (order may vary)
    std::vector<std::string> ids;
    for (const auto& eq : all) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("id1", "id2", "id3"));
}

// Test finding equipment by status
TEST_F(DataStorageTest, FindEquipmentByStatus) {
    DataStorage storage(test_db_path);
    storage.initialize();
    
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
    ASSERT_GE(maintenance.size(), 1);
    EXPECT_EQ(maintenance[0].getId(), "id2");
}

// Test finding equipment by type
TEST_F(DataStorageTest, FindEquipmentByType) {
    DataStorage storage(test_db_path);
    storage.initialize();
    
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
    ASSERT_GE(cranes.size(), 1);
    EXPECT_EQ(cranes[0].getId(), "id2");
}

// Test finding equipment in area
TEST_F(DataStorageTest, FindEquipmentInArea) {
    DataStorage storage(test_db_path);
    storage.initialize();
    
    // Create equipment with different positions
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    eq1.setLastPosition(Position(37.7749, -122.4194));
    
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    eq2.setLastPosition(Position(37.3382, -121.8863));
    
    Equipment eq3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setLastPosition(Position(37.7749, -122.4150));
    
    EXPECT_TRUE(storage.saveEquipment(eq1));
    EXPECT_TRUE(storage.saveEquipment(eq2));
    EXPECT_TRUE(storage.saveEquipment(eq3));
    
    // Find equipment in San Francisco area
    auto sf_equipment = storage.findEquipmentInArea(
        37.7700, -122.4300,
        37.7800, -122.4100
    );
    
    EXPECT_EQ(sf_equipment.size(), 2);
    
    // Find equipment in San Jose area
    auto sj_equipment = storage.findEquipmentInArea(
        37.3300, -121.9000,
        37.3400, -121.8800
    );
    
    EXPECT_EQ(sj_equipment.size(), 1);
    ASSERT_GE(sj_equipment.size(), 1);
    EXPECT_EQ(sj_equipment[0].getId(), "id2");
}

// Test initialization failure handling
TEST_F(DataStorageTest, InitializationFailureHandling) {
    // Create a path that can't be written to
    std::string invalid_path = "/nonexistent/directory/that/cannot/be/created";
    
    DataStorage storage(invalid_path);
    EXPECT_FALSE(storage.initialize());
}

// Test concurrent access
TEST_F(DataStorageTest, ConcurrentAccess) {
    DataStorage storage(test_db_path);
    storage.initialize();
    
    // Save initial equipment
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(storage.saveEquipment(equipment));
    
    // Create threads that read and write concurrently
    std::vector<std::thread> threads;
    
    // Thread 1: Update equipment name repeatedly
    threads.emplace_back([&storage, id = equipment.getId()]() {
        for (int i = 0; i < 10; i++) {
            auto eq = storage.loadEquipment(id);
            if (eq) {
                eq->setName("Updated Name " + std::to_string(i));
                storage.updateEquipment(*eq);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    
    // Thread 2: Add positions
    threads.emplace_back([&storage, id = equipment.getId()]() {
        for (int i = 0; i < 10; i++) {
            Position pos(37.7749 + i * 0.001, -122.4194 + i * 0.001);
            storage.savePosition(id, pos);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    
    // Thread 3: Read equipment repeatedly
    threads.emplace_back([&storage, id = equipment.getId()]() {
        for (int i = 0; i < 10; i++) {
            storage.loadEquipment(id);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    
    // Thread 4: Get position history repeatedly
    threads.emplace_back([&storage, id = equipment.getId()]() {
        for (int i = 0; i < 10; i++) {
            storage.getPositionHistory(id);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify the equipment still exists and has position history
    auto loaded = storage.loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded.has_value());
    
    auto history = storage.getPositionHistory(equipment.getId());
    EXPECT_GT(history.size(), 0);
}

// Test error handling when saving to invalid location
TEST_F(DataStorageTest, ErrorHandlingWhenSavingToInvalidLocation) {
    // Create a storage with a path that becomes invalid after initialization
    DataStorage storage(test_db_path);
    storage.initialize();
    
    // Make the equipment directory read-only
    std::string equipment_dir = test_db_path + "/equipment";
    std::filesystem::permissions(
        equipment_dir, 
        std::filesystem::perms::owner_read | 
        std::filesystem::perms::group_read | 
        std::filesystem::perms::others_read,
        std::filesystem::perm_options::replace
    );
    
    // Try to save equipment (should fail gracefully)
    Equipment equipment = createTestEquipment();
    EXPECT_FALSE(storage.saveEquipment(equipment));
    
    // Restore permissions for cleanup
    std::filesystem::permissions(
        equipment_dir,
        std::filesystem::perms::owner_read | 
        std::filesystem::perms::owner_write |
        std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace
    );
}

} // namespace equipment_tracker

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>