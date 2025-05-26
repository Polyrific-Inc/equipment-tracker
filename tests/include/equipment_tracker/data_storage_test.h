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
        temp_db_path_ = getTempDir() + "test_equipment_db_" + 
                        std::to_string(testing::UnitTest::GetInstance()->random_seed()) + ".db";
        
        // Create a fresh instance for each test
        data_storage_ = std::make_unique<DataStorage>(temp_db_path_);
        data_storage_->initialize();
    }

    void TearDown() override {
        data_storage_.reset();
        // Clean up the test database file
        if (std::filesystem::exists(temp_db_path_)) {
            std::filesystem::remove(temp_db_path_);
        }
    }

    // Helper method to create a test equipment
    Equipment createTestEquipment(EquipmentId id = "EQ001") {
        Equipment equipment;
        equipment.id = id;
        equipment.name = "Test Equipment";
        equipment.type = EquipmentType::VEHICLE;
        equipment.status = EquipmentStatus::ACTIVE;
        equipment.last_maintenance = getCurrentTimestamp() - 86400; // Yesterday
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

    std::string temp_db_path_;
    std::unique_ptr<DataStorage> data_storage_;
};

TEST_F(DataStorageTest, InitializeDatabase) {
    // Re-initialize should also work
    EXPECT_TRUE(data_storage_->initialize());
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    Equipment equipment = createTestEquipment();
    
    // Save equipment
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Load equipment
    auto loaded = data_storage_->loadEquipment(equipment.id);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->id, equipment.id);
    EXPECT_EQ(loaded->name, equipment.name);
    EXPECT_EQ(loaded->type, equipment.type);
    EXPECT_EQ(loaded->status, equipment.status);
    EXPECT_EQ(loaded->last_maintenance, equipment.last_maintenance);
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    auto loaded = data_storage_->loadEquipment("NONEXISTENT");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    Equipment equipment = createTestEquipment();
    
    // Save initial equipment
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Update equipment
    equipment.name = "Updated Equipment";
    equipment.status = EquipmentStatus::MAINTENANCE;
    EXPECT_TRUE(data_storage_->updateEquipment(equipment));
    
    // Verify update
    auto loaded = data_storage_->loadEquipment(equipment.id);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->name, "Updated Equipment");
    EXPECT_EQ(loaded->status, EquipmentStatus::MAINTENANCE);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    Equipment equipment = createTestEquipment();
    
    // Save equipment
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Delete equipment
    EXPECT_TRUE(data_storage_->deleteEquipment(equipment.id));
    
    // Verify deletion
    auto loaded = data_storage_->loadEquipment(equipment.id);
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Save multiple positions
    Position pos1 = createTestPosition(37.7749, -122.4194);
    pos1.timestamp = getCurrentTimestamp() - 3600; // 1 hour ago
    Position pos2 = createTestPosition(37.7750, -122.4195);
    pos2.timestamp = getCurrentTimestamp() - 1800; // 30 minutes ago
    Position pos3 = createTestPosition(37.7751, -122.4196);
    pos3.timestamp = getCurrentTimestamp(); // Now
    
    EXPECT_TRUE(data_storage_->savePosition(equipment.id, pos1));
    EXPECT_TRUE(data_storage_->savePosition(equipment.id, pos2));
    EXPECT_TRUE(data_storage_->savePosition(equipment.id, pos3));
    
    // Get full history
    auto history = data_storage_->getPositionHistory(equipment.id);
    EXPECT_EQ(history.size(), 3);
    
    // Get history with time range
    auto recent_history = data_storage_->getPositionHistory(
        equipment.id, 
        getCurrentTimestamp() - 2000, // 33.3 minutes ago
        getCurrentTimestamp()
    );
    EXPECT_EQ(recent_history.size(), 2);
}

TEST_F(DataStorageTest, GetAllEquipment) {
    // Save multiple equipment
    Equipment eq1 = createTestEquipment("EQ001");
    Equipment eq2 = createTestEquipment("EQ002");
    Equipment eq3 = createTestEquipment("EQ003");
    
    EXPECT_TRUE(data_storage_->saveEquipment(eq1));
    EXPECT_TRUE(data_storage_->saveEquipment(eq2));
    EXPECT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Get all equipment
    auto all = data_storage_->getAllEquipment();
    EXPECT_EQ(all.size(), 3);
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    // Save equipment with different statuses
    Equipment eq1 = createTestEquipment("EQ001");
    eq1.status = EquipmentStatus::ACTIVE;
    
    Equipment eq2 = createTestEquipment("EQ002");
    eq2.status = EquipmentStatus::MAINTENANCE;
    
    Equipment eq3 = createTestEquipment("EQ003");
    eq3.status = EquipmentStatus::ACTIVE;
    
    EXPECT_TRUE(data_storage_->saveEquipment(eq1));
    EXPECT_TRUE(data_storage_->saveEquipment(eq2));
    EXPECT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find by status
    auto active = data_storage_->findEquipmentByStatus(EquipmentStatus::ACTIVE);
    EXPECT_EQ(active.size(), 2);
    
    auto maintenance = data_storage_->findEquipmentByStatus(EquipmentStatus::MAINTENANCE);
    EXPECT_EQ(maintenance.size(), 1);
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    // Save equipment with different types
    Equipment eq1 = createTestEquipment("EQ001");
    eq1.type = EquipmentType::VEHICLE;
    
    Equipment eq2 = createTestEquipment("EQ002");
    eq2.type = EquipmentType::TOOL;
    
    Equipment eq3 = createTestEquipment("EQ003");
    eq3.type = EquipmentType::VEHICLE;
    
    EXPECT_TRUE(data_storage_->saveEquipment(eq1));
    EXPECT_TRUE(data_storage_->saveEquipment(eq2));
    EXPECT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Find by type
    auto vehicles = data_storage_->findEquipmentByType(EquipmentType::VEHICLE);
    EXPECT_EQ(vehicles.size(), 2);
    
    auto tools = data_storage_->findEquipmentByType(EquipmentType::TOOL);
    EXPECT_EQ(tools.size(), 1);
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    // Save equipment with positions in different areas
    Equipment eq1 = createTestEquipment("EQ001");
    Equipment eq2 = createTestEquipment("EQ002");
    Equipment eq3 = createTestEquipment("EQ003");
    
    EXPECT_TRUE(data_storage_->saveEquipment(eq1));
    EXPECT_TRUE(data_storage_->saveEquipment(eq2));
    EXPECT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Save positions
    Position pos1 = createTestPosition(37.7749, -122.4194); // San Francisco
    Position pos2 = createTestPosition(34.0522, -118.2437); // Los Angeles
    Position pos3 = createTestPosition(37.7750, -122.4195); // Also San Francisco
    
    EXPECT_TRUE(data_storage_->savePosition(eq1.id, pos1));
    EXPECT_TRUE(data_storage_->savePosition(eq2.id, pos2));
    EXPECT_TRUE(data_storage_->savePosition(eq3.id, pos3));
    
    // Find in SF area
    auto sf_equipment = data_storage_->findEquipmentInArea(
        37.7, -122.5,  // Southwest corner
        37.8, -122.4   // Northeast corner
    );
    EXPECT_EQ(sf_equipment.size(), 2);
    
    // Find in LA area
    auto la_equipment = data_storage_->findEquipmentInArea(
        34.0, -118.3,  // Southwest corner
        34.1, -118.2   // Northeast corner
    );
    EXPECT_EQ(la_equipment.size(), 1);
}

TEST_F(DataStorageTest, EmptyPositionHistory) {
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Get history for equipment with no positions
    auto history = data_storage_->getPositionHistory(equipment.id);
    EXPECT_TRUE(history.empty());
}

TEST_F(DataStorageTest, NonExistentEquipmentPositionHistory) {
    // Get history for non-existent equipment
    auto history = data_storage_->getPositionHistory("NONEXISTENT");
    EXPECT_TRUE(history.empty());
}

TEST_F(DataStorageTest, UpdateNonExistentEquipment) {
    Equipment equipment = createTestEquipment("NONEXISTENT");
    EXPECT_FALSE(data_storage_->updateEquipment(equipment));
}

TEST_F(DataStorageTest, DeleteNonExistentEquipment) {
    EXPECT_FALSE(data_storage_->deleteEquipment("NONEXISTENT"));
}

} // namespace equipment_tracker
// </test_code>