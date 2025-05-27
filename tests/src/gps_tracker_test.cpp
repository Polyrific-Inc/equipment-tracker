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
    MOCK_METHOD(void, setPositionCallback, (std::function<void(double, double, double)> callback));
    
    void OnError(CNMEAParserData::ERROR_E nError, char* pCmd);
    void LockDataAccess();
    void UnlockDataAccess();
    
private:
    std::mutex mutex_;
};
}

// Mock for EquipmentNMEAParser
class MockNMEAParser : public equipment_tracker::EquipmentNMEAParser {
public:
    MOCK_METHOD(ERROR_E, ProcessNMEABuffer, (char* pBuffer, int nBufferSize), (override));
    MOCK_METHOD(ERROR_E, GetGPGGA, (GGA_DATA_T& ggaData), (override));
    MOCK_METHOD(void, setPositionCallback, (std::function<void(double, double, double)> callback), (override));
};

// Test fixture for GPSTracker
class GPSTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a mock NMEA parser
        mock_nmea_parser_ = new MockNMEAParser();
        
        // Create GPSTracker with default update interval
        gps_tracker_ = std::make_unique<equipment_tracker::GPSTracker>(1000);
        
        // Replace the internal NMEA parser with our mock
        gps_tracker_->nmea_parser_.reset(mock_nmea_parser_);
    }
    
    void TearDown() override {
        gps_tracker_.reset();
    }
    
    std::unique_ptr<equipment_tracker::GPSTracker> gps_tracker_;
    MockNMEAParser* mock_nmea_parser_; // Owned by gps_tracker_
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

// Test setUpdateInterval
TEST_F(GPSTrackerTest, SetUpdateInterval) {
    gps_tracker_->setUpdateInterval(2000);
    EXPECT_EQ(gps_tracker_->update_interval_ms_, 2000);
}

// Test start and stop
TEST_F(GPSTrackerTest, StartStop) {
    EXPECT_FALSE(gps_tracker_->is_running_);
    
    gps_tracker_->start();
    EXPECT_TRUE(gps_tracker_->is_running_);
    
    gps_tracker_->stop();
    EXPECT_FALSE(gps_tracker_->is_running_);
}

// Test double start/stop has no effect
TEST_F(GPSTrackerTest, DoubleStartStop) {
    gps_tracker_->start();
    EXPECT_TRUE(gps_tracker_->is_running_);
    
    // Second start should have no effect
    gps_tracker_->start();
    EXPECT_TRUE(gps_tracker_->is_running_);
    
    gps_tracker_->stop();
    EXPECT_FALSE(gps_tracker_->is_running_);
    
    // Second stop should have no effect
    gps_tracker_->stop();
    EXPECT_FALSE(gps_tracker_->is_running_);
}

// Test registerPositionCallback
TEST_F(GPSTrackerTest, RegisterPositionCallback) {
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    std::string timestamp;
    
    EXPECT_CALL(*mock_nmea_parser_, setPositionCallback(::testing::_))
        .Times(1);
    
    gps_tracker_->registerPositionCallback(
        [&callback_called, &lat, &lon, &alt, &timestamp]
        (double latitude, double longitude, double altitude, const std::string& ts) {
            callback_called = true;
            lat = latitude;
            lon = longitude;
            alt = altitude;
            timestamp = ts;
        }
    );
}

// Test simulatePosition
TEST_F(GPSTrackerTest, SimulatePosition) {
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    std::string timestamp;
    
    EXPECT_CALL(*mock_nmea_parser_, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_OK));
    
    gps_tracker_->registerPositionCallback(
        [&callback_called, &lat, &lon, &alt, &timestamp]
        (double latitude, double longitude, double altitude, const std::string& ts) {
            callback_called = true;
            lat = latitude;
            lon = longitude;
            alt = altitude;
            timestamp = ts;
        }
    );
    
    gps_tracker_->simulatePosition(37.7749, -122.4194, 10.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(lat, 37.7749);
    EXPECT_DOUBLE_EQ(lon, -122.4194);
    EXPECT_DOUBLE_EQ(alt, 10.0);
    EXPECT_FALSE(timestamp.empty());
}

// Test processNMEAData
TEST_F(GPSTrackerTest, ProcessNMEAData) {
    std::string test_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    EXPECT_CALL(*mock_nmea_parser_, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_OK));
    
    bool result = gps_tracker_->processNMEAData(test_data);
    EXPECT_TRUE(result);
}

// Test processNMEAData with error
TEST_F(GPSTrackerTest, ProcessNMEADataError) {
    std::string test_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    EXPECT_CALL(*mock_nmea_parser_, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_GENERIC));
    
    bool result = gps_tracker_->processNMEAData(test_data);
    EXPECT_FALSE(result);
}

// Test handlePositionUpdate
TEST_F(GPSTrackerTest, HandlePositionUpdate) {
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    std::string timestamp;
    
    CNMEAParserData::GGA_DATA_T ggaData;
    ggaData.dLatitude = 37.7749;
    ggaData.dLongitude = -122.4194;
    ggaData.dAltitudeMSL = 10.0;
    
    EXPECT_CALL(*mock_nmea_parser_, GetGPGGA(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(ggaData),
            ::testing::Return(CNMEAParserData::ERROR_OK)
        ));
    
    gps_tracker_->registerPositionCallback(
        [&callback_called, &lat, &lon, &alt, &timestamp]
        (double latitude, double longitude, double altitude, const std::string& ts) {
            callback_called = true;
            lat = latitude;
            lon = longitude;
            alt = altitude;
            timestamp = ts;
        }
    );
    
    gps_tracker_->handlePositionUpdate(37.7749, -122.4194, 10.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(lat, 37.7749);
    EXPECT_DOUBLE_EQ(lon, -122.4194);
    EXPECT_DOUBLE_EQ(alt, 10.0);
    EXPECT_FALSE(timestamp.empty());
}

// Test handlePositionUpdate with error
TEST_F(GPSTrackerTest, HandlePositionUpdateError) {
    bool callback_called = false;
    
    EXPECT_CALL(*mock_nmea_parser_, GetGPGGA(::testing::_))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_GENERIC));
    
    gps_tracker_->registerPositionCallback(
        [&callback_called](double, double, double, const std::string&) {
            callback_called = true;
        }
    );
    
    gps_tracker_->handlePositionUpdate(37.7749, -122.4194, 10.0);
    
    EXPECT_FALSE(callback_called);
}

// Test serial port operations
TEST_F(GPSTrackerTest, SerialPortOperations) {
    // Test openSerialPort
    bool result = gps_tracker_->openSerialPort("/dev/ttyUSB0");
    EXPECT_FALSE(result);
    EXPECT_EQ(gps_tracker_->serial_port_, "/dev/ttyUSB0");
    EXPECT_FALSE(gps_tracker_->is_port_open_);
    
    // Test closeSerialPort
    gps_tracker_->closeSerialPort();
    EXPECT_FALSE(gps_tracker_->is_port_open_);
    
    // Test readSerialData
    std::string data;
    result = gps_tracker_->readSerialData(data);
    EXPECT_TRUE(result);
    EXPECT_TRUE(data.empty());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>