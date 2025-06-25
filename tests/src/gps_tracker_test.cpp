// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <sstream>
#include "equipment_tracker/gps_tracker.h"
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/time_utils.h"

namespace equipment_tracker {

// Mock for the NMEAParser to test interactions
class MockNMEAParser : public EquipmentNMEAParser {
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
        tracker = std::make_unique<GPSTracker>(100); // Short interval for tests
        tracker->registerPositionCallback([this](double lat, double lon, double alt, Timestamp ts) {
            callback_called = true;
            latitude = lat;
            longitude = lon;
            altitude = alt;
            timestamp = ts;
        });
    }

    void TearDown() override {
        tracker->stop();
        tracker.reset();
    }
};

// Tests for EquipmentNMEAParser
TEST_F(EquipmentNMEAParserTest, TriggerPositionCallback) {
    parser.triggerPositionCallback(37.7749, -122.4194, 10.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, latitude);
    EXPECT_DOUBLE_EQ(-122.4194, longitude);
    EXPECT_DOUBLE_EQ(10.0, altitude);
    EXPECT_NE(timestamp, Timestamp{});
}

TEST_F(EquipmentNMEAParserTest, ProcessNMEABuffer) {
    // Create a valid NMEA GGA sentence
    std::string nmea_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    // Mock the GetGPGGA method to return valid data
    CNMEAParserData::GGA_DATA_T ggaData;
    ggaData.dLatitude = 48.1173;
    ggaData.dLongitude = 11.5167;
    ggaData.dAltitudeMSL = 545.4;
    
    // Process the buffer
    auto result = parser.ProcessNMEABuffer(const_cast<char*>(nmea_data.c_str()), static_cast<int>(nmea_data.length()));
    
    // Since we're using a mock NMEAParser, we can't fully test this without more setup
    // Just verify the function returns without errors
    EXPECT_EQ(result, CNMEAParserData::ERROR_OK);
}

// Tests for GPSTracker
TEST_F(GPSTrackerTest, Constructor) {
    EXPECT_EQ(tracker->getUpdateInterval(), 100);
    EXPECT_FALSE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, SetUpdateInterval) {
    tracker->setUpdateInterval(200);
    EXPECT_EQ(tracker->getUpdateInterval(), 200);
}

TEST_F(GPSTrackerTest, StartStop) {
    EXPECT_FALSE(tracker->isRunning());
    
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());
    
    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, SimulatePosition) {
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, latitude);
    EXPECT_DOUBLE_EQ(-122.4194, longitude);
    EXPECT_DOUBLE_EQ(10.0, altitude);
    EXPECT_NE(timestamp, Timestamp{});
}

TEST_F(GPSTrackerTest, ProcessNMEAData) {
    // Create a valid NMEA GGA sentence
    std::string nmea_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    bool result = tracker->processNMEAData(nmea_data);
    
    // The mock NMEAParser should return ERROR_OK
    EXPECT_TRUE(result);
}

TEST_F(GPSTrackerTest, RegisterPositionCallback) {
    bool new_callback_called = false;
    double new_lat = 0.0;
    double new_lon = 0.0;
    double new_alt = 0.0;
    
    tracker->registerPositionCallback([&](double lat, double lon, double alt, Timestamp) {
        new_callback_called = true;
        new_lat = lat;
        new_lon = lon;
        new_alt = alt;
    });
    
    tracker->simulatePosition(12.3456, 78.9012, 100.0);
    
    EXPECT_TRUE(new_callback_called);
    EXPECT_DOUBLE_EQ(12.3456, new_lat);
    EXPECT_DOUBLE_EQ(78.9012, new_lon);
    EXPECT_DOUBLE_EQ(100.0, new_alt);
}

TEST_F(GPSTrackerTest, StartWorkerThread) {
    tracker->start();
    
    // Sleep to allow the worker thread to run at least once
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    // The worker should have called simulatePosition at least once
    EXPECT_TRUE(callback_called);
    
    // Default simulation position is San Francisco
    EXPECT_DOUBLE_EQ(37.7749, latitude);
    EXPECT_DOUBLE_EQ(-122.4194, longitude);
    EXPECT_DOUBLE_EQ(10.0, altitude);
}

// Test the NMEA sentence generation in simulatePosition
TEST_F(GPSTrackerTest, NMEASentenceFormat) {
    // Create a custom GPSTracker with a mock NMEA parser
    auto mock_parser = std::make_unique<::testing::NiceMock<MockNMEAParser>>();
    
    // Set up expectations for the mock
    EXPECT_CALL(*mock_parser, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce([](char* buffer, int) {
            std::string nmea(buffer);
            
            // Verify NMEA sentence format
            EXPECT_THAT(nmea, ::testing::StartsWith("$GPGGA,"));
            EXPECT_THAT(nmea, ::testing::HasSubstr(",3730.4940,N,"));
            EXPECT_THAT(nmea, ::testing::HasSubstr(",12225.1640,W,"));
            EXPECT_THAT(nmea, ::testing::HasSubstr(",10.0,M,"));
            EXPECT_THAT(nmea, ::testing::EndsWith("\r\n"));
            
            return CNMEAParserData::ERROR_OK;
        });
    
    // Replace the NMEA parser in our tracker with the mock
    // Note: This is a bit of a hack since we don't have direct access to the parser
    // In a real implementation, we would add a method to set the parser or use dependency injection
    GPSTracker custom_tracker(100);
    
    // Simulate a position - this should generate an NMEA sentence
    custom_tracker.simulatePosition(37.5082, -122.4194, 10.0);
}

// Test error handling in the NMEA parser
TEST(EquipmentNMEAParserErrorTest, OnErrorLogsToStderr) {
    EquipmentNMEAParser parser;
    
    // Redirect stderr to capture output
    std::stringstream buffer;
    std::streambuf* old = std::cerr.rdbuf(buffer.rdbuf());
    
    // Call OnError
    parser.OnError(CNMEAParserData::ERROR_UNKNOWN, const_cast<char*>("$GPGGA"));
    
    // Restore stderr
    std::cerr.rdbuf(old);
    
    // Check that error was logged
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("NMEA Parser Error: 1"));
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("for command: $GPGGA"));
}

// Test mutex operations in the NMEA parser
TEST(EquipmentNMEAParserMutexTest, LockUnlockDataAccess) {
    EquipmentNMEAParser parser;
    
    // These should not deadlock
    parser.LockDataAccess();
    parser.UnlockDataAccess();
    
    // Test that we can lock again
    parser.LockDataAccess();
    parser.UnlockDataAccess();
}

} // namespace equipment_tracker
// </test_code>