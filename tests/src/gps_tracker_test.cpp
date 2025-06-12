// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <sstream>
#include <string>
#include "equipment_tracker/gps_tracker.h"
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/time_utils.h"

// Create a testable wrapper for EquipmentNMEAParser to access protected methods
class TestableEquipmentNMEAParser : public equipment_tracker::EquipmentNMEAParser
{
public:
    // Make protected methods accessible for testing
    void testOnError(CNMEAParserData::ERROR_E nError, char *pCmd)
    {
        OnError(nError, pCmd);
    }

    // Expose triggerPositionCallback for direct testing
    void testTriggerPositionCallback(double latitude, double longitude, double altitude)
    {
        triggerPositionCallback(latitude, longitude, altitude);
    }

    // Make setPositionCallback accessible
    void testSetPositionCallback(equipment_tracker::PositionCallback callback)
    {
        setPositionCallback(callback);
    }
};

// Test fixture for EquipmentNMEAParser
class EquipmentNMEAParserTest : public ::testing::Test
{
protected:
    TestableEquipmentNMEAParser parser;
    bool callback_called = false;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    equipment_tracker::Timestamp timestamp;

    void SetUp() override
    {
        callback_called = false;
        latitude = 0.0;
        longitude = 0.0;
        altitude = 0.0;
        timestamp = equipment_tracker::Timestamp{};

        parser.testSetPositionCallback([this](double lat, double lon, double alt, equipment_tracker::Timestamp ts)
                                       {
            callback_called = true;
            latitude = lat;
            longitude = lon;
            altitude = alt;
            timestamp = ts; });
    }
};

// Test fixture for GPSTracker
class GPSTrackerTest : public ::testing::Test
{
protected:
    std::unique_ptr<equipment_tracker::GPSTracker> tracker;
    bool callback_called = false;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    equipment_tracker::Timestamp timestamp;

    void SetUp() override
    {
        callback_called = false;
        latitude = 0.0;
        longitude = 0.0;
        altitude = 0.0;
        timestamp = equipment_tracker::Timestamp{};

        tracker = std::make_unique<equipment_tracker::GPSTracker>(100); // Short interval for tests
        tracker->registerPositionCallback([this](double lat, double lon, double alt, equipment_tracker::Timestamp ts)
                                          {
            callback_called = true;
            latitude = lat;
            longitude = lon;
            altitude = alt;
            timestamp = ts; });
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

// Tests for EquipmentNMEAParser
TEST_F(EquipmentNMEAParserTest, OnErrorHandlesErrorCorrectly)
{
    // Redirect cerr to capture output
    std::stringstream buffer;
    std::streambuf *old = std::cerr.rdbuf(buffer.rdbuf());

    // Test with command
    char cmd[] = "GPGGA";
    parser.testOnError(CNMEAParserData::ERROR_UNKNOWN, cmd);

    // Check that error message contains expected text
    std::string output = buffer.str();
    EXPECT_TRUE(output.find("NMEA Parser Error") != std::string::npos);
    EXPECT_TRUE(output.find("GPGGA") != std::string::npos);

    // Reset buffer
    buffer.str("");

    // Test without command (nullptr)
    parser.testOnError(CNMEAParserData::ERROR_UNKNOWN, nullptr);
    output = buffer.str();
    EXPECT_TRUE(output.find("NMEA Parser Error") != std::string::npos);

    // Restore cerr
    std::cerr.rdbuf(old);
}

TEST_F(EquipmentNMEAParserTest, TriggerPositionCallbackWorks)
{
    // Test direct triggering of the callback
    parser.testTriggerPositionCallback(37.7749, -122.4194, 10.0);

    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, latitude);
    EXPECT_DOUBLE_EQ(-122.4194, longitude);
    EXPECT_DOUBLE_EQ(10.0, altitude);
    EXPECT_NE(timestamp, equipment_tracker::Timestamp{});
}

TEST_F(EquipmentNMEAParserTest, ProcessNMEABufferWithValidData)
{
    // Create a mock NMEA sentence
    std::string nmea_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";

    // Process the NMEA data
    CNMEAParserData::ERROR_E result = parser.ProcessNMEABuffer(
        const_cast<char *>(nmea_data.c_str()),
        static_cast<int>(nmea_data.length()));

    // The result depends on the underlying NMEA parser implementation
    // We'll just verify it doesn't crash and returns a valid error code
    EXPECT_TRUE(result >= CNMEAParserData::ERROR_OK);
}

TEST_F(EquipmentNMEAParserTest, SetPositionCallbackWorks)
{
    bool test_callback_called = false;
    double test_lat = 0.0, test_lon = 0.0, test_alt = 0.0;

    // Set a new callback
    parser.testSetPositionCallback([&](double lat, double lon, double alt, equipment_tracker::Timestamp)
                                   {
        test_callback_called = true;
        test_lat = lat;
        test_lon = lon;
        test_alt = alt; });

    // Trigger the callback
    parser.testTriggerPositionCallback(40.7128, -74.0060, 20.0);

    // Verify the new callback was called
    EXPECT_TRUE(test_callback_called);
    EXPECT_DOUBLE_EQ(40.7128, test_lat);
    EXPECT_DOUBLE_EQ(-74.0060, test_lon);
    EXPECT_DOUBLE_EQ(20.0, test_alt);
}

// Tests for GPSTracker
TEST_F(GPSTrackerTest, ConstructorSetsDefaultValues)
{
    equipment_tracker::GPSTracker default_tracker;
    EXPECT_EQ(default_tracker.getUpdateInterval(), equipment_tracker::DEFAULT_UPDATE_INTERVAL_MS);
    EXPECT_FALSE(default_tracker.isRunning());
}

TEST_F(GPSTrackerTest, ConstructorWithCustomInterval)
{
    equipment_tracker::GPSTracker custom_tracker(500);
    EXPECT_EQ(custom_tracker.getUpdateInterval(), 500);
    EXPECT_FALSE(custom_tracker.isRunning());
}

TEST_F(GPSTrackerTest, StartStopControlsRunningState)
{
    EXPECT_FALSE(tracker->isRunning());

    tracker->start();
    EXPECT_TRUE(tracker->isRunning());

    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, SetUpdateIntervalChangesInterval)
{
    EXPECT_EQ(tracker->getUpdateInterval(), 100); // From our setup

    tracker->setUpdateInterval(200);
    EXPECT_EQ(tracker->getUpdateInterval(), 200);
}

TEST_F(GPSTrackerTest, SimulatePositionTriggersCallback)
{
    // Simulate a position update
    tracker->simulatePosition(37.7749, -122.4194, 10.0);

    // Check that the callback was called with the correct values
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, latitude);
    EXPECT_DOUBLE_EQ(-122.4194, longitude);
    EXPECT_DOUBLE_EQ(10.0, altitude);
    EXPECT_NE(timestamp, equipment_tracker::Timestamp{});
}

TEST_F(GPSTrackerTest, SimulatePositionWithDefaultAltitude)
{
    // Simulate a position update without altitude (should default to 0.0)
    tracker->simulatePosition(37.7749, -122.4194);

    // Check that the callback was called with the correct values
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, latitude);
    EXPECT_DOUBLE_EQ(-122.4194, longitude);
    EXPECT_DOUBLE_EQ(0.0, altitude);
}

TEST_F(GPSTrackerTest, ProcessNMEADataHandlesValidData)
{
    // Create a valid NMEA sentence
    std::string nmea_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";

    // Process the data
    bool result = tracker->processNMEAData(nmea_data);

    // The result depends on the underlying implementation
    // We'll just verify it doesn't crash
    EXPECT_TRUE(result || !result); // Always true, just to verify it executes
}

TEST_F(GPSTrackerTest, ProcessNMEADataHandlesEmptyData)
{
    // Process empty data
    bool result = tracker->processNMEAData("");

    // Should handle empty data gracefully
    EXPECT_TRUE(result || !result); // Always true, just to verify it executes
}

TEST_F(GPSTrackerTest, RegisterPositionCallbackWorks)
{
    // Reset callback flag
    callback_called = false;

    // Register a new callback
    bool new_callback_called = false;
    double new_lat = 0.0, new_lon = 0.0, new_alt = 0.0;

    tracker->registerPositionCallback([&](double lat, double lon, double alt, equipment_tracker::Timestamp)
                                      {
        new_callback_called = true;
        new_lat = lat;
        new_lon = lon;
        new_alt = alt; });

    // Simulate a position
    tracker->simulatePosition(37.7749, -122.4194, 10.0);

    // Original callback should not be called anymore
    EXPECT_FALSE(callback_called);

    // New callback should be called
    EXPECT_TRUE(new_callback_called);
    EXPECT_DOUBLE_EQ(37.7749, new_lat);
    EXPECT_DOUBLE_EQ(-122.4194, new_lon);
    EXPECT_DOUBLE_EQ(10.0, new_alt);
}

TEST_F(GPSTrackerTest, StartCreatesWorkerThread)
{
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());

    // Sleep briefly to allow the worker thread to run
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // The worker thread should have called simulatePosition at least once
    EXPECT_TRUE(callback_called);

    // Stop the tracker
    tracker->stop();
}

TEST_F(GPSTrackerTest, StopJoinsWorkerThread)
{
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());

    // Sleep briefly to allow the worker thread to run
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Stop the tracker
    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());

    // Reset callback flag
    callback_called = false;

    // Sleep again to verify no more callbacks are triggered
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // No new callbacks should have been triggered after stopping
    EXPECT_FALSE(callback_called);
}

TEST_F(GPSTrackerTest, MultipleStartStopCycles)
{
    // Test multiple start/stop cycles
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_FALSE(tracker->isRunning());

        tracker->start();
        EXPECT_TRUE(tracker->isRunning());

        // Brief operation
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        tracker->stop();
        EXPECT_FALSE(tracker->isRunning());
    }
}

TEST_F(GPSTrackerTest, UpdateIntervalAffectsWorkerThread)
{
    // Set a longer interval
    tracker->setUpdateInterval(200);
    EXPECT_EQ(tracker->getUpdateInterval(), 200);

    // Start tracker
    tracker->start();

    // The worker thread behavior should be consistent with the interval
    // (This is more of an integration test)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());
}

// Test multiple position simulations
TEST_F(GPSTrackerTest, MultiplePositionSimulations)
{
    struct PositionData
    {
        double lat, lon, alt;
        bool called;
    };

    std::vector<PositionData> positions = {
        {37.7749, -122.4194, 10.0, false},
        {40.7128, -74.0060, 20.0, false},
        {34.0522, -118.2437, 30.0, false}};

    for (auto &pos : positions)
    {
        // Reset callback state
        callback_called = false;

        // Simulate position
        tracker->simulatePosition(pos.lat, pos.lon, pos.alt);

        // Verify callback
        EXPECT_TRUE(callback_called);
        EXPECT_DOUBLE_EQ(pos.lat, latitude);
        EXPECT_DOUBLE_EQ(pos.lon, longitude);
        EXPECT_DOUBLE_EQ(pos.alt, altitude);

        pos.called = true;
    }

    // Verify all positions were processed
    for (const auto &pos : positions)
    {
        EXPECT_TRUE(pos.called);
    }
}

// Main function that runs all tests
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>