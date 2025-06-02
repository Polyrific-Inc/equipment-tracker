// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/utils/types.h"
#include "equipment_tracker/utils/constants.h"
#include <NMEAParser.h>
#include <string>
#include <thread>
#include <chrono>
#include <memory>

namespace equipment_tracker {

// Mock for EquipmentNMEAParser to test GPSTracker
class MockEquipmentNMEAParser : public EquipmentNMEAParser {
public:
    MOCK_METHOD(CNMEAParserData::ERROR_E, ProcessNMEABuffer, (char* pBuffer, int iSize), (override));
    MOCK_METHOD(void, setPositionCallback, (PositionCallback callback));
    MOCK_METHOD(void, triggerPositionCallback, (double latitude, double longitude, double altitude));
};

// Forward declaration of the implementation class for testing
class EquipmentNMEAParser : public CNMEAParser {
public:
    EquipmentNMEAParser() = default;
    void setPositionCallback(PositionCallback callback);
    CNMEAParserData::ERROR_E ProcessNMEABuffer(char* pBuffer, int iSize) override;
    void triggerPositionCallback(double latitude, double longitude, double altitude);
protected:
    void OnError(CNMEAParserData::ERROR_E nError, char* pCmd) override;
    void LockDataAccess() override;
    void UnlockDataAccess() override;
private:
    PositionCallback position_callback_;
    std::mutex mutex_;
};

class GPSTracker {
public:
    explicit GPSTracker(int update_interval_ms = DEFAULT_UPDATE_INTERVAL_MS);
    ~GPSTracker();
    void start();
    void stop();
    bool isRunning() const;
    void setUpdateInterval(int milliseconds);
    int getUpdateInterval() const;
    void registerPositionCallback(PositionCallback callback);
    void simulatePosition(double latitude, double longitude, double altitude = 0.0);
    bool processNMEAData(const std::string& data);
    
    // For testing purposes
    void setNMEAParser(std::unique_ptr<EquipmentNMEAParser> parser) {
        nmea_parser_ = std::move(parser);
    }
    
private:
    int update_interval_ms_;
    std::atomic<bool> is_running_;
    std::thread worker_thread_;
    PositionCallback position_callback_;
    std::unique_ptr<EquipmentNMEAParser> nmea_parser_;
    std::string serial_port_;
    int serial_baud_rate_{9600};
    bool is_port_open_{false};
    void workerThreadFunction();
    bool openSerialPort(const std::string& port_name);
    void closeSerialPort();
    bool readSerialData(std::string& data);
    void handlePositionUpdate(double latitude, double longitude, double altitude, Timestamp timestamp);
};

// Implementation of EquipmentNMEAParser for testing
void EquipmentNMEAParser::setPositionCallback(PositionCallback callback) {
    position_callback_ = callback;
}

CNMEAParserData::ERROR_E EquipmentNMEAParser::ProcessNMEABuffer(char* pBuffer, int iSize) {
    return CNMEAParser::ProcessNMEABuffer(pBuffer, iSize);
}

void EquipmentNMEAParser::triggerPositionCallback(double latitude, double longitude, double altitude) {
    if (position_callback_) {
        position_callback_(latitude, longitude, altitude, getCurrentTimestamp());
    }
}

void EquipmentNMEAParser::OnError(CNMEAParserData::ERROR_E nError, char* pCmd) {
    CNMEAParser::OnError(nError, pCmd);
}

void EquipmentNMEAParser::LockDataAccess() {
    mutex_.lock();
}

void EquipmentNMEAParser::UnlockDataAccess() {
    mutex_.unlock();
}

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
    std::unique_ptr<MockEquipmentNMEAParser> mock_parser;
    
    bool callback_called = false;
    double received_lat = 0.0;
    double received_lon = 0.0;
    double received_alt = 0.0;
    
    void SetUp() override {
        mock_parser = std::make_unique<MockEquipmentNMEAParser>();
        tracker = std::make_unique<GPSTracker>(100); // Short interval for testing
        
        // Set up position callback
        tracker->registerPositionCallback([this](double lat, double lon, double alt, Timestamp) {
            callback_called = true;
            received_lat = lat;
            received_lon = lon;
            received_alt = alt;
        });
    }
    
    void TearDown() override {
        if (tracker->isRunning()) {
            tracker->stop();
        }
        tracker.reset();
        mock_parser.reset();
    }
};

// Tests for EquipmentNMEAParser
TEST_F(EquipmentNMEAParserTest, TriggerPositionCallback) {
    // Test that the position callback is triggered correctly
    parser.triggerPositionCallback(45.0, -75.0, 100.0);
    
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(45.0, received_lat);
    EXPECT_DOUBLE_EQ(-75.0, received_lon);
    EXPECT_DOUBLE_EQ(100.0, received_alt);
}

TEST_F(EquipmentNMEAParserTest, NoCallbackRegistered) {
    // Create a new parser without a callback
    EquipmentNMEAParser no_callback_parser;
    
    // This should not crash
    no_callback_parser.triggerPositionCallback(45.0, -75.0, 100.0);
    
    // Original parser's callback should not be triggered
    EXPECT_FALSE(callback_called);
}

TEST_F(EquipmentNMEAParserTest, ProcessNMEABuffer) {
    // Create a simple NMEA sentence
    std::string nmea_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    char buffer[100];
    std::strcpy(buffer, nmea_data.c_str());
    
    // Process the buffer
    CNMEAParserData::ERROR_E result = parser.ProcessNMEABuffer(buffer, static_cast<int>(nmea_data.length()));
    
    // Since we're using a mock NMEA parser, we just verify it returns OK
    EXPECT_EQ(CNMEAParserData::ERROR_OK, result);
}

// Tests for GPSTracker
TEST_F(GPSTrackerTest, DefaultConstructor) {
    GPSTracker default_tracker;
    EXPECT_EQ(DEFAULT_UPDATE_INTERVAL_MS, default_tracker.getUpdateInterval());
    EXPECT_FALSE(default_tracker.isRunning());
}

TEST_F(GPSTrackerTest, CustomIntervalConstructor) {
    GPSTracker custom_tracker(1000);
    EXPECT_EQ(1000, custom_tracker.getUpdateInterval());
    EXPECT_FALSE(custom_tracker.isRunning());
}

TEST_F(GPSTrackerTest, SetUpdateInterval) {
    tracker->setUpdateInterval(2000);
    EXPECT_EQ(2000, tracker->getUpdateInterval());
}

TEST_F(GPSTrackerTest, StartStop) {
    EXPECT_FALSE(tracker->isRunning());
    
    tracker->start();
    EXPECT_TRUE(tracker->isRunning());
    
    tracker->stop();
    EXPECT_FALSE(tracker->isRunning());
}

TEST_F(GPSTrackerTest, SimulatePosition) {
    // Simulate a position update
    tracker->simulatePosition(40.0, -80.0, 200.0);
    
    // Check that the callback was called with correct values
    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(40.0, received_lat);
    EXPECT_DOUBLE_EQ(-80.0, received_lon);
    EXPECT_DOUBLE_EQ(200.0, received_alt);
}

TEST_F(GPSTrackerTest, ProcessNMEAData) {
    // Create a simple NMEA sentence
    std::string nmea_data = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    
    // Set expectations on the mock parser
    EXPECT_CALL(*mock_parser, ProcessNMEABuffer(testing::_, testing::_))
        .WillOnce(testing::Return(CNMEAParserData::ERROR_OK));
    
    // Set the mock parser in the tracker
    tracker->setNMEAParser(std::move(mock_parser));
    
    // Process the NMEA data
    bool result = tracker->processNMEAData(nmea_data);
    
    // Since we're using a mock, we can't fully test the processing,
    // but we can verify the method returns true when the parser returns ERROR_OK
    EXPECT_TRUE(result);
}

TEST_F(GPSTrackerTest, RegisterPositionCallback) {
    bool new_callback_called = false;
    double new_lat = 0.0, new_lon = 0.0, new_alt = 0.0;
    
    // Register a new callback
    tracker->registerPositionCallback([&](double lat, double lon, double alt, Timestamp) {
        new_callback_called = true;
        new_lat = lat;
        new_lon = lon;
        new_alt = alt;
    });
    
    // Simulate a position update
    tracker->simulatePosition(35.0, -70.0, 150.0);
    
    // Check that the new callback was called with correct values
    EXPECT_TRUE(new_callback_called);
    EXPECT_DOUBLE_EQ(35.0, new_lat);
    EXPECT_DOUBLE_EQ(-70.0, new_lon);
    EXPECT_DOUBLE_EQ(150.0, new_alt);
    
    // Original callback should not have been called
    EXPECT_FALSE(callback_called);
}

TEST_F(GPSTrackerTest, NoCallbackRegistered) {
    // Create a new tracker without registering a callback
    auto no_callback_tracker = std::make_unique<GPSTracker>();
    
    // This should not crash
    no_callback_tracker->simulatePosition(35.0, -70.0, 150.0);
    
    // Original tracker's callback should not be triggered
    EXPECT_FALSE(callback_called);
}

} // namespace equipment_tracker
// </test_code>