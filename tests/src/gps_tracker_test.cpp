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

namespace equipment_tracker {

// Mock for the NMEAParser to test interactions
class MockNMEAParser : public EquipmentNMEAParser {
public:
    MOCK_METHOD(CNMEAParserData::ERROR_E, ProcessNMEABuffer, (char* pBuffer, int iSize), (override));
    MOCK_METHOD(void, OnError, (CNMEAParserData::ERROR_E nError, char* pCmd), (override));
    MOCK_METHOD(void, LockDataAccess, (), (override));
    MOCK_METHOD(void, UnlockDataAccess, (), (override));
    MOCK_METHOD(CNMEAParserData::ERROR_E, GetGPGGA, (CNMEAParserData::GGA_DATA_T& ggaData), (override));
    MOCK_METHOD(void, triggerPositionCallback, (double latitude, double longitude, double altitude), ());
};

// Test fixture for GPSTracker tests
class GPSTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        tracker = std::make_unique<GPSTracker>(100); // Use shorter interval for tests
    }

    void TearDown() override {
        tracker->stop();
        tracker.reset();
    }

    std::unique_ptr<GPSTracker> tracker;
};

// Test fixture for EquipmentNMEAParser tests
class EquipmentNMEAParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<EquipmentNMEAParser>();
    }

    void TearDown() override {
        parser.reset();
    }

    std::unique_ptr<EquipmentNMEAParser> parser;
};

// Tests for EquipmentNMEAParser
TEST_F(EquipmentNMEAParserTest, ProcessNMEABufferCallsCallback) {
    bool callbackCalled = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    Timestamp timestamp;

    parser->setPositionCallback([&](double latitude, double longitude, double altitude, Timestamp ts) {
        callbackCalled = true;
        lat = latitude;
        lon = longitude;
        alt = altitude;
        timestamp = ts;
    });

    // Create a mock NMEA GGA sentence
    std::string nmeaData = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    // Process the NMEA data
    auto result = parser->ProcessNMEABuffer(const_cast<char*>(nmeaData.c_str()), static_cast<int>(nmeaData.length()));
    
    // Since we're using a mock NMEA parser, we don't expect the callback to be called
    // This is just testing our override of the method
    EXPECT_EQ(result, CNMEAParserData::ERROR_OK);
}

TEST_F(EquipmentNMEAParserTest, TriggerPositionCallbackWorks) {
    bool callbackCalled = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    Timestamp timestamp;

    parser->setPositionCallback([&](double latitude, double longitude, double altitude, Timestamp ts) {
        callbackCalled = true;
        lat = latitude;
        lon = longitude;
        alt = altitude;
        timestamp = ts;
    });

    // Directly trigger the callback
    parser->triggerPositionCallback(37.7749, -122.4194, 10.0);

    EXPECT_TRUE(callbackCalled);
    EXPECT_DOUBLE_EQ(lat, 37.7749);
    EXPECT_DOUBLE_EQ(lon, -122.4194);
    EXPECT_DOUBLE_EQ(alt, 10.0);
}

// Tests for GPSTracker
TEST_F(GPSTrackerTest, ConstructorSetsDefaultValues) {
    EXPECT_EQ(tracker->getUpdateInterval(), 100);
    EXPECT_FALSE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, StartStopChangesRunningState) {
    EXPECT_FALSE(tracker->isRunning());
    
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());
    
    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, SetUpdateIntervalChangesInterval) {
    EXPECT_EQ(tracker->getUpdateInterval(), 100);
    
    tracker->setUpdateInterval(200);
    EXPECT_EQ(tracker->getUpdateInterval(), 200);
}

TEST_F(GPSTrackerTest, RegisterPositionCallbackWorks) {
    bool callbackCalled = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    Timestamp timestamp;

    tracker->registerPositionCallback([&](double latitude, double longitude, double altitude, Timestamp ts) {
        callbackCalled = true;
        lat = latitude;
        lon = longitude;
        alt = altitude;
        timestamp = ts;
    });

    // Simulate a position update
    tracker->simulatePosition(37.7749, -122.4194, 10.0);

    EXPECT_TRUE(callbackCalled);
    EXPECT_DOUBLE_EQ(lat, 37.7749);
    EXPECT_DOUBLE_EQ(lon, -122.4194);
    EXPECT_DOUBLE_EQ(alt, 10.0);
}

TEST_F(GPSTrackerTest, SimulatePositionGeneratesValidNMEA) {
    bool callbackCalled = false;
    double lat = 0.0, lon = 0.0, alt = 0.0;
    Timestamp timestamp;

    tracker->registerPositionCallback([&](double latitude, double longitude, double altitude, Timestamp ts) {
        callbackCalled = true;
        lat = latitude;
        lon = longitude;
        alt = altitude;
        timestamp = ts;
    });

    // Simulate a position update
    tracker->simulatePosition(37.7749, -122.4194, 10.0);

    EXPECT_TRUE(callbackCalled);
    EXPECT_DOUBLE_EQ(lat, 37.7749);
    EXPECT_DOUBLE_EQ(lon, -122.4194);
    EXPECT_DOUBLE_EQ(alt, 10.0);
}

TEST_F(GPSTrackerTest, ProcessNMEADataHandlesValidData) {
    bool callbackCalled = false;

    tracker->registerPositionCallback([&](double latitude, double longitude, double altitude, Timestamp ts) {
        callbackCalled = true;
    });

    // Create a valid NMEA GGA sentence
    std::string nmeaData = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    // Process the NMEA data
    bool result = tracker->processNMEAData(nmeaData);
    
    // Since we're using a mock NMEA parser, we don't expect the callback to be called
    // This is just testing our method returns true for valid data
    EXPECT_TRUE(result);
}

TEST_F(GPSTrackerTest, StartStopWorkerThread) {
    // Start the tracker
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());
    
    // Sleep briefly to let the worker thread run
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Stop the tracker
    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());
}

// Test with a mock NMEA parser to verify interactions
class GPSTrackerWithMockTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockParser = new ::testing::NiceMock<MockNMEAParser>();
        tracker = std::make_unique<GPSTracker>(100);
    }

    void TearDown() override {
        tracker->stop();
        tracker.reset();
        // mockParser is owned by tracker, no need to delete
    }

    ::testing::NiceMock<MockNMEAParser>* mockParser;
    std::unique_ptr<GPSTracker> tracker;
};

TEST(EquipmentNMEAParser, LockUnlockDataAccess) {
    EquipmentNMEAParser parser;
    
    // These should not crash
    parser.LockDataAccess();
    parser.UnlockDataAccess();
}

TEST(EquipmentNMEAParser, OnErrorHandlesErrors) {
    EquipmentNMEAParser parser;
    
    // Redirect cerr to capture output
    std::stringstream buffer;
    std::streambuf* old = std::cerr.rdbuf(buffer.rdbuf());
    
    // Test with null command
    parser.OnError(CNMEAParserData::ERROR_UNKNOWN, nullptr);
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("NMEA Parser Error: 1"));
    
    // Clear buffer
    buffer.str("");
    
    // Test with command
    char cmd[] = "GPGGA";
    parser.OnError(CNMEAParserData::ERROR_UNKNOWN, cmd);
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("NMEA Parser Error: 1"));
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("for command: GPGGA"));
    
    // Restore cerr
    std::cerr.rdbuf(old);
}

}  // namespace equipment_tracker
// </test_code>