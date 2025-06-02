// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <sstream>
#include <string>
#include "equipment_tracker/gps_tracker.h"
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/time_utils.h"

// Mock for the NMEAParser
class MockNMEAParser : public CNMEAParser {
public:
    MOCK_METHOD(CNMEAParserData::ERROR_E, ProcessNMEABuffer, (char* pBuffer, int iSize), (override));
    MOCK_METHOD(CNMEAParserData::ERROR_E, GetGPGGA, (CNMEAParserData::GGA_DATA_T& ggaData), (override));
    MOCK_METHOD(void, OnError, (CNMEAParserData::ERROR_E nError, char* pCmd), (override));
    MOCK_METHOD(void, LockDataAccess, (), (override));
    MOCK_METHOD(void, UnlockDataAccess, (), (override));
};

// Mock for the EquipmentNMEAParser
class MockEquipmentNMEAParser : public equipment_tracker::EquipmentNMEAParser {
public:
    MOCK_METHOD(CNMEAParserData::ERROR_E, ProcessNMEABuffer, (char* pBuffer, int iSize), (override));
    MOCK_METHOD(CNMEAParserData::ERROR_E, GetGPGGA, (CNMEAParserData::GGA_DATA_T& ggaData), (override));
    MOCK_METHOD(void, OnError, (CNMEAParserData::ERROR_E nError, char* pCmd), (override));
    MOCK_METHOD(void, LockDataAccess, (), (override));
    MOCK_METHOD(void, UnlockDataAccess, (), (override));
    MOCK_METHOD(void, triggerPositionCallback, (double latitude, double longitude, double altitude), (override));
    
    void setPositionCallback(equipment_tracker::PositionCallback callback) {
        equipment_tracker::EquipmentNMEAParser::setPositionCallback(callback);
    }
};

// Test fixture for EquipmentNMEAParser
class EquipmentNMEAParserTest : public ::testing::Test {
protected:
    equipment_tracker::EquipmentNMEAParser parser;
    bool callback_called = false;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    equipment_tracker::Timestamp timestamp;

    void SetUp() override {
        parser.setPositionCallback([this](double lat, double lon, double alt, equipment_tracker::Timestamp ts) {
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
    std::unique_ptr<equipment_tracker::GPSTracker> tracker;
    bool callback_called = false;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    equipment_tracker::Timestamp timestamp;

    void SetUp() override {
        tracker = std::make_unique<equipment_tracker::GPSTracker>(100); // Short interval for tests
        tracker->registerPositionCallback([this](double lat, double lon, double alt, equipment_tracker::Timestamp ts) {
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
TEST_F(EquipmentNMEAParserTest, OnErrorHandlesErrorCorrectly) {
    // Redirect cerr to capture output
    std::stringstream buffer;
    std::streambuf* old = std::cerr.rdbuf(buffer.rdbuf());

    // Test with command
    char cmd[] = "GPGGA";
    parser.OnError(CNMEAParserData::ERROR_UNKNOWN, cmd);
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("NMEA Parser Error: 1 for command: GPGGA"));

    // Reset buffer
    buffer.str("");

    // Test without command
    parser.OnError(CNMEAParserData::ERROR_UNKNOWN, nullptr);
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("NMEA Parser Error: 1"));

    // Restore cerr
    std::cerr.rdbuf(old);
}

TEST_F(EquipmentNMEAParserTest, ProcessNMEABufferTriggersCallback) {
    // Create a mock NMEA sentence
    std::string nmea_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    // Create a GGA data structure that would be returned by GetGPGGA
    CNMEAParserData::GGA_DATA_T ggaData;
    ggaData.dLatitude = 48.1173;
    ggaData.dLongitude = 11.5167;
    ggaData.dAltitudeMSL = 545.4;
    
    // Use a mock to test the behavior
    MockEquipmentNMEAParser mockParser;
    
    // Set expectations
    EXPECT_CALL(mockParser, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_OK));
    
    EXPECT_CALL(mockParser, GetGPGGA(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(ggaData),
            ::testing::Return(CNMEAParserData::ERROR_OK)
        ));
    
    EXPECT_CALL(mockParser, triggerPositionCallback(48.1173, 11.5167, 545.4))
        .Times(1);
    
    // Register a callback
    bool callback_called = false;
    mockParser.setPositionCallback([&callback_called](double, double, double, equipment_tracker::Timestamp) {
        callback_called = true;
    });
    
    // Process the NMEA data
    mockParser.ProcessNMEABuffer(const_cast<char*>(nmea_data.c_str()), static_cast<int>(nmea_data.length()));
    
    // The mock will verify the expectations
}

TEST_F(EquipmentNMEAParserTest, TriggerPositionCallbackWorks) {
    // Test direct triggering of the callback
    parser.triggerPositionCallback(37.7749, -122.4194, 10.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, latitude);
    EXPECT_DOUBLE_EQ(-122.4194, longitude);
    EXPECT_DOUBLE_EQ(10.0, altitude);
    EXPECT_NE(timestamp, equipment_tracker::Timestamp{});
}

// Tests for GPSTracker
TEST_F(GPSTrackerTest, ConstructorSetsDefaultValues) {
    equipment_tracker::GPSTracker tracker;
    EXPECT_EQ(tracker.getUpdateInterval(), equipment_tracker::DEFAULT_UPDATE_INTERVAL_MS);
    EXPECT_FALSE(tracker.isRunning());
}

TEST_F(GPSTrackerTest, StartStopControlsRunningState) {
    EXPECT_FALSE(tracker->isRunning());
    
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());
    
    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, SetUpdateIntervalChangesInterval) {
    EXPECT_EQ(tracker->getUpdateInterval(), 100); // From our setup
    
    tracker->setUpdateInterval(200);
    EXPECT_EQ(tracker->getUpdateInterval(), 200);
}

TEST_F(GPSTrackerTest, SimulatePositionTriggersCallback) {
    // Simulate a position update
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    
    // Check that the callback was called with the correct values
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, latitude);
    EXPECT_DOUBLE_EQ(-122.4194, longitude);
    EXPECT_DOUBLE_EQ(10.0, altitude);
    EXPECT_NE(timestamp, equipment_tracker::Timestamp{});
}

TEST_F(GPSTrackerTest, ProcessNMEADataHandlesValidData) {
    // Create a valid NMEA sentence
    std::string nmea_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    // Process the data
    bool result = tracker->processNMEAData(nmea_data);
    
    // Since we're using a mock NMEA parser, this should succeed
    EXPECT_TRUE(result);
}

TEST_F(GPSTrackerTest, SimulatePositionGeneratesValidNMEA) {
    // Reset callback flag
    callback_called = false;
    
    // Simulate a position
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    
    // Verify callback was triggered
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, latitude);
    EXPECT_DOUBLE_EQ(-122.4194, longitude);
    EXPECT_DOUBLE_EQ(10.0, altitude);
}

TEST_F(GPSTrackerTest, RegisterPositionCallbackWorks) {
    // Reset callback flag
    callback_called = false;
    
    // Register a new callback
    bool new_callback_called = false;
    double new_lat = 0.0, new_lon = 0.0, new_alt = 0.0;
    
    tracker->registerPositionCallback([&](double lat, double lon, double alt, equipment_tracker::Timestamp) {
        new_callback_called = true;
        new_lat = lat;
        new_lon = lon;
        new_alt = alt;
    });
    
    // Simulate a position
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    
    // Original callback should not be called
    EXPECT_FALSE(callback_called);
    
    // New callback should be called
    EXPECT_TRUE(new_callback_called);
    EXPECT_DOUBLE_EQ(37.7749, new_lat);
    EXPECT_DOUBLE_EQ(-122.4194, new_lon);
    EXPECT_DOUBLE_EQ(10.0, new_alt);
}

TEST_F(GPSTrackerTest, StartCreatesWorkerThread) {
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());
    
    // Sleep briefly to allow the worker thread to run
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // The worker thread should have called simulatePosition at least once
    EXPECT_TRUE(callback_called);
    
    // Stop the tracker
    tracker->stop();
}

TEST_F(GPSTrackerTest, StopJoinsWorkerThread) {
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());
    
    // Sleep briefly to allow the worker thread to run
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Stop the tracker
    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());
    
    // Reset callback flag
    callback_called = false;
    
    // Sleep again to verify no more callbacks are triggered
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // No new callbacks should have been triggered
    EXPECT_FALSE(callback_called);
}

// Main function that runs all tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>