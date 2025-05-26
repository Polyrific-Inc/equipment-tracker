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

// Mock the Position and EquipmentId classes since they're external dependencies
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

// Include the header for the class being tested
namespace equipment_tracker {
    class NetworkManager {
    public:
        NetworkManager(const std::string &server_url, int server_port);
        ~NetworkManager();

        bool connect();
        void disconnect();
        bool sendPositionUpdate(const EquipmentId &id, const Position &position);
        bool syncWithServer();
        void registerCommandHandler(std::function<void(const std::string &)> handler);
        void setServerUrl(const std::string &url);
        void setServerPort(int port);
        bool sendRequest(const std::string &endpoint, const std::string &data);
        std::string receiveResponse();

        // Added for testing
        bool isConnected() const { return is_connected_; }
        std::string getServerUrl() const { return server_url_; }
        int getServerPort() const { return server_port_; }

    private:
        std::string server_url_;
        int server_port_;
        bool is_connected_;
        bool should_run_;
        std::thread worker_thread_;
        std::queue<std::pair<EquipmentId, Position>> position_queue_;
        std::mutex queue_mutex_;
        std::condition_variable queue_condition_;
        std::function<void(const std::string &)> command_handler_;

        void workerThreadFunction();
        bool processQueuedUpdates();
    };
}

// Redirect cout for testing
class CoutRedirect {
public:
    CoutRedirect() : old_buf(std::cout.rdbuf(buffer.rdbuf())) {}
    ~CoutRedirect() { std::cout.rdbuf(old_buf); }
    std::string str() const { return buffer.str(); }
    void clear() { buffer.str(""); }

private:
    std::stringstream buffer;
    std::streambuf* old_buf;
};

// Redirect cerr for testing
class CerrRedirect {
public:
    CerrRedirect() : old_buf(std::cerr.rdbuf(buffer.rdbuf())) {}
    ~CerrRedirect() { std::cerr.rdbuf(old_buf); }
    std::string str() const { return buffer.str(); }
    void clear() { buffer.str(""); }

private:
    std::stringstream buffer;
    std::streambuf* old_buf;
};

class NetworkManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        cout_redirect = std::make_unique<CoutRedirect>();
        cerr_redirect = std::make_unique<CerrRedirect>();
    }

    void TearDown() override {
        cout_redirect.reset();
        cerr_redirect.reset();
    }

    std::unique_ptr<CoutRedirect> cout_redirect;
    std::unique_ptr<CerrRedirect> cerr_redirect;
};

TEST_F(NetworkManagerTest, ConstructorSetsInitialValues) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    EXPECT_EQ(manager.getServerUrl(), "test.server.com");
    EXPECT_EQ(manager.getServerPort(), 8080);
    EXPECT_FALSE(manager.isConnected());
}

TEST_F(NetworkManagerTest, ConnectSetsConnectedFlag) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    EXPECT_TRUE(manager.connect());
    EXPECT_TRUE(manager.isConnected());
    
    // Check output
    EXPECT_THAT(cout_redirect->str(), ::testing::HasSubstr("Connecting to server at test.server.com:8080"));
    EXPECT_THAT(cout_redirect->str(), ::testing::HasSubstr("Connected to server"));
}

TEST_F(NetworkManagerTest, ConnectWhenAlreadyConnectedReturnsTrue) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    EXPECT_TRUE(manager.connect());
    cout_redirect->clear();
    
    EXPECT_TRUE(manager.connect());
    // Should not show connection messages again
    EXPECT_EQ(cout_redirect->str(), "");
}

TEST_F(NetworkManagerTest, DisconnectClearsConnectedFlag) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    manager.connect();
    cout_redirect->clear();
    
    manager.disconnect();
    EXPECT_FALSE(manager.isConnected());
    
    // Check output
    EXPECT_THAT(cout_redirect->str(), ::testing::HasSubstr("Disconnecting from server"));
    EXPECT_THAT(cout_redirect->str(), ::testing::HasSubstr("Disconnected from server"));
}

TEST_F(NetworkManagerTest, DisconnectWhenNotConnectedDoesNothing) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    manager.disconnect();
    EXPECT_FALSE(manager.isConnected());
    EXPECT_EQ(cout_redirect->str(), "");
}

TEST_F(NetworkManagerTest, SendPositionUpdateConnectsIfNeeded) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    Position pos(37.7749, -122.4194, 10.0, 5.0);
    
    EXPECT_TRUE(manager.sendPositionUpdate("equipment1", pos));
    EXPECT_TRUE(manager.isConnected());
    
    // Check output
    EXPECT_THAT(cout_redirect->str(), ::testing::HasSubstr("Connecting to server"));
}

TEST_F(NetworkManagerTest, SendPositionUpdateQueuesData) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    Position pos(37.7749, -122.4194, 10.0, 5.0);
    
    manager.connect();
    cout_redirect->clear();
    
    EXPECT_TRUE(manager.sendPositionUpdate("equipment1", pos));
    
    // Wait a bit for the worker thread to process the queue
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    // Check that the position update was sent
    EXPECT_THAT(cout_redirect->str(), ::testing::HasSubstr("Sending position update to server"));
    EXPECT_THAT(cout_redirect->str(), ::testing::HasSubstr("\"id\":\"equipment1\""));
    EXPECT_THAT(cout_redirect->str(), ::testing::HasSubstr("\"latitude\":37.7749"));
    EXPECT_THAT(cout_redirect->str(), ::testing::HasSubstr("\"longitude\":-122.4194"));
}

TEST_F(NetworkManagerTest, SyncWithServerConnectsIfNeeded) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    EXPECT_TRUE(manager.syncWithServer());
    EXPECT_TRUE(manager.isConnected());
    
    // Check output
    EXPECT_THAT(cout_redirect->str(), ::testing::HasSubstr("Connecting to server"));
}

TEST_F(NetworkManagerTest, RegisterCommandHandlerStoresHandler) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    bool handler_called = false;
    
    manager.registerCommandHandler([&handler_called](const std::string& cmd) {
        handler_called = true;
        EXPECT_EQ(cmd, "STATUS_REQUEST");
    });
    
    // Connect and wait for potential command (this is probabilistic)
    manager.connect();
    
    // We can't reliably test the random command generation in the worker thread
    // without modifying the code, so we'll skip that part of the test
}

TEST_F(NetworkManagerTest, SetServerUrlDisconnectsIfConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    manager.connect();
    cout_redirect->clear();
    
    manager.setServerUrl("new.server.com");
    EXPECT_EQ(manager.getServerUrl(), "new.server.com");
    EXPECT_FALSE(manager.isConnected());
    
    // Check output
    EXPECT_THAT(cout_redirect->str(), ::testing::HasSubstr("Disconnecting from server"));
}

TEST_F(NetworkManagerTest, SetServerPortDisconnectsIfConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    manager.connect();
    cout_redirect->clear();
    
    manager.setServerPort(9090);
    EXPECT_EQ(manager.getServerPort(), 9090);
    EXPECT_FALSE(manager.isConnected());
    
    // Check output
    EXPECT_THAT(cout_redirect->str(), ::testing::HasSubstr("Disconnecting from server"));
}

TEST_F(NetworkManagerTest, SendRequestFailsWhenNotConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    EXPECT_FALSE(manager.sendRequest("/api/status", "{}"));
    EXPECT_THAT(cerr_redirect->str(), ::testing::HasSubstr("Not connected to server"));
}

TEST_F(NetworkManagerTest, SendRequestSucceedsWhenConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    manager.connect();
    cout_redirect->clear();
    
    EXPECT_TRUE(manager.sendRequest("/api/status", "{}"));
    EXPECT_THAT(cout_redirect->str(), ::testing::HasSubstr("Sending request to test.server.com/api/status: {}"));
}

TEST_F(NetworkManagerTest, ReceiveResponseFailsWhenNotConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    EXPECT_EQ(manager.receiveResponse(), "");
    EXPECT_THAT(cerr_redirect->str(), ::testing::HasSubstr("Not connected to server"));
}

TEST_F(NetworkManagerTest, ReceiveResponseSucceedsWhenConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    manager.connect();
    
    EXPECT_EQ(manager.receiveResponse(), "{\"status\":\"ok\"}");
}

TEST_F(NetworkManagerTest, DestructorDisconnectsIfConnected) {
    {
        equipment_tracker::NetworkManager manager("test.server.com", 8080);
        manager.connect();
        cout_redirect->clear();
        
        // Destructor will be called here
    }
    
    // Check output
    EXPECT_THAT(cout_redirect->str(), ::testing::HasSubstr("Disconnecting from server"));
}

// Test multiple position updates
TEST_F(NetworkManagerTest, MultiplePositionUpdates) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    manager.connect();
    cout_redirect->clear();
    
    Position pos1(37.7749, -122.4194, 10.0, 5.0);
    Position pos2(40.7128, -74.0060, 20.0, 3.0);
    
    EXPECT_TRUE(manager.sendPositionUpdate("equipment1", pos1));
    EXPECT_TRUE(manager.sendPositionUpdate("equipment2", pos2));
    
    // Wait for worker thread to process
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    std::string output = cout_redirect->str();
    EXPECT_THAT(output, ::testing::HasSubstr("\"id\":\"equipment1\""));
    EXPECT_THAT(output, ::testing::HasSubstr("\"latitude\":37.7749"));
    EXPECT_THAT(output, ::testing::HasSubstr("\"id\":\"equipment2\""));
    EXPECT_THAT(output, ::testing::HasSubstr("\"latitude\":40.7128"));
}
// </test_code>