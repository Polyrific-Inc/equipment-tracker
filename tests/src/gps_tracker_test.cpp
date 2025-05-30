// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <string>
#include <sstream>
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
};

namespace equipment_tracker {

// Test fixture for EquipmentNMEAParser
class EquipmentNMEAParserTest : public ::testing::Test {
protected:
    EquipmentNMEAParser parser;
    bool callback_called = false;
    double received_lat = 0.0;
    double received_lon = 0.0;
    double received_alt = 0.0;

    void SetUp() override {
        parser.setPositionCallback([this](double lat, double lon, double alt, Timestamp) {
            callback_called = true;
            received_lat = lat;
            received_lon = lon;
            received_alt = alt;
        });
    }
};

// Test fixture for GPSTracker
class GPSTrackerTest : public ::testing::Test {
protected:
    std::unique_ptr<GPSTracker> tracker;
    bool callback_called = false;
    double received_lat = 0.0;
    double received_lon = 0.0;
    double received_alt = 0.0;
    Timestamp received_timestamp;

    void SetUp() override {
        tracker = std::make_unique<GPSTracker>(100); // Short interval for tests
        tracker->registerPositionCallback([this](double lat, double lon, double alt, Timestamp timestamp) {
            callback_called = true;
            received_lat = lat;
            received_lon = lon;
            received_alt = alt;
            received_timestamp = timestamp;
        });
    }

    void TearDown() override {
        if (tracker && tracker->isRunning()) {
            tracker->stop();
        }
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

TEST_F(EquipmentNMEAParserTest, TriggerPositionCallbackCallsCallback) {
    // Test triggering the callback
    parser.triggerPositionCallback(37.7749, -122.4194, 10.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, received_lat);
    EXPECT_DOUBLE_EQ(-122.4194, received_lon);
    EXPECT_DOUBLE_EQ(10.0, received_alt);
}

TEST_F(EquipmentNMEAParserTest, ProcessNMEABufferTriggersCallback) {
    // Create a valid NMEA GGA sentence
    std::string nmea_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    // Set up a mock to return valid GGA data
    CNMEAParserData::GGA_DATA_T ggaData;
    ggaData.dLatitude = 48.1173;
    ggaData.dLongitude = 11.5167;
    ggaData.dAltitudeMSL = 545.4;
    
    // Create a test instance with a mock implementation
    class TestParser : public EquipmentNMEAParser {
    public:
        CNMEAParserData::ERROR_E GetGPGGA(CNMEAParserData::GGA_DATA_T& data) override {
            data = test_data;
            return CNMEAParserData::ERROR_OK;
        }
        
        CNMEAParserData::GGA_DATA_T test_data;
    };
    
    TestParser test_parser;
    test_parser.test_data = ggaData;
    
    bool test_callback_called = false;
    double test_lat = 0.0, test_lon = 0.0, test_alt = 0.0;
    
    test_parser.setPositionCallback([&](double lat, double lon, double alt, Timestamp) {
        test_callback_called = true;
        test_lat = lat;
        test_lon = lon;
        test_alt = alt;
    });
    
    // Process the NMEA buffer
    test_parser.ProcessNMEABuffer(const_cast<char*>(nmea_data.c_str()), static_cast<int>(nmea_data.length()));
    
    // Verify callback was called with correct data
    EXPECT_TRUE(test_callback_called);
    EXPECT_DOUBLE_EQ(48.1173, test_lat);
    EXPECT_DOUBLE_EQ(11.5167, test_lon);
    EXPECT_DOUBLE_EQ(545.4, test_alt);
}

// Tests for GPSTracker

TEST_F(GPSTrackerTest, ConstructorSetsDefaultValues) {
    GPSTracker tracker;
    EXPECT_EQ(DEFAULT_UPDATE_INTERVAL_MS, tracker.getUpdateInterval());
    EXPECT_FALSE(tracker.isRunning());
}

TEST_F(GPSTrackerTest, SetUpdateIntervalChangesInterval) {
    tracker->setUpdateInterval(2000);
    EXPECT_EQ(2000, tracker->getUpdateInterval());
}

TEST_F(GPSTrackerTest, StartAndStopControlsRunningState) {
    EXPECT_FALSE(tracker->isRunning());
    
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());
    
    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, StartDoesNothingIfAlreadyRunning) {
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());
    
    // This should be a no-op
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());
    
    tracker->stop();
}

TEST_F(GPSTrackerTest, StopDoesNothingIfNotRunning) {
    EXPECT_FALSE(tracker->isRunning());
    
    // This should be a no-op
    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, SimulatePositionTriggersCallback) {
    // Reset callback state
    callback_called = false;
    
    // Simulate a position
    double test_lat = 37.7749;
    double test_lon = -122.4194;
    double test_alt = 10.0;
    
    tracker->simulatePosition(test_lat, test_lon, test_alt);
    
    // Verify callback was called with correct data
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(test_lat, received_lat);
    EXPECT_DOUBLE_EQ(test_lon, received_lon);
    EXPECT_DOUBLE_EQ(test_alt, received_alt);
    
    // Verify timestamp is recent (within last second)
    auto now = getCurrentTimestamp();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - received_timestamp).count();
    EXPECT_LT(diff, 1000);
}

TEST_F(GPSTrackerTest, ProcessNMEADataHandlesValidData) {
    // Create a valid NMEA GGA sentence
    std::string nmea_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    // Process the data
    bool result = tracker->processNMEAData(nmea_data);
    
    // Since we're using a mock NMEA parser, this should succeed
    EXPECT_TRUE(result);
}

TEST_F(GPSTrackerTest, SimulatePositionGeneratesValidNMEA) {
    // Reset callback state
    callback_called = false;
    
    // Simulate a position
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    
    // Verify callback was called
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, received_lat);
    EXPECT_DOUBLE_EQ(-122.4194, received_lon);
    EXPECT_DOUBLE_EQ(10.0, received_alt);
}

// Test with a custom mock NMEA parser
TEST(GPSTrackerCustomTest, UsesCustomNMEAParser) {
    // Create a mock NMEA parser
    auto mock_parser = std::make_unique<MockEquipmentNMEAParser>();
    
    // Set expectations
    EXPECT_CALL(*mock_parser, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_OK));
    
    // Create a test instance that can use our mock
    class TestGPSTracker : public GPSTracker {
    public:
        TestGPSTracker(std::unique_ptr<EquipmentNMEAParser> parser) 
            : GPSTracker(100) {
            // Replace the parser with our mock
            nmea_parser_ = std::move(parser);
        }
        
        // Expose the parser for testing
        std::unique_ptr<EquipmentNMEAParser>& getNMEAParser() {
            return nmea_parser_;
        }
    };
    
    // Create the test tracker with our mock parser
    TestGPSTracker test_tracker(std::move(mock_parser));
    
    // Process some NMEA data to verify our mock is used
    std::string nmea_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    test_tracker.processNMEAData(nmea_data);
    
    // The expectation on the mock should be satisfied
}

} // namespace equipment_tracker
// </test_code>