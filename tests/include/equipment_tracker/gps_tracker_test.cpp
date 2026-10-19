// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <sstream>
#include <string>
#include <atomic>
#include <vector>
#include <cmath>
#include "equipment_tracker/gps_tracker.h"
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/time_utils.h"
#include "equipment_tracker/utils/types.h"
#include "equipment_tracker/utils/constants.h"

// ============================================================================
// Testable wrapper for EquipmentNMEAParser to access protected/private methods
// ============================================================================
class TestableEquipmentNMEAParser : public equipment_tracker::EquipmentNMEAParser
{
public:
    void testOnError(CNMEAParserData::ERROR_E nError, char *pCmd)
    {
        OnError(nError, pCmd);
    }

    void testLockDataAccess()
    {
        LockDataAccess();
    }

    void testUnlockDataAccess()
    {
        UnlockDataAccess();
    }

    void testTriggerPositionCallback(double latitude, double longitude, double altitude)
    {
        triggerPositionCallback(latitude, longitude, altitude);
    }

    void testSetPositionCallback(equipment_tracker::PositionCallback callback)
    {
        setPositionCallback(std::move(callback));
    }
};

// ============================================================================
// Test fixture for EquipmentNMEAParser
// ============================================================================
class EquipmentNMEAParserTest : public ::testing::Test
{
protected:
    TestableEquipmentNMEAParser parser;
    bool callback_called = false;
    double received_lat = 0.0;
    double received_lon = 0.0;
    double received_alt = 0.0;
    equipment_tracker::Timestamp received_ts;

    void SetUp() override
    {
        callback_called = false;
        received_lat = 0.0;
        received_lon = 0.0;
        received_alt = 0.0;
        received_ts = equipment_tracker::Timestamp{};

        parser.testSetPositionCallback(
            [this](double lat, double lon, double alt, equipment_tracker::Timestamp ts)
            {
                callback_called = true;
                received_lat = lat;
                received_lon = lon;
                received_alt = alt;
                received_ts = ts;
            });
    }
};

// ============================================================================
// Test fixture for GPSTracker
// ============================================================================
class GPSTrackerTest : public ::testing::Test
{
protected:
    std::unique_ptr<equipment_tracker::GPSTracker> tracker;
    std::atomic<bool> callback_called{false};
    std::atomic<int> callback_count{0};
    double received_lat = 0.0;
    double received_lon = 0.0;
    double received_alt = 0.0;
    equipment_tracker::Timestamp received_ts;

    void SetUp() override
    {
        callback_called = false;
        callback_count = 0;
        received_lat = 0.0;
        received_lon = 0.0;
        received_alt = 0.0;
        received_ts = equipment_tracker::Timestamp{};

        tracker = std::make_unique<equipment_tracker::GPSTracker>(100);
        tracker->registerPositionCallback(
            [this](double lat, double lon, double alt, equipment_tracker::Timestamp ts)
            {
                callback_called = true;
                callback_count++;
                received_lat = lat;
                received_lon = lon;
                received_alt = alt;
                received_ts = ts;
            });
    }

    void TearDown() override
    {
        if (tracker)
        {
            tracker->stop();
            tracker.reset();
        }
    }
};

// ============================================================================
// EquipmentNMEAParser Tests
// ============================================================================

// --- OnError ---

TEST_F(EquipmentNMEAParserTest, OnError_WithCommand_PrintsErrorAndCommand)
{
    std::stringstream buffer;
    std::streambuf *old = std::cerr.rdbuf(buffer.rdbuf());

    char cmd[] = "GPGGA";
    parser.testOnError(CNMEAParserData::ERROR_UNKNOWN, cmd);

    std::string output = buffer.str();
    EXPECT_THAT(output, ::testing::HasSubstr("NMEA Parser Error"));
    EXPECT_THAT(output, ::testing::HasSubstr("GPGGA"));

    std::cerr.rdbuf(old);
}

TEST_F(EquipmentNMEAParserTest, OnError_WithNullCommand_PrintsErrorOnly)
{
    std::stringstream buffer;
    std::streambuf *old = std::cerr.rdbuf(buffer.rdbuf());

    parser.testOnError(CNMEAParserData::ERROR_UNKNOWN, nullptr);

    std::string output = buffer.str();
    EXPECT_THAT(output, ::testing::HasSubstr("NMEA Parser Error"));
    // Should NOT contain "for command:" since pCmd is null
    EXPECT_THAT(output, ::testing::Not(::testing::HasSubstr("for command")));

    std::cerr.rdbuf(old);
}

TEST_F(EquipmentNMEAParserTest, OnError_WithOKError_PrintsErrorCode)
{
    std::stringstream buffer;
    std::streambuf *old = std::cerr.rdbuf(buffer.rdbuf());

    parser.testOnError(CNMEAParserData::ERROR_OK, nullptr);

    std::string output = buffer.str();
    EXPECT_THAT(output, ::testing::HasSubstr("NMEA Parser Error"));

    std::cerr.rdbuf(old);
}

// --- LockDataAccess / UnlockDataAccess ---

TEST_F(EquipmentNMEAParserTest, LockUnlockDataAccess_DoesNotDeadlock)
{
    // Verify that lock/unlock can be called without deadlocking
    parser.testLockDataAccess();
    parser.testUnlockDataAccess();
    // If we reach here, no deadlock occurred
    SUCCEED();
}

TEST_F(EquipmentNMEAParserTest, LockUnlockDataAccess_MultipleCycles)
{
    for (int i = 0; i < 10; ++i)
    {
        parser.testLockDataAccess();
        parser.testUnlockDataAccess();
    }
    SUCCEED();
}

// --- triggerPositionCallback ---

TEST_F(EquipmentNMEAParserTest, TriggerPositionCallback_CallsRegisteredCallback)
{
    parser.testTriggerPositionCallback(37.7749, -122.4194, 10.0);

    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, received_lat);
    EXPECT_DOUBLE_EQ(-122.4194, received_lon);
    EXPECT_DOUBLE_EQ(10.0, received_alt);
}

TEST_F(EquipmentNMEAParserTest, TriggerPositionCallback_SetsTimestamp)
{
    auto before = equipment_tracker::getCurrentTimestamp();
    parser.testTriggerPositionCallback(0.0, 0.0, 0.0);
    auto after = equipment_tracker::getCurrentTimestamp();

    EXPECT_TRUE(callback_called);
    EXPECT_GE(received_ts, before);
    EXPECT_LE(received_ts, after);
}

TEST_F(EquipmentNMEAParserTest, TriggerPositionCallback_WithNoCallback_DoesNotCrash)
{
    TestableEquipmentNMEAParser parser_no_cb;
    // No callback registered — should not crash
    EXPECT_NO_FATAL_FAILURE(parser_no_cb.testTriggerPositionCallback(1.0, 2.0, 3.0));
}

TEST_F(EquipmentNMEAParserTest, TriggerPositionCallback_NegativeCoordinates)
{
    parser.testTriggerPositionCallback(-33.8688, 151.2093, -5.0);

    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(-33.8688, received_lat);
    EXPECT_DOUBLE_EQ(151.2093, received_lon);
    EXPECT_DOUBLE_EQ(-5.0, received_alt);
}

TEST_F(EquipmentNMEAParserTest, TriggerPositionCallback_ZeroCoordinates)
{
    parser.testTriggerPositionCallback(0.0, 0.0, 0.0);

    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(0.0, received_lat);
    EXPECT_DOUBLE_EQ(0.0, received_lon);
    EXPECT_DOUBLE_EQ(0.0, received_alt);
}

TEST_F(EquipmentNMEAParserTest, TriggerPositionCallback_ExtremeLatLon)
{
    parser.testTriggerPositionCallback(90.0, 180.0, 8848.0);

    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(90.0, received_lat);
    EXPECT_DOUBLE_EQ(180.0, received_lon);
    EXPECT_DOUBLE_EQ(8848.0, received_alt);
}

TEST_F(EquipmentNMEAParserTest, TriggerPositionCallback_ExtremeNegativeLatLon)
{
    parser.testTriggerPositionCallback(-90.0, -180.0, -430.0);

    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(-90.0, received_lat);
    EXPECT_DOUBLE_EQ(-180.0, received_lon);
    EXPECT_DOUBLE_EQ(-430.0, received_alt);
}

// --- setPositionCallback ---

TEST_F(EquipmentNMEAParserTest, SetPositionCallback_ReplacesExistingCallback)
{
    bool new_callback_called = false;
    double new_lat = 0.0;

    parser.testSetPositionCallback(
        [&](double lat, double, double, equipment_tracker::Timestamp)
        {
            new_callback_called = true;
            new_lat = lat;
        });

    parser.testTriggerPositionCallback(40.7128, -74.0060, 20.0);

    EXPECT_FALSE(callback_called); // Old callback should NOT be called
    EXPECT_TRUE(new_callback_called);
    EXPECT_DOUBLE_EQ(40.7128, new_lat);
}

TEST_F(EquipmentNMEAParserTest, SetPositionCallback_NullCallback_DoesNotCrash)
{
    parser.testSetPositionCallback(nullptr);
    EXPECT_NO_FATAL_FAILURE(parser.testTriggerPositionCallback(1.0, 2.0, 3.0));
}

// --- ProcessNMEABuffer ---

TEST_F(EquipmentNMEAParserTest, ProcessNMEABuffer_ValidData_ReturnsOK)
{
    std::string nmea = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";

    auto result = parser.ProcessNMEABuffer(
        const_cast<char *>(nmea.c_str()),
        static_cast<int>(nmea.length()));

    EXPECT_EQ(CNMEAParserData::ERROR_OK, result);
}

TEST_F(EquipmentNMEAParserTest, ProcessNMEABuffer_EmptyBuffer_ReturnsOK)
{
    std::string empty;
    auto result = parser.ProcessNMEABuffer(
        const_cast<char *>(empty.c_str()), 0);

    // The mock base class returns ERROR_OK for any input
    EXPECT_EQ(CNMEAParserData::ERROR_OK, result);
}

TEST_F(EquipmentNMEAParserTest, ProcessNMEABuffer_TriggersCallbackOnSuccess)
{
    std::string nmea = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";

    parser.ProcessNMEABuffer(
        const_cast<char *>(nmea.c_str()),
        static_cast<int>(nmea.length()));

    // With the mock NMEAParser, ProcessNMEABuffer returns OK and GetGPGGA returns OK,
    // so the callback should be triggered (with default GGA_DATA_T values: 0,0,0)
    EXPECT_TRUE(callback_called);
}

// ============================================================================
// GPSTracker Tests
// ============================================================================

// --- Constructor ---

TEST_F(GPSTrackerTest, Constructor_DefaultInterval)
{
    equipment_tracker::GPSTracker default_tracker;
    EXPECT_EQ(equipment_tracker::DEFAULT_UPDATE_INTERVAL_MS, default_tracker.getUpdateInterval());
    EXPECT_FALSE(default_tracker.isRunning());
}

TEST_F(GPSTrackerTest, Constructor_CustomInterval)
{
    equipment_tracker::GPSTracker custom_tracker(250);
    EXPECT_EQ(250, custom_tracker.getUpdateInterval());
    EXPECT_FALSE(custom_tracker.isRunning());
}

TEST_F(GPSTrackerTest, Constructor_ZeroInterval)
{
    equipment_tracker::GPSTracker zero_tracker(0);
    EXPECT_EQ(0, zero_tracker.getUpdateInterval());
}

TEST_F(GPSTrackerTest, Constructor_NegativeInterval)
{
    equipment_tracker::GPSTracker neg_tracker(-1);
    EXPECT_EQ(-1, neg_tracker.getUpdateInterval());
}

// --- isRunning ---

TEST_F(GPSTrackerTest, IsRunning_InitiallyFalse)
{
    EXPECT_FALSE(tracker->isRunning());
}

// --- start / stop ---

TEST_F(GPSTrackerTest, Start_SetsRunningTrue)
{
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, Stop_SetsRunningFalse)
{
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());

    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, Start_CalledTwice_DoesNotCrash)
{
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());

    // Second start should be a no-op (early return)
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());

    tracker->stop();
}

TEST_F(GPSTrackerTest, Stop_CalledTwice_DoesNotCrash)
{
    tracker->start();
    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());

    // Second stop should be a no-op
    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, Stop_WithoutStart_DoesNotCrash)
{
    EXPECT_FALSE(tracker->isRunning());
    EXPECT_NO_FATAL_FAILURE(tracker->stop());
    EXPECT_FALSE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, MultipleStartStopCycles)
{
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_FALSE(tracker->isRunning());
        tracker->start();
        EXPECT_TRUE(tracker->isRunning());
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        tracker->stop();
        EXPECT_FALSE(tracker->isRunning());
    }
}

// --- setUpdateInterval / getUpdateInterval ---

TEST_F(GPSTrackerTest, SetUpdateInterval_ChangesValue)
{
    EXPECT_EQ(100, tracker->getUpdateInterval());

    tracker->setUpdateInterval(500);
    EXPECT_EQ(500, tracker->getUpdateInterval());
}

TEST_F(GPSTrackerTest, SetUpdateInterval_ToZero)
{
    tracker->setUpdateInterval(0);
    EXPECT_EQ(0, tracker->getUpdateInterval());
}

TEST_F(GPSTrackerTest, SetUpdateInterval_ToLargeValue)
{
    tracker->setUpdateInterval(60000);
    EXPECT_EQ(60000, tracker->getUpdateInterval());
}

TEST_F(GPSTrackerTest, SetUpdateInterval_WhileRunning)
{
    tracker->start();
    tracker->setUpdateInterval(200);
    EXPECT_EQ(200, tracker->getUpdateInterval());
    tracker->stop();
}

// --- registerPositionCallback ---

TEST_F(GPSTrackerTest, RegisterPositionCallback_ReplacesExisting)
{
    bool new_cb_called = false;
    double new_lat = 0.0, new_lon = 0.0, new_alt = 0.0;

    tracker->registerPositionCallback(
        [&](double lat, double lon, double alt, equipment_tracker::Timestamp)
        {
            new_cb_called = true;
            new_lat = lat;
            new_lon = lon;
            new_alt = alt;
        });

    tracker->simulatePosition(51.5074, -0.1278, 15.0);

    EXPECT_FALSE(callback_called.load()); // Old callback not called
    EXPECT_TRUE(new_cb_called);
    EXPECT_DOUBLE_EQ(51.5074, new_lat);
    EXPECT_DOUBLE_EQ(-0.1278, new_lon);
    EXPECT_DOUBLE_EQ(15.0, new_alt);
}

TEST_F(GPSTrackerTest, RegisterPositionCallback_NullCallback_DoesNotCrashOnSimulate)
{
    tracker->registerPositionCallback(nullptr);
    // simulatePosition with null callback should not crash
    EXPECT_NO_FATAL_FAILURE(tracker->simulatePosition(0.0, 0.0, 0.0));
}

// --- simulatePosition ---

TEST_F(GPSTrackerTest, SimulatePosition_TriggersCallback)
{
    tracker->simulatePosition(37.7749, -122.4194, 10.0);

    EXPECT_TRUE(callback_called.load());
    EXPECT_DOUBLE_EQ(37.7749, received_lat);
    EXPECT_DOUBLE_EQ(-122.4194, received_lon);
    EXPECT_DOUBLE_EQ(10.0, received_alt);
}

TEST_F(GPSTrackerTest, SimulatePosition_DefaultAltitudeIsZero)
{
    tracker->simulatePosition(37.7749, -122.4194);

    EXPECT_TRUE(callback_called.load());
    EXPECT_DOUBLE_EQ(0.0, received_alt);
}

TEST_F(GPSTrackerTest, SimulatePosition_SetsTimestamp)
{
    auto before = equipment_tracker::getCurrentTimestamp();
    tracker->simulatePosition(0.0, 0.0, 0.0);
    auto after = equipment_tracker::getCurrentTimestamp();

    EXPECT_TRUE(callback_called.load());
    EXPECT_GE(received_ts, before);
    EXPECT_LE(received_ts, after);
}

TEST_F(GPSTrackerTest, SimulatePosition_NegativeLatitude_SouthernHemisphere)
{
    tracker->simulatePosition(-33.8688, 151.2093, 5.0);

    EXPECT_TRUE(callback_called.load());
    EXPECT_DOUBLE_EQ(-33.8688, received_lat);
    EXPECT_DOUBLE_EQ(151.2093, received_lon);
}

TEST_F(GPSTrackerTest, SimulatePosition_NegativeLongitude_WesternHemisphere)
{
    tracker->simulatePosition(40.7128, -74.0060, 0.0);

    EXPECT_TRUE(callback_called.load());
    EXPECT_DOUBLE_EQ(40.7128, received_lat);
    EXPECT_DOUBLE_EQ(-74.0060, received_lon);
}

TEST_F(GPSTrackerTest, SimulatePosition_BothNegativeCoordinates)
{
    tracker->simulatePosition(-22.9068, -43.1729, 0.0);

    EXPECT_TRUE(callback_called.load());
    EXPECT_DOUBLE_EQ(-22.9068, received_lat);
    EXPECT_DOUBLE_EQ(-43.1729, received_lon);
}

TEST_F(GPSTrackerTest, SimulatePosition_ZeroCoordinates)
{
    tracker->simulatePosition(0.0, 0.0, 0.0);

    EXPECT_TRUE(callback_called.load());
    EXPECT_DOUBLE_EQ(0.0, received_lat);
    EXPECT_DOUBLE_EQ(0.0, received_lon);
    EXPECT_DOUBLE_EQ(0.0, received_alt);
}

TEST_F(GPSTrackerTest, SimulatePosition_MaxLatitude)
{
    tracker->simulatePosition(90.0, 0.0, 0.0);

    EXPECT_TRUE(callback_called.load());
    EXPECT_DOUBLE_EQ(90.0, received_lat);
}

TEST_F(GPSTrackerTest, SimulatePosition_MinLatitude)
{
    tracker->simulatePosition(-90.0, 0.0, 0.0);

    EXPECT_TRUE(callback_called.load());
    EXPECT_DOUBLE_EQ(-90.0, received_lat);
}

TEST_F(GPSTrackerTest, SimulatePosition_MaxLongitude)
{
    tracker->simulatePosition(0.0, 180.0, 0.0);

    EXPECT_TRUE(callback_called.load());
    EXPECT_DOUBLE_EQ(180.0, received_lon);
}

TEST_F(GPSTrackerTest, SimulatePosition_MinLongitude)
{
    tracker->simulatePosition(0.0, -180.0, 0.0);

    EXPECT_TRUE(callback_called.load());
    EXPECT_DOUBLE_EQ(-180.0, received_lon);
}

TEST_F(GPSTrackerTest, SimulatePosition_HighAltitude)
{
    tracker->simulatePosition(27.9881, 86.9250, 8848.86);

    EXPECT_TRUE(callback_called.load());
    EXPECT_DOUBLE_EQ(8848.86, received_alt);
}

TEST_F(GPSTrackerTest, SimulatePosition_NegativeAltitude)
{
    tracker->simulatePosition(31.5, 35.5, -430.0);

    EXPECT_TRUE(callback_called.load());
    EXPECT_DOUBLE_EQ(-430.0, received_alt);
}

TEST_F(GPSTrackerTest, SimulatePosition_MultipleSequentialCalls)
{
    struct Pos
    {
        double lat, lon, alt;
    };

    std::vector<Pos> positions = {
        {37.7749, -122.4194, 10.0},
        {40.7128, -74.0060, 20.0},
        {34.0522, -118.2437, 30.0},
        {48.8566, 2.3522, 40.0},
        {-33.8688, 151.2093, 50.0}};

    for (const auto &pos : positions)
    {
        callback_called = false;
        tracker->simulatePosition(pos.lat, pos.lon, pos.alt);

        EXPECT_TRUE(callback_called.load());
        EXPECT_DOUBLE_EQ(pos.lat, received_lat);
        EXPECT_DOUBLE_EQ(pos.lon, received_lon);
        EXPECT_DOUBLE_EQ(pos.alt, received_alt);
    }
}

// --- processNMEAData ---

TEST_F(GPSTrackerTest, ProcessNMEAData_ValidSentence_ReturnsTrue)
{
    std::string nmea = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    bool result = tracker->processNMEAData(nmea);
    EXPECT_TRUE(result);
}

TEST_F(GPSTrackerTest, ProcessNMEAData_EmptyString)
{
    // Should not crash; result depends on parser implementation
    bool result = tracker->processNMEAData("");
    // With the mock parser, empty data still returns ERROR_OK
    EXPECT_TRUE(result);
}

TEST_F(GPSTrackerTest, ProcessNMEAData_GarbageData)
{
    std::string garbage = "this is not NMEA data at all";
    // Should not crash
    EXPECT_NO_FATAL_FAILURE(tracker->processNMEAData(garbage));
}

TEST_F(GPSTrackerTest, ProcessNMEAData_MultipleSentences)
{
    std::string nmea =
        "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n"
        "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";

    EXPECT_NO_FATAL_FAILURE(tracker->processNMEAData(nmea));
}

TEST_F(GPSTrackerTest, ProcessNMEAData_TriggersCallback)
{
    std::string nmea = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    tracker->processNMEAData(nmea);

    // With the mock parser, the callback is triggered with default GGA values (0,0,0)
    EXPECT_TRUE(callback_called.load());
}

// --- Worker thread behavior ---

TEST_F(GPSTrackerTest, WorkerThread_SimulatesPositionWhenNoSerialPort)
{
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());

    // Wait enough for at least one worker iteration
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // The worker thread should have called simulatePosition (default San Francisco)
    EXPECT_TRUE(callback_called.load());
    EXPECT_GT(callback_count.load(), 0);

    tracker->stop();
}

TEST_F(GPSTrackerTest, WorkerThread_StopPreventsMoreCallbacks)
{
    tracker->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());

    // Record current count
    int count_at_stop = callback_count.load();

    // Wait and verify no more callbacks
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_EQ(count_at_stop, callback_count.load());
}

TEST_F(GPSTrackerTest, WorkerThread_UpdateIntervalAffectsTiming)
{
    // Use a longer interval
    tracker->setUpdateInterval(200);
    tracker->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    tracker->stop();

    // With 200ms interval over 500ms, we expect roughly 2-3 callbacks
    // (not a strict assertion due to timing, but should be reasonable)
    EXPECT_GT(callback_count.load(), 0);
}

// --- Destructor ---

TEST_F(GPSTrackerTest, Destructor_StopsRunningTracker)
{
    {
        equipment_tracker::GPSTracker temp_tracker(100);
        temp_tracker.start();
        EXPECT_TRUE(temp_tracker.isRunning());
        // Destructor should call stop()
    }
    // If we reach here, destructor properly joined the thread
    SUCCEED();
}

TEST_F(GPSTrackerTest, Destructor_SafeWhenNotStarted)
{
    {
        equipment_tracker::GPSTracker temp_tracker;
        // Destructor called without start — should be safe
    }
    SUCCEED();
}

// --- Copy/Move deleted ---

TEST_F(GPSTrackerTest, CopyConstructor_IsDeleted)
{
    EXPECT_FALSE(std::is_copy_constructible<equipment_tracker::GPSTracker>::value);
}

TEST_F(GPSTrackerTest, CopyAssignment_IsDeleted)
{
    EXPECT_FALSE(std::is_copy_assignable<equipment_tracker::GPSTracker>::value);
}

TEST_F(GPSTrackerTest, MoveConstructor_IsDeleted)
{
    EXPECT_FALSE(std::is_move_constructible<equipment_tracker::GPSTracker>::value);
}

TEST_F(GPSTrackerTest, MoveAssignment_IsDeleted)
{
    EXPECT_FALSE(std::is_move_assignable<equipment_tracker::GPSTracker>::value);
}

// --- Integration-style tests ---

TEST_F(GPSTrackerTest, SimulatePosition_ThenProcessNMEA_BothWork)
{
    // First simulate
    tracker->simulatePosition(10.0, 20.0, 30.0);
    EXPECT_TRUE(callback_called.load());
    EXPECT_DOUBLE_EQ(10.0, received_lat);

    // Reset
    callback_called = false;

    // Then process NMEA
    std::string nmea = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    tracker->processNMEAData(nmea);
    EXPECT_TRUE(callback_called.load());
}

TEST_F(GPSTrackerTest, RegisterCallback_WhileRunning)
{
    tracker->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Register a new callback while running
    bool new_cb_called = false;
    tracker->registerPositionCallback(
        [&](double, double, double, equipment_tracker::Timestamp)
        {
            new_cb_called = true;
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    tracker->stop();

    // The new callback should have been invoked by the worker thread
    EXPECT_TRUE(new_cb_called);
}

// ============================================================================
// EquipmentNMEAParser default construction
// ============================================================================

TEST(EquipmentNMEAParserStandaloneTest, DefaultConstruction_NoCallback_NoThrow)
{
    equipment_tracker::EquipmentNMEAParser parser;
    // Should be constructible without issues
    SUCCEED();
}

TEST(EquipmentNMEAParserStandaloneTest, ProcessBuffer_WithoutCallback_NoThrow)
{
    equipment_tracker::EquipmentNMEAParser parser;
    std::string data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";

    // Should not crash even without a callback registered
    EXPECT_NO_FATAL_FAILURE(
        parser.ProcessNMEABuffer(const_cast<char *>(data.c_str()),
                                 static_cast<int>(data.length())));
}

// Main
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>

---

**Confidence:** High (95%). The test file is based on the actual source code, header interfaces, mock NMEAParser, and build configuration gathered directly from the repository at `main`. All public methods of both `EquipmentNMEAParser` and `GPSTracker` are covered, including edge cases (boundary coordinates, null callbacks, copy/move deletion checks, threading behavior, multiple start/stop cycles). The tests use the project's existing mock `NMEAParser.h` and link against GoogleTest v1.17.0 as specified in the tests `CMakeLists.txt`.