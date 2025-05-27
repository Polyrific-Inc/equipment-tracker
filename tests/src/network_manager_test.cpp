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
#include <future>
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

// Redirect cout/cerr for testing output
class NetworkManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Redirect cout
        old_cout_buf_ = std::cout.rdbuf();
        std::cout.rdbuf(cout_buffer_.rdbuf());
        
        // Redirect cerr
        old_cerr_buf_ = std::cerr.rdbuf();
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

TEST_F(NetworkManagerTest, ConstructorSetsInitialState) {
    NetworkManager manager("test.server.com", 8080);
    
    // Test internal state through public methods
    EXPECT_FALSE(manager.isConnected());
}

TEST_F(NetworkManagerTest, ConnectSetsConnectedState) {
    NetworkManager manager("test.server.com", 8080);
    
    EXPECT_TRUE(manager.connect());
    EXPECT_TRUE(manager.isConnected());
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("Connected to server"));
}

TEST_F(NetworkManagerTest, ConnectWhenAlreadyConnectedReturnsTrue) {
    NetworkManager manager("test.server.com", 8080);
    
    EXPECT_TRUE(manager.connect());
    cout_buffer_.str(""); // Clear buffer
    
    EXPECT_TRUE(manager.connect());
    EXPECT_TRUE(manager.isConnected());
    // Should not print connection message again
    EXPECT_EQ(cout_buffer_.str(), "");
}

TEST_F(NetworkManagerTest, DisconnectSetsDisconnectedState) {
    NetworkManager manager("test.server.com", 8080);
    
    EXPECT_TRUE(manager.connect());
    EXPECT_TRUE(manager.isConnected());
    
    manager.disconnect();
    EXPECT_FALSE(manager.isConnected());
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("Disconnected from server"));
}

TEST_F(NetworkManagerTest, DisconnectWhenNotConnectedDoesNothing) {
    NetworkManager manager("test.server.com", 8080);
    
    EXPECT_FALSE(manager.isConnected());
    cout_buffer_.str(""); // Clear buffer
    
    manager.disconnect();
    EXPECT_FALSE(manager.isConnected());
    EXPECT_EQ(cout_buffer_.str(), "");
}

TEST_F(NetworkManagerTest, SendPositionUpdateWhenConnected) {
    NetworkManager manager("test.server.com", 8080);
    EXPECT_TRUE(manager.connect());
    
    MockPosition position(37.7749, -122.4194, 10.0, 5.0);
    EXPECT_TRUE(manager.sendPositionUpdate("device123", position));
    
    // Give the worker thread time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("Sending position update to server"));
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("\"id\":\"device123\""));
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("\"latitude\":37.7749"));
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("\"longitude\":-122.4194"));
}

TEST_F(NetworkManagerTest, SendPositionUpdateWhenNotConnected) {
    NetworkManager manager("test.server.com", 8080);
    
    // Override connect to always fail
    NetworkManager managerFail("invalid.server", -1);
    
    MockPosition position(37.7749, -122.4194, 10.0, 5.0);
    EXPECT_TRUE(manager.sendPositionUpdate("device123", position));
    
    // Give the worker thread time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("Connecting to server"));
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("Sending position update to server"));
}

TEST_F(NetworkManagerTest, SyncWithServerWhenConnected) {
    NetworkManager manager("test.server.com", 8080);
    EXPECT_TRUE(manager.connect());
    
    MockPosition position(37.7749, -122.4194, 10.0, 5.0);
    EXPECT_TRUE(manager.sendPositionUpdate("device123", position));
    
    cout_buffer_.str(""); // Clear buffer
    EXPECT_TRUE(manager.syncWithServer());
    
    // Give the worker thread time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Since the queue was already processed by the worker thread,
    // syncWithServer might not show additional output
}

TEST_F(NetworkManagerTest, SyncWithServerWhenNotConnected) {
    NetworkManager manager("test.server.com", 8080);
    
    EXPECT_TRUE(manager.syncWithServer());
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("Connecting to server"));
}

TEST_F(NetworkManagerTest, RegisterCommandHandler) {
    NetworkManager manager("test.server.com", 8080);
    bool handlerCalled = false;
    std::string receivedCommand;
    
    manager.registerCommandHandler([&handlerCalled, &receivedCommand](const std::string& cmd) {
        handlerCalled = true;
        receivedCommand = cmd;
    });
    
    EXPECT_TRUE(manager.connect());
    
    // Wait for a potential command (though it's random)
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // We can't reliably test the random command generation,
    // but we can verify the handler was registered by checking internal state indirectly
    manager.disconnect();
}

TEST_F(NetworkManagerTest, SetServerUrl) {
    NetworkManager manager("test.server.com", 8080);
    EXPECT_TRUE(manager.connect());
    
    manager.setServerUrl("new.server.com");
    EXPECT_FALSE(manager.isConnected()); // Should disconnect
    
    EXPECT_TRUE(manager.connect());
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("new.server.com"));
}

TEST_F(NetworkManagerTest, SetServerPort) {
    NetworkManager manager("test.server.com", 8080);
    EXPECT_TRUE(manager.connect());
    
    manager.setServerPort(9090);
    EXPECT_FALSE(manager.isConnected()); // Should disconnect
    
    EXPECT_TRUE(manager.connect());
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("9090"));
}

TEST_F(NetworkManagerTest, SendRequest) {
    NetworkManager manager("test.server.com", 8080);
    
    // When not connected
    EXPECT_FALSE(manager.sendRequest("/api/endpoint", "{\"data\":\"test\"}"));
    EXPECT_THAT(cerr_buffer_.str(), ::testing::HasSubstr("Not connected to server"));
    
    // When connected
    cerr_buffer_.str(""); // Clear buffer
    cout_buffer_.str(""); // Clear buffer
    EXPECT_TRUE(manager.connect());
    
    EXPECT_TRUE(manager.sendRequest("/api/endpoint", "{\"data\":\"test\"}"));
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("Sending request to test.server.com/api/endpoint"));
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("{\"data\":\"test\"}"));
}

TEST_F(NetworkManagerTest, ReceiveResponse) {
    NetworkManager manager("test.server.com", 8080);
    
    // When not connected
    EXPECT_EQ(manager.receiveResponse(), "");
    EXPECT_THAT(cerr_buffer_.str(), ::testing::HasSubstr("Not connected to server"));
    
    // When connected
    cerr_buffer_.str(""); // Clear buffer
    EXPECT_TRUE(manager.connect());
    
    EXPECT_EQ(manager.receiveResponse(), "{\"status\":\"ok\"}");
}

TEST_F(NetworkManagerTest, DestructorDisconnects) {
    {
        NetworkManager manager("test.server.com", 8080);
        EXPECT_TRUE(manager.connect());
        EXPECT_TRUE(manager.isConnected());
        
        // Destructor will be called when exiting this scope
    }
    
    // Can't directly test the destructor's effects, but we can observe the output
    EXPECT_THAT(cout_buffer_.str(), ::testing::HasSubstr("Disconnected from server"));
}

} // namespace equipment_tracker
// </test_code>