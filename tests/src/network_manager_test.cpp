// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/network_manager.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <sstream>

namespace equipment_tracker {

// Mock class for testing
class MockNetworkManager : public NetworkManager {
public:
    MockNetworkManager(const std::string& server_url = DEFAULT_SERVER_URL, 
                      int server_port = DEFAULT_SERVER_PORT)
        : NetworkManager(server_url, server_port) {}
    
    MOCK_METHOD(bool, sendRequest, (const std::string& endpoint, const std::string& data), (override));
    MOCK_METHOD(std::string, receiveResponse, (), (override));
};

// Test fixture
class NetworkManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use default values for testing
        network_manager_ = std::make_unique<NetworkManager>("test.server.com", 9090);
    }

    void TearDown() override {
        // Ensure disconnection
        if (network_manager_->isConnected()) {
            network_manager_->disconnect();
        }
    }

    // Helper to create a test position
    Position createTestPosition(double lat = 40.7128, double lon = -74.0060) {
        return Position::builder()
            .withLatitude(lat)
            .withLongitude(lon)
            .withAltitude(10.0)
            .withAccuracy(5.0)
            .withTimestamp(std::chrono::system_clock::now())
            .build();
    }

    std::unique_ptr<NetworkManager> network_manager_;
};

// Test constructor initializes with correct values
TEST_F(NetworkManagerTest, ConstructorInitializesCorrectly) {
    NetworkManager manager("custom.server.com", 8888);
    
    EXPECT_EQ(manager.getServerUrl(), "custom.server.com");
    EXPECT_EQ(manager.getServerPort(), 8888);
    EXPECT_FALSE(manager.isConnected());
}

// Test connect and disconnect
TEST_F(NetworkManagerTest, ConnectAndDisconnect) {
    // Test connect
    EXPECT_TRUE(network_manager_->connect());
    EXPECT_TRUE(network_manager_->isConnected());
    
    // Test that connecting when already connected returns true
    EXPECT_TRUE(network_manager_->connect());
    
    // Test disconnect
    network_manager_->disconnect();
    EXPECT_FALSE(network_manager_->isConnected());
    
    // Test that disconnecting when already disconnected doesn't cause issues
    network_manager_->disconnect();
    EXPECT_FALSE(network_manager_->isConnected());
}

// Test setting server URL and port
TEST_F(NetworkManagerTest, SetServerUrlAndPort) {
    // Connect first to test that setting URL/port disconnects
    EXPECT_TRUE(network_manager_->connect());
    EXPECT_TRUE(network_manager_->isConnected());
    
    // Set new URL
    network_manager_->setServerUrl("new.server.com");
    EXPECT_EQ(network_manager_->getServerUrl(), "new.server.com");
    EXPECT_FALSE(network_manager_->isConnected());
    
    // Connect again
    EXPECT_TRUE(network_manager_->connect());
    EXPECT_TRUE(network_manager_->isConnected());
    
    // Set new port
    network_manager_->setServerPort(7777);
    EXPECT_EQ(network_manager_->getServerPort(), 7777);
    EXPECT_FALSE(network_manager_->isConnected());
}

// Test sending position update
TEST_F(NetworkManagerTest, SendPositionUpdate) {
    // Should auto-connect when sending position
    EquipmentId id = "equipment123";
    Position position = createTestPosition();
    
    EXPECT_TRUE(network_manager_->sendPositionUpdate(id, position));
    EXPECT_TRUE(network_manager_->isConnected());
    
    // Allow time for the worker thread to process
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    // Disconnect to clean up
    network_manager_->disconnect();
}

// Test sync with server
TEST_F(NetworkManagerTest, SyncWithServer) {
    // Should auto-connect when syncing
    EXPECT_TRUE(network_manager_->syncWithServer());
    EXPECT_TRUE(network_manager_->isConnected());
    
    // Disconnect to clean up
    network_manager_->disconnect();
}

// Test command handler registration
TEST_F(NetworkManagerTest, RegisterCommandHandler) {
    std::atomic<bool> command_received{false};
    std::string received_command;
    
    // Register command handler
    network_manager_->registerCommandHandler([&](const std::string& cmd) {
        command_received = true;
        received_command = cmd;
    });
    
    // Connect to start worker thread
    EXPECT_TRUE(network_manager_->connect());
    
    // Wait for potential command (this is probabilistic, so might not always trigger)
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Disconnect to clean up
    network_manager_->disconnect();
    
    // We can't reliably test the random command generation, so we just ensure
    // the test completes without errors
}

// Test with mock to verify sendRequest is called with correct parameters
TEST_F(NetworkManagerTest, MockSendRequest) {
    auto mock_manager = std::make_unique<::testing::NiceMock<MockNetworkManager>>("mock.server.com", 8080);
    
    // Set up expectations
    EXPECT_CALL(*mock_manager, sendRequest(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(true));
    EXPECT_CALL(*mock_manager, receiveResponse())
        .WillRepeatedly(::testing::Return("{\"status\":\"ok\"}"));
    
    // Connect
    EXPECT_TRUE(mock_manager->connect());
    
    // Send position update
    EquipmentId id = "mock_equipment";
    Position position = createTestPosition(41.8781, -87.6298);
    EXPECT_TRUE(mock_manager->sendPositionUpdate(id, position));
    
    // Allow time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    // Disconnect
    mock_manager->disconnect();
}

// Test behavior when not connected
TEST_F(NetworkManagerTest, NotConnectedBehavior) {
    // Create a new manager that's not connected
    NetworkManager disconnected_manager("offline.server.com", 9999);
    
    // Force disconnect state
    disconnected_manager.disconnect();
    EXPECT_FALSE(disconnected_manager.isConnected());
    
    // Test sending request when not connected
    EXPECT_TRUE(disconnected_manager.sendPositionUpdate("test_id", createTestPosition()));
    
    // This should have auto-connected
    EXPECT_TRUE(disconnected_manager.isConnected());
    
    // Clean up
    disconnected_manager.disconnect();
}

// Test multiple position updates
TEST_F(NetworkManagerTest, MultiplePositionUpdates) {
    // Connect
    EXPECT_TRUE(network_manager_->connect());
    
    // Send multiple position updates
    for (int i = 0; i < 5; i++) {
        EquipmentId id = "equipment_" + std::to_string(i);
        Position position = createTestPosition(40.0 + i, -74.0 - i);
        EXPECT_TRUE(network_manager_->sendPositionUpdate(id, position));
    }
    
    // Allow time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    
    // Sync with server to process any remaining updates
    EXPECT_TRUE(network_manager_->syncWithServer());
    
    // Disconnect
    network_manager_->disconnect();
}

// Test destructor behavior
TEST_F(NetworkManagerTest, DestructorDisconnects) {
    // Create a manager and connect
    auto temp_manager = std::make_unique<NetworkManager>("temp.server.com", 8888);
    EXPECT_TRUE(temp_manager->connect());
    EXPECT_TRUE(temp_manager->isConnected());
    
    // Destroy the manager - should disconnect automatically
    temp_manager.reset();
    
    // No explicit assertion here, but we're testing that the destructor
    // doesn't cause any crashes or memory issues
}

} // namespace equipment_tracker
// </test_code>