// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/equipment_tracker_service.h"

// Mock classes for dependencies
class MockGPSTracker : public equipment_tracker::GPSTracker {
public:
    MockGPSTracker() : equipment_tracker::GPSTracker(1000) {}
    
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
};

class MockNetworkManager : public equipment_tracker::NetworkManager {
public:
    MOCK_METHOD(bool, connect, (), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(bool, isConnected, (), (const, override));
    MOCK_METHOD(bool, sendPositionUpdate, (const equipment_tracker::EquipmentId&, const equipment_tracker::Position&), (override));
    MOCK_METHOD(void, registerCommandHandler, (std::function<void(const std::string&)>), (override));
};

// Test fixture for EquipmentTrackerService
class EquipmentTrackerServiceTest : public ::testing::Test {
protected:
    std::unique_ptr<MockGPSTracker> mockGpsTracker;
    std::unique_ptr<MockDataStorage> mockDataStorage;
    std::unique_ptr<MockNetworkManager> mockNetworkManager;
    std::unique_ptr<equipment_tracker::EquipmentTrackerService> service;

    void SetUp() override {
        // Create mocks
        mockGpsTracker = std::make_unique<MockGPSTracker>();
        mockDataStorage = std::make_unique<MockDataStorage>();
        mockNetworkManager = std::make_unique<MockNetworkManager>();
        
        // Create service with mocks
        service = std::make_unique<equipment_tracker::EquipmentTrackerService>();
        
        // Replace service's dependencies with mocks
        service->getGPSTracker() = *mockGpsTracker;
        service->getDataStorage() = *mockDataStorage;
        service->getNetworkManager() = *mockNetworkManager;
    }
    
    // Helper method to create a test equipment
    equipment_tracker::Equipment createTestEquipment(const std::string& id = "TEST-001") {
        return equipment_tracker::Equipment(
            id, 
            equipment_tracker::EquipmentType::Forklift, 
            "Test Forklift"
        );
    }
};

// Test starting the service
TEST_F(EquipmentTrackerServiceTest, StartServiceSuccess) {
    // Setup expectations
    EXPECT_CALL(*mockDataStorage, initialize())
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mockDataStorage, getAllEquipment())
        .WillOnce(testing::Return(std::vector<equipment_tracker::Equipment>()));
    EXPECT_CALL(*mockNetworkManager, connect())
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mockGpsTracker, start());
    
    // Call the method under test
    service->start();
    
    // Verify the service is running
    EXPECT_TRUE(service->isRunning());
}

// Test starting the service with data storage initialization failure
TEST_F(EquipmentTrackerServiceTest, StartServiceFailsWhenDataStorageInitFails) {
    // Setup expectations
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
TEST_F(EquipmentTrackerServiceTest, StopService) {
    // First start the service
    EXPECT_CALL(*mockDataStorage, initialize())
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mockDataStorage, getAllEquipment())
        .WillOnce(testing::Return(std::vector<equipment_tracker::Equipment>()));
    EXPECT_CALL(*mockNetworkManager, connect());
    EXPECT_CALL(*mockGpsTracker, start());
    
    service->start();
    
    // Setup expectations for stop
    EXPECT_CALL(*mockGpsTracker, stop());
    EXPECT_CALL(*mockNetworkManager, disconnect());
    
    // Call the method under test
    service->stop();
    
    // Verify the service is not running
    EXPECT_FALSE(service->isRunning());
}

// Test adding equipment
TEST_F(EquipmentTrackerServiceTest, AddEquipmentSuccess) {
    auto equipment = createTestEquipment();
    
    // Setup expectations
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillOnce(testing::Return(true));
    
    // Call the method under test
    bool result = service->addEquipment(equipment);
    
    // Verify the result
    EXPECT_TRUE(result);
    
    // Verify the equipment was added
    auto retrievedEquipment = service->getEquipment(equipment.getId());
    EXPECT_TRUE(retrievedEquipment.has_value());
    EXPECT_EQ(retrievedEquipment->getId(), equipment.getId());
}

// Test adding duplicate equipment
TEST_F(EquipmentTrackerServiceTest, AddDuplicateEquipmentFails) {
    auto equipment = createTestEquipment();
    
    // Add the equipment first
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillOnce(testing::Return(true));
    service->addEquipment(equipment);
    
    // Try to add it again
    bool result = service->addEquipment(equipment);
    
    // Verify the result
    EXPECT_FALSE(result);
}

// Test removing equipment
TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentSuccess) {
    auto equipment = createTestEquipment();
    
    // Add the equipment first
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillOnce(testing::Return(true));
    service->addEquipment(equipment);
    
    // Setup expectations for remove
    EXPECT_CALL(*mockDataStorage, deleteEquipment(equipment.getId()))
        .WillOnce(testing::Return(true));
    
    // Call the method under test
    bool result = service->removeEquipment(equipment.getId());
    
    // Verify the result
    EXPECT_TRUE(result);
    
    // Verify the equipment was removed
    auto retrievedEquipment = service->getEquipment(equipment.getId());
    EXPECT_FALSE(retrievedEquipment.has_value());
}

// Test removing non-existent equipment
TEST_F(EquipmentTrackerServiceTest, RemoveNonExistentEquipmentFails) {
    // Call the method under test with a non-existent ID
    bool result = service->removeEquipment("NONEXISTENT-001");
    
    // Verify the result
    EXPECT_FALSE(result);
}

// Test getting all equipment
TEST_F(EquipmentTrackerServiceTest, GetAllEquipment) {
    // Add some equipment
    auto equipment1 = createTestEquipment("TEST-001");
    auto equipment2 = createTestEquipment("TEST-002");
    
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillRepeatedly(testing::Return(true));
    
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    
    // Call the method under test
    auto allEquipment = service->getAllEquipment();
    
    // Verify the result
    EXPECT_EQ(allEquipment.size(), 2);
    
    // Check if both equipment items are in the result
    bool found1 = false, found2 = false;
    for (const auto& equipment : allEquipment) {
        if (equipment.getId() == "TEST-001") found1 = true;
        if (equipment.getId() == "TEST-002") found2 = true;
    }
    
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

// Test finding equipment by status
TEST_F(EquipmentTrackerServiceTest, FindEquipmentByStatus) {
    // Add equipment with different statuses
    auto equipment1 = createTestEquipment("TEST-001");
    equipment1.setStatus(equipment_tracker::EquipmentStatus::Active);
    
    auto equipment2 = createTestEquipment("TEST-002");
    equipment2.setStatus(equipment_tracker::EquipmentStatus::Maintenance);
    
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillRepeatedly(testing::Return(true));
    
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    
    // Call the method under test
    auto activeEquipment = service->findEquipmentByStatus(equipment_tracker::EquipmentStatus::Active);
    
    // Verify the result
    EXPECT_EQ(activeEquipment.size(), 1);
    EXPECT_EQ(activeEquipment[0].getId(), "TEST-001");
    
    // Test with maintenance status
    auto maintenanceEquipment = service->findEquipmentByStatus(equipment_tracker::EquipmentStatus::Maintenance);
    
    // Verify the result
    EXPECT_EQ(maintenanceEquipment.size(), 1);
    EXPECT_EQ(maintenanceEquipment[0].getId(), "TEST-002");
}

// Test finding active equipment
TEST_F(EquipmentTrackerServiceTest, FindActiveEquipment) {
    // Add equipment with different statuses
    auto equipment1 = createTestEquipment("TEST-001");
    equipment1.setStatus(equipment_tracker::EquipmentStatus::Active);
    
    auto equipment2 = createTestEquipment("TEST-002");
    equipment2.setStatus(equipment_tracker::EquipmentStatus::Inactive);
    
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillRepeatedly(testing::Return(true));
    
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    
    // Call the method under test
    auto activeEquipment = service->findActiveEquipment();
    
    // Verify the result
    EXPECT_EQ(activeEquipment.size(), 1);
    EXPECT_EQ(activeEquipment[0].getId(), "TEST-001");
}

// Test finding equipment in area
TEST_F(EquipmentTrackerServiceTest, FindEquipmentInArea) {
    // Create equipment with positions
    auto equipment1 = createTestEquipment("TEST-001");
    equipment_tracker::Position pos1(37.7749, -122.4194); // San Francisco
    equipment1.setLastPosition(pos1);
    
    auto equipment2 = createTestEquipment("TEST-002");
    equipment_tracker::Position pos2(34.0522, -118.2437); // Los Angeles
    equipment2.setLastPosition(pos2);
    
    auto equipment3 = createTestEquipment("TEST-003");
    // No position set for equipment3
    
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillRepeatedly(testing::Return(true));
    
    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    service->addEquipment(equipment3);
    
    // Call the method under test - search area covering San Francisco
    auto equipmentInArea = service->findEquipmentInArea(
        37.7, -123.0,  // Southwest corner
        38.0, -122.0   // Northeast corner
    );
    
    // Verify the result
    EXPECT_EQ(equipmentInArea.size(), 1);
    EXPECT_EQ(equipmentInArea[0].getId(), "TEST-001");
    
    // Search area covering both San Francisco and Los Angeles
    auto largerAreaEquipment = service->findEquipmentInArea(
        34.0, -123.0,  // Southwest corner
        38.0, -118.0   // Northeast corner
    );
    
    // Verify the result
    EXPECT_EQ(largerAreaEquipment.size(), 2);
}

// Test setting geofence
TEST_F(EquipmentTrackerServiceTest, SetGeofence) {
    auto equipment = createTestEquipment();
    
    // Add the equipment first
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillOnce(testing::Return(true));
    service->addEquipment(equipment);
    
    // Call the method under test
    bool result = service->setGeofence(
        equipment.getId(),
        37.7, -122.5,  // Southwest corner
        37.8, -122.4   // Northeast corner
    );
    
    // Verify the result
    EXPECT_TRUE(result);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>