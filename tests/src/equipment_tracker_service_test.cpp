// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <map>
#include "equipment_tracker/equipment_tracker_service.h"

namespace equipment_tracker {

// Mock classes for dependencies
class MockGPSTracker : public GPSTracker {
public:
    MOCK_METHOD(void, start, (), (override));
    MOCK_METHOD(void, stop, (), (override));
    MOCK_METHOD(void, registerPositionCallback, (std::function<void(double, double, double, Timestamp)>), (override));
};

class MockDataStorage : public DataStorage {
public:
    MOCK_METHOD(bool, initialize, (), (override));
    MOCK_METHOD(bool, saveEquipment, (const Equipment&), (override));
    MOCK_METHOD(bool, updateEquipment, (const Equipment&), (override));
    MOCK_METHOD(bool, deleteEquipment, (const EquipmentId&), (override));
    MOCK_METHOD(std::vector<Equipment>, getAllEquipment, (), (const, override));
    MOCK_METHOD(bool, savePosition, (const EquipmentId&, const Position&), (override));
};

class MockNetworkManager : public NetworkManager {
public:
    MOCK_METHOD(void, connect, (), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(void, registerCommandHandler, (std::function<void(const std::string&)>), (override));
    MOCK_METHOD(void, sendPositionUpdate, (const EquipmentId&, const Position&), (override));
};

// Test fixture
class EquipmentTrackerServiceTest : public ::testing::Test {
protected:
    std::unique_ptr<MockGPSTracker> mock_gps_tracker_;
    std::unique_ptr<MockDataStorage> mock_data_storage_;
    std::unique_ptr<MockNetworkManager> mock_network_manager_;
    std::unique_ptr<EquipmentTrackerService> service_;
    
    // Callback storage
    std::function<void(double, double, double, Timestamp)> position_callback_;
    std::function<void(const std::string&)> command_handler_;

    void SetUp() override {
        mock_gps_tracker_ = std::make_unique<MockGPSTracker>();
        mock_data_storage_ = std::make_unique<MockDataStorage>();
        mock_network_manager_ = std::make_unique<MockNetworkManager>();
        
        // Set up expectations for constructor
        EXPECT_CALL(*mock_gps_tracker_, registerPositionCallback(::testing::_))
            .WillOnce(::testing::SaveArg<0>(&position_callback_));
        EXPECT_CALL(*mock_network_manager_, registerCommandHandler(::testing::_))
            .WillOnce(::testing::SaveArg<0>(&command_handler_));
        
        // Create service with mocked dependencies
        service_ = std::make_unique<EquipmentTrackerService>(
            std::move(mock_gps_tracker_),
            std::move(mock_data_storage_),
            std::move(mock_network_manager_)
        );
        
        // Reset mocks after constructor
        mock_gps_tracker_ = service_->getGPSTrackerForTesting();
        mock_data_storage_ = service_->getDataStorageForTesting();
        mock_network_manager_ = service_->getNetworkManagerForTesting();
    }
    
    // Helper to create a sample equipment
    Equipment createSampleEquipment(const std::string& id = "TEST-001") {
        Equipment equipment(id, "Test Equipment", EquipmentType::Forklift);
        equipment.setStatus(EquipmentStatus::Idle);
        return equipment;
    }
};

TEST_F(EquipmentTrackerServiceTest, StartInitializesAndStartsComponents) {
    // Setup expectations
    EXPECT_CALL(*mock_data_storage_, initialize())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock_data_storage_, getAllEquipment())
        .WillOnce(::testing::Return(std::vector<Equipment>{}));
    EXPECT_CALL(*mock_network_manager_, connect());
    EXPECT_CALL(*mock_gps_tracker_, start());
    
    // Call method under test
    service_->start();
    
    // Verify service is running
    EXPECT_TRUE(service_->isRunning());
}

TEST_F(EquipmentTrackerServiceTest, StartFailsWhenStorageInitializationFails) {
    // Setup expectations
    EXPECT_CALL(*mock_data_storage_, initialize())
        .WillOnce(::testing::Return(false));
    
    // These should not be called
    EXPECT_CALL(*mock_network_manager_, connect()).Times(0);
    EXPECT_CALL(*mock_gps_tracker_, start()).Times(0);
    
    // Call method under test
    service_->start();
    
    // Verify service is not running
    EXPECT_FALSE(service_->isRunning());
}

TEST_F(EquipmentTrackerServiceTest, StartDoesNothingWhenAlreadyRunning) {
    // First start the service
    EXPECT_CALL(*mock_data_storage_, initialize())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock_data_storage_, getAllEquipment())
        .WillOnce(::testing::Return(std::vector<Equipment>{}));
    EXPECT_CALL(*mock_network_manager_, connect());
    EXPECT_CALL(*mock_gps_tracker_, start());
    
    service_->start();
    EXPECT_TRUE(service_->isRunning());
    
    // Try to start again - should do nothing
    service_->start();
}

TEST_F(EquipmentTrackerServiceTest, StopStopsComponentsWhenRunning) {
    // First start the service
    EXPECT_CALL(*mock_data_storage_, initialize())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock_data_storage_, getAllEquipment())
        .WillOnce(::testing::Return(std::vector<Equipment>{}));
    EXPECT_CALL(*mock_network_manager_, connect());
    EXPECT_CALL(*mock_gps_tracker_, start());
    
    service_->start();
    
    // Setup expectations for stop
    EXPECT_CALL(*mock_gps_tracker_, stop());
    EXPECT_CALL(*mock_network_manager_, disconnect());
    
    // Call method under test
    service_->stop();
    
    // Verify service is not running
    EXPECT_FALSE(service_->isRunning());
}

TEST_F(EquipmentTrackerServiceTest, StopDoesNothingWhenNotRunning) {
    // Service starts not running
    EXPECT_FALSE(service_->isRunning());
    
    // These should not be called
    EXPECT_CALL(*mock_gps_tracker_, stop()).Times(0);
    EXPECT_CALL(*mock_network_manager_, disconnect()).Times(0);
    
    // Call method under test
    service_->stop();
    
    // Still not running
    EXPECT_FALSE(service_->isRunning());
}

TEST_F(EquipmentTrackerServiceTest, AddEquipmentSucceeds) {
    Equipment equipment = createSampleEquipment();
    
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    
    bool result = service_->addEquipment(equipment);
    
    EXPECT_TRUE(result);
    
    // Verify equipment was added
    auto retrieved = service_->getEquipment(equipment.getId());
    EXPECT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->getId(), equipment.getId());
}

TEST_F(EquipmentTrackerServiceTest, AddEquipmentFailsForDuplicate) {
    Equipment equipment = createSampleEquipment();
    
    // First add succeeds
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    
    bool result1 = service_->addEquipment(equipment);
    EXPECT_TRUE(result1);
    
    // Second add with same ID fails
    bool result2 = service_->addEquipment(equipment);
    EXPECT_FALSE(result2);
}

TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentSucceeds) {
    Equipment equipment = createSampleEquipment();
    
    // First add the equipment
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    
    service_->addEquipment(equipment);
    
    // Then remove it
    EXPECT_CALL(*mock_data_storage_, deleteEquipment(equipment.getId()))
        .WillOnce(::testing::Return(true));
    
    bool result = service_->removeEquipment(equipment.getId());
    
    EXPECT_TRUE(result);
    
    // Verify equipment was removed
    auto retrieved = service_->getEquipment(equipment.getId());
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentFailsForNonexistent) {
    // Try to remove non-existent equipment
    EXPECT_CALL(*mock_data_storage_, deleteEquipment(::testing::_)).Times(0);
    
    bool result = service_->removeEquipment("NONEXISTENT-ID");
    
    EXPECT_FALSE(result);
}

TEST_F(EquipmentTrackerServiceTest, GetEquipmentReturnsCorrectEquipment) {
    Equipment equipment1 = createSampleEquipment("TEST-001");
    Equipment equipment2 = createSampleEquipment("TEST-002");
    
    // Add equipment
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service_->addEquipment(equipment1);
    service_->addEquipment(equipment2);
    
    // Get equipment
    auto retrieved1 = service_->getEquipment("TEST-001");
    auto retrieved2 = service_->getEquipment("TEST-002");
    auto retrieved3 = service_->getEquipment("NONEXISTENT-ID");
    
    EXPECT_TRUE(retrieved1.has_value());
    EXPECT_EQ(retrieved1->getId(), "TEST-001");
    
    EXPECT_TRUE(retrieved2.has_value());
    EXPECT_EQ(retrieved2->getId(), "TEST-002");
    
    EXPECT_FALSE(retrieved3.has_value());
}

TEST_F(EquipmentTrackerServiceTest, GetAllEquipmentReturnsAllEquipment) {
    Equipment equipment1 = createSampleEquipment("TEST-001");
    Equipment equipment2 = createSampleEquipment("TEST-002");
    
    // Add equipment
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service_->addEquipment(equipment1);
    service_->addEquipment(equipment2);
    
    // Get all equipment
    auto all_equipment = service_->getAllEquipment();
    
    EXPECT_EQ(all_equipment.size(), 2);
    
    // Check if both equipment are in the result
    bool found1 = false, found2 = false;
    for (const auto& equipment : all_equipment) {
        if (equipment.getId() == "TEST-001") found1 = true;
        if (equipment.getId() == "TEST-002") found2 = true;
    }
    
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

TEST_F(EquipmentTrackerServiceTest, FindEquipmentByStatusReturnsCorrectEquipment) {
    Equipment equipment1 = createSampleEquipment("TEST-001");
    equipment1.setStatus(EquipmentStatus::Active);
    
    Equipment equipment2 = createSampleEquipment("TEST-002");
    equipment2.setStatus(EquipmentStatus::Idle);
    
    Equipment equipment3 = createSampleEquipment("TEST-003");
    equipment3.setStatus(EquipmentStatus::Active);
    
    // Add equipment
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service_->addEquipment(equipment1);
    service_->addEquipment(equipment2);
    service_->addEquipment(equipment3);
    
    // Find active equipment
    auto active_equipment = service_->findEquipmentByStatus(EquipmentStatus::Active);
    
    EXPECT_EQ(active_equipment.size(), 2);
    
    // Check if both active equipment are in the result
    bool found1 = false, found3 = false;
    for (const auto& equipment : active_equipment) {
        if (equipment.getId() == "TEST-001") found1 = true;
        if (equipment.getId() == "TEST-003") found3 = true;
    }
    
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found3);
    
    // Find idle equipment
    auto idle_equipment = service_->findEquipmentByStatus(EquipmentStatus::Idle);
    
    EXPECT_EQ(idle_equipment.size(), 1);
    EXPECT_EQ(idle_equipment[0].getId(), "TEST-002");
}

TEST_F(EquipmentTrackerServiceTest, FindActiveEquipmentReturnsActiveEquipment) {
    Equipment equipment1 = createSampleEquipment("TEST-001");
    equipment1.setStatus(EquipmentStatus::Active);
    
    Equipment equipment2 = createSampleEquipment("TEST-002");
    equipment2.setStatus(EquipmentStatus::Idle);
    
    // Add equipment
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service_->addEquipment(equipment1);
    service_->addEquipment(equipment2);
    
    // Find active equipment
    auto active_equipment = service_->findActiveEquipment();
    
    EXPECT_EQ(active_equipment.size(), 1);
    EXPECT_EQ(active_equipment[0].getId(), "TEST-001");
}

TEST_F(EquipmentTrackerServiceTest, FindEquipmentInAreaReturnsEquipmentInArea) {
    Equipment equipment1 = createSampleEquipment("TEST-001");
    Position pos1(10.0, 20.0, 100.0, 5.0, 12345);
    equipment1.recordPosition(pos1);
    
    Equipment equipment2 = createSampleEquipment("TEST-002");
    Position pos2(30.0, 40.0, 200.0, 5.0, 12345);
    equipment2.recordPosition(pos2);
    
    Equipment equipment3 = createSampleEquipment("TEST-003");
    // No position recorded
    
    // Add equipment
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service_->addEquipment(equipment1);
    service_->addEquipment(equipment2);
    service_->addEquipment(equipment3);
    
    // Find equipment in area that includes equipment1
    auto result1 = service_->findEquipmentInArea(5.0, 15.0, 15.0, 25.0);
    
    EXPECT_EQ(result1.size(), 1);
    EXPECT_EQ(result1[0].getId(), "TEST-001");
    
    // Find equipment in area that includes equipment2
    auto result2 = service_->findEquipmentInArea(25.0, 35.0, 35.0, 45.0);
    
    EXPECT_EQ(result2.size(), 1);
    EXPECT_EQ(result2[0].getId(), "TEST-002");
    
    // Find equipment in area that includes both
    auto result3 = service_->findEquipmentInArea(5.0, 15.0, 35.0, 45.0);
    
    EXPECT_EQ(result3.size(), 2);
    
    // Find equipment in area that includes none
    auto result4 = service_->findEquipmentInArea(50.0, 60.0, 60.0, 70.0);
    
    EXPECT_EQ(result4.size(), 0);
}

TEST_F(EquipmentTrackerServiceTest, SetGeofenceReturnsTrue) {
    bool result = service_->setGeofence("TEST-001", 10.0, 20.0, 30.0, 40.0);
    EXPECT_TRUE(result);
}

TEST_F(EquipmentTrackerServiceTest, LoadEquipmentLoadsFromStorage) {
    // Create sample equipment list
    std::vector<Equipment> equipment_list;
    equipment_list.push_back(createSampleEquipment("TEST-001"));
    equipment_list.push_back(createSampleEquipment("TEST-002"));
    
    // Setup expectations
    EXPECT_CALL(*mock_data_storage_, getAllEquipment())
        .WillOnce(::testing::Return(equipment_list));
    
    // Call method under test
    service_->loadEquipment();
    
    // Verify equipment was loaded
    auto all_equipment = service_->getAllEquipment();
    EXPECT_EQ(all_equipment.size(), 2);
    
    // Check if both equipment are in the result
    bool found1 = false, found2 = false;
    for (const auto& equipment : all_equipment) {
        if (equipment.getId() == "TEST-001") found1 = true;
        if (equipment.getId() == "TEST-002") found2 = true;
    }
    
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

TEST_F(EquipmentTrackerServiceTest, HandlePositionUpdateUpdatesEquipment) {
    // Add equipment
    Equipment equipment = createSampleEquipment("FORKLIFT-001");
    
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    
    service_->addEquipment(equipment);
    
    // Setup expectations for position update
    EXPECT_CALL(*mock_data_storage_, savePosition(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock_data_storage_, updateEquipment(::testing::_))
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock_network_manager_, sendPositionUpdate(::testing::_, ::testing::_));
    
    // Call position callback
    position_callback_(10.0, 20.0, 100.0, 12345);
    
    // Verify equipment was updated
    auto updated_equipment = service_->getEquipment("FORKLIFT-001");
    EXPECT_TRUE(updated_equipment.has_value());
    EXPECT_EQ(updated_equipment->getStatus(), EquipmentStatus::Active);
    
    auto position = updated_equipment->getLastPosition();
    EXPECT_TRUE(position.has_value());
    EXPECT_EQ(position->getLatitude(), 10.0);
    EXPECT_EQ(position->getLongitude(), 20.0);
    EXPECT_EQ(position->getAltitude(), 100.0);
}

TEST_F(EquipmentTrackerServiceTest, HandleRemoteCommandProcessesCommand) {
    // Add equipment
    Equipment equipment1 = createSampleEquipment("TEST-001");
    Equipment equipment2 = createSampleEquipment("TEST-002");
    
    EXPECT_CALL(*mock_data_storage_, saveEquipment(::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    service_->addEquipment(equipment1);
    service_->addEquipment(equipment2);
    
    // Call command handler
    command_handler_("STATUS_REQUEST");
    
    // No specific expectations to verify, just ensure it doesn't crash
}

} // namespace equipment_tracker
// </test_code>