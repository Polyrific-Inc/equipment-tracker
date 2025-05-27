// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <filesystem>
#include "utils/types.h"
#include "utils/constants.h"
#include "equipment.h"
#include "position.h"
#include "data_storage.h"

// For cross-platform temporary directory
std::string getTempDir() {
#ifdef _WIN32
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    return std::string(tempPath);
#else
    return "/tmp/";
#endif
}

namespace equipment_tracker {

class DataStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a unique temporary database path for each test
        test_db_path_ = getTempDir() + "test_equipment_db_" + 
                        std::to_string(testing::UnitTest::GetInstance()->random_seed()) + ".db";
        
        // Create a fresh instance for each test
        storage_ = std::make_unique<DataStorage>(test_db_path_);
        ASSERT_TRUE(storage_->initialize());
    }

    void TearDown() override {
        storage_.reset();
        // Clean up the test database file
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove(test_db_path_);
        }
    }

    // Helper method to create a test equipment
    Equipment createTestEquipment(EquipmentId id = "EQ123") {
        Equipment eq;
        eq.id = id;
        eq.name = "Test Equipment";
        eq.type = EquipmentType::VEHICLE;
        eq.status = EquipmentStatus::ACTIVE;
        eq.last_maintenance = getCurrentTimestamp() - 86400; // 1 day ago
        return eq;
    }

    // Helper method to create a test position
    Position createTestPosition(double lat = 37.7749, double lon = -122.4194) {
        Position pos;
        pos.latitude = lat;
        pos.longitude = lon;
        pos.altitude = 10.0;
        pos.timestamp = getCurrentTimestamp();
        pos.accuracy = 5.0;
        return pos;
    }

    std::string test_db_path_;
    std::unique_ptr<DataStorage> storage_;
};

TEST_F(DataStorageTest, InitializeDatabase) {
    // Test already initialized in SetUp
    EXPECT_TRUE(storage_->initialize());
    
    // Test with invalid path
    DataStorage invalid_storage("/invalid/path/to/db.db");
    EXPECT_FALSE(invalid_storage.initialize());
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    Equipment eq = createTestEquipment();
    
    // Test saving
    EXPECT_TRUE(storage_->saveEquipment(eq));
    
    // Test loading
    auto loaded = storage_->loadEquipment(eq.id);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->id, eq.id);
    EXPECT_EQ(loaded->name, eq.name);
    EXPECT_EQ(loaded->type, eq.type);
    EXPECT_EQ(loaded->status, eq.status);
    EXPECT_EQ(loaded->last_maintenance, eq.last_maintenance);
    
    // Test loading non-existent equipment
    auto not_found = storage_->loadEquipment("NONEXISTENT");
    EXPECT_FALSE(not_found.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    Equipment eq = createTestEquipment();
    ASSERT_TRUE(storage_->saveEquipment(eq));
    
    // Modify and update
    eq.name = "Updated Equipment";
    eq.status = EquipmentStatus::MAINTENANCE;
    EXPECT_TRUE(storage_->updateEquipment(eq));
    
    // Verify update
    auto loaded = storage_->loadEquipment(eq.id);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->name, "Updated Equipment");
    EXPECT_EQ(loaded->status, EquipmentStatus::MAINTENANCE);
    
    // Test updating non-existent equipment
    Equipment nonexistent;
    nonexistent.id = "NONEXISTENT";
    nonexistent.name = "Does Not Exist";
    EXPECT_FALSE(storage_->updateEquipment(nonexistent));
}

TEST_F(DataStorageTest, DeleteEquipment) {
    Equipment eq = createTestEquipment();
    ASSERT_TRUE(storage_->saveEquipment(eq));
    
    // Test deletion
    EXPECT_TRUE(storage_->deleteEquipment(eq.id));
    
    // Verify it's gone
    auto loaded = storage_->loadEquipment(eq.id);
    EXPECT_FALSE(loaded.has_value());
    
    // Test deleting non-existent equipment
    EXPECT_FALSE(storage_->deleteEquipment("NONEXISTENT"));
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    Equipment eq = createTestEquipment();
    ASSERT_TRUE(storage_->saveEquipment(eq));
    
    // Save multiple positions
    Position pos1 = createTestPosition(37.7749, -122.4194);
    pos1.timestamp = getCurrentTimestamp() - 3600; // 1 hour ago
    Position pos2 = createTestPosition(37.7750, -122.4195);
    pos2.timestamp = getCurrentTimestamp() - 1800; // 30 minutes ago
    Position pos3 = createTestPosition(37.7751, -122.4196);
    pos3.timestamp = getCurrentTimestamp();
    
    EXPECT_TRUE(storage_->savePosition(eq.id, pos1));
    EXPECT_TRUE(storage_->savePosition(eq.id, pos2));
    EXPECT_TRUE(storage_->savePosition(eq.id, pos3));
    
    // Get all position history
    auto history = storage_->getPositionHistory(eq.id);
    EXPECT_EQ(history.size(), 3);
    
    // Get position history with time range
    auto recent_history = storage_->getPositionHistory(
        eq.id, 
        getCurrentTimestamp() - 2000, // 33.3 minutes ago
        getCurrentTimestamp()
    );
    EXPECT_EQ(recent_history.size(), 2);
    
    // Test with non-existent equipment
    auto empty_history = storage_->getPositionHistory("NONEXISTENT");
    EXPECT_TRUE(empty_history.empty());
}

TEST_F(DataStorageTest, GetAllEquipment) {
    // Add multiple equipment
    Equipment eq1 = createTestEquipment("EQ1");
    Equipment eq2 = createTestEquipment("EQ2");
    Equipment eq3 = createTestEquipment("EQ3");
    
    ASSERT_TRUE(storage_->saveEquipment(eq1));
    ASSERT_TRUE(storage_->saveEquipment(eq2));
    ASSERT_TRUE(storage_->saveEquipment(eq3));
    
    // Get all equipment
    auto all = storage_->getAllEquipment();
    EXPECT_EQ(all.size(), 3);
    
    // Verify equipment IDs are present
    std::vector<std::string> ids;
    for (const auto& eq : all) {
        ids.push_back(eq.id);
    }
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("EQ1", "EQ2", "EQ3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    // Add equipment with different statuses
    Equipment eq1 = createTestEquipment("EQ1");
    eq1.status = EquipmentStatus::ACTIVE;
    
    Equipment eq2 = createTestEquipment("EQ2");
    eq2.status = EquipmentStatus::MAINTENANCE;
    
    Equipment eq3 = createTestEquipment("EQ3");
    eq3.status = EquipmentStatus::INACTIVE;
    
    Equipment eq4 = createTestEquipment("EQ4");
    eq4.status = EquipmentStatus::ACTIVE;
    
    ASSERT_TRUE(storage_->saveEquipment(eq1));
    ASSERT_TRUE(storage_->saveEquipment(eq2));
    ASSERT_TRUE(storage_->saveEquipment(eq3));
    ASSERT_TRUE(storage_->saveEquipment(eq4));
    
    // Find by status
    auto active = storage_->findEquipmentByStatus(EquipmentStatus::ACTIVE);
    EXPECT_EQ(active.size(), 2);
    
    auto maintenance = storage_->findEquipmentByStatus(EquipmentStatus::MAINTENANCE);
    EXPECT_EQ(maintenance.size(), 1);
    EXPECT_EQ(maintenance[0].id, "EQ2");
    
    auto inactive = storage_->findEquipmentByStatus(EquipmentStatus::INACTIVE);
    EXPECT_EQ(inactive.size(), 1);
    EXPECT_EQ(inactive[0].id, "EQ3");
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    // Add equipment with different types
    Equipment eq1 = createTestEquipment("EQ1");
    eq1.type = EquipmentType::VEHICLE;
    
    Equipment eq2 = createTestEquipment("EQ2");
    eq2.type = EquipmentType::TOOL;
    
    Equipment eq3 = createTestEquipment("EQ3");
    eq3.type = EquipmentType::SENSOR;
    
    Equipment eq4 = createTestEquipment("EQ4");
    eq4.type = EquipmentType::VEHICLE;
    
    ASSERT_TRUE(storage_->saveEquipment(eq1));
    ASSERT_TRUE(storage_->saveEquipment(eq2));
    ASSERT_TRUE(storage_->saveEquipment(eq3));
    ASSERT_TRUE(storage_->saveEquipment(eq4));
    
    // Find by type
    auto vehicles = storage_->findEquipmentByType(EquipmentType::VEHICLE);
    EXPECT_EQ(vehicles.size(), 2);
    
    auto tools = storage_->findEquipmentByType(EquipmentType::TOOL);
    EXPECT_EQ(tools.size(), 1);
    EXPECT_EQ(tools[0].id, "EQ2");
    
    auto sensors = storage_->findEquipmentByType(EquipmentType::SENSOR);
    EXPECT_EQ(sensors.size(), 1);
    EXPECT_EQ(sensors[0].id, "EQ3");
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    Equipment eq1 = createTestEquipment("EQ1");
    Equipment eq2 = createTestEquipment("EQ2");
    Equipment eq3 = createTestEquipment("EQ3");
    
    ASSERT_TRUE(storage_->saveEquipment(eq1));
    ASSERT_TRUE(storage_->saveEquipment(eq2));
    ASSERT_TRUE(storage_->saveEquipment(eq3));
    
    // Save positions for each equipment
    Position pos1 = createTestPosition(37.7749, -122.4194); // San Francisco
    Position pos2 = createTestPosition(34.0522, -118.2437); // Los Angeles
    Position pos3 = createTestPosition(40.7128, -74.0060);  // New York
    
    ASSERT_TRUE(storage_->savePosition(eq1.id, pos1));
    ASSERT_TRUE(storage_->savePosition(eq2.id, pos2));
    ASSERT_TRUE(storage_->savePosition(eq3.id, pos3));
    
    // Find equipment in San Francisco area
    auto sf_area = storage_->findEquipmentInArea(
        37.70, -122.50,  // Southwest corner
        37.80, -122.40   // Northeast corner
    );
    EXPECT_EQ(sf_area.size(), 1);
    EXPECT_EQ(sf_area[0].id, "EQ1");
    
    // Find equipment in California
    auto california = storage_->findEquipmentInArea(
        32.0, -125.0,  // Southwest corner
        42.0, -114.0   // Northeast corner
    );
    EXPECT_EQ(california.size(), 2);
    
    // Find equipment in empty area
    auto empty_area = storage_->findEquipmentInArea(
        0.0, 0.0,  // Southwest corner
        1.0, 1.0   // Northeast corner
    );
    EXPECT_TRUE(empty_area.empty());
}

TEST_F(DataStorageTest, ConcurrentAccess) {
    // This test verifies that the mutex protects against concurrent access
    // by simulating multiple threads accessing the database
    
    Equipment eq = createTestEquipment();
    ASSERT_TRUE(storage_->saveEquipment(eq));
    
    // Create multiple threads that read and write to the database
    std::vector<std::thread> threads;
    const int num_threads = 10;
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([this, i, &eq]() {
            // Each thread does a mix of operations
            if (i % 3 == 0) {
                // Update equipment
                Equipment updated = eq;
                updated.name = "Updated " + std::to_string(i);
                storage_->updateEquipment(updated);
            } else if (i % 3 == 1) {
                // Read equipment
                auto loaded = storage_->loadEquipment(eq.id);
                EXPECT_TRUE(loaded.has_value());
            } else {
                // Save position
                Position pos = createTestPosition(37.7 + i * 0.01, -122.4 + i * 0.01);
                storage_->savePosition(eq.id, pos);
            }
        });
    }
    
    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify the equipment still exists
    auto loaded = storage_->loadEquipment(eq.id);
    EXPECT_TRUE(loaded.has_value());
    
    // Verify position history was saved
    auto history = storage_->getPositionHistory(eq.id);
    EXPECT_FALSE(history.empty());
}

} // namespace equipment_tracker
// </test_code>