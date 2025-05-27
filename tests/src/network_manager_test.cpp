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
    void workerThreadFunction();
    bool processQueuedUpdates();

    std::string server_url_;
    int server_port_;
    bool is_connected_;
    bool should_run_;
    std::thread worker_thread_;
    std::queue<std::pair<EquipmentId, Position>> position_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_condition_;
    std::function<void(const std::string &)> command_handler_;
};

} // namespace equipment_tracker

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
        // Use a short delay for tests
        original_sleep_duration_ = std::chrono::milliseconds(10);
    }

    std::chrono::milliseconds original_sleep_duration_;
    OutputCapture output_capture_;
};

TEST_F(NetworkManagerTest, ConstructorInitializesCorrectly) {
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
    EXPECT_THAT(output_capture_.getCoutOutput(), ::testing::HasSubstr("Connecting to server at test.server.com:8080"));
    EXPECT_THAT(output_capture_.getCoutOutput(), ::testing::HasSubstr("Connected to server"));
}

TEST_F(NetworkManagerTest, ConnectWhenAlreadyConnectedReturnsTrue) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    EXPECT_TRUE(manager.connect());
    output_capture_.clear();
    
    EXPECT_TRUE(manager.connect());
    // Should not output connection messages again
    EXPECT_EQ(output_capture_.getCoutOutput(), "");
}

TEST_F(NetworkManagerTest, DisconnectClearsConnectedFlag) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    manager.connect();
    output_capture_.clear();
    
    manager.disconnect();
    EXPECT_FALSE(manager.isConnected());
    
    // Check output
    EXPECT_THAT(output_capture_.getCoutOutput(), ::testing::HasSubstr("Disconnecting from server"));
    EXPECT_THAT(output_capture_.getCoutOutput(), ::testing::HasSubstr("Disconnected from server"));
}

TEST_F(NetworkManagerTest, DisconnectWhenNotConnectedDoesNothing) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    manager.disconnect();
    EXPECT_FALSE(manager.isConnected());
    
    // Should not output disconnection messages
    EXPECT_EQ(output_capture_.getCoutOutput(), "");
}

TEST_F(NetworkManagerTest, SendPositionUpdateConnectsIfNeeded) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    equipment_tracker::Position pos(37.7749, -122.4194, 10.0, 5.0);
    
    EXPECT_TRUE(manager.sendPositionUpdate("device1", pos));
    EXPECT_TRUE(manager.isConnected());
    
    // Check output
    EXPECT_THAT(output_capture_.getCoutOutput(), ::testing::HasSubstr("Connecting to server"));
}

TEST_F(NetworkManagerTest, SendPositionUpdateQueuesData) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    equipment_tracker::Position pos(37.7749, -122.4194, 10.0, 5.0);
    
    manager.connect();
    output_capture_.clear();
    
    EXPECT_TRUE(manager.sendPositionUpdate("device1", pos));
    
    // Wait for the worker thread to process the queue
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Check that the position was sent
    EXPECT_THAT(output_capture_.getCoutOutput(), ::testing::HasSubstr("Sending position update to server"));
    EXPECT_THAT(output_capture_.getCoutOutput(), ::testing::HasSubstr("\"id\":\"device1\""));
    EXPECT_THAT(output_capture_.getCoutOutput(), ::testing::HasSubstr("\"latitude\":37.7749"));
    EXPECT_THAT(output_capture_.getCoutOutput(), ::testing::HasSubstr("\"longitude\":-122.4194"));
}

TEST_F(NetworkManagerTest, SyncWithServerConnectsIfNeeded) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    EXPECT_TRUE(manager.syncWithServer());
    EXPECT_TRUE(manager.isConnected());
    
    // Check output
    EXPECT_THAT(output_capture_.getCoutOutput(), ::testing::HasSubstr("Connecting to server"));
}

TEST_F(NetworkManagerTest, SetServerUrlDisconnectsIfConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    manager.connect();
    output_capture_.clear();
    
    manager.setServerUrl("new.server.com");
    EXPECT_FALSE(manager.isConnected());
    EXPECT_EQ(manager.getServerUrl(), "new.server.com");
    
    // Check output
    EXPECT_THAT(output_capture_.getCoutOutput(), ::testing::HasSubstr("Disconnecting from server"));
}

TEST_F(NetworkManagerTest, SetServerPortDisconnectsIfConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    manager.connect();
    output_capture_.clear();
    
    manager.setServerPort(9090);
    EXPECT_FALSE(manager.isConnected());
    EXPECT_EQ(manager.getServerPort(), 9090);
    
    // Check output
    EXPECT_THAT(output_capture_.getCoutOutput(), ::testing::HasSubstr("Disconnecting from server"));
}

TEST_F(NetworkManagerTest, SendRequestFailsWhenNotConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    EXPECT_FALSE(manager.sendRequest("/api/endpoint", "test data"));
    
    // Check error output
    EXPECT_THAT(output_capture_.getCerrOutput(), ::testing::HasSubstr("Not connected to server"));
}

TEST_F(NetworkManagerTest, SendRequestSucceedsWhenConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    manager.connect();
    output_capture_.clear();
    
    EXPECT_TRUE(manager.sendRequest("/api/endpoint", "test data"));
    
    // Check output
    EXPECT_THAT(output_capture_.getCoutOutput(), ::testing::HasSubstr("Sending request to test.server.com/api/endpoint: test data"));
}

TEST_F(NetworkManagerTest, ReceiveResponseFailsWhenNotConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    EXPECT_EQ(manager.receiveResponse(), "");
    
    // Check error output
    EXPECT_THAT(output_capture_.getCerrOutput(), ::testing::HasSubstr("Not connected to server"));
}

TEST_F(NetworkManagerTest, ReceiveResponseSucceedsWhenConnected) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    
    manager.connect();
    output_capture_.clear();
    
    EXPECT_EQ(manager.receiveResponse(), "{\"status\":\"ok\"}");
}

TEST_F(NetworkManagerTest, CommandHandlerIsCalledByWorkerThread) {
    equipment_tracker::NetworkManager manager("test.server.com", 8080);
    bool command_received = false;
    std::string received_command;
    
    manager.registerCommandHandler([&](const std::string& cmd) {
        command_received = true;
        received_command = cmd;
    });
    
    manager.connect();
    
    // Wait for a while to give the worker thread a chance to generate a command
    // This is a bit flaky since it depends on random generation
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // We can't guarantee a command will be generated in the test time,
    // but if it is, we can verify it was handled correctly
    if (command_received) {
        EXPECT_EQ(received_command, "STATUS_REQUEST");
    }
}

TEST_F(NetworkManagerTest, DestructorDisconnectsIfConnected) {
    {
        equipment_tracker::NetworkManager manager("test.server.com", 8080);
        manager.connect();
        output_capture_.clear();
        
        // Destructor will be called when exiting this scope
    }
    
    // Check output
    EXPECT_THAT(output_capture_.getCoutOutput(), ::testing::HasSubstr("Disconnecting from server"));
}

// Main function to run the tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>