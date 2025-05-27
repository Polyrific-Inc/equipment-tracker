// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <iostream>
#include <chrono>
#include <cstring>
#include <sstream>
#include <thread>
#include <mutex>
#include "equipment_tracker/gps_tracker.h"
#include "equipment_tracker/position.h"

// Mock for CNMEAParserData
class CNMEAParserData {
public:
    enum ERROR_E {
        ERROR_OK = 0,
        ERROR_GENERIC
    };

    struct GGA_DATA_T {
        double dLatitude;
        double dLongitude;
        double dAltitudeMSL;
    };
};

// Mock for EquipmentNMEAParser
namespace equipment_tracker {
class EquipmentNMEAParser : public CNMEAParserData {
public:
    MOCK_METHOD(ERROR_E, ProcessNMEABuffer, (char*, int));
    MOCK_METHOD(ERROR_E, GetGPGGA, (GGA_DATA_T&));
    MOCK_METHOD(void, OnError, (CNMEAParserData::ERROR_E, char*));
    MOCK_METHOD(void, LockDataAccess, ());
    MOCK_METHOD(void, UnlockDataAccess, ());
    MOCK_METHOD(void, setPositionCallback, (std::function<void(double, double, double)>));
};
}

// Mock for the NMEA parser
class MockNMEAParser : public equipment_tracker::EquipmentNMEAParser {
public:
    MOCK_METHOD(ERROR_E, ProcessNMEABuffer, (char*, int), (override));
    MOCK_METHOD(ERROR_E, GetGPGGA, (GGA_DATA_T&), (override));
    MOCK_METHOD(void, setPositionCallback, (std::function<void(double, double, double)>), (override));
};

// Test fixture for GPSTracker
class GPSTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_nmea_parser_ = new MockNMEAParser();
    }

    void TearDown() override {
        // Cleanup
    }

    MockNMEAParser* mock_nmea_parser_;
};

// Helper function to get current timestamp
std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buffer);
}

// Test constructor and default values
TEST_F(GPSTrackerTest, Constructor) {
    equipment_tracker::GPSTracker tracker(1000);
    EXPECT_FALSE(tracker.is_running());
}

// Test start and stop methods
TEST_F(GPSTrackerTest, StartStop) {
    equipment_tracker::GPSTracker tracker(1000);
    
    // Start the tracker
    tracker.start();
    EXPECT_TRUE(tracker.is_running());
    
    // Stop the tracker
    tracker.stop();
    EXPECT_FALSE(tracker.is_running());
}

// Test setting update interval
TEST_F(GPSTrackerTest, SetUpdateInterval) {
    equipment_tracker::GPSTracker tracker(1000);
    
    // Change the update interval
    tracker.setUpdateInterval(2000);
    EXPECT_EQ(tracker.getUpdateInterval(), 2000);
}

// Test position callback registration
TEST_F(GPSTrackerTest, RegisterPositionCallback) {
    equipment_tracker::GPSTracker tracker(1000);
    
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    std::string timestamp;
    
    tracker.registerPositionCallback([&](double latitude, double longitude, double altitude, const std::string& time) {
        callback_called = true;
        lat = latitude;
        lon = longitude;
        alt = altitude;
        timestamp = time;
    });
    
    // Simulate a position update
    tracker.simulatePosition(37.7749, -122.4194, 10.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_NEAR(lat, 37.7749, 0.0001);
    EXPECT_NEAR(lon, -122.4194, 0.0001);
    EXPECT_NEAR(alt, 10.0, 0.0001);
    EXPECT_FALSE(timestamp.empty());
}

// Test position simulation with NMEA data generation
TEST_F(GPSTrackerTest, SimulatePosition) {
    equipment_tracker::GPSTracker tracker(1000);
    
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    
    tracker.registerPositionCallback([&](double latitude, double longitude, double altitude, const std::string&) {
        callback_called = true;
        lat = latitude;
        lon = longitude;
        alt = altitude;
    });
    
    // Test with positive coordinates
    tracker.simulatePosition(37.7749, 122.4194, 10.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_NEAR(lat, 37.7749, 0.0001);
    EXPECT_NEAR(lon, 122.4194, 0.0001);
    EXPECT_NEAR(alt, 10.0, 0.0001);
    
    // Reset and test with negative coordinates
    callback_called = false;
    tracker.simulatePosition(-37.7749, -122.4194, -10.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_NEAR(lat, -37.7749, 0.0001);
    EXPECT_NEAR(lon, -122.4194, 0.0001);
    EXPECT_NEAR(alt, -10.0, 0.0001);
}

// Test NMEA data processing
TEST_F(GPSTrackerTest, ProcessNMEAData) {
    equipment_tracker::GPSTracker tracker(1000);
    
    // Valid NMEA sentence
    std::string valid_nmea = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    // Should return true for valid data
    EXPECT_TRUE(tracker.processNMEAData(valid_nmea));
    
    // Invalid NMEA sentence (corrupted)
    std::string invalid_nmea = "$GPGGA,123519,4807.X38,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*XX\r\n";
    
    // Should handle invalid data gracefully
    EXPECT_FALSE(tracker.processNMEAData(invalid_nmea));
}

// Test serial port operations
TEST_F(GPSTrackerTest, SerialPortOperations) {
    equipment_tracker::GPSTracker tracker(1000);
    
    // Test opening a serial port (will be simulated)
    EXPECT_FALSE(tracker.openSerialPort("/dev/ttyUSB0"));
    
    // Test closing the port
    tracker.closeSerialPort();
    EXPECT_FALSE(tracker.is_port_open());
}

// Test position update handling from NMEA parser
TEST_F(GPSTrackerTest, HandlePositionUpdate) {
    equipment_tracker::GPSTracker tracker(1000);
    
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    
    tracker.registerPositionCallback([&](double latitude, double longitude, double altitude, const std::string&) {
        callback_called = true;
        lat = latitude;
        lon = longitude;
        alt = altitude;
    });
    
    // Simulate a position update through the NMEA parser callback
    tracker.handlePositionUpdate(37.7749, -122.4194, 10.0);
    
    // Since we can't directly test the internal NMEA parser behavior,
    // we'll just verify the callback was registered correctly
    EXPECT_TRUE(callback_called);
    EXPECT_NEAR(lat, 37.7749, 0.0001);
    EXPECT_NEAR(lon, -122.4194, 0.0001);
    EXPECT_NEAR(alt, 10.0, 0.0001);
}

// Test edge cases
TEST_F(GPSTrackerTest, EdgeCases) {
    equipment_tracker::GPSTracker tracker(1000);
    
    // Starting an already running tracker should be a no-op
    tracker.start();
    EXPECT_TRUE(tracker.is_running());
    tracker.start(); // Second call should be a no-op
    EXPECT_TRUE(tracker.is_running());
    
    // Stopping an already stopped tracker should be a no-op
    tracker.stop();
    EXPECT_FALSE(tracker.is_running());
    tracker.stop(); // Second call should be a no-op
    EXPECT_FALSE(tracker.is_running());
    
    // Test with extreme coordinates
    bool callback_called = false;
    tracker.registerPositionCallback([&](double, double, double, const std::string&) {
        callback_called = true;
    });
    
    // Test with extreme latitude/longitude values
    tracker.simulatePosition(90.0, 180.0, 9999.9);
    EXPECT_TRUE(callback_called);
    
    callback_called = false;
    tracker.simulatePosition(-90.0, -180.0, -9999.9);
    EXPECT_TRUE(callback_called);
}

// Test with empty or null callbacks
TEST_F(GPSTrackerTest, EmptyCallbacks) {
    equipment_tracker::GPSTracker tracker(1000);
    
    // Register a null callback (should not crash)
    tracker.registerPositionCallback(nullptr);
    
    // Simulate position (should not crash even with null callback)
    tracker.simulatePosition(37.7749, -122.4194, 10.0);
    
    // Test processNMEAData with empty string
    EXPECT_TRUE(tracker.processNMEAData(""));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>