// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <iostream>
#include <chrono>
#include <cstring>
#include <sstream>
#include <thread>
#include "equipment_tracker/gps_tracker.h"
#include "equipment_tracker/position.h"

// Mock for CNMEAParserData
class MockCNMEAParserData {
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

// Mock for the NMEA Parser
class MockEquipmentNMEAParser : public equipment_tracker::EquipmentNMEAParser {
public:
    MOCK_METHOD(void, setPositionCallback, (std::function<void(double, double, double)>), (override));
    MOCK_METHOD(MockCNMEAParserData::ERROR_E, ProcessNMEABuffer, (char*, int), ());
    MOCK_METHOD(MockCNMEAParserData::ERROR_E, GetGPGGA, (MockCNMEAParserData::GGA_DATA_T&), ());
};

namespace equipment_tracker {

class GPSTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test instance with a 100ms update interval
        tracker = std::make_unique<GPSTracker>(100);
    }

    void TearDown() override {
        tracker->stop();
        tracker.reset();
    }

    std::unique_ptr<GPSTracker> tracker;
};

// Test fixture for testing with a mock NMEA parser
class GPSTrackerWithMockTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_parser = new ::testing::NiceMock<MockEquipmentNMEAParser>();
        tracker = std::make_unique<GPSTracker>(100);
        
        // Replace the real parser with our mock
        tracker->nmea_parser_.reset(mock_parser);
    }

    void TearDown() override {
        tracker->stop();
        tracker.reset();
        // mock_parser is owned by tracker, no need to delete
    }

    std::unique_ptr<GPSTracker> tracker;
    MockEquipmentNMEAParser* mock_parser; // Non-owning pointer
};

// Test constructor and default values
TEST_F(GPSTrackerTest, Constructor) {
    EXPECT_EQ(tracker->update_interval_ms_, 100);
    EXPECT_FALSE(tracker->is_running_);
    EXPECT_TRUE(tracker->nmea_parser_ != nullptr);
}

// Test setting update interval
TEST_F(GPSTrackerTest, SetUpdateInterval) {
    tracker->setUpdateInterval(500);
    EXPECT_EQ(tracker->update_interval_ms_, 500);
}

// Test start and stop
TEST_F(GPSTrackerTest, StartStop) {
    EXPECT_FALSE(tracker->is_running_);
    
    tracker->start();
    EXPECT_TRUE(tracker->is_running_);
    
    tracker->stop();
    EXPECT_FALSE(tracker->is_running_);
}

// Test double start has no effect
TEST_F(GPSTrackerTest, DoubleStart) {
    tracker->start();
    EXPECT_TRUE(tracker->is_running_);
    
    // Capture the thread ID to verify it doesn't change
    std::thread::id first_thread_id = tracker->worker_thread_.get_id();
    
    tracker->start(); // Should have no effect
    EXPECT_TRUE(tracker->is_running_);
    EXPECT_EQ(first_thread_id, tracker->worker_thread_.get_id());
    
    tracker->stop();
}

// Test double stop has no effect
TEST_F(GPSTrackerTest, DoubleStop) {
    tracker->start();
    EXPECT_TRUE(tracker->is_running_);
    
    tracker->stop();
    EXPECT_FALSE(tracker->is_running_);
    
    tracker->stop(); // Should have no effect
    EXPECT_FALSE(tracker->is_running_);
}

// Test position callback registration
TEST_F(GPSTrackerWithMockTest, RegisterPositionCallback) {
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    std::string timestamp;
    
    EXPECT_CALL(*mock_parser, setPositionCallback(::testing::_))
        .Times(1);
    
    tracker->registerPositionCallback([&](double latitude, double longitude, double altitude, const std::string& time) {
        callback_called = true;
        lat = latitude;
        lon = longitude;
        alt = altitude;
        timestamp = time;
    });
    
    // Simulate a position update
    tracker->simulatePosition(45.0, -75.0, 100.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(lat, 45.0);
    EXPECT_DOUBLE_EQ(lon, -75.0);
    EXPECT_DOUBLE_EQ(alt, 100.0);
    EXPECT_FALSE(timestamp.empty());
}

// Test position simulation
TEST_F(GPSTrackerTest, SimulatePosition) {
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    
    tracker->registerPositionCallback([&](double latitude, double longitude, double altitude, const std::string&) {
        callback_called = true;
        lat = latitude;
        lon = longitude;
        alt = altitude;
    });
    
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(lat, 37.7749);
    EXPECT_DOUBLE_EQ(lon, -122.4194);
    EXPECT_DOUBLE_EQ(alt, 10.0);
}

// Test NMEA data processing
TEST_F(GPSTrackerWithMockTest, ProcessNMEAData) {
    std::string valid_nmea = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    EXPECT_CALL(*mock_parser, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(MockCNMEAParserData::ERROR_OK));
    
    bool result = tracker->processNMEAData(valid_nmea);
    EXPECT_TRUE(result);
}

// Test NMEA data processing with null parser
TEST_F(GPSTrackerTest, ProcessNMEADataNullParser) {
    std::string valid_nmea = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    // Set parser to null
    tracker->nmea_parser_.reset();
    
    bool result = tracker->processNMEAData(valid_nmea);
    EXPECT_FALSE(result);
}

// Test position update handling from NMEA parser
TEST_F(GPSTrackerWithMockTest, HandlePositionUpdate) {
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    
    tracker->registerPositionCallback([&](double latitude, double longitude, double altitude, const std::string&) {
        callback_called = true;
        lat = latitude;
        lon = longitude;
        alt = altitude;
    });
    
    // Set up mock to return GGA data
    MockCNMEAParserData::GGA_DATA_T ggaData;
    ggaData.dLatitude = 37.7749;
    ggaData.dLongitude = -122.4194;
    ggaData.dAltitudeMSL = 10.0;
    
    EXPECT_CALL(*mock_parser, GetGPGGA(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(ggaData),
            ::testing::Return(MockCNMEAParserData::ERROR_OK)
        ));
    
    // Call the handler
    tracker->handlePositionUpdate(0.0, 0.0, 0.0); // Parameters are ignored, mock returns the data
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(lat, 37.7749);
    EXPECT_DOUBLE_EQ(lon, -122.4194);
    EXPECT_DOUBLE_EQ(alt, 10.0);
}

// Test serial port operations
TEST_F(GPSTrackerTest, SerialPortOperations) {
    // Test opening a port (should fail in test environment)
    bool result = tracker->openSerialPort("/dev/ttyS0");
    EXPECT_FALSE(result);
    EXPECT_EQ(tracker->serial_port_, "/dev/ttyS0");
    EXPECT_FALSE(tracker->is_port_open_);
    
    // Test closing a port
    tracker->closeSerialPort();
    EXPECT_FALSE(tracker->is_port_open_);
    
    // Test reading data (should return empty in test environment)
    std::string data = "test";
    result = tracker->readSerialData(data);
    EXPECT_TRUE(result);
    EXPECT_TRUE(data.empty());
}

// Test worker thread function indirectly
TEST_F(GPSTrackerTest, WorkerThreadFunction) {
    bool callback_called = false;
    
    tracker->registerPositionCallback([&](double, double, double, const std::string&) {
        callback_called = true;
    });
    
    tracker->start();
    
    // Wait for the worker thread to run at least once
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    tracker->stop();
    
    // Since we have no real device, the worker should simulate a position
    EXPECT_TRUE(callback_called);
}

} // namespace equipment_tracker
// </test_code>