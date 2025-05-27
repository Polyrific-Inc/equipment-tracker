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

// Mock for CNMEAParserData to simulate the NMEA parser functionality
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
class EquipmentNMEAParser {
public:
    virtual ~EquipmentNMEAParser() = default;
    virtual void OnError(CNMEAParserData::ERROR_E nError, char* pCmd) {}
    virtual void LockDataAccess() {}
    virtual void UnlockDataAccess() {}
    virtual CNMEAParserData::ERROR_E ProcessNMEABuffer(char* pBuffer, int nBufferSize) { return CNMEAParserData::ERROR_OK; }
    virtual CNMEAParserData::ERROR_E GetGPGGA(CNMEAParserData::GGA_DATA_T& ggaData) { return CNMEAParserData::ERROR_OK; }
    virtual void setPositionCallback(std::function<void(double, double, double)> callback) {}
};
}

// Mock implementation for testing
class MockNMEAParser : public equipment_tracker::EquipmentNMEAParser {
public:
    MOCK_METHOD(CNMEAParserData::ERROR_E, ProcessNMEABuffer, (char* pBuffer, int nBufferSize), (override));
    MOCK_METHOD(CNMEAParserData::ERROR_E, GetGPGGA, (CNMEAParserData::GGA_DATA_T& ggaData), (override));
    MOCK_METHOD(void, setPositionCallback, (std::function<void(double, double, double)> callback), (override));
};

// Test fixture for GPSTracker
class GPSTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a mock NMEA parser
        mock_parser_ = new MockNMEAParser();
    }

    void TearDown() override {
        // Clean up
    }

    // Helper to create a GPSTracker with a mock parser
    std::unique_ptr<equipment_tracker::GPSTracker> createTrackerWithMockParser() {
        auto tracker = std::make_unique<equipment_tracker::GPSTracker>(100);
        // Replace the real parser with our mock
        tracker->nmea_parser_.reset(mock_parser_);
        return tracker;
    }

    MockNMEAParser* mock_parser_;
};

// Test constructor with different update intervals
TEST_F(GPSTrackerTest, Constructor) {
    equipment_tracker::GPSTracker tracker1(100);
    EXPECT_EQ(100, tracker1.update_interval_ms_);
    EXPECT_FALSE(tracker1.is_running_);
    EXPECT_TRUE(tracker1.nmea_parser_ != nullptr);

    equipment_tracker::GPSTracker tracker2(500);
    EXPECT_EQ(500, tracker2.update_interval_ms_);
}

// Test start and stop methods
TEST_F(GPSTrackerTest, StartStop) {
    auto tracker = createTrackerWithMockParser();
    
    // Test start
    EXPECT_FALSE(tracker->is_running_);
    tracker->start();
    EXPECT_TRUE(tracker->is_running_);
    
    // Test starting when already running
    tracker->start();
    EXPECT_TRUE(tracker->is_running_);
    
    // Test stop
    tracker->stop();
    EXPECT_FALSE(tracker->is_running_);
    
    // Test stopping when already stopped
    tracker->stop();
    EXPECT_FALSE(tracker->is_running_);
}

// Test setUpdateInterval method
TEST_F(GPSTrackerTest, SetUpdateInterval) {
    auto tracker = createTrackerWithMockParser();
    
    EXPECT_EQ(100, tracker->update_interval_ms_);
    
    tracker->setUpdateInterval(200);
    EXPECT_EQ(200, tracker->update_interval_ms_);
    
    tracker->setUpdateInterval(0);
    EXPECT_EQ(0, tracker->update_interval_ms_);
}

// Test registerPositionCallback method
TEST_F(GPSTrackerTest, RegisterPositionCallback) {
    auto tracker = createTrackerWithMockParser();
    
    // Expect the setPositionCallback method to be called on the parser
    EXPECT_CALL(*mock_parser_, setPositionCallback(::testing::_))
        .Times(1);
    
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    std::string timestamp;
    
    tracker->registerPositionCallback([&](double latitude, double longitude, double altitude, const std::string& time) {
        callback_called = true;
        lat = latitude;
        lon = longitude;
        alt = altitude;
        timestamp = time;
    });
    
    // Verify the callback was registered
    EXPECT_FALSE(callback_called);
}

// Test simulatePosition method
TEST_F(GPSTrackerTest, SimulatePosition) {
    auto tracker = createTrackerWithMockParser();
    
    // Set up expectations for the mock parser
    EXPECT_CALL(*mock_parser_, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_OK));
    
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    std::string timestamp;
    
    tracker->registerPositionCallback([&](double latitude, double longitude, double altitude, const std::string& time) {
        callback_called = true;
        lat = latitude;
        lon = longitude;
        alt = altitude;
        timestamp = time;
    });
    
    // Simulate a position
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    
    // Verify the callback was called with correct values
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, lat);
    EXPECT_DOUBLE_EQ(-122.4194, lon);
    EXPECT_DOUBLE_EQ(10.0, alt);
    EXPECT_FALSE(timestamp.empty());
}

// Test processNMEAData method
TEST_F(GPSTrackerTest, ProcessNMEAData) {
    auto tracker = createTrackerWithMockParser();
    
    // Test with valid NMEA data
    EXPECT_CALL(*mock_parser_, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_OK));
    
    bool result = tracker->processNMEAData("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n");
    EXPECT_TRUE(result);
    
    // Test with invalid NMEA data
    EXPECT_CALL(*mock_parser_, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_GENERIC));
    
    result = tracker->processNMEAData("Invalid NMEA data");
    EXPECT_FALSE(result);
}

// Test handlePositionUpdate method
TEST_F(GPSTrackerTest, HandlePositionUpdate) {
    auto tracker = createTrackerWithMockParser();
    
    // Set up a GGA data structure to be returned by the mock
    CNMEAParserData::GGA_DATA_T ggaData;
    ggaData.dLatitude = 37.7749;
    ggaData.dLongitude = -122.4194;
    ggaData.dAltitudeMSL = 10.0;
    
    // Set up expectations for the mock parser
    EXPECT_CALL(*mock_parser_, GetGPGGA(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(ggaData),
            ::testing::Return(CNMEAParserData::ERROR_OK)
        ));
    
    bool callback_called = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    std::string timestamp;
    
    tracker->registerPositionCallback([&](double latitude, double longitude, double altitude, const std::string& time) {
        callback_called = true;
        lat = latitude;
        lon = longitude;
        alt = altitude;
        timestamp = time;
    });
    
    // Call handlePositionUpdate
    tracker->handlePositionUpdate(37.7749, -122.4194, 10.0);
    
    // Verify the callback was called with correct values
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, lat);
    EXPECT_DOUBLE_EQ(-122.4194, lon);
    EXPECT_DOUBLE_EQ(10.0, alt);
    EXPECT_FALSE(timestamp.empty());
}

// Test serial port operations
TEST_F(GPSTrackerTest, SerialPortOperations) {
    auto tracker = createTrackerWithMockParser();
    
    // Test opening a serial port
    bool result = tracker->openSerialPort("/dev/ttyUSB0");
    EXPECT_FALSE(result);  // Should be false in our mock implementation
    EXPECT_EQ("/dev/ttyUSB0", tracker->serial_port_);
    
    // Test closing a serial port
    tracker->closeSerialPort();
    EXPECT_FALSE(tracker->is_port_open_);
    
    // Test reading serial data
    std::string data;
    result = tracker->readSerialData(data);
    EXPECT_TRUE(result);
    EXPECT_TRUE(data.empty());
}

// Test NMEA sentence generation in simulatePosition
TEST_F(GPSTrackerTest, NMEASentenceGeneration) {
    auto tracker = createTrackerWithMockParser();
    
    // Capture the NMEA sentence generated
    std::string captured_nmea;
    ON_CALL(*mock_parser_, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillByDefault([&captured_nmea](char* pBuffer, int nBufferSize) {
            captured_nmea = std::string(pBuffer, nBufferSize);
            return CNMEAParserData::ERROR_OK;
        });
    
    // Simulate a position
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    
    // Verify the NMEA sentence format
    EXPECT_THAT(captured_nmea, ::testing::HasSubstr("$GPGGA"));
    EXPECT_THAT(captured_nmea, ::testing::HasSubstr("3746.4940"));  // Latitude in DDMM.MMMM format
    EXPECT_THAT(captured_nmea, ::testing::HasSubstr("N"));          // North
    EXPECT_THAT(captured_nmea, ::testing::HasSubstr("12225.1640")); // Longitude in DDDMM.MMMM format
    EXPECT_THAT(captured_nmea, ::testing::HasSubstr("W"));          // West
    EXPECT_THAT(captured_nmea, ::testing::HasSubstr("10.0"));       // Altitude
}

// Test edge cases
TEST_F(GPSTrackerTest, EdgeCases) {
    auto tracker = createTrackerWithMockParser();
    
    // Test with null parser
    tracker->nmea_parser_.reset(nullptr);
    bool result = tracker->processNMEAData("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n");
    EXPECT_FALSE(result);
    
    // Test with empty NMEA data
    tracker->nmea_parser_.reset(mock_parser_);
    EXPECT_CALL(*mock_parser_, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_OK));
    result = tracker->processNMEAData("");
    EXPECT_TRUE(result);
    
    // Test with extreme coordinates
    bool callback_called = false;
    tracker->registerPositionCallback([&](double latitude, double longitude, double altitude, const std::string& time) {
        callback_called = true;
    });
    
    EXPECT_CALL(*mock_parser_, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_OK));
    tracker->simulatePosition(90.0, 180.0, 9999.9);
    EXPECT_TRUE(callback_called);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>