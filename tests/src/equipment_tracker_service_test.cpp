// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/equipment_tracker_service.h"

// Mock classes for dependencies
class MockGPSTracker : public equipment_tracker::GPSTracker {
public:
    explicit MockGPSTracker(int update_interval_ms = equipment_tracker::DEFAULT_UPDATE_INTERVAL_MS)
        : GPSTracker(update_interval_ms) {}
    
    MOCK_METHOD(void, start, (), (override));
    MOCK_METHOD(void, stop, (), (override));
    MOCK_METHOD(bool, isRunning, (), (const, override));
    MOCK_METHOD(void, registerPositionCallback, (equipment_tracker::PositionCallback), (override));
    MOCK_METHOD(void, simulatePosition, (double, double, double), (override));
};

class MockDataStorage : public equipment_tracker::DataStorage {
public:
    explicit MockDataStorage(const std::string& db_path = equipment_tracker::DEFAULT_DB_PATH)
        : DataStorage(db_path) {}
    
    MOCK_METHOD(bool, initialize, (), (override));
    MOCK_METHOD(bool, saveEquipment, (const equipment_tracker::Equipment&), (override));
    MOCK_METHOD(std::optional<equipment_tracker::Equipment>, loadEquipment, (const equipment_tracker::EquipmentId&), (override));
    MOCK_METHOD(bool, updateEquipment, (const equipment_tracker::Equipment&), (override));
    MOCK_METHOD(bool, deleteEquipment, (const equipment_tracker::EquipmentId&), (override));
    MOCK_METHOD(bool, savePosition, (const equipment_tracker::EquipmentId&, const equipment_tracker::Position&), (override));
    MOCK_METHOD(std::vector<equipment_tracker::Position>, getPositionHistory, 
                (const equipment_tracker::EquipmentId&, const equipment_tracker::Timestamp&, const equipment_tracker::Timestamp&), 
                (override));
    MOCK_METHOD(std::vector<equipment_tracker::Equipment>, getAllEquipment, (), (override));
    MOCK_METHOD(std::vector<equipment_tracker::Equipment>, findEquipmentByStatus, (equipment_tracker::EquipmentStatus), (override));
    MOCK_METHOD(std::vector<equipment_tracker::Equipment>, findEquipmentByType, (equipment_tracker::EquipmentType), (override));
    MOCK_METHOD(std::vector<equipment_tracker::Equipment>, findEquipmentInArea, 
                (double, double, double, double), (override));
};

class MockNetworkManager : public equipment_tracker::NetworkManager {
public:
    explicit MockNetworkManager(const std::string& server_url = equipment_tracker::DEFAULT_SERVER_URL, 
                               int server_port = equipment_tracker::DEFAULT_SERVER_PORT)
        : NetworkManager(server_url, server_port) {}
    
    MOCK_METHOD(bool, connect, (), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(bool, isConnected, (), (const, override));
    MOCK_METHOD(bool, sendPositionUpdate, (const equipment_tracker::EquipmentId&, const equipment_tracker::Position&), (override));
    MOCK_METHOD(bool, syncWithServer, (), (override));
    MOCK_METHOD(void, registerCommandHandler, (std::function<void(const std::string&)>), (override));
};

// Custom test fixture with mocked dependencies
class EquipmentTrackerServiceTest : public ::testing::Test {
protected:
    std::unique_ptr<MockGPSTracker> mock_gps_tracker_;
    std::unique_ptr<MockDataStorage> mock_data_storage_;
    std::unique_ptr<MockNetworkManager> mock_network_manager_;
    
    // Custom service class that allows injecting mocks
    class TestableEquipmentTrackerService : public equipment_tracker::EquipmentTrackerService {
    public:
        TestableEquipmentTrackerService(
            std::unique_ptr<equipment_tracker::GPSTracker> gps_tracker,
            std::unique_ptr<equipment_tracker::DataStorage> data_storage,
            std::unique_ptr<equipment_tracker::NetworkManager> network_manager
        ) {
            gps_tracker_ = std::move(gps_tracker);
            data_storage_ = std::move(data_storage);
            network_manager_ = std::move(network_manager);
            
            // Register callbacks
            gps_tracker_->registerPositionCallback(
                [this](double lat, double lon, double alt, equipment_tracker::Timestamp timestamp) {
                    this->handlePositionUpdate(lat, lon, alt, timestamp);
                });

            network_manager_->registerCommandHandler(
                [this](const std::string &command) {
                    this->handleRemoteCommand(command);
                });
        }
        
        // Expose private methods for testing
        using EquipmentTrackerService::handlePositionUpdate;
        using EquipmentTrackerService::handleRemoteCommand;
        using EquipmentTrackerService::determineEquipmentId;
        
        // Access to internal map for testing
        const std::unordered_map<equipment_tracker::EquipmentId, equipment_tracker::Equipment>& getEquipmentMap() const {
            return equipment_map_;
        }
    };
    
    std::unique_ptr<TestableEquipmentTrackerService> service_;
    
    void SetUp() override {
        mock_gps_tracker_ = std::make_unique<MockGPSTracker>();
        mock_data_storage_ = std::make_unique<MockDataStorage>();
        mock_network_manager_ = std::make_unique<MockNetworkManager>();
        
        // Set up default behaviors for mocks
        EXPECT_CALL(*mock_gps_tracker_, registerPositionCallback(::testing::_)).Times(1);
        EXPECT_CALL(*mock_network_manager_, registerCommandHandler(::testing::_)).Times(1);
        
        service_ = std::make_unique<TestableEquipmentTrackerService>(
            std::move(mock_gps_tracker_),
            std::move(mock_data_storage_),
            std::move(mock_network_manager_)
        );
        
        // Get the mocks back as raw pointers for setting expectations
        mock_gps_tracker_ = dynamic_cast<MockGPSTracker*>(&service_->getGPSTracker());
        mock_data_storage_ = dynamic_cast<MockDataStorage*>(&service_->getDataStorage());
        mock_network_manager_ = dynamic_cast<MockNetworkManager*>(&service_->getNetworkManager());
    }
};

// Test starting the service
TEST_F(EquipmentTrackerServiceTest, StartServiceSuccess) {
    // Set up expectations
    EXPECT_CALL(*mock_data_storage_, initialize())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock_data_storage_, getAllEquipment())
        .WillOnce(::testing::Return(std::vector<equipment_tracker::Equipment>()));
    EXPECT_CALL(*mock_network_manager_, connect())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock_gps_tracker_, start())
        .Times(1);
    
    // Call the method under test
    service_->start();
    
    // Verify the service is running
    EXPECT_TRUE(service_->isRunning());
}

// Test starting the service with data storage initialization failure
TEST_F(EquipmentTrackerServiceTest, StartServiceFailsWhenStorageInitFails) {
    // Set up expectations
    EXPECT_CALL(*mock_data_storage_, initialize())
        .WillOnce(::testing::Return(false));
    
    // These should not be called if initialization fails
    EXPECT_CALL(*mock_data_storage_, getAllEquipment()).Times(0);
    EXPECT_CALL(*mock_network_manager_, connect()).Times(0);
    EXPECT_CALL(*mock_gps_tracker_, start()).Times(0);
    
    // Call the method under test
    service_->start();
    
    // Verify the service is not running
    EXPECT_FALSE(service_->isRunning());
}

// Test stopping the service
TEST_F(EquipmentTrackerServiceTest, StopService) {
    // First start the service
    EXPECT_CALL(*mock_data_storage_, initialize())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock_data_storage_, getAllEquipment())
        .WillOnce(::testing::Return(std::vector<equipment_tracker::Equipment>()));
    EXPECT_CALL(*mock_network_manager_, connect())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock_gps_tracker_, start())
        .Times(1);
    
    service_->start();
    EXPECT_TRUE(service_->isRunning());
    
    // Set up expectations for stop
    EXPECT_CALL(*mock_gps_tracker_, stop())
        .Times(1);
    EXPECT_CALL(*mock_network_manager_, disconnect())
        .Times(1);
    
    // Call the method under test
    service_->stop();
    
    // Verify the service is not running
    EXPECT_FALSE(service_->isRunning());
}

// Test adding equipment
TEST_F(EquipmentTrackerServiceTest, AddEquipmentSuccess) {
    // Create test equipment
    equipment_tracker::Equipment test_equipment("TEST-001", equipment_tracker::EquipmentType::Forklift, "Test Forklift");
    
    // Set up expectations
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    
    // Call the method under test
    bool result = service_->addEquipment(test_equipment);
    
    // Verify the result
    EXPECT_TRUE(result);
    
    // Verify the equipment was added to the map
    auto equipment = service_->getEquipment("TEST-001");
    EXPECT_TRUE(equipment.has_value());
    EXPECT_EQ(equipment->getId(), "TEST-001");
    EXPECT_EQ(equipment->getName(), "Test Forklift");
}

// Test adding duplicate equipment
TEST_F(EquipmentTrackerServiceTest, AddDuplicateEquipmentFails) {
    // Create test equipment
    equipment_tracker::Equipment test_equipment("TEST-001", equipment_tracker::EquipmentType::Forklift, "Test Forklift");
    
    // Add the equipment first
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    service_->addEquipment(test_equipment);
    
    // Try to add it again
    bool result = service_->addEquipment(test_equipment);
    
    // Verify the result
    EXPECT_FALSE(result);
}

// Test removing equipment
TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentSuccess) {
    // Create and add test equipment
    equipment_tracker::Equipment test_equipment("TEST-001", equipment_tracker::EquipmentType::Forklift, "Test Forklift");
    
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    service_->addEquipment(test_equipment);
    
    // Set up expectations for remove
    EXPECT_CALL(*mock_data_storage_, deleteEquipment("TEST-001"))
        .WillOnce(::testing::Return(true));
    
    // Call the method under test
    bool result = service_->removeEquipment("TEST-001");
    
    // Verify the result
    EXPECT_TRUE(result);
    
    // Verify the equipment was removed from the map
    auto equipment = service_->getEquipment("TEST-001");
    EXPECT_FALSE(equipment.has_value());
}

// Test removing non-existent equipment
TEST_F(EquipmentTrackerServiceTest, RemoveNonExistentEquipmentFails) {
    // Call the method under test with a non-existent ID
    bool result = service_->removeEquipment("NONEXISTENT-ID");
    
    // Verify the result
    EXPECT_FALSE(result);
}

// Test getting equipment
TEST_F(EquipmentTrackerServiceTest, GetEquipmentSuccess) {
    // Create and add test equipment
    equipment_tracker::Equipment test_equipment("TEST-001", equipment_tracker::EquipmentType::Forklift, "Test Forklift");
    
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    service_->addEquipment(test_equipment);
    
    // Call the method under test
    auto result = service_->getEquipment("TEST-001");
    
    // Verify the result
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->getId(), "TEST-001");
    EXPECT_EQ(result->getName(), "Test Forklift");
}

// Test getting non-existent equipment
TEST_F(EquipmentTrackerServiceTest, GetNonExistentEquipment) {
    // Call the method under test with a non-existent ID
    auto result = service_->getEquipment("NONEXISTENT-ID");
    
    // Verify the result
    EXPECT_FALSE(result.has_value());
}

// Test getting all equipment
TEST_F(EquipmentTrackerServiceTest, GetAllEquipment) {
    // Create and add test equipment
    equipment_tracker::Equipment test_equipment1("TEST-001", equipment_tracker::EquipmentType::Forklift, "Test Forklift 1");
    equipment_tracker::Equipment test_equipment2("TEST-002", equipment_tracker::EquipmentType::Crane, "Test Crane");
    
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    service_->addEquipment(test_equipment1);
    service_->addEquipment(test_equipment2);
    
    // Call the method under test
    auto result = service_->getAllEquipment();
    
    // Verify the result
    EXPECT_EQ(result.size(), 2);
    
    // Check that both equipment items are in the result
    bool found1 = false, found2 = false;
    for (const auto& equipment : result) {
        if (equipment.getId() == "TEST-001") found1 = true;
        if (equipment.getId() == "TEST-002") found2 = true;
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

// Test finding equipment by status
TEST_F(EquipmentTrackerServiceTest, FindEquipmentByStatus) {
    // Create and add test equipment with different statuses
    equipment_tracker::Equipment test_equipment1("TEST-001", equipment_tracker::EquipmentType::Forklift, "Test Forklift");
    test_equipment1.setStatus(equipment_tracker::EquipmentStatus::Active);
    
    equipment_tracker::Equipment test_equipment2("TEST-002", equipment_tracker::EquipmentType::Crane, "Test Crane");
    test_equipment2.setStatus(equipment_tracker::EquipmentStatus::Maintenance);
    
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    service_->addEquipment(test_equipment1);
    service_->addEquipment(test_equipment2);
    
    // Call the method under test
    auto result = service_->findEquipmentByStatus(equipment_tracker::EquipmentStatus::Active);
    
    // Verify the result
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].getId(), "TEST-001");
}

// Test finding active equipment
TEST_F(EquipmentTrackerServiceTest, FindActiveEquipment) {
    // Create and add test equipment with different statuses
    equipment_tracker::Equipment test_equipment1("TEST-001", equipment_tracker::EquipmentType::Forklift, "Test Forklift");
    test_equipment1.setStatus(equipment_tracker::EquipmentStatus::Active);
    
    equipment_tracker::Equipment test_equipment2("TEST-002", equipment_tracker::EquipmentType::Crane, "Test Crane");
    test_equipment2.setStatus(equipment_tracker::EquipmentStatus::Inactive);
    
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    service_->addEquipment(test_equipment1);
    service_->addEquipment(test_equipment2);
    
    // Call the method under test
    auto result = service_->findActiveEquipment();
    
    // Verify the result
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].getId(), "TEST-001");
}

// Test finding equipment in area
TEST_F(EquipmentTrackerServiceTest, FindEquipmentInArea) {
    // Create equipment with positions
    equipment_tracker::Equipment test_equipment1("TEST-001", equipment_tracker::EquipmentType::Forklift, "Test Forklift");
    equipment_tracker::Position pos1(37.7749, -122.4194); // San Francisco
    test_equipment1.setLastPosition(pos1);
    
    equipment_tracker::Equipment test_equipment2("TEST-002", equipment_tracker::EquipmentType::Crane, "Test Crane");
    equipment_tracker::Position pos2(40.7128, -74.0060); // New York
    test_equipment2.setLastPosition(pos2);
    
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    service_->addEquipment(test_equipment1);
    service_->addEquipment(test_equipment2);
    
    // Call the method under test - search around San Francisco
    auto result = service_->findEquipmentInArea(37.7, -122.5, 37.8, -122.3);
    
    // Verify the result
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].getId(), "TEST-001");
}

// Test setting geofence
TEST_F(EquipmentTrackerServiceTest, SetGeofence) {
    // Create and add test equipment
    equipment_tracker::Equipment test_equipment("TEST-001", equipment_tracker::EquipmentType::Forklift, "Test Forklift");
    
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    service_->addEquipment(test_equipment);
    
    // Call the method under test
    bool result = service_->setGeofence("TEST-001", 37.7, -122.5, 37.8, -122.3);
    
    // Verify the result
    EXPECT_TRUE(result);
}

// Test handling position update
TEST_F(EquipmentTrackerServiceTest, HandlePositionUpdate) {
    // Create and add test equipment
    equipment_tracker::Equipment test_equipment("FORKLIFT-001", equipment_tracker::EquipmentType::Forklift, "Test Forklift");
    
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    service_->addEquipment(test_equipment);
    
    // Set up expectations
    EXPECT_CALL(*mock_data_storage_, savePosition(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock_data_storage_, updateEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock_network_manager_, sendPositionUpdate(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(true));
    
    // Call the method under test
    auto timestamp = equipment_tracker::getCurrentTimestamp();
    service_->handlePositionUpdate(37.7749, -122.4194, 10.0, timestamp);
    
    // Verify the equipment was updated
    auto updated_equipment = service_->getEquipment("FORKLIFT-001");
    EXPECT_TRUE(updated_equipment.has_value());
    EXPECT_EQ(updated_equipment->getStatus(), equipment_tracker::EquipmentStatus::Active);
    
    auto position = updated_equipment->getLastPosition();
    EXPECT_TRUE(position.has_value());
    EXPECT_DOUBLE_EQ(position->getLatitude(), 37.7749);
    EXPECT_DOUBLE_EQ(position->getLongitude(), -122.4194);
    EXPECT_DOUBLE_EQ(position->getAltitude(), 10.0);
}

// Test handling remote command
TEST_F(EquipmentTrackerServiceTest, HandleRemoteCommand) {
    // Create and add test equipment
    equipment_tracker::Equipment test_equipment("TEST-001", equipment_tracker::EquipmentType::Forklift, "Test Forklift");
    
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    service_->addEquipment(test_equipment);
    
    // Call the method under test
    service_->handleRemoteCommand("STATUS_REQUEST");
    
    // No specific expectations to verify, just ensure it doesn't crash
}

// Test determining equipment ID
TEST_F(EquipmentTrackerServiceTest, DetermineEquipmentId) {
    // Create and add test equipment
    equipment_tracker::Equipment test_equipment("TEST-001", equipment_tracker::EquipmentType::Forklift, "Test Forklift");
    
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    service_->addEquipment(test_equipment);
    
    // Call the method under test
    auto result = service_->determineEquipmentId();
    
    // Verify the result
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, "TEST-001");
}

// Test determining equipment ID with empty map
TEST_F(EquipmentTrackerServiceTest, DetermineEquipmentIdWithEmptyMap) {
    // Call the method under test with an empty map
    auto result = service_->determineEquipmentId();
    
    // Verify the result
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, "FORKLIFT-001");
}
// </test_code>