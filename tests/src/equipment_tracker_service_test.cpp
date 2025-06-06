// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/equipment_tracker_service.h"

// Mock classes for dependencies
class MockGPSTracker : public equipment_tracker::GPSTracker {
public:
    MOCK_METHOD(void, start, (), (override));
    MOCK_METHOD(void, stop, (), (override));
    MOCK_METHOD(bool, isRunning, (), (const, override));
    MOCK_METHOD(void, registerPositionCallback, (equipment_tracker::PositionCallback), (override));
    MOCK_METHOD(void, simulatePosition, (double, double, double), (override));
};

class MockDataStorage : public equipment_tracker::DataStorage {
public:
    MOCK_METHOD(bool, initialize, (), (override));
    MOCK_METHOD(bool, saveEquipment, (const equipment_tracker::Equipment&), (override));
    MOCK_METHOD(std::optional<equipment_tracker::Equipment>, loadEquipment, (const equipment_tracker::EquipmentId&), (override));
    MOCK_METHOD(bool, updateEquipment, (const equipment_tracker::Equipment&), (override));
    MOCK_METHOD(bool, deleteEquipment, (const equipment_tracker::EquipmentId&), (override));
    MOCK_METHOD(bool, savePosition, (const equipment_tracker::EquipmentId&, const equipment_tracker::Position&), (override));
    MOCK_METHOD(std::vector<equipment_tracker::Equipment>, getAllEquipment, (), (override));
    MOCK_METHOD(std::vector<equipment_tracker::Equipment>, findEquipmentByStatus, (equipment_tracker::EquipmentStatus), (override));
    MOCK_METHOD(std::vector<equipment_tracker::Equipment>, findEquipmentInArea, (double, double, double, double), (override));
};

class MockNetworkManager : public equipment_tracker::NetworkManager {
public:
    MOCK_METHOD(bool, connect, (), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(bool, isConnected, (), (const, override));
    MOCK_METHOD(bool, sendPositionUpdate, (const equipment_tracker::EquipmentId&, const equipment_tracker::Position&), (override));
    MOCK_METHOD(void, registerCommandHandler, (std::function<void(const std::string&)>), (override));
};

// Custom EquipmentTrackerService for testing that uses our mock objects
class TestableEquipmentTrackerService : public equipment_tracker::EquipmentTrackerService {
public:
    TestableEquipmentTrackerService(
        std::unique_ptr<MockGPSTracker> gps_tracker,
        std::unique_ptr<MockDataStorage> data_storage,
        std::unique_ptr<MockNetworkManager> network_manager
    ) : EquipmentTrackerService() {
        // Replace the real components with our mocks
        gps_tracker_ = std::move(gps_tracker);
        data_storage_ = std::move(data_storage);
        network_manager_ = std::move(network_manager);
    }

    // Expose protected methods for testing
    using EquipmentTrackerService::handlePositionUpdate;
    using EquipmentTrackerService::handleRemoteCommand;
    using EquipmentTrackerService::determineEquipmentId;
    
    // Access to mocks for verification
    MockGPSTracker& getMockGPSTracker() {
        return *static_cast<MockGPSTracker*>(gps_tracker_.get());
    }
    
    MockDataStorage& getMockDataStorage() {
        return *static_cast<MockDataStorage*>(data_storage_.get());
    }
    
    MockNetworkManager& getMockNetworkManager() {
        return *static_cast<MockNetworkManager*>(network_manager_.get());
    }
};

class EquipmentTrackerServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_gps_tracker = std::make_unique<MockGPSTracker>();
        mock_data_storage = std::make_unique<MockDataStorage>();
        mock_network_manager = std::make_unique<MockNetworkManager>();
        
        // Keep raw pointers to the mocks for use in test expectations
        gps_tracker_ptr = mock_gps_tracker.get();
        data_storage_ptr = mock_data_storage.get();
        network_manager_ptr = mock_network_manager.get();
        
        // Set up common expectations for the constructor
        EXPECT_CALL(*gps_tracker_ptr, registerPositionCallback(::testing::_)).Times(1);
        EXPECT_CALL(*network_manager_ptr, registerCommandHandler(::testing::_)).Times(1);
        
        // Create the service with our mocks
        service = std::make_unique<TestableEquipmentTrackerService>(
            std::move(mock_gps_tracker),
            std::move(mock_data_storage),
            std::move(mock_network_manager)
        );
    }

    std::unique_ptr<MockGPSTracker> mock_gps_tracker;
    std::unique_ptr<MockDataStorage> mock_data_storage;
    std::unique_ptr<MockNetworkManager> mock_network_manager;
    
    // Raw pointers for setting expectations
    MockGPSTracker* gps_tracker_ptr;
    MockDataStorage* data_storage_ptr;
    MockNetworkManager* network_manager_ptr;
    
    std::unique_ptr<TestableEquipmentTrackerService> service;
    
    // Helper to create test equipment
    equipment_tracker::Equipment createTestEquipment(const std::string& id = "TEST-001") {
        return equipment_tracker::Equipment(
            id, 
            equipment_tracker::EquipmentType::Forklift, 
            "Test Forklift"
        );
    }
};

TEST_F(EquipmentTrackerServiceTest, StartInitializesComponentsCorrectly) {
    // Set up expectations
    EXPECT_CALL(*data_storage_ptr, initialize())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*data_storage_ptr, getAllEquipment())
        .WillOnce(::testing::Return(std::vector<equipment_tracker::Equipment>{}));
    EXPECT_CALL(*network_manager_ptr, connect())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*gps_tracker_ptr, start())
        .Times(1);
    
    // Call the method under test
    service->start();
    
    // Verify the service is running
    EXPECT_TRUE(service->isRunning());
}

TEST_F(EquipmentTrackerServiceTest, StartFailsWhenDataStorageInitializationFails) {
    // Set up expectations
    EXPECT_CALL(*data_storage_ptr, initialize())
        .WillOnce(::testing::Return(false));
    
    // These should not be called if initialization fails
    EXPECT_CALL(*data_storage_ptr, getAllEquipment()).Times(0);
    EXPECT_CALL(*network_manager_ptr, connect()).Times(0);
    EXPECT_CALL(*gps_tracker_ptr, start()).Times(0);
    
    // Call the method under test
    service->start();
    
    // Verify the service is not running
    EXPECT_FALSE(service->isRunning());
}

TEST_F(EquipmentTrackerServiceTest, StopShutdownsComponentsCorrectly) {
    // First start the service
    EXPECT_CALL(*data_storage_ptr, initialize())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*data_storage_ptr, getAllEquipment())
        .WillOnce(::testing::Return(std::vector<equipment_tracker::Equipment>{}));
    EXPECT_CALL(*network_manager_ptr, connect())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*gps_tracker_ptr, start())
        .Times(1);
    
    service->start();
    EXPECT_TRUE(service->isRunning());
    
    // Now test stopping
    EXPECT_CALL(*gps_tracker_ptr, stop()).Times(1);
    EXPECT_CALL(*network_manager_ptr, disconnect()).Times(1);
    
    service->stop();
    
    // Verify the service is not running
    EXPECT_FALSE(service->isRunning());
}

TEST_F(EquipmentTrackerServiceTest, StopDoesNothingWhenNotRunning) {
    // Service starts not running
    EXPECT_FALSE(service->isRunning());
    
    // These should not be called if the service is not running
    EXPECT_CALL(*gps_tracker_ptr, stop()).Times(0);
    EXPECT_CALL(*network_manager_ptr, disconnect()).Times(0);
    
    service->stop();
    
    // Still not running
    EXPECT_FALSE(service->isRunning());
}

TEST_F(EquipmentTrackerServiceTest, AddEquipmentSucceeds) {
    auto equipment = createTestEquipment();
    
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    
    bool result = service->addEquipment(equipment);
    
    EXPECT_TRUE(result);
    
    // Verify we can retrieve the added equipment
    auto retrieved = service->getEquipment(equipment.getId());
    EXPECT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->getId(), equipment.getId());
}

TEST_F(EquipmentTrackerServiceTest, AddEquipmentFailsForDuplicateId) {
    auto equipment = createTestEquipment();
    
    // First add succeeds
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    
    bool result1 = service->addEquipment(equipment);
    EXPECT_TRUE(result1);
    
    // Second add with same ID fails, storage not called
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .Times(0);
    
    bool result2 = service->addEquipment(equipment);
    EXPECT_FALSE(result2);
}

TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentSucceeds) {
    auto equipment = createTestEquipment();
    
    // First add the equipment
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    
    service->addEquipment(equipment);
    
    // Now test removal
    EXPECT_CALL(*data_storage_ptr, deleteEquipment(equipment.getId()))
        .WillOnce(::testing::Return(true));
    
    bool result = service->removeEquipment(equipment.getId());
    
    EXPECT_TRUE(result);
    
    // Verify it's gone
    auto retrieved = service->getEquipment(equipment.getId());
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentFailsForNonexistentId) {
    // Try to remove equipment that doesn't exist
    EXPECT_CALL(*data_storage_ptr, deleteEquipment(::testing::_))
        .Times(0);
    
    bool result = service->removeEquipment("NONEXISTENT-ID");
    
    EXPECT_FALSE(result);
}

TEST_F(EquipmentTrackerServiceTest, GetAllEquipmentReturnsCorrectList) {
    // Add multiple equipment
    auto equipment1 = createTestEquipment("TEST-001");
    auto equipment2 = createTestEquipment("TEST-002");
    auto equipment3 = createTestEquipment("TEST-003");
    
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    service->addEquipment(equipment3);
    
    // Get all equipment
    auto all_equipment = service->getAllEquipment();
    
    // Verify the list
    EXPECT_EQ(all_equipment.size(), 3);
    
    // Check that all IDs are present
    std::vector<std::string> ids;
    for (const auto& eq : all_equipment) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("TEST-001", "TEST-002", "TEST-003"));
}

TEST_F(EquipmentTrackerServiceTest, FindEquipmentByStatusReturnsCorrectList) {
    // Add equipment with different statuses
    auto equipment1 = createTestEquipment("TEST-001");
    auto equipment2 = createTestEquipment("TEST-002");
    auto equipment3 = createTestEquipment("TEST-003");
    
    equipment1.setStatus(equipment_tracker::EquipmentStatus::Active);
    equipment2.setStatus(equipment_tracker::EquipmentStatus::Maintenance);
    equipment3.setStatus(equipment_tracker::EquipmentStatus::Active);
    
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    service->addEquipment(equipment3);
    
    // Find active equipment
    auto active_equipment = service->findEquipmentByStatus(equipment_tracker::EquipmentStatus::Active);
    
    // Verify the list
    EXPECT_EQ(active_equipment.size(), 2);
    
    // Check that the correct IDs are present
    std::vector<std::string> ids;
    for (const auto& eq : active_equipment) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("TEST-001", "TEST-003"));
}

TEST_F(EquipmentTrackerServiceTest, FindActiveEquipmentReturnsCorrectList) {
    // Add equipment with different statuses
    auto equipment1 = createTestEquipment("TEST-001");
    auto equipment2 = createTestEquipment("TEST-002");
    
    equipment1.setStatus(equipment_tracker::EquipmentStatus::Active);
    equipment2.setStatus(equipment_tracker::EquipmentStatus::Inactive);
    
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    
    // Find active equipment
    auto active_equipment = service->findActiveEquipment();
    
    // Verify the list
    EXPECT_EQ(active_equipment.size(), 1);
    EXPECT_EQ(active_equipment[0].getId(), "TEST-001");
}

TEST_F(EquipmentTrackerServiceTest, FindEquipmentInAreaReturnsCorrectList) {
    // Create equipment with positions
    auto equipment1 = createTestEquipment("TEST-001");
    auto equipment2 = createTestEquipment("TEST-002");
    auto equipment3 = createTestEquipment("TEST-003");
    
    // Set positions
    equipment1.setLastPosition(equipment_tracker::Position(10.0, 20.0));
    equipment2.setLastPosition(equipment_tracker::Position(30.0, 40.0));
    equipment3.setLastPosition(equipment_tracker::Position(15.0, 25.0));
    
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    service->addEquipment(equipment3);
    
    // Find equipment in area (should include equipment1 and equipment3)
    auto area_equipment = service->findEquipmentInArea(5.0, 15.0, 20.0, 30.0);
    
    // Verify the list
    EXPECT_EQ(area_equipment.size(), 2);
    
    // Check that the correct IDs are present
    std::vector<std::string> ids;
    for (const auto& eq : area_equipment) {
        ids.push_back(eq.getId());
    }
    
    EXPECT_THAT(ids, ::testing::UnorderedElementsAre("TEST-001", "TEST-003"));
}

TEST_F(EquipmentTrackerServiceTest, SetGeofenceReturnsTrue) {
    // This is just testing the placeholder implementation
    bool result = service->setGeofence("TEST-001", 10.0, 20.0, 30.0, 40.0);
    EXPECT_TRUE(result);
}

TEST_F(EquipmentTrackerServiceTest, HandlePositionUpdateUpdatesEquipment) {
    // Add test equipment
    auto equipment = createTestEquipment("FORKLIFT-001");
    
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    
    service->addEquipment(equipment);
    
    // Set up expectations for position update
    EXPECT_CALL(*data_storage_ptr, savePosition(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*data_storage_ptr, updateEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*network_manager_ptr, sendPositionUpdate(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(true));
    
    // Call the method under test
    service->handlePositionUpdate(10.0, 20.0, 30.0, equipment_tracker::getCurrentTimestamp());
    
    // Verify the equipment was updated
    auto updated = service->getEquipment("FORKLIFT-001");
    EXPECT_TRUE(updated.has_value());
    EXPECT_EQ(updated->getStatus(), equipment_tracker::EquipmentStatus::Active);
    
    auto position = updated->getLastPosition();
    EXPECT_TRUE(position.has_value());
    EXPECT_NEAR(position->getLatitude(), 10.0, 0.001);
    EXPECT_NEAR(position->getLongitude(), 20.0, 0.001);
    EXPECT_NEAR(position->getAltitude(), 30.0, 0.001);
}

TEST_F(EquipmentTrackerServiceTest, HandleRemoteCommandProcessesStatusRequest) {
    // Add some test equipment
    auto equipment1 = createTestEquipment("TEST-001");
    auto equipment2 = createTestEquipment("TEST-002");
    
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    
    // Call the method under test
    service->handleRemoteCommand("STATUS_REQUEST");
    
    // No specific expectations to verify as the method just logs
    // This test mainly ensures the method doesn't crash
}

TEST_F(EquipmentTrackerServiceTest, DetermineEquipmentIdReturnsFirstEquipmentWhenAvailable) {
    // Add test equipment
    auto equipment = createTestEquipment("TEST-001");
    
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    
    service->addEquipment(equipment);
    
    // Call the method under test
    auto id = service->determineEquipmentId();
    
    // Verify the result
    EXPECT_TRUE(id.has_value());
    EXPECT_EQ(*id, "TEST-001");
}

TEST_F(EquipmentTrackerServiceTest, DetermineEquipmentIdReturnsDefaultWhenNoEquipment) {
    // Call the method under test with no equipment added
    auto id = service->determineEquipmentId();
    
    // Verify the result
    EXPECT_TRUE(id.has_value());
    EXPECT_EQ(*id, "FORKLIFT-001");
}
// </test_code>