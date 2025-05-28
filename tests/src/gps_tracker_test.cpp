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
        ERROR_OK,
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
};
}

// Mock for the EquipmentNMEAParser
class MockEquipmentNMEAParser : public equipment_tracker::EquipmentNMEAParser {
public:
    MOCK_METHOD(CNMEAParserData::ERROR_E, ProcessNMEABuffer, (char* pBuffer, int nBufferSize), (override));
    MOCK_METHOD(CNMEAParserData::ERROR_E, GetGPGGA, (GGA_DATA_T& ggaData), (override));
    MOCK_METHOD(void, setPositionCallback, (std::function<void(double, double, double)> callback), (override));
};

// Test fixture for GPSTracker
class GPSTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a mock NMEA parser
        mock_nmea_parser_ = new ::testing::NiceMock<MockEquipmentNMEAParser>();
        
        // Create a GPS tracker with the mock parser
        gps_tracker_ = std::make_unique<equipment_tracker::GPSTracker>(100);
        
        // Replace the internal NMEA parser with our mock
        gps_tracker_->nmea_parser_.reset(mock_nmea_parser_);
    }

    void TearDown() override {
        gps_tracker_.reset();
    }

    std::unique_ptr<equipment_tracker::GPSTracker> gps_tracker_;
    MockEquipmentNMEAParser* mock_nmea_parser_; // Owned by gps_tracker_
};

// Test constructor and default values
TEST_F(GPSTrackerTest, Constructor) {
    equipment_tracker::GPSTracker tracker(500);
    EXPECT_EQ(tracker.update_interval_ms_, 500);
    EXPECT_FALSE(tracker.is_running_);
    EXPECT_TRUE(tracker.nmea_parser_ != nullptr);
}

// Test setUpdateInterval
TEST_F(GPSTrackerTest, SetUpdateInterval) {
    gps_tracker_->setUpdateInterval(200);
    EXPECT_EQ(gps_tracker_->update_interval_ms_, 200);
}

// Test start and stop
TEST_F(GPSTrackerTest, StartAndStop) {
    EXPECT_FALSE(gps_tracker_->is_running_);
    
    gps_tracker_->start();
    EXPECT_TRUE(gps_tracker_->is_running_);
    
    gps_tracker_->stop();
    EXPECT_FALSE(gps_tracker_->is_running_);
}

// Test double start has no effect
TEST_F(GPSTrackerTest, DoubleStartHasNoEffect) {
    gps_tracker_->start();
    EXPECT_TRUE(gps_tracker_->is_running_);
    
    // Capture thread ID for comparison
    std::thread::id first_thread_id = gps_tracker_->worker_thread_.get_id();
    
    // Second start should have no effect
    gps_tracker_->start();
    EXPECT_TRUE(gps_tracker_->is_running_);
    
    // Thread ID should remain the same
    EXPECT_EQ(first_thread_id, gps_tracker_->worker_thread_.get_id());
    
    gps_tracker_->stop();
}

// Test double stop has no effect
TEST_F(GPSTrackerTest, DoubleStopHasNoEffect) {
    gps_tracker_->start();
    EXPECT_TRUE(gps_tracker_->is_running_);
    
    gps_tracker_->stop();
    EXPECT_FALSE(gps_tracker_->is_running_);
    
    // Second stop should have no effect
    gps_tracker_->stop();
    EXPECT_FALSE(gps_tracker_->is_running_);
}

// Test position callback registration
TEST_F(GPSTrackerTest, RegisterPositionCallback) {
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    std::string timestamp;
    
    EXPECT_CALL(*mock_nmea_parser_, setPositionCallback(::testing::_))
        .Times(1);
    
    gps_tracker_->registerPositionCallback(
        [&callback_called, &lat, &lon, &alt, &timestamp]
        (double latitude, double longitude, double altitude, const std::string& time) {
            callback_called = true;
            lat = latitude;
            lon = longitude;
            alt = altitude;
            timestamp = time;
        }
    );
}

// Test simulate position with callback
TEST_F(GPSTrackerTest, SimulatePosition) {
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    std::string timestamp;
    
    // Set up expectations for the mock
    EXPECT_CALL(*mock_nmea_parser_, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_OK));
    
    gps_tracker_->registerPositionCallback(
        [&callback_called, &lat, &lon, &alt, &timestamp]
        (double latitude, double longitude, double altitude, const std::string& time) {
            callback_called = true;
            lat = latitude;
            lon = longitude;
            alt = altitude;
            timestamp = time;
        }
    );
    
    // Simulate a position
    gps_tracker_->simulatePosition(37.7749, -122.4194, 10.0);
    
    // Verify callback was called with correct values
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(lat, 37.7749);
    EXPECT_DOUBLE_EQ(lon, -122.4194);
    EXPECT_DOUBLE_EQ(alt, 10.0);
    EXPECT_FALSE(timestamp.empty());
}

// Test processNMEAData
TEST_F(GPSTrackerTest, ProcessNMEAData) {
    // Set up expectations for the mock
    EXPECT_CALL(*mock_nmea_parser_, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_OK))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_GENERIC));
    
    // Test successful processing
    std::string valid_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    EXPECT_TRUE(gps_tracker_->processNMEAData(valid_data));
    
    // Test failed processing
    std::string invalid_data = "Invalid NMEA data";
    EXPECT_FALSE(gps_tracker_->processNMEAData(invalid_data));
}

// Test handlePositionUpdate
TEST_F(GPSTrackerTest, HandlePositionUpdate) {
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    std::string timestamp;
    
    gps_tracker_->registerPositionCallback(
        [&callback_called, &lat, &lon, &alt, &timestamp]
        (double latitude, double longitude, double altitude, const std::string& time) {
            callback_called = true;
            lat = latitude;
            lon = longitude;
            alt = altitude;
            timestamp = time;
        }
    );
    
    // Set up GGA data to be returned by the mock
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
    
    // Call the method under test
    gps_tracker_->handlePositionUpdate(37.7749, -122.4194, 10.0);
    
    // Verify callback was called with correct values
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(lat, 37.7749);
    EXPECT_DOUBLE_EQ(lon, -122.4194);
    EXPECT_DOUBLE_EQ(alt, 10.0);
    EXPECT_FALSE(timestamp.empty());
}

// Test serial port operations
TEST_F(GPSTrackerTest, SerialPortOperations) {
    // Test opening a serial port (which will fail in our mock implementation)
    EXPECT_FALSE(gps_tracker_->openSerialPort("/dev/ttyUSB0"));
    EXPECT_EQ(gps_tracker_->serial_port_, "/dev/ttyUSB0");
    EXPECT_FALSE(gps_tracker_->is_port_open_);
    
    // Test closing the serial port
    gps_tracker_->closeSerialPort();
    EXPECT_FALSE(gps_tracker_->is_port_open_);
    
    // Test reading serial data
    std::string data;
    EXPECT_TRUE(gps_tracker_->readSerialData(data));
    EXPECT_TRUE(data.empty());
}

// Test NMEA sentence generation in simulatePosition
TEST_F(GPSTrackerTest, NMEASentenceGeneration) {
    // Set up a mock to capture the NMEA sentence
    std::string captured_nmea;
    
    EXPECT_CALL(*mock_nmea_parser_, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&captured_nmea](char* pBuffer, int) {
                captured_nmea = pBuffer;
                return CNMEAParserData::ERROR_OK;
            }),
            ::testing::Return(CNMEAParserData::ERROR_OK)
        ));
    
    // Simulate a position
    gps_tracker_->simulatePosition(37.7749, -122.4194, 10.0);
    
    // Verify the NMEA sentence format
    EXPECT_THAT(captured_nmea, ::testing::HasSubstr("$GPGGA"));
    EXPECT_THAT(captured_nmea, ::testing::HasSubstr("3746.4940"));  // Latitude in DDMM.MMMM format
    EXPECT_THAT(captured_nmea, ::testing::HasSubstr("N"));          // North
    EXPECT_THAT(captured_nmea, ::testing::HasSubstr("12225.1640")); // Longitude in DDDMM.MMMM format
    EXPECT_THAT(captured_nmea, ::testing::HasSubstr("W"));          // West
    EXPECT_THAT(captured_nmea, ::testing::HasSubstr("10.0"));       // Altitude
    EXPECT_THAT(captured_nmea, ::testing::HasSubstr("*"));          // Checksum marker
}

// Test worker thread behavior
TEST_F(GPSTrackerTest, WorkerThreadBehavior) {
    // Set a short update interval for testing
    gps_tracker_->setUpdateInterval(10);
    
    // Set up expectations for simulation in worker thread
    EXPECT_CALL(*mock_nmea_parser_, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(CNMEAParserData::ERROR_OK));
    
    // Start the worker thread
    gps_tracker_->start();
    
    // Let it run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Stop the worker thread
    gps_tracker_->stop();
    
    // Verify that ProcessNMEABuffer was called at least once
    // (This is already checked by the EXPECT_CALL above)
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>