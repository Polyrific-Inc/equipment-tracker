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
        
        // Create the data storage with the test path
        data_storage_ = std::make_unique<DataStorage>(test_db_path_.string());
    }

    void TearDown() override {
        // Clean up the test directory
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove_all(test_db_path_);
        }
    }

    // Helper method to create a test equipment
    Equipment createTestEquipment(const std::string& id = "test-id") {
        Equipment equipment(id, EquipmentType::Forklift, "Test Forklift");
        equipment.setStatus(EquipmentStatus::Active);
        
        // Add a position
        Position position(37.7749, -122.4194, 10.0, 2.0);
        equipment.setLastPosition(position);
        
        return equipment;
    }

    std::filesystem::path test_db_path_;
    std::unique_ptr<DataStorage> data_storage_;
};

TEST_F(DataStorageTest, Initialize) {
    // Test initialization
    EXPECT_TRUE(data_storage_->initialize());
    
    // Verify directories were created
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ / "positions"));
    
    // Test re-initialization
    EXPECT_TRUE(data_storage_->initialize());
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    // Initialize storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create test equipment
    Equipment original = createTestEquipment();
    
    // Save equipment
    EXPECT_TRUE(data_storage_->saveEquipment(original));
    
    // Load equipment
    auto loaded = data_storage_->loadEquipment(original.getId());
    
    // Verify loaded equipment
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getId(), original.getId());
    EXPECT_EQ(loaded->getName(), original.getName());
    EXPECT_EQ(loaded->getType(), original.getType());
    EXPECT_EQ(loaded->getStatus(), original.getStatus());
    
    // Verify position
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
    // Initialize storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Try to load non-existent equipment
    auto loaded = data_storage_->loadEquipment("non-existent-id");
    
    // Verify result
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    // Initialize storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment original = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(original));
    
    // Modify equipment
    Equipment updated = original;
    updated.setName("Updated Forklift");
    updated.setStatus(EquipmentStatus::Maintenance);
    
    // Update equipment
    EXPECT_TRUE(data_storage_->updateEquipment(updated));
    
    // Load equipment
    auto loaded = data_storage_->loadEquipment(original.getId());
    
    // Verify loaded equipment has updated values
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getName(), "Updated Forklift");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::Maintenance);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    // Initialize storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Verify equipment exists
    EXPECT_TRUE(data_storage_->loadEquipment(equipment.getId()).has_value());
    
    // Delete equipment
    EXPECT_TRUE(data_storage_->deleteEquipment(equipment.getId()));
    
    // Verify equipment no longer exists
    EXPECT_FALSE(data_storage_->loadEquipment(equipment.getId()).has_value());
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    // Initialize storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create test equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create test positions with different timestamps
    auto now = std::chrono::system_clock::now();
    Position pos1(37.7749, -122.4194, 10.0, 2.0, now - std::chrono::hours(2));
    Position pos2(37.7750, -122.4195, 11.0, 2.1, now - std::chrono::hours(1));
    Position pos3(37.7751, -122.4196, 12.0, 2.2, now);
    
    // Save positions
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos1));
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos2));
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos3));
    
    // Get position history (all)
    auto history = data_storage_->getPositionHistory(
        equipment.getId(), 
        now - std::chrono::hours(3),
        now + std::chrono::hours(1)
    );
    
    // Verify history
    ASSERT_EQ(history.size(), 3);
    
    // Positions should be sorted by timestamp
    EXPECT_NEAR(history[0].getLatitude(), pos1.getLatitude(), 0.0001);
    EXPECT_NEAR(history[1].getLatitude(), pos2.getLatitude(), 0.0001);
    EXPECT_NEAR(history[2].getLatitude(), pos3.getLatitude(), 0.0001);
    
    // Get position history with time range
    auto filtered_history = data_storage_->getPositionHistory(
        equipment.getId(), 
        now - std::chrono::hours(1) - std::chrono::minutes(30),
        now - std::chrono::minutes(30)
    );
    
    // Verify filtered history
    ASSERT_EQ(filtered_history.size(), 1);
    EXPECT_NEAR(filtered_history[0].getLatitude(), pos2.getLatitude(), 0.0001);
}

TEST_F(DataStorageTest, GetAllEquipment) {
    // Initialize storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save multiple equipment
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Get all equipment
    auto all = data_storage_->getAllEquipment();
    
    // Verify result
    ASSERT_EQ(all.size(), 3);
    
    // Check if all equipment are present (order may vary)
    bool found_eq1 = false, found_eq2 = false, found_eq3 = false;
    
    for (const auto& eq : all) {
        if (eq.getId() == "id1") found_eq1 = true;
        if (eq.getId() == "id2") found_eq2 = true;
        if (eq.getId() == "id3") found_eq3 = true;
    }
    
    EXPECT_TRUE(found_eq1);
    EXPECT_TRUE(found_eq2);
    EXPECT_TRUE(found_eq3);
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    // Initialize storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save equipment with different statuses
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    eq1.setStatus(EquipmentStatus::Active);
    
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    eq2.setStatus(EquipmentStatus::Maintenance);
    
    Equipment eq3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setStatus(EquipmentStatus::Active);
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find equipment by status
    auto active = data_storage_->findEquipmentByStatus(EquipmentStatus::Active);
    auto maintenance = data_storage_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    auto inactive = data_storage_->findEquipmentByStatus(EquipmentStatus::Inactive);
    
    // Verify results
    ASSERT_EQ(active.size(), 2);
    ASSERT_EQ(maintenance.size(), 1);
    ASSERT_EQ(inactive.size(), 0);
    
    // Check active equipment
    bool found_eq1 = false, found_eq3 = false;
    
    for (const auto& eq : active) {
        if (eq.getId() == "id1") found_eq1 = true;
        if (eq.getId() == "id3") found_eq3 = true;
    }
    
    EXPECT_TRUE(found_eq1);
    EXPECT_TRUE(found_eq3);
    
    // Check maintenance equipment
    EXPECT_EQ(maintenance[0].getId(), "id2");
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    // Initialize storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save equipment with different types
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("id3", EquipmentType::Forklift, "Forklift 2");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find equipment by type
    auto forklifts = data_storage_->findEquipmentByType(EquipmentType::Forklift);
    auto cranes = data_storage_->findEquipmentByType(EquipmentType::Crane);
    auto bulldozers = data_storage_->findEquipmentByType(EquipmentType::Bulldozer);
    
    // Verify results
    ASSERT_EQ(forklifts.size(), 2);
    ASSERT_EQ(cranes.size(), 1);
    ASSERT_EQ(bulldozers.size(), 0);
    
    // Check forklift equipment
    bool found_eq1 = false, found_eq3 = false;
    
    for (const auto& eq : forklifts) {
        if (eq.getId() == "id1") found_eq1 = true;
        if (eq.getId() == "id3") found_eq3 = true;
    }
    
    EXPECT_TRUE(found_eq1);
    EXPECT_TRUE(found_eq3);
    
    // Check crane equipment
    EXPECT_EQ(cranes[0].getId(), "id2");
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    // Initialize storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save equipment with different positions
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    eq1.setLastPosition(Position(37.7749, -122.4194)); // San Francisco
    
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    eq2.setLastPosition(Position(40.7128, -74.0060)); // New York
    
    Equipment eq3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setLastPosition(Position(37.7833, -122.4167)); // Also San Francisco
    
    Equipment eq4("id4", EquipmentType::Excavator, "Excavator 1");
    // No position set
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    ASSERT_TRUE(data_storage_->saveEquipment(eq4));
    
    // Find equipment in San Francisco area
    auto sf_area = data_storage_->findEquipmentInArea(
        37.7, -122.5,  // Southwest corner
        37.8, -122.4   // Northeast corner
    );
    
    // Verify results
    ASSERT_EQ(sf_area.size(), 2);
    
    // Check equipment in SF area
    bool found_eq1 = false, found_eq3 = false;
    
    for (const auto& eq : sf_area) {
        if (eq.getId() == "id1") found_eq1 = true;
        if (eq.getId() == "id3") found_eq3 = true;
    }
    
    EXPECT_TRUE(found_eq1);
    EXPECT_TRUE(found_eq3);
    
    // Find equipment in New York area
    auto ny_area = data_storage_->findEquipmentInArea(
        40.7, -74.1,  // Southwest corner
        40.8, -74.0   // Northeast corner
    );
    
    // Verify results
    ASSERT_EQ(ny_area.size(), 1);
    EXPECT_EQ(ny_area[0].getId(), "id2");
    
    // Find equipment in empty area
    auto empty_area = data_storage_->findEquipmentInArea(
        0.0, 0.0,  // Southwest corner
        1.0, 1.0   // Northeast corner
    );
    
    // Verify results
    ASSERT_EQ(empty_area.size(), 0);
}

TEST_F(DataStorageTest, InitializationFailure) {
    // Create a data storage with an invalid path (contains a file that can't be a directory)
    std::string invalid_path = (test_db_path_ / "invalid_file").string();
    
    // Create a file at the path
    std::ofstream file(invalid_path);
    file << "This is a file, not a directory" << std::endl;
    file.close();
    
    // Create data storage with the invalid path
    DataStorage invalid_storage(invalid_path);
    
    // Initialization should fail
    EXPECT_FALSE(invalid_storage.initialize());
}

TEST_F(DataStorageTest, ConcurrentAccess) {
    // Initialize storage
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create test equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create multiple threads to access the data storage concurrently
    std::vector<std::thread> threads;
    const int num_threads = 10;
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([this, i, &equipment]() {
            // Each thread performs a different operation
            switch (i % 5) {
                case 0:
                    // Load equipment
                    data_storage_->loadEquipment(equipment.getId());
                    break;
                case 1:
                    // Update equipment
                    Equipment updated = equipment;
                    updated.setName("Updated " + std::to_string(i));
                    data_storage_->updateEquipment(updated);
                    break;
                case 2:
                    // Save position
                    Position pos(37.7749 + i * 0.001, -122.4194 + i * 0.001);
                    data_storage_->savePosition(equipment.getId(), pos);
                    break;
                case 3:
                    // Get position history
                    data_storage_->getPositionHistory(equipment.getId());
                    break;
                case 4:
                    // Get all equipment
                    data_storage_->getAllEquipment();
                    break;
            }
        });
    }
    
    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // If we got here without exceptions, the test passes
    // We're testing that concurrent access doesn't cause crashes or deadlocks
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>