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
    MOCK_METHOD(ERROR_E, ProcessNMEABuffer, (char* pBuffer, int nBufferSize));
    MOCK_METHOD(ERROR_E, GetGPGGA, (GGA_DATA_T& ggaData));
    MOCK_METHOD(void, OnError, (CNMEAParserData::ERROR_E nError, char* pCmd));
    MOCK_METHOD(void, LockDataAccess, ());
    MOCK_METHOD(void, UnlockDataAccess, ());
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
        mock_parser_ = new MockNMEAParser();
        tracker_ = std::make_unique<equipment_tracker::GPSTracker>(100);
        
        // Replace the internal NMEA parser with our mock
        tracker_->nmea_parser_.reset(mock_parser_);
    }

    void TearDown() override {
        tracker_.reset();
        // mock_parser_ is owned by tracker_ after SetUp
    }

    std::unique_ptr<equipment_tracker::GPSTracker> tracker_;
    MockNMEAParser* mock_parser_; // Non-owning pointer
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
    equipment_tracker::GPSTracker tracker(500);
    EXPECT_FALSE(tracker.is_running_);
    EXPECT_EQ(tracker.update_interval_ms_, 500);
    EXPECT_TRUE(tracker.nmea_parser_ != nullptr);
}

// Test start and stop methods
TEST_F(GPSTrackerTest, StartStop) {
    EXPECT_FALSE(tracker_->is_running_);
    
    tracker_->start();
    EXPECT_TRUE(tracker_->is_running_);
    
    tracker_->stop();
    EXPECT_FALSE(tracker_->is_running_);
}

// Test setting update interval
TEST_F(GPSTrackerTest, SetUpdateInterval) {
    EXPECT_EQ(tracker_->update_interval_ms_, 100);
    
    tracker_->setUpdateInterval(250);
    EXPECT_EQ(tracker_->update_interval_ms_, 250);
}

// Test position callback registration
TEST_F(GPSTrackerTest, RegisterPositionCallback) {
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    std::string timestamp;
    
    EXPECT_CALL(*mock_parser_, setPositionCallback(::testing::_))
        .Times(1);
    
    tracker_->registerPositionCallback([&](double latitude, double longitude, double altitude, const std::string& time) {
        callback_called = true;
        lat = latitude;
        lon = longitude;
        alt = altitude;
        timestamp = time;
    });
    
    // Simulate position update
    CNMEAParserData::GGA_DATA_T ggaData;
    ggaData.dLatitude = 37.7749;
    ggaData.dLongitude = -122.4194;
    ggaData.dAltitudeMSL = 10.0;
    
    EXPECT_CALL(*mock_parser_, GetGPGGA(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(ggaData),
            ::testing::Return(CNMEAParserData::ERROR_OK)
        ));
    
    // Trigger the position update
    tracker_->handlePositionUpdate(37.7749, -122.4194, 10.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(lat, 37.7749);
    EXPECT_DOUBLE_EQ(lon, -122.4194);
    EXPECT_DOUBLE_EQ(alt, 10.0);
    EXPECT_FALSE(timestamp.empty());
}

// Test simulating position
TEST_F(GPSTrackerTest, SimulatePosition) {
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    std::string timestamp;
    
    tracker_->registerPositionCallback([&](double latitude, double longitude, double altitude, const std::string& time) {
        callback_called = true;
        lat = latitude;
        lon = longitude;
        alt = altitude;
        timestamp = time;
    });
    
    // Expect the NMEA parser to be called with a buffer
    EXPECT_CALL(*mock_parser_, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_OK));
    
    // Simulate a position
    tracker_->simulatePosition(40.7128, -74.0060, 15.0); // New York
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(lat, 40.7128);
    EXPECT_DOUBLE_EQ(lon, -74.0060);
    EXPECT_DOUBLE_EQ(alt, 15.0);
    EXPECT_FALSE(timestamp.empty());
}

// Test processing NMEA data
TEST_F(GPSTrackerTest, ProcessNMEAData) {
    std::string nmea_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    EXPECT_CALL(*mock_parser_, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_OK));
    
    bool result = tracker_->processNMEAData(nmea_data);
    EXPECT_TRUE(result);
}

// Test handling position update
TEST_F(GPSTrackerTest, HandlePositionUpdate) {
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    std::string timestamp;
    
    tracker_->registerPositionCallback([&](double latitude, double longitude, double altitude, const std::string& time) {
        callback_called = true;
        lat = latitude;
        lon = longitude;
        alt = altitude;
        timestamp = time;
    });
    
    CNMEAParserData::GGA_DATA_T ggaData;
    ggaData.dLatitude = 35.6762;
    ggaData.dLongitude = 139.6503;
    ggaData.dAltitudeMSL = 25.0;
    
    EXPECT_CALL(*mock_parser_, GetGPGGA(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(ggaData),
            ::testing::Return(CNMEAParserData::ERROR_OK)
        ));
    
    tracker_->handlePositionUpdate(35.6762, 139.6503, 25.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(lat, 35.6762);
    EXPECT_DOUBLE_EQ(lon, 139.6503);
    EXPECT_DOUBLE_EQ(alt, 25.0);
    EXPECT_FALSE(timestamp.empty());
}

// Test serial port operations
TEST_F(GPSTrackerTest, SerialPortOperations) {
    // Test opening serial port (which is simulated)
    bool result = tracker_->openSerialPort("/dev/ttyUSB0");
    EXPECT_FALSE(result); // Should be false in the mock implementation
    EXPECT_EQ(tracker_->serial_port_, "/dev/ttyUSB0");
    EXPECT_FALSE(tracker_->is_port_open_);
    
    // Test closing serial port
    tracker_->closeSerialPort();
    EXPECT_FALSE(tracker_->is_port_open_);
    
    // Test reading serial data
    std::string data;
    result = tracker_->readSerialData(data);
    EXPECT_TRUE(result);
    EXPECT_TRUE(data.empty());
}

// Test worker thread function behavior
TEST_F(GPSTrackerTest, WorkerThreadBehavior) {
    // Set up a position callback to verify the worker thread calls it
    bool callback_called = false;
    
    tracker_->registerPositionCallback([&](double latitude, double longitude, double altitude, const std::string& time) {
        callback_called = true;
    });
    
    // Set a very short update interval for testing
    tracker_->setUpdateInterval(10);
    
    // Expect the NMEA parser to be called when the worker thread simulates a position
    EXPECT_CALL(*mock_parser_, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(CNMEAParserData::ERROR_OK));
    
    // Start the tracker
    tracker_->start();
    
    // Wait a bit for the worker thread to run
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Stop the tracker
    tracker_->stop();
    
    // Verify the callback was called
    EXPECT_TRUE(callback_called);
}

// Test error handling in NMEA parser
TEST(EquipmentNMEAParserTest, ErrorHandling) {
    equipment_tracker::EquipmentNMEAParser parser;
    
    // Redirect cerr to capture output
    std::stringstream buffer;
    std::streambuf* old = std::cerr.rdbuf(buffer.rdbuf());
    
    // Test OnError with command
    char cmd[] = "GPGGA";
    parser.OnError(CNMEAParserData::ERROR_GENERIC, cmd);
    
    // Test OnError without command
    parser.OnError(CNMEAParserData::ERROR_GENERIC, nullptr);
    
    // Restore cerr
    std::cerr.rdbuf(old);
    
    // Verify error messages were output
    std::string output = buffer.str();
    EXPECT_THAT(output, ::testing::HasSubstr("NMEA Parser Error"));
    EXPECT_THAT(output, ::testing::HasSubstr("GPGGA"));
}

// Test mutex operations in NMEA parser
TEST(EquipmentNMEAParserTest, MutexOperations) {
    equipment_tracker::EquipmentNMEAParser parser;
    
    // Test locking and unlocking
    parser.LockDataAccess();
    parser.UnlockDataAccess();
    
    // If we can lock again, the previous unlock worked
    bool can_lock_again = parser.mutex_.try_lock();
    EXPECT_TRUE(can_lock_again);
    
    // Clean up
    if (can_lock_again) {
        parser.mutex_.unlock();
    }
}
// </test_code>