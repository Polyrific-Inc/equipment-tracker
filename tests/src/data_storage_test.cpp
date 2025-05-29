// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <ctime>
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
        test_db_path = "test_db_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        
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

    // Helper method to create a test equipment
    Equipment createTestEquipment(const std::string& id = "test123", 
                                 EquipmentType type = EquipmentType::Forklift,
                                 const std::string& name = "Test Equipment") {
        Equipment equipment(id, type, name);
        equipment.setStatus(EquipmentStatus::Active);
        return equipment;
    }

    // Helper method to create a test position
    Position createTestPosition(double lat = 37.7749, 
                               double lon = -122.4194,
                               double alt = 10.0,
                               double accuracy = 2.0) {
        return Position(lat, lon, alt, accuracy);
    }
};

// Test initialization
TEST_F(DataStorageTest, InitializeCreatesDirectories) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    
    // Check that directories were created
    EXPECT_TRUE(std::filesystem::exists(test_db_path));
    EXPECT_TRUE(std::filesystem::exists(test_db_path + "/equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path + "/positions"));
}

// Test initialization idempotence
TEST_F(DataStorageTest, InitializeIsIdempotent) {
    DataStorage storage(test_db_path);
    EXPECT_TRUE(storage.initialize());
    EXPECT_TRUE(storage.initialize()); // Should still return true on second call
}

// Test saving equipment
TEST_F(DataStorageTest, SaveEquipmentCreatesFile) {
    DataStorage storage(test_db_path);
    Equipment equipment = createTestEquipment();
    
    EXPECT_TRUE(storage.saveEquipment(equipment));
    
    // Check that equipment file was created
    std::string equipment_file = test_db_path + "/equipment/test123.txt";
    EXPECT_TRUE(std::filesystem::exists(equipment_file));
    
    // Check file contents
    std::ifstream file(equipment_file);
    ASSERT_TRUE(file.is_open());
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_THAT(content, ::testing::HasSubstr("id=test123"));
    EXPECT_THAT(content, ::testing::HasSubstr("name=Test Equipment"));
    EXPECT_THAT(content, ::testing::HasSubstr("type=0")); // EquipmentType::Forklift
    EXPECT_THAT(content, ::testing::HasSubstr("status=0")); // EquipmentStatus::Active
}

// Test saving equipment with position
TEST_F(DataStorageTest, SaveEquipmentWithPosition) {
    DataStorage storage(test_db_path);
    Equipment equipment = createTestEquipment();
    Position position = createTestPosition();
    equipment.setLastPosition(position);
    
    EXPECT_TRUE(storage.saveEquipment(equipment));
    
    // Check that equipment file was created
    std::string equipment_file = test_db_path + "/equipment/test123.txt";
    EXPECT_TRUE(std::filesystem::exists(equipment_file));
    
    // Check file contents
    std::ifstream file(equipment_file);
    ASSERT_TRUE(file.is_open());
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_THAT(content, ::testing::HasSubstr("last_position=37.7749"));
    EXPECT_THAT(content, ::testing::HasSubstr("-122.4194"));
}

// Test loading equipment
TEST_F(DataStorageTest, LoadEquipment) {
    DataStorage storage(test_db_path);
    Equipment original = createTestEquipment();
    Position position = createTestPosition();
    original.setLastPosition(position);
    
    EXPECT_TRUE(storage.saveEquipment(original));
    
    auto loaded = storage.loadEquipment("test123");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getId(), "test123");
    EXPECT_EQ(loaded->getName(), "Test Equipment");
    EXPECT_EQ(loaded->getType(), EquipmentType::Forklift);
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::Active);
    
    auto loaded_pos = loaded->getLastPosition();
    ASSERT_TRUE(loaded_pos.has_value());
    EXPECT_NEAR(loaded_pos->getLatitude(), 37.7749, 0.0001);
    EXPECT_NEAR(loaded_pos->getLongitude(), -122.4194, 0.0001);
    EXPECT_NEAR(loaded_pos->getAltitude(), 10.0, 0.0001);
    EXPECT_NEAR(loaded_pos->getAccuracy(), 2.0, 0.0001);
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
    Equipment original = createTestEquipment();
    
    EXPECT_TRUE(storage.saveEquipment(original));
    
    // Modify equipment
    Equipment updated = original;
    updated.setName("Updated Equipment");
    updated.setStatus(EquipmentStatus::Maintenance);
    
    EXPECT_TRUE(storage.updateEquipment(updated));
    
    // Load and verify
    auto loaded = storage.loadEquipment("test123");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getName(), "Updated Equipment");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::Maintenance);
}

// Test deleting equipment
TEST_F(DataStorageTest, DeleteEquipment) {
    DataStorage storage(test_db_path);
    Equipment equipment = createTestEquipment();
    
    EXPECT_TRUE(storage.saveEquipment(equipment));
    EXPECT_TRUE(storage.deleteEquipment("test123"));
    
    // Check that equipment file was deleted
    std::string equipment_file = test_db_path + "/equipment/test123.txt";
    EXPECT_FALSE(std::filesystem::exists(equipment_file));
    
    // Check that loading returns nullopt
    auto loaded = storage.loadEquipment("test123");
    EXPECT_FALSE(loaded.has_value());
}

// Test saving position
TEST_F(DataStorageTest, SavePosition) {
    DataStorage storage(test_db_path);
    Position position = createTestPosition();
    
    EXPECT_TRUE(storage.savePosition("test123", position));
    
    // Check that positions directory was created
    std::string positions_dir = test_db_path + "/positions/test123";
    EXPECT_TRUE(std::filesystem::exists(positions_dir));
    
    // Check that at least one position file exists
    bool found_file = false;
    for (const auto& entry : std::filesystem::directory_iterator(positions_dir)) {
        if (entry.is_regular_file()) {
            found_file = true;
            break;
        }
    }
    EXPECT_TRUE(found_file);
}

// Test getting position history
TEST_F(DataStorageTest, GetPositionHistory) {
    DataStorage storage(test_db_path);
    
    // Create multiple positions with different timestamps
    auto now = std::chrono::system_clock::now();
    Position pos1(37.7749, -122.4194, 10.0, 2.0, now - std::chrono::hours(2));
    Position pos2(37.7750, -122.4195, 11.0, 2.1, now - std::chrono::hours(1));
    Position pos3(37.7751, -122.4196, 12.0, 2.2, now);
    
    EXPECT_TRUE(storage.savePosition("test123", pos1));
    EXPECT_TRUE(storage.savePosition("test123", pos2));
    EXPECT_TRUE(storage.savePosition("test123", pos3));
    
    // Get all history
    auto history = storage.getPositionHistory("test123", 
                                             now - std::chrono::hours(3), 
                                             now + std::chrono::hours(1));
    EXPECT_EQ(history.size(), 3);
    
    // Get partial history
    auto recent_history = storage.getPositionHistory("test123", 
                                                    now - std::chrono::minutes(90), 
                                                    now + std::chrono::hours(1));
    EXPECT_EQ(recent_history.size(), 2);
    
    // Check that positions are sorted by timestamp
    if (recent_history.size() >= 2) {
        EXPECT_LT(recent_history[0].getTimestamp(), recent_history[1].getTimestamp());
    }
}

// Test getting all equipment
TEST_F(DataStorageTest, GetAllEquipment) {
    DataStorage storage(test_db_path);
    
    // Create multiple equipment
    Equipment eq1("eq1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("eq2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("eq3", EquipmentType::Bulldozer, "Bulldozer 1");
    
    EXPECT_TRUE(storage.saveEquipment(eq1));
    EXPECT_TRUE(storage.saveEquipment(eq2));
    EXPECT_TRUE(storage.saveEquipment(eq3));
    
    auto all_equipment = storage.getAllEquipment();
    EXPECT_EQ(all_equipment.size(), 3);
    
    // Check that all equipment are loaded correctly
    bool found_eq1 = false, found_eq2 = false, found_eq3 = false;
    for (const auto& eq : all_equipment) {
        if (eq.getId() == "eq1") found_eq1 = true;
        if (eq.getId() == "eq2") found_eq2 = true;
        if (eq.getId() == "eq3") found_eq3 = true;
    }
    
    EXPECT_TRUE(found_eq1);
    EXPECT_TRUE(found_eq2);
    EXPECT_TRUE(found_eq3);
}

// Test finding equipment by status
TEST_F(DataStorageTest, FindEquipmentByStatus) {
    DataStorage storage(test_db_path);
    
    // Create equipment with different statuses
    Equipment eq1("eq1", EquipmentType::Forklift, "Forklift 1");
    eq1.setStatus(EquipmentStatus::Active);
    
    Equipment eq2("eq2", EquipmentType::Crane, "Crane 1");
    eq2.setStatus(EquipmentStatus::Maintenance);
    
    Equipment eq3("eq3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setStatus(EquipmentStatus::Active);
    
    EXPECT_TRUE(storage.saveEquipment(eq1));
    EXPECT_TRUE(storage.saveEquipment(eq2));
    EXPECT_TRUE(storage.saveEquipment(eq3));
    
    auto active_equipment = storage.findEquipmentByStatus(EquipmentStatus::Active);
    EXPECT_EQ(active_equipment.size(), 2);
    
    auto maintenance_equipment = storage.findEquipmentByStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(maintenance_equipment.size(), 1);
    EXPECT_EQ(maintenance_equipment[0].getId(), "eq2");
}

// Test finding equipment by type
TEST_F(DataStorageTest, FindEquipmentByType) {
    DataStorage storage(test_db_path);
    
    // Create equipment with different types
    Equipment eq1("eq1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("eq2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("eq3", EquipmentType::Forklift, "Forklift 2");
    
    EXPECT_TRUE(storage.saveEquipment(eq1));
    EXPECT_TRUE(storage.saveEquipment(eq2));
    EXPECT_TRUE(storage.saveEquipment(eq3));
    
    auto forklifts = storage.findEquipmentByType(EquipmentType::Forklift);
    EXPECT_EQ(forklifts.size(), 2);
    
    auto cranes = storage.findEquipmentByType(EquipmentType::Crane);
    EXPECT_EQ(cranes.size(), 1);
    EXPECT_EQ(cranes[0].getId(), "eq2");
}

// Test finding equipment in area
TEST_F(DataStorageTest, FindEquipmentInArea) {
    DataStorage storage(test_db_path);
    
    // Create equipment with different positions
    Equipment eq1("eq1", EquipmentType::Forklift, "Forklift 1");
    eq1.setLastPosition(Position(37.7749, -122.4194));
    
    Equipment eq2("eq2", EquipmentType::Crane, "Crane 1");
    eq2.setLastPosition(Position(37.3382, -121.8863));
    
    Equipment eq3("eq3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setLastPosition(Position(37.7749, -122.4195));
    
    EXPECT_TRUE(storage.saveEquipment(eq1));
    EXPECT_TRUE(storage.saveEquipment(eq2));
    EXPECT_TRUE(storage.saveEquipment(eq3));
    
    // Find equipment in San Francisco area
    auto sf_equipment = storage.findEquipmentInArea(37.7, -122.5, 37.8, -122.4);
    EXPECT_EQ(sf_equipment.size(), 2);
    
    // Find equipment in San Jose area
    auto sj_equipment = storage.findEquipmentInArea(37.3, -122.0, 37.4, -121.8);
    EXPECT_EQ(sj_equipment.size(), 1);
    EXPECT_EQ(sj_equipment[0].getId(), "eq2");
}

// Test concurrent operations
TEST_F(DataStorageTest, ConcurrentOperations) {
    DataStorage storage(test_db_path);
    storage.initialize();
    
    // Create and save test equipment
    Equipment equipment = createTestEquipment();
    EXPECT_TRUE(storage.saveEquipment(equipment));
    
    // Run concurrent operations
    std::thread t1([&storage]() {
        for (int i = 0; i < 10; i++) {
            auto eq = storage.loadEquipment("test123");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    std::thread t2([&storage]() {
        for (int i = 0; i < 10; i++) {
            Position pos = Position(37.7749 + i * 0.001, -122.4194 + i * 0.001);
            storage.savePosition("test123", pos);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    t1.join();
    t2.join();
    
    // Verify that position history was saved
    auto history = storage.getPositionHistory("test123");
    EXPECT_GE(history.size(), 10);
}

// Test error handling for invalid paths
TEST_F(DataStorageTest, ErrorHandlingInvalidPath) {
    // Create a storage with an invalid path (contains characters not allowed in filenames)
    DataStorage storage("/invalid/path/with/illegal/characters/*/");
    
    // Operations should fail gracefully
    EXPECT_FALSE(storage.initialize());
    EXPECT_FALSE(storage.saveEquipment(createTestEquipment()));
    EXPECT_FALSE(storage.loadEquipment("test123").has_value());
}

// Test empty database
TEST_F(DataStorageTest, EmptyDatabase) {
    DataStorage storage(test_db_path);
    storage.initialize();
    
    // Operations on empty database should return empty results
    EXPECT_TRUE(storage.getAllEquipment().empty());
    EXPECT_TRUE(storage.findEquipmentByStatus(EquipmentStatus::Active).empty());
    EXPECT_TRUE(storage.findEquipmentByType(EquipmentType::Forklift).empty());
    EXPECT_TRUE(storage.findEquipmentInArea(0, 0, 1, 1).empty());
    EXPECT_TRUE(storage.getPositionHistory("nonexistent").empty());
}

} // namespace equipment_tracker
// </test_code>