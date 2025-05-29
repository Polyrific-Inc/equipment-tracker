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

using namespace equipment_tracker;
using namespace testing;

class DataStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for testing
        test_db_path = std::filesystem::temp_directory_path() / "equipment_tracker_test";
        
        // Clean up any existing test directory
        if (std::filesystem::exists(test_db_path)) {
            std::filesystem::remove_all(test_db_path);
        }
        
        // Create storage with test path
        storage = std::make_unique<DataStorage>(test_db_path.string());
    }

    void TearDown() override {
        // Clean up test directory
        storage.reset();
        if (std::filesystem::exists(test_db_path)) {
            std::filesystem::remove_all(test_db_path);
        }
    }

    // Helper method to create a test equipment
    Equipment createTestEquipment(const std::string& id = "test123") {
        Equipment equipment(id, EquipmentType::Forklift, "Test Forklift");
        equipment.setStatus(EquipmentStatus::Active);
        
        // Add a position
        Position pos(37.7749, -122.4194, 10.0, 5.0);
        equipment.setLastPosition(pos);
        
        return equipment;
    }

    // Helper method to create a position at a specific time
    Position createPosition(double lat, double lon, double alt, 
                           std::chrono::system_clock::time_point time = std::chrono::system_clock::now()) {
        return Position(lat, lon, alt, DEFAULT_POSITION_ACCURACY, time);
    }

    std::filesystem::path test_db_path;
    std::unique_ptr<DataStorage> storage;
};

TEST_F(DataStorageTest, InitializeCreatesDirectories) {
    ASSERT_TRUE(storage->initialize());
    ASSERT_TRUE(std::filesystem::exists(test_db_path));
    ASSERT_TRUE(std::filesystem::exists(test_db_path / "equipment"));
    ASSERT_TRUE(std::filesystem::exists(test_db_path / "positions"));
}

TEST_F(DataStorageTest, InitializeIsIdempotent) {
    ASSERT_TRUE(storage->initialize());
    ASSERT_TRUE(storage->initialize()); // Second call should also return true
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    ASSERT_TRUE(storage->initialize());
    
    Equipment original = createTestEquipment();
    ASSERT_TRUE(storage->saveEquipment(original));
    
    auto loaded = storage->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    
    EXPECT_EQ(loaded->getId(), original.getId());
    EXPECT_EQ(loaded->getName(), original.getName());
    EXPECT_EQ(static_cast<int>(loaded->getType()), static_cast<int>(original.getType()));
    EXPECT_EQ(static_cast<int>(loaded->getStatus()), static_cast<int>(original.getStatus()));
    
    // Check position
    auto original_pos = original.getLastPosition();
    auto loaded_pos = loaded->getLastPosition();
    ASSERT_TRUE(loaded_pos.has_value());
    EXPECT_NEAR(loaded_pos->getLatitude(), original_pos->getLatitude(), 1e-10);
    EXPECT_NEAR(loaded_pos->getLongitude(), original_pos->getLongitude(), 1e-10);
    EXPECT_NEAR(loaded_pos->getAltitude(), original_pos->getAltitude(), 1e-10);
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    ASSERT_TRUE(storage->initialize());
    auto loaded = storage->loadEquipment("nonexistent");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    ASSERT_TRUE(storage->initialize());
    
    Equipment original = createTestEquipment();
    ASSERT_TRUE(storage->saveEquipment(original));
    
    // Modify and update
    original.setName("Updated Forklift");
    original.setStatus(EquipmentStatus::Maintenance);
    ASSERT_TRUE(storage->updateEquipment(original));
    
    // Load and verify
    auto loaded = storage->loadEquipment(original.getId());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getName(), "Updated Forklift");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::Maintenance);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    ASSERT_TRUE(storage->initialize());
    
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(storage->saveEquipment(equipment));
    
    // Verify it exists
    ASSERT_TRUE(storage->loadEquipment(equipment.getId()).has_value());
    
    // Delete it
    ASSERT_TRUE(storage->deleteEquipment(equipment.getId()));
    
    // Verify it's gone
    EXPECT_FALSE(storage->loadEquipment(equipment.getId()).has_value());
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    ASSERT_TRUE(storage->initialize());
    
    std::string id = "test456";
    
    // Create positions at different times
    auto now = std::chrono::system_clock::now();
    auto time1 = now - std::chrono::hours(2);
    auto time2 = now - std::chrono::hours(1);
    auto time3 = now;
    
    Position pos1(37.7749, -122.4194, 10.0, 5.0, time1);
    Position pos2(37.7750, -122.4195, 11.0, 5.0, time2);
    Position pos3(37.7751, -122.4196, 12.0, 5.0, time3);
    
    // Save positions
    ASSERT_TRUE(storage->savePosition(id, pos1));
    ASSERT_TRUE(storage->savePosition(id, pos2));
    ASSERT_TRUE(storage->savePosition(id, pos3));
    
    // Get all history
    auto history = storage->getPositionHistory(id, time1, time3);
    ASSERT_EQ(history.size(), 3);
    
    // Get partial history
    auto partial = storage->getPositionHistory(id, time2, time3);
    ASSERT_EQ(partial.size(), 2);
    
    // Verify order and values
    EXPECT_NEAR(history[0].getLatitude(), pos1.getLatitude(), 1e-10);
    EXPECT_NEAR(history[1].getLatitude(), pos2.getLatitude(), 1e-10);
    EXPECT_NEAR(history[2].getLatitude(), pos3.getLatitude(), 1e-10);
}

TEST_F(DataStorageTest, GetAllEquipment) {
    ASSERT_TRUE(storage->initialize());
    
    // Save multiple equipment
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    
    ASSERT_TRUE(storage->saveEquipment(eq1));
    ASSERT_TRUE(storage->saveEquipment(eq2));
    ASSERT_TRUE(storage->saveEquipment(eq3));
    
    // Get all equipment
    auto all = storage->getAllEquipment();
    ASSERT_EQ(all.size(), 3);
    
    // Verify IDs (order may vary)
    std::vector<std::string> ids;
    for (const auto& eq : all) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, UnorderedElementsAre("id1", "id2", "id3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    ASSERT_TRUE(storage->initialize());
    
    // Create equipment with different statuses
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    eq1.setStatus(EquipmentStatus::Active);
    
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    eq2.setStatus(EquipmentStatus::Maintenance);
    
    Equipment eq3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setStatus(EquipmentStatus::Active);
    
    ASSERT_TRUE(storage->saveEquipment(eq1));
    ASSERT_TRUE(storage->saveEquipment(eq2));
    ASSERT_TRUE(storage->saveEquipment(eq3));
    
    // Find active equipment
    auto active = storage->findEquipmentByStatus(EquipmentStatus::Active);
    ASSERT_EQ(active.size(), 2);
    
    // Find maintenance equipment
    auto maintenance = storage->findEquipmentByStatus(EquipmentStatus::Maintenance);
    ASSERT_EQ(maintenance.size(), 1);
    EXPECT_EQ(maintenance[0].getId(), "id2");
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    ASSERT_TRUE(storage->initialize());
    
    // Create equipment with different types
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("id3", EquipmentType::Forklift, "Forklift 2");
    
    ASSERT_TRUE(storage->saveEquipment(eq1));
    ASSERT_TRUE(storage->saveEquipment(eq2));
    ASSERT_TRUE(storage->saveEquipment(eq3));
    
    // Find forklifts
    auto forklifts = storage->findEquipmentByType(EquipmentType::Forklift);
    ASSERT_EQ(forklifts.size(), 2);
    
    // Find cranes
    auto cranes = storage->findEquipmentByType(EquipmentType::Crane);
    ASSERT_EQ(cranes.size(), 1);
    EXPECT_EQ(cranes[0].getId(), "id2");
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    ASSERT_TRUE(storage->initialize());
    
    // Create equipment with different positions
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    eq1.setLastPosition(Position(37.7749, -122.4194));
    
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    eq2.setLastPosition(Position(37.3382, -121.8863));
    
    Equipment eq3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setLastPosition(Position(37.7749, -122.4195));
    
    ASSERT_TRUE(storage->saveEquipment(eq1));
    ASSERT_TRUE(storage->saveEquipment(eq2));
    ASSERT_TRUE(storage->saveEquipment(eq3));
    
    // Find equipment in San Francisco area
    auto sf_equipment = storage->findEquipmentInArea(37.7, -122.5, 37.8, -122.4);
    ASSERT_EQ(sf_equipment.size(), 2);
    
    // Find equipment in San Jose area
    auto sj_equipment = storage->findEquipmentInArea(37.3, -122.0, 37.4, -121.8);
    ASSERT_EQ(sj_equipment.size(), 1);
    EXPECT_EQ(sj_equipment[0].getId(), "id2");
}

TEST_F(DataStorageTest, SaveEquipmentWithoutInitialization) {
    // Don't call initialize() first
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(storage->saveEquipment(equipment));
    
    // Verify it was saved
    auto loaded = storage->loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded.has_value());
}

TEST_F(DataStorageTest, SavePositionWithoutInitialization) {
    // Don't call initialize() first
    std::string id = "test789";
    Position pos(37.7749, -122.4194, 10.0);
    
    ASSERT_TRUE(storage->savePosition(id, pos));
    
    // Verify it was saved
    auto history = storage->getPositionHistory(id);
    ASSERT_EQ(history.size(), 1);
}

TEST_F(DataStorageTest, EquipmentWithPositionHistory) {
    ASSERT_TRUE(storage->initialize());
    
    std::string id = "test_history";
    Equipment equipment(id, EquipmentType::Excavator, "Excavator with History");
    
    // Save equipment first
    ASSERT_TRUE(storage->saveEquipment(equipment));
    
    // Add position history
    auto now = std::chrono::system_clock::now();
    for (int i = 0; i < 5; i++) {
        auto time = now - std::chrono::minutes(i * 10);
        Position pos(37.7749 + i * 0.001, -122.4194 + i * 0.001, 10.0 + i, 5.0, time);
        ASSERT_TRUE(storage->savePosition(id, pos));
    }
    
    // Load equipment and check if history is loaded
    auto loaded = storage->loadEquipment(id);
    ASSERT_TRUE(loaded.has_value());
    
    auto history = loaded->getPositionHistory();
    ASSERT_EQ(history.size(), 5);
}

TEST_F(DataStorageTest, DeleteNonExistentEquipment) {
    ASSERT_TRUE(storage->initialize());
    // Should return true even if equipment doesn't exist
    EXPECT_TRUE(storage->deleteEquipment("nonexistent"));
}

TEST_F(DataStorageTest, GetPositionHistoryForNonExistentEquipment) {
    ASSERT_TRUE(storage->initialize());
    auto history = storage->getPositionHistory("nonexistent");
    EXPECT_TRUE(history.empty());
}

TEST_F(DataStorageTest, ConcurrentAccess) {
    ASSERT_TRUE(storage->initialize());
    
    // Create test equipment
    Equipment equipment = createTestEquipment("concurrent_test");
    ASSERT_TRUE(storage->saveEquipment(equipment));
    
    // Create threads that read and write concurrently
    std::vector<std::thread> threads;
    
    // Thread 1: Update equipment
    threads.emplace_back([this, &equipment]() {
        for (int i = 0; i < 10; i++) {
            equipment.setName("Updated " + std::to_string(i));
            storage->updateEquipment(equipment);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    
    // Thread 2: Save positions
    threads.emplace_back([this]() {
        for (int i = 0; i < 10; i++) {
            Position pos(37.7749 + i * 0.001, -122.4194 + i * 0.001, 10.0 + i);
            storage->savePosition("concurrent_test", pos);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    
    // Thread 3: Read equipment
    threads.emplace_back([this]() {
        for (int i = 0; i < 10; i++) {
            auto loaded = storage->loadEquipment("concurrent_test");
            EXPECT_TRUE(loaded.has_value());
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    
    // Thread 4: Read position history
    threads.emplace_back([this]() {
        for (int i = 0; i < 10; i++) {
            auto history = storage->getPositionHistory("concurrent_test");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify equipment still exists and has position history
    auto loaded = storage->loadEquipment("concurrent_test");
    ASSERT_TRUE(loaded.has_value());
    
    auto history = storage->getPositionHistory("concurrent_test");
    EXPECT_GT(history.size(), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>