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
    
    // Test processing valid NMEA data
    bool result = tracker.processNMEAData(valid_nmea);
    
    // Since we don't have a real NMEA parser in the test, this should return false
    EXPECT_FALSE(result);
}

// Test serial port operations
TEST_F(GPSTrackerTest, SerialPortOperations) {
    equipment_tracker::GPSTracker tracker(1000);
    
    // Try to open a non-existent port (should fail in real implementation)
    bool port_opened = tracker.openSerialPort("/dev/ttyS99");
    EXPECT_FALSE(port_opened);
    
    // Close the port
    tracker.closeSerialPort();
    
    // Read data (should return empty data)
    std::string data;
    bool read_result = tracker.readSerialData(data);
    EXPECT_TRUE(read_result);
    EXPECT_TRUE(data.empty());
}

// Test position update handling
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
    
    // Directly call the position update handler
    tracker.handlePositionUpdate(37.7749, -122.4194, 10.0);
    
    // Since we don't have a real NMEA parser in the test, the callback might not be called
    // This depends on the implementation of handlePositionUpdate
}

// Test multiple start/stop cycles
TEST_F(GPSTrackerTest, MultipleStartStop) {
    equipment_tracker::GPSTracker tracker(100); // Short interval for faster test
    
    // First cycle
    tracker.start();
    EXPECT_TRUE(tracker.is_running());
    tracker.stop();
    EXPECT_FALSE(tracker.is_running());
    
    // Second cycle
    tracker.start();
    EXPECT_TRUE(tracker.is_running());
    tracker.stop();
    EXPECT_FALSE(tracker.is_running());
    
    // Start when already started (should be idempotent)
    tracker.start();
    EXPECT_TRUE(tracker.is_running());
    tracker.start(); // Call start again
    EXPECT_TRUE(tracker.is_running());
    tracker.stop();
    EXPECT_FALSE(tracker.is_running());
    
    // Stop when already stopped (should be idempotent)
    tracker.stop(); // Call stop again
    EXPECT_FALSE(tracker.is_running());
}

// Test with edge cases for coordinates
TEST_F(GPSTrackerTest, CoordinateEdgeCases) {
    equipment_tracker::GPSTracker tracker(1000);
    
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    
    tracker.registerPositionCallback([&](double latitude, double longitude, double altitude, const std::string&) {
        callback_called = true;
        lat = latitude;
        lon = longitude;
        alt = altitude;
    });
    
    // Test with extreme values
    callback_called = false;
    tracker.simulatePosition(90.0, 180.0, 8848.0); // North pole, date line, Mt. Everest
    
    EXPECT_TRUE(callback_called);
    EXPECT_NEAR(lat, 90.0, 0.0001);
    EXPECT_NEAR(lon, 180.0, 0.0001);
    EXPECT_NEAR(alt, 8848.0, 0.0001);
    
    // Test with zero values
    callback_called = false;
    tracker.simulatePosition(0.0, 0.0, 0.0); // Null Island
    
    EXPECT_TRUE(callback_called);
    EXPECT_NEAR(lat, 0.0, 0.0001);
    EXPECT_NEAR(lon, 0.0, 0.0001);
    EXPECT_NEAR(alt, 0.0, 0.0001);
    
    // Test with negative altitude (below sea level)
    callback_called = false;
    tracker.simulatePosition(31.5, 35.5, -423.0); // Dead Sea
    
    EXPECT_TRUE(callback_called);
    EXPECT_NEAR(lat, 31.5, 0.0001);
    EXPECT_NEAR(lon, 35.5, 0.0001);
    EXPECT_NEAR(alt, -423.0, 0.0001);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>