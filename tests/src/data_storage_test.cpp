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

namespace equipment_tracker {

// Test fixture for DataStorage tests
class DataStorageTest : public ::testing::Test {
protected:
    const std::string TEST_DB_PATH = "test_equipment_db";
    
    void SetUp() override {
        // Clean up any existing test database
        if (std::filesystem::exists(TEST_DB_PATH)) {
            std::filesystem::remove_all(TEST_DB_PATH);
        }
        
        // Create a fresh data storage instance
        storage = std::make_unique<DataStorage>(TEST_DB_PATH);
    }
    
    void TearDown() override {
        // Clean up after tests
        storage.reset();
        if (std::filesystem::exists(TEST_DB_PATH)) {
            std::filesystem::remove_all(TEST_DB_PATH);
        }
    }
    
    // Helper method to create a test equipment
    Equipment createTestEquipment(const std::string& id = "test123") {
        Equipment equipment(id, EquipmentType::Forklift, "Test Forklift");
        equipment.setStatus(EquipmentStatus::Active);
        
        // Add a position
        Position pos(37.7749, -122.4194, 10.0, 2.0);
        equipment.setLastPosition(pos);
        
        return equipment;
    }
    
    // Helper method to create a test position
    Position createTestPosition(double lat = 37.7749, double lon = -122.4194) {
        return Position(lat, lon, 10.0, 2.0);
    }
    
    std::unique_ptr<DataStorage> storage;
};

// Test initialization
TEST_F(DataStorageTest, InitializeCreatesDirectories) {
    ASSERT_TRUE(storage->initialize());
    EXPECT_TRUE(std::filesystem::exists(TEST_DB_PATH));
    EXPECT_TRUE(std::filesystem::exists(TEST_DB_PATH + "/equipment"));
    EXPECT_TRUE(std::filesystem::exists(TEST_DB_PATH + "/positions"));
}

// Test double initialization
TEST_F(DataStorageTest, InitializeTwiceReturnsTrue) {
    ASSERT_TRUE(storage->initialize());
    EXPECT_TRUE(storage->initialize());
}

// Test saving equipment
TEST_F(DataStorageTest, SaveEquipmentCreatesFile) {
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(storage->saveEquipment(equipment));
    
    std::string equipmentFile = TEST_DB_PATH + "/equipment/test123.txt";
    EXPECT_TRUE(std::filesystem::exists(equipmentFile));
    
    // Verify file content
    std::ifstream file(equipmentFile);
    ASSERT_TRUE(file.is_open());
    
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    
    EXPECT_THAT(content, ::testing::HasSubstr("id=test123"));
    EXPECT_THAT(content, ::testing::HasSubstr("name=Test Forklift"));
    EXPECT_THAT(content, ::testing::HasSubstr("type=0")); // EquipmentType::Forklift
    EXPECT_THAT(content, ::testing::HasSubstr("status=0")); // EquipmentStatus::Active
    EXPECT_THAT(content, ::testing::HasSubstr("last_position=37.7749,-122.4194,10,2"));
}

// Test loading equipment
TEST_F(DataStorageTest, LoadEquipmentReturnsCorrectData) {
    // Save equipment first
    Equipment original = createTestEquipment();
    ASSERT_TRUE(storage->saveEquipment(original));
    
    // Load equipment
    auto loaded = storage->loadEquipment("test123");
    ASSERT_TRUE(loaded.has_value());
    
    // Verify loaded data
    EXPECT_EQ(loaded->getId(), "test123");
    EXPECT_EQ(loaded->getName(), "Test Forklift");
    EXPECT_EQ(loaded->getType(), EquipmentType::Forklift);
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::Active);
    
    // Verify position
    auto pos = loaded->getLastPosition();
    ASSERT_TRUE(pos.has_value());
    EXPECT_DOUBLE_EQ(pos->getLatitude(), 37.7749);
    EXPECT_DOUBLE_EQ(pos->getLongitude(), -122.4194);
    EXPECT_DOUBLE_EQ(pos->getAltitude(), 10.0);
    EXPECT_DOUBLE_EQ(pos->getAccuracy(), 2.0);
}

// Test loading non-existent equipment
TEST_F(DataStorageTest, LoadNonExistentEquipmentReturnsNullopt) {
    auto loaded = storage->loadEquipment("nonexistent");
    EXPECT_FALSE(loaded.has_value());
}

// Test updating equipment
TEST_F(DataStorageTest, UpdateEquipmentChangesData) {
    // Save equipment first
    Equipment original = createTestEquipment();
    ASSERT_TRUE(storage->saveEquipment(original));
    
    // Modify and update
    Equipment updated = original;
    updated.setName("Updated Forklift");
    updated.setStatus(EquipmentStatus::Maintenance);
    ASSERT_TRUE(storage->updateEquipment(updated));
    
    // Load and verify
    auto loaded = storage->loadEquipment("test123");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getName(), "Updated Forklift");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::Maintenance);
}

// Test deleting equipment
TEST_F(DataStorageTest, DeleteEquipmentRemovesFile) {
    // Save equipment first
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(storage->saveEquipment(equipment));
    
    std::string equipmentFile = TEST_DB_PATH + "/equipment/test123.txt";
    EXPECT_TRUE(std::filesystem::exists(equipmentFile));
    
    // Delete equipment
    ASSERT_TRUE(storage->deleteEquipment("test123"));
    EXPECT_FALSE(std::filesystem::exists(equipmentFile));
}

// Test saving position
TEST_F(DataStorageTest, SavePositionCreatesFile) {
    Position position = createTestPosition();
    ASSERT_TRUE(storage->savePosition("test123", position));
    
    std::string positionsDir = TEST_DB_PATH + "/positions/test123";
    EXPECT_TRUE(std::filesystem::exists(positionsDir));
    
    // Verify at least one file exists in the directory
    bool fileFound = false;
    for (const auto& entry : std::filesystem::directory_iterator(positionsDir)) {
        if (entry.is_regular_file()) {
            fileFound = true;
            break;
        }
    }
    EXPECT_TRUE(fileFound);
}

// Test getting position history
TEST_F(DataStorageTest, GetPositionHistoryReturnsCorrectData) {
    // Create equipment
    Equipment equipment = createTestEquipment();
    ASSERT_TRUE(storage->saveEquipment(equipment));
    
    // Save multiple positions with different timestamps
    auto now = std::chrono::system_clock::now();
    
    Position pos1(37.7749, -122.4194, 10.0, 2.0, now - std::chrono::hours(2));
    Position pos2(37.7750, -122.4195, 11.0, 2.1, now - std::chrono::hours(1));
    Position pos3(37.7751, -122.4196, 12.0, 2.2, now);
    
    ASSERT_TRUE(storage->savePosition("test123", pos1));
    ASSERT_TRUE(storage->savePosition("test123", pos2));
    ASSERT_TRUE(storage->savePosition("test123", pos3));
    
    // Get full history
    auto history = storage->getPositionHistory("test123");
    EXPECT_EQ(history.size(), 3);
    
    // Get limited history
    auto limitedHistory = storage->getPositionHistory(
        "test123", 
        now - std::chrono::minutes(90), 
        now
    );
    EXPECT_EQ(limitedHistory.size(), 2);
    
    // Verify the most recent position
    if (!limitedHistory.empty()) {
        EXPECT_DOUBLE_EQ(limitedHistory.back().getLatitude(), 37.7751);
        EXPECT_DOUBLE_EQ(limitedHistory.back().getLongitude(), -122.4196);
    }
}

// Test getting all equipment
TEST_F(DataStorageTest, GetAllEquipmentReturnsCorrectData) {
    // Save multiple equipment
    Equipment eq1("test1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("test2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("test3", EquipmentType::Bulldozer, "Bulldozer 1");
    
    ASSERT_TRUE(storage->saveEquipment(eq1));
    ASSERT_TRUE(storage->saveEquipment(eq2));
    ASSERT_TRUE(storage->saveEquipment(eq3));
    
    // Get all equipment
    auto allEquipment = storage->getAllEquipment();
    EXPECT_EQ(allEquipment.size(), 3);
    
    // Verify IDs are present
    std::vector<std::string> ids;
    for (const auto& eq : allEquipment) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("test1", "test2", "test3"));
}

// Test finding equipment by status
TEST_F(DataStorageTest, FindEquipmentByStatusReturnsCorrectData) {
    // Save equipment with different statuses
    Equipment eq1("test1", EquipmentType::Forklift, "Forklift 1");
    eq1.setStatus(EquipmentStatus::Active);
    
    Equipment eq2("test2", EquipmentType::Crane, "Crane 1");
    eq2.setStatus(EquipmentStatus::Maintenance);
    
    Equipment eq3("test3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setStatus(EquipmentStatus::Active);
    
    ASSERT_TRUE(storage->saveEquipment(eq1));
    ASSERT_TRUE(storage->saveEquipment(eq2));
    ASSERT_TRUE(storage->saveEquipment(eq3));
    
    // Find active equipment
    auto activeEquipment = storage->findEquipmentByStatus(EquipmentStatus::Active);
    EXPECT_EQ(activeEquipment.size(), 2);
    
    // Find maintenance equipment
    auto maintenanceEquipment = storage->findEquipmentByStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(maintenanceEquipment.size(), 1);
    EXPECT_EQ(maintenanceEquipment[0].getId(), "test2");
}

// Test finding equipment by type
TEST_F(DataStorageTest, FindEquipmentByTypeReturnsCorrectData) {
    // Save equipment with different types
    Equipment eq1("test1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("test2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("test3", EquipmentType::Forklift, "Forklift 2");
    
    ASSERT_TRUE(storage->saveEquipment(eq1));
    ASSERT_TRUE(storage->saveEquipment(eq2));
    ASSERT_TRUE(storage->saveEquipment(eq3));
    
    // Find forklifts
    auto forklifts = storage->findEquipmentByType(EquipmentType::Forklift);
    EXPECT_EQ(forklifts.size(), 2);
    
    // Find cranes
    auto cranes = storage->findEquipmentByType(EquipmentType::Crane);
    EXPECT_EQ(cranes.size(), 1);
    EXPECT_EQ(cranes[0].getId(), "test2");
}

// Test finding equipment in area
TEST_F(DataStorageTest, FindEquipmentInAreaReturnsCorrectData) {
    // Save equipment with different positions
    Equipment eq1("test1", EquipmentType::Forklift, "Forklift 1");
    eq1.setLastPosition(Position(37.7749, -122.4194));
    
    Equipment eq2("test2", EquipmentType::Crane, "Crane 1");
    eq2.setLastPosition(Position(37.8000, -122.4000));
    
    Equipment eq3("test3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setLastPosition(Position(37.7800, -122.4100));
    
    ASSERT_TRUE(storage->saveEquipment(eq1));
    ASSERT_TRUE(storage->saveEquipment(eq2));
    ASSERT_TRUE(storage->saveEquipment(eq3));
    
    // Find equipment in area
    auto equipmentInArea = storage->findEquipmentInArea(
        37.7700, -122.4200,
        37.7850, -122.4000
    );
    
    EXPECT_EQ(equipmentInArea.size(), 2);
    
    // Verify IDs
    std::vector<std::string> ids;
    for (const auto& eq : equipmentInArea) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("test1", "test3"));
}

// Test edge case: empty database
TEST_F(DataStorageTest, EmptyDatabaseReturnsEmptyResults) {
    ASSERT_TRUE(storage->initialize());
    
    EXPECT_TRUE(storage->getAllEquipment().empty());
    EXPECT_TRUE(storage->findEquipmentByStatus(EquipmentStatus::Active).empty());
    EXPECT_TRUE(storage->findEquipmentByType(EquipmentType::Forklift).empty());
    EXPECT_TRUE(storage->findEquipmentInArea(0, 0, 1, 1).empty());
    EXPECT_TRUE(storage->getPositionHistory("nonexistent").empty());
}

// Test edge case: equipment with no position
TEST_F(DataStorageTest, EquipmentWithNoPositionNotFoundInAreaSearch) {
    Equipment eq("test1", EquipmentType::Forklift, "Forklift 1");
    // No position set
    
    ASSERT_TRUE(storage->saveEquipment(eq));
    
    auto equipmentInArea = storage->findEquipmentInArea(0, 0, 100, 100);
    EXPECT_TRUE(equipmentInArea.empty());
}

// Test concurrent access
TEST_F(DataStorageTest, ConcurrentAccessIsThreadSafe) {
    ASSERT_TRUE(storage->initialize());
    
    // Create and start threads
    std::vector<std::thread> threads;
    const int numThreads = 10;
    
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([this, i]() {
            std::string id = "test" + std::to_string(i);
            Equipment eq(id, EquipmentType::Forklift, "Forklift " + std::to_string(i));
            
            // Perform various operations
            EXPECT_TRUE(storage->saveEquipment(eq));
            auto loaded = storage->loadEquipment(id);
            EXPECT_TRUE(loaded.has_value());
            
            // Add position
            Position pos(37.7749 + i * 0.001, -122.4194 + i * 0.001);
            EXPECT_TRUE(storage->savePosition(id, pos));
            
            // Get all equipment
            auto all = storage->getAllEquipment();
        });
    }
    
    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all equipment was saved
    auto allEquipment = storage->getAllEquipment();
    EXPECT_EQ(allEquipment.size(), numThreads);
}

} // namespace equipment_tracker
// </test_code>