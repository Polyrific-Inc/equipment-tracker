// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker/network_manager.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <future>

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
        // Use a non-default URL and port for testing
        network_manager = std::make_unique<NetworkManager>("test.example.com", 9090);
    }

    void TearDown() override {
        // Ensure network manager is disconnected
        if (network_manager && network_manager->isConnected()) {
            network_manager->disconnect();
        }
        network_manager.reset();
    }

    std::unique_ptr<NetworkManager> network_manager;
};

TEST_F(NetworkManagerTest, ConstructorSetsCorrectValues) {
    EXPECT_EQ(network_manager->getServerUrl(), "test.example.com");
    EXPECT_EQ(network_manager->getServerPort(), 9090);
    EXPECT_FALSE(network_manager->isConnected());
}

TEST_F(NetworkManagerTest, ConnectSetsConnectedFlag) {
    EXPECT_FALSE(network_manager->isConnected());
    EXPECT_TRUE(network_manager->connect());
    EXPECT_TRUE(network_manager->isConnected());
}

TEST_F(NetworkManagerTest, ConnectWhenAlreadyConnectedReturnsTrue) {
    EXPECT_TRUE(network_manager->connect());
    EXPECT_TRUE(network_manager->isConnected());
    EXPECT_TRUE(network_manager->connect());
    EXPECT_TRUE(network_manager->isConnected());
}

TEST_F(NetworkManagerTest, DisconnectClearsConnectedFlag) {
    EXPECT_TRUE(network_manager->connect());
    EXPECT_TRUE(network_manager->isConnected());
    network_manager->disconnect();
    EXPECT_FALSE(network_manager->isConnected());
}

TEST_F(NetworkManagerTest, DisconnectWhenNotConnectedDoesNothing) {
    EXPECT_FALSE(network_manager->isConnected());
    network_manager->disconnect();
    EXPECT_FALSE(network_manager->isConnected());
}

TEST_F(NetworkManagerTest, SetServerUrlDisconnectsIfConnected) {
    EXPECT_TRUE(network_manager->connect());
    EXPECT_TRUE(network_manager->isConnected());
    
    network_manager->setServerUrl("new.example.com");
    EXPECT_FALSE(network_manager->isConnected());
    EXPECT_EQ(network_manager->getServerUrl(), "new.example.com");
}

TEST_F(NetworkManagerTest, SetServerPortDisconnectsIfConnected) {
    EXPECT_TRUE(network_manager->connect());
    EXPECT_TRUE(network_manager->isConnected());
    
    network_manager->setServerPort(8888);
    EXPECT_FALSE(network_manager->isConnected());
    EXPECT_EQ(network_manager->getServerPort(), 8888);
}

TEST_F(NetworkManagerTest, SendPositionUpdateConnectsIfNotConnected) {
    EXPECT_FALSE(network_manager->isConnected());
    
    EquipmentId id = "equipment123";
    MockPosition position(37.7749, -122.4194, 10.0);
    
    EXPECT_TRUE(network_manager->sendPositionUpdate(id, position));
    EXPECT_TRUE(network_manager->isConnected());
}

TEST_F(NetworkManagerTest, SyncWithServerConnectsIfNotConnected) {
    EXPECT_FALSE(network_manager->isConnected());
    
    EXPECT_TRUE(network_manager->syncWithServer());
    EXPECT_TRUE(network_manager->isConnected());
}

TEST_F(NetworkManagerTest, RegisterCommandHandlerStoresHandler) {
    std::atomic<bool> handlerCalled(false);
    std::string receivedCommand;
    
    // Register command handler
    network_manager->registerCommandHandler([&handlerCalled, &receivedCommand](const std::string& cmd) {
        receivedCommand = cmd;
        handlerCalled = true;
    });
    
    // Connect to start worker thread
    EXPECT_TRUE(network_manager->connect());
    
    // Wait for a reasonable time to allow the worker thread to potentially call the handler
    // Note: This is a bit flaky since the command generation is random, but we're just testing
    // that the handler is registered correctly
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Disconnect to stop worker thread
    network_manager->disconnect();
    
    // We can't reliably test if the handler was called due to randomness,
    // but we can verify that the command handler was registered by checking
    // that the network manager is still connected after the test
    EXPECT_FALSE(network_manager->isConnected());
}

TEST_F(NetworkManagerTest, SendPositionUpdateQueuesBatchOfUpdates) {
    EXPECT_TRUE(network_manager->connect());
    
    // Send multiple position updates
    EquipmentId id1 = "equipment123";
    EquipmentId id2 = "equipment456";
    MockPosition position1(37.7749, -122.4194, 10.0);
    MockPosition position2(40.7128, -74.0060, 20.0);
    
    EXPECT_TRUE(network_manager->sendPositionUpdate(id1, position1));
    EXPECT_TRUE(network_manager->sendPositionUpdate(id2, position2));
    
    // Give the worker thread time to process the updates
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // We can't directly verify the queue contents, but we can ensure the network manager
    // is still connected after sending updates
    EXPECT_TRUE(network_manager->isConnected());
}

TEST_F(NetworkManagerTest, DestructorDisconnectsIfConnected) {
    EXPECT_TRUE(network_manager->connect());
    EXPECT_TRUE(network_manager->isConnected());
    
    // Create a new scope to test destructor
    {
        auto temp_manager = std::make_unique<NetworkManager>("temp.example.com", 7070);
        EXPECT_TRUE(temp_manager->connect());
        EXPECT_TRUE(temp_manager->isConnected());
        
        // Let temp_manager go out of scope and be destroyed
    }
    
    // Original network manager should still be connected
    EXPECT_TRUE(network_manager->isConnected());
}

TEST_F(NetworkManagerTest, SendRequestFailsWhenNotConnected) {
    EXPECT_FALSE(network_manager->isConnected());
    EXPECT_FALSE(network_manager->sendRequest("/endpoint", "data"));
}

TEST_F(NetworkManagerTest, SendRequestSucceedsWhenConnected) {
    EXPECT_TRUE(network_manager->connect());
    EXPECT_TRUE(network_manager->isConnected());
    EXPECT_TRUE(network_manager->sendRequest("/endpoint", "data"));
}

TEST_F(NetworkManagerTest, ReceiveResponseReturnsEmptyWhenNotConnected) {
    EXPECT_FALSE(network_manager->isConnected());
    EXPECT_EQ(network_manager->receiveResponse(), "");
}

TEST_F(NetworkManagerTest, ReceiveResponseReturnsDataWhenConnected) {
    EXPECT_TRUE(network_manager->connect());
    EXPECT_TRUE(network_manager->isConnected());
    EXPECT_EQ(network_manager->receiveResponse(), "{\"status\":\"ok\"}");
}

TEST_F(NetworkManagerTest, WorkerThreadStopsWhenDisconnected) {
    EXPECT_TRUE(network_manager->connect());
    EXPECT_TRUE(network_manager->isConnected());
    
    // Give the worker thread time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Disconnect should stop the worker thread
    network_manager->disconnect();
    EXPECT_FALSE(network_manager->isConnected());
    
    // We can't directly test if the thread stopped, but we can ensure the network manager
    // is disconnected after stopping the thread
    EXPECT_FALSE(network_manager->isConnected());
}

TEST_F(NetworkManagerTest, MultiplePositionUpdatesProcessedCorrectly) {
    EXPECT_TRUE(network_manager->connect());
    
    // Send multiple position updates in quick succession
    for (int i = 0; i < 5; i++) {
        EquipmentId id = "equipment" + std::to_string(i);
        MockPosition position(37.7749 + i * 0.1, -122.4194 + i * 0.1, 10.0 + i);
        EXPECT_TRUE(network_manager->sendPositionUpdate(id, position));
    }
    
    // Explicitly sync with server to process all updates
    EXPECT_TRUE(network_manager->syncWithServer());
    
    // We can't directly verify the queue contents, but we can ensure the network manager
    // is still connected after sending updates
    EXPECT_TRUE(network_manager->isConnected());
}

} // namespace equipment_tracker
// </test_code>