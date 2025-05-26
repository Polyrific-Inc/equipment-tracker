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
    char buffer[MAX_PATH];
    GetTempPathA(MAX_PATH, buffer);
    return std::string(buffer);
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
        data_storage_ = std::make_unique<DataStorage>(test_db_path_);
        data_storage_->initialize();
    }

    void TearDown() override {
        data_storage_.reset();
        // Clean up the test database file
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove(test_db_path_);
        }
    }

    // Helper method to create a test equipment
    Equipment createTestEquipment(const EquipmentId& id = "EQ123") {
        Equipment equipment;
        equipment.id = id;
        equipment.name = "Test Equipment";
        equipment.type = EquipmentType::VEHICLE;
        equipment.status = EquipmentStatus::ACTIVE;
        equipment.last_maintenance = getCurrentTimestamp() - 86400; // 1 day ago
        return equipment;
    }

    // Helper method to create a test position
    Position createTestPosition(double lat = 37.7749, double lon = -122.4194) {
        Position position;
        position.latitude = lat;
        position.longitude = lon;
        position.altitude = 10.0;
        position.timestamp = getCurrentTimestamp();
        position.accuracy = 5.0;
        return position;
    }

    std::string test_db_path_;
    std::unique_ptr<DataStorage> data_storage_;
};

TEST_F(DataStorageTest, InitializeCreatesDatabase) {
    // Re-initialize to test the initialization process
    EXPECT_TRUE(data_storage_->initialize());
    
    // Verify the database file exists
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    Equipment original = createTestEquipment();
    
    // Save the equipment
    EXPECT_TRUE(data_storage_->saveEquipment(original));
    
    // Load the equipment
    auto loaded = data_storage_->loadEquipment(original.id);
    ASSERT_TRUE(loaded.has_value());
    
    // Verify the loaded equipment matches the original
    EXPECT_EQ(loaded->id, original.id);
    EXPECT_EQ(loaded->name, original.name);
    EXPECT_EQ(loaded->type, original.type);
    EXPECT_EQ(loaded->status, original.status);
    EXPECT_EQ(loaded->last_maintenance, original.last_maintenance);
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    // Try to load equipment that doesn't exist
    auto result = data_storage_->loadEquipment("NONEXISTENT");
    EXPECT_FALSE(result.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    // First save the equipment
    Equipment original = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(original));
    
    // Update the equipment
    original.name = "Updated Equipment";
    original.status = EquipmentStatus::MAINTENANCE;
    EXPECT_TRUE(data_storage_->updateEquipment(original));
    
    // Load and verify the update
    auto loaded = data_storage_->loadEquipment(original.id);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->name, "Updated Equipment");
    EXPECT_EQ(loaded->status, EquipmentStatus::MAINTENANCE);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    // First save the equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Delete the equipment
    EXPECT_TRUE(data_storage_->deleteEquipment(equipment.id));
    
    // Verify it's gone
    auto result = data_storage_->loadEquipment(equipment.id);
    EXPECT_FALSE(result.has_value());
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    // First save an equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Save multiple positions
    Position pos1 = createTestPosition(37.7749, -122.4194);
    Position pos2 = createTestPosition(37.7750, -122.4195);
    Position pos3 = createTestPosition(37.7751, -122.4196);
    
    // Add a small delay between timestamps
    pos1.timestamp = getCurrentTimestamp() - 200;
    pos2.timestamp = getCurrentTimestamp() - 100;
    pos3.timestamp = getCurrentTimestamp();
    
    EXPECT_TRUE(data_storage_->savePosition(equipment.id, pos1));
    EXPECT_TRUE(data_storage_->savePosition(equipment.id, pos2));
    EXPECT_TRUE(data_storage_->savePosition(equipment.id, pos3));
    
    // Get all position history
    auto history = data_storage_->getPositionHistory(equipment.id);
    ASSERT_EQ(history.size(), 3);
    
    // Positions should be in chronological order
    EXPECT_NEAR(history[0].latitude, pos1.latitude, 0.0001);
    EXPECT_NEAR(history[1].latitude, pos2.latitude, 0.0001);
    EXPECT_NEAR(history[2].latitude, pos3.latitude, 0.0001);
}

TEST_F(DataStorageTest, GetPositionHistoryWithTimeRange) {
    // First save an equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Save positions with specific timestamps
    Position pos1, pos2, pos3;
    
    // Set timestamps 300, 200, and 100 seconds ago
    Timestamp now = getCurrentTimestamp();
    pos1.timestamp = now - 300;
    pos2.timestamp = now - 200;
    pos3.timestamp = now - 100;
    
    pos1.latitude = 37.7749;
    pos1.longitude = -122.4194;
    
    pos2.latitude = 37.7750;
    pos2.longitude = -122.4195;
    
    pos3.latitude = 37.7751;
    pos3.longitude = -122.4196;
    
    ASSERT_TRUE(data_storage_->savePosition(equipment.id, pos1));
    ASSERT_TRUE(data_storage_->savePosition(equipment.id, pos2));
    ASSERT_TRUE(data_storage_->savePosition(equipment.id, pos3));
    
    // Get position history with time range (only pos2 and pos3)
    auto history = data_storage_->getPositionHistory(equipment.id, now - 250, now - 50);
    ASSERT_EQ(history.size(), 2);
    
    // Verify the correct positions are returned
    EXPECT_NEAR(history[0].latitude, pos2.latitude, 0.0001);
    EXPECT_NEAR(history[1].latitude, pos3.latitude, 0.0001);
}

TEST_F(DataStorageTest, GetAllEquipment) {
    // Save multiple equipment
    Equipment eq1 = createTestEquipment("EQ1");
    Equipment eq2 = createTestEquipment("EQ2");
    Equipment eq3 = createTestEquipment("EQ3");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Get all equipment
    auto all = data_storage_->getAllEquipment();
    ASSERT_EQ(all.size(), 3);
    
    // Verify all equipment are returned (order may vary)
    std::vector<std::string> ids;
    for (const auto& eq : all) {
        ids.push_back(eq.id);
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("EQ1", "EQ2", "EQ3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    // Save equipment with different statuses
    Equipment eq1 = createTestEquipment("EQ1");
    eq1.status = EquipmentStatus::ACTIVE;
    
    Equipment eq2 = createTestEquipment("EQ2");
    eq2.status = EquipmentStatus::MAINTENANCE;
    
    Equipment eq3 = createTestEquipment("EQ3");
    eq3.status = EquipmentStatus::ACTIVE;
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find active equipment
    auto active = data_storage_->findEquipmentByStatus(EquipmentStatus::ACTIVE);
    ASSERT_EQ(active.size(), 2);
    
    // Find maintenance equipment
    auto maintenance = data_storage_->findEquipmentByStatus(EquipmentStatus::MAINTENANCE);
    ASSERT_EQ(maintenance.size(), 1);
    EXPECT_EQ(maintenance[0].id, "EQ2");
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    // Save equipment with different types
    Equipment eq1 = createTestEquipment("EQ1");
    eq1.type = EquipmentType::VEHICLE;
    
    Equipment eq2 = createTestEquipment("EQ2");
    eq2.type = EquipmentType::TOOL;
    
    Equipment eq3 = createTestEquipment("EQ3");
    eq3.type = EquipmentType::VEHICLE;
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find vehicles
    auto vehicles = data_storage_->findEquipmentByType(EquipmentType::VEHICLE);
    ASSERT_EQ(vehicles.size(), 2);
    
    // Find tools
    auto tools = data_storage_->findEquipmentByType(EquipmentType::TOOL);
    ASSERT_EQ(tools.size(), 1);
    EXPECT_EQ(tools[0].id, "EQ2");
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    // Save equipment with positions
    Equipment eq1 = createTestEquipment("EQ1");
    Equipment eq2 = createTestEquipment("EQ2");
    Equipment eq3 = createTestEquipment("EQ3");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Save positions for each equipment
    Position pos1 = createTestPosition(37.7749, -122.4194); // San Francisco
    Position pos2 = createTestPosition(34.0522, -118.2437); // Los Angeles
    Position pos3 = createTestPosition(40.7128, -74.0060);  // New York
    
    ASSERT_TRUE(data_storage_->savePosition(eq1.id, pos1));
    ASSERT_TRUE(data_storage_->savePosition(eq2.id, pos2));
    ASSERT_TRUE(data_storage_->savePosition(eq3.id, pos3));
    
    // Find equipment in California (covers SF and LA)
    auto california = data_storage_->findEquipmentInArea(
        33.0, -125.0,  // Southwest corner
        42.0, -115.0   // Northeast corner
    );
    
    ASSERT_EQ(california.size(), 2);
    
    // Find equipment in New York area
    auto newYork = data_storage_->findEquipmentInArea(
        40.0, -75.0,   // Southwest corner
        41.0, -73.0    // Northeast corner
    );
    
    ASSERT_EQ(newYork.size(), 1);
    EXPECT_EQ(newYork[0].id, "EQ3");
}

TEST_F(DataStorageTest, UpdateNonExistentEquipment) {
    Equipment nonExistent = createTestEquipment("NONEXISTENT");
    EXPECT_FALSE(data_storage_->updateEquipment(nonExistent));
}

TEST_F(DataStorageTest, DeleteNonExistentEquipment) {
    EXPECT_FALSE(data_storage_->deleteEquipment("NONEXISTENT"));
}

TEST_F(DataStorageTest, SavePositionForNonExistentEquipment) {
    Position position = createTestPosition();
    EXPECT_FALSE(data_storage_->savePosition("NONEXISTENT", position));
}

TEST_F(DataStorageTest, GetPositionHistoryForNonExistentEquipment) {
    auto history = data_storage_->getPositionHistory("NONEXISTENT");
    EXPECT_TRUE(history.empty());
}

TEST_F(DataStorageTest, FindEquipmentInEmptyArea) {
    // Area in the middle of the ocean where no equipment should be
    auto result = data_storage_->findEquipmentInArea(0.0, 0.0, 1.0, 1.0);
    EXPECT_TRUE(result.empty());
}

} // namespace equipment_tracker
// </test_code>