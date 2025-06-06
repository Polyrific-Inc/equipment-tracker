// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/data_storage.h"
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/types.h"
#include "equipment_tracker/utils/constants.h"
#include <filesystem>
#include <thread>
#include <chrono>

namespace equipment_tracker {

// Mock class for testing
class MockDataStorage : public DataStorage {
public:
    explicit MockDataStorage(const std::string& db_path = DEFAULT_DB_PATH) 
        : DataStorage(db_path) {}
    
    MOCK_METHOD(bool, initialize, (), (override));
    MOCK_METHOD(bool, saveEquipment, (const Equipment&), (override));
    MOCK_METHOD(std::optional<Equipment>, loadEquipment, (const EquipmentId&), (override));
    MOCK_METHOD(bool, updateEquipment, (const Equipment&), (override));
    MOCK_METHOD(bool, deleteEquipment, (const EquipmentId&), (override));
    MOCK_METHOD(bool, savePosition, (const EquipmentId&, const Position&), (override));
    MOCK_METHOD(std::vector<Position>, getPositionHistory, 
                (const EquipmentId&, const Timestamp&, const Timestamp&), (override));
    MOCK_METHOD(std::vector<Equipment>, getAllEquipment, (), (override));
    MOCK_METHOD(std::vector<Equipment>, findEquipmentByStatus, (EquipmentStatus), (override));
    MOCK_METHOD(std::vector<Equipment>, findEquipmentByType, (EquipmentType), (override));
    MOCK_METHOD(std::vector<Equipment>, findEquipmentInArea, 
                (double, double, double, double), (override));
};

class DataStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use a temporary file for testing
        temp_db_path_ = "test_equipment_tracker.db";
        
        // Delete the file if it exists
        if (std::filesystem::exists(temp_db_path_)) {
            std::filesystem::remove(temp_db_path_);
        }
        
        data_storage_ = std::make_unique<DataStorage>(temp_db_path_);
    }
    
    void TearDown() override {
        data_storage_.reset();
        
        // Clean up the temporary file
        if (std::filesystem::exists(temp_db_path_)) {
            std::filesystem::remove(temp_db_path_);
        }
    }
    
    std::string temp_db_path_;
    std::unique_ptr<DataStorage> data_storage_;
    
    // Helper method to create a test equipment
    Equipment createTestEquipment(const std::string& id = "EQ123") {
        Equipment equipment(id, EquipmentType::Forklift, "Test Forklift");
        equipment.setStatus(EquipmentStatus::Active);
        return equipment;
    }
    
    // Helper method to create a test position
    Position createTestPosition(double lat = 40.7128, double lon = -74.0060) {
        return Position(lat, lon, 10.0, 2.0, getCurrentTimestamp());
    }
};

TEST_F(DataStorageTest, InitializeDatabase) {
    EXPECT_TRUE(data_storage_->initialize());
    
    // Calling initialize again should still return true
    EXPECT_TRUE(data_storage_->initialize());
}

TEST_F(DataStorageTest, SaveAndLoadEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(data_storage_->saveEquipment(equipment));
    
    auto loaded_equipment = data_storage_->loadEquipment("EQ123");
    ASSERT_TRUE(loaded_equipment.has_value());
    EXPECT_EQ(loaded_equipment->getId(), "EQ123");
    EXPECT_EQ(loaded_equipment->getName(), "Test Forklift");
    EXPECT_EQ(loaded_equipment->getType(), EquipmentType::Forklift);
    EXPECT_EQ(loaded_equipment->getStatus(), EquipmentStatus::Active);
}

TEST_F(DataStorageTest, LoadNonExistentEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    auto loaded_equipment = data_storage_->loadEquipment("NONEXISTENT");
    EXPECT_FALSE(loaded_equipment.has_value());
}

TEST_F(DataStorageTest, UpdateEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    equipment.setName("Updated Forklift");
    equipment.setStatus(EquipmentStatus::Maintenance);
    EXPECT_TRUE(data_storage_->updateEquipment(equipment));
    
    auto loaded_equipment = data_storage_->loadEquipment("EQ123");
    ASSERT_TRUE(loaded_equipment.has_value());
    EXPECT_EQ(loaded_equipment->getName(), "Updated Forklift");
    EXPECT_EQ(loaded_equipment->getStatus(), EquipmentStatus::Maintenance);
}

TEST_F(DataStorageTest, UpdateNonExistentEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment equipment("NONEXISTENT", EquipmentType::Forklift, "Test Forklift");
    EXPECT_FALSE(data_storage_->updateEquipment(equipment));
}

TEST_F(DataStorageTest, DeleteEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    EXPECT_TRUE(data_storage_->deleteEquipment("EQ123"));
    
    auto loaded_equipment = data_storage_->loadEquipment("EQ123");
    EXPECT_FALSE(loaded_equipment.has_value());
}

TEST_F(DataStorageTest, DeleteNonExistentEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    EXPECT_FALSE(data_storage_->deleteEquipment("NONEXISTENT"));
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    Position pos1 = createTestPosition(40.7128, -74.0060);
    Position pos2 = createTestPosition(40.7129, -74.0061);
    
    EXPECT_TRUE(data_storage_->savePosition("EQ123", pos1));
    
    // Sleep a bit to ensure timestamps are different
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    EXPECT_TRUE(data_storage_->savePosition("EQ123", pos2));
    
    auto history = data_storage_->getPositionHistory("EQ123");
    ASSERT_EQ(history.size(), 2);
    
    // Positions should be ordered by timestamp (newest first)
    EXPECT_NEAR(history[0].getLatitude(), 40.7129, 0.0001);
    EXPECT_NEAR(history[0].getLongitude(), -74.0061, 0.0001);
    EXPECT_NEAR(history[1].getLatitude(), 40.7128, 0.0001);
    EXPECT_NEAR(history[1].getLongitude(), -74.0060, 0.0001);
}

TEST_F(DataStorageTest, GetPositionHistoryWithTimeRange) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    auto now = getCurrentTimestamp();
    auto past = now - std::chrono::hours(2);
    auto future = now + std::chrono::hours(2);
    
    Position pos1(40.7128, -74.0060, 10.0, 2.0, past);
    Position pos2(40.7129, -74.0061, 10.0, 2.0, now);
    Position pos3(40.7130, -74.0062, 10.0, 2.0, future);
    
    EXPECT_TRUE(data_storage_->savePosition("EQ123", pos1));
    EXPECT_TRUE(data_storage_->savePosition("EQ123", pos2));
    EXPECT_TRUE(data_storage_->savePosition("EQ123", pos3));
    
    // Get positions between past and now
    auto history = data_storage_->getPositionHistory("EQ123", past, now);
    ASSERT_EQ(history.size(), 2);
    
    // Get positions after now
    auto future_history = data_storage_->getPositionHistory("EQ123", now + std::chrono::seconds(1), future + std::chrono::seconds(1));
    ASSERT_EQ(future_history.size(), 1);
    
    // Get all positions
    auto all_history = data_storage_->getPositionHistory("EQ123");
    ASSERT_EQ(all_history.size(), 3);
}

TEST_F(DataStorageTest, GetAllEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment eq1("EQ123", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("EQ456", EquipmentType::Crane, "Crane 1");
    Equipment eq3("EQ789", EquipmentType::Bulldozer, "Bulldozer 1");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    auto all_equipment = data_storage_->getAllEquipment();
    ASSERT_EQ(all_equipment.size(), 3);
    
    // Check if all equipment is returned
    bool found_eq1 = false, found_eq2 = false, found_eq3 = false;
    for (const auto& eq : all_equipment) {
        if (eq.getId() == "EQ123") found_eq1 = true;
        if (eq.getId() == "EQ456") found_eq2 = true;
        if (eq.getId() == "EQ789") found_eq3 = true;
    }
    
    EXPECT_TRUE(found_eq1);
    EXPECT_TRUE(found_eq2);
    EXPECT_TRUE(found_eq3);
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment eq1("EQ123", EquipmentType::Forklift, "Forklift 1");
    eq1.setStatus(EquipmentStatus::Active);
    
    Equipment eq2("EQ456", EquipmentType::Crane, "Crane 1");
    eq2.setStatus(EquipmentStatus::Maintenance);
    
    Equipment eq3("EQ789", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setStatus(EquipmentStatus::Active);
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    auto active_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Active);
    ASSERT_EQ(active_equipment.size(), 2);
    
    auto maintenance_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    ASSERT_EQ(maintenance_equipment.size(), 1);
    EXPECT_EQ(maintenance_equipment[0].getId(), "EQ456");
    
    auto inactive_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Inactive);
    ASSERT_EQ(inactive_equipment.size(), 0);
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment eq1("EQ123", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("EQ456", EquipmentType::Crane, "Crane 1");
    Equipment eq3("EQ789", EquipmentType::Forklift, "Forklift 2");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    auto forklifts = data_storage_->findEquipmentByType(EquipmentType::Forklift);
    ASSERT_EQ(forklifts.size(), 2);
    
    auto cranes = data_storage_->findEquipmentByType(EquipmentType::Crane);
    ASSERT_EQ(cranes.size(), 1);
    EXPECT_EQ(cranes[0].getId(), "EQ456");
    
    auto bulldozers = data_storage_->findEquipmentByType(EquipmentType::Bulldozer);
    ASSERT_EQ(bulldozers.size(), 0);
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment eq1("EQ123", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("EQ456", EquipmentType::Crane, "Crane 1");
    Equipment eq3("EQ789", EquipmentType::Bulldozer, "Bulldozer 1");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Set positions for equipment
    Position pos1(40.7128, -74.0060); // New York
    Position pos2(34.0522, -118.2437); // Los Angeles
    Position pos3(41.8781, -87.6298); // Chicago
    
    ASSERT_TRUE(data_storage_->savePosition("EQ123", pos1));
    ASSERT_TRUE(data_storage_->savePosition("EQ456", pos2));
    ASSERT_TRUE(data_storage_->savePosition("EQ789", pos3));
    
    // Find equipment in New York area
    auto ny_area = data_storage_->findEquipmentInArea(40.70, -74.05, 40.75, -74.00);
    ASSERT_EQ(ny_area.size(), 1);
    EXPECT_EQ(ny_area[0].getId(), "EQ123");
    
    // Find equipment in a larger area covering Chicago and New York
    auto east_area = data_storage_->findEquipmentInArea(40.0, -90.0, 42.0, -70.0);
    ASSERT_EQ(east_area.size(), 2);
    
    // Find equipment in an area with no equipment
    auto empty_area = data_storage_->findEquipmentInArea(0.0, 0.0, 1.0, 1.0);
    ASSERT_EQ(empty_area.size(), 0);
}

TEST_F(DataStorageTest, ConcurrentAccess) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create multiple threads to access the database concurrently
    std::vector<std::thread> threads;
    const int num_threads = 10;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i]() {
            // Each thread performs a different operation
            switch (i % 5) {
                case 0: {
                    auto eq = data_storage_->loadEquipment("EQ123");
                    EXPECT_TRUE(eq.has_value());
                    break;
                }
                case 1: {
                    Equipment updated_eq = createTestEquipment();
                    updated_eq.setName("Thread " + std::to_string(i));
                    EXPECT_TRUE(data_storage_->updateEquipment(updated_eq));
                    break;
                }
                case 2: {
                    Position pos = createTestPosition(40.0 + i * 0.1, -74.0 + i * 0.1);
                    EXPECT_TRUE(data_storage_->savePosition("EQ123", pos));
                    break;
                }
                case 3: {
                    auto history = data_storage_->getPositionHistory("EQ123");
                    // Don't assert size as it may change during test
                    break;
                }
                case 4: {
                    auto all = data_storage_->getAllEquipment();
                    EXPECT_GE(all.size(), 1);
                    break;
                }
            }
        });
    }
    
    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify the equipment still exists after concurrent access
    auto loaded_equipment = data_storage_->loadEquipment("EQ123");
    ASSERT_TRUE(loaded_equipment.has_value());
}

// Test with mock to verify interface usage
TEST(DataStorageMockTest, MockInterface) {
    MockDataStorage mock_storage;
    
    // Set up expectations
    EXPECT_CALL(mock_storage, initialize())
        .WillOnce(::testing::Return(true));
        
    Equipment test_equipment("TEST123", EquipmentType::Forklift, "Test Equipment");
    EXPECT_CALL(mock_storage, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
        
    EXPECT_CALL(mock_storage, loadEquipment("TEST123"))
        .WillOnce(::testing::Return(std::optional<Equipment>(test_equipment)));
        
    // Execute the methods
    EXPECT_TRUE(mock_storage.initialize());
    EXPECT_TRUE(mock_storage.saveEquipment(test_equipment));
    
    auto loaded = mock_storage.loadEquipment("TEST123");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getId(), "TEST123");
}

} // namespace equipment_tracker
// </test_code>