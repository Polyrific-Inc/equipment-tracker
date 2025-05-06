#pragma once

#include <string>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>
#include "utils/types.h"
#include "utils/constants.h"
#include "position.h"

namespace equipment_tracker {

/**
 * @brief Manages network communication with central servers
 */
class NetworkManager {
public:
    // Constructor
    NetworkManager(const std::string& server_url = DEFAULT_SERVER_URL, 
                   int server_port = DEFAULT_SERVER_PORT);
    
    // Destructor to ensure clean shutdown
    ~NetworkManager();
    
    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const { return is_connected_; }
    
    // Data synchronization
    bool sendPositionUpdate(const EquipmentId& id, const Position& position);
    bool syncWithServer();
    
    // Command handling
    void registerCommandHandler(std::function<void(const std::string&)> handler);
    
    // Server configuration
    void setServerUrl(const std::string& url);
    void setServerPort(int port);
    std::string getServerUrl() const { return server_url_; }
    int getServerPort() const { return server_port_; }
    
private:
    std::string server_url_;
    int server_port_;
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> should_run_{false};
    
    std::function<void(const std::string&)> command_handler_;
    
    // Background processing
    std::thread worker_thread_;
    std::queue<std::pair<EquipmentId, Position>> position_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_condition_;
    
    // Private methods
    void workerThreadFunction();
    bool processQueuedUpdates();
    
    // Network operations
    bool sendRequest(const std::string& endpoint, const std::string& data);
    std::string receiveResponse();
};

} // namespace equipment_tracker