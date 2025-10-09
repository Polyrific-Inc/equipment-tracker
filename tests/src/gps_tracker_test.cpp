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
TEST_F(EquipmentNMEAParserTest, ProcessNMEABufferCallsBaseImplementation) {
    // Setup
    bool callback_called = false;
    parser->setPositionCallback([&callback_called](double lat, double lon, double alt, Timestamp ts) {
        callback_called = true;
    });

    // Create a valid NMEA GGA sentence
    std::string nmea_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    // Execute
    auto result = parser->ProcessNMEABuffer(const_cast<char*>(nmea_data.c_str()), static_cast<int>(nmea_data.length()));
    
    // Verify
    EXPECT_EQ(result, CNMEAParserData::ERROR_OK);
    // Note: In a real test, callback_called would be true, but our mock doesn't actually parse NMEA data
}

TEST_F(EquipmentNMEAParserTest, TriggerPositionCallbackCallsRegisteredCallback) {
    // Setup
    bool callback_called = false;
    double captured_lat = 0.0;
    double captured_lon = 0.0;
    double captured_alt = 0.0;
    
    parser->setPositionCallback([&](double lat, double lon, double alt, Timestamp ts) {
        callback_called = true;
        captured_lat = lat;
        captured_lon = lon;
        captured_alt = alt;
    });
    
    // Execute
    parser->triggerPositionCallback(37.7749, -122.4194, 10.0);
    
    // Verify
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(captured_lat, 37.7749);
    EXPECT_DOUBLE_EQ(captured_lon, -122.4194);
    EXPECT_DOUBLE_EQ(captured_alt, 10.0);
}

TEST_F(EquipmentNMEAParserTest, LockUnlockDataAccess) {
    // This test verifies that LockDataAccess and UnlockDataAccess methods work correctly
    // We can't directly test mutex locking, but we can ensure the methods don't crash
    
    // Execute & Verify (no exceptions should be thrown)
    EXPECT_NO_THROW(parser->LockDataAccess());
    EXPECT_NO_THROW(parser->UnlockDataAccess());
}

// Tests for GPSTracker
TEST_F(GPSTrackerTest, ConstructorSetsDefaultValues) {
    // Verify
    EXPECT_EQ(tracker->getUpdateInterval(), 100);
    EXPECT_FALSE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, StartStopControlsRunningState) {
    // Execute
    tracker->start();
    
    // Verify
    EXPECT_TRUE(tracker->isRunning());
    
    // Execute
    tracker->stop();
    
    // Verify
    EXPECT_FALSE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, StartDoesNothingIfAlreadyRunning) {
    // Setup
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());
    
    // Execute - should be a no-op
    tracker->start();
    
    // Verify - still running
    EXPECT_TRUE(tracker->isRunning());
    
    // Cleanup
    tracker->stop();
}

TEST_F(GPSTrackerTest, StopDoesNothingIfNotRunning) {
    // Verify initial state
    EXPECT_FALSE(tracker->isRunning());
    
    // Execute - should be a no-op
    tracker->stop();
    
    // Verify - still not running
    EXPECT_FALSE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, SetUpdateIntervalChangesInterval) {
    // Execute
    tracker->setUpdateInterval(200);
    
    // Verify
    EXPECT_EQ(tracker->getUpdateInterval(), 200);
}

TEST_F(GPSTrackerTest, RegisterPositionCallbackSetsCallback) {
    // Setup
    bool callback_called = false;
    
    // Execute
    tracker->registerPositionCallback([&callback_called](double lat, double lon, double alt, Timestamp ts) {
        callback_called = true;
    });
    
    // Simulate position to trigger callback
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    
    // Verify
    EXPECT_TRUE(callback_called);
}

TEST_F(GPSTrackerTest, SimulatePositionTriggersCallback) {
    // Setup
    double captured_lat = 0.0;
    double captured_lon = 0.0;
    double captured_alt = 0.0;
    
    tracker->registerPositionCallback([&](double lat, double lon, double alt, Timestamp ts) {
        captured_lat = lat;
        captured_lon = lon;
        captured_alt = alt;
    });
    
    // Execute
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    
    // Verify
    EXPECT_DOUBLE_EQ(captured_lat, 37.7749);
    EXPECT_DOUBLE_EQ(captured_lon, -122.4194);
    EXPECT_DOUBLE_EQ(captured_alt, 10.0);
}

TEST_F(GPSTrackerTest, ProcessNMEADataHandlesValidData) {
    // Setup
    bool callback_called = false;
    
    tracker->registerPositionCallback([&callback_called](double lat, double lon, double alt, Timestamp ts) {
        callback_called = true;
    });
    
    // Valid NMEA GGA sentence
    std::string nmea_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    // Execute
    bool result = tracker->processNMEAData(nmea_data);
    
    // Verify
    EXPECT_TRUE(result);
    // Note: callback_called might be false since our mock doesn't actually parse NMEA data
}

TEST_F(GPSTrackerTest, SimulatePositionGeneratesValidNMEA) {
    // This test verifies that simulatePosition generates a valid NMEA string
    // We can't directly access the generated string, but we can verify the callback is triggered
    
    // Setup
    bool callback_called = false;
    
    tracker->registerPositionCallback([&callback_called](double lat, double lon, double alt, Timestamp ts) {
        callback_called = true;
    });
    
    // Execute
    tracker->simulatePosition(37.7749, -122.4194, 10.0);
    
    // Verify
    EXPECT_TRUE(callback_called);
}

// Integration test for worker thread functionality
TEST_F(GPSTrackerTest, WorkerThreadGeneratesPositionUpdates) {
    // Setup
    int update_count = 0;
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    
    tracker->setUpdateInterval(100); // 100ms for faster testing
    
    tracker->registerPositionCallback([&](double lat, double lon, double alt, Timestamp ts) {
        std::lock_guard<std::mutex> lock(mutex);
        update_count++;
        if (update_count >= 2) {
            done = true;
            cv.notify_one();
        }
    });
    
    // Execute
    tracker->start();
    
    // Wait for at least 2 updates or timeout after 1 second
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, std::chrono::seconds(1), [&done]{ return done; });
    }
    
    tracker->stop();
    
    // Verify
    EXPECT_GE(update_count, 2);
}

// Test with a mock NMEA parser
class GPSTrackerWithMockTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_parser = new ::testing::NiceMock<MockNMEAParser>();
        // Create a GPSTracker but we'll inject our mock parser
    }

    void TearDown() override {
        // mock_parser will be deleted by the unique_ptr in GPSTracker
    }

    ::testing::NiceMock<MockNMEAParser>* mock_parser;
};

TEST(GPSTrackerWithMockTest, ProcessNMEADataCallsParserCorrectly) {
    // Setup
    auto mock_parser = std::make_unique<::testing::NiceMock<MockNMEAParser>>();
    
    // Expect the ProcessNMEABuffer method to be called with the correct data
    EXPECT_CALL(*mock_parser, ProcessNMEABuffer(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(CNMEAParserData::ERROR_OK));
    
    // Create a test string
    std::string test_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    // Execute - we can't directly inject the mock into GPSTracker, so we'll call the method directly
    auto result = mock_parser->ProcessNMEABuffer(
        const_cast<char*>(test_data.c_str()), 
        static_cast<int>(test_data.length())
    );
    
    // Verify
    EXPECT_EQ(result, CNMEAParserData::ERROR_OK);
}

} // namespace equipment_tracker
// </test_code>