// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/network_manager.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <string>
#include <functional>

namespace equipment_tracker {

// Mock Position class for testing
class MockPosition : public Position {
public:
    MockPosition(double lat = 0.0, double lon = 0.0, double alt = 0.0, 
                 double acc = DEFAULT_POSITION_ACCURACY,
                 Timestamp ts = getCurrentTimestamp())
        : Position(lat, lon, alt, acc, ts) {}
};

class NetworkManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Redirect cout to capture output
        original_cout_buf = std::cout.rdbuf();
        std::cout.rdbuf(output.rdbuf());
    }

    void TearDown() override {
        // Restore cout
        std::cout.rdbuf(original_cout_buf);
    }

    std::stringstream output;
    std::streambuf* original_cout_buf;
    
    // Helper to check if a string contains a substring
    bool outputContains(const std::string& substr) {
        return output.str().find(substr) != std::string::npos;
    }
};

TEST_F(NetworkManagerTest, ConstructorSetsInitialValues) {
    NetworkManager manager("test.server.com", 9090);
    
    EXPECT_EQ(manager.getServerUrl(), "test.server.com");
    EXPECT_EQ(manager.getServerPort(), 9090);
    EXPECT_FALSE(manager.isConnected());
}

TEST_F(NetworkManagerTest, DefaultConstructorUsesDefaultValues) {
    NetworkManager manager;
    
    EXPECT_EQ(manager.getServerUrl(), DEFAULT_SERVER_URL);
    EXPECT_EQ(manager.getServerPort(), DEFAULT_SERVER_PORT);
    EXPECT_FALSE(manager.isConnected());
}

TEST_F(NetworkManagerTest, ConnectSetsConnectedFlag) {
    NetworkManager manager("test.server.com", 9090);
    
    EXPECT_FALSE(manager.isConnected());
    EXPECT_TRUE(manager.connect());
    EXPECT_TRUE(manager.isConnected());
    
    // Verify output
    EXPECT_THAT(output.str(), ::testing::HasSubstr("Connecting to server at test.server.com:9090"));
    EXPECT_THAT(output.str(), ::testing::HasSubstr("Connected to server"));
}

TEST_F(NetworkManagerTest, ConnectWhenAlreadyConnectedReturnsTrue) {
    NetworkManager manager;
    
    EXPECT_TRUE(manager.connect());
    output.str(""); // Clear output
    
    EXPECT_TRUE(manager.connect());
    
    // Should not see connection message again
    EXPECT_EQ(output.str(), "");
}

TEST_F(NetworkManagerTest, DisconnectClearsConnectedFlag) {
    NetworkManager manager;
    
    manager.connect();
    EXPECT_TRUE(manager.isConnected());
    
    output.str(""); // Clear output
    manager.disconnect();
    EXPECT_FALSE(manager.isConnected());
    
    // Verify output
    EXPECT_THAT(output.str(), ::testing::HasSubstr("Disconnecting from server"));
    EXPECT_THAT(output.str(), ::testing::HasSubstr("Disconnected from server"));
}

TEST_F(NetworkManagerTest, DisconnectWhenNotConnectedDoesNothing) {
    NetworkManager manager;
    
    EXPECT_FALSE(manager.isConnected());
    manager.disconnect();
    EXPECT_FALSE(manager.isConnected());
    
    // Should not see disconnection message
    EXPECT_EQ(output.str(), "");
}

TEST_F(NetworkManagerTest, SendPositionUpdateConnectsIfNotConnected) {
    NetworkManager manager;
    MockPosition position(37.7749, -122.4194);
    
    EXPECT_FALSE(manager.isConnected());
    EXPECT_TRUE(manager.sendPositionUpdate("equipment1", position));
    EXPECT_TRUE(manager.isConnected());
    
    // Verify connection output
    EXPECT_THAT(output.str(), ::testing::HasSubstr("Connecting to server"));
}

TEST_F(NetworkManagerTest, SendPositionUpdateQueuesPosition) {
    NetworkManager manager;
    MockPosition position(37.7749, -122.4194, 10.0, 5.0);
    
    manager.connect();
    EXPECT_TRUE(manager.sendPositionUpdate("equipment1", position));
    
    // Wait for background processing
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    // Verify position was sent
    EXPECT_THAT(output.str(), ::testing::HasSubstr("Sending position update to server"));
    EXPECT_THAT(output.str(), ::testing::HasSubstr("\"id\":\"equipment1\""));
    EXPECT_THAT(output.str(), ::testing::HasSubstr("\"latitude\":37.7749"));
    EXPECT_THAT(output.str(), ::testing::HasSubstr("\"longitude\":-122.4194"));
    EXPECT_THAT(output.str(), ::testing::HasSubstr("\"altitude\":10"));
    EXPECT_THAT(output.str(), ::testing::HasSubstr("\"accuracy\":5"));
}

TEST_F(NetworkManagerTest, SyncWithServerProcessesQueue) {
    NetworkManager manager;
    MockPosition position(37.7749, -122.4194);
    
    manager.connect();
    EXPECT_TRUE(manager.sendPositionUpdate("equipment1", position));
    
    output.str(""); // Clear output
    EXPECT_TRUE(manager.syncWithServer());
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Verify sync processed the queue
    EXPECT_THAT(output.str(), ::testing::HasSubstr("Sending position update to server"));
}

TEST_F(NetworkManagerTest, SyncWithServerConnectsIfNotConnected) {
    NetworkManager manager;
    
    EXPECT_FALSE(manager.isConnected());
    EXPECT_TRUE(manager.syncWithServer());
    EXPECT_TRUE(manager.isConnected());
    
    // Verify connection output
    EXPECT_THAT(output.str(), ::testing::HasSubstr("Connecting to server"));
}

TEST_F(NetworkManagerTest, RegisterCommandHandlerStoresHandler) {
    NetworkManager manager;
    std::atomic<bool> handlerCalled(false);
    std::string receivedCommand;
    
    manager.registerCommandHandler([&handlerCalled, &receivedCommand](const std::string& cmd) {
        handlerCalled = true;
        receivedCommand = cmd;
    });
    
    manager.connect();
    
    // Wait for potential command (may not happen due to randomness)
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // If handler was called, verify it received the expected command
    if (handlerCalled) {
        EXPECT_EQ(receivedCommand, "STATUS_REQUEST");
    }
    
    // Disconnect to stop the worker thread
    manager.disconnect();
}

TEST_F(NetworkManagerTest, SetServerUrlDisconnectsIfConnected) {
    NetworkManager manager;
    
    manager.connect();
    EXPECT_TRUE(manager.isConnected());
    
    output.str(""); // Clear output
    manager.setServerUrl("new.server.com");
    
    EXPECT_FALSE(manager.isConnected());
    EXPECT_EQ(manager.getServerUrl(), "new.server.com");
    
    // Verify disconnection output
    EXPECT_THAT(output.str(), ::testing::HasSubstr("Disconnecting from server"));
}

TEST_F(NetworkManagerTest, SetServerPortDisconnectsIfConnected) {
    NetworkManager manager;
    
    manager.connect();
    EXPECT_TRUE(manager.isConnected());
    
    output.str(""); // Clear output
    manager.setServerPort(9999);
    
    EXPECT_FALSE(manager.isConnected());
    EXPECT_EQ(manager.getServerPort(), 9999);
    
    // Verify disconnection output
    EXPECT_THAT(output.str(), ::testing::HasSubstr("Disconnecting from server"));
}

TEST_F(NetworkManagerTest, SendRequestFailsWhenNotConnected) {
    NetworkManager manager;
    
    EXPECT_FALSE(manager.isConnected());
    
    // Use the private method through a friend test or test the behavior indirectly
    // Here we test indirectly by checking that syncWithServer connects first
    EXPECT_TRUE(manager.syncWithServer());
    EXPECT_TRUE(manager.isConnected());
}

TEST_F(NetworkManagerTest, DestructorDisconnectsIfConnected) {
    {
        NetworkManager manager;
        manager.connect();
        EXPECT_TRUE(manager.isConnected());
        
        output.str(""); // Clear output
    } // Destructor called here
    
    // Verify disconnection output
    EXPECT_THAT(output.str(), ::testing::HasSubstr("Disconnecting from server"));
}

TEST_F(NetworkManagerTest, MultiplePositionUpdatesAreProcessed) {
    NetworkManager manager;
    MockPosition position1(37.7749, -122.4194);
    MockPosition position2(40.7128, -74.0060);
    
    manager.connect();
    EXPECT_TRUE(manager.sendPositionUpdate("equipment1", position1));
    EXPECT_TRUE(manager.sendPositionUpdate("equipment2", position2));
    
    // Wait for background processing
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    // Verify both positions were sent
    EXPECT_THAT(output.str(), ::testing::HasSubstr("\"id\":\"equipment1\""));
    EXPECT_THAT(output.str(), ::testing::HasSubstr("\"latitude\":37.7749"));
    EXPECT_THAT(output.str(), ::testing::HasSubstr("\"id\":\"equipment2\""));
    EXPECT_THAT(output.str(), ::testing::HasSubstr("\"latitude\":40.7128"));
}

} // namespace equipment_tracker

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>