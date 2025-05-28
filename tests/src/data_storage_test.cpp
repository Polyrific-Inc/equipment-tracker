// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <cstdio>
#include "equipment_tracker/data_storage.h"
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/types.h"
#include "equipment_tracker/utils/constants.h"

using namespace equipment_tracker;
using namespace testing;

// Platform-specific time handling
#ifdef _WIN32
#define PLATFORM_GMTIME(t, tm) gmtime_s(tm, t)
#else
#define PLATFORM_GMTIME(t, tm) gmtime_r(t, tm)
#endif

class DataStorageTest : public ::testing::Test {
protected:
    std::string test_db_path;
    
    void SetUp() override {
        // Create a unique temporary directory for each test
        test_db_path = "test_db_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    }
    
    void TearDown() override {
        // Clean up the test directory
        if (std::filesystem::exists(test_db_path)) {
            std::filesystem::remove_all(test_db_path);
        }
    }
    
    // Helper method to create a test equipment
    Equipment createTestEquipment(const std::string& id = "test123") {
        Equipment equipment(id, EquipmentType::Forklift, "Test Forklift");
        equipment.setStatus(EquipmentStatus::Active);
        return equipment;
    }
    
    // Helper method to create a test position
    Position createTestPosition(double lat = 40.7128, double lon = -74.0060) {
        return Position(lat, lon, 10.0, 5.0, std::chrono::system_clock::now());
    }
    
    // Helper method to verify equipment directory exists
    bool equipmentDirectoryExists() {
        return std::filesystem::exists(test_db_path + "/equipment");
    }
    
    // Helper method to verify positions directory exists
    bool positionsDirectoryExists() {
        return std::filesystem::exists(test_db_path + "/positions");
    }
};

TEST_F(DataStorageTest, InitializeCreatesDirectories) {
    DataStorage storage(test_db_path);
    
    EXPECT_TRUE(storage.initialize());
    EXPECT_TRUE(std::filesystem::exists(test_db_path));
    EXPECT_TRUE(equipmentDirectoryExists());
    EXPECT_TRUE(positionsDirectoryExists());
}

TEST_F(DataStorageTest, InitializeIsIdempotent) {
    DataStorage storage(test_db_path);
    
    EXPECT_TRUE(storage.initialize());
    EXPECT_TRUE(storage.initialize()); // Second call should also return true
}

TEST_F(DataStorageTest, SaveEquipmentCreatesFile) {
    DataStorage storage(test_db_path);
    Equipment equipment = createTestEquipment();
    
    EXPECT_TRUE(storage.saveEquipment(equipment));
    EXPECT_TRUE(std::filesystem::exists(test_db_path + "/equipment/test123.txt"));
}

TEST_F(DataStorageTest, SaveEquipmentWithPosition) {
    DataStorage storage(test_db_path);
    Equipment equipment = createTestEquipment();
    Position position = createTestPosition();
    equipment.setLastPosition(position);
    
    EXPECT_TRUE(storage.saveEquipment(equipment));
    
    // Verify file contains position data
    std::ifstream file(test_db_path + "/equipment/test123.txt");
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_THAT(content, HasSubstr("last_position="));
}

TEST_F(DataStorageTest, LoadEquipmentReturnsNulloptForNonexistentId) {
    DataStorage storage(test_db_path);
    storage.initialize();
    
    auto result = storage.loadEquipment("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(DataStorageTest, LoadEquipmentReturnsCorrectData) {
    DataStorage storage(test_db_path);
    Equipment original = createTestEquipment();
    storage.saveEquipment(original);
    
    auto loaded = storage.loadEquipment("test123");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getId(), "test123");
    EXPECT_EQ(loaded->getName(), "Test Forklift");
    EXPECT_EQ(loaded->getType(), EquipmentType::Forklift);
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::Active);
}

TEST_F(DataStorageTest, LoadEquipmentWithPosition) {
    DataStorage storage(test_db_path);
    Equipment original = createTestEquipment();
    Position position = createTestPosition(42.3601, -71.0589); // Boston coordinates
    original.setLastPosition(position);
    storage.saveEquipment(original);
    
    auto loaded = storage.loadEquipment("test123");
    ASSERT_TRUE(loaded.has_value());
    ASSERT_TRUE(loaded->getLastPosition().has_value());
    
    auto loadedPos = loaded->getLastPosition().value();
    EXPECT_NEAR(loadedPos.getLatitude(), 42.3601, 0.0001);
    EXPECT_NEAR(loadedPos.getLongitude(), -71.0589, 0.0001);
    EXPECT_NEAR(loadedPos.getAltitude(), 10.0, 0.0001);
}

TEST_F(DataStorageTest, UpdateEquipmentModifiesExistingData) {
    DataStorage storage(test_db_path);
    Equipment original = createTestEquipment();
    storage.saveEquipment(original);
    
    // Modify equipment
    Equipment updated("test123", EquipmentType::Forklift, "Updated Forklift");
    updated.setStatus(EquipmentStatus::Maintenance);
    
    EXPECT_TRUE(storage.updateEquipment(updated));
    
    auto loaded = storage.loadEquipment("test123");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getName(), "Updated Forklift");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::Maintenance);
}

TEST_F(DataStorageTest, DeleteEquipmentRemovesFile) {
    DataStorage storage(test_db_path);
    Equipment equipment = createTestEquipment();
    storage.saveEquipment(equipment);
    
    EXPECT_TRUE(storage.deleteEquipment("test123"));
    EXPECT_FALSE(std::filesystem::exists(test_db_path + "/equipment/test123.txt"));
}

TEST_F(DataStorageTest, SavePositionCreatesFile) {
    DataStorage storage(test_db_path);
    Position position = createTestPosition();
    
    EXPECT_TRUE(storage.savePosition("test123", position));
    EXPECT_TRUE(std::filesystem::exists(test_db_path + "/positions/test123"));
    
    // Check that at least one position file exists
    bool found_file = false;
    for (const auto& entry : std::filesystem::directory_iterator(test_db_path + "/positions/test123")) {
        if (entry.is_regular_file()) {
            found_file = true;
            break;
        }
    }
    EXPECT_TRUE(found_file);
}

TEST_F(DataStorageTest, GetPositionHistoryReturnsEmptyForNonexistentId) {
    DataStorage storage(test_db_path);
    storage.initialize();
    
    auto history = storage.getPositionHistory("nonexistent");
    EXPECT_TRUE(history.empty());
}

TEST_F(DataStorageTest, GetPositionHistoryReturnsCorrectData) {
    DataStorage storage(test_db_path);
    
    // Create positions with different timestamps
    auto now = std::chrono::system_clock::now();
    Position pos1(40.7128, -74.0060, 10.0, 5.0, now - std::chrono::hours(2));
    Position pos2(40.7129, -74.0061, 11.0, 5.0, now - std::chrono::hours(1));
    Position pos3(40.7130, -74.0062, 12.0, 5.0, now);
    
    // Save positions
    storage.savePosition("test123", pos1);
    storage.savePosition("test123", pos2);
    storage.savePosition("test123", pos3);
    
    // Get all history
    auto history = storage.getPositionHistory("test123");
    EXPECT_EQ(history.size(), 3);
    
    // Get history with time range
    auto rangeHistory = storage.getPositionHistory(
        "test123", 
        now - std::chrono::hours(1) - std::chrono::minutes(5), 
        now + std::chrono::minutes(5)
    );
    EXPECT_EQ(rangeHistory.size(), 2);
}

TEST_F(DataStorageTest, GetAllEquipmentReturnsEmptyForEmptyDatabase) {
    DataStorage storage(test_db_path);
    storage.initialize();
    
    auto all = storage.getAllEquipment();
    EXPECT_TRUE(all.empty());
}

TEST_F(DataStorageTest, GetAllEquipmentReturnsAllSavedEquipment) {
    DataStorage storage(test_db_path);
    
    // Save multiple equipment
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    
    storage.saveEquipment(eq1);
    storage.saveEquipment(eq2);
    storage.saveEquipment(eq3);
    
    auto all = storage.getAllEquipment();
    EXPECT_EQ(all.size(), 3);
    
    // Check if all IDs are present
    std::vector<std::string> ids;
    for (const auto& eq : all) {
        ids.push_back(eq.getId());
    }
    EXPECT_THAT(ids, UnorderedElementsAre("id1", "id2", "id3"));
}

TEST_F(DataStorageTest, FindEquipmentByStatusReturnsMatchingEquipment) {
    DataStorage storage(test_db_path);
    
    // Save equipment with different statuses
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    eq1.setStatus(EquipmentStatus::Active);
    
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    eq2.setStatus(EquipmentStatus::Maintenance);
    
    Equipment eq3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setStatus(EquipmentStatus::Active);
    
    storage.saveEquipment(eq1);
    storage.saveEquipment(eq2);
    storage.saveEquipment(eq3);
    
    auto active = storage.findEquipmentByStatus(EquipmentStatus::Active);
    EXPECT_EQ(active.size(), 2);
    
    auto maintenance = storage.findEquipmentByStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(maintenance.size(), 1);
    EXPECT_EQ(maintenance[0].getId(), "id2");
}

TEST_F(DataStorageTest, FindEquipmentByTypeReturnsMatchingEquipment) {
    DataStorage storage(test_db_path);
    
    // Save equipment with different types
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("id3", EquipmentType::Forklift, "Forklift 2");
    
    storage.saveEquipment(eq1);
    storage.saveEquipment(eq2);
    storage.saveEquipment(eq3);
    
    auto forklifts = storage.findEquipmentByType(EquipmentType::Forklift);
    EXPECT_EQ(forklifts.size(), 2);
    
    auto cranes = storage.findEquipmentByType(EquipmentType::Crane);
    EXPECT_EQ(cranes.size(), 1);
    EXPECT_EQ(cranes[0].getId(), "id2");
}

TEST_F(DataStorageTest, FindEquipmentInAreaReturnsMatchingEquipment) {
    DataStorage storage(test_db_path);
    
    // Save equipment with different positions
    Equipment eq1("id1", EquipmentType::Forklift, "Forklift 1");
    eq1.setLastPosition(Position(40.7128, -74.0060)); // NYC
    
    Equipment eq2("id2", EquipmentType::Crane, "Crane 1");
    eq2.setLastPosition(Position(34.0522, -118.2437)); // LA
    
    Equipment eq3("id3", EquipmentType::Bulldozer, "Bulldozer 1");
    eq3.setLastPosition(Position(41.8781, -87.6298)); // Chicago
    
    storage.saveEquipment(eq1);
    storage.saveEquipment(eq2);
    storage.saveEquipment(eq3);
    
    // Search in East Coast area (should find NYC)
    auto eastCoast = storage.findEquipmentInArea(35.0, -80.0, 45.0, -70.0);
    EXPECT_EQ(eastCoast.size(), 1);
    EXPECT_EQ(eastCoast[0].getId(), "id1");
    
    // Search in West Coast area (should find LA)
    auto westCoast = storage.findEquipmentInArea(30.0, -125.0, 40.0, -115.0);
    EXPECT_EQ(westCoast.size(), 1);
    EXPECT_EQ(westCoast[0].getId(), "id2");
    
    // Search in Midwest area (should find Chicago)
    auto midwest = storage.findEquipmentInArea(40.0, -95.0, 45.0, -85.0);
    EXPECT_EQ(midwest.size(), 1);
    EXPECT_EQ(midwest[0].getId(), "id3");
    
    // Search in area with no equipment
    auto noEquipment = storage.findEquipmentInArea(0.0, 0.0, 10.0, 10.0);
    EXPECT_TRUE(noEquipment.empty());
}

TEST_F(DataStorageTest, ConcurrentOperationsAreThreadSafe) {
    DataStorage storage(test_db_path);
    storage.initialize();
    
    // Create and run multiple threads that perform operations concurrently
    std::vector<std::thread> threads;
    
    // Thread 1: Save equipment
    threads.emplace_back([&storage]() {
        for (int i = 0; i < 10; i++) {
            Equipment eq("thread1_" + std::to_string(i), EquipmentType::Forklift, "Thread 1 Equipment");
            storage.saveEquipment(eq);
        }
    });
    
    // Thread 2: Save different equipment
    threads.emplace_back([&storage]() {
        for (int i = 0; i < 10; i++) {
            Equipment eq("thread2_" + std::to_string(i), EquipmentType::Crane, "Thread 2 Equipment");
            storage.saveEquipment(eq);
        }
    });
    
    // Thread 3: Load equipment
    threads.emplace_back([&storage]() {
        for (int i = 0; i < 10; i++) {
            storage.loadEquipment("thread1_" + std::to_string(i));
            storage.loadEquipment("thread2_" + std::to_string(i));
        }
    });
    
    // Thread 4: Get all equipment
    threads.emplace_back([&storage]() {
        for (int i = 0; i < 5; i++) {
            storage.getAllEquipment();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify that all equipment was saved correctly
    auto all = storage.getAllEquipment();
    EXPECT_EQ(all.size(), 20);
}

TEST_F(DataStorageTest, HandlesMalformedEquipmentFile) {
    DataStorage storage(test_db_path);
    storage.initialize();
    
    // Create a malformed equipment file
    std::filesystem::create_directory(test_db_path + "/equipment");
    std::ofstream file(test_db_path + "/equipment/malformed.txt");
    file << "This is not a valid equipment file format";
    file.close();
    
    // Try to load the malformed equipment
    auto result = storage.loadEquipment("malformed");
    EXPECT_FALSE(result.has_value());
}

TEST_F(DataStorageTest, HandlesNonexistentDatabase) {
    // Use a path that definitely doesn't exist
    DataStorage storage("/path/that/definitely/does/not/exist/db");
    
    // Operations should fail gracefully
    EXPECT_FALSE(storage.loadEquipment("test123").has_value());
    EXPECT_TRUE(storage.getPositionHistory("test123").empty());
    EXPECT_TRUE(storage.getAllEquipment().empty());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>