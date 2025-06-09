// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/equipment_tracker_service.h"

// Mock classes for dependencies
class MockGPSTracker : public equipment_tracker::GPSTracker {
public:
    MockGPSTracker() : equipment_tracker::GPSTracker() {}
    
    MOCK_METHOD(void, start, (), (override));
    MOCK_METHOD(void, stop, (), (override));
    MOCK_METHOD(void, registerPositionCallback, (equipment_tracker::PositionCallback), (override));
    MOCK_METHOD(bool, isRunning, (), (const, override));
};

class MockDataStorage : public equipment_tracker::DataStorage {
public:
    MockDataStorage() : equipment_tracker::DataStorage() {}
    
    MOCK_METHOD(bool, initialize, (), (override));
    MOCK_METHOD(bool, saveEquipment, (const equipment_tracker::Equipment&), (override));
    MOCK_METHOD(bool, deleteEquipment, (const equipment_tracker::EquipmentId&), (override));
    MOCK_METHOD(std::vector<equipment_tracker::Equipment>, getAllEquipment, (), (override));
    MOCK_METHOD(bool, updateEquipment, (const equipment_tracker::Equipment&), (override));
    MOCK_METHOD(bool, savePosition, (const equipment_tracker::EquipmentId&, const equipment_tracker::Position&), (override));
};

class MockNetworkManager : public equipment_tracker::NetworkManager {
public:
    MockNetworkManager() : equipment_tracker::NetworkManager() {}
    
    MOCK_METHOD(bool, connect, (), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(bool, isConnected, (), (const, override));
    MOCK_METHOD(void, registerCommandHandler, (std::function<void(const std::string&)>), (override));
    MOCK_METHOD(bool, sendPositionUpdate, (const equipment_tracker::EquipmentId&, const equipment_tracker::Position&), (override));
};

// Test fixture for EquipmentTrackerService
class EquipmentTrackerServiceTest : public ::testing::Test {
protected:
    std::unique_ptr<MockGPSTracker> mockGpsTracker;
    std::unique_ptr<MockDataStorage> mockDataStorage;
    std::unique_ptr<MockNetworkManager> mockNetworkManager;
    
    // Custom service class that allows injecting mocks
    class TestableEquipmentTrackerService : public equipment_tracker::EquipmentTrackerService {
    public:
        TestableEquipmentTrackerService(
            std::unique_ptr<equipment_tracker::GPSTracker> gpsTracker,
            std::unique_ptr<equipment_tracker::DataStorage> dataStorage,
            std::unique_ptr<equipment_tracker::NetworkManager> networkManager
        ) {
            gps_tracker_ = std::move(gpsTracker);
            data_storage_ = std::move(dataStorage);
            network_manager_ = std::move(networkManager);
        }
        
        // Expose private methods for testing
        void callHandlePositionUpdate(double lat, double lon, double alt, equipment_tracker::Timestamp timestamp) {
            handlePositionUpdate(lat, lon, alt, timestamp);
        }
        
        void callHandleRemoteCommand(const std::string& command) {
            handleRemoteCommand(command);
        }
        
        std::optional<equipment_tracker::EquipmentId> callDetermineEquipmentId() {
            return determineEquipmentId();
        }
    };
    
    std::unique_ptr<TestableEquipmentTrackerService> service;
    
    void SetUp() override {
        mockGpsTracker = std::make_unique<MockGPSTracker>();
        mockDataStorage = std::make_unique<MockDataStorage>();
        mockNetworkManager = std::make_unique<MockNetworkManager>();
        
        // Set up expectations for constructor
        EXPECT_CALL(*mockGpsTracker, registerPositionCallback(testing::_)).Times(1);
        EXPECT_CALL(*mockNetworkManager, registerCommandHandler(testing::_)).Times(1);
        
        service = std::make_unique<TestableEquipmentTrackerService>(
            std::move(mockGpsTracker),
            std::move(mockDataStorage),
            std::move(mockNetworkManager)
        );
        
        // Get the mocks back as raw pointers for setting expectations
        mockGpsTracker = dynamic_cast<MockGPSTracker*>(&service->getGPSTracker());
        mockDataStorage = dynamic_cast<MockDataStorage*>(&service->getDataStorage());
        mockNetworkManager = dynamic_cast<MockNetworkManager*>(&service->getNetworkManager());
    }
    
    void TearDown() override {
        service.reset();
    }
    
    // Helper to create a test equipment
    equipment_tracker::Equipment createTestEquipment(const std::string& id = "TEST-001") {
        return equipment_tracker::Equipment(id, equipment_tracker::EquipmentType::Forklift, "Test Forklift");
    }
};

// Test starting the service
TEST_F(EquipmentTrackerServiceTest, StartServiceInitializesComponentsCorrectly) {
    // Set up expectations
    EXPECT_CALL(*mockDataStorage, initialize())
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mockDataStorage, getAllEquipment())
        .WillOnce(testing::Return(std::vector<equipment_tracker::Equipment>()));
    EXPECT_CALL(*mockNetworkManager, connect())
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mockGpsTracker, start())
        .Times(1);
    
    // Call the method under test
    service->start();
    
    // Verify the service is running
    EXPECT_TRUE(service->isRunning());
}

// Test starting the service with data storage initialization failure
TEST_F(EquipmentTrackerServiceTest, StartServiceFailsWhenDataStorageInitializationFails) {
    // Set up expectations
    EXPECT_CALL(*mockDataStorage, initialize())
        .WillOnce(testing::Return(false));
    
    // These should not be called if initialization fails
    EXPECT_CALL(*mockDataStorage, getAllEquipment()).Times(0);
    EXPECT_CALL(*mockNetworkManager, connect()).Times(0);
    EXPECT_CALL(*mockGpsTracker, start()).Times(0);
    
    // Call the method under test
    service->start();
    
    // Verify the service is not running
    EXPECT_FALSE(service->isRunning());
}

// Test stopping the service
TEST_F(EquipmentTrackerServiceTest, StopServiceShutdownsComponentsCorrectly) {
    // First start the service
    EXPECT_CALL(*mockDataStorage, initialize())
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mockDataStorage, getAllEquipment())
        .WillOnce(testing::Return(std::vector<equipment_tracker::Equipment>()));
    EXPECT_CALL(*mockNetworkManager, connect())
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mockGpsTracker, start())
        .Times(1);
    
    service->start();
    EXPECT_TRUE(service->isRunning());
    
    // Now test stopping
    EXPECT_CALL(*mockGpsTracker, stop())
        .Times(1);
    EXPECT_CALL(*mockNetworkManager, disconnect())
        .Times(1);
    
    service->stop();
    
    // Verify the service is not running
    EXPECT_FALSE(service->isRunning());
}

// Test adding equipment
TEST_F(EquipmentTrackerServiceTest, AddEquipmentSucceedsWithValidEquipment) {
    auto equipment = createTestEquipment();
    
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillOnce(testing::Return(true));
    
    bool result = service->addEquipment(equipment);
    
    EXPECT_TRUE(result);
    
    // Verify we can retrieve the equipment
    auto retrieved = service->getEquipment(equipment.getId());
    EXPECT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->getId(), equipment.getId());
}

// Test adding duplicate equipment
TEST_F(EquipmentTrackerServiceTest, AddEquipmentFailsWithDuplicateId) {
    auto equipment = createTestEquipment();
    
    // First add succeeds
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillOnce(testing::Return(true));
    
    bool firstResult = service->addEquipment(equipment);
    EXPECT_TRUE(firstResult);
    
    // Second add with same ID fails
    bool secondResult = service->addEquipment(equipment);
    EXPECT_FALSE(secondResult);
}

// Test removing equipment
TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentSucceedsWithExistingEquipment) {
    auto equipment = createTestEquipment();
    
    // First add the equipment
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillOnce(testing::Return(true));
    
    service->addEquipment(equipment);
    
    // Now test removal
    EXPECT_CALL(*mockDataStorage, deleteEquipment(equipment.getId()))
        .WillOnce(testing::Return(true));
    
    bool result = service->removeEquipment(equipment.getId());
    
    EXPECT_TRUE(result);
    
    // Verify it's gone
    auto retrieved = service->getEquipment(equipment.getId());
    EXPECT_FALSE(retrieved.has_value());
}

// Test removing non-existent equipment
TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentFailsWithNonExistentEquipment) {
    bool result = service->removeEquipment("NONEXISTENT-001");
    EXPECT_FALSE(result);
}

// Test getting all equipment
TEST_F(EquipmentTrackerServiceTest, GetAllEquipmentReturnsAllAddedEquipment) {
    auto equipment1 = createTestEquipment("TEST-001");
    auto equipment2 = createTestEquipment("TEST-002");
    
    // Add equipment
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillRepeatedly(testing::Return(true));
    
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    
    // Get all equipment
    auto allEquipment = service->getAllEquipment();
    
    EXPECT_EQ(allEquipment.size(), 2);
    
    // Check if both equipment are in the result
    bool found1 = false, found2 = false;
    for (const auto& eq : allEquipment) {
        if (eq.getId() == "TEST-001") found1 = true;
        if (eq.getId() == "TEST-002") found2 = true;
    }
    
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

// Test finding equipment by status
TEST_F(EquipmentTrackerServiceTest, FindEquipmentByStatusReturnsCorrectEquipment) {
    auto equipment1 = createTestEquipment("TEST-001");
    auto equipment2 = createTestEquipment("TEST-002");
    
    // Set different statuses
    equipment1.setStatus(equipment_tracker::EquipmentStatus::Active);
    equipment2.setStatus(equipment_tracker::EquipmentStatus::Maintenance);
    
    // Add equipment
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillRepeatedly(testing::Return(true));
    
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    
    // Find active equipment
    auto activeEquipment = service->findEquipmentByStatus(equipment_tracker::EquipmentStatus::Active);
    EXPECT_EQ(activeEquipment.size(), 1);
    EXPECT_EQ(activeEquipment[0].getId(), "TEST-001");
    
    // Find maintenance equipment
    auto maintenanceEquipment = service->findEquipmentByStatus(equipment_tracker::EquipmentStatus::Maintenance);
    EXPECT_EQ(maintenanceEquipment.size(), 1);
    EXPECT_EQ(maintenanceEquipment[0].getId(), "TEST-002");
}

// Test finding active equipment
TEST_F(EquipmentTrackerServiceTest, FindActiveEquipmentReturnsOnlyActiveEquipment) {
    auto equipment1 = createTestEquipment("TEST-001");
    auto equipment2 = createTestEquipment("TEST-002");
    
    // Set different statuses
    equipment1.setStatus(equipment_tracker::EquipmentStatus::Active);
    equipment2.setStatus(equipment_tracker::EquipmentStatus::Inactive);
    
    // Add equipment
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillRepeatedly(testing::Return(true));
    
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    
    // Find active equipment
    auto activeEquipment = service->findActiveEquipment();
    EXPECT_EQ(activeEquipment.size(), 1);
    EXPECT_EQ(activeEquipment[0].getId(), "TEST-001");
}

// Test finding equipment in area
TEST_F(EquipmentTrackerServiceTest, FindEquipmentInAreaReturnsEquipmentInSpecifiedArea) {
    auto equipment1 = createTestEquipment("TEST-001");
    auto equipment2 = createTestEquipment("TEST-002");
    
    // Set positions
    equipment_tracker::Position pos1(37.7749, -122.4194); // San Francisco
    equipment_tracker::Position pos2(34.0522, -118.2437); // Los Angeles
    
    equipment1.setLastPosition(pos1);
    equipment2.setLastPosition(pos2);
    
    // Add equipment
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillRepeatedly(testing::Return(true));
    
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    
    // Find equipment in SF area
    auto sfAreaEquipment = service->findEquipmentInArea(37.7, -122.5, 37.8, -122.3);
    EXPECT_EQ(sfAreaEquipment.size(), 1);
    EXPECT_EQ(sfAreaEquipment[0].getId(), "TEST-001");
    
    // Find equipment in LA area
    auto laAreaEquipment = service->findEquipmentInArea(34.0, -118.3, 34.1, -118.2);
    EXPECT_EQ(laAreaEquipment.size(), 1);
    EXPECT_EQ(laAreaEquipment[0].getId(), "TEST-002");
    
    // Find equipment in wider area (should include both)
    auto wideAreaEquipment = service->findEquipmentInArea(34.0, -123.0, 38.0, -118.0);
    EXPECT_EQ(wideAreaEquipment.size(), 2);
}

// Test setting geofence
TEST_F(EquipmentTrackerServiceTest, SetGeofenceReturnsTrue) {
    auto equipment = createTestEquipment();
    
    // Add equipment
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillOnce(testing::Return(true));
    
    service->addEquipment(equipment);
    
    // Set geofence
    bool result = service->setGeofence(equipment.getId(), 37.7, -122.5, 37.8, -122.3);
    EXPECT_TRUE(result);
}

// Test handling position update
TEST_F(EquipmentTrackerServiceTest, HandlePositionUpdateUpdatesEquipmentPosition) {
    auto equipment = createTestEquipment("FORKLIFT-001");
    
    // Add equipment
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillOnce(testing::Return(true));
    
    service->addEquipment(equipment);
    
    // Set expectations for position update
    EXPECT_CALL(*mockDataStorage, updateEquipment(testing::_))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mockDataStorage, savePosition(testing::_, testing::_))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mockNetworkManager, sendPositionUpdate(testing::_, testing::_))
        .WillOnce(testing::Return(true));
    
    // Call the position update handler
    auto timestamp = equipment_tracker::getCurrentTimestamp();
    service->callHandlePositionUpdate(37.7749, -122.4194, 10.0, timestamp);
    
    // Verify equipment position was updated
    auto updatedEquipment = service->getEquipment("FORKLIFT-001");
    ASSERT_TRUE(updatedEquipment.has_value());
    ASSERT_TRUE(updatedEquipment->getLastPosition().has_value());
    EXPECT_DOUBLE_EQ(updatedEquipment->getLastPosition()->getLatitude(), 37.7749);
    EXPECT_DOUBLE_EQ(updatedEquipment->getLastPosition()->getLongitude(), -122.4194);
    EXPECT_DOUBLE_EQ(updatedEquipment->getLastPosition()->getAltitude(), 10.0);
    EXPECT_EQ(updatedEquipment->getStatus(), equipment_tracker::EquipmentStatus::Active);
}

// Test handling remote command
TEST_F(EquipmentTrackerServiceTest, HandleRemoteCommandProcessesStatusRequestCommand) {
    auto equipment = createTestEquipment();
    
    // Add equipment
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillOnce(testing::Return(true));
    
    service->addEquipment(equipment);
    
    // Call the remote command handler
    service->callHandleRemoteCommand("STATUS_REQUEST");
    
    // No specific expectations to verify, just ensure it doesn't crash
}

// Test determine equipment ID
TEST_F(EquipmentTrackerServiceTest, DetermineEquipmentIdReturnsFirstEquipmentIdWhenAvailable) {
    auto equipment = createTestEquipment("TEST-001");
    
    // Add equipment
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillOnce(testing::Return(true));
    
    service->addEquipment(equipment);
    
    // Call determine equipment ID
    auto equipmentId = service->callDetermineEquipmentId();
    
    ASSERT_TRUE(equipmentId.has_value());
    EXPECT_EQ(*equipmentId, "TEST-001");
}

// Test determine equipment ID with empty equipment map
TEST_F(EquipmentTrackerServiceTest, DetermineEquipmentIdReturnsDefaultIdWhenNoEquipmentAvailable) {
    // Call determine equipment ID with empty map
    auto equipmentId = service->callDetermineEquipmentId();
    
    ASSERT_TRUE(equipmentId.has_value());
    EXPECT_EQ(*equipmentId, "FORKLIFT-001");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>