#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sstream>
#include <chrono>
#include <cmath>
#include <thread>
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/constants.h"

namespace equipment_tracker
{

    class EquipmentTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            // Common setup for all tests
        }
    };

    TEST_F(EquipmentTest, ConstructorInitializesCorrectly)
    {
        Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");

        EXPECT_EQ(equipment.getId(), "123");
        EXPECT_EQ(equipment.getType(), EquipmentType::Forklift);
        EXPECT_EQ(equipment.getName(), "Test Forklift");
        EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Inactive);
        EXPECT_FALSE(equipment.getLastPosition().has_value());
        EXPECT_TRUE(equipment.getPositionHistory().empty());
    }

    TEST_F(EquipmentTest, MoveConstructorWorksCorrectly)
    {
        Equipment original("123", EquipmentType::Forklift, "Test Forklift");
        original.setStatus(EquipmentStatus::Active);

        Position pos(10.0, 20.0, 0.0, 0.0);
        original.recordPosition(pos);

        Equipment moved(std::move(original));

        EXPECT_EQ(moved.getId(), "123");
        EXPECT_EQ(moved.getType(), EquipmentType::Forklift);
        EXPECT_EQ(moved.getName(), "Test Forklift");
        EXPECT_EQ(moved.getStatus(), EquipmentStatus::Active);
        EXPECT_TRUE(moved.getLastPosition().has_value());
        EXPECT_EQ(moved.getPositionHistory().size(), 1);

        // Original should be in an unknown state
        EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
    }

    TEST_F(EquipmentTest, MoveAssignmentWorksCorrectly)
    {
        Equipment original("123", EquipmentType::Forklift, "Test Forklift");
        original.setStatus(EquipmentStatus::Active);

        Position pos(10.0, 20.0, 0.0, 0.0);
        original.recordPosition(pos);

        Equipment target("456", EquipmentType::Crane, "Test Crane");
        target = std::move(original);

        EXPECT_EQ(target.getId(), "123");
        EXPECT_EQ(target.getType(), EquipmentType::Forklift);
        EXPECT_EQ(target.getName(), "Test Forklift");
        EXPECT_EQ(target.getStatus(), EquipmentStatus::Active);
        EXPECT_TRUE(target.getLastPosition().has_value());
        EXPECT_EQ(target.getPositionHistory().size(), 1);

        // Original should be in an unknown state
        EXPECT_EQ(original.getStatus(), EquipmentStatus::Unknown);
    }

    TEST_F(EquipmentTest, SelfMoveAssignmentHandledCorrectly)
    {
        Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");
        equipment.setStatus(EquipmentStatus::Active);

        // Self-assignment should be a no-op
        equipment = std::move(equipment);

        EXPECT_EQ(equipment.getId(), "123");
        EXPECT_EQ(equipment.getType(), EquipmentType::Forklift);
        EXPECT_EQ(equipment.getName(), "Test Forklift");
        EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
    }

    TEST_F(EquipmentTest, SettersWorkCorrectly)
    {
        Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");

        equipment.setStatus(EquipmentStatus::Maintenance);
        EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Maintenance);

        equipment.setName("New Name");
        EXPECT_EQ(equipment.getName(), "New Name");

        Position pos(10.0, 20.0, 0.0, 0.0);
        equipment.setLastPosition(pos);
        EXPECT_TRUE(equipment.getLastPosition().has_value());
        EXPECT_NEAR(equipment.getLastPosition()->getLatitude(), 10.0, 0.0001);
        EXPECT_NEAR(equipment.getLastPosition()->getLongitude(), 20.0, 0.0001);
    }

    TEST_F(EquipmentTest, RecordPositionUpdatesHistoryAndStatus)
    {
        Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");

        Position pos1(10.0, 20.0, 0.0, 0.0);
        equipment.recordPosition(pos1);

        EXPECT_EQ(equipment.getStatus(), EquipmentStatus::Active);
        EXPECT_TRUE(equipment.getLastPosition().has_value());
        EXPECT_NEAR(equipment.getLastPosition()->getLatitude(), 10.0, 0.0001);
        EXPECT_NEAR(equipment.getLastPosition()->getLongitude(), 20.0, 0.0001);
        EXPECT_EQ(equipment.getPositionHistory().size(), 1);

        Position pos2(11.0, 21.0, 0.0, 0.0);
        equipment.recordPosition(pos2);

        EXPECT_NEAR(equipment.getLastPosition()->getLatitude(), 11.0, 0.0001);
        EXPECT_NEAR(equipment.getLastPosition()->getLongitude(), 21.0, 0.0001);
        EXPECT_EQ(equipment.getPositionHistory().size(), 2);
    }

    TEST_F(EquipmentTest, ClearPositionHistoryWorks)
    {
        Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");

        Position pos(10.0, 20.0, 0.0, 0.0);
        equipment.recordPosition(pos);

        EXPECT_EQ(equipment.getPositionHistory().size(), 1);

        equipment.clearPositionHistory();

        EXPECT_EQ(equipment.getPositionHistory().size(), 0);
        // Last position should still be available
        EXPECT_TRUE(equipment.getLastPosition().has_value());
    }

    TEST_F(EquipmentTest, IsMovingReturnsFalseWithInsufficientData)
    {
        Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");

        // No positions
        EXPECT_FALSE(equipment.isMoving());

        // Only one position
        Position pos(10.0, 20.0, 0.0, 0.0);
        equipment.recordPosition(pos);
        EXPECT_FALSE(equipment.isMoving());
    }

    TEST_F(EquipmentTest, IsMovingDetectsMovement)
    {
        Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");

        // First position - using default constructor (current time)
        Position pos1(10.0, 20.0, 0.0, 0.0); // lat, lon, alt, accuracy
        equipment.recordPosition(pos1);

        // Wait at least 1 second to ensure time_diff >= 1 in isMoving() method
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Second position, moved significant distance (should trigger movement)
        // Moving 0.01 degrees at this latitude is approximately 1.1 km, which over 2 seconds
        // gives a speed of ~550 m/s, well above the 0.5 m/s threshold
        Position pos2(10.01, 20.01, 0.0, 0.0);
        equipment.recordPosition(pos2);

        EXPECT_TRUE(equipment.isMoving());
    }

    TEST_F(EquipmentTest, IsMovingReturnsFalseForSlowMovement)
    {
        Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");

        // First position
        Position pos1(10.0, 20.0, 0.0, 0.0);
        equipment.recordPosition(pos1);

        // Add a small delay to ensure different timestamps
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Second position, very small movement (below threshold)
        Position pos2(10.00001, 20.0, 0.0, 0.0); // Very small movement
        equipment.recordPosition(pos2);

        EXPECT_FALSE(equipment.isMoving());
    }

    TEST_F(EquipmentTest, IsMovingHandlesSmallTimeDifference)
    {
        Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");

        // First position
        Position pos1(10.0, 20.0, 0.0, 0.0);
        equipment.recordPosition(pos1);

        // Second position, almost immediately after (small time difference)
        Position pos2(10.001, 20.001, 0.0, 0.0); // Moved, but time diff will be very small
        equipment.recordPosition(pos2);

        // Since the time difference is very small, isMoving should return false
        EXPECT_FALSE(equipment.isMoving());
    }

    TEST_F(EquipmentTest, ToStringFormatsCorrectly)
    {
        Equipment equipment("123", EquipmentType::Forklift, "Test Forklift");

        std::string result = equipment.toString();

        EXPECT_THAT(result, ::testing::HasSubstr("id=123"));
        EXPECT_THAT(result, ::testing::HasSubstr("name=Test Forklift"));
        EXPECT_THAT(result, ::testing::HasSubstr("type=Forklift"));
        EXPECT_THAT(result, ::testing::HasSubstr("status=Inactive"));
    }

    TEST_F(EquipmentTest, ToStringHandlesAllEquipmentTypes)
    {
        Equipment forklift("1", EquipmentType::Forklift, "F");
        Equipment crane("2", EquipmentType::Crane, "C");
        Equipment bulldozer("3", EquipmentType::Bulldozer, "B");
        Equipment excavator("4", EquipmentType::Excavator, "E");
        Equipment truck("5", EquipmentType::Truck, "T");
        Equipment other("6", static_cast<EquipmentType>(99), "O");

        EXPECT_THAT(forklift.toString(), ::testing::HasSubstr("type=Forklift"));
        EXPECT_THAT(crane.toString(), ::testing::HasSubstr("type=Crane"));
        EXPECT_THAT(bulldozer.toString(), ::testing::HasSubstr("type=Bulldozer"));
        EXPECT_THAT(excavator.toString(), ::testing::HasSubstr("type=Excavator"));
        EXPECT_THAT(truck.toString(), ::testing::HasSubstr("type=Truck"));
        EXPECT_THAT(other.toString(), ::testing::HasSubstr("type=Other"));
    }

    TEST_F(EquipmentTest, ToStringHandlesAllStatusTypes)
    {
        Equipment active("1", EquipmentType::Forklift, "A");
        active.setStatus(EquipmentStatus::Active);

        Equipment inactive("2", EquipmentType::Forklift, "I");
        inactive.setStatus(EquipmentStatus::Inactive);

        Equipment maintenance("3", EquipmentType::Forklift, "M");
        maintenance.setStatus(EquipmentStatus::Maintenance);

        Equipment unknown("4", EquipmentType::Forklift, "U");
        unknown.setStatus(static_cast<EquipmentStatus>(99));

        EXPECT_THAT(active.toString(), ::testing::HasSubstr("status=Active"));
        EXPECT_THAT(inactive.toString(), ::testing::HasSubstr("status=Inactive"));
        EXPECT_THAT(maintenance.toString(), ::testing::HasSubstr("status=Maintenance"));
        EXPECT_THAT(unknown.toString(), ::testing::HasSubstr("status=Unknown"));
    }

} // namespace equipment_tracker