// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/equipment_tracker_service.h"

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
    MOCK_METHOD(std::vector<Position>, getPositionHistory, 
                (const EquipmentId&, const Timestamp&, const Timestamp&), (override));
    MOCK_METHOD(std::vector<Equipment>, getAllEquipment, (), (override));
    MOCK_METHOD(std::vector<Equipment>, findEquipmentByStatus, (EquipmentStatus), (override));
    MOCK_METHOD(std::vector<Equipment>, findEquipmentByType, (EquipmentType), (override));
    MOCK_METHOD(std::vector<Equipment>, findEquipmentInArea, 
                (double, double, double, double), (override));
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

// Test fixture
class EquipmentTrackerServiceTest : public ::testing::Test {
protected:
    std::unique_ptr<MockGPSTracker> mockGpsTracker;
    std::unique_ptr<MockDataStorage> mockDataStorage;
    std::unique_ptr<MockNetworkManager> mockNetworkManager;
    
    void SetUp() override {
        mockGpsTracker = std::make_unique<MockGPSTracker>();
        mockDataStorage = std::make_unique<MockDataStorage>();
        mockNetworkManager = std::make_unique<MockNetworkManager>();
    }
};

// Custom matcher for Equipment objects
MATCHER_P(EquipmentEq, expected, "Equipment equality") {
    return arg.getId() == expected.getId() &&
           arg.getType() == expected.getType() &&
           arg.getName() == expected.getName() &&
           arg.getStatus() == expected.getStatus();
}

// Helper to create a test equipment
Equipment createTestEquipment(const std::string& id = "TEST-001", 
                             EquipmentType type = EquipmentType::Forklift,
                             const std::string& name = "Test Equipment") {
    return Equipment(id, type, name);
}

// Test class that exposes protected methods for testing
class TestableEquipmentTrackerService : public EquipmentTrackerService {
public:
    TestableEquipmentTrackerService(
        std::unique_ptr<GPSTracker> gps_tracker,
        std::unique_ptr<DataStorage> data_storage,
        std::unique_ptr<NetworkManager> network_manager
    ) {
        gps_tracker_ = std::move(gps_tracker);
        data_storage_ = std::move(data_storage);
        network_manager_ = std::move(network_manager);
    }
    
    using EquipmentTrackerService::handlePositionUpdate;
    using EquipmentTrackerService::handleRemoteCommand;
    using EquipmentTrackerService::determineEquipmentId;
    using EquipmentTrackerService::loadEquipment;
};

// Tests for EquipmentTrackerService

TEST_F(EquipmentTrackerServiceTest, StartInitializesComponentsCorrectly) {
    // Setup expectations
    EXPECT_CALL(*mockDataStorage, initialize())
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mockDataStorage, getAllEquipment())
        .WillOnce(testing::Return(std::vector<Equipment>{}));
    EXPECT_CALL(*mockNetworkManager, connect())
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mockGpsTracker, start());
    
    // Create service with mocks
    TestableEquipmentTrackerService service(
        std::move(mockGpsTracker),
        std::move(mockDataStorage),
        std::move(mockNetworkManager)
    );
    
    // Execute
    service.start();
    
    // Verify
    EXPECT_TRUE(service.isRunning());
}

TEST_F(EquipmentTrackerServiceTest, StartFailsWhenDataStorageInitializationFails) {
    // Setup expectations
    EXPECT_CALL(*mockDataStorage, initialize())
        .WillOnce(testing::Return(false));
    
    // No other components should be initialized if data storage fails
    EXPECT_CALL(*mockNetworkManager, connect()).Times(0);
    EXPECT_CALL(*mockGpsTracker, start()).Times(0);
    
    // Create service with mocks
    TestableEquipmentTrackerService service(
        std::move(mockGpsTracker),
        std::move(mockDataStorage),
        std::move(mockNetworkManager)
    );
    
    // Execute
    service.start();
    
    // Verify
    EXPECT_FALSE(service.isRunning());
}

TEST_F(EquipmentTrackerServiceTest, StopDisconnectsComponentsCorrectly) {
    // Setup expectations for start
    EXPECT_CALL(*mockDataStorage, initialize())
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mockDataStorage, getAllEquipment())
        .WillOnce(testing::Return(std::vector<Equipment>{}));
    EXPECT_CALL(*mockNetworkManager, connect())
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mockGpsTracker, start());
    
    // Setup expectations for stop
    EXPECT_CALL(*mockGpsTracker, stop());
    EXPECT_CALL(*mockNetworkManager, disconnect());
    
    // Create service with mocks
    TestableEquipmentTrackerService service(
        std::move(mockGpsTracker),
        std::move(mockDataStorage),
        std::move(mockNetworkManager)
    );
    
    // Execute
    service.start();
    service.stop();
    
    // Verify
    EXPECT_FALSE(service.isRunning());
}

TEST_F(EquipmentTrackerServiceTest, AddEquipmentSucceeds) {
    // Create test equipment
    Equipment testEquipment = createTestEquipment();
    
    // Setup expectations
    EXPECT_CALL(*mockDataStorage, saveEquipment(EquipmentEq(testEquipment)))
        .WillOnce(testing::Return(true));
    
    // Create service with mocks
    TestableEquipmentTrackerService service(
        std::move(mockGpsTracker),
        std::move(mockDataStorage),
        std::move(mockNetworkManager)
    );
    
    // Execute
    bool result = service.addEquipment(testEquipment);
    
    // Verify
    EXPECT_TRUE(result);
    
    // Verify equipment was added by retrieving it
    auto retrievedEquipment = service.getEquipment(testEquipment.getId());
    ASSERT_TRUE(retrievedEquipment.has_value());
    EXPECT_EQ(retrievedEquipment->getId(), testEquipment.getId());
    EXPECT_EQ(retrievedEquipment->getName(), testEquipment.getName());
    EXPECT_EQ(retrievedEquipment->getType(), testEquipment.getType());
}

TEST_F(EquipmentTrackerServiceTest, AddEquipmentFailsForDuplicateId) {
    // Create test equipment
    Equipment testEquipment = createTestEquipment();
    
    // Setup expectations for first add (success)
    EXPECT_CALL(*mockDataStorage, saveEquipment(EquipmentEq(testEquipment)))
        .WillOnce(testing::Return(true));
    
    // Create service with mocks
    TestableEquipmentTrackerService service(
        std::move(mockGpsTracker),
        std::move(mockDataStorage),
        std::move(mockNetworkManager)
    );
    
    // Add equipment first time
    EXPECT_TRUE(service.addEquipment(testEquipment));
    
    // Try to add again with same ID (should fail)
    bool result = service.addEquipment(testEquipment);
    
    // Verify
    EXPECT_FALSE(result);
}

TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentSucceeds) {
    // Create test equipment
    Equipment testEquipment = createTestEquipment();
    
    // Setup expectations
    EXPECT_CALL(*mockDataStorage, saveEquipment(EquipmentEq(testEquipment)))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mockDataStorage, deleteEquipment(testEquipment.getId()))
        .WillOnce(testing::Return(true));
    
    // Create service with mocks
    TestableEquipmentTrackerService service(
        std::move(mockGpsTracker),
        std::move(mockDataStorage),
        std::move(mockNetworkManager)
    );
    
    // Add equipment first
    service.addEquipment(testEquipment);
    
    // Execute remove
    bool result = service.removeEquipment(testEquipment.getId());
    
    // Verify
    EXPECT_TRUE(result);
    EXPECT_FALSE(service.getEquipment(testEquipment.getId()).has_value());
}

TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentFailsForNonexistentId) {
    // Setup expectations
    EXPECT_CALL(*mockDataStorage, deleteEquipment(testing::_))
        .Times(0);
    
    // Create service with mocks
    TestableEquipmentTrackerService service(
        std::move(mockGpsTracker),
        std::move(mockDataStorage),
        std::move(mockNetworkManager)
    );
    
    // Execute remove with non-existent ID
    bool result = service.removeEquipment("NONEXISTENT-ID");
    
    // Verify
    EXPECT_FALSE(result);
}

TEST_F(EquipmentTrackerServiceTest, GetAllEquipmentReturnsCorrectList) {
    // Create test equipment
    Equipment equipment1 = createTestEquipment("TEST-001", EquipmentType::Forklift, "Forklift 1");
    Equipment equipment2 = createTestEquipment("TEST-002", EquipmentType::Crane, "Crane 1");
    Equipment equipment3 = createTestEquipment("TEST-003", EquipmentType::Bulldozer, "Bulldozer 1");
    
    // Setup expectations
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillRepeatedly(testing::Return(true));
    
    // Create service with mocks
    TestableEquipmentTrackerService service(
        std::move(mockGpsTracker),
        std::move(mockDataStorage),
        std::move(mockNetworkManager)
    );
    
    // Add equipment
    service.addEquipment(equipment1);
    service.addEquipment(equipment2);
    service.addEquipment(equipment3);
    
    // Execute
    std::vector<Equipment> allEquipment = service.getAllEquipment();
    
    // Verify
    EXPECT_EQ(allEquipment.size(), 3);
    
    // Check if all equipment is in the result
    auto hasEquipment = [&allEquipment](const std::string& id) {
        return std::find_if(allEquipment.begin(), allEquipment.end(),
                          [&id](const Equipment& e) { return e.getId() == id; }) != allEquipment.end();
    };
    
    EXPECT_TRUE(hasEquipment("TEST-001"));
    EXPECT_TRUE(hasEquipment("TEST-002"));
    EXPECT_TRUE(hasEquipment("TEST-003"));
}

TEST_F(EquipmentTrackerServiceTest, FindEquipmentByStatusReturnsCorrectList) {
    // Create test equipment with different statuses
    Equipment equipment1 = createTestEquipment("TEST-001", EquipmentType::Forklift, "Forklift 1");
    equipment1.setStatus(EquipmentStatus::Active);
    
    Equipment equipment2 = createTestEquipment("TEST-002", EquipmentType::Crane, "Crane 1");
    equipment2.setStatus(EquipmentStatus::Inactive);
    
    Equipment equipment3 = createTestEquipment("TEST-003", EquipmentType::Bulldozer, "Bulldozer 1");
    equipment3.setStatus(EquipmentStatus::Active);
    
    // Setup expectations
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillRepeatedly(testing::Return(true));
    
    // Create service with mocks
    TestableEquipmentTrackerService service(
        std::move(mockGpsTracker),
        std::move(mockDataStorage),
        std::move(mockNetworkManager)
    );
    
    // Add equipment
    service.addEquipment(equipment1);
    service.addEquipment(equipment2);
    service.addEquipment(equipment3);
    
    // Execute
    std::vector<Equipment> activeEquipment = service.findEquipmentByStatus(EquipmentStatus::Active);
    
    // Verify
    EXPECT_EQ(activeEquipment.size(), 2);
    
    // Check if correct equipment is in the result
    auto hasEquipment = [&activeEquipment](const std::string& id) {
        return std::find_if(activeEquipment.begin(), activeEquipment.end(),
                          [&id](const Equipment& e) { return e.getId() == id; }) != activeEquipment.end();
    };
    
    EXPECT_TRUE(hasEquipment("TEST-001"));
    EXPECT_TRUE(hasEquipment("TEST-003"));
    EXPECT_FALSE(hasEquipment("TEST-002"));
}

TEST_F(EquipmentTrackerServiceTest, FindActiveEquipmentReturnsCorrectList) {
    // Create test equipment with different statuses
    Equipment equipment1 = createTestEquipment("TEST-001", EquipmentType::Forklift, "Forklift 1");
    equipment1.setStatus(EquipmentStatus::Active);
    
    Equipment equipment2 = createTestEquipment("TEST-002", EquipmentType::Crane, "Crane 1");
    equipment2.setStatus(EquipmentStatus::Inactive);
    
    // Setup expectations
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillRepeatedly(testing::Return(true));
    
    // Create service with mocks
    TestableEquipmentTrackerService service(
        std::move(mockGpsTracker),
        std::move(mockDataStorage),
        std::move(mockNetworkManager)
    );
    
    // Add equipment
    service.addEquipment(equipment1);
    service.addEquipment(equipment2);
    
    // Execute
    std::vector<Equipment> activeEquipment = service.findActiveEquipment();
    
    // Verify
    EXPECT_EQ(activeEquipment.size(), 1);
    EXPECT_EQ(activeEquipment[0].getId(), "TEST-001");
}

TEST_F(EquipmentTrackerServiceTest, FindEquipmentInAreaReturnsCorrectList) {
    // Create test equipment with positions
    Equipment equipment1 = createTestEquipment("TEST-001");
    Position pos1(37.7749, -122.4194); // San Francisco
    equipment1.setLastPosition(pos1);
    
    Equipment equipment2 = createTestEquipment("TEST-002");
    Position pos2(34.0522, -118.2437); // Los Angeles
    equipment2.setLastPosition(pos2);
    
    Equipment equipment3 = createTestEquipment("TEST-003");
    Position pos3(40.7128, -74.0060); // New York
    equipment3.setLastPosition(pos3);
    
    // Setup expectations
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillRepeatedly(testing::Return(true));
    
    // Create service with mocks
    TestableEquipmentTrackerService service(
        std::move(mockGpsTracker),
        std::move(mockDataStorage),
        std::move(mockNetworkManager)
    );
    
    // Add equipment
    service.addEquipment(equipment1);
    service.addEquipment(equipment2);
    service.addEquipment(equipment3);
    
    // Execute - search for equipment in California
    std::vector<Equipment> equipmentInCalifornia = service.findEquipmentInArea(
        33.0, -125.0, // Southwest corner
        39.0, -115.0  // Northeast corner
    );
    
    // Verify
    EXPECT_EQ(equipmentInCalifornia.size(), 2);
    
    // Check if correct equipment is in the result
    auto hasEquipment = [&equipmentInCalifornia](const std::string& id) {
        return std::find_if(equipmentInCalifornia.begin(), equipmentInCalifornia.end(),
                          [&id](const Equipment& e) { return e.getId() == id; }) != equipmentInCalifornia.end();
    };
    
    EXPECT_TRUE(hasEquipment("TEST-001"));
    EXPECT_TRUE(hasEquipment("TEST-002"));
    EXPECT_FALSE(hasEquipment("TEST-003"));
}

TEST_F(EquipmentTrackerServiceTest, SetGeofenceReturnsTrue) {
    // Create service with mocks
    TestableEquipmentTrackerService service(
        std::move(mockGpsTracker),
        std::move(mockDataStorage),
        std::move(mockNetworkManager)
    );
    
    // Execute
    bool result = service.setGeofence("TEST-001", 37.7, -122.5, 37.8, -122.4);
    
    // Verify
    EXPECT_TRUE(result);
}

TEST_F(EquipmentTrackerServiceTest, HandlePositionUpdateUpdatesEquipmentPosition) {
    // Create test equipment
    Equipment testEquipment = createTestEquipment("FORKLIFT-001");
    
    // Setup expectations
    EXPECT_CALL(*mockDataStorage, saveEquipment(EquipmentEq(testEquipment)))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mockDataStorage, savePosition(testing::_, testing::_))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mockDataStorage, updateEquipment(testing::_))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mockNetworkManager, sendPositionUpdate(testing::_, testing::_))
        .WillOnce(testing::Return(true));
    
    // Create service with mocks
    TestableEquipmentTrackerService service(
        std::move(mockGpsTracker),
        std::move(mockDataStorage),
        std::move(mockNetworkManager)
    );
    
    // Add equipment
    service.addEquipment(testEquipment);
    
    // Execute
    double latitude = 37.7749;
    double longitude = -122.4194;
    double altitude = 10.0;
    Timestamp timestamp = getCurrentTimestamp();
    
    service.handlePositionUpdate(latitude, longitude, altitude, timestamp);
    
    // Verify
    auto updatedEquipment = service.getEquipment("FORKLIFT-001");
    ASSERT_TRUE(updatedEquipment.has_value());
    ASSERT_TRUE(updatedEquipment->getLastPosition().has_value());
    
    auto position = updatedEquipment->getLastPosition().value();
    EXPECT_DOUBLE_EQ(position.getLatitude(), latitude);
    EXPECT_DOUBLE_EQ(position.getLongitude(), longitude);
    EXPECT_DOUBLE_EQ(position.getAltitude(), altitude);
    EXPECT_EQ(updatedEquipment->getStatus(), EquipmentStatus::Active);
}

TEST_F(EquipmentTrackerServiceTest, HandleRemoteCommandProcessesStatusRequest) {
    // Create test equipment
    Equipment equipment1 = createTestEquipment("TEST-001");
    Equipment equipment2 = createTestEquipment("TEST-002");
    
    // Setup expectations
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillRepeatedly(testing::Return(true));
    
    // Create service with mocks
    TestableEquipmentTrackerService service(
        std::move(mockGpsTracker),
        std::move(mockDataStorage),
        std::move(mockNetworkManager)
    );
    
    // Add equipment
    service.addEquipment(equipment1);
    service.addEquipment(equipment2);
    
    // Execute
    service.handleRemoteCommand("STATUS_REQUEST");
    
    // No specific verification needed as this just logs to console
    // This test mainly ensures the method doesn't crash
}

TEST_F(EquipmentTrackerServiceTest, DetermineEquipmentIdReturnsFirstEquipmentOrDefault) {
    // Create test equipment
    Equipment testEquipment = createTestEquipment("TEST-001");
    
    // Setup expectations
    EXPECT_CALL(*mockDataStorage, saveEquipment(testing::_))
        .WillOnce(testing::Return(true));
    
    // Create service with mocks
    TestableEquipmentTrackerService service(
        std::move(mockGpsTracker),
        std::move(mockDataStorage),
        std::move(mockNetworkManager)
    );
    
    // Test with empty map (should return default)
    auto emptyResult = service.determineEquipmentId();
    ASSERT_TRUE(emptyResult.has_value());
    EXPECT_EQ(*emptyResult, "FORKLIFT-001");
    
    // Add equipment
    service.addEquipment(testEquipment);
    
    // Test with equipment in map
    auto result = service.determineEquipmentId();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "TEST-001");
}

} // namespace equipment_tracker

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>