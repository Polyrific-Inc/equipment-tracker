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
    
    // Access to mocks
    MockGPSTracker& getMockGPSTracker() { 
        return *static_cast<MockGPSTracker*>(gps_tracker_.get()); 
    }
    
    MockDataStorage& getMockDataStorage() { 
        return *static_cast<MockDataStorage*>(data_storage_.get()); 
    }
    
    MockNetworkManager& getMockNetworkManager() { 
        return *static_cast<MockNetworkManager*>(network_manager_.get()); 
    }

private:
    // We need to redefine these to make them accessible
    using EquipmentTrackerService::gps_tracker_;
    using EquipmentTrackerService::data_storage_;
    using EquipmentTrackerService::network_manager_;
};

class EquipmentTrackerServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_gps_tracker = std::make_unique<MockGPSTracker>();
        mock_data_storage = std::make_unique<MockDataStorage>();
        mock_network_manager = std::make_unique<MockNetworkManager>();
        
        // Keep raw pointers to the mocks for expectations
        gps_tracker_ptr = mock_gps_tracker.get();
        data_storage_ptr = mock_data_storage.get();
        network_manager_ptr = mock_network_manager.get();
        
        // Set up common expectations for constructor
        EXPECT_CALL(*gps_tracker_ptr, registerPositionCallback(::testing::_)).Times(1);
        EXPECT_CALL(*network_manager_ptr, registerCommandHandler(::testing::_)).Times(1);
        
        service = std::make_unique<TestableEquipmentTrackerService>(
            std::move(mock_gps_tracker),
            std::move(mock_data_storage),
            std::move(mock_network_manager)
        );
    }

    // Sample equipment for testing
    equipment_tracker::Equipment createSampleEquipment(const std::string& id = "TEST-001") {
        return equipment_tracker::Equipment(
            id, 
            equipment_tracker::EquipmentType::Forklift, 
            "Test Forklift"
        );
    }

    std::unique_ptr<TestableEquipmentTrackerService> service;
    std::unique_ptr<MockGPSTracker> mock_gps_tracker;
    std::unique_ptr<MockDataStorage> mock_data_storage;
    std::unique_ptr<MockNetworkManager> mock_network_manager;
    
    // Raw pointers for setting expectations
    MockGPSTracker* gps_tracker_ptr;
    MockDataStorage* data_storage_ptr;
    MockNetworkManager* network_manager_ptr;
};

// Test start method
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

    // Verify service is running
    EXPECT_TRUE(service->isRunning());
}

// Test start method with initialization failure
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

    // Verify service is not running
    EXPECT_FALSE(service->isRunning());
}

// Test stop method
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

    // Set up expectations for stop
    EXPECT_CALL(*gps_tracker_ptr, stop())
        .Times(1);
    EXPECT_CALL(*network_manager_ptr, disconnect())
        .Times(1);

    // Call the method under test
    service->stop();

    // Verify service is not running
    EXPECT_FALSE(service->isRunning());
}

// Test addEquipment method
TEST_F(EquipmentTrackerServiceTest, AddEquipmentSucceedsForNewEquipment) {
    auto equipment = createSampleEquipment();
    
    // Set up expectations
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));

    // Call the method under test
    bool result = service->addEquipment(equipment);

    // Verify result
    EXPECT_TRUE(result);
    
    // Verify equipment was added by checking if we can retrieve it
    auto retrieved = service->getEquipment(equipment.getId());
    EXPECT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->getId(), equipment.getId());
}

// Test addEquipment method with duplicate ID
TEST_F(EquipmentTrackerServiceTest, AddEquipmentFailsForDuplicateId) {
    auto equipment = createSampleEquipment();
    
    // Add equipment first time
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    bool first_result = service->addEquipment(equipment);
    EXPECT_TRUE(first_result);
    
    // Try to add again with same ID
    bool second_result = service->addEquipment(equipment);
    
    // Verify second attempt fails
    EXPECT_FALSE(second_result);
}

// Test removeEquipment method
TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentSucceedsForExistingEquipment) {
    auto equipment = createSampleEquipment();
    
    // Add equipment first
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    service->addEquipment(equipment);
    
    // Set up expectations for remove
    EXPECT_CALL(*data_storage_ptr, deleteEquipment(equipment.getId()))
        .WillOnce(::testing::Return(true));

    // Call the method under test
    bool result = service->removeEquipment(equipment.getId());

    // Verify result
    EXPECT_TRUE(result);
    
    // Verify equipment was removed
    auto retrieved = service->getEquipment(equipment.getId());
    EXPECT_FALSE(retrieved.has_value());
}

// Test removeEquipment method with non-existent ID
TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentFailsForNonExistentId) {
    // Call the method under test with a non-existent ID
    bool result = service->removeEquipment("NONEXISTENT-ID");

    // Verify result
    EXPECT_FALSE(result);
}

// Test getEquipment method
TEST_F(EquipmentTrackerServiceTest, GetEquipmentReturnsCorrectEquipment) {
    auto equipment1 = createSampleEquipment("TEST-001");
    auto equipment2 = createSampleEquipment("TEST-002");
    
    // Add equipment
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    
    // Call the method under test
    auto result1 = service->getEquipment("TEST-001");
    auto result2 = service->getEquipment("TEST-002");
    auto result3 = service->getEquipment("NONEXISTENT-ID");

    // Verify results
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(result1->getId(), "TEST-001");
    
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(result2->getId(), "TEST-002");
    
    EXPECT_FALSE(result3.has_value());
}

// Test getAllEquipment method
TEST_F(EquipmentTrackerServiceTest, GetAllEquipmentReturnsAllEquipment) {
    auto equipment1 = createSampleEquipment("TEST-001");
    auto equipment2 = createSampleEquipment("TEST-002");
    
    // Add equipment
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    
    // Call the method under test
    auto result = service->getAllEquipment();

    // Verify results
    EXPECT_EQ(result.size(), 2);
    
    // Check if both equipment items are in the result
    bool found1 = false, found2 = false;
    for (const auto& equip : result) {
        if (equip.getId() == "TEST-001") found1 = true;
        if (equip.getId() == "TEST-002") found2 = true;
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

// Test findEquipmentByStatus method
TEST_F(EquipmentTrackerServiceTest, FindEquipmentByStatusReturnsCorrectEquipment) {
    auto equipment1 = createSampleEquipment("TEST-001");
    auto equipment2 = createSampleEquipment("TEST-002");
    
    // Set different statuses
    equipment1.setStatus(equipment_tracker::EquipmentStatus::Active);
    equipment2.setStatus(equipment_tracker::EquipmentStatus::Maintenance);
    
    // Add equipment
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    
    // Call the method under test
    auto active_result = service->findEquipmentByStatus(equipment_tracker::EquipmentStatus::Active);
    auto maintenance_result = service->findEquipmentByStatus(equipment_tracker::EquipmentStatus::Maintenance);
    auto inactive_result = service->findEquipmentByStatus(equipment_tracker::EquipmentStatus::Inactive);

    // Verify results
    EXPECT_EQ(active_result.size(), 1);
    EXPECT_EQ(active_result[0].getId(), "TEST-001");
    
    EXPECT_EQ(maintenance_result.size(), 1);
    EXPECT_EQ(maintenance_result[0].getId(), "TEST-002");
    
    EXPECT_EQ(inactive_result.size(), 0);
}

// Test findActiveEquipment method
TEST_F(EquipmentTrackerServiceTest, FindActiveEquipmentReturnsOnlyActiveEquipment) {
    auto equipment1 = createSampleEquipment("TEST-001");
    auto equipment2 = createSampleEquipment("TEST-002");
    auto equipment3 = createSampleEquipment("TEST-003");
    
    // Set different statuses
    equipment1.setStatus(equipment_tracker::EquipmentStatus::Active);
    equipment2.setStatus(equipment_tracker::EquipmentStatus::Maintenance);
    equipment3.setStatus(equipment_tracker::EquipmentStatus::Active);
    
    // Add equipment
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    service->addEquipment(equipment3);
    
    // Call the method under test
    auto result = service->findActiveEquipment();

    // Verify results
    EXPECT_EQ(result.size(), 2);
    
    // Check if both active equipment items are in the result
    bool found1 = false, found3 = false;
    for (const auto& equip : result) {
        if (equip.getId() == "TEST-001") found1 = true;
        if (equip.getId() == "TEST-003") found3 = true;
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found3);
}

// Test findEquipmentInArea method
TEST_F(EquipmentTrackerServiceTest, FindEquipmentInAreaReturnsEquipmentInBounds) {
    auto equipment1 = createSampleEquipment("TEST-001");
    auto equipment2 = createSampleEquipment("TEST-002");
    auto equipment3 = createSampleEquipment("TEST-003");
    
    // Set positions
    equipment_tracker::Position pos1(37.7749, -122.4194); // San Francisco
    equipment_tracker::Position pos2(34.0522, -118.2437); // Los Angeles
    equipment_tracker::Position pos3(40.7128, -74.0060);  // New York
    
    equipment1.setLastPosition(pos1);
    equipment2.setLastPosition(pos2);
    equipment3.setLastPosition(pos3);
    
    // Add equipment
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    service->addEquipment(equipment3);
    
    // Call the method under test - West Coast area (includes SF and LA)
    auto west_coast = service->findEquipmentInArea(33.0, -125.0, 38.0, -118.0);
    
    // Call the method under test - East Coast area (includes NY)
    auto east_coast = service->findEquipmentInArea(39.0, -75.0, 41.0, -73.0);
    
    // Call the method under test - Central US (includes nothing)
    auto central = service->findEquipmentInArea(35.0, -100.0, 40.0, -90.0);

    // Verify results
    EXPECT_EQ(west_coast.size(), 2);
    EXPECT_EQ(east_coast.size(), 1);
    EXPECT_EQ(central.size(), 0);
    
    // Check specific equipment in west coast
    bool found_sf = false, found_la = false;
    for (const auto& equip : west_coast) {
        if (equip.getId() == "TEST-001") found_sf = true;
        if (equip.getId() == "TEST-002") found_la = true;
    }
    EXPECT_TRUE(found_sf);
    EXPECT_TRUE(found_la);
    
    // Check NY in east coast
    EXPECT_EQ(east_coast[0].getId(), "TEST-003");
}

// Test setGeofence method
TEST_F(EquipmentTrackerServiceTest, SetGeofenceReturnsTrue) {
    // Call the method under test
    bool result = service->setGeofence("TEST-001", 37.7, -122.5, 37.8, -122.4);

    // Verify result (currently always returns true as it's a placeholder)
    EXPECT_TRUE(result);
}

// Test handlePositionUpdate method
TEST_F(EquipmentTrackerServiceTest, HandlePositionUpdateUpdatesEquipmentPosition) {
    auto equipment = createSampleEquipment("FORKLIFT-001");
    
    // Add equipment
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
    service->handlePositionUpdate(37.7749, -122.4194, 10.0, equipment_tracker::getCurrentTimestamp());

    // Verify equipment position was updated
    auto updated = service->getEquipment("FORKLIFT-001");
    EXPECT_TRUE(updated.has_value());
    EXPECT_TRUE(updated->getLastPosition().has_value());
    EXPECT_NEAR(updated->getLastPosition()->getLatitude(), 37.7749, 0.0001);
    EXPECT_NEAR(updated->getLastPosition()->getLongitude(), -122.4194, 0.0001);
    EXPECT_EQ(updated->getStatus(), equipment_tracker::EquipmentStatus::Active);
}

// Test handleRemoteCommand method
TEST_F(EquipmentTrackerServiceTest, HandleRemoteCommandProcessesStatusRequest) {
    // Add some equipment
    auto equipment1 = createSampleEquipment("TEST-001");
    auto equipment2 = createSampleEquipment("TEST-002");
    
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);

    // Call the method under test
    service->handleRemoteCommand("STATUS_REQUEST");
    
    // No specific verification since this just logs to console in the current implementation
}

// Test determineEquipmentId method
TEST_F(EquipmentTrackerServiceTest, DetermineEquipmentIdReturnsFirstEquipmentOrDefault) {
    // Test with empty equipment map
    auto result1 = service->determineEquipmentId();
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(*result1, "FORKLIFT-001");  // Default value when map is empty
    
    // Add equipment
    auto equipment = createSampleEquipment("TEST-001");
    EXPECT_CALL(*data_storage_ptr, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    service->addEquipment(equipment);
    
    // Test with non-empty equipment map
    auto result2 = service->determineEquipmentId();
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(*result2, "TEST-001");  // Should return first equipment ID
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>