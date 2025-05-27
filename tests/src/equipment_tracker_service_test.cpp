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

class Position {
public:
    Position(double lat, double lon, double alt, double accuracy, uint64_t timestamp)
        : lat_(lat), lon_(lon), alt_(alt), accuracy_(accuracy), timestamp_(timestamp) {}
    
    double getLatitude() const { return lat_; }
    double getLongitude() const { return lon_; }
    double getAltitude() const { return alt_; }
    double getAccuracy() const { return accuracy_; }
    uint64_t getTimestamp() const { return timestamp_; }
    
private:
    double lat_;
    double lon_;
    double alt_;
    double accuracy_;
    uint64_t timestamp_;
};

enum class EquipmentStatus {
    Active,
    Inactive,
    Maintenance,
    Unknown
};

using EquipmentId = std::string;
using Timestamp = uint64_t;

class Equipment {
public:
    Equipment(const EquipmentId& id, const std::string& name, EquipmentStatus status = EquipmentStatus::Unknown)
        : id_(id), name_(name), status_(status) {}
    
    EquipmentId getId() const { return id_; }
    std::string getName() const { return name_; }
    EquipmentStatus getStatus() const { return status_; }
    void setStatus(EquipmentStatus status) { status_ = status; }
    
    std::optional<Position> getLastPosition() const { return last_position_; }
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

class DataStorage {
public:
    MOCK_METHOD(bool, initialize, ());
    MOCK_METHOD(bool, saveEquipment, (const Equipment& equipment));
    MOCK_METHOD(bool, updateEquipment, (const Equipment& equipment));
    MOCK_METHOD(bool, deleteEquipment, (const EquipmentId& id));
    MOCK_METHOD(std::vector<Equipment>, getAllEquipment, ());
    MOCK_METHOD(bool, savePosition, (const EquipmentId& id, const Position& position));
};

class NetworkManager {
public:
    using CommandHandler = std::function<void(const std::string&)>;
    
    MOCK_METHOD(void, registerCommandHandler, (CommandHandler handler));
    MOCK_METHOD(void, connect, ());
    MOCK_METHOD(void, disconnect, ());
    MOCK_METHOD(void, sendPositionUpdate, (const EquipmentId& id, const Position& position));
};

// Include the class we're testing
#include "equipment_tracker/equipment_tracker_service.h"

// Create mock classes using GMock
class MockGPSTracker : public GPSTracker {
public:
    MOCK_METHOD(void, registerPositionCallback, (PositionCallback callback), (override));
    MOCK_METHOD(void, start, (), (override));
    MOCK_METHOD(void, stop, (), (override));
};

class MockDataStorage : public DataStorage {
public:
    MOCK_METHOD(bool, initialize, (), (override));
    MOCK_METHOD(bool, saveEquipment, (const Equipment& equipment), (override));
    MOCK_METHOD(bool, updateEquipment, (const Equipment& equipment), (override));
    MOCK_METHOD(bool, deleteEquipment, (const EquipmentId& id), (override));
    MOCK_METHOD(std::vector<Equipment>, getAllEquipment, (), (override));
    MOCK_METHOD(bool, savePosition, (const EquipmentId& id, const Position& position), (override));
};

class MockNetworkManager : public NetworkManager {
public:
    MOCK_METHOD(void, registerCommandHandler, (CommandHandler handler), (override));
    MOCK_METHOD(void, connect, (), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(void, sendPositionUpdate, (const EquipmentId& id, const Position& position), (override));
};

// Test fixture
class EquipmentTrackerServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create mock objects
        mock_gps_tracker_ = std::make_unique<MockGPSTracker>();
        mock_data_storage_ = std::make_unique<MockDataStorage>();
        mock_network_manager_ = std::make_unique<MockNetworkManager>();
        
        // Capture pointers before they're moved
        mock_gps_tracker_ptr_ = mock_gps_tracker_.get();
        mock_data_storage_ptr_ = mock_data_storage_.get();
        mock_network_manager_ptr_ = mock_network_manager_.get();
        
        // Create service with mock dependencies
        service_ = std::make_unique<equipment_tracker::EquipmentTrackerService>(
            std::move(mock_gps_tracker_),
            std::move(mock_data_storage_),
            std::move(mock_network_manager_)
        );
    }
    
    // Test equipment data
    Equipment createTestEquipment(const std::string& id_suffix = "001", 
                                 EquipmentStatus status = EquipmentStatus::Active) {
        return Equipment("TEST-" + id_suffix, "Test Equipment " + id_suffix, status);
    }
    
    std::unique_ptr<equipment_tracker::EquipmentTrackerService> service_;
    std::unique_ptr<MockGPSTracker> mock_gps_tracker_;
    std::unique_ptr<MockDataStorage> mock_data_storage_;
    std::unique_ptr<MockNetworkManager> mock_network_manager_;
    
    // Raw pointers to mocks (for use after they're moved)
    MockGPSTracker* mock_gps_tracker_ptr_;
    MockDataStorage* mock_data_storage_ptr_;
    MockNetworkManager* mock_network_manager_ptr_;
    
    const double DEFAULT_POSITION_ACCURACY = 5.0; // meters
};

// Test start method
TEST_F(EquipmentTrackerServiceTest, StartInitializesSystemCorrectly) {
    // Setup expectations
    EXPECT_CALL(*mock_data_storage_ptr_, initialize())
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mock_data_storage_ptr_, getAllEquipment())
        .WillOnce(testing::Return(std::vector<Equipment>{}));
    EXPECT_CALL(*mock_network_manager_ptr_, connect());
    EXPECT_CALL(*mock_gps_tracker_ptr_, start());
    
    // Call method under test
    service_->start();
    
    // Verify service is running
    EXPECT_TRUE(service_->isRunning());
}

// Test start method with initialization failure
TEST_F(EquipmentTrackerServiceTest, StartFailsWhenStorageInitializationFails) {
    // Setup expectations
    EXPECT_CALL(*mock_data_storage_ptr_, initialize())
        .WillOnce(testing::Return(false));
    
    // These should not be called if initialization fails
    EXPECT_CALL(*mock_network_manager_ptr_, connect()).Times(0);
    EXPECT_CALL(*mock_gps_tracker_ptr_, start()).Times(0);
    
    // Call method under test
    service_->start();
    
    // Verify service is not running
    EXPECT_FALSE(service_->isRunning());
}

// Test stop method
TEST_F(EquipmentTrackerServiceTest, StopShutdownsSystemCorrectly) {
    // First start the service
    EXPECT_CALL(*mock_data_storage_ptr_, initialize())
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mock_data_storage_ptr_, getAllEquipment())
        .WillOnce(testing::Return(std::vector<Equipment>{}));
    EXPECT_CALL(*mock_network_manager_ptr_, connect());
    EXPECT_CALL(*mock_gps_tracker_ptr_, start());
    
    service_->start();
    EXPECT_TRUE(service_->isRunning());
    
    // Setup expectations for stop
    EXPECT_CALL(*mock_gps_tracker_ptr_, stop());
    EXPECT_CALL(*mock_network_manager_ptr_, disconnect());
    
    // Call method under test
    service_->stop();
    
    // Verify service is not running
    EXPECT_FALSE(service_->isRunning());
}

// Test addEquipment method
TEST_F(EquipmentTrackerServiceTest, AddEquipmentStoresEquipmentCorrectly) {
    Equipment test_equipment = createTestEquipment();
    
    // Setup expectations
    EXPECT_CALL(*mock_data_storage_ptr_, saveEquipment(testing::_))
        .WillOnce(testing::Return(true));
    
    // Call method under test
    bool result = service_->addEquipment(test_equipment);
    
    // Verify result
    EXPECT_TRUE(result);
    
    // Verify equipment was stored
    auto stored_equipment = service_->getEquipment(test_equipment.getId());
    EXPECT_TRUE(stored_equipment.has_value());
    EXPECT_EQ(stored_equipment->getId(), test_equipment.getId());
    EXPECT_EQ(stored_equipment->getName(), test_equipment.getName());
    EXPECT_EQ(stored_equipment->getStatus(), test_equipment.getStatus());
}

// Test addEquipment with duplicate ID
TEST_F(EquipmentTrackerServiceTest, AddEquipmentFailsWithDuplicateId) {
    Equipment test_equipment = createTestEquipment();
    
    // Add equipment first time
    EXPECT_CALL(*mock_data_storage_ptr_, saveEquipment(testing::_))
        .WillOnce(testing::Return(true));
    EXPECT_TRUE(service_->addEquipment(test_equipment));
    
    // Try to add again with same ID
    bool result = service_->addEquipment(test_equipment);
    
    // Verify result
    EXPECT_FALSE(result);
}

// Test removeEquipment method
TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentDeletesEquipmentCorrectly) {
    Equipment test_equipment = createTestEquipment();
    
    // Add equipment first
    EXPECT_CALL(*mock_data_storage_ptr_, saveEquipment(testing::_))
        .WillOnce(testing::Return(true));
    service_->addEquipment(test_equipment);
    
    // Setup expectations for remove
    EXPECT_CALL(*mock_data_storage_ptr_, deleteEquipment(test_equipment.getId()))
        .WillOnce(testing::Return(true));
    
    // Call method under test
    bool result = service_->removeEquipment(test_equipment.getId());
    
    // Verify result
    EXPECT_TRUE(result);
    
    // Verify equipment was removed
    auto stored_equipment = service_->getEquipment(test_equipment.getId());
    EXPECT_FALSE(stored_equipment.has_value());
}

// Test removeEquipment with non-existent ID
TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentFailsWithNonExistentId) {
    // Call method under test with non-existent ID
    bool result = service_->removeEquipment("NON-EXISTENT-ID");
    
    // Verify result
    EXPECT_FALSE(result);
}

// Test getEquipment method
TEST_F(EquipmentTrackerServiceTest, GetEquipmentReturnsCorrectEquipment) {
    Equipment test_equipment = createTestEquipment();
    
    // Add equipment
    EXPECT_CALL(*mock_data_storage_ptr_, saveEquipment(testing::_))
        .WillOnce(testing::Return(true));
    service_->addEquipment(test_equipment);
    
    // Call method under test
    auto result = service_->getEquipment(test_equipment.getId());
    
    // Verify result
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->getId(), test_equipment.getId());
    EXPECT_EQ(result->getName(), test_equipment.getName());
    EXPECT_EQ(result->getStatus(), test_equipment.getStatus());
}

// Test getEquipment with non-existent ID
TEST_F(EquipmentTrackerServiceTest, GetEquipmentReturnsNulloptWithNonExistentId) {
    // Call method under test with non-existent ID
    auto result = service_->getEquipment("NON-EXISTENT-ID");
    
    // Verify result
    EXPECT_FALSE(result.has_value());
}

// Test getAllEquipment method
TEST_F(EquipmentTrackerServiceTest, GetAllEquipmentReturnsAllEquipment) {
    // Add multiple equipment
    Equipment equipment1 = createTestEquipment("001");
    Equipment equipment2 = createTestEquipment("002");
    Equipment equipment3 = createTestEquipment("003");
    
    EXPECT_CALL(*mock_data_storage_ptr_, saveEquipment(testing::_))
        .WillRepeatedly(testing::Return(true));
    
    service_->addEquipment(equipment1);
    service_->addEquipment(equipment2);
    service_->addEquipment(equipment3);
    
    // Call method under test
    auto result = service_->getAllEquipment();
    
    // Verify result
    EXPECT_EQ(result.size(), 3);
    
    // Check if all equipment are in the result
    auto hasEquipment = [&result](const Equipment& equipment) {
        return std::find_if(result.begin(), result.end(), 
                           [&equipment](const Equipment& e) { 
                               return e.getId() == equipment.getId(); 
                           }) != result.end();
    };
    
    EXPECT_TRUE(hasEquipment(equipment1));
    EXPECT_TRUE(hasEquipment(equipment2));
    EXPECT_TRUE(hasEquipment(equipment3));
}

// Test findEquipmentByStatus method
TEST_F(EquipmentTrackerServiceTest, FindEquipmentByStatusReturnsCorrectEquipment) {
    // Add equipment with different statuses
    Equipment active1 = createTestEquipment("001", EquipmentStatus::Active);
    Equipment active2 = createTestEquipment("002", EquipmentStatus::Active);
    Equipment inactive = createTestEquipment("003", EquipmentStatus::Inactive);
    Equipment maintenance = createTestEquipment("004", EquipmentStatus::Maintenance);
    
    EXPECT_CALL(*mock_data_storage_ptr_, saveEquipment(testing::_))
        .WillRepeatedly(testing::Return(true));
    
    service_->addEquipment(active1);
    service_->addEquipment(active2);
    service_->addEquipment(inactive);
    service_->addEquipment(maintenance);
    
    // Call method under test
    auto active_result = service_->findEquipmentByStatus(EquipmentStatus::Active);
    auto inactive_result = service_->findEquipmentByStatus(EquipmentStatus::Inactive);
    auto maintenance_result = service_->findEquipmentByStatus(EquipmentStatus::Maintenance);
    
    // Verify results
    EXPECT_EQ(active_result.size(), 2);
    EXPECT_EQ(inactive_result.size(), 1);
    EXPECT_EQ(maintenance_result.size(), 1);
    
    // Check specific equipment
    auto hasEquipment = [](const std::vector<Equipment>& list, const Equipment& equipment) {
        return std::find_if(list.begin(), list.end(), 
                           [&equipment](const Equipment& e) { 
                               return e.getId() == equipment.getId(); 
                           }) != list.end();
    };
    
    EXPECT_TRUE(hasEquipment(active_result, active1));
    EXPECT_TRUE(hasEquipment(active_result, active2));
    EXPECT_TRUE(hasEquipment(inactive_result, inactive));
    EXPECT_TRUE(hasEquipment(maintenance_result, maintenance));
}

// Test findActiveEquipment method
TEST_F(EquipmentTrackerServiceTest, FindActiveEquipmentReturnsOnlyActiveEquipment) {
    // Add equipment with different statuses
    Equipment active1 = createTestEquipment("001", EquipmentStatus::Active);
    Equipment active2 = createTestEquipment("002", EquipmentStatus::Active);
    Equipment inactive = createTestEquipment("003", EquipmentStatus::Inactive);
    
    EXPECT_CALL(*mock_data_storage_ptr_, saveEquipment(testing::_))
        .WillRepeatedly(testing::Return(true));
    
    service_->addEquipment(active1);
    service_->addEquipment(active2);
    service_->addEquipment(inactive);
    
    // Call method under test
    auto result = service_->findActiveEquipment();
    
    // Verify result
    EXPECT_EQ(result.size(), 2);
    
    // Check if only active equipment are in the result
    auto hasEquipment = [&result](const Equipment& equipment) {
        return std::find_if(result.begin(), result.end(), 
                           [&equipment](const Equipment& e) { 
                               return e.getId() == equipment.getId(); 
                           }) != result.end();
    };
    
    EXPECT_TRUE(hasEquipment(active1));
    EXPECT_TRUE(hasEquipment(active2));
    EXPECT_FALSE(hasEquipment(inactive));
}

// Test findEquipmentInArea method
TEST_F(EquipmentTrackerServiceTest, FindEquipmentInAreaReturnsEquipmentInBounds) {
    // Create equipment with positions
    Equipment equipment1 = createTestEquipment("001");
    Equipment equipment2 = createTestEquipment("002");
    Equipment equipment3 = createTestEquipment("003");
    
    // Add positions
    Position pos1(10.0, 20.0, 100.0, 5.0, 123456789);
    Position pos2(15.0, 25.0, 200.0, 5.0, 123456789);
    Position pos3(30.0, 40.0, 300.0, 5.0, 123456789);
    
    equipment1.recordPosition(pos1);
    equipment2.recordPosition(pos2);
    equipment3.recordPosition(pos3);
    
    EXPECT_CALL(*mock_data_storage_ptr_, saveEquipment(testing::_))
        .WillRepeatedly(testing::Return(true));
    
    service_->addEquipment(equipment1);
    service_->addEquipment(equipment2);
    service_->addEquipment(equipment3);
    
    // Call method under test - search area that includes equipment1 and equipment2
    auto result = service_->findEquipmentInArea(5.0, 15.0, 20.0, 30.0);
    
    // Verify result
    EXPECT_EQ(result.size(), 2);
    
    // Check if correct equipment are in the result
    auto hasEquipment = [&result](const Equipment& equipment) {
        return std::find_if(result.begin(), result.end(), 
                           [&equipment](const Equipment& e) { 
                               return e.getId() == equipment.getId(); 
                           }) != result.end();
    };
    
    EXPECT_TRUE(hasEquipment(equipment1));
    EXPECT_TRUE(hasEquipment(equipment2));
    EXPECT_FALSE(hasEquipment(equipment3));
}

// Test setGeofence method
TEST_F(EquipmentTrackerServiceTest, SetGeofenceReturnsTrue) {
    // Call method under test
    bool result = service_->setGeofence("TEST-001", 10.0, 20.0, 30.0, 40.0);
    
    // Verify result - this is just a placeholder in the implementation
    EXPECT_TRUE(result);
}

// Test handlePositionUpdate method
TEST_F(EquipmentTrackerServiceTest, HandlePositionUpdateUpdatesEquipmentPosition) {
    // Add test equipment
    Equipment test_equipment = createTestEquipment();
    
    EXPECT_CALL(*mock_data_storage_ptr_, saveEquipment(testing::_))
        .WillOnce(testing::Return(true));
    
    service_->addEquipment(test_equipment);
    
    // Setup expectations for position update
    EXPECT_CALL(*mock_data_storage_ptr_, savePosition(testing::_, testing::_))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mock_data_storage_ptr_, updateEquipment(testing::_))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mock_network_manager_ptr_, sendPositionUpdate(testing::_, testing::_));
    
    // Call method under test
    double lat = 10.0, lon = 20.0, alt = 100.0;
    Timestamp timestamp = 123456789;
    
    // We need to access the private method, so we'll use the callback that was registered
    // This requires modifying the service to expose the callback for testing
    service_->testHandlePositionUpdate(lat, lon, alt, timestamp);
    
    // Verify equipment position was updated
    auto updated_equipment = service_->getEquipment(test_equipment.getId());
    EXPECT_TRUE(updated_equipment.has_value());
    
    auto position = updated_equipment->getLastPosition();
    EXPECT_TRUE(position.has_value());
    EXPECT_NEAR(position->getLatitude(), lat, 0.0001);
    EXPECT_NEAR(position->getLongitude(), lon, 0.0001);
    EXPECT_NEAR(position->getAltitude(), alt, 0.0001);
    EXPECT_EQ(position->getTimestamp(), timestamp);
    
    // Verify status was updated to Active
    EXPECT_EQ(updated_equipment->getStatus(), EquipmentStatus::Active);
}

// Test handleRemoteCommand method
TEST_F(EquipmentTrackerServiceTest, HandleRemoteCommandProcessesStatusRequest) {
    // Add test equipment
    Equipment test_equipment = createTestEquipment();
    
    EXPECT_CALL(*mock_data_storage_ptr_, saveEquipment(testing::_))
        .WillOnce(testing::Return(true));
    
    service_->addEquipment(test_equipment);
    
    // Call method under test
    service_->testHandleRemoteCommand("STATUS_REQUEST");
    
    // This is mostly a placeholder test since the method just logs information
    // In a real implementation, we would verify that appropriate status data was sent
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>