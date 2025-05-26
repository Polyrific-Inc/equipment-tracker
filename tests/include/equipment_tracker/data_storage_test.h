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

namespace equipment_tracker {
namespace testing {

// Mock getCurrentTimestamp for testing
Timestamp testTimestamp() {
    return Timestamp(1609459200); // 2021-01-01 00:00:00 UTC
}

class DataStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use a temporary file for testing
        temp_db_path_ = std::filesystem::temp_directory_path() / "test_equipment_db.sqlite";
        
        // Remove any existing test database
        if (std::filesystem::exists(temp_db_path_)) {
            std::filesystem::remove(temp_db_path_);
        }
        
        storage_ = std::make_unique<DataStorage>(temp_db_path_.string());
        ASSERT_TRUE(storage_->initialize());
    }

    void TearDown() override {
        storage_.reset();
        // Clean up the test database
        if (std::filesystem::exists(temp_db_path_)) {
            std::filesystem::remove(temp_db_path_);
        }
    }

    // Helper method to create a test equipment
    Equipment createTestEquipment(const EquipmentId& id = "EQ001") {
        Equipment equipment;
        equipment.id = id;
        equipment.name = "Test Equipment";
        equipment.type = EquipmentType::BULLDOZER;
        equipment.status = EquipmentStatus::AVAILABLE;
        equipment.last_maintenance = Timestamp(1609372800); // 2020-12-31
        return equipment;
    }

    // Helper method to create a test position
    Position createTestPosition(double lat = 37.7749, double lon = -122.4194) {
        Position position;
        position.latitude = lat;
        position.longitude = lon;
        position.altitude = 10.0;
        position.timestamp = testTimestamp();
        position.accuracy = 5.0;
        return position;
    }

    std::filesystem::path temp_db_path_;
    std::unique_ptr<DataStorage> storage_;
};

TEST_F(DataStorageTest, InitializeDatabase) {
    // Already initialized in SetUp, just verify it worked
    EXPECT_TRUE(storage_->initialize());
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    Equipment original = createTestEquipment();
    
    // Save equipment
    EXPECT_TRUE(storage_->saveEquipment(original));
    
    // Load equipment
    auto loaded = storage_->loadEquipment(original.id);
    ASSERT_TRUE(loaded.has_value());
    
    // Verify loaded equipment matches original
    EXPECT_EQ(loaded->id, original.id);
    EXPECT_EQ(loaded->name, original.name);
    EXPECT_EQ(loaded->type, original.type);
    EXPECT_EQ(loaded->status, original.status);
    EXPECT_EQ(loaded->last_maintenance, original.last_maintenance);
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    auto result = storage_->loadEquipment("NONEXISTENT");
    EXPECT_FALSE(result.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    // First save equipment
    Equipment original = createTestEquipment();
    ASSERT_TRUE(storage_->saveEquipment(original));
    
    // Update equipment
    original.name = "Updated Equipment";
    original.status = EquipmentStatus::IN_USE;
    EXPECT_TRUE(storage_->updateEquipment(original));
    
    // Verify update
    auto loaded = storage_->loadEquipment(original.id);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->name, "Updated Equipment");
    EXPECT_EQ(loaded->status, EquipmentStatus::IN_USE);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    // First save equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(storage_->saveEquipment(equipment));
    
    // Delete equipment
    EXPECT_TRUE(storage_->deleteEquipment(equipment.id));
    
    // Verify deletion
    auto result = storage_->loadEquipment(equipment.id);
    EXPECT_FALSE(result.has_value());
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    // First save equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(storage_->saveEquipment(equipment));
    
    // Save positions
    Position pos1 = createTestPosition(37.7749, -122.4194);
    Position pos2 = createTestPosition(37.7750, -122.4195);
    Position pos3 = createTestPosition(37.7751, -122.4196);
    
    EXPECT_TRUE(storage_->savePosition(equipment.id, pos1));
    EXPECT_TRUE(storage_->savePosition(equipment.id, pos2));
    EXPECT_TRUE(storage_->savePosition(equipment.id, pos3));
    
    // Get position history
    auto positions = storage_->getPositionHistory(equipment.id);
    ASSERT_EQ(positions.size(), 3);
    
    // Verify positions are in correct order (newest first)
    EXPECT_NEAR(positions[0].latitude, pos3.latitude, 0.0001);
    EXPECT_NEAR(positions[0].longitude, pos3.longitude, 0.0001);
    EXPECT_NEAR(positions[1].latitude, pos2.latitude, 0.0001);
    EXPECT_NEAR(positions[1].longitude, pos2.longitude, 0.0001);
    EXPECT_NEAR(positions[2].latitude, pos1.latitude, 0.0001);
    EXPECT_NEAR(positions[2].longitude, pos1.longitude, 0.0001);
}

TEST_F(DataStorageTest, GetPositionHistoryWithTimeRange) {
    // First save equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(storage_->saveEquipment(equipment));
    
    // Save positions with different timestamps
    Position pos1, pos2, pos3;
    pos1 = createTestPosition();
    pos1.timestamp = Timestamp(1609372800); // 2020-12-31
    
    pos2 = createTestPosition();
    pos2.timestamp = Timestamp(1609459200); // 2021-01-01
    
    pos3 = createTestPosition();
    pos3.timestamp = Timestamp(1609545600); // 2021-01-02
    
    EXPECT_TRUE(storage_->savePosition(equipment.id, pos1));
    EXPECT_TRUE(storage_->savePosition(equipment.id, pos2));
    EXPECT_TRUE(storage_->savePosition(equipment.id, pos3));
    
    // Get position history with time range
    auto positions = storage_->getPositionHistory(
        equipment.id, 
        Timestamp(1609459200), // 2021-01-01
        Timestamp(1609545600)  // 2021-01-02
    );
    
    ASSERT_EQ(positions.size(), 2);
    EXPECT_EQ(positions[0].timestamp, pos3.timestamp);
    EXPECT_EQ(positions[1].timestamp, pos2.timestamp);
}

TEST_F(DataStorageTest, GetAllEquipment) {
    // Save multiple equipment
    Equipment eq1 = createTestEquipment("EQ001");
    Equipment eq2 = createTestEquipment("EQ002");
    Equipment eq3 = createTestEquipment("EQ003");
    
    ASSERT_TRUE(storage_->saveEquipment(eq1));
    ASSERT_TRUE(storage_->saveEquipment(eq2));
    ASSERT_TRUE(storage_->saveEquipment(eq3));
    
    // Get all equipment
    auto equipment = storage_->getAllEquipment();
    ASSERT_EQ(equipment.size(), 3);
    
    // Verify equipment IDs (order may vary)
    std::vector<std::string> ids;
    for (const auto& eq : equipment) {
        ids.push_back(eq.id);
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("EQ001", "EQ002", "EQ003"));
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    // Save equipment with different statuses
    Equipment eq1 = createTestEquipment("EQ001");
    eq1.status = EquipmentStatus::AVAILABLE;
    
    Equipment eq2 = createTestEquipment("EQ002");
    eq2.status = EquipmentStatus::IN_USE;
    
    Equipment eq3 = createTestEquipment("EQ003");
    eq3.status = EquipmentStatus::MAINTENANCE;
    
    Equipment eq4 = createTestEquipment("EQ004");
    eq4.status = EquipmentStatus::IN_USE;
    
    ASSERT_TRUE(storage_->saveEquipment(eq1));
    ASSERT_TRUE(storage_->saveEquipment(eq2));
    ASSERT_TRUE(storage_->saveEquipment(eq3));
    ASSERT_TRUE(storage_->saveEquipment(eq4));
    
    // Find equipment by status
    auto inUseEquipment = storage_->findEquipmentByStatus(EquipmentStatus::IN_USE);
    ASSERT_EQ(inUseEquipment.size(), 2);
    
    // Verify equipment IDs
    std::vector<std::string> ids;
    for (const auto& eq : inUseEquipment) {
        ids.push_back(eq.id);
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("EQ002", "EQ004"));
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    // Save equipment with different types
    Equipment eq1 = createTestEquipment("EQ001");
    eq1.type = EquipmentType::BULLDOZER;
    
    Equipment eq2 = createTestEquipment("EQ002");
    eq2.type = EquipmentType::EXCAVATOR;
    
    Equipment eq3 = createTestEquipment("EQ003");
    eq3.type = EquipmentType::BULLDOZER;
    
    ASSERT_TRUE(storage_->saveEquipment(eq1));
    ASSERT_TRUE(storage_->saveEquipment(eq2));
    ASSERT_TRUE(storage_->saveEquipment(eq3));
    
    // Find equipment by type
    auto bulldozers = storage_->findEquipmentByType(EquipmentType::BULLDOZER);
    ASSERT_EQ(bulldozers.size(), 2);
    
    // Verify equipment IDs
    std::vector<std::string> ids;
    for (const auto& eq : bulldozers) {
        ids.push_back(eq.id);
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("EQ001", "EQ003"));
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    // First save equipment
    Equipment eq1 = createTestEquipment("EQ001");
    Equipment eq2 = createTestEquipment("EQ002");
    Equipment eq3 = createTestEquipment("EQ003");
    
    ASSERT_TRUE(storage_->saveEquipment(eq1));
    ASSERT_TRUE(storage_->saveEquipment(eq2));
    ASSERT_TRUE(storage_->saveEquipment(eq3));
    
    // Save positions for each equipment
    Position pos1 = createTestPosition(37.7749, -122.4194); // San Francisco
    Position pos2 = createTestPosition(34.0522, -118.2437); // Los Angeles
    Position pos3 = createTestPosition(40.7128, -74.0060);  // New York
    
    EXPECT_TRUE(storage_->savePosition(eq1.id, pos1));
    EXPECT_TRUE(storage_->savePosition(eq2.id, pos2));
    EXPECT_TRUE(storage_->savePosition(eq3.id, pos3));
    
    // Find equipment in California area (roughly)
    auto california = storage_->findEquipmentInArea(
        32.0, -125.0,  // Southwest corner
        42.0, -114.0   // Northeast corner
    );
    
    ASSERT_EQ(california.size(), 2);
    
    // Verify equipment IDs
    std::vector<std::string> ids;
    for (const auto& eq : california) {
        ids.push_back(eq.id);
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("EQ001", "EQ002"));
}

TEST_F(DataStorageTest, EmptyPositionHistory) {
    // Create equipment but don't add positions
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(storage_->saveEquipment(equipment));
    
    // Get position history
    auto positions = storage_->getPositionHistory(equipment.id);
    EXPECT_TRUE(positions.empty());
}

TEST_F(DataStorageTest, NonExistentEquipmentPositionHistory) {
    // Try to get position history for non-existent equipment
    auto positions = storage_->getPositionHistory("NONEXISTENT");
    EXPECT_TRUE(positions.empty());
}

TEST_F(DataStorageTest, UpdateNonExistentEquipment) {
    Equipment nonExistent = createTestEquipment("NONEXISTENT");
    EXPECT_FALSE(storage_->updateEquipment(nonExistent));
}

TEST_F(DataStorageTest, DeleteNonExistentEquipment) {
    EXPECT_FALSE(storage_->deleteEquipment("NONEXISTENT"));
}

} // namespace testing
} // namespace equipment_tracker
// </test_code>