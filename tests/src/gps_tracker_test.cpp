// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <sstream>
#include <cstring>
#include "equipment_tracker/gps_tracker.h"
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/time_utils.h"

namespace equipment_tracker {

// Mock for the NMEAParser to test interactions
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
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    Timestamp timestamp;

    void SetUp() override {
        parser.setPositionCallback([this](double lat, double lon, double alt, Timestamp ts) {
            callback_called = true;
            latitude = lat;
            longitude = lon;
            altitude = alt;
            timestamp = ts;
        });
    }
};

// Test fixture for GPSTracker
class GPSTrackerTest : public ::testing::Test {
protected:
    std::unique_ptr<GPSTracker> tracker;
    bool callback_called = false;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    Timestamp timestamp;

    void SetUp() override {
        tracker = std::make_unique<GPSTracker>(100); // Short interval for testing
        tracker->registerPositionCallback([this](double lat, double lon, double alt, Timestamp ts) {
            callback_called = true;
            latitude = lat;
            longitude = lon;
            altitude = alt;
            timestamp = ts;
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
    EXPECT_DOUBLE_EQ(37.7749, latitude);
    EXPECT_DOUBLE_EQ(-122.4194, longitude);
    EXPECT_DOUBLE_EQ(10.0, altitude);
    // We can't test exact timestamp, but it should be close to now
    auto now = getCurrentTimestamp();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - timestamp).count();
    EXPECT_LT(std::abs(diff), 2); // Within 2 seconds
}

TEST_F(EquipmentNMEAParserTest, ProcessNMEABuffer) {
    // Create a valid NMEA GGA sentence
    std::string nmea = "$GPGGA,123519.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    // Mock the GetGPGGA method to return valid data
    CNMEAParserData::GGA_DATA_T ggaData;
    ggaData.dLatitude = 48.1173;
    ggaData.dLongitude = 11.5167;
    ggaData.dAltitudeMSL = 545.4;
    
    // Process the buffer
    auto result = parser.ProcessNMEABuffer(const_cast<char*>(nmea.c_str()), static_cast<int>(nmea.length()));
    
    // Since we're using a mock NMEA parser, we can't fully test the processing
    // But we can verify the return value
    EXPECT_EQ(CNMEAParserData::ERROR_OK, result);
}

// Tests for GPSTracker
TEST_F(GPSTrackerTest, Constructor) {
    // Test default constructor
    GPSTracker tracker;
    EXPECT_EQ(DEFAULT_UPDATE_INTERVAL_MS, tracker.getUpdateInterval());
    EXPECT_FALSE(tracker.isRunning());
    
    // Test constructor with custom interval
    GPSTracker tracker2(1000);
    EXPECT_EQ(1000, tracker2.getUpdateInterval());
    EXPECT_FALSE(tracker2.isRunning());
}

TEST_F(GPSTrackerTest, StartStop) {
    // Test start
    EXPECT_FALSE(tracker->isRunning());
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());
    
    // Test stop
    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());
    
    // Test starting when already started
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());
    tracker->start(); // Should be a no-op
    EXPECT_TRUE(tracker->isRunning());
    
    // Test stopping when already stopped
    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());
    tracker->stop(); // Should be a no-op
    EXPECT_FALSE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, SetUpdateInterval) {
    EXPECT_EQ(100, tracker->getUpdateInterval());
    tracker->setUpdateInterval(500);
    EXPECT_EQ(500, tracker->getUpdateInterval());
}

TEST_F(GPSTrackerTest, SimulatePosition) {
    // Test simulating a position
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, latitude);
    EXPECT_DOUBLE_EQ(-122.4194, longitude);
    EXPECT_DOUBLE_EQ(10.0, altitude);
    
    // Reset and test with different values
    callback_called = false;
    tracker->simulatePosition(40.7128, -74.0060, 20.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(40.7128, latitude);
    EXPECT_DOUBLE_EQ(-74.0060, longitude);
    EXPECT_DOUBLE_EQ(20.0, altitude);
}

TEST_F(GPSTrackerTest, ProcessNMEAData) {
    // Create a valid NMEA GGA sentence
    std::string nmea = "$GPGGA,123519.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    // Process the NMEA data
    bool result = tracker->processNMEAData(nmea);
    
    // Since we're using a mock NMEA parser, we can't fully test the processing
    // But we can verify the return value
    EXPECT_TRUE(result);
}

TEST_F(GPSTrackerTest, RegisterPositionCallback) {
    // Reset the callback
    bool new_callback_called = false;
    double new_lat = 0.0, new_lon = 0.0, new_alt = 0.0;
    Timestamp new_ts;
    
    // Register a new callback
    tracker->registerPositionCallback([&](double lat, double lon, double alt, Timestamp ts) {
        new_callback_called = true;
        new_lat = lat;
        new_lon = lon;
        new_alt = alt;
        new_ts = ts;
    });
    
    // Simulate a position to trigger the callback
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    
    // Verify the new callback was called
    EXPECT_TRUE(new_callback_called);
    EXPECT_DOUBLE_EQ(37.7749, new_lat);
    EXPECT_DOUBLE_EQ(-122.4194, new_lon);
    EXPECT_DOUBLE_EQ(10.0, new_alt);
}

TEST_F(GPSTrackerTest, WorkerThreadFunction) {
    // Start the tracker
    tracker->start();
    
    // Wait for the worker thread to run at least once
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Verify that the callback was called (worker thread should simulate a position)
    EXPECT_TRUE(callback_called);
    
    // Stop the tracker
    tracker->stop();
}

// Test the NMEA sentence generation in simulatePosition
TEST_F(GPSTrackerTest, NMEASentenceGeneration) {
    // Create a GPSTracker with a mock NMEA parser
    class TestableGPSTracker : public GPSTracker {
    public:
        TestableGPSTracker() : GPSTracker(100) {}
        
        // Expose the processNMEAData method for testing
        using GPSTracker::processNMEAData;
    };
    
    TestableGPSTracker testTracker;
    
    // Register a callback to capture the processed data
    bool callback_called = false;
    testTracker.registerPositionCallback([&](double lat, double lon, double alt, Timestamp) {
        callback_called = true;
        EXPECT_NEAR(37.7749, lat, 0.0001);
        EXPECT_NEAR(-122.4194, lon, 0.0001);
        EXPECT_NEAR(10.0, alt, 0.0001);
    });
    
    // Simulate a position
    testTracker.simulatePosition(37.7749, -122.4194, 10.0);
    
    // Verify the callback was called
    EXPECT_TRUE(callback_called);
}

// Test error handling in EquipmentNMEAParser
TEST_F(EquipmentNMEAParserTest, OnError) {
    // Redirect cerr to capture output
    std::stringstream buffer;
    std::streambuf* old = std::cerr.rdbuf(buffer.rdbuf());
    
    // Test with null command
    parser.OnError(CNMEAParserData::ERROR_UNKNOWN, nullptr);
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("NMEA Parser Error: 1"));
    
    // Clear buffer and test with a command
    buffer.str("");
    char cmd[] = "$GPGGA";
    parser.OnError(CNMEAParserData::ERROR_UNKNOWN, cmd);
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("NMEA Parser Error: 1 for command: $GPGGA"));
    
    // Restore cerr
    std::cerr.rdbuf(old);
}

// Test thread safety in EquipmentNMEAParser
TEST_F(EquipmentNMEAParserTest, ThreadSafety) {
    // Test that LockDataAccess and UnlockDataAccess work
    // This is a basic test that doesn't actually verify thread safety
    // but at least ensures the methods don't crash
    parser.LockDataAccess();
    parser.UnlockDataAccess();
    
    // A more thorough test would involve multiple threads
    // but that's beyond the scope of this unit test
}

}  // namespace equipment_tracker
// </test_code>