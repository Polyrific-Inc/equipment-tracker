// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <vector>
#include "utils/types.h"
#include "equipment.h"
#include "gps_tracker.h"
#include "data_storage.h"
#include "network_manager.h"
#include "equipment_tracker_service.h" // Assuming this is the filename for the provided code

namespace equipment_tracker {

// Mock classes for dependencies
class MockGPSTracker : public GPSTracker {
public:
    MOCK_METHOD(void, start, (), (override));
    MOCK_METHOD(void, stop, (), (override));
    MOCK_METHOD(bool, isRunning, (), (const, override));
    // Add other methods as needed
};

class MockDataStorage : public DataStorage {
public:
    MOCK_METHOD(bool, saveEquipment, (const Equipment&), (override));
    MOCK_METHOD(bool, deleteEquipment, (const EquipmentId&), (override));
    MOCK_METHOD(std::optional<Equipment>, loadEquipment, (const EquipmentId&), (const, override));
    MOCK_METHOD(std::vector<Equipment>, loadAllEquipment, (), (const, override));
    // Add other methods as needed
};

class MockNetworkManager : public NetworkManager {
public:
    MOCK_METHOD(void, start, (), (override));
    MOCK_METHOD(void, stop, (), (override));
    MOCK_METHOD(bool, isRunning, (), (const, override));
    // Add other methods as needed
};

// Test fixture
class EquipmentTrackerServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create mock objects
        mock_gps_tracker_ = new MockGPSTracker();
        mock_data_storage_ = new MockDataStorage();
        mock_network_manager_ = new MockNetworkManager();
        
        // Create test equipment
        test_equipment_.id = "EQ123";
        test_equipment_.name = "Test Equipment";
        test_equipment_.type = EquipmentType::HEAVY_MACHINERY;
        test_equipment_.status = EquipmentStatus::ACTIVE;
    }

    void TearDown() override {
        // Cleanup
    }

    // Helper method to create a service with mocked dependencies
    std::unique_ptr<EquipmentTrackerService> createServiceWithMocks() {
        auto service = std::make_unique<EquipmentTrackerService>();
        
        // Replace the real components with mocks
        service->getGPSTracker() = *mock_gps_tracker_;
        service->getDataStorage() = *mock_data_storage_;
        service->getNetworkManager() = *mock_network_manager_;
        
        return service;
    }

    MockGPSTracker* mock_gps_tracker_;
    MockDataStorage* mock_data_storage_;
    MockNetworkManager* mock_network_manager_;
    Equipment test_equipment_;
};

// Test constructor and destructor
TEST_F(EquipmentTrackerServiceTest, ConstructorDestructor) {
    EquipmentTrackerService service;
    EXPECT_FALSE(service.isRunning());
}

// Test start and stop methods
TEST_F(EquipmentTrackerServiceTest, StartStop) {
    auto service = createServiceWithMocks();
    
    // Set expectations for start
    EXPECT_CALL(*mock_gps_tracker_, start()).Times(1);
    EXPECT_CALL(*mock_network_manager_, start()).Times(1);
    
    // Test start
    service->start();
    EXPECT_TRUE(service->isRunning());
    
    // Set expectations for stop
    EXPECT_CALL(*mock_gps_tracker_, stop()).Times(1);
    EXPECT_CALL(*mock_network_manager_, stop()).Times(1);
    
    // Test stop
    service->stop();
    EXPECT_FALSE(service->isRunning());
}

// Test equipment management
TEST_F(EquipmentTrackerServiceTest, AddEquipment) {
    auto service = createServiceWithMocks();
    
    // Set expectations
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    
    // Test adding equipment
    EXPECT_TRUE(service->addEquipment(test_equipment_));
    
    // Verify equipment was added
    auto result = service->getEquipment(test_equipment_.id);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->id, test_equipment_.id);
    EXPECT_EQ(result->name, test_equipment_.name);
    EXPECT_EQ(result->type, test_equipment_.type);
    EXPECT_EQ(result->status, test_equipment_.status);
}

TEST_F(EquipmentTrackerServiceTest, RemoveEquipment) {
    auto service = createServiceWithMocks();
    
    // Add equipment first
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    EXPECT_TRUE(service->addEquipment(test_equipment_));
    
    // Set expectations for removal
    EXPECT_CALL(*mock_data_storage_, deleteEquipment(test_equipment_.id))
        .WillOnce(::testing::Return(true));
    
    // Test removing equipment
    EXPECT_TRUE(service->removeEquipment(test_equipment_.id));
    
    // Verify equipment was removed
    auto result = service->getEquipment(test_equipment_.id);
    EXPECT_FALSE(result.has_value());
}

TEST_F(EquipmentTrackerServiceTest, GetEquipment) {
    auto service = createServiceWithMocks();
    
    // Add equipment
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    service->addEquipment(test_equipment_);
    
    // Test getting equipment
    auto result = service->getEquipment(test_equipment_.id);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->id, test_equipment_.id);
    
    // Test getting non-existent equipment
    auto not_found = service->getEquipment("NONEXISTENT");
    EXPECT_FALSE(not_found.has_value());
}

TEST_F(EquipmentTrackerServiceTest, GetAllEquipment) {
    auto service = createServiceWithMocks();
    
    // Add multiple equipment
    Equipment eq1 = test_equipment_;
    Equipment eq2 = test_equipment_;
    eq2.id = "EQ456";
    eq2.name = "Another Equipment";
    
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service->addEquipment(eq1);
    service->addEquipment(eq2);
    
    // Test getting all equipment
    auto all_equipment = service->getAllEquipment();
    EXPECT_EQ(all_equipment.size(), 2);
    
    // Verify equipment is in the list
    bool found_eq1 = false, found_eq2 = false;
    for (const auto& eq : all_equipment) {
        if (eq.id == eq1.id) found_eq1 = true;
        if (eq.id == eq2.id) found_eq2 = true;
    }
    EXPECT_TRUE(found_eq1);
    EXPECT_TRUE(found_eq2);
}

// Test equipment queries
TEST_F(EquipmentTrackerServiceTest, FindEquipmentByStatus) {
    auto service = createServiceWithMocks();
    
    // Add equipment with different statuses
    Equipment active_eq = test_equipment_;
    active_eq.status = EquipmentStatus::ACTIVE;
    
    Equipment inactive_eq = test_equipment_;
    inactive_eq.id = "EQ456";
    inactive_eq.status = EquipmentStatus::INACTIVE;
    
    Equipment maintenance_eq = test_equipment_;
    maintenance_eq.id = "EQ789";
    maintenance_eq.status = EquipmentStatus::MAINTENANCE;
    
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service->addEquipment(active_eq);
    service->addEquipment(inactive_eq);
    service->addEquipment(maintenance_eq);
    
    // Test finding by status
    auto active_equipment = service->findEquipmentByStatus(EquipmentStatus::ACTIVE);
    EXPECT_EQ(active_equipment.size(), 1);
    EXPECT_EQ(active_equipment[0].id, active_eq.id);
    
    auto inactive_equipment = service->findEquipmentByStatus(EquipmentStatus::INACTIVE);
    EXPECT_EQ(inactive_equipment.size(), 1);
    EXPECT_EQ(inactive_equipment[0].id, inactive_eq.id);
    
    auto maintenance_equipment = service->findEquipmentByStatus(EquipmentStatus::MAINTENANCE);
    EXPECT_EQ(maintenance_equipment.size(), 1);
    EXPECT_EQ(maintenance_equipment[0].id, maintenance_eq.id);
}

TEST_F(EquipmentTrackerServiceTest, FindActiveEquipment) {
    auto service = createServiceWithMocks();
    
    // Add equipment with different statuses
    Equipment active_eq1 = test_equipment_;
    active_eq1.status = EquipmentStatus::ACTIVE;
    
    Equipment active_eq2 = test_equipment_;
    active_eq2.id = "EQ456";
    active_eq2.status = EquipmentStatus::ACTIVE;
    
    Equipment inactive_eq = test_equipment_;
    inactive_eq.id = "EQ789";
    inactive_eq.status = EquipmentStatus::INACTIVE;
    
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service->addEquipment(active_eq1);
    service->addEquipment(active_eq2);
    service->addEquipment(inactive_eq);
    
    // Test finding active equipment
    auto active_equipment = service->findActiveEquipment();
    EXPECT_EQ(active_equipment.size(), 2);
    
    // Verify both active equipment are in the list
    bool found_eq1 = false, found_eq2 = false;
    for (const auto& eq : active_equipment) {
        if (eq.id == active_eq1.id) found_eq1 = true;
        if (eq.id == active_eq2.id) found_eq2 = true;
    }
    EXPECT_TRUE(found_eq1);
    EXPECT_TRUE(found_eq2);
}

TEST_F(EquipmentTrackerServiceTest, FindEquipmentInArea) {
    auto service = createServiceWithMocks();
    
    // Add equipment with different locations
    Equipment eq1 = test_equipment_;
    eq1.latitude = 10.0;
    eq1.longitude = 10.0;
    
    Equipment eq2 = test_equipment_;
    eq2.id = "EQ456";
    eq2.latitude = 20.0;
    eq2.longitude = 20.0;
    
    Equipment eq3 = test_equipment_;
    eq3.id = "EQ789";
    eq3.latitude = 30.0;
    eq3.longitude = 30.0;
    
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service->addEquipment(eq1);
    service->addEquipment(eq2);
    service->addEquipment(eq3);
    
    // Test finding equipment in area
    auto equipment_in_area = service->findEquipmentInArea(5.0, 5.0, 25.0, 25.0);
    EXPECT_EQ(equipment_in_area.size(), 2);
    
    // Verify correct equipment are in the list
    bool found_eq1 = false, found_eq2 = false;
    for (const auto& eq : equipment_in_area) {
        if (eq.id == eq1.id) found_eq1 = true;
        if (eq.id == eq2.id) found_eq2 = true;
    }
    EXPECT_TRUE(found_eq1);
    EXPECT_TRUE(found_eq2);
}

// Test advanced features
TEST_F(EquipmentTrackerServiceTest, SetGeofence) {
    auto service = createServiceWithMocks();
    
    // Add equipment
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service->addEquipment(test_equipment_);
    
    // Test setting geofence for existing equipment
    EXPECT_TRUE(service->setGeofence(test_equipment_.id, 10.0, 10.0, 20.0, 20.0));
    
    // Test setting geofence for non-existent equipment
    EXPECT_FALSE(service->setGeofence("NONEXISTENT", 10.0, 10.0, 20.0, 20.0));
}

// Test component access
TEST_F(EquipmentTrackerServiceTest, ComponentAccess) {
    EquipmentTrackerService service;
    
    // Test accessing components
    EXPECT_NO_THROW(service.getGPSTracker());
    EXPECT_NO_THROW(service.getDataStorage());
    EXPECT_NO_THROW(service.getNetworkManager());
}

} // namespace equipment_tracker
// </test_code>