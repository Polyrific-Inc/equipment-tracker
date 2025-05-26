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
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

// Mock the necessary headers
#include "equipment_tracker/network_manager.h"

// Mock Position class
namespace equipment_tracker {
    class Position {
    public:
        Position(double lat = 0.0, double lon = 0.0, double alt = 0.0, double acc = 0.0)
            : latitude_(lat), longitude_(lon), altitude_(alt), accuracy_(acc),
              timestamp_(std::chrono::system_clock::now()) {}
        
        double getLatitude() const { return latitude_; }
        double getLongitude() const { return longitude_; }
        double getAltitude() const { return altitude_; }
        double getAccuracy() const { return accuracy_; }
        std::chrono::system_clock::time_point getTimestamp() const { return timestamp_; }
        
    private:
        double latitude_;
        double longitude_;
        double altitude_;
        double accuracy_;
        std::chrono::system_clock::time_point timestamp_;
    };

    using EquipmentId = std::string;
}

// Redirect cout for testing
class NetworkManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Redirect cout to our stringstream
        old_cout_buf = std::cout.rdbuf();
        std::cout.rdbuf(buffer.rdbuf());
        
        // Redirect cerr to our stringstream
        old_cerr_buf = std::cerr.rdbuf();
        std::cerr.rdbuf(error_buffer.rdbuf());
    }

    void TearDown() override {
        // Restore cout
        std::cout.rdbuf(old_cout_buf);
        std::cerr.rdbuf(old_cerr_buf);
    }

    std::stringstream buffer;
    std::stringstream error_buffer;
    std::streambuf* old_cout_buf;
    std::streambuf* old_cerr_buf;
};

TEST_F(NetworkManagerTest, ConstructorSetsInitialValues) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    // Test server URL and port are set correctly
    EXPECT_EQ("test.server.com", manager.getServerUrl());
    EXPECT_EQ(8080, manager.getServerPort());
    
    // Test initial connection state
    EXPECT_FALSE(manager.isConnected());
}

TEST_F(NetworkManagerTest, ConnectEstablishesConnection) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    EXPECT_TRUE(manager.connect());
    EXPECT_TRUE(manager.isConnected());
    
    // Check output messages
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("Connecting to server at test.server.com:8080"));
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("Connected to server"));
}

TEST_F(NetworkManagerTest, ConnectWhenAlreadyConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    // Connect first time
    EXPECT_TRUE(manager.connect());
    buffer.str(""); // Clear buffer
    
    // Connect second time
    EXPECT_TRUE(manager.connect());
    
    // Should not show connection messages again
    EXPECT_EQ("", buffer.str());
}

TEST_F(NetworkManagerTest, DisconnectClosesConnection) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    // Connect first
    manager.connect();
    buffer.str(""); // Clear buffer
    
    // Disconnect
    manager.disconnect();
    EXPECT_FALSE(manager.isConnected());
    
    // Check output messages
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("Disconnecting from server"));
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("Disconnected from server"));
}

TEST_F(NetworkManagerTest, DisconnectWhenNotConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    // Disconnect without connecting first
    manager.disconnect();
    
    // Should not show disconnection messages
    EXPECT_EQ("", buffer.str());
}

TEST_F(NetworkManagerTest, SendPositionUpdateWhenConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    manager.connect();
    buffer.str(""); // Clear buffer
    
    equipment_tracker::Position pos(37.7749, -122.4194, 10.0, 5.0);
    EXPECT_TRUE(manager.sendPositionUpdate("equipment123", pos));
    
    // Wait for background processing
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    // Check that position update was sent
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("Sending position update to server"));
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("\"id\":\"equipment123\""));
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("\"latitude\":37.7749"));
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("\"longitude\":-122.4194"));
}

TEST_F(NetworkManagerTest, SendPositionUpdateWhenNotConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    equipment_tracker::Position pos(37.7749, -122.4194, 10.0, 5.0);
    EXPECT_TRUE(manager.sendPositionUpdate("equipment123", pos));
    
    // Should auto-connect
    EXPECT_TRUE(manager.isConnected());
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("Connecting to server"));
}

TEST_F(NetworkManagerTest, SyncWithServerWhenConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    manager.connect();
    buffer.str(""); // Clear buffer
    
    EXPECT_TRUE(manager.syncWithServer());
}

TEST_F(NetworkManagerTest, SyncWithServerWhenNotConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    EXPECT_TRUE(manager.syncWithServer());
    
    // Should auto-connect
    EXPECT_TRUE(manager.isConnected());
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("Connecting to server"));
}

TEST_F(NetworkManagerTest, RegisterCommandHandler) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    bool handlerCalled = false;
    std::string receivedCommand;
    
    manager.registerCommandHandler([&handlerCalled, &receivedCommand](const std::string& cmd) {
        handlerCalled = true;
        receivedCommand = cmd;
    });
    
    // Connect and wait for potential command (though it's random)
    manager.connect();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // We can't reliably test the random command generation, but we can verify
    // the handler was registered by testing the sendRequest method
    manager.disconnect();
}

TEST_F(NetworkManagerTest, SetServerUrl) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    manager.connect();
    buffer.str(""); // Clear buffer
    
    manager.setServerUrl("new.server.com");
    
    // Should disconnect first
    EXPECT_FALSE(manager.isConnected());
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("Disconnecting from server"));
    
    // URL should be updated
    EXPECT_EQ("new.server.com", manager.getServerUrl());
}

TEST_F(NetworkManagerTest, SetServerPort) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    manager.connect();
    buffer.str(""); // Clear buffer
    
    manager.setServerPort(9090);
    
    // Should disconnect first
    EXPECT_FALSE(manager.isConnected());
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("Disconnecting from server"));
    
    // Port should be updated
    EXPECT_EQ(9090, manager.getServerPort());
}

TEST_F(NetworkManagerTest, SendRequestWhenConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    manager.connect();
    buffer.str(""); // Clear buffer
    
    EXPECT_TRUE(manager.sendRequest("/api/status", "{\"query\":\"status\"}"));
    
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("Sending request to test.server.com/api/status"));
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("{\"query\":\"status\"}"));
}

TEST_F(NetworkManagerTest, SendRequestWhenNotConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    EXPECT_FALSE(manager.sendRequest("/api/status", "{\"query\":\"status\"}"));
    
    EXPECT_THAT(error_buffer.str(), ::testing::HasSubstr("Not connected to server"));
}

TEST_F(NetworkManagerTest, ReceiveResponseWhenConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    manager.connect();
    
    std::string response = manager.receiveResponse();
    
    EXPECT_EQ("{\"status\":\"ok\"}", response);
}

TEST_F(NetworkManagerTest, ReceiveResponseWhenNotConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    std::string response = manager.receiveResponse();
    
    EXPECT_EQ("", response);
    EXPECT_THAT(error_buffer.str(), ::testing::HasSubstr("Not connected to server"));
}

TEST_F(NetworkManagerTest, DestructorDisconnects) {
    {
        equipment_tracker::NetworkManager manager("test.server.com", 8080);
        manager.connect();
        EXPECT_TRUE(manager.isConnected());
        buffer.str(""); // Clear buffer
        
        // Destructor will be called when exiting this scope
    }
    
    // Check that disconnect messages were printed
    EXPECT_THAT(buffer.str(), ::testing::HasSubstr("Disconnecting from server"));
}

TEST_F(NetworkManagerTest, ProcessQueuedUpdatesWithMultiplePositions) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    manager.connect();
    buffer.str(""); // Clear buffer
    
    // Send multiple position updates
    equipment_tracker::Position pos1(37.7749, -122.4194, 10.0, 5.0);
    equipment_tracker::Position pos2(40.7128, -74.0060, 15.0, 3.0);
    
    manager.sendPositionUpdate("equipment123", pos1);
    manager.sendPositionUpdate("equipment456", pos2);
    
    // Wait for background processing
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    // Check that both position updates were sent
    std::string output = buffer.str();
    EXPECT_THAT(output, ::testing::HasSubstr("\"id\":\"equipment123\""));
    EXPECT_THAT(output, ::testing::HasSubstr("\"latitude\":37.7749"));
    EXPECT_THAT(output, ::testing::HasSubstr("\"id\":\"equipment456\""));
    EXPECT_THAT(output, ::testing::HasSubstr("\"latitude\":40.7128"));
}
// </test_code>