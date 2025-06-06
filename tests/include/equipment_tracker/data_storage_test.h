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
        // Create a temporary database file for testing
        test_db_path_ = "test_equipment_tracker.db";
        
        // Delete the test database if it exists
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove(test_db_path_);
        }
        
        data_storage_ = std::make_unique<DataStorage>(test_db_path_);
    }
    
    void TearDown() override {
        data_storage_.reset();
        
        // Clean up the test database
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove(test_db_path_);
        }
    }
    
    std::string test_db_path_;
    std::unique_ptr<DataStorage> data_storage_;
    
    // Helper method to create a test equipment
    Equipment createTestEquipment(const std::string& id = "EQ001") {
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
    
    auto loaded_equipment = data_storage_->loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded_equipment.has_value());
    EXPECT_EQ(loaded_equipment->getId(), equipment.getId());
    EXPECT_EQ(loaded_equipment->getName(), equipment.getName());
    EXPECT_EQ(loaded_equipment->getType(), equipment.getType());
    EXPECT_EQ(loaded_equipment->getStatus(), equipment.getStatus());
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
    
    // Update equipment
    equipment.setName("Updated Forklift");
    equipment.setStatus(EquipmentStatus::Maintenance);
    EXPECT_TRUE(data_storage_->updateEquipment(equipment));
    
    // Verify update
    auto loaded_equipment = data_storage_->loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded_equipment.has_value());
    EXPECT_EQ(loaded_equipment->getName(), "Updated Forklift");
    EXPECT_EQ(loaded_equipment->getStatus(), EquipmentStatus::Maintenance);
}

TEST_F(DataStorageTest, DeleteEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Delete equipment
    EXPECT_TRUE(data_storage_->deleteEquipment(equipment.getId()));
    
    // Verify deletion
    auto loaded_equipment = data_storage_->loadEquipment(equipment.getId());
    EXPECT_FALSE(loaded_equipment.has_value());
}

TEST_F(DataStorageTest, SaveAndGetPositionHistory) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Save positions
    Position pos1 = createTestPosition(40.7128, -74.0060);
    Position pos2 = createTestPosition(40.7129, -74.0061);
    
    // Add a small delay to ensure different timestamps
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos1));
    
    // Add another small delay
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos2));
    
    // Get position history
    auto history = data_storage_->getPositionHistory(equipment.getId());
    ASSERT_EQ(history.size(), 2);
    
    // Positions should be returned in chronological order
    EXPECT_NEAR(history[0].getLatitude(), pos1.getLatitude(), 0.0001);
    EXPECT_NEAR(history[0].getLongitude(), pos1.getLongitude(), 0.0001);
    EXPECT_NEAR(history[1].getLatitude(), pos2.getLatitude(), 0.0001);
    EXPECT_NEAR(history[1].getLongitude(), pos2.getLongitude(), 0.0001);
}

TEST_F(DataStorageTest, GetPositionHistoryWithTimeRange) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create positions with specific timestamps
    auto now = getCurrentTimestamp();
    auto past1 = now - std::chrono::hours(2);
    auto past2 = now - std::chrono::hours(1);
    auto future = now + std::chrono::hours(1);
    
    Position pos1(40.7128, -74.0060, 10.0, 2.0, past1);
    Position pos2(40.7129, -74.0061, 10.0, 2.0, past2);
    Position pos3(40.7130, -74.0062, 10.0, 2.0, now);
    Position pos4(40.7131, -74.0063, 10.0, 2.0, future);
    
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos1));
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos2));
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos3));
    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos4));
    
    // Get position history with time range
    auto history = data_storage_->getPositionHistory(
        equipment.getId(), 
        past2 - std::chrono::minutes(5), 
        now + std::chrono::minutes(5)
    );
    
    ASSERT_EQ(history.size(), 2);
    EXPECT_NEAR(history[0].getLatitude(), pos2.getLatitude(), 0.0001);
    EXPECT_NEAR(history[1].getLatitude(), pos3.getLatitude(), 0.0001);
}

TEST_F(DataStorageTest, GetAllEquipment) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment eq1("EQ001", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("EQ002", EquipmentType::Crane, "Crane 1");
    Equipment eq3("EQ003", EquipmentType::Bulldozer, "Bulldozer 1");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    auto all_equipment = data_storage_->getAllEquipment();
    ASSERT_EQ(all_equipment.size(), 3);
    
    // Check if all equipment are retrieved
    bool found_eq1 = false, found_eq2 = false, found_eq3 = false;
    for (const auto& eq : all_equipment) {
        if (eq.getId() == "EQ001") found_eq1 = true;
        if (eq.getId() == "EQ002") found_eq2 = true;
        if (eq.getId() == "EQ003") found_eq3 = true;
    }
    
    EXPECT_TRUE(found_eq1);
    EXPECT_TRUE(found_eq2);
    EXPECT_TRUE(found_eq3);
}

TEST_F(DataStorageTest, FindEquipmentByStatus) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment eq1("EQ001", EquipmentType::Forklift, "Forklift 1");
    eq1.setStatus(EquipmentStatus::Active);
    
    Equipment eq2("EQ002", EquipmentType::Crane, "Crane 1");
    eq2.setStatus(EquipmentStatus::Maintenance);
    
    Equipment eq3("EQ003", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setStatus(EquipmentStatus::Active);
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    auto active_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Active);
    ASSERT_EQ(active_equipment.size(), 2);
    
    auto maintenance_equipment = data_storage_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    ASSERT_EQ(maintenance_equipment.size(), 1);
    EXPECT_EQ(maintenance_equipment[0].getId(), "EQ002");
}

TEST_F(DataStorageTest, FindEquipmentByType) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment eq1("EQ001", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("EQ002", EquipmentType::Crane, "Crane 1");
    Equipment eq3("EQ003", EquipmentType::Forklift, "Forklift 2");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    auto forklifts = data_storage_->findEquipmentByType(EquipmentType::Forklift);
    ASSERT_EQ(forklifts.size(), 2);
    
    auto cranes = data_storage_->findEquipmentByType(EquipmentType::Crane);
    ASSERT_EQ(cranes.size(), 1);
    EXPECT_EQ(cranes[0].getId(), "EQ002");
}

TEST_F(DataStorageTest, FindEquipmentInArea) {
    ASSERT_TRUE(data_storage_->initialize());
    
    Equipment eq1("EQ001", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("EQ002", EquipmentType::Crane, "Crane 1");
    Equipment eq3("EQ003", EquipmentType::Bulldozer, "Bulldozer 1");
    
    ASSERT_TRUE(data_storage_->saveEquipment(eq1));
    ASSERT_TRUE(data_storage_->saveEquipment(eq2));
    ASSERT_TRUE(data_storage_->saveEquipment(eq3));
    
    // Save positions for equipment
    Position pos1(40.7128, -74.0060); // New York
    Position pos2(34.0522, -118.2437); // Los Angeles
    Position pos3(41.8781, -87.6298); // Chicago
    
    ASSERT_TRUE(data_storage_->savePosition("EQ001", pos1));
    ASSERT_TRUE(data_storage_->savePosition("EQ002", pos2));
    ASSERT_TRUE(data_storage_->savePosition("EQ003", pos3));
    
    // Find equipment in New York area
    auto ny_area = data_storage_->findEquipmentInArea(
        40.70, -74.05, // Southwest corner
        40.75, -73.95  // Northeast corner
    );
    
    ASSERT_EQ(ny_area.size(), 1);
    EXPECT_EQ(ny_area[0].getId(), "EQ001");
    
    // Find equipment in a larger area covering Chicago
    auto midwest_area = data_storage_->findEquipmentInArea(
        40.0, -90.0, // Southwest corner
        43.0, -85.0  // Northeast corner
    );
    
    ASSERT_EQ(midwest_area.size(), 1);
    EXPECT_EQ(midwest_area[0].getId(), "EQ003");
}

TEST_F(DataStorageTest, ConcurrentAccess) {
    ASSERT_TRUE(data_storage_->initialize());
    
    // Create and save test equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(data_storage_->saveEquipment(equipment));
    
    // Create threads that concurrently access the database
    std::vector<std::thread> threads;
    const int num_threads = 10;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i, &equipment]() {
            // Each thread performs a different operation
            switch (i % 5) {
                case 0: {
                    // Load equipment
                    auto loaded = data_storage_->loadEquipment(equipment.getId());
                    EXPECT_TRUE(loaded.has_value());
                    break;
                }
                case 1: {
                    // Update equipment
                    Equipment updated = equipment;
                    updated.setName("Updated " + std::to_string(i));
                    EXPECT_TRUE(data_storage_->updateEquipment(updated));
                    break;
                }
                case 2: {
                    // Save position
                    Position pos = createTestPosition(40.0 + i * 0.01, -74.0 + i * 0.01);
                    EXPECT_TRUE(data_storage_->savePosition(equipment.getId(), pos));
                    break;
                }
                case 3: {
                    // Get position history
                    auto history = data_storage_->getPositionHistory(equipment.getId());
                    // We can't assert the size as it changes during the test
                    break;
                }
                case 4: {
                    // Get all equipment
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
    
    // Verify the equipment still exists after concurrent operations
    auto loaded_equipment = data_storage_->loadEquipment(equipment.getId());
    ASSERT_TRUE(loaded_equipment.has_value());
}

} // namespace equipment_tracker
// </test_code>