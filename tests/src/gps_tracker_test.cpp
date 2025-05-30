// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <string>
#include <sstream>
#include <cstring>
#include <iomanip>
#include "equipment_tracker/gps_tracker.h"
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/time_utils.h"

namespace equipment_tracker {

// Mock for the CNMEAParser to test EquipmentNMEAParser
class MockNMEAParser : public CNMEAParser {
public:
    MOCK_METHOD(CNMEAParserData::ERROR_E, ProcessNMEABuffer, (char* pBuffer, int iSize), (override));
    MOCK_METHOD(CNMEAParserData::ERROR_E, GetGPGGA, (CNMEAParserData::GGA_DATA_T& ggaData), (override));
    MOCK_METHOD(void, OnError, (CNMEAParserData::ERROR_E nError, char* pCmd), (override));
    MOCK_METHOD(void, LockDataAccess, (), (override));
    MOCK_METHOD(void, UnlockDataAccess, (), (override));
};

// Test fixture for EquipmentNMEAParser
class EquipmentNMEAParserTest : public ::testing::Test {
protected:
    EquipmentNMEAParser parser;
    bool callback_called = false;
    double callback_lat = 0.0;
    double callback_lon = 0.0;
    double callback_alt = 0.0;
    Timestamp callback_timestamp;

    void SetUp() override {
        parser.setPositionCallback([this](double lat, double lon, double alt, Timestamp ts) {
            callback_called = true;
            callback_lat = lat;
            callback_lon = lon;
            callback_alt = alt;
            callback_timestamp = ts;
        });
    }
};

// Test fixture for GPSTracker
class GPSTrackerTest : public ::testing::Test {
protected:
    std::unique_ptr<GPSTracker> tracker;
    bool callback_called = false;
    double callback_lat = 0.0;
    double callback_lon = 0.0;
    double callback_alt = 0.0;
    Timestamp callback_timestamp;

    void SetUp() override {
        tracker = std::make_unique<GPSTracker>(100); // Short interval for tests
        tracker->registerPositionCallback([this](double lat, double lon, double alt, Timestamp ts) {
            callback_called = true;
            callback_lat = lat;
            callback_lon = lon;
            callback_alt = alt;
            callback_timestamp = ts;
        });
    }

    void TearDown() override {
        if (tracker && tracker->isRunning()) {
            tracker->stop();
        }
        tracker.reset();
    }
};

// Tests for EquipmentNMEAParser
TEST_F(EquipmentNMEAParserTest, TriggerPositionCallback) {
    // Test that the position callback is triggered correctly
    parser.triggerPositionCallback(37.7749, -122.4194, 10.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, callback_lat);
    EXPECT_DOUBLE_EQ(-122.4194, callback_lon);
    EXPECT_DOUBLE_EQ(10.0, callback_alt);
}

TEST_F(EquipmentNMEAParserTest, ProcessNMEABuffer) {
    // Create a valid NMEA GGA sentence
    std::string nmea_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    // Create a mock GGA data structure to be returned by GetGPGGA
    CNMEAParserData::GGA_DATA_T ggaData;
    ggaData.dLatitude = 48.1173;
    ggaData.dLongitude = 11.5167;
    ggaData.dAltitudeMSL = 545.4;
    
    // Process the buffer and check if callback is triggered
    auto result = parser.ProcessNMEABuffer(const_cast<char*>(nmea_data.c_str()), static_cast<int>(nmea_data.length()));
    
    // The base implementation is mocked, so we don't expect the callback to be triggered
    // But we can verify the function returns the expected result
    EXPECT_EQ(CNMEAParserData::ERROR_OK, result);
}

// Tests for GPSTracker
TEST_F(GPSTrackerTest, Constructor) {
    // Test default constructor
    GPSTracker tracker_default;
    EXPECT_EQ(DEFAULT_UPDATE_INTERVAL_MS, tracker_default.getUpdateInterval());
    EXPECT_FALSE(tracker_default.isRunning());
    
    // Test constructor with custom interval
    GPSTracker tracker_custom(1000);
    EXPECT_EQ(1000, tracker_custom.getUpdateInterval());
    EXPECT_FALSE(tracker_custom.isRunning());
}

TEST_F(GPSTrackerTest, StartStop) {
    // Test start
    EXPECT_FALSE(tracker->isRunning());
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());
    
    // Test stop
    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());
    
    // Test starting an already running tracker (should be a no-op)
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());
    tracker->start(); // Should be a no-op
    EXPECT_TRUE(tracker->isRunning());
    
    // Test stopping an already stopped tracker (should be a no-op)
    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());
    tracker->stop(); // Should be a no-op
    EXPECT_FALSE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, SetUpdateInterval) {
    EXPECT_EQ(100, tracker->getUpdateInterval()); // From setup
    
    tracker->setUpdateInterval(500);
    EXPECT_EQ(500, tracker->getUpdateInterval());
    
    tracker->setUpdateInterval(0);
    EXPECT_EQ(0, tracker->getUpdateInterval());
}

TEST_F(GPSTrackerTest, SimulatePosition) {
    // Test simulating a position
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, callback_lat);
    EXPECT_DOUBLE_EQ(-122.4194, callback_lon);
    EXPECT_DOUBLE_EQ(10.0, callback_alt);
    
    // Reset and test with different values
    callback_called = false;
    tracker->simulatePosition(40.7128, -74.0060, 20.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(40.7128, callback_lat);
    EXPECT_DOUBLE_EQ(-74.0060, callback_lon);
    EXPECT_DOUBLE_EQ(20.0, callback_alt);
}

TEST_F(GPSTrackerTest, ProcessNMEAData) {
    // Create a valid NMEA GGA sentence
    std::string nmea_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    // Process the NMEA data
    bool result = tracker->processNMEAData(nmea_data);
    
    // Since we're using a mock NMEA parser, we expect this to succeed
    EXPECT_TRUE(result);
}

TEST_F(GPSTrackerTest, WorkerThreadSimulation) {
    // Start the tracker
    tracker->start();
    
    // Wait for the worker thread to run at least once
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Stop the tracker
    tracker->stop();
    
    // Check that the callback was called (worker thread should have simulated a position)
    EXPECT_TRUE(callback_called);
    
    // The default simulation is San Francisco
    EXPECT_DOUBLE_EQ(37.7749, callback_lat);
    EXPECT_DOUBLE_EQ(-122.4194, callback_lon);
    EXPECT_DOUBLE_EQ(10.0, callback_alt);
}

TEST_F(GPSTrackerTest, RegisterPositionCallback) {
    // Reset the callback flag
    callback_called = false;
    
    // Register a new callback
    bool new_callback_called = false;
    double new_lat = 0.0, new_lon = 0.0, new_alt = 0.0;
    Timestamp new_timestamp;
    
    tracker->registerPositionCallback([&](double lat, double lon, double alt, Timestamp ts) {
        new_callback_called = true;
        new_lat = lat;
        new_lon = lon;
        new_alt = alt;
        new_timestamp = ts;
    });
    
    // Simulate a position
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    
    // Check that the new callback was called
    EXPECT_TRUE(new_callback_called);
    EXPECT_DOUBLE_EQ(37.7749, new_lat);
    EXPECT_DOUBLE_EQ(-122.4194, new_lon);
    EXPECT_DOUBLE_EQ(10.0, new_alt);
    
    // Check that the old callback was not called
    EXPECT_FALSE(callback_called);
}

// Test NMEA sentence generation in simulatePosition
TEST_F(GPSTrackerTest, NMEASentenceGeneration) {
    // Create a stringstream to capture cerr output
    std::stringstream buffer;
    std::streambuf* old = std::cerr.rdbuf(buffer.rdbuf());
    
    // Simulate a position with known values
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    
    // Restore cerr
    std::cerr.rdbuf(old);
    
    // Check that no errors were reported
    EXPECT_TRUE(buffer.str().empty());
    
    // Verify the callback was called with correct values
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, callback_lat);
    EXPECT_DOUBLE_EQ(-122.4194, callback_lon);
    EXPECT_DOUBLE_EQ(10.0, callback_alt);
}

// Test error handling in EquipmentNMEAParser
TEST(EquipmentNMEAParserErrorTest, OnError) {
    EquipmentNMEAParser parser;
    
    // Create a stringstream to capture cerr output
    std::stringstream buffer;
    std::streambuf* old = std::cerr.rdbuf(buffer.rdbuf());
    
    // Test OnError with null command
    parser.OnError(CNMEAParserData::ERROR_UNKNOWN, nullptr);
    
    // Test OnError with a command
    char cmd[] = "GPGGA";
    parser.OnError(CNMEAParserData::ERROR_UNKNOWN, cmd);
    
    // Restore cerr
    std::cerr.rdbuf(old);
    
    // Check that error messages were output to cerr
    std::string output = buffer.str();
    EXPECT_THAT(output, ::testing::HasSubstr("NMEA Parser Error: 1"));
    EXPECT_THAT(output, ::testing::HasSubstr("for command: GPGGA"));
}

// Test mutex operations in EquipmentNMEAParser
TEST(EquipmentNMEAParserMutexTest, LockUnlock) {
    EquipmentNMEAParser parser;
    
    // These methods just call mutex lock/unlock, so we're mainly testing that they don't crash
    parser.LockDataAccess();
    parser.UnlockDataAccess();
    
    // No assertion needed - if the test completes without crashing, it passes
}

}  // namespace equipment_tracker
// </test_code>