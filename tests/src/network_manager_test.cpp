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

// Mock the Position and EquipmentId classes since they're not provided
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

// Include the NetworkManager class definition
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

} // namespace equipment_tracker

// Include the implementation
#include "equipment_tracker/network_manager.h"

// Redirect cout/cerr for testing
class OutputCapture {
public:
    OutputCapture() {
        old_cout_buf_ = std::cout.rdbuf();
        old_cerr_buf_ = std::cerr.rdbuf();
        std::cout.rdbuf(cout_capture_.rdbuf());
        std::cerr.rdbuf(cerr_capture_.rdbuf());
    }

    ~OutputCapture() {
        std::cout.rdbuf(old_cout_buf_);
        std::cerr.rdbuf(old_cerr_buf_);
    }

    std::string getCoutOutput() const { return cout_capture_.str(); }
    std::string getCerrOutput() const { return cerr_capture_.str(); }
    
    void clear() {
        cout_capture_.str("");
        cout_capture_.clear();
        cerr_capture_.str("");
        cerr_capture_.clear();
    }

private:
    std::stringstream cout_capture_;
    std::stringstream cerr_capture_;
    std::streambuf* old_cout_buf_;
    std::streambuf* old_cerr_buf_;
};

class NetworkManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        network_manager = std::make_unique<equipment_tracker::NetworkManager>("test.server.com", 8080);
    }

    void TearDown() override {
        network_manager.reset();
    }

    std::unique_ptr<equipment_tracker::NetworkManager> network_manager;
    OutputCapture output_capture;
};

TEST_F(NetworkManagerTest, ConstructorSetsInitialValues) {
    EXPECT_EQ("test.server.com", network_manager->getServerUrl());
    EXPECT_EQ(8080, network_manager->getServerPort());
    EXPECT_FALSE(network_manager->isConnected());
}

TEST_F(NetworkManagerTest, ConnectSetsConnectedFlag) {
    EXPECT_TRUE(network_manager->connect());
    EXPECT_TRUE(network_manager->isConnected());
    
    std::string output = output_capture.getCoutOutput();
    EXPECT_THAT(output, ::testing::HasSubstr("Connecting to server at test.server.com:8080"));
    EXPECT_THAT(output, ::testing::HasSubstr("Connected to server"));
}

TEST_F(NetworkManagerTest, ConnectWhenAlreadyConnectedReturnsTrue) {
    EXPECT_TRUE(network_manager->connect());
    output_capture.clear();
    
    EXPECT_TRUE(network_manager->connect());
    EXPECT_TRUE(network_manager->isConnected());
    
    // Should not print connection messages again
    std::string output = output_capture.getCoutOutput();
    EXPECT_EQ("", output);
}

TEST_F(NetworkManagerTest, DisconnectClearsConnectedFlag) {
    network_manager->connect();
    output_capture.clear();
    
    network_manager->disconnect();
    EXPECT_FALSE(network_manager->isConnected());
    
    std::string output = output_capture.getCoutOutput();
    EXPECT_THAT(output, ::testing::HasSubstr("Disconnecting from server"));
    EXPECT_THAT(output, ::testing::HasSubstr("Disconnected from server"));
}

TEST_F(NetworkManagerTest, DisconnectWhenNotConnectedDoesNothing) {
    EXPECT_FALSE(network_manager->isConnected());
    output_capture.clear();
    
    network_manager->disconnect();
    EXPECT_FALSE(network_manager->isConnected());
    
    // Should not print disconnection messages
    std::string output = output_capture.getCoutOutput();
    EXPECT_EQ("", output);
}

TEST_F(NetworkManagerTest, SendPositionUpdateConnectsIfNotConnected) {
    EXPECT_FALSE(network_manager->isConnected());
    
    equipment_tracker::Position position(37.7749, -122.4194, 10.0, 5.0);
    EXPECT_TRUE(network_manager->sendPositionUpdate("device1", position));
    
    EXPECT_TRUE(network_manager->isConnected());
    std::string output = output_capture.getCoutOutput();
    EXPECT_THAT(output, ::testing::HasSubstr("Connecting to server"));
}

TEST_F(NetworkManagerTest, SendPositionUpdateQueuesPosition) {
    network_manager->connect();
    output_capture.clear();
    
    equipment_tracker::Position position(37.7749, -122.4194, 10.0, 5.0);
    EXPECT_TRUE(network_manager->sendPositionUpdate("device1", position));
    
    // Wait a bit for the worker thread to process
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    std::string output = output_capture.getCoutOutput();
    EXPECT_THAT(output, ::testing::HasSubstr("Sending position update to server"));
    EXPECT_THAT(output, ::testing::HasSubstr("\"id\":\"device1\""));
    EXPECT_THAT(output, ::testing::HasSubstr("\"latitude\":37.7749"));
    EXPECT_THAT(output, ::testing::HasSubstr("\"longitude\":-122.4194"));
    EXPECT_THAT(output, ::testing::HasSubstr("\"altitude\":10"));
    EXPECT_THAT(output, ::testing::HasSubstr("\"accuracy\":5"));
}

TEST_F(NetworkManagerTest, SyncWithServerConnectsIfNotConnected) {
    EXPECT_FALSE(network_manager->isConnected());
    
    EXPECT_TRUE(network_manager->syncWithServer());
    
    EXPECT_TRUE(network_manager->isConnected());
    std::string output = output_capture.getCoutOutput();
    EXPECT_THAT(output, ::testing::HasSubstr("Connecting to server"));
}

TEST_F(NetworkManagerTest, RegisterCommandHandlerStoresHandler) {
    bool handler_called = false;
    network_manager->registerCommandHandler([&handler_called](const std::string& cmd) {
        handler_called = true;
        EXPECT_EQ("TEST_COMMAND", cmd);
    });
    
    // Manually invoke the handler through a test-only method
    auto& handler = network_manager->command_handler_;
    handler("TEST_COMMAND");
    
    EXPECT_TRUE(handler_called);
}

TEST_F(NetworkManagerTest, SetServerUrlDisconnectsIfConnected) {
    network_manager->connect();
    EXPECT_TRUE(network_manager->isConnected());
    output_capture.clear();
    
    network_manager->setServerUrl("new.server.com");
    
    EXPECT_FALSE(network_manager->isConnected());
    EXPECT_EQ("new.server.com", network_manager->getServerUrl());
    std::string output = output_capture.getCoutOutput();
    EXPECT_THAT(output, ::testing::HasSubstr("Disconnecting from server"));
}

TEST_F(NetworkManagerTest, SetServerPortDisconnectsIfConnected) {
    network_manager->connect();
    EXPECT_TRUE(network_manager->isConnected());
    output_capture.clear();
    
    network_manager->setServerPort(9090);
    
    EXPECT_FALSE(network_manager->isConnected());
    EXPECT_EQ(9090, network_manager->getServerPort());
    std::string output = output_capture.getCoutOutput();
    EXPECT_THAT(output, ::testing::HasSubstr("Disconnecting from server"));
}

TEST_F(NetworkManagerTest, SendRequestFailsWhenNotConnected) {
    EXPECT_FALSE(network_manager->isConnected());
    
    EXPECT_FALSE(network_manager->sendRequest("/api/endpoint", "{\"data\":\"value\"}"));
    
    std::string error = output_capture.getCerrOutput();
    EXPECT_THAT(error, ::testing::HasSubstr("Not connected to server"));
}

TEST_F(NetworkManagerTest, SendRequestSucceedsWhenConnected) {
    network_manager->connect();
    output_capture.clear();
    
    EXPECT_TRUE(network_manager->sendRequest("/api/endpoint", "{\"data\":\"value\"}"));
    
    std::string output = output_capture.getCoutOutput();
    EXPECT_THAT(output, ::testing::HasSubstr("Sending request to test.server.com/api/endpoint"));
    EXPECT_THAT(output, ::testing::HasSubstr("{\"data\":\"value\"}"));
}

TEST_F(NetworkManagerTest, ReceiveResponseFailsWhenNotConnected) {
    EXPECT_FALSE(network_manager->isConnected());
    
    EXPECT_EQ("", network_manager->receiveResponse());
    
    std::string error = output_capture.getCerrOutput();
    EXPECT_THAT(error, ::testing::HasSubstr("Not connected to server"));
}

TEST_F(NetworkManagerTest, ReceiveResponseReturnsDataWhenConnected) {
    network_manager->connect();
    output_capture.clear();
    
    std::string response = network_manager->receiveResponse();
    
    EXPECT_EQ("{\"status\":\"ok\"}", response);
}

// This test verifies that the destructor properly disconnects
TEST_F(NetworkManagerTest, DestructorDisconnects) {
    network_manager->connect();
    EXPECT_TRUE(network_manager->isConnected());
    output_capture.clear();
    
    network_manager.reset();
    
    std::string output = output_capture.getCoutOutput();
    EXPECT_THAT(output, ::testing::HasSubstr("Disconnecting from server"));
}
// </test_code>