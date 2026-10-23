// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>

#include "equipment_tracker/data_storage.h"
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/types.h"
#include "equipment_tracker/utils/constants.h"

namespace equipment_tracker {
namespace {

// Helper to create a unique test directory for each test
class DataStorageTest : public ::testing::Test {
protected:
    std::string test_db_path_;

    void SetUp() override {
        // Create a unique path for each test
        test_db_path_ = "test_db_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        // Clean up if it exists from a previous failed run
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove_all(test_db_path_);
        }
    }

    void TearDown() override {
        // Clean up test directory
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove_all(test_db_path_);
        }
    }

    // Helper to create a Position with a specific time offset (seconds from epoch)
    Position makePosition(double lat, double lon, double alt, double acc, time_t time_val) {
        return Position(lat, lon, alt, acc, std::chrono::system_clock::from_time_t(time_val));
    }

    // Helper to create a basic Equipment
    Equipment makeEquipment(const std::string& id, EquipmentType type, const std::string& name) {
        return Equipment(id, type, name);
    }
};

// ==================== Initialization Tests ====================

TEST_F(DataStorageTest, ConstructorSetsPath) {
    DataStorage storage(test_db_path_);
    // Should not throw, just construct
    SUCCEED();
}

TEST_F(DataStorageTest, InitializeCreatesDirectories) {
    DataStorage storage(test_db_path_);
    ASSERT_TRUE(storage.initialize());
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ + "/equipment"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ + "/positions"));
}

TEST_F(DataStorageTest, InitializeTwiceSucceeds) {
    DataStorage storage(test_db_path_);
    ASSERT_TRUE(storage.initialize());
    ASSERT_TRUE(storage.initialize());
}

TEST_F(DataStorageTest, DefaultConstructorUsesDefaultPath) {
    // Just verify it compiles and constructs with default path
    DataStorage storage;
    // Don't initialize to avoid creating files in the working directory
    SUCCEED();
}

// ==================== Save Equipment Tests ====================

TEST_F(DataStorageTest, SaveEquipmentBasic) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Equipment eq("eq001", EquipmentType::Forklift, "Test Forklift");
    EXPECT_TRUE(storage.saveEquipment(eq));

    // Verify file was created
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ + "/equipment/eq001.txt"));
}

TEST_F(DataStorageTest, SaveEquipmentWithoutInitialize) {
    DataStorage storage(test_db_path_);
    // Don't call initialize() - saveEquipment should auto-initialize
    Equipment eq("eq002", EquipmentType::Crane, "Test Crane");
    EXPECT_TRUE(storage.saveEquipment(eq));
}

TEST_F(DataStorageTest, SaveEquipmentWithPosition) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Equipment eq("eq003", EquipmentType::Bulldozer, "Test Bulldozer");
    Position pos = makePosition(40.7128, -74.0060, 10.0, 3.0, 1000000);
    eq.setLastPosition(pos);
    eq.setStatus(EquipmentStatus::Active);

    EXPECT_TRUE(storage.saveEquipment(eq));

    // Verify file content
    std::ifstream file(test_db_path_ + "/equipment/eq003.txt");
    ASSERT_TRUE(file.is_open());
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    EXPECT_THAT(content, ::testing::HasSubstr("id=eq003"));
    EXPECT_THAT(content, ::testing::HasSubstr("name=Test Bulldozer"));
    EXPECT_THAT(content, ::testing::HasSubstr("last_position="));
}

TEST_F(DataStorageTest, SaveEquipmentWithoutPosition) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Equipment eq("eq004", EquipmentType::Truck, "Test Truck");
    EXPECT_TRUE(storage.saveEquipment(eq));

    // Verify file content does not have last_position
    std::ifstream file(test_db_path_ + "/equipment/eq004.txt");
    ASSERT_TRUE(file.is_open());
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    EXPECT_THAT(content, ::testing::Not(::testing::HasSubstr("last_position=")));
}

TEST_F(DataStorageTest, SaveEquipmentAllTypes) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    std::vector<EquipmentType> types = {
        EquipmentType::Forklift, EquipmentType::Crane,
        EquipmentType::Bulldozer, EquipmentType::Excavator,
        EquipmentType::Truck, EquipmentType::Other
    };

    for (size_t i = 0; i < types.size(); ++i) {
        Equipment eq("type_" + std::to_string(i), types[i], "Equipment " + std::to_string(i));
        EXPECT_TRUE(storage.saveEquipment(eq));
    }
}

TEST_F(DataStorageTest, SaveEquipmentAllStatuses) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    std::vector<EquipmentStatus> statuses = {
        EquipmentStatus::Active, EquipmentStatus::Inactive,
        EquipmentStatus::Maintenance, EquipmentStatus::Unknown
    };

    for (size_t i = 0; i < statuses.size(); ++i) {
        Equipment eq("status_" + std::to_string(i), EquipmentType::Other, "Equipment " + std::to_string(i));
        eq.setStatus(statuses[i]);
        EXPECT_TRUE(storage.saveEquipment(eq));
    }
}

// ==================== Load Equipment Tests ====================

TEST_F(DataStorageTest, LoadEquipmentBasic) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Equipment eq("eq_load_1", EquipmentType::Forklift, "Load Test Forklift");
    eq.setStatus(EquipmentStatus::Active);
    ASSERT_TRUE(storage.saveEquipment(eq));

    auto loaded = storage.loadEquipment("eq_load_1");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getId(), "eq_load_1");
    EXPECT_EQ(loaded->getName(), "Load Test Forklift");
    EXPECT_EQ(loaded->getType(), EquipmentType::Forklift);
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::Active);
}

TEST_F(DataStorageTest, LoadEquipmentWithPosition) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Equipment eq("eq_load_pos", EquipmentType::Crane, "Crane With Position");
    Position pos = makePosition(51.5074, -0.1278, 20.0, 5.0, 1500000);
    eq.setLastPosition(pos);
    ASSERT_TRUE(storage.saveEquipment(eq));

    auto loaded = storage.loadEquipment("eq_load_pos");
    ASSERT_TRUE(loaded.has_value());

    auto loaded_pos = loaded->getLastPosition();
    ASSERT_TRUE(loaded_pos.has_value());
    EXPECT_NEAR(loaded_pos->getLatitude(), 51.5074, 0.0001);
    EXPECT_NEAR(loaded_pos->getLongitude(), -0.1278, 0.0001);
    EXPECT_NEAR(loaded_pos->getAltitude(), 20.0, 0.0001);
    EXPECT_NEAR(loaded_pos->getAccuracy(), 5.0, 0.0001);
}

TEST_F(DataStorageTest, LoadEquipmentNonExistent) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    auto loaded = storage.loadEquipment("nonexistent_id");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(DataStorageTest, LoadEquipmentWithoutInitialize) {
    DataStorage storage(test_db_path_);
    // Don't call initialize()
    auto loaded = storage.loadEquipment("nonexistent_id");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(DataStorageTest, LoadEquipmentPreservesAllStatuses) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    std::vector<EquipmentStatus> statuses = {
        EquipmentStatus::Active, EquipmentStatus::Inactive,
        EquipmentStatus::Maintenance, EquipmentStatus::Unknown
    };

    for (size_t i = 0; i < statuses.size(); ++i) {
        std::string id = "status_test_" + std::to_string(i);
        Equipment eq(id, EquipmentType::Other, "Status Test " + std::to_string(i));
        eq.setStatus(statuses[i]);
        ASSERT_TRUE(storage.saveEquipment(eq));

        auto loaded = storage.loadEquipment(id);
        ASSERT_TRUE(loaded.has_value());
        EXPECT_EQ(loaded->getStatus(), statuses[i]);
    }
}

TEST_F(DataStorageTest, LoadEquipmentPreservesAllTypes) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    std::vector<EquipmentType> types = {
        EquipmentType::Forklift, EquipmentType::Crane,
        EquipmentType::Bulldozer, EquipmentType::Excavator,
        EquipmentType::Truck, EquipmentType::Other
    };

    for (size_t i = 0; i < types.size(); ++i) {
        std::string id = "type_test_" + std::to_string(i);
        Equipment eq(id, types[i], "Type Test " + std::to_string(i));
        ASSERT_TRUE(storage.saveEquipment(eq));

        auto loaded = storage.loadEquipment(id);
        ASSERT_TRUE(loaded.has_value());
        EXPECT_EQ(loaded->getType(), types[i]);
    }
}

// ==================== Update Equipment Tests ====================

TEST_F(DataStorageTest, UpdateEquipmentOverwrites) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Equipment eq("eq_update", EquipmentType::Forklift, "Original Name");
    eq.setStatus(EquipmentStatus::Active);
    ASSERT_TRUE(storage.saveEquipment(eq));

    // Update with new name and status
    Equipment eq_updated("eq_update", EquipmentType::Forklift, "Updated Name");
    eq_updated.setStatus(EquipmentStatus::Maintenance);
    ASSERT_TRUE(storage.updateEquipment(eq_updated));

    auto loaded = storage.loadEquipment("eq_update");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getName(), "Updated Name");
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::Maintenance);
}

// ==================== Delete Equipment Tests ====================

TEST_F(DataStorageTest, DeleteEquipmentBasic) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Equipment eq("eq_delete", EquipmentType::Forklift, "To Delete");
    ASSERT_TRUE(storage.saveEquipment(eq));

    // Verify it exists
    auto loaded = storage.loadEquipment("eq_delete");
    ASSERT_TRUE(loaded.has_value());

    // Delete
    EXPECT_TRUE(storage.deleteEquipment("eq_delete"));

    // Verify it's gone
    loaded = storage.loadEquipment("eq_delete");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(DataStorageTest, DeleteEquipmentNonExistent) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    // Deleting non-existent equipment should still return true
    EXPECT_TRUE(storage.deleteEquipment("nonexistent_id"));
}

TEST_F(DataStorageTest, DeleteEquipmentWithPositionHistory) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Equipment eq("eq_del_hist", EquipmentType::Crane, "Delete With History");
    ASSERT_TRUE(storage.saveEquipment(eq));

    // Save some positions
    Position pos1 = makePosition(10.0, 20.0, 0.0, 2.5, 1000000);
    Position pos2 = makePosition(10.1, 20.1, 0.0, 2.5, 1000100);
    ASSERT_TRUE(storage.savePosition("eq_del_hist", pos1));
    ASSERT_TRUE(storage.savePosition("eq_del_hist", pos2));

    // Delete
    EXPECT_TRUE(storage.deleteEquipment("eq_del_hist"));

    // Verify equipment file is gone
    EXPECT_FALSE(std::filesystem::exists(test_db_path_ + "/equipment/eq_del_hist.txt"));

    // Verify position directory is gone
    EXPECT_FALSE(std::filesystem::exists(test_db_path_ + "/positions/eq_del_hist"));
}

TEST_F(DataStorageTest, DeleteEquipmentWithoutInitialize) {
    DataStorage storage(test_db_path_);
    // Should auto-initialize and succeed
    EXPECT_TRUE(storage.deleteEquipment("nonexistent"));
}

// ==================== Save Position Tests ====================

TEST_F(DataStorageTest, SavePositionBasic) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Position pos = makePosition(48.8566, 2.3522, 35.0, 1.5, 2000000);
    EXPECT_TRUE(storage.savePosition("eq_pos_1", pos));

    // Verify directory and file were created
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ + "/positions/eq_pos_1"));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_ + "/positions/eq_pos_1/2000000.txt"));
}

TEST_F(DataStorageTest, SavePositionMultiple) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    for (int i = 0; i < 5; ++i) {
        Position pos = makePosition(48.0 + i * 0.01, 2.0 + i * 0.01, 0.0, 2.5, 3000000 + i * 100);
        EXPECT_TRUE(storage.savePosition("eq_multi_pos", pos));
    }

    // Verify 5 files were created
    int file_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(test_db_path_ + "/positions/eq_multi_pos")) {
        if (entry.is_regular_file()) {
            file_count++;
        }
    }
    EXPECT_EQ(file_count, 5);
}

TEST_F(DataStorageTest, SavePositionWithoutInitialize) {
    DataStorage storage(test_db_path_);
    Position pos = makePosition(10.0, 20.0, 0.0, 2.5, 1000000);
    EXPECT_TRUE(storage.savePosition("eq_auto_init", pos));
}

TEST_F(DataStorageTest, SavePositionFileContent) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Position pos = makePosition(35.6762, 139.6503, 40.0, 3.5, 4000000);
    ASSERT_TRUE(storage.savePosition("eq_content", pos));

    std::ifstream file(test_db_path_ + "/positions/eq_content/4000000.txt");
    ASSERT_TRUE(file.is_open());
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    EXPECT_THAT(content, ::testing::HasSubstr("latitude="));
    EXPECT_THAT(content, ::testing::HasSubstr("longitude="));
    EXPECT_THAT(content, ::testing::HasSubstr("altitude="));
    EXPECT_THAT(content, ::testing::HasSubstr("accuracy="));
    EXPECT_THAT(content, ::testing::HasSubstr("timestamp=4000000"));
}

// ==================== Get Position History Tests ====================

TEST_F(DataStorageTest, GetPositionHistoryBasic) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    // Save positions
    time_t base_time = 5000000;
    for (int i = 0; i < 3; ++i) {
        Position pos = makePosition(10.0 + i, 20.0 + i, 0.0, 2.5, base_time + i * 100);
        ASSERT_TRUE(storage.savePosition("eq_hist", pos));
    }

    // Get all history
    auto start = std::chrono::system_clock::from_time_t(base_time - 1);
    auto end = std::chrono::system_clock::from_time_t(base_time + 300);
    auto history = storage.getPositionHistory("eq_hist", start, end);

    EXPECT_EQ(history.size(), 3u);
}

TEST_F(DataStorageTest, GetPositionHistoryTimeRange) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    time_t base_time = 6000000;
    for (int i = 0; i < 5; ++i) {
        Position pos = makePosition(10.0 + i, 20.0 + i, 0.0, 2.5, base_time + i * 1000);
        ASSERT_TRUE(storage.savePosition("eq_range", pos));
    }

    // Get only middle positions (indices 1, 2, 3)
    auto start = std::chrono::system_clock::from_time_t(base_time + 1000);
    auto end = std::chrono::system_clock::from_time_t(base_time + 3000);
    auto history = storage.getPositionHistory("eq_range", start, end);

    EXPECT_EQ(history.size(), 3u);
}

TEST_F(DataStorageTest, GetPositionHistoryEmpty) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    auto start = std::chrono::system_clock::from_time_t(0);
    auto end = std::chrono::system_clock::from_time_t(9999999);
    auto history = storage.getPositionHistory("nonexistent_eq", start, end);

    EXPECT_TRUE(history.empty());
}

TEST_F(DataStorageTest, GetPositionHistoryNoMatchingRange) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    time_t base_time = 7000000;
    Position pos = makePosition(10.0, 20.0, 0.0, 2.5, base_time);
    ASSERT_TRUE(storage.savePosition("eq_no_match", pos));

    // Query a range that doesn't include the saved position
    auto start = std::chrono::system_clock::from_time_t(base_time + 1000);
    auto end = std::chrono::system_clock::from_time_t(base_time + 2000);
    auto history = storage.getPositionHistory("eq_no_match", start, end);

    EXPECT_TRUE(history.empty());
}

TEST_F(DataStorageTest, GetPositionHistoryVerifyValues) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    time_t ts = 8000000;
    Position pos = makePosition(45.123456, -93.654321, 100.5, 1.2, ts);
    ASSERT_TRUE(storage.savePosition("eq_verify", pos));

    auto start = std::chrono::system_clock::from_time_t(ts - 1);
    auto end = std::chrono::system_clock::from_time_t(ts + 1);
    auto history = storage.getPositionHistory("eq_verify", start, end);

    ASSERT_EQ(history.size(), 1u);
    EXPECT_NEAR(history[0].getLatitude(), 45.123456, 0.0001);
    EXPECT_NEAR(history[0].getLongitude(), -93.654321, 0.0001);
    EXPECT_NEAR(history[0].getAltitude(), 100.5, 0.01);
    EXPECT_NEAR(history[0].getAccuracy(), 1.2, 0.01);
}

TEST_F(DataStorageTest, GetPositionHistorySorted) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    time_t base_time = 9000000;
    // Save in reverse order
    for (int i = 4; i >= 0; --i) {
        Position pos = makePosition(10.0 + i, 20.0, 0.0, 2.5, base_time + i * 1000);
        ASSERT_TRUE(storage.savePosition("eq_sorted", pos));
    }

    auto start = std::chrono::system_clock::from_time_t(base_time - 1);
    auto end = std::chrono::system_clock::from_time_t(base_time + 5000);
    auto history = storage.getPositionHistory("eq_sorted", start, end);

    ASSERT_EQ(history.size(), 5u);
    // Verify sorted by timestamp (ascending)
    for (size_t i = 1; i < history.size(); ++i) {
        EXPECT_LE(history[i - 1].getTimestamp(), history[i].getTimestamp());
    }
}

// ==================== Get All Equipment Tests ====================

TEST_F(DataStorageTest, GetAllEquipmentEmpty) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    auto all = storage.getAllEquipment();
    EXPECT_TRUE(all.empty());
}

TEST_F(DataStorageTest, GetAllEquipmentMultiple) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Equipment eq1("all_1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("all_2", EquipmentType::Crane, "Crane 1");
    Equipment eq3("all_3", EquipmentType::Truck, "Truck 1");

    ASSERT_TRUE(storage.saveEquipment(eq1));
    ASSERT_TRUE(storage.saveEquipment(eq2));
    ASSERT_TRUE(storage.saveEquipment(eq3));

    auto all = storage.getAllEquipment();
    EXPECT_EQ(all.size(), 3u);

    // Verify all IDs are present
    std::vector<std::string> ids;
    for (const auto& eq : all) {
        ids.push_back(eq.getId());
    }
    std::sort(ids.begin(), ids.end());
    EXPECT_EQ(ids[0], "all_1");
    EXPECT_EQ(ids[1], "all_2");
    EXPECT_EQ(ids[2], "all_3");
}

TEST_F(DataStorageTest, GetAllEquipmentWithoutInitialize) {
    DataStorage storage(test_db_path_);
    auto all = storage.getAllEquipment();
    EXPECT_TRUE(all.empty());
}

// ==================== Find Equipment By Status Tests ====================

TEST_F(DataStorageTest, FindEquipmentByStatusActive) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Equipment eq1("status_1", EquipmentType::Forklift, "Active Forklift");
    eq1.setStatus(EquipmentStatus::Active);
    Equipment eq2("status_2", EquipmentType::Crane, "Inactive Crane");
    eq2.setStatus(EquipmentStatus::Inactive);
    Equipment eq3("status_3", EquipmentType::Truck, "Active Truck");
    eq3.setStatus(EquipmentStatus::Active);

    ASSERT_TRUE(storage.saveEquipment(eq1));
    ASSERT_TRUE(storage.saveEquipment(eq2));
    ASSERT_TRUE(storage.saveEquipment(eq3));

    auto active = storage.findEquipmentByStatus(EquipmentStatus::Active);
    EXPECT_EQ(active.size(), 2u);

    for (const auto& eq : active) {
        EXPECT_EQ(eq.getStatus(), EquipmentStatus::Active);
    }
}

TEST_F(DataStorageTest, FindEquipmentByStatusNoMatch) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Equipment eq1("no_match_1", EquipmentType::Forklift, "Active Forklift");
    eq1.setStatus(EquipmentStatus::Active);
    ASSERT_TRUE(storage.saveEquipment(eq1));

    auto maintenance = storage.findEquipmentByStatus(EquipmentStatus::Maintenance);
    EXPECT_TRUE(maintenance.empty());
}

TEST_F(DataStorageTest, FindEquipmentByStatusEmpty) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    auto result = storage.findEquipmentByStatus(EquipmentStatus::Active);
    EXPECT_TRUE(result.empty());
}

// ==================== Find Equipment By Type Tests ====================

TEST_F(DataStorageTest, FindEquipmentByTypeForklift) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Equipment eq1("type_f1", EquipmentType::Forklift, "Forklift 1");
    Equipment eq2("type_c1", EquipmentType::Crane, "Crane 1");
    Equipment eq3("type_f2", EquipmentType::Forklift, "Forklift 2");

    ASSERT_TRUE(storage.saveEquipment(eq1));
    ASSERT_TRUE(storage.saveEquipment(eq2));
    ASSERT_TRUE(storage.saveEquipment(eq3));

    auto forklifts = storage.findEquipmentByType(EquipmentType::Forklift);
    EXPECT_EQ(forklifts.size(), 2u);

    for (const auto& eq : forklifts) {
        EXPECT_EQ(eq.getType(), EquipmentType::Forklift);
    }
}

TEST_F(DataStorageTest, FindEquipmentByTypeNoMatch) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Equipment eq1("type_nm", EquipmentType::Forklift, "Forklift");
    ASSERT_TRUE(storage.saveEquipment(eq1));

    auto excavators = storage.findEquipmentByType(EquipmentType::Excavator);
    EXPECT_TRUE(excavators.empty());
}

// ==================== Find Equipment In Area Tests ====================

TEST_F(DataStorageTest, FindEquipmentInAreaBasic) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    // Equipment inside area
    Equipment eq1("area_in", EquipmentType::Forklift, "Inside");
    Position pos1 = makePosition(40.0, -74.0, 0.0, 2.5, 10000000);
    eq1.setLastPosition(pos1);
    ASSERT_TRUE(storage.saveEquipment(eq1));

    // Equipment outside area
    Equipment eq2("area_out", EquipmentType::Crane, "Outside");
    Position pos2 = makePosition(50.0, -80.0, 0.0, 2.5, 10000000);
    eq2.setLastPosition(pos2);
    ASSERT_TRUE(storage.saveEquipment(eq2));

    auto result = storage.findEquipmentInArea(39.0, -75.0, 41.0, -73.0);
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].getId(), "area_in");
}

TEST_F(DataStorageTest, FindEquipmentInAreaReversedCoordinates) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Equipment eq("area_rev", EquipmentType::Forklift, "Inside");
    Position pos = makePosition(40.0, -74.0, 0.0, 2.5, 10000000);
    eq.setLastPosition(pos);
    ASSERT_TRUE(storage.saveEquipment(eq));

    // Pass coordinates in reversed order (lat2 < lat1, lon2 < lon1)
    auto result = storage.findEquipmentInArea(41.0, -73.0, 39.0, -75.0);
    EXPECT_EQ(result.size(), 1u);
}

TEST_F(DataStorageTest, FindEquipmentInAreaNoPosition) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    // Equipment without position should not be found
    Equipment eq("area_nopos", EquipmentType::Forklift, "No Position");
    ASSERT_TRUE(storage.saveEquipment(eq));

    auto result = storage.findEquipmentInArea(-90.0, -180.0, 90.0, 180.0);
    EXPECT_TRUE(result.empty());
}

TEST_F(DataStorageTest, FindEquipmentInAreaEmpty) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    auto result = storage.findEquipmentInArea(0.0, 0.0, 1.0, 1.0);
    EXPECT_TRUE(result.empty());
}

TEST_F(DataStorageTest, FindEquipmentInAreaBoundary) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    // Equipment exactly on the boundary
    Equipment eq("area_boundary", EquipmentType::Forklift, "On Boundary");
    Position pos = makePosition(40.0, -74.0, 0.0, 2.5, 10000000);
    eq.setLastPosition(pos);
    ASSERT_TRUE(storage.saveEquipment(eq));

    // Area where equipment is exactly on the edge
    auto result = storage.findEquipmentInArea(40.0, -74.0, 41.0, -73.0);
    EXPECT_EQ(result.size(), 1u);
}

// ==================== Round-trip Tests ====================

TEST_F(DataStorageTest, EquipmentRoundTrip) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Equipment eq("roundtrip", EquipmentType::Excavator, "Excavator RT");
    eq.setStatus(EquipmentStatus::Maintenance);
    Position pos = makePosition(-33.8688, 151.2093, 58.0, 2.0, 11000000);
    eq.setLastPosition(pos);

    ASSERT_TRUE(storage.saveEquipment(eq));

    auto loaded = storage.loadEquipment("roundtrip");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getId(), "roundtrip");
    EXPECT_EQ(loaded->getName(), "Excavator RT");
    EXPECT_EQ(loaded->getType(), EquipmentType::Excavator);
    EXPECT_EQ(loaded->getStatus(), EquipmentStatus::Maintenance);

    auto loaded_pos = loaded->getLastPosition();
    ASSERT_TRUE(loaded_pos.has_value());
    EXPECT_NEAR(loaded_pos->getLatitude(), -33.8688, 0.0001);
    EXPECT_NEAR(loaded_pos->getLongitude(), 151.2093, 0.0001);
    EXPECT_NEAR(loaded_pos->getAltitude(), 58.0, 0.01);
    EXPECT_NEAR(loaded_pos->getAccuracy(), 2.0, 0.01);
}

TEST_F(DataStorageTest, PositionHistoryRoundTrip) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    time_t base = 12000000;
    std::vector<Position> positions;
    for (int i = 0; i < 3; ++i) {
        positions.push_back(makePosition(10.0 + i * 0.1, 20.0 + i * 0.1, i * 10.0, 2.5, base + i * 60));
        ASSERT_TRUE(storage.savePosition("rt_pos", positions.back()));
    }

    auto start = std::chrono::system_clock::from_time_t(base - 1);
    auto end = std::chrono::system_clock::from_time_t(base + 200);
    auto history = storage.getPositionHistory("rt_pos", start, end);

    ASSERT_EQ(history.size(), 3u);
    for (size_t i = 0; i < history.size(); ++i) {
        EXPECT_NEAR(history[i].getLatitude(), 10.0 + i * 0.1, 0.0001);
        EXPECT_NEAR(history[i].getLongitude(), 20.0 + i * 0.1, 0.0001);
        EXPECT_NEAR(history[i].getAltitude(), i * 10.0, 0.01);
    }
}

// ==================== Edge Cases ====================

TEST_F(DataStorageTest, SaveAndLoadEquipmentWithEmptyName) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Equipment eq("empty_name", EquipmentType::Other, "");
    ASSERT_TRUE(storage.saveEquipment(eq));

    auto loaded = storage.loadEquipment("empty_name");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getName(), "");
}

TEST_F(DataStorageTest, SaveEquipmentOverwrite) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Equipment eq1("overwrite", EquipmentType::Forklift, "Version 1");
    ASSERT_TRUE(storage.saveEquipment(eq1));

    Equipment eq2("overwrite", EquipmentType::Crane, "Version 2");
    ASSERT_TRUE(storage.saveEquipment(eq2));

    auto loaded = storage.loadEquipment("overwrite");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->getName(), "Version 2");
    EXPECT_EQ(loaded->getType(), EquipmentType::Crane);
}

TEST_F(DataStorageTest, NegativeCoordinates) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Position pos = makePosition(-45.0, -170.0, -10.0, 2.5, 13000000);
    ASSERT_TRUE(storage.savePosition("neg_coords", pos));

    auto start = std::chrono::system_clock::from_time_t(13000000 - 1);
    auto end = std::chrono::system_clock::from_time_t(13000000 + 1);
    auto history = storage.getPositionHistory("neg_coords", start, end);

    ASSERT_EQ(history.size(), 1u);
    EXPECT_NEAR(history[0].getLatitude(), -45.0, 0.0001);
    EXPECT_NEAR(history[0].getLongitude(), -170.0, 0.0001);
    EXPECT_NEAR(history[0].getAltitude(), -10.0, 0.01);
}

TEST_F(DataStorageTest, ZeroCoordinates) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Position pos = makePosition(0.0, 0.0, 0.0, 0.0, 14000000);
    ASSERT_TRUE(storage.savePosition("zero_coords", pos));

    auto start = std::chrono::system_clock::from_time_t(14000000 - 1);
    auto end = std::chrono::system_clock::from_time_t(14000000 + 1);
    auto history = storage.getPositionHistory("zero_coords", start, end);

    ASSERT_EQ(history.size(), 1u);
    EXPECT_NEAR(history[0].getLatitude(), 0.0, 0.0001);
    EXPECT_NEAR(history[0].getLongitude(), 0.0, 0.0001);
}

// ==================== Execute Query Test ====================

TEST_F(DataStorageTest, ExecuteQueryPlaceholder) {
    DataStorage storage(test_db_path_);
    storage.initialize();
    // executeQuery is private, but we can test it indirectly
    // The method is a placeholder that always returns true
    // We just verify the class works correctly overall
    SUCCEED();
}

// ==================== Thread Safety Tests ====================

TEST_F(DataStorageTest, ConcurrentSaveAndLoad) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    // Save initial equipment
    Equipment eq("concurrent", EquipmentType::Forklift, "Concurrent Test");
    ASSERT_TRUE(storage.saveEquipment(eq));

    const int num_threads = 4;
    const int ops_per_thread = 10;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&storage, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string id = "thread_" + std::to_string(t) + "_" + std::to_string(i);
                Equipment eq(id, EquipmentType::Forklift, "Thread Equipment");
                storage.saveEquipment(eq);
                storage.loadEquipment(id);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify all equipment was saved
    auto all = storage.getAllEquipment();
    // Should have the initial one plus all thread-created ones
    EXPECT_EQ(all.size(), static_cast<size_t>(1 + num_threads * ops_per_thread));
}

// ==================== Load Equipment With Position History ====================

TEST_F(DataStorageTest, LoadEquipmentIncludesPositionHistory) {
    DataStorage storage(test_db_path_);
    storage.initialize();

    Equipment eq("eq_with_hist", EquipmentType::Forklift, "With History");
    ASSERT_TRUE(storage.saveEquipment(eq));

    // Save position history
    time_t base = 15000000;
    for (int i = 0; i < 3; ++i) {
        Position pos = makePosition(10.0 + i, 20.0 + i, 0.0, 2.5, base + i * 100);
        ASSERT_TRUE(storage.savePosition("eq_with_hist", pos));
    }

    auto loaded = storage.loadEquipment("eq_with_hist");
    ASSERT_TRUE(loaded.has_value());

    // The loaded equipment should have position history
    auto history = loaded->getPositionHistory();
    EXPECT_EQ(history.size(), 3u);
}

// ==================== Path with subdirectories ====================

TEST_F(DataStorageTest, NestedDbPath) {
    std::string nested_path = test_db_path_ + "/sub1/sub2/db";
    DataStorage storage(nested_path);
    ASSERT_TRUE(storage.initialize());
    EXPECT_TRUE(std::filesystem::exists(nested_path));

    // Clean up
    std::filesystem::remove_all(test_db_path_);
}

} // anonymous namespace
} // namespace equipment_tracker
// </test_code>