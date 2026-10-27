// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>
#include <optional>
#include <thread>
#include <chrono>
#include <cmath>
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/types.h"
#include "equipment_tracker/utils/constants.h"

using namespace equipment_tracker;
using ::testing::HasSubstr;

// ---------------------------------------------------------------------------
// Helper utilities
// ---------------------------------------------------------------------------

static Timestamp makeTimestamp(int64_t secondsSinceEpoch)
{
    return std::chrono::system_clock::from_time_t(static_cast<std::time_t>(secondsSinceEpoch));
}

static Position makePosition(double lat, double lon, double alt = 0.0,
                              double acc = DEFAULT_POSITION_ACCURACY,
                              Timestamp ts = std::chrono::system_clock::now())
{
    return Position(lat, lon, alt, acc, ts);
}

// Two positions far enough apart that speed > MOVEMENT_SPEED_THRESHOLD
// when recorded 1 second apart.
// ~111 m per 0.001 degree of latitude at the equator.
static Position makeFarPosition1(Timestamp ts)
{
    return Position(0.0, 0.0, 0.0, DEFAULT_POSITION_ACCURACY, ts);
}

static Position makeFarPosition2(Timestamp ts)
{
    return Position(0.01, 0.0, 0.0, DEFAULT_POSITION_ACCURACY, ts); // ~1.1 km away
}

// ===================================================================
// Test fixture
// ===================================================================

class EquipmentTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        eq = std::make_unique<Equipment>("EQ-001", EquipmentType::Crane, "Test Crane");
    }

    std::unique_ptr<Equipment> eq;
};

// ===================================================================
// Constructor tests
// ===================================================================

TEST_F(EquipmentTest, Constructor_SetsIdTypeAndName)
{
    EXPECT_EQ(eq->getId(), "EQ-001");
    EXPECT_EQ(eq->getType(), EquipmentType::Crane);
    EXPECT_EQ(eq->getName(), "Test Crane");
}

TEST_F(EquipmentTest, Constructor_DefaultStatusIsInactive)
{
    EXPECT_EQ(eq->getStatus(), EquipmentStatus::Inactive);
}

TEST_F(EquipmentTest, Constructor_NoLastPositionInitially)
{
    auto pos = eq->getLastPosition();
    EXPECT_FALSE(pos.has_value());
}

TEST_F(EquipmentTest, Constructor_EmptyPositionHistory)
{
    auto history = eq->getPositionHistory();
    EXPECT_TRUE(history.empty());
}

TEST_F(EquipmentTest, Constructor_EmptyId)
{
    Equipment e("", EquipmentType::Truck, "No-ID Truck");
    EXPECT_EQ(e.getId(), "");
    EXPECT_EQ(e.getName(), "No-ID Truck");
}

TEST_F(EquipmentTest, Constructor_EmptyName)
{
    Equipment e("EQ-X", EquipmentType::Forklift, "");
    EXPECT_EQ(e.getName(), "");
}

TEST_F(EquipmentTest, Constructor_AllEquipmentTypes)
{
    Equipment e1("1", EquipmentType::Forklift, "F");
    EXPECT_EQ(e1.getType(), EquipmentType::Forklift);

    Equipment e2("2", EquipmentType::Crane, "C");
    EXPECT_EQ(e2.getType(), EquipmentType::Crane);

    Equipment e3("3", EquipmentType::Bulldozer, "B");
    EXPECT_EQ(e3.getType(), EquipmentType::Bulldozer);

    Equipment e4("4", EquipmentType::Excavator, "E");
    EXPECT_EQ(e4.getType(), EquipmentType::Excavator);

    Equipment e5("5", EquipmentType::Truck, "T");
    EXPECT_EQ(e5.getType(), EquipmentType::Truck);

    Equipment e6("6", EquipmentType::Other, "O");
    EXPECT_EQ(e6.getType(), EquipmentType::Other);
}

// ===================================================================
// Copy constructor / copy assignment
// ===================================================================

TEST_F(EquipmentTest, CopyConstructor_CopiesAllFields)
{
    eq->setStatus(EquipmentStatus::Active);
    eq->setName("Renamed Crane");
    auto ts = makeTimestamp(1000000);
    Position p(10.0, 20.0, 30.0, 1.0, ts);
    eq->recordPosition(p);

    Equipment copy(*eq);

    EXPECT_EQ(copy.getId(), eq->getId());
    EXPECT_EQ(copy.getType(), eq->getType());
    EXPECT_EQ(copy.getName(), eq->getName());
    EXPECT_EQ(copy.getStatus(), eq->getStatus());
    ASSERT_TRUE(copy.getLastPosition().has_value());
    EXPECT_NEAR(copy.getLastPosition()->getLatitude(), 10.0, 1e-9);
    EXPECT_EQ(copy.getPositionHistory().size(), eq->getPositionHistory().size());
}

TEST_F(EquipmentTest, CopyConstructor_IndependentOfOriginal)
{
    eq->recordPosition(makePosition(1.0, 2.0));
    Equipment copy(*eq);

    // Mutate original
    eq->setName("Changed");
    eq->clearPositionHistory();

    // Copy should be unaffected
    EXPECT_EQ(copy.getName(), "Test Crane");
    EXPECT_EQ(copy.getPositionHistory().size(), 1u);
}

TEST_F(EquipmentTest, CopyAssignment_CopiesAllFields)
{
    eq->setStatus(EquipmentStatus::Maintenance);
    auto ts = makeTimestamp(2000000);
    eq->recordPosition(Position(5.0, 6.0, 7.0, 1.0, ts));

    Equipment other("EQ-OTHER", EquipmentType::Truck, "Other");
    other = *eq;

    EXPECT_EQ(other.getId(), "EQ-001");
    EXPECT_EQ(other.getType(), EquipmentType::Crane);
    EXPECT_EQ(other.getName(), "Test Crane");
    // recordPosition sets status to Active
    EXPECT_EQ(other.getStatus(), EquipmentStatus::Active);
    EXPECT_EQ(other.getPositionHistory().size(), 1u);
}

TEST_F(EquipmentTest, CopyAssignment_SelfAssignment)
{
    eq->recordPosition(makePosition(1.0, 2.0));
    auto &ref = *eq;
    // Self-assignment should be a no-op
    *eq = ref;

    EXPECT_EQ(eq->getId(), "EQ-001");
    EXPECT_EQ(eq->getPositionHistory().size(), 1u);
}

// ===================================================================
// Move constructor / move assignment
// ===================================================================

TEST_F(EquipmentTest, MoveConstructor_TransfersOwnership)
{
    eq->setStatus(EquipmentStatus::Active);
    auto ts = makeTimestamp(3000000);
    eq->recordPosition(Position(11.0, 22.0, 33.0, 1.0, ts));

    Equipment moved(std::move(*eq));

    EXPECT_EQ(moved.getId(), "EQ-001");
    EXPECT_EQ(moved.getType(), EquipmentType::Crane);
    EXPECT_EQ(moved.getName(), "Test Crane");
    // recordPosition sets status to Active
    EXPECT_EQ(moved.getStatus(), EquipmentStatus::Active);
    ASSERT_TRUE(moved.getLastPosition().has_value());
    EXPECT_NEAR(moved.getLastPosition()->getLatitude(), 11.0, 1e-9);
    EXPECT_EQ(moved.getPositionHistory().size(), 1u);

    // Moved-from object should have Unknown status
    EXPECT_EQ(eq->getStatus(), EquipmentStatus::Unknown);
}

TEST_F(EquipmentTest, MoveAssignment_TransfersOwnership)
{
    eq->recordPosition(makePosition(44.0, 55.0));

    Equipment target("EQ-TARGET", EquipmentType::Truck, "Target");
    target = std::move(*eq);

    EXPECT_EQ(target.getId(), "EQ-001");
    EXPECT_EQ(target.getName(), "Test Crane");
    EXPECT_EQ(target.getPositionHistory().size(), 1u);

    // Moved-from object should have Unknown status
    EXPECT_EQ(eq->getStatus(), EquipmentStatus::Unknown);
}

TEST_F(EquipmentTest, MoveAssignment_SelfAssignment)
{
    eq->recordPosition(makePosition(1.0, 2.0));
    auto *raw = eq.get();
    // Self-move-assignment should be safe
    *raw = std::move(*raw);

    EXPECT_EQ(raw->getId(), "EQ-001");
    EXPECT_EQ(raw->getPositionHistory().size(), 1u);
}

// ===================================================================
// Getter tests
// ===================================================================

TEST_F(EquipmentTest, GetId_ReturnsCorrectId)
{
    EXPECT_EQ(eq->getId(), "EQ-001");
}

TEST_F(EquipmentTest, GetType_ReturnsCorrectType)
{
    EXPECT_EQ(eq->getType(), EquipmentType::Crane);
}

TEST_F(EquipmentTest, GetName_ReturnsCorrectName)
{
    EXPECT_EQ(eq->getName(), "Test Crane");
}

TEST_F(EquipmentTest, GetStatus_ReturnsCorrectStatus)
{
    EXPECT_EQ(eq->getStatus(), EquipmentStatus::Inactive);
}

// ===================================================================
// Setter tests
// ===================================================================

TEST_F(EquipmentTest, SetStatus_ChangesStatus)
{
    eq->setStatus(EquipmentStatus::Active);
    EXPECT_EQ(eq->getStatus(), EquipmentStatus::Active);

    eq->setStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(eq->getStatus(), EquipmentStatus::Maintenance);

    eq->setStatus(EquipmentStatus::Unknown);
    EXPECT_EQ(eq->getStatus(), EquipmentStatus::Unknown);

    eq->setStatus(EquipmentStatus::Inactive);
    EXPECT_EQ(eq->getStatus(), EquipmentStatus::Inactive);
}

TEST_F(EquipmentTest, SetName_ChangesName)
{
    eq->setName("New Name");
    EXPECT_EQ(eq->getName(), "New Name");
}

TEST_F(EquipmentTest, SetName_EmptyString)
{
    eq->setName("");
    EXPECT_EQ(eq->getName(), "");
}

TEST_F(EquipmentTest, SetName_LongString)
{
    std::string longName(1000, 'A');
    eq->setName(longName);
    EXPECT_EQ(eq->getName(), longName);
}

TEST_F(EquipmentTest, SetLastPosition_SetsPosition)
{
    auto ts = makeTimestamp(5000000);
    Position p(40.0, -74.0, 10.0, 2.0, ts);
    eq->setLastPosition(p);

    auto result = eq->getLastPosition();
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->getLatitude(), 40.0, 1e-9);
    EXPECT_NEAR(result->getLongitude(), -74.0, 1e-9);
    EXPECT_NEAR(result->getAltitude(), 10.0, 1e-9);
}

TEST_F(EquipmentTest, SetLastPosition_DoesNotAffectHistory)
{
    Position p(40.0, -74.0);
    eq->setLastPosition(p);

    // setLastPosition should NOT add to history
    EXPECT_TRUE(eq->getPositionHistory().empty());
}

TEST_F(EquipmentTest, SetLastPosition_OverwritesPrevious)
{
    eq->setLastPosition(makePosition(10.0, 20.0));
    eq->setLastPosition(makePosition(30.0, 40.0));

    auto result = eq->getLastPosition();
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->getLatitude(), 30.0, 1e-9);
    EXPECT_NEAR(result->getLongitude(), 40.0, 1e-9);
}

// ===================================================================
// recordPosition tests
// ===================================================================

TEST_F(EquipmentTest, RecordPosition_AddsToHistory)
{
    auto ts = makeTimestamp(6000000);
    Position p(51.5, -0.1, 0.0, 2.5, ts);
    eq->recordPosition(p);

    auto history = eq->getPositionHistory();
    ASSERT_EQ(history.size(), 1u);
    EXPECT_NEAR(history[0].getLatitude(), 51.5, 1e-9);
    EXPECT_NEAR(history[0].getLongitude(), -0.1, 1e-9);
}

TEST_F(EquipmentTest, RecordPosition_UpdatesLastPosition)
{
    auto ts = makeTimestamp(7000000);
    Position p(48.8, 2.3, 35.0, 1.0, ts);
    eq->recordPosition(p);

    auto last = eq->getLastPosition();
    ASSERT_TRUE(last.has_value());
    EXPECT_NEAR(last->getLatitude(), 48.8, 1e-9);
    EXPECT_NEAR(last->getLongitude(), 2.3, 1e-9);
}

TEST_F(EquipmentTest, RecordPosition_SetsStatusToActive)
{
    EXPECT_EQ(eq->getStatus(), EquipmentStatus::Inactive);

    eq->recordPosition(makePosition(1.0, 2.0));
    EXPECT_EQ(eq->getStatus(), EquipmentStatus::Active);
}

TEST_F(EquipmentTest, RecordPosition_SetsStatusToActiveFromMaintenance)
{
    eq->setStatus(EquipmentStatus::Maintenance);
    eq->recordPosition(makePosition(1.0, 2.0));
    EXPECT_EQ(eq->getStatus(), EquipmentStatus::Active);
}

TEST_F(EquipmentTest, RecordPosition_MultiplePositions)
{
    for (int i = 0; i < 5; ++i)
    {
        auto ts = makeTimestamp(8000000 + i * 100);
        eq->recordPosition(Position(static_cast<double>(i), static_cast<double>(i), 0.0, 2.5, ts));
    }

    auto history = eq->getPositionHistory();
    EXPECT_EQ(history.size(), 5u);

    // Last position should be the most recently recorded
    auto last = eq->getLastPosition();
    ASSERT_TRUE(last.has_value());
    EXPECT_NEAR(last->getLatitude(), 4.0, 1e-9);
}

TEST_F(EquipmentTest, RecordPosition_HistoryTrimmedAtMaxSize)
{
    // DEFAULT_MAX_HISTORY_SIZE is 100
    for (size_t i = 0; i <= DEFAULT_MAX_HISTORY_SIZE; ++i)
    {
        auto ts = makeTimestamp(static_cast<int64_t>(9000000 + i * 10));
        eq->recordPosition(Position(static_cast<double>(i) * 0.001, 0.0, 0.0, 2.5, ts));
    }

    auto history = eq->getPositionHistory();
    // After recording 101 positions, the oldest should be removed
    EXPECT_EQ(history.size(), DEFAULT_MAX_HISTORY_SIZE);

    // The first entry should be the second position recorded (index 1), not index 0
    EXPECT_NEAR(history[0].getLatitude(), 0.001, 1e-9);
}

TEST_F(EquipmentTest, RecordPosition_HistoryDoesNotExceedMaxSize)
{
    // Record well over the max
    for (size_t i = 0; i < DEFAULT_MAX_HISTORY_SIZE + 50; ++i)
    {
        auto ts = makeTimestamp(static_cast<int64_t>(10000000 + i * 10));
        eq->recordPosition(Position(static_cast<double>(i) * 0.0001, 0.0, 0.0, 2.5, ts));
    }

    auto history = eq->getPositionHistory();
    EXPECT_EQ(history.size(), DEFAULT_MAX_HISTORY_SIZE);
}

// ===================================================================
// getPositionHistory tests
// ===================================================================

TEST_F(EquipmentTest, GetPositionHistory_EmptyInitially)
{
    EXPECT_TRUE(eq->getPositionHistory().empty());
}

TEST_F(EquipmentTest, GetPositionHistory_ReturnsCopy)
{
    eq->recordPosition(makePosition(1.0, 2.0));
    auto history = eq->getPositionHistory();
    history.clear(); // Clearing the copy should not affect the equipment

    EXPECT_EQ(eq->getPositionHistory().size(), 1u);
}

// ===================================================================
// clearPositionHistory tests
// ===================================================================

TEST_F(EquipmentTest, ClearPositionHistory_ClearsHistory)
{
    eq->recordPosition(makePosition(1.0, 2.0));
    eq->recordPosition(makePosition(3.0, 4.0));
    EXPECT_EQ(eq->getPositionHistory().size(), 2u);

    eq->clearPositionHistory();
    EXPECT_TRUE(eq->getPositionHistory().empty());
}

TEST_F(EquipmentTest, ClearPositionHistory_DoesNotAffectLastPosition)
{
    eq->recordPosition(makePosition(10.0, 20.0));
    eq->clearPositionHistory();

    // Last position should still be set even after clearing history
    auto last = eq->getLastPosition();
    ASSERT_TRUE(last.has_value());
    EXPECT_NEAR(last->getLatitude(), 10.0, 1e-9);
}

TEST_F(EquipmentTest, ClearPositionHistory_OnEmptyHistory)
{
    // Should not crash
    eq->clearPositionHistory();
    EXPECT_TRUE(eq->getPositionHistory().empty());
}

// ===================================================================
// isMoving tests
// ===================================================================

TEST_F(EquipmentTest, IsMoving_FalseWithNoPositions)
{
    EXPECT_FALSE(eq->isMoving());
}

TEST_F(EquipmentTest, IsMoving_FalseWithOnePosition)
{
    eq->recordPosition(makePosition(0.0, 0.0));
    EXPECT_FALSE(eq->isMoving());
}

TEST_F(EquipmentTest, IsMoving_FalseWhenStationaryTwoPositions)
{
    auto ts1 = makeTimestamp(11000000);
    auto ts2 = makeTimestamp(11000010); // 10 seconds later

    // Same location, different times
    eq->recordPosition(Position(40.0, -74.0, 0.0, 2.5, ts1));
    eq->recordPosition(Position(40.0, -74.0, 0.0, 2.5, ts2));

    EXPECT_FALSE(eq->isMoving());
}

TEST_F(EquipmentTest, IsMoving_TrueWhenMovingFast)
{
    auto ts1 = makeTimestamp(12000000);
    auto ts2 = makeTimestamp(12000001); // 1 second later

    // Positions ~1.1 km apart, 1 second apart => speed ~1100 m/s >> 0.5 m/s threshold
    eq->recordPosition(makeFarPosition1(ts1));
    eq->recordPosition(makeFarPosition2(ts2));

    EXPECT_TRUE(eq->isMoving());
}

TEST_F(EquipmentTest, IsMoving_FalseWhenTimeDiffLessThanOneSecond)
{
    auto ts1 = makeTimestamp(13000000);
    auto ts2 = ts1; // Same timestamp => time_diff < 1

    eq->recordPosition(makeFarPosition1(ts1));
    eq->recordPosition(makeFarPosition2(ts2));

    // Even though positions are far apart, time_diff < 1 returns false
    EXPECT_FALSE(eq->isMoving());
}

TEST_F(EquipmentTest, IsMoving_FalseWhenSpeedBelowThreshold)
{
    auto ts1 = makeTimestamp(14000000);
    auto ts2 = makeTimestamp(14100000); // 100000 seconds later

    // Very small distance, very long time => speed well below threshold
    // ~0.00001 degrees latitude ≈ ~1.1 m
    eq->recordPosition(Position(0.0, 0.0, 0.0, 2.5, ts1));
    eq->recordPosition(Position(0.00001, 0.0, 0.0, 2.5, ts2));

    // distance ~1.1 m / 100000 s = ~0.000011 m/s << 0.5 m/s
    EXPECT_FALSE(eq->isMoving());
}

TEST_F(EquipmentTest, IsMoving_UsesLastTwoPositionsOnly)
{
    auto ts1 = makeTimestamp(15000000);
    auto ts2 = makeTimestamp(15000001);
    auto ts3 = makeTimestamp(15000002);

    // First two positions are far apart (moving)
    eq->recordPosition(makeFarPosition1(ts1));
    eq->recordPosition(makeFarPosition2(ts2));
    EXPECT_TRUE(eq->isMoving());

    // Third position is same as second (stationary)
    eq->recordPosition(Position(0.01, 0.0, 0.0, DEFAULT_POSITION_ACCURACY, ts3));
    // Now last two positions are the same => not moving
    EXPECT_FALSE(eq->isMoving());
}

// ===================================================================
// toString tests
// ===================================================================

TEST_F(EquipmentTest, ToString_ContainsId)
{
    std::string str = eq->toString();
    EXPECT_THAT(str, HasSubstr("EQ-001"));
}

TEST_F(EquipmentTest, ToString_ContainsName)
{
    std::string str = eq->toString();
    EXPECT_THAT(str, HasSubstr("Test Crane"));
}

TEST_F(EquipmentTest, ToString_ContainsTypeCrane)
{
    std::string str = eq->toString();
    EXPECT_THAT(str, HasSubstr("Crane"));
}

TEST_F(EquipmentTest, ToString_ContainsStatusInactive)
{
    std::string str = eq->toString();
    EXPECT_THAT(str, HasSubstr("Inactive"));
}

TEST_F(EquipmentTest, ToString_ReflectsStatusChange)
{
    eq->setStatus(EquipmentStatus::Active);
    EXPECT_THAT(eq->toString(), HasSubstr("Active"));

    eq->setStatus(EquipmentStatus::Maintenance);
    EXPECT_THAT(eq->toString(), HasSubstr("Maintenance"));

    eq->setStatus(EquipmentStatus::Unknown);
    EXPECT_THAT(eq->toString(), HasSubstr("Unknown"));
}

TEST_F(EquipmentTest, ToString_AllEquipmentTypes)
{
    Equipment e1("1", EquipmentType::Forklift, "F");
    EXPECT_THAT(e1.toString(), HasSubstr("Forklift"));

    Equipment e2("2", EquipmentType::Crane, "C");
    EXPECT_THAT(e2.toString(), HasSubstr("Crane"));

    Equipment e3("3", EquipmentType::Bulldozer, "B");
    EXPECT_THAT(e3.toString(), HasSubstr("Bulldozer"));

    Equipment e4("4", EquipmentType::Excavator, "E");
    EXPECT_THAT(e4.toString(), HasSubstr("Excavator"));

    Equipment e5("5", EquipmentType::Truck, "T");
    EXPECT_THAT(e5.toString(), HasSubstr("Truck"));

    Equipment e6("6", EquipmentType::Other, "O");
    EXPECT_THAT(e6.toString(), HasSubstr("Other"));
}

TEST_F(EquipmentTest, ToString_ContainsEquipmentPrefix)
{
    std::string str = eq->toString();
    EXPECT_THAT(str, HasSubstr("Equipment("));
}

// ===================================================================
// Thread-safety tests
// ===================================================================

TEST_F(EquipmentTest, ThreadSafety_ConcurrentRecordAndRead)
{
    // Verify no crashes or data races with concurrent access
    constexpr int numIterations = 200;

    auto writer = [this]()
    {
        for (int i = 0; i < numIterations; ++i)
        {
            auto ts = makeTimestamp(16000000 + i);
            eq->recordPosition(Position(static_cast<double>(i) * 0.001, 0.0, 0.0, 2.5, ts));
        }
    };

    auto reader = [this]()
    {
        for (int i = 0; i < numIterations; ++i)
        {
            auto pos = eq->getLastPosition();
            auto history = eq->getPositionHistory();
            (void)eq->isMoving();
            (void)pos;
            (void)history;
        }
    };

    std::thread t1(writer);
    std::thread t2(reader);
    std::thread t3(reader);

    t1.join();
    t2.join();
    t3.join();

    // After all writes, history should be capped at max
    auto history = eq->getPositionHistory();
    EXPECT_LE(history.size(), DEFAULT_MAX_HISTORY_SIZE);
    EXPECT_GT(history.size(), 0u);
}

TEST_F(EquipmentTest, ThreadSafety_ConcurrentSetAndGetLastPosition)
{
    constexpr int numIterations = 200;

    auto writer = [this]()
    {
        for (int i = 0; i < numIterations; ++i)
        {
            eq->setLastPosition(makePosition(static_cast<double>(i), static_cast<double>(i)));
        }
    };

    auto reader = [this]()
    {
        for (int i = 0; i < numIterations; ++i)
        {
            auto pos = eq->getLastPosition();
            (void)pos;
        }
    };

    std::thread t1(writer);
    std::thread t2(reader);

    t1.join();
    t2.join();

    // Should have a valid last position
    EXPECT_TRUE(eq->getLastPosition().has_value());
}

TEST_F(EquipmentTest, ThreadSafety_ConcurrentClearAndRecord)
{
    constexpr int numIterations = 100;

    auto writer = [this]()
    {
        for (int i = 0; i < numIterations; ++i)
        {
            auto ts = makeTimestamp(17000000 + i);
            eq->recordPosition(Position(static_cast<double>(i) * 0.001, 0.0, 0.0, 2.5, ts));
        }
    };

    auto clearer = [this]()
    {
        for (int i = 0; i < numIterations; ++i)
        {
            eq->clearPositionHistory();
        }
    };

    std::thread t1(writer);
    std::thread t2(clearer);

    t1.join();
    t2.join();

    // Should not crash; history size is indeterminate but valid
    auto history = eq->getPositionHistory();
    EXPECT_LE(history.size(), DEFAULT_MAX_HISTORY_SIZE);
}

// ===================================================================
// Edge case / boundary tests
// ===================================================================

TEST_F(EquipmentTest, RecordPosition_ExactlyMaxHistorySize)
{
    for (size_t i = 0; i < DEFAULT_MAX_HISTORY_SIZE; ++i)
    {
        auto ts = makeTimestamp(static_cast<int64_t>(18000000 + i * 10));
        eq->recordPosition(Position(static_cast<double>(i) * 0.001, 0.0, 0.0, 2.5, ts));
    }

    auto history = eq->getPositionHistory();
    EXPECT_EQ(history.size(), DEFAULT_MAX_HISTORY_SIZE);
}

TEST_F(EquipmentTest, RecordPosition_ExtremeCoordinates)
{
    // North pole
    eq->recordPosition(makePosition(90.0, 0.0));
    auto last = eq->getLastPosition();
    ASSERT_TRUE(last.has_value());
    EXPECT_NEAR(last->getLatitude(), 90.0, 1e-9);

    // South pole
    eq->recordPosition(makePosition(-90.0, 0.0));
    last = eq->getLastPosition();
    ASSERT_TRUE(last.has_value());
    EXPECT_NEAR(last->getLatitude(), -90.0, 1e-9);

    // International date line
    eq->recordPosition(makePosition(0.0, 180.0));
    last = eq->getLastPosition();
    ASSERT_TRUE(last.has_value());
    EXPECT_NEAR(last->getLongitude(), 180.0, 1e-9);

    eq->recordPosition(makePosition(0.0, -180.0));
    last = eq->getLastPosition();
    ASSERT_TRUE(last.has_value());
    EXPECT_NEAR(last->getLongitude(), -180.0, 1e-9);
}

TEST_F(EquipmentTest, GetLastPosition_ThreadSafe_ReturnsOptional)
{
    // Before any position is set, should return empty optional
    auto pos = eq->getLastPosition();
    EXPECT_FALSE(pos.has_value());

    // After setting, should return value
    eq->setLastPosition(makePosition(1.0, 2.0));
    pos = eq->getLastPosition();
    EXPECT_TRUE(pos.has_value());
}

TEST_F(EquipmentTest, SetName_SpecialCharacters)
{
    eq->setName("Crane #1 (Main) - 50t / \"Heavy\"");
    EXPECT_EQ(eq->getName(), "Crane #1 (Main) - 50t / \"Heavy\"");
}

TEST_F(EquipmentTest, SetName_UnicodeCharacters)
{
    eq->setName("Kran \xC3\xBC\x62\x65\x72 50t"); // "Kran über 50t" in UTF-8
    EXPECT_EQ(eq->getName(), "Kran \xC3\xBC\x62\x65\x72 50t");
}

TEST_F(EquipmentTest, IsMoving_AfterClearHistory)
{
    auto ts1 = makeTimestamp(19000000);
    auto ts2 = makeTimestamp(19000001);

    eq->recordPosition(makeFarPosition1(ts1));
    eq->recordPosition(makeFarPosition2(ts2));
    EXPECT_TRUE(eq->isMoving());

    eq->clearPositionHistory();
    EXPECT_FALSE(eq->isMoving());
}

TEST_F(EquipmentTest, RecordPosition_AfterClearHistory_StartsNewHistory)
{
    eq->recordPosition(makePosition(1.0, 2.0));
    eq->recordPosition(makePosition(3.0, 4.0));
    eq->clearPositionHistory();

    eq->recordPosition(makePosition(5.0, 6.0));
    auto history = eq->getPositionHistory();
    ASSERT_EQ(history.size(), 1u);
    EXPECT_NEAR(history[0].getLatitude(), 5.0, 1e-9);
}

// ===================================================================
// Multiple operations integration test
// ===================================================================

TEST_F(EquipmentTest, Integration_FullLifecycle)
{
    // Initial state
    EXPECT_EQ(eq->getStatus(), EquipmentStatus::Inactive);
    EXPECT_FALSE(eq->getLastPosition().has_value());
    EXPECT_TRUE(eq->getPositionHistory().empty());
    EXPECT_FALSE(eq->isMoving());

    // Set to maintenance
    eq->setStatus(EquipmentStatus::Maintenance);
    EXPECT_EQ(eq->getStatus(), EquipmentStatus::Maintenance);

    // Start recording positions (becomes active)
    auto ts1 = makeTimestamp(20000000);
    eq->recordPosition(Position(40.7128, -74.0060, 10.0, 2.5, ts1));
    EXPECT_EQ(eq->getStatus(), EquipmentStatus::Active);
    EXPECT_FALSE(eq->isMoving()); // Only one position

    // Record second position far away, 2 seconds later
    auto ts2 = makeTimestamp(20000002);
    eq->recordPosition(Position(40.7228, -74.0060, 10.0, 2.5, ts2));
    EXPECT_TRUE(eq->isMoving()); // ~1.1 km in 2 seconds

    // Record third position same as second, 2 seconds later
    auto ts3 = makeTimestamp(20000004);
    eq->recordPosition(Position(40.7228, -74.0060, 10.0, 2.5, ts3));
    EXPECT_FALSE(eq->isMoving()); // Stationary

    // Verify history
    auto history = eq->getPositionHistory();
    EXPECT_EQ(history.size(), 3u);

    // Clear and verify
    eq->clearPositionHistory();
    EXPECT_TRUE(eq->getPositionHistory().empty());
    EXPECT_TRUE(eq->getLastPosition().has_value()); // Last position persists

    // Rename
    eq->setName("Renamed Crane");
    EXPECT_EQ(eq->getName(), "Renamed Crane");

    // toString should reflect current state
    std::string str = eq->toString();
    EXPECT_THAT(str, HasSubstr("Renamed Crane"));
    EXPECT_THAT(str, HasSubstr("Active"));
    EXPECT_THAT(str, HasSubstr("Crane"));
}
// </test_code>

---

**Confidence: High**
All source code, dependencies, and build configuration were successfully retrieved from the repository. The test file covers the full public interface of the `Equipment` class including: constructor variants, copy/move semantics, getters/setters, position recording with history trimming, `isMoving()` logic with all branches (no positions, one position, stationary, moving, zero time-diff), `toString()` for all enum values, thread-safety under concurrent access, edge cases (extreme coordinates, empty strings, special characters, self-assignment), and a full lifecycle integration test. The tests use GoogleTest v1.17.0 as specified in the project's `tests/CMakeLists.txt` and link against the `equipment_tracker` static library.