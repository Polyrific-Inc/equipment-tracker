// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <ctime>
#include <chrono>
#include <thread>
#include "equipment_tracker/data_storage.h"
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/types.h"
#include "equipment_tracker/utils/constants.h"

namespace equipment_tracker {

// Test fixture for DataStorage tests
class DataStorageTest : public ::testing::Test {
protected:
    std::string test_db_path;

    void SetUp() override {
        // Create a unique temporary directory for each test
        test_db_path = "test_db_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    }

    void TearDown() override {
        // Clean up the test database directory
        if (std::filesystem::exists(test_db_path)) {
            std::filesystem::remove_all(test_db_path);
        }
    }

    // Helper method to create a test equipment
    Equipment createTestEquipment(const std::string& id = "test123") {
        Equipment equipment(id, EquipmentType::Forklift, "Test Forklift");
        equipment.setStatus(EquipmentStatus::Active);
        
        // Add a position
        Position position(37.7749, -122.4194, 10.0, 2.0);
        equipment.setLastPosition(position);
        
        return equipment;
    }

    // Helper method to verify equipment equality
    void verifyEquipment(const Equipment& expected, const Equipment& actual) {
        EXPECT_EQ(expected.getId(), actual.getId());
        EXPECT_EQ(expected.getName(), actual.getName());
        EXPECT_EQ(static_cast<int>(expected.getType()), static_cast<int>(actual.getType()));
        EXPECT_EQ(static_cast<int>(expected.getStatus()), static_cast<int>(actual.getStatus()));
        
        auto expectedPos = expected.getLastPosition();
        auto actualPos = actual.getLastPosition();
        
        if (expectedPos && actualPos) {
            EXPECT_DOUBLE_EQ(expectedPos->getLatitude(), actualPos->getLatitude());
            EXPECT_DOUBLE_EQ(expectedPos->getLongitude(), actualPos->getLongitude());
            EXPECT_DOUBLE_EQ(expectedPos->getAltitude(), actualPos->getAltitude());
            EXPECT_DOUBLE_EQ(expectedPos->getAccuracy(), actualPos->getAccuracy());
            
            // Compare timestamps with some tolerance
            auto expected_time = std::chrono::system_clock::to_time_t(expectedPos->getTimestamp());
            auto actual_time = std::chrono::system_clock::to_time_t(actualPos->getTimestamp());
            EXPECT_NEAR(expected_time, actual_time, 2); // Allow 2 seconds difference
        } else {
            EXPECT_EQ(expectedPos.has_value(), actualPos.has_value());
        }
    }
};

// Test initialization
TEST_F(DataStorageTest, Initialize) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Verify directory structure was created
    EXPECT_TRUE(std::filesystem::exists(test_db_path));
    EXPECT_TRUE(std::filesystem::exists(test_db_path + "/equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path + "/positions"));
}

// Test saving and loading equipment
TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Create test equipment
    Equipment original = createTestEquipment();
    
    // Save equipment
    EXPECT_TRUE(storage.saveEquipment(original));
    
    // Load equipment
    auto loaded = storage.loadEquipment(original.getId());
    EXPECT_TRUE(loaded.has_value());
    
    // Verify loaded equipment matches original
    if (loaded) {
        verifyEquipment(original, *loaded);
    }
}

// Test updating equipment
TEST_F(DataStorageTest, UpdateEquipment) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Create and save test equipment
    Equipment original = createTestEquipment();
    EXPECT_TRUE(storage.saveEquipment(original));
    
    // Modify equipment
    Equipment updated = original;
    updated.setName("Updated Forklift");
    updated.setStatus(EquipmentStatus::Maintenance);
    
    // Update equipment
    EXPECT_TRUE(storage.updateEquipment(updated));
    
    // Load equipment
    auto loaded = storage.loadEquipment(original.getId());
    EXPECT_TRUE(loaded.has_value());
    
    // Verify loaded equipment matches updated
    if (loaded) {
        verifyEquipment(updated, *loaded);
    }
}

// Test deleting equipment
TEST_F(DataStorageTest, DeleteEquipment) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Create and save test equipment
    Equipment original = createTestEquipment();
    EXPECT_TRUE(storage.saveEquipment(original));
    
    // Verify equipment exists
    EXPECT_TRUE(storage.loadEquipment(original.getId()).has_value());
    
    // Delete equipment
    EXPECT_TRUE(storage.deleteEquipment(original.getId()));
    
    // Verify equipment no longer exists
    EXPECT_FALSE(storage.loadEquipment(original.getId()).has_value());
}

// Test saving and retrieving position history
TEST_F(DataStorageTest, SaveAndRetrievePositionHistory) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Create test equipment
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(storage.saveEquipment(equipment));
    
    // Create test positions
    std::vector<Position> positions;
    auto now = std::chrono::system_clock::now();
    
    for (int i = 0; i < 5; i++) {
        auto timestamp = now - std::chrono::minutes(i * 10);
        positions.emplace_back(
            37.7749 + (i * 0.001),
            -122.4194 + (i * 0.001),
            10.0 + i,
            2.0,
            timestamp
        );
    }
    
    // Save positions
    for (const auto& pos : positions) {
        EXPECT_TRUE(storage.savePosition(equipment.getId(), pos));
    }
    
    // Retrieve position history
    auto start_time = now - std::chrono::minutes(50);
    auto end_time = now;
    auto history = storage.getPositionHistory(equipment.getId(), start_time, end_time);
    
    // Verify history size
    EXPECT_EQ(positions.size(), history.size());
    
    // Verify positions are sorted by timestamp (oldest to newest)
    if (history.size() >= 2) {
        for (size_t i = 1; i < history.size(); i++) {
            EXPECT_LE(
                std::chrono::system_clock::to_time_t(history[i-1].getTimestamp()),
                std::chrono::system_clock::to_time_t(history[i].getTimestamp())
            );
        }
    }
    
    // Verify position data
    // Note: We can't directly compare with original positions because they might be
    // in a different order, so we'll check that each position has matching values
    for (const auto& original : positions) {
        bool found = false;
        for (const auto& loaded : history) {
            if (std::abs(original.getLatitude() - loaded.getLatitude()) < 0.0001 &&
                std::abs(original.getLongitude() - loaded.getLongitude()) < 0.0001) {
                found = true;
                EXPECT_DOUBLE_EQ(original.getAltitude(), loaded.getAltitude());
                EXPECT_DOUBLE_EQ(original.getAccuracy(), loaded.getAccuracy());
                
                // Compare timestamps with some tolerance
                auto original_time = std::chrono::system_clock::to_time_t(original.getTimestamp());
                auto loaded_time = std::chrono::system_clock::to_time_t(loaded.getTimestamp());
                EXPECT_NEAR(original_time, loaded_time, 2); // Allow 2 seconds difference
                break;
            }
        }
        EXPECT_TRUE(found) << "Position not found in history: " 
                          << original.getLatitude() << ", " 
                          << original.getLongitude();
    }
}

// Test getting all equipment
TEST_F(DataStorageTest, GetAllEquipment) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Create and save multiple equipment
    std::vector<Equipment> originals;
    for (int i = 0; i < 3; i++) {
        std::string id = "test" + std::to_string(i);
        Equipment equipment(id, EquipmentType::Forklift, "Forklift " + std::to_string(i));
        equipment.setStatus(EquipmentStatus::Active);
        
        Position position(37.7749 + (i * 0.001), -122.4194 + (i * 0.001), 10.0 + i, 2.0);
        equipment.setLastPosition(position);
        
        EXPECT_TRUE(storage.saveEquipment(equipment));
        originals.push_back(equipment);
    }
    
    // Get all equipment
    auto all = storage.getAllEquipment();
    
    // Verify count
    EXPECT_EQ(originals.size(), all.size());
    
    // Verify each equipment is in the result
    for (const auto& original : originals) {
        bool found = false;
        for (const auto& loaded : all) {
            if (original.getId() == loaded.getId()) {
                found = true;
                verifyEquipment(original, loaded);
                break;
            }
        }
        EXPECT_TRUE(found) << "Equipment not found: " << original.getId();
    }
}

// Test finding equipment by status
TEST_F(DataStorageTest, FindEquipmentByStatus) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Create and save equipment with different statuses
    Equipment active1("active1", EquipmentType::Forklift, "Active Forklift 1");
    active1.setStatus(EquipmentStatus::Active);
    EXPECT_TRUE(storage.saveEquipment(active1));
    
    Equipment active2("active2", EquipmentType::Crane, "Active Crane");
    active2.setStatus(EquipmentStatus::Active);
    EXPECT_TRUE(storage.saveEquipment(active2));
    
    Equipment maintenance("maint1", EquipmentType::Bulldozer, "Maintenance Bulldozer");
    maintenance.setStatus(EquipmentStatus::Maintenance);
    EXPECT_TRUE(storage.saveEquipment(maintenance));
    
    // Find active equipment
    auto active = storage.findEquipmentByStatus(EquipmentStatus::Active);
    EXPECT_EQ(2, active.size());
    
    // Find maintenance equipment
    auto maint = storage.findEquipmentByStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(1, maint.size());
    
    // Find inactive equipment (should be none)
    auto inactive = storage.findEquipmentByStatus(EquipmentStatus::Inactive);
    EXPECT_EQ(0, inactive.size());
}

// Test finding equipment by type
TEST_F(DataStorageTest, FindEquipmentByType) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Create and save equipment with different types
    Equipment forklift1("fork1", EquipmentType::Forklift, "Forklift 1");
    EXPECT_TRUE(storage.saveEquipment(forklift1));
    
    Equipment forklift2("fork2", EquipmentType::Forklift, "Forklift 2");
    EXPECT_TRUE(storage.saveEquipment(forklift2));
    
    Equipment crane("crane1", EquipmentType::Crane, "Crane 1");
    EXPECT_TRUE(storage.saveEquipment(crane));
    
    // Find forklifts
    auto forklifts = storage.findEquipmentByType(EquipmentType::Forklift);
    EXPECT_EQ(2, forklifts.size());
    
    // Find cranes
    auto cranes = storage.findEquipmentByType(EquipmentType::Crane);
    EXPECT_EQ(1, cranes.size());
    
    // Find excavators (should be none)
    auto excavators = storage.findEquipmentByType(EquipmentType::Excavator);
    EXPECT_EQ(0, excavators.size());
}

// Test finding equipment in area
TEST_F(DataStorageTest, FindEquipmentInArea) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Create and save equipment with different positions
    Equipment eq1("eq1", EquipmentType::Forklift, "Equipment 1");
    eq1.setLastPosition(Position(37.7749, -122.4194, 10.0, 2.0));
    EXPECT_TRUE(storage.saveEquipment(eq1));
    
    Equipment eq2("eq2", EquipmentType::Crane, "Equipment 2");
    eq2.setLastPosition(Position(37.7850, -122.4300, 15.0, 2.0));
    EXPECT_TRUE(storage.saveEquipment(eq2));
    
    Equipment eq3("eq3", EquipmentType::Bulldozer, "Equipment 3");
    eq3.setLastPosition(Position(37.8000, -122.5000, 20.0, 2.0));
    EXPECT_TRUE(storage.saveEquipment(eq3));
    
    // Find equipment in area that includes eq1 and eq2
    auto result1 = storage.findEquipmentInArea(37.7700, -122.4400, 37.7900, -122.4100);
    EXPECT_EQ(2, result1.size());
    
    // Find equipment in area that includes only eq3
    auto result2 = storage.findEquipmentInArea(37.7900, -122.5100, 37.8100, -122.4900);
    EXPECT_EQ(1, result2.size());
    
    // Find equipment in area that includes all
    auto result3 = storage.findEquipmentInArea(37.7700, -122.5100, 37.8100, -122.4100);
    EXPECT_EQ(3, result3.size());
    
    // Find equipment in area that includes none
    auto result4 = storage.findEquipmentInArea(38.0000, -123.0000, 38.1000, -122.9000);
    EXPECT_EQ(0, result4.size());
}

// Test edge case: Initialize with invalid path
TEST_F(DataStorageTest, InitializeWithInvalidPath) {
    // Create a path that should be invalid on most systems
    std::string invalid_path = "/nonexistent/directory/that/should/not/exist/db";
    
    // On Windows, use a different invalid path
    #ifdef _WIN32
    invalid_path = "Z:\\nonexistent\\directory\\that\\should\\not\\exist\\db";
    #endif
    
    DataStorage storage(invalid_path);
    
    // This might succeed or fail depending on permissions, but it shouldn't crash
    storage.initialize();
    
    // Try to save equipment (should fail gracefully)
    Equipment equipment = createTestEquipment();
    EXPECT_FALSE(storage.saveEquipment(equipment));
}

// Test concurrent access
TEST_F(DataStorageTest, ConcurrentAccess) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Create test equipment
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(storage.saveEquipment(equipment));
    
    // Create multiple threads that read and write concurrently
    std::vector<std::thread> threads;
    const int num_threads = 10;
    const int operations_per_thread = 10;
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&storage, i, operations_per_thread]() {
            for (int j = 0; j < operations_per_thread; j++) {
                // Alternate between read and write operations
                if (j % 2 == 0) {
                    // Read operation
                    auto eq = storage.loadEquipment("test123");
                    if (eq) {
                        // Just access some data to ensure it's valid
                        auto name = eq->getName();
                        auto status = eq->getStatus();
                        (void)name;  // Prevent unused variable warnings
                        (void)status;
                    }
                } else {
                    // Write operation
                    Equipment updated("test123", EquipmentType::Forklift, 
                                     "Updated Forklift " + std::to_string(i) + "-" + std::to_string(j));
                    updated.setStatus(EquipmentStatus::Active);
                    storage.updateEquipment(updated);
                }
                
                // Small sleep to increase chance of thread interleaving
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify equipment still exists and is readable
    auto final = storage.loadEquipment("test123");
    EXPECT_TRUE(final.has_value());
}

// Test behavior with empty database
TEST_F(DataStorageTest, EmptyDatabase) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Try to load non-existent equipment
    auto equipment = storage.loadEquipment("nonexistent");
    EXPECT_FALSE(equipment.has_value());
    
    // Try to get position history for non-existent equipment
    auto history = storage.getPositionHistory("nonexistent");
    EXPECT_TRUE(history.empty());
    
    // Try to get all equipment from empty database
    auto all = storage.getAllEquipment();
    EXPECT_TRUE(all.empty());
}

// Test time range filtering for position history
TEST_F(DataStorageTest, PositionHistoryTimeRangeFiltering) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Create test equipment
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(storage.saveEquipment(equipment));
    
    // Create test positions at different times
    auto now = std::chrono::system_clock::now();
    
    // Position 1: 1 hour ago
    Position pos1(37.7749, -122.4194, 10.0, 2.0, now - std::chrono::hours(1));
    EXPECT_TRUE(storage.savePosition(equipment.getId(), pos1));
    
    // Position 2: 30 minutes ago
    Position pos2(37.7750, -122.4195, 11.0, 2.0, now - std::chrono::minutes(30));
    EXPECT_TRUE(storage.savePosition(equipment.getId(), pos2));
    
    // Position 3: 15 minutes ago
    Position pos3(37.7751, -122.4196, 12.0, 2.0, now - std::chrono::minutes(15));
    EXPECT_TRUE(storage.savePosition(equipment.getId(), pos3));
    
    // Get all positions
    auto all_history = storage.getPositionHistory(
        equipment.getId(), 
        now - std::chrono::hours(2), 
        now
    );
    EXPECT_EQ(3, all_history.size());
    
    // Get positions from last 20 minutes
    auto recent_history = storage.getPositionHistory(
        equipment.getId(), 
        now - std::chrono::minutes(20), 
        now
    );
    EXPECT_EQ(1, recent_history.size());
    
    // Get positions from 45 minutes to 15 minutes ago
    auto mid_history = storage.getPositionHistory(
        equipment.getId(), 
        now - std::chrono::minutes(45), 
        now - std::chrono::minutes(15)
    );
    EXPECT_EQ(1, mid_history.size());
}

} // namespace equipment_tracker
// </test_code>