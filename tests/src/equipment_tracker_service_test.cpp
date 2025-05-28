// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <iostream>
#include <algorithm>
#include <optional>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <mutex>

// Mock classes for dependencies
class GPSTracker {
public:
    using PositionCallback = std::function<void(double, double, double, uint64_t)>;
    
    MOCK_METHOD(void, registerPositionCallback, (PositionCallback callback));
    MOCK_METHOD(void, start, ());
    MOCK_METHOD(void, stop, ());
};

class DataStorage {
public:
    MOCK_METHOD(bool, initialize, ());
    MOCK_METHOD(bool, saveEquipment, (const class Equipment&));
    MOCK_METHOD(bool, deleteEquipment, (const std::string&));
    MOCK_METHOD(std::vector<class Equipment>, getAllEquipment, ());
    MOCK_METHOD(bool, savePosition, (const std::string&, const class Position&));
    MOCK_METHOD(bool, updateEquipment, (const class Equipment&));
};

class NetworkManager {
public:
    using CommandHandler = std::function<void(const std::string&)>;
    
    MOCK_METHOD(void, registerCommandHandler, (CommandHandler handler));
    MOCK_METHOD(void, connect, ());
    MOCK_METHOD(void, disconnect, ());
    MOCK_METHOD(void, sendPositionUpdate, (const std::string&, const class Position&));
};

// Forward declarations for types used in the service
enum class EquipmentStatus { Inactive, Active, Maintenance, OutOfService };
using EquipmentId = std::string;
using Timestamp = uint64_t;

constexpr double DEFAULT_POSITION_ACCURACY = 5.0;

// Position class
class Position {
public:
    Position(double lat, double lon, double alt, double acc, Timestamp ts)
        : latitude_(lat), longitude_(lon), altitude_(alt), accuracy_(acc), timestamp_(ts) {}
    
    double getLatitude() const { return latitude_; }
    double getLongitude() const { return longitude_; }
    double getAltitude() const { return altitude_; }
    double getAccuracy() const { return accuracy_; }
    Timestamp getTimestamp() const { return timestamp_; }
    
private:
    double latitude_;
    double longitude_;
    double altitude_;
    double accuracy_;
    Timestamp timestamp_;
};

// Equipment class
class Equipment {
public:
    Equipment(const EquipmentId& id, const std::string& name, EquipmentStatus status = EquipmentStatus::Inactive)
        : id_(id), name_(name), status_(status) {}
    
    EquipmentId getId() const { return id_; }
    std::string getName() const { return name_; }
    EquipmentStatus getStatus() const { return status_; }
    std::optional<Position> getLastPosition() const { return last_position_; }
    
    void setStatus(EquipmentStatus status) { status_ = status; }
    void recordPosition(const Position& position) { last_position_ = position; }
    
    std::string toString() const { 
        return "Equipment " + id_ + " (" + name_ + ")"; 
    }
    
private:
    EquipmentId id_;
    std::string name_;
    EquipmentStatus status_;
    std::optional<Position> last_position_;
};

// Include the class being tested
#include "equipment_tracker/equipment_tracker_service.h"

// Create mock classes for dependencies
class MockGPSTracker : public GPSTracker {
public:
    MOCK_METHOD(void, registerPositionCallback, (PositionCallback callback), (override));
    MOCK_METHOD(void, start, (), (override));
    MOCK_METHOD(void, stop, (), (override));
};

class MockDataStorage : public DataStorage {
public:
    MOCK_METHOD(bool, initialize, (), (override));
    MOCK_METHOD(bool, saveEquipment, (const Equipment&), (override));
    MOCK_METHOD(bool, deleteEquipment, (const EquipmentId&), (override));
    MOCK_METHOD(std::vector<Equipment>, getAllEquipment, (), (override));
    MOCK_METHOD(bool, savePosition, (const EquipmentId&, const Position&), (override));
    MOCK_METHOD(bool, updateEquipment, (const Equipment&), (override));
};

class MockNetworkManager : public NetworkManager {
public:
    MOCK_METHOD(void, registerCommandHandler, (CommandHandler handler), (override));
    MOCK_METHOD(void, connect, (), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(void, sendPositionUpdate, (const EquipmentId&, const Position&), (override));
};

namespace equipment_tracker {

// Test fixture
class EquipmentTrackerServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        gps_tracker_mock_ = new ::testing::NiceMock<MockGPSTracker>();
        data_storage_mock_ = new ::testing::NiceMock<MockDataStorage>();
        network_manager_mock_ = new ::testing::NiceMock<MockNetworkManager>();
        
        // Capture callbacks
        EXPECT_CALL(*gps_tracker_mock_, registerPositionCallback(::testing::_))
            .WillOnce(::testing::SaveArg<0>(&position_callback_));
        
        EXPECT_CALL(*network_manager_mock_, registerCommandHandler(::testing::_))
            .WillOnce(::testing::SaveArg<0>(&command_handler_));
        
        // Create service with mocks
        service_ = std::make_unique<EquipmentTrackerService>(
            std::unique_ptr<GPSTracker>(gps_tracker_mock_),
            std::unique_ptr<DataStorage>(data_storage_mock_),
            std::unique_ptr<NetworkManager>(network_manager_mock_)
        );
    }
    
    void TearDown() override {
        service_.reset();
    }
    
    // Helper method to add test equipment
    void AddTestEquipment() {
        Equipment e1("FORKLIFT-001", "Forklift 1", EquipmentStatus::Active);
        Equipment e2("BULLDOZER-001", "Bulldozer 1", EquipmentStatus::Inactive);
        Equipment e3("CRANE-001", "Crane 1", EquipmentStatus::Maintenance);
        
        EXPECT_CALL(*data_storage_mock_, saveEquipment(::testing::_))
            .WillRepeatedly(::testing::Return(true));
        
        service_->addEquipment(e1);
        service_->addEquipment(e2);
        service_->addEquipment(e3);
    }
    
    // Helper to simulate position with equipment
    void SimulatePositionWithEquipment(const EquipmentId& id, double lat, double lon, double alt) {
        Equipment equipment(id, "Test Equipment", EquipmentStatus::Active);
        Position position(lat, lon, alt, DEFAULT_POSITION_ACCURACY, 12345);
        equipment.recordPosition(position);
        
        EXPECT_CALL(*data_storage_mock_, saveEquipment(::testing::_))
            .WillOnce(::testing::Return(true));
        
        service_->addEquipment(equipment);
    }
    
    // Mocks (raw pointers for access in tests)
    MockGPSTracker* gps_tracker_mock_;
    MockDataStorage* data_storage_mock_;
    MockNetworkManager* network_manager_mock_;
    
    // Service under test
    std::unique_ptr<EquipmentTrackerService> service_;
    
    // Captured callbacks
    GPSTracker::PositionCallback position_callback_;
    NetworkManager::CommandHandler command_handler_;
};

// Constructor test
TEST_F(EquipmentTrackerServiceTest, ConstructorRegistersCallbacks) {
    // Callbacks should have been captured in SetUp
    EXPECT_TRUE(position_callback_ != nullptr);
    EXPECT_TRUE(command_handler_ != nullptr);
}

// Start/Stop tests
TEST_F(EquipmentTrackerServiceTest, StartInitializesComponentsWhenNotRunning) {
    // Setup expectations
    EXPECT_CALL(*data_storage_mock_, initialize())
        .WillOnce(::testing::Return(true));
    
    std::vector<Equipment> equipment_list;
    EXPECT_CALL(*data_storage_mock_, getAllEquipment())
        .WillOnce(::testing::Return(equipment_list));
    
    EXPECT_CALL(*network_manager_mock_, connect());
    EXPECT_CALL(*gps_tracker_mock_, start());
    
    // Call method under test
    service_->start();
    
    // Verify service is running
    EXPECT_TRUE(service_->isRunning());
}

TEST_F(EquipmentTrackerServiceTest, StartDoesNothingWhenAlreadyRunning) {
    // Setup initial state
    EXPECT_CALL(*data_storage_mock_, initialize())
        .WillOnce(::testing::Return(true));
    
    std::vector<Equipment> equipment_list;
    EXPECT_CALL(*data_storage_mock_, getAllEquipment())
        .WillOnce(::testing::Return(equipment_list));
    
    EXPECT_CALL(*network_manager_mock_, connect());
    EXPECT_CALL(*gps_tracker_mock_, start());
    
    service_->start();
    
    // No expectations for second call - nothing should happen
    service_->start();
}

TEST_F(EquipmentTrackerServiceTest, StartFailsWhenStorageInitializationFails) {
    // Setup expectations
    EXPECT_CALL(*data_storage_mock_, initialize())
        .WillOnce(::testing::Return(false));
    
    // These should not be called
    EXPECT_CALL(*network_manager_mock_, connect()).Times(0);
    EXPECT_CALL(*gps_tracker_mock_, start()).Times(0);
    
    // Call method under test
    service_->start();
    
    // Verify service is not running
    EXPECT_FALSE(service_->isRunning());
}

TEST_F(EquipmentTrackerServiceTest, StopShutdownsComponentsWhenRunning) {
    // Setup initial state
    EXPECT_CALL(*data_storage_mock_, initialize())
        .WillOnce(::testing::Return(true));
    
    std::vector<Equipment> equipment_list;
    EXPECT_CALL(*data_storage_mock_, getAllEquipment())
        .WillOnce(::testing::Return(equipment_list));
    
    EXPECT_CALL(*network_manager_mock_, connect());
    EXPECT_CALL(*gps_tracker_mock_, start());
    
    service_->start();
    
    // Setup expectations for stop
    EXPECT_CALL(*gps_tracker_mock_, stop());
    EXPECT_CALL(*network_manager_mock_, disconnect());
    
    // Call method under test
    service_->stop();
    
    // Verify service is not running
    EXPECT_FALSE(service_->isRunning());
}

TEST_F(EquipmentTrackerServiceTest, StopDoesNothingWhenNotRunning) {
    // No expectations - nothing should happen
    EXPECT_CALL(*gps_tracker_mock_, stop()).Times(0);
    EXPECT_CALL(*network_manager_mock_, disconnect()).Times(0);
    
    // Call method under test
    service_->stop();
    
    // Verify service is not running
    EXPECT_FALSE(service_->isRunning());
}

// Equipment management tests
TEST_F(EquipmentTrackerServiceTest, AddEquipmentSucceeds) {
    Equipment equipment("TEST-001", "Test Equipment");
    
    EXPECT_CALL(*data_storage_mock_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    
    bool result = service_->addEquipment(equipment);
    
    EXPECT_TRUE(result);
    
    // Verify equipment was added
    auto retrieved = service_->getEquipment("TEST-001");
    EXPECT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->getId(), "TEST-001");
    EXPECT_EQ(retrieved->getName(), "Test Equipment");
}

TEST_F(EquipmentTrackerServiceTest, AddEquipmentFailsForDuplicate) {
    Equipment equipment("TEST-001", "Test Equipment");
    
    EXPECT_CALL(*data_storage_mock_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    
    bool result1 = service_->addEquipment(equipment);
    EXPECT_TRUE(result1);
    
    // Try to add again - should fail
    bool result2 = service_->addEquipment(equipment);
    EXPECT_FALSE(result2);
}

TEST_F(EquipmentTrackerServiceTest, AddEquipmentFailsWhenStorageFails) {
    Equipment equipment("TEST-001", "Test Equipment");
    
    EXPECT_CALL(*data_storage_mock_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(false));
    
    bool result = service_->addEquipment(equipment);
    
    EXPECT_FALSE(result);
}

TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentSucceeds) {
    // Add equipment first
    Equipment equipment("TEST-001", "Test Equipment");
    
    EXPECT_CALL(*data_storage_mock_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    
    service_->addEquipment(equipment);
    
    // Now remove it
    EXPECT_CALL(*data_storage_mock_, deleteEquipment("TEST-001"))
        .WillOnce(::testing::Return(true));
    
    bool result = service_->removeEquipment("TEST-001");
    
    EXPECT_TRUE(result);
    
    // Verify equipment was removed
    auto retrieved = service_->getEquipment("TEST-001");
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentFailsForNonexistent) {
    bool result = service_->removeEquipment("NONEXISTENT");
    EXPECT_FALSE(result);
}

TEST_F(EquipmentTrackerServiceTest, GetEquipmentReturnsCorrectEquipment) {
    // Add equipment
    Equipment equipment("TEST-001", "Test Equipment");
    
    EXPECT_CALL(*data_storage_mock_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    
    service_->addEquipment(equipment);
    
    // Get equipment
    auto retrieved = service_->getEquipment("TEST-001");
    
    EXPECT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->getId(), "TEST-001");
    EXPECT_EQ(retrieved->getName(), "Test Equipment");
}

TEST_F(EquipmentTrackerServiceTest, GetEquipmentReturnsNulloptForNonexistent) {
    auto retrieved = service_->getEquipment("NONEXISTENT");
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(EquipmentTrackerServiceTest, GetAllEquipmentReturnsAllEquipment) {
    AddTestEquipment();
    
    auto all_equipment = service_->getAllEquipment();
    
    EXPECT_EQ(all_equipment.size(), 3);
    
    // Check if all equipment is present (order may vary)
    auto has_id = [&all_equipment](const std::string& id) {
        return std::any_of(all_equipment.begin(), all_equipment.end(),
                          [&id](const Equipment& e) { return e.getId() == id; });
    };
    
    EXPECT_TRUE(has_id("FORKLIFT-001"));
    EXPECT_TRUE(has_id("BULLDOZER-001"));
    EXPECT_TRUE(has_id("CRANE-001"));
}

TEST_F(EquipmentTrackerServiceTest, FindEquipmentByStatusReturnsCorrectEquipment) {
    AddTestEquipment();
    
    auto active_equipment = service_->findEquipmentByStatus(EquipmentStatus::Active);
    EXPECT_EQ(active_equipment.size(), 1);
    EXPECT_EQ(active_equipment[0].getId(), "FORKLIFT-001");
    
    auto maintenance_equipment = service_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(maintenance_equipment.size(), 1);
    EXPECT_EQ(maintenance_equipment[0].getId(), "CRANE-001");
    
    auto inactive_equipment = service_->findEquipmentByStatus(EquipmentStatus::Inactive);
    EXPECT_EQ(inactive_equipment.size(), 1);
    EXPECT_EQ(inactive_equipment[0].getId(), "BULLDOZER-001");
}

TEST_F(EquipmentTrackerServiceTest, FindActiveEquipmentReturnsActiveEquipment) {
    AddTestEquipment();
    
    auto active_equipment = service_->findActiveEquipment();
    
    EXPECT_EQ(active_equipment.size(), 1);
    EXPECT_EQ(active_equipment[0].getId(), "FORKLIFT-001");
}

TEST_F(EquipmentTrackerServiceTest, FindEquipmentInAreaReturnsCorrectEquipment) {
    // Add equipment with positions
    SimulatePositionWithEquipment("EQUIP-001", 10.0, 20.0, 100.0);
    SimulatePositionWithEquipment("EQUIP-002", 15.0, 25.0, 100.0);
    SimulatePositionWithEquipment("EQUIP-003", 30.0, 40.0, 100.0);
    
    // Search in area that includes first two equipment
    auto result = service_->findEquipmentInArea(5.0, 15.0, 20.0, 30.0);
    
    EXPECT_EQ(result.size(), 2);
    
    // Check if correct equipment is present (order may vary)
    auto has_id = [&result](const std::string& id) {
        return std::any_of(result.begin(), result.end(),
                          [&id](const Equipment& e) { return e.getId() == id; });
    };
    
    EXPECT_TRUE(has_id("EQUIP-001"));
    EXPECT_TRUE(has_id("EQUIP-002"));
    EXPECT_FALSE(has_id("EQUIP-003"));
}

TEST_F(EquipmentTrackerServiceTest, SetGeofenceReturnsTrue) {
    bool result = service_->setGeofence("TEST-001", 10.0, 20.0, 30.0, 40.0);
    EXPECT_TRUE(result);
}

// Callback handling tests
TEST_F(EquipmentTrackerServiceTest, HandlePositionUpdateUpdatesEquipment) {
    // Add equipment
    Equipment equipment("FORKLIFT-001", "Test Forklift");
    
    EXPECT_CALL(*data_storage_mock_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    
    service_->addEquipment(equipment);
    
    // Setup expectations for position update
    EXPECT_CALL(*data_storage_mock_, savePosition(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(true));
    
    EXPECT_CALL(*data_storage_mock_, updateEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    
    EXPECT_CALL(*network_manager_mock_, sendPositionUpdate(::testing::_, ::testing::_));
    
    // Trigger position callback
    position_callback_(10.0, 20.0, 100.0, 12345);
    
    // Verify equipment was updated
    auto updated = service_->getEquipment("FORKLIFT-001");
    EXPECT_TRUE(updated.has_value());
    EXPECT_EQ(updated->getStatus(), EquipmentStatus::Active);
    
    auto position = updated->getLastPosition();
    EXPECT_TRUE(position.has_value());
    EXPECT_EQ(position->getLatitude(), 10.0);
    EXPECT_EQ(position->getLongitude(), 20.0);
    EXPECT_EQ(position->getAltitude(), 100.0);
}

TEST_F(EquipmentTrackerServiceTest, HandleRemoteCommandProcessesStatusRequest) {
    AddTestEquipment();
    
    // Trigger command handler
    command_handler_("STATUS_REQUEST");
    
    // No specific expectations to verify, just ensure it doesn't crash
}

} // namespace equipment_tracker
// </test_code>