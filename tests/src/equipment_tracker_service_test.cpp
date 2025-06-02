// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/equipment_tracker_service.h"
#include "equipment_tracker/utils/types.h"
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/position.h"

namespace equipment_tracker {

// Mock classes for dependencies
class MockGPSTracker : public GPSTracker {
public:
    MockGPSTracker() : GPSTracker(DEFAULT_UPDATE_INTERVAL_MS) {}
    
    MOCK_METHOD(void, start, (), (override));
    MOCK_METHOD(void, stop, (), (override));
    MOCK_METHOD(bool, isRunning, (), (const, override));
    MOCK_METHOD(void, registerPositionCallback, (PositionCallback), (override));
    MOCK_METHOD(void, simulatePosition, (double, double, double), (override));
};

class MockDataStorage : public DataStorage {
public:
    MockDataStorage() : DataStorage(DEFAULT_DB_PATH) {}
    
    MOCK_METHOD(bool, initialize, (), (override));
    MOCK_METHOD(bool, saveEquipment, (const Equipment&), (override));
    MOCK_METHOD(std::optional<Equipment>, loadEquipment, (const EquipmentId&), (override));
    MOCK_METHOD(bool, updateEquipment, (const Equipment&), (override));
    MOCK_METHOD(bool, deleteEquipment, (const EquipmentId&), (override));
    MOCK_METHOD(bool, savePosition, (const EquipmentId&, const Position&), (override));
    MOCK_METHOD(std::vector<Position>, getPositionHistory, (const EquipmentId&, const Timestamp&, const Timestamp&), (override));
    MOCK_METHOD(std::vector<Equipment>, getAllEquipment, (), (override));
    MOCK_METHOD(std::vector<Equipment>, findEquipmentByStatus, (EquipmentStatus), (override));
    MOCK_METHOD(std::vector<Equipment>, findEquipmentByType, (EquipmentType), (override));
    MOCK_METHOD(std::vector<Equipment>, findEquipmentInArea, (double, double, double, double), (override));
};

class MockNetworkManager : public NetworkManager {
public:
    MockNetworkManager() : NetworkManager(DEFAULT_SERVER_URL, DEFAULT_SERVER_PORT) {}
    
    MOCK_METHOD(bool, connect, (), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(bool, isConnected, (), (const, override));
    MOCK_METHOD(bool, sendPositionUpdate, (const EquipmentId&, const Position&), (override));
    MOCK_METHOD(bool, syncWithServer, (), (override));
    MOCK_METHOD(void, registerCommandHandler, (std::function<void(const std::string&)>), (override));
};

// Custom service class that uses our mocks
class TestableEquipmentTrackerService : public EquipmentTrackerService {
public:
    TestableEquipmentTrackerService(
        std::unique_ptr<MockGPSTracker> gps_tracker,
        std::unique_ptr<MockDataStorage> data_storage,
        std::unique_ptr<MockNetworkManager> network_manager
    ) {
        gps_tracker_ = std::move(gps_tracker);
        data_storage_ = std::move(data_storage);
        network_manager_ = std::move(network_manager);
    }
    
    MockGPSTracker& getMockGPSTracker() {
        return static_cast<MockGPSTracker&>(*gps_tracker_);
    }
    
    MockDataStorage& getMockDataStorage() {
        return static_cast<MockDataStorage&>(*data_storage_);
    }
    
    MockNetworkManager& getMockNetworkManager() {
        return static_cast<MockNetworkManager&>(*network_manager_);
    }
    
    // Expose private methods for testing
    void testHandlePositionUpdate(double lat, double lon, double alt, Timestamp timestamp) {
        handlePositionUpdate(lat, lon, alt, timestamp);
    }
    
    void testHandleRemoteCommand(const std::string& command) {
        handleRemoteCommand(command);
    }
    
    std::optional<EquipmentId> testDetermineEquipmentId() {
        return determineEquipmentId();
    }
};

class EquipmentTrackerServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_gps_tracker = std::make_unique<MockGPSTracker>();
        mock_data_storage = std::make_unique<MockDataStorage>();
        mock_network_manager = std::make_unique<MockNetworkManager>();
        
        // Set up default behaviors for mocks
        EXPECT_CALL(*mock_gps_tracker, registerPositionCallback(::testing::_)).Times(1);
        EXPECT_CALL(*mock_network_manager, registerCommandHandler(::testing::_)).Times(1);
        
        // Keep raw pointers for expectations
        gps_tracker_ptr = mock_gps_tracker.get();
        data_storage_ptr = mock_data_storage.get();
        network_manager_ptr = mock_network_manager.get();
        
        service = std::make_unique<TestableEquipmentTrackerService>(
            std::move(mock_gps_tracker),
            std::move(mock_data_storage),
            std::move(mock_network_manager)
        );
    }
    
    // Helper method to create test equipment
    Equipment createTestEquipment(const std::string& id, EquipmentType type = EquipmentType::Forklift) {
        return Equipment(id, type, "Test Equipment " + id);
    }
    
    std::unique_ptr<TestableEquipmentTrackerService> service;
    std::unique_ptr<MockGPSTracker> mock_gps_tracker;
    std::unique_ptr<MockDataStorage> mock_data_storage;
    std::unique_ptr<MockNetworkManager> mock_network_manager;
    
    // Raw pointers for setting expectations after move
    MockGPSTracker* gps_tracker_ptr;
    MockDataStorage* data_storage_ptr;
    MockNetworkManager* network_manager_ptr;
};

TEST_F(EquipmentTrackerServiceTest, StartInitializesComponentsCorrectly) {
    // Setup expectations
    EXPECT_CALL(service->getMockDataStorage(), initialize())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(service->getMockDataStorage(), getAllEquipment())
        .WillOnce(::testing::Return(std::vector<Equipment>{}));
    EXPECT_CALL(service->getMockNetworkManager(), connect())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(service->getMockGPSTracker(), start())
        .Times(1);
    
    // Execute
    service->start();
    
    // Verify
    EXPECT_TRUE(service->isRunning());
}

TEST_F(EquipmentTrackerServiceTest, StartFailsWhenDataStorageInitializationFails) {
    // Setup expectations
    EXPECT_CALL(service->getMockDataStorage(), initialize())
        .WillOnce(::testing::Return(false));
    
    // These should not be called if initialization fails
    EXPECT_CALL(service->getMockNetworkManager(), connect())
        .Times(0);
    EXPECT_CALL(service->getMockGPSTracker(), start())
        .Times(0);
    
    // Execute
    service->start();
    
    // Verify
    EXPECT_FALSE(service->isRunning());
}

TEST_F(EquipmentTrackerServiceTest, StartDoesNothingIfAlreadyRunning) {
    // First start normally
    EXPECT_CALL(service->getMockDataStorage(), initialize())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(service->getMockDataStorage(), getAllEquipment())
        .WillOnce(::testing::Return(std::vector<Equipment>{}));
    EXPECT_CALL(service->getMockNetworkManager(), connect())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(service->getMockGPSTracker(), start())
        .Times(1);
    
    service->start();
    EXPECT_TRUE(service->isRunning());
    
    // Now try to start again - no methods should be called
    service->start();
}

TEST_F(EquipmentTrackerServiceTest, StopShutdownsComponentsCorrectly) {
    // First start the service
    EXPECT_CALL(service->getMockDataStorage(), initialize())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(service->getMockDataStorage(), getAllEquipment())
        .WillOnce(::testing::Return(std::vector<Equipment>{}));
    EXPECT_CALL(service->getMockNetworkManager(), connect())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(service->getMockGPSTracker(), start())
        .Times(1);
    
    service->start();
    
    // Setup expectations for stop
    EXPECT_CALL(service->getMockGPSTracker(), stop())
        .Times(1);
    EXPECT_CALL(service->getMockNetworkManager(), disconnect())
        .Times(1);
    
    // Execute
    service->stop();
    
    // Verify
    EXPECT_FALSE(service->isRunning());
}

TEST_F(EquipmentTrackerServiceTest, StopDoesNothingIfNotRunning) {
    // Service is not running initially
    EXPECT_FALSE(service->isRunning());
    
    // These should not be called if not running
    EXPECT_CALL(service->getMockGPSTracker(), stop())
        .Times(0);
    EXPECT_CALL(service->getMockNetworkManager(), disconnect())
        .Times(0);
    
    // Execute
    service->stop();
    
    // Verify still not running
    EXPECT_FALSE(service->isRunning());
}

TEST_F(EquipmentTrackerServiceTest, AddEquipmentSucceeds) {
    // Create test equipment
    Equipment test_equipment = createTestEquipment("TEST-001");
    
    // Setup expectations
    EXPECT_CALL(service->getMockDataStorage(), saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    
    // Execute
    bool result = service->addEquipment(test_equipment);
    
    // Verify
    EXPECT_TRUE(result);
    
    // Verify equipment was added to internal map
    auto retrieved = service->getEquipment("TEST-001");
    EXPECT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->getId(), "TEST-001");
}

TEST_F(EquipmentTrackerServiceTest, AddEquipmentFailsForDuplicateId) {
    // Create test equipment
    Equipment test_equipment = createTestEquipment("TEST-001");
    
    // Add it first time
    EXPECT_CALL(service->getMockDataStorage(), saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    bool first_result = service->addEquipment(test_equipment);
    EXPECT_TRUE(first_result);
    
    // Try to add again with same ID
    Equipment duplicate_equipment = createTestEquipment("TEST-001", EquipmentType::Crane);
    
    // Storage should not be called for duplicate
    EXPECT_CALL(service->getMockDataStorage(), saveEquipment(::testing::_))
        .Times(0);
    
    // Execute
    bool second_result = service->addEquipment(duplicate_equipment);
    
    // Verify
    EXPECT_FALSE(second_result);
}

TEST_F(EquipmentTrackerServiceTest, AddEquipmentFailsWhenStorageFails) {
    // Create test equipment
    Equipment test_equipment = createTestEquipment("TEST-001");
    
    // Setup expectations - storage fails
    EXPECT_CALL(service->getMockDataStorage(), saveEquipment(::testing::_))
        .WillOnce(::testing::Return(false));
    
    // Execute
    bool result = service->addEquipment(test_equipment);
    
    // Verify
    EXPECT_FALSE(result);
    
    // Verify equipment was not added to internal map
    auto retrieved = service->getEquipment("TEST-001");
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentSucceeds) {
    // First add equipment
    Equipment test_equipment = createTestEquipment("TEST-001");
    
    EXPECT_CALL(service->getMockDataStorage(), saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    service->addEquipment(test_equipment);
    
    // Setup expectations for removal
    EXPECT_CALL(service->getMockDataStorage(), deleteEquipment("TEST-001"))
        .WillOnce(::testing::Return(true));
    
    // Execute
    bool result = service->removeEquipment("TEST-001");
    
    // Verify
    EXPECT_TRUE(result);
    
    // Verify equipment was removed from internal map
    auto retrieved = service->getEquipment("TEST-001");
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentFailsForNonexistentId) {
    // Try to remove equipment that doesn't exist
    EXPECT_CALL(service->getMockDataStorage(), deleteEquipment(::testing::_))
        .Times(0);
    
    // Execute
    bool result = service->removeEquipment("NONEXISTENT-ID");
    
    // Verify
    EXPECT_FALSE(result);
}

TEST_F(EquipmentTrackerServiceTest, GetEquipmentReturnsCorrectEquipment) {
    // Add test equipment
    Equipment test_equipment = createTestEquipment("TEST-001");
    
    EXPECT_CALL(service->getMockDataStorage(), saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    service->addEquipment(test_equipment);
    
    // Execute
    auto result = service->getEquipment("TEST-001");
    
    // Verify
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->getId(), "TEST-001");
    EXPECT_EQ(result->getName(), "Test Equipment TEST-001");
    EXPECT_EQ(result->getType(), EquipmentType::Forklift);
}

TEST_F(EquipmentTrackerServiceTest, GetEquipmentReturnsNulloptForNonexistentId) {
    // Execute
    auto result = service->getEquipment("NONEXISTENT-ID");
    
    // Verify
    EXPECT_FALSE(result.has_value());
}

TEST_F(EquipmentTrackerServiceTest, GetAllEquipmentReturnsAllEquipment) {
    // Add multiple equipment
    Equipment equipment1 = createTestEquipment("TEST-001", EquipmentType::Forklift);
    Equipment equipment2 = createTestEquipment("TEST-002", EquipmentType::Crane);
    Equipment equipment3 = createTestEquipment("TEST-003", EquipmentType::Bulldozer);
    
    EXPECT_CALL(service->getMockDataStorage(), saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    service->addEquipment(equipment3);
    
    // Execute
    auto result = service->getAllEquipment();
    
    // Verify
    EXPECT_EQ(result.size(), 3);
    
    // Check if all equipment are in the result
    std::vector<std::string> ids;
    for (const auto& equipment : result) {
        ids.push_back(equipment.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("TEST-001", "TEST-002", "TEST-003"));
}

TEST_F(EquipmentTrackerServiceTest, FindEquipmentByStatusReturnsCorrectEquipment) {
    // Add equipment with different statuses
    Equipment equipment1 = createTestEquipment("TEST-001");
    equipment1.setStatus(EquipmentStatus::Active);
    
    Equipment equipment2 = createTestEquipment("TEST-002");
    equipment2.setStatus(EquipmentStatus::Inactive);
    
    Equipment equipment3 = createTestEquipment("TEST-003");
    equipment3.setStatus(EquipmentStatus::Active);
    
    EXPECT_CALL(service->getMockDataStorage(), saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    service->addEquipment(equipment3);
    
    // Execute
    auto result = service->findEquipmentByStatus(EquipmentStatus::Active);
    
    // Verify
    EXPECT_EQ(result.size(), 2);
    
    // Check if correct equipment are in the result
    std::vector<std::string> ids;
    for (const auto& equipment : result) {
        ids.push_back(equipment.getId());
        EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("TEST-001", "TEST-003"));
}

TEST_F(EquipmentTrackerServiceTest, FindActiveEquipmentReturnsActiveEquipment) {
    // Add equipment with different statuses
    Equipment equipment1 = createTestEquipment("TEST-001");
    equipment1.setStatus(EquipmentStatus::Active);
    
    Equipment equipment2 = createTestEquipment("TEST-002");
    equipment2.setStatus(EquipmentStatus::Inactive);
    
    EXPECT_CALL(service->getMockDataStorage(), saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    
    // Execute
    auto result = service->findActiveEquipment();
    
    // Verify
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].getId(), "TEST-001");
    EXPECT_EQ(result[0].getStatus(), EquipmentStatus::Active);
}

TEST_F(EquipmentTrackerServiceTest, FindEquipmentInAreaReturnsEquipmentInArea) {
    // Create equipment with positions
    Equipment equipment1 = createTestEquipment("TEST-001");
    Position pos1(37.7749, -122.4194); // San Francisco
    equipment1.setLastPosition(pos1);
    
    Equipment equipment2 = createTestEquipment("TEST-002");
    Position pos2(34.0522, -118.2437); // Los Angeles
    equipment2.setLastPosition(pos2);
    
    Equipment equipment3 = createTestEquipment("TEST-003");
    Position pos3(40.7128, -74.0060); // New York
    equipment3.setLastPosition(pos3);
    
    Equipment equipment4 = createTestEquipment("TEST-004");
    // No position set for equipment4
    
    EXPECT_CALL(service->getMockDataStorage(), saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    service->addEquipment(equipment3);
    service->addEquipment(equipment4);
    
    // Execute - search for equipment in California
    auto result = service->findEquipmentInArea(33.0, -125.0, 42.0, -115.0);
    
    // Verify
    EXPECT_EQ(result.size(), 2);
    
    // Check if correct equipment are in the result
    std::vector<std::string> ids;
    for (const auto& equipment : result) {
        ids.push_back(equipment.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("TEST-001", "TEST-002"));
}

TEST_F(EquipmentTrackerServiceTest, SetGeofenceReturnsTrue) {
    // Execute
    bool result = service->setGeofence("TEST-001", 37.7, -122.5, 37.8, -122.4);
    
    // Verify - this is just a placeholder in the implementation
    EXPECT_TRUE(result);
}

TEST_F(EquipmentTrackerServiceTest, HandlePositionUpdateUpdatesEquipmentPosition) {
    // Add test equipment
    Equipment test_equipment = createTestEquipment("FORKLIFT-001");
    
    EXPECT_CALL(service->getMockDataStorage(), saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    service->addEquipment(test_equipment);
    
    // Setup expectations
    EXPECT_CALL(service->getMockDataStorage(), savePosition(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(service->getMockDataStorage(), updateEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(service->getMockNetworkManager(), sendPositionUpdate(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(true));
    
    // Execute
    Timestamp now = getCurrentTimestamp();
    service->testHandlePositionUpdate(37.7749, -122.4194, 10.0, now);
    
    // Verify equipment position was updated
    auto equipment = service->getEquipment("FORKLIFT-001");
    ASSERT_TRUE(equipment.has_value());
    ASSERT_TRUE(equipment->getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(equipment->getLastPosition()->getLatitude(), 37.7749);
    EXPECT_DOUBLE_EQ(equipment->getLastPosition()->getLongitude(), -122.4194);
    EXPECT_DOUBLE_EQ(equipment->getLastPosition()->getAltitude(), 10.0);
    EXPECT_EQ(equipment->getStatus(), EquipmentStatus::Active);
}

TEST_F(EquipmentTrackerServiceTest, HandleRemoteCommandProcessesStatusRequest) {
    // Add some equipment
    Equipment equipment1 = createTestEquipment("TEST-001");
    Equipment equipment2 = createTestEquipment("TEST-002");
    
    EXPECT_CALL(service->getMockDataStorage(), saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    
    // Execute
    service->testHandleRemoteCommand("STATUS_REQUEST");
    
    // No specific verification needed as the method just logs the command
    // This is primarily testing that the method doesn't crash
}

TEST_F(EquipmentTrackerServiceTest, DetermineEquipmentIdReturnsFirstEquipmentOrDefault) {
    // Test with empty equipment map
    auto result1 = service->testDetermineEquipmentId();
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(*result1, "FORKLIFT-001"); // Default value
    
    // Add equipment and test again
    Equipment test_equipment = createTestEquipment("TEST-001");
    
    EXPECT_CALL(service->getMockDataStorage(), saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    service->addEquipment(test_equipment);
    
    auto result2 = service->testDetermineEquipmentId();
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(*result2, "TEST-001"); // First equipment in map
}

} // namespace equipment_tracker
// </test_code>