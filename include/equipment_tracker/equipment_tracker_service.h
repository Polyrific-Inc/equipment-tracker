#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <vector>
#include "utils/types.h"
#include "equipment.h"
#include "gps_tracker.h"
#include "data_storage.h"
#include "network_manager.h"

namespace equipment_tracker {

/**
 * @brief Main service class that coordinates all components
 */
class EquipmentTrackerService {
public:
    // Constructor
    EquipmentTrackerService();
    
    // Destructor
    ~EquipmentTrackerService();
    
    // Service control
    void start();
    void stop();
    bool isRunning() const { return is_running_; }
    
    // Equipment management
    bool addEquipment(const Equipment& equipment);
    bool removeEquipment(const EquipmentId& id);
    std::optional<Equipment> getEquipment(const EquipmentId& id) const;
    std::vector<Equipment> getAllEquipment() const;
    
    // Equipment queries
    std::vector<Equipment> findEquipmentByStatus(EquipmentStatus status) const;
    std::vector<Equipment> findActiveEquipment() const;
    std::vector<Equipment> findEquipmentInArea(
        double lat1, double lon1, 
        double lat2, double lon2
    ) const;
    
    // Advanced features 
    bool setGeofence(const EquipmentId& id, 
                    double lat1, double lon1, 
                    double lat2, double lon2);
    
    // Component access (for advanced usage)
    GPSTracker& getGPSTracker() { return *gps_tracker_; }
    DataStorage& getDataStorage() { return *data_storage_; }
    NetworkManager& getNetworkManager() { return *network_manager_; }
    
private:
    std::unique_ptr<GPSTracker> gps_tracker_;
    std::unique_ptr<DataStorage> data_storage_;
    std::unique_ptr<NetworkManager> network_manager_;
    
    std::unordered_map<EquipmentId, Equipment> equipment_map_;
    bool is_running_{false};
    mutable std::mutex mutex_;
    
    // Private methods
    void loadEquipment();
    void handlePositionUpdate(double latitude, double longitude, 
                            double altitude, Timestamp timestamp);
    void handleRemoteCommand(const std::string& command);
    std::optional<EquipmentId> determineEquipmentId();
};

} // namespace equipment_tracker