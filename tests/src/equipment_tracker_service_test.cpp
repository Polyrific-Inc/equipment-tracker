// <test_code>
#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "equipment_tracker/equipment_tracker_service.h"

// Since the original classes aren't designed for mocking, we'll use integration testing approach
class TestableEquipmentTrackerService : public equipment_tracker::EquipmentTrackerService
{
public:
    TestableEquipmentTrackerService() : equipment_tracker::EquipmentTrackerService() {}

    // Expose internal state for testing if needed
    bool isInternalRunning() const { return isRunning(); }
};

// Test fixture for EquipmentTrackerService
class EquipmentTrackerServiceTest : public ::testing::Test
{
protected:
    std::unique_ptr<TestableEquipmentTrackerService> service;

    void SetUp() override
    {
        service = std::make_unique<TestableEquipmentTrackerService>();
    }

    void TearDown() override
    {
        if (service && service->isRunning())
        {
            service->stop();
        }
    }

    // Helper method to create a test equipment
    equipment_tracker::Equipment createTestEquipment(const std::string &id = "TEST-001")
    {
        return equipment_tracker::Equipment(
            id,
            equipment_tracker::EquipmentType::Forklift,
            "Test Forklift");
    }
};

// Test starting the service
TEST_F(EquipmentTrackerServiceTest, StartServiceSuccess)
{
    // Call the method under test
    service->start();

    // Brief wait to allow initialization
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify the service is running
    EXPECT_TRUE(service->isRunning());
}

// Test stopping the service
TEST_F(EquipmentTrackerServiceTest, StopService)
{
    // First start the service
    service->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(service->isRunning());

    // Call the method under test
    service->stop();

    // Verify the service is not running
    EXPECT_FALSE(service->isRunning());
}

// Test adding equipment
TEST_F(EquipmentTrackerServiceTest, AddEquipmentSuccess)
{
    auto equipment = createTestEquipment();

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
TEST_F(EquipmentTrackerServiceTest, AddDuplicateEquipmentFails)
{
    auto equipment = createTestEquipment();

    // Add the equipment first
    bool firstAdd = service->addEquipment(equipment);
    EXPECT_TRUE(firstAdd);

    // Try to add it again
    bool result = service->addEquipment(equipment);

    // Verify the result - should fail for duplicate
    EXPECT_FALSE(result);
}

// Test removing equipment
TEST_F(EquipmentTrackerServiceTest, RemoveEquipmentSuccess)
{
    auto equipment = createTestEquipment();

    // Add the equipment first
    bool addResult = service->addEquipment(equipment);
    EXPECT_TRUE(addResult);

    // Call the method under test
    bool result = service->removeEquipment(equipment.getId());

    // Verify the result
    EXPECT_TRUE(result);

    // Verify the equipment was removed
    auto retrievedEquipment = service->getEquipment(equipment.getId());
    EXPECT_FALSE(retrievedEquipment.has_value());
}

// Test removing non-existent equipment
TEST_F(EquipmentTrackerServiceTest, RemoveNonExistentEquipmentFails)
{
    // Call the method under test with a non-existent ID
    bool result = service->removeEquipment("NONEXISTENT-001");

    // Verify the result
    EXPECT_FALSE(result);
}

// Test getting all equipment
TEST_F(EquipmentTrackerServiceTest, GetAllEquipment)
{
    // Add some equipment
    auto equipment1 = createTestEquipment("TEST-001");
    auto equipment2 = createTestEquipment("TEST-002");

    service->addEquipment(equipment1);
    service->addEquipment(equipment2);

    // Call the method under test
    auto allEquipment = service->getAllEquipment();

    // Verify the result
    EXPECT_EQ(allEquipment.size(), 2);

    // Check if both equipment items are in the result
    bool found1 = false, found2 = false;
    for (const auto &equipment : allEquipment)
    {
        if (equipment.getId() == "TEST-001")
            found1 = true;
        if (equipment.getId() == "TEST-002")
            found2 = true;
    }

    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

// Test finding equipment by status
TEST_F(EquipmentTrackerServiceTest, FindEquipmentByStatus)
{
    // Add equipment with different statuses
    auto equipment1 = createTestEquipment("TEST-001");
    equipment1.setStatus(equipment_tracker::EquipmentStatus::Active);

    auto equipment2 = createTestEquipment("TEST-002");
    equipment2.setStatus(equipment_tracker::EquipmentStatus::Maintenance);

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
TEST_F(EquipmentTrackerServiceTest, FindActiveEquipment)
{
    // Add equipment with different statuses
    auto equipment1 = createTestEquipment("TEST-001");
    equipment1.setStatus(equipment_tracker::EquipmentStatus::Active);

    auto equipment2 = createTestEquipment("TEST-002");
    equipment2.setStatus(equipment_tracker::EquipmentStatus::Inactive);

    service->addEquipment(equipment1);
    service->addEquipment(equipment2);

    // Call the method under test
    auto activeEquipment = service->findActiveEquipment();

    // Verify the result
    EXPECT_EQ(activeEquipment.size(), 1);
    EXPECT_EQ(activeEquipment[0].getId(), "TEST-001");
}

// Test finding equipment in area
TEST_F(EquipmentTrackerServiceTest, FindEquipmentInArea)
{
    // Create equipment with positions
    auto equipment1 = createTestEquipment("TEST-001");
    equipment_tracker::Position pos1(37.7749, -122.4194); // San Francisco
    equipment1.setLastPosition(pos1);

    auto equipment2 = createTestEquipment("TEST-002");
    equipment_tracker::Position pos2(34.0522, -118.2437); // Los Angeles
    equipment2.setLastPosition(pos2);

    auto equipment3 = createTestEquipment("TEST-003");
    // No position set for equipment3

    service->addEquipment(equipment1);
    service->addEquipment(equipment2);
    service->addEquipment(equipment3);

    // Call the method under test - search area covering San Francisco
    auto equipmentInArea = service->findEquipmentInArea(
        37.7, -123.0, // Southwest corner
        38.0, -122.0  // Northeast corner
    );

    // Verify the result
    EXPECT_EQ(equipmentInArea.size(), 1);
    EXPECT_EQ(equipmentInArea[0].getId(), "TEST-001");

    // Search area covering both San Francisco and Los Angeles
    auto largerAreaEquipment = service->findEquipmentInArea(
        34.0, -123.0, // Southwest corner
        38.0, -118.0  // Northeast corner
    );

    // Verify the result
    EXPECT_EQ(largerAreaEquipment.size(), 2);
}

// Test setting geofence
TEST_F(EquipmentTrackerServiceTest, SetGeofence)
{
    auto equipment = createTestEquipment();

    // Add the equipment first
    bool addResult = service->addEquipment(equipment);
    EXPECT_TRUE(addResult);

    // Call the method under test
    bool result = service->setGeofence(
        equipment.getId(),
        37.7, -122.5, // Southwest corner
        37.8, -122.4  // Northeast corner
    );

    // Verify the result
    EXPECT_TRUE(result);
}

// Test getting non-existent equipment
TEST_F(EquipmentTrackerServiceTest, GetNonExistentEquipment)
{
    // Call the method under test with a non-existent ID
    auto result = service->getEquipment("NONEXISTENT-001");

    // Verify the result
    EXPECT_FALSE(result.has_value());
}

// Test service state consistency
TEST_F(EquipmentTrackerServiceTest, ServiceStateConsistency)
{
    // Initially not running
    EXPECT_FALSE(service->isRunning());

    // Start service
    service->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(service->isRunning());

    // Stop service
    service->stop();
    EXPECT_FALSE(service->isRunning());

    // Can start again
    service->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(service->isRunning());
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>