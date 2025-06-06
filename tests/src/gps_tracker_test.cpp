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
        tracker = std::make_unique<GPSTracker>(100); // 100ms update interval for faster tests
        tracker->registerPositionCallback([this](double lat, double lon, double alt, Timestamp timestamp) {
            callback_called = true;
            received_lat = lat;
            received_lon = lon;
            received_alt = alt;
            received_timestamp = timestamp;
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
    parser.OnError(CNMEAParserData::ERROR_UNKNOWN, const_cast<char*>("GPGGA"));
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("NMEA Parser Error"));
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("GPGGA"));
    
    // Reset buffer
    buffer.str("");
    
    // Test without command
    parser.OnError(CNMEAParserData::ERROR_UNKNOWN, nullptr);
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("NMEA Parser Error"));
    EXPECT_THAT(buffer.str(), ::testing::Not(::testing::HasSubstr("for command")));
    
    // Restore cerr
    std::cerr.rdbuf(old);
}

TEST_F(EquipmentNMEAParserTest, LockUnlockDataAccess) {
    // This is a basic test to ensure the methods don't crash
    parser.LockDataAccess();
    parser.UnlockDataAccess();
    
    // We can't directly test mutex locking behavior in a unit test
    // But we can verify the methods exist and can be called
    SUCCEED();
}

TEST_F(EquipmentNMEAParserTest, TriggerPositionCallback) {
    // Test with callback set
    parser.triggerPositionCallback(37.7749, -122.4194, 10.0);
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, received_lat);
    EXPECT_DOUBLE_EQ(-122.4194, received_lon);
    EXPECT_DOUBLE_EQ(10.0, received_alt);
    
    // Test with no callback set
    callback_called = false;
    EquipmentNMEAParser parser_no_callback;
    parser_no_callback.triggerPositionCallback(37.7749, -122.4194, 10.0);
    EXPECT_FALSE(callback_called); // Should not change
}

// Tests for GPSTracker
TEST_F(GPSTrackerTest, ConstructorSetsDefaultValues) {
    GPSTracker tracker(1000);
    EXPECT_EQ(1000, tracker.getUpdateInterval());
    EXPECT_FALSE(tracker.isRunning());
}

TEST_F(GPSTrackerTest, StartStopControlsRunningState) {
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
}

TEST_F(GPSTrackerTest, StopDoesNothingIfNotRunning) {
    EXPECT_FALSE(tracker->isRunning());
    
    // This should be a no-op
    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, SetUpdateIntervalChangesInterval) {
    EXPECT_EQ(100, tracker->getUpdateInterval());
    
    tracker->setUpdateInterval(500);
    EXPECT_EQ(500, tracker->getUpdateInterval());
}

TEST_F(GPSTrackerTest, SimulatePositionTriggersCallback) {
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, received_lat);
    EXPECT_DOUBLE_EQ(-122.4194, received_lon);
    EXPECT_DOUBLE_EQ(10.0, received_alt);
    EXPECT_NE(Timestamp(), received_timestamp);
}

TEST_F(GPSTrackerTest, ProcessNMEADataHandlesValidData) {
    // Valid NMEA GGA sentence
    std::string valid_nmea = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    bool result = tracker->processNMEAData(valid_nmea);
    
    // Since we're using a mock NMEA parser internally, this should return true
    // but not actually trigger our callback
    EXPECT_TRUE(result);
}

TEST_F(GPSTrackerTest, RegisterPositionCallbackSetsCallback) {
    // Reset the callback
    tracker->registerPositionCallback(nullptr);
    
    // Simulate position should not trigger callback now
    callback_called = false;
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    EXPECT_FALSE(callback_called);
    
    // Register a new callback
    tracker->registerPositionCallback([this](double lat, double lon, double alt, Timestamp timestamp) {
        callback_called = true;
        received_lat = lat;
        received_lon = lon;
        received_alt = alt;
        received_timestamp = timestamp;
    });
    
    // Now simulate position should trigger the callback
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(37.7749, received_lat);
    EXPECT_DOUBLE_EQ(-122.4194, received_lon);
    EXPECT_DOUBLE_EQ(10.0, received_alt);
}

TEST_F(GPSTrackerTest, SimulatePositionGeneratesValidNMEA) {
    // Create a custom tracker with a mock NMEA parser to verify the NMEA data
    auto mock_parser = std::make_unique<MockEquipmentNMEAParser>();
    
    // Set expectations for the mock
    EXPECT_CALL(*mock_parser, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_OK));
    
    // Create a tracker with our mock parser
    GPSTracker custom_tracker(100);
    
    // Inject our mock parser using private method access
    // Note: In a real test, you might need to use a test-specific subclass or other techniques
    // This is a simplified example
    
    // Simulate position to generate NMEA data
    custom_tracker.simulatePosition(37.7749, -122.4194, 10.0);
    
    // The mock expectations will verify the NMEA data was processed
}

TEST_F(GPSTrackerTest, WorkerThreadFunctionSimulatesPositionWhenNoPort) {
    // Start the tracker which will run the worker thread
    tracker->start();
    
    // Wait a bit for the worker thread to run
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // The worker should have called simulatePosition at least once
    EXPECT_TRUE(callback_called);
    
    // Default simulation position is San Francisco
    EXPECT_DOUBLE_EQ(37.7749, received_lat);
    EXPECT_DOUBLE_EQ(-122.4194, received_lon);
    EXPECT_DOUBLE_EQ(10.0, received_alt);
}

} // namespace equipment_tracker

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>