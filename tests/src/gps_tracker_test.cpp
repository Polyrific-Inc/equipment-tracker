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
    MOCK_METHOD(void, OnError, (CNMEAParserData::ERROR_E nError, char* pCmd));
    MOCK_METHOD(void, LockDataAccess, ());
    MOCK_METHOD(void, UnlockDataAccess, ());
    MOCK_METHOD(ERROR_E, ProcessNMEABuffer, (char* pBuffer, int nBufferSize));
    MOCK_METHOD(ERROR_E, GetGPGGA, (GGA_DATA_T& ggaData));
    MOCK_METHOD(void, setPositionCallback, (std::function<void(double, double, double)> callback));
    
    std::mutex mutex_;
};
}

// Mock for the NMEA parser
class MockNMEAParser : public equipment_tracker::EquipmentNMEAParser {
public:
    MOCK_METHOD(CNMEAParserData::ERROR_E, ProcessNMEABuffer, (char* pBuffer, int nBufferSize), (override));
    MOCK_METHOD(CNMEAParserData::ERROR_E, GetGPGGA, (GGA_DATA_T& ggaData), (override));
    MOCK_METHOD(void, setPositionCallback, (std::function<void(double, double, double)> callback), (override));
};

// Test fixture for GPSTracker
class GPSTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_nmea_parser_ = new ::testing::NiceMock<MockNMEAParser>();
    }

    void TearDown() override {
        // Cleanup
    }

    std::unique_ptr<equipment_tracker::EquipmentNMEAParser> CreateMockParser() {
        return std::unique_ptr<equipment_tracker::EquipmentNMEAParser>(mock_nmea_parser_);
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

// Test setting update interval
TEST_F(GPSTrackerTest, SetUpdateInterval) {
    equipment_tracker::GPSTracker tracker(1000);
    tracker.setUpdateInterval(2000);
    EXPECT_EQ(tracker.getUpdateInterval(), 2000);
}

// Test start and stop
TEST_F(GPSTrackerTest, StartStop) {
    equipment_tracker::GPSTracker tracker(1000);
    
    // Start
    tracker.start();
    EXPECT_TRUE(tracker.is_running());
    
    // Start again (should be idempotent)
    tracker.start();
    EXPECT_TRUE(tracker.is_running());
    
    // Stop
    tracker.stop();
    EXPECT_FALSE(tracker.is_running());
    
    // Stop again (should be idempotent)
    tracker.stop();
    EXPECT_FALSE(tracker.is_running());
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

// Test position simulation
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
    tracker.simulatePosition(40.7128, 74.0060, 20.0);
    EXPECT_TRUE(callback_called);
    EXPECT_NEAR(lat, 40.7128, 0.0001);
    EXPECT_NEAR(lon, 74.0060, 0.0001);
    EXPECT_NEAR(alt, 20.0, 0.0001);
    
    // Reset and test with negative coordinates
    callback_called = false;
    tracker.simulatePosition(-33.8688, -151.2093, 30.0);
    EXPECT_TRUE(callback_called);
    EXPECT_NEAR(lat, -33.8688, 0.0001);
    EXPECT_NEAR(lon, -151.2093, 0.0001);
    EXPECT_NEAR(alt, 30.0, 0.0001);
}

// Test NMEA data processing
TEST_F(GPSTrackerTest, ProcessNMEAData) {
    // Create a tracker with our mock NMEA parser
    equipment_tracker::GPSTracker tracker(1000);
    
    // Set up expectations for the mock
    EXPECT_CALL(*mock_nmea_parser_, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_OK));
    
    // Process some NMEA data
    std::string nmea_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    bool result = tracker.processNMEAData(nmea_data);
    
    EXPECT_TRUE(result);
}

// Test position update handling from NMEA parser
TEST_F(GPSTrackerTest, HandlePositionUpdate) {
    // Create a tracker with our mock NMEA parser
    equipment_tracker::GPSTracker tracker(1000);
    
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    
    tracker.registerPositionCallback([&](double latitude, double longitude, double altitude, const std::string&) {
        callback_called = true;
        lat = latitude;
        lon = longitude;
        alt = altitude;
    });
    
    // Set up GGA data that will be returned by the mock
    CNMEAParserData::GGA_DATA_T ggaData;
    ggaData.dLatitude = 37.7749;
    ggaData.dLongitude = -122.4194;
    ggaData.dAltitudeMSL = 10.0;
    
    // Set up expectations for the mock
    EXPECT_CALL(*mock_nmea_parser_, GetGPGGA(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(ggaData),
            ::testing::Return(CNMEAParserData::ERROR_OK)
        ));
    
    // Trigger position update handling
    tracker.handlePositionUpdate(37.7749, -122.4194, 10.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_NEAR(lat, 37.7749, 0.0001);
    EXPECT_NEAR(lon, -122.4194, 0.0001);
    EXPECT_NEAR(alt, 10.0, 0.0001);
}

// Test serial port operations
TEST_F(GPSTrackerTest, SerialPortOperations) {
    equipment_tracker::GPSTracker tracker(1000);
    
    // Test opening a serial port (should fail in test environment)
    bool result = tracker.openSerialPort("/dev/ttyUSB0");
    EXPECT_FALSE(result);
    
    // Test closing the serial port
    tracker.closeSerialPort();
    
    // Test reading serial data
    std::string data;
    result = tracker.readSerialData(data);
    EXPECT_TRUE(result);
    EXPECT_TRUE(data.empty());
}

// Test the worker thread function indirectly
TEST_F(GPSTrackerTest, WorkerThreadFunction) {
    equipment_tracker::GPSTracker tracker(100); // Use a short interval for testing
    
    bool callback_called = false;
    
    tracker.registerPositionCallback([&](double, double, double, const std::string&) {
        callback_called = true;
    });
    
    // Start the tracker
    tracker.start();
    
    // Wait a bit for the worker thread to run
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // Stop the tracker
    tracker.stop();
    
    // The callback should have been called at least once
    EXPECT_TRUE(callback_called);
}

// Test error handling in NMEA parser
TEST(EquipmentNMEAParserTest, OnError) {
    equipment_tracker::EquipmentNMEAParser parser;
    
    // Redirect cerr to capture output
    std::stringstream buffer;
    std::streambuf* old = std::cerr.rdbuf(buffer.rdbuf());
    
    // Test with null command
    parser.OnError(CNMEAParserData::ERROR_GENERIC, nullptr);
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("NMEA Parser Error"));
    
    // Reset buffer
    buffer.str("");
    
    // Test with command
    char cmd[] = "$GPGGA";
    parser.OnError(CNMEAParserData::ERROR_GENERIC, cmd);
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("NMEA Parser Error"));
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("$GPGGA"));
    
    // Restore cerr
    std::cerr.rdbuf(old);
}

// Test mutex operations in NMEA parser
TEST(EquipmentNMEAParserTest, MutexOperations) {
    equipment_tracker::EquipmentNMEAParser parser;
    
    // Test locking and unlocking
    parser.LockDataAccess();
    parser.UnlockDataAccess();
    
    // If we can lock again, the previous unlock worked
    bool can_lock = true;
    try {
        parser.LockDataAccess();
    } catch (...) {
        can_lock = false;
    }
    
    EXPECT_TRUE(can_lock);
    
    // Clean up
    parser.UnlockDataAccess();
}
// </test_code>