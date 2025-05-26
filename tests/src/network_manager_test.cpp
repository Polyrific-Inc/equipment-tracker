// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <iomanip>
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <ctime>
#include <random>
#include <atomic>
#include "equipment_tracker/network_manager.h"

// Mock for Position class
namespace equipment_tracker {
class MockPosition : public Position {
public:
    MockPosition(double lat = 0.0, double lon = 0.0, double alt = 0.0, double acc = 0.0)
        : latitude_(lat), longitude_(lon), altitude_(alt), accuracy_(acc),
          timestamp_(std::chrono::system_clock::now()) {}

    double getLatitude() const override { return latitude_; }
    double getLongitude() const override { return longitude_; }
    double getAltitude() const override { return altitude_; }
    double getAccuracy() const override { return accuracy_; }
    std::chrono::system_clock::time_point getTimestamp() const override { return timestamp_; }

private:
    double latitude_;
    double longitude_;
    double altitude_;
    double accuracy_;
    std::chrono::system_clock::time_point timestamp_;
};

// Test fixture for NetworkManager tests
class NetworkManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Redirect cout/cerr for testing
        old_cout_buf_ = std::cout.rdbuf();
        old_cerr_buf_ = std::cerr.rdbuf();
        std::cout.rdbuf(cout_buffer_.rdbuf());
        std::cerr.rdbuf(cerr_buffer_.rdbuf());
    }

    void TearDown() override {
        // Restore cout/cerr
        std::cout.rdbuf(old_cout_buf_);
        std::cerr.rdbuf(old_cerr_buf_);
    }

    std::stringstream cout_buffer_;
    std::stringstream cerr_buffer_;
    std::streambuf* old_cout_buf_;
    std::streambuf* old_cerr_buf_;
};

// Test constructor and initial state
TEST_F(NetworkManagerTest, Constructor) {
    NetworkManager manager("test.server.com", 8080);
    EXPECT_FALSE(manager.isConnected());
}

// Test connect method
TEST_F(NetworkManagerTest, Connect) {
    NetworkManager manager("test.server.com", 8080);
    EXPECT_TRUE(manager.connect());
    EXPECT_TRUE(manager.isConnected());
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("Connecting to server at test.server.com:8080"));
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("Connected to server"));
}

// Test disconnect method
TEST_F(NetworkManagerTest, Disconnect) {
    NetworkManager manager("test.server.com", 8080);
    manager.connect();
    cout_buffer_.str(""); // Clear buffer
    
    manager.disconnect();
    EXPECT_FALSE(manager.isConnected());
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("Disconnecting from server"));
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("Disconnected from server"));
}

// Test disconnect when not connected
TEST_F(NetworkManagerTest, DisconnectWhenNotConnected) {
    NetworkManager manager("test.server.com", 8080);
    cout_buffer_.str(""); // Clear buffer
    
    manager.disconnect();
    EXPECT_FALSE(manager.isConnected());
    EXPECT_EQ(cout_buffer_.str(), ""); // Should not output anything
}

// Test sendPositionUpdate when connected
TEST_F(NetworkManagerTest, SendPositionUpdateWhenConnected) {
    NetworkManager manager("test.server.com", 8080);
    manager.connect();
    cout_buffer_.str(""); // Clear buffer
    
    MockPosition position(37.7749, -122.4194, 10.0, 5.0);
    EXPECT_TRUE(manager.sendPositionUpdate("device123", position));
    
    // Sleep to allow worker thread to process
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("Sending position update to server"));
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("\"id\":\"device123\""));
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("\"latitude\":37.7749"));
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("\"longitude\":-122.4194"));
}

// Test sendPositionUpdate when not connected
TEST_F(NetworkManagerTest, SendPositionUpdateWhenNotConnected) {
    NetworkManager manager("test.server.com", 8080);
    cout_buffer_.str(""); // Clear buffer
    cerr_buffer_.str(""); // Clear buffer
    
    MockPosition position(37.7749, -122.4194, 10.0, 5.0);
    EXPECT_TRUE(manager.sendPositionUpdate("device123", position));
    
    // Should auto-connect
    EXPECT_TRUE(manager.isConnected());
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("Connecting to server"));
}

// Test syncWithServer when connected
TEST_F(NetworkManagerTest, SyncWithServerWhenConnected) {
    NetworkManager manager("test.server.com", 8080);
    manager.connect();
    cout_buffer_.str(""); // Clear buffer
    
    EXPECT_TRUE(manager.syncWithServer());
}

// Test syncWithServer when not connected
TEST_F(NetworkManagerTest, SyncWithServerWhenNotConnected) {
    NetworkManager manager("test.server.com", 8080);
    cout_buffer_.str(""); // Clear buffer
    cerr_buffer_.str(""); // Clear buffer
    
    EXPECT_TRUE(manager.syncWithServer());
    
    // Should auto-connect
    EXPECT_TRUE(manager.isConnected());
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("Connecting to server"));
}

// Test registerCommandHandler
TEST_F(NetworkManagerTest, RegisterCommandHandler) {
    NetworkManager manager("test.server.com", 8080);
    bool handlerCalled = false;
    std::string receivedCommand;
    
    manager.registerCommandHandler([&handlerCalled, &receivedCommand](const std::string& cmd) {
        handlerCalled = true;
        receivedCommand = cmd;
    });
    
    // We can't directly test the handler since it's called randomly in the worker thread
    // But we can verify it's registered by checking internal state indirectly
    manager.connect();
    
    // Just to ensure the test doesn't hang
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// Test setServerUrl
TEST_F(NetworkManagerTest, SetServerUrl) {
    NetworkManager manager("test.server.com", 8080);
    manager.connect();
    EXPECT_TRUE(manager.isConnected());
    
    manager.setServerUrl("new.server.com");
    EXPECT_FALSE(manager.isConnected()); // Should disconnect
    
    manager.connect();
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("Connecting to server at new.server.com:8080"));
}

// Test setServerPort
TEST_F(NetworkManagerTest, SetServerPort) {
    NetworkManager manager("test.server.com", 8080);
    manager.connect();
    EXPECT_TRUE(manager.isConnected());
    
    manager.setServerPort(9090);
    EXPECT_FALSE(manager.isConnected()); // Should disconnect
    
    manager.connect();
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("Connecting to server at test.server.com:9090"));
}

// Test sendRequest when connected
TEST_F(NetworkManagerTest, SendRequestWhenConnected) {
    NetworkManager manager("test.server.com", 8080);
    manager.connect();
    cout_buffer_.str(""); // Clear buffer
    
    EXPECT_TRUE(manager.sendRequest("/api/status", "{\"query\":\"status\"}"));
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("Sending request to test.server.com/api/status"));
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("{\"query\":\"status\"}"));
}

// Test sendRequest when not connected
TEST_F(NetworkManagerTest, SendRequestWhenNotConnected) {
    NetworkManager manager("test.server.com", 8080);
    cerr_buffer_.str(""); // Clear buffer
    
    EXPECT_FALSE(manager.sendRequest("/api/status", "{\"query\":\"status\"}"));
    EXPECT_THAT(cerr_buffer_.str(), ::testing::HasSubstr("Not connected to server"));
}

// Test receiveResponse when connected
TEST_F(NetworkManagerTest, ReceiveResponseWhenConnected) {
    NetworkManager manager("test.server.com", 8080);
    manager.connect();
    
    std::string response = manager.receiveResponse();
    EXPECT_EQ(response, "{\"status\":\"ok\"}");
}

// Test receiveResponse when not connected
TEST_F(NetworkManagerTest, ReceiveResponseWhenNotConnected) {
    NetworkManager manager("test.server.com", 8080);
    cerr_buffer_.str(""); // Clear buffer
    
    std::string response = manager.receiveResponse();
    EXPECT_EQ(response, "");
    EXPECT_THAT(cerr_buffer_.str(), ::testing::HasSubstr("Not connected to server"));
}

// Test destructor properly cleans up
TEST_F(NetworkManagerTest, DestructorCleansUp) {
    {
        NetworkManager manager("test.server.com", 8080);
        manager.connect();
        EXPECT_TRUE(manager.isConnected());
        // Manager will go out of scope and destructor should be called
    }
    // No explicit test assertion here, but if the destructor doesn't properly
    // clean up (especially the thread), we might see crashes or test failures
}

// Test multiple position updates
TEST_F(NetworkManagerTest, MultiplePositionUpdates) {
    NetworkManager manager("test.server.com", 8080);
    manager.connect();
    cout_buffer_.str(""); // Clear buffer
    
    MockPosition position1(37.7749, -122.4194, 10.0, 5.0);
    MockPosition position2(40.7128, -74.0060, 15.0, 3.0);
    
    EXPECT_TRUE(manager.sendPositionUpdate("device123", position1));
    EXPECT_TRUE(manager.sendPositionUpdate("device456", position2));
    
    // Sleep to allow worker thread to process
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    std::string output = cout_buffer_.str();
    EXPECT_THAT(output, ::testing::HasSubstr("\"id\":\"device123\""));
    EXPECT_THAT(output, ::testing::HasSubstr("\"latitude\":37.7749"));
    EXPECT_THAT(output, ::testing::HasSubstr("\"id\":\"device456\""));
    EXPECT_THAT(output, ::testing::HasSubstr("\"latitude\":40.7128"));
}

} // namespace equipment_tracker
// </test_code>