#include <iostream>
#include <algorithm>
#include "equipment_tracker/equipment_tracker_service.h"

namespace equipment_tracker
{

    EquipmentTrackerService::EquipmentTrackerService()
        : gps_tracker_(std::make_unique<GPSTracker>()),
          data_storage_(std::make_unique<DataStorage>()),
          network_manager_(std::make_unique<NetworkManager>()),
          is_running_(false)
    {

        // Register position callback
        gps_tracker_->registerPositionCallback(
            [this](double lat, double lon, double alt, Timestamp timestamp)
            {
                this->handlePositionUpdate(lat, lon, alt, timestamp);
            });

        // Register command handler
        network_manager_->registerCommandHandler(
            [this](const std::string &command)
            {
                this->handleRemoteCommand(command);
            });
    }

    EquipmentTrackerService::~EquipmentTrackerService()
    {
        stop();
    }

    void EquipmentTrackerService::start()
    {
        if (is_running_)
            return;

        std::cout << "Starting Equipment Tracker Service..." << std::endl;

        // Initialize data storage
        if (!data_storage_->initialize())
        {
            std::cerr << "Failed to initialize data storage." << std::endl;
            return;
        }

        // Load equipment from storage
        loadEquipment();

        // Connect to server
        network_manager_->connect();

        // Start GPS tracker
        gps_tracker_->start();

        is_running_ = true;

        std::cout << "Equipment Tracker Service started." << std::endl;
    }

    void EquipmentTrackerService::stop()
    {
        if (!is_running_)
            return;

        std::cout << "Stopping Equipment Tracker Service..." << std::endl;

        // Stop GPS tracker
        gps_tracker_->stop();

        // Disconnect from server
        network_manager_->disconnect();

        is_running_ = false;

        std::cout << "Equipment Tracker Service stopped." << std::endl;
    }

    bool EquipmentTrackerService::addEquipment(const Equipment &equipment)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Check if equipment already exists
        if (equipment_map_.find(equipment.getId()) != equipment_map_.end())
        {
            std::cerr << "Equipment with ID " << equipment.getId() << " already exists." << std::endl;
            return false;
        }

        // Add to map and storage
        equipment_map_[equipment.getId()] = equipment;
        return data_storage_->saveEquipment(equipment);
    }

    bool EquipmentTrackerService::removeEquipment(const EquipmentId &id)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Check if equipment exists
        if (equipment_map_.find(id) == equipment_map_.end())
        {
            std::cerr << "Equipment with ID " << id << " does not exist." << std::endl;
            return false;
        }

        // Remove from map and storage
        equipment_map_.erase(id);
        return data_storage_->deleteEquipment(id);
    }

    std::optional<Equipment> EquipmentTrackerService::getEquipment(const EquipmentId &id) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = equipment_map_.find(id);
        if (it == equipment_map_.end())
        {
            return std::nullopt;
        }

        return it->second;
    }

    std::vector<Equipment> EquipmentTrackerService::getAllEquipment() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<Equipment> result;
        result.reserve(equipment_map_.size());

        for (const auto &[_, equipment] : equipment_map_)
        {
            result.push_back(equipment);
        }

        return result;
    }

    std::vector<Equipment> EquipmentTrackerService::findEquipmentByStatus(EquipmentStatus status) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<Equipment> result;

        for (const auto &[_, equipment] : equipment_map_)
        {
            if (equipment.getStatus() == status)
            {
                result.push_back(equipment);
            }
        }

        return result;
    }

    std::vector<Equipment> EquipmentTrackerService::findActiveEquipment() const
    {
        return findEquipmentByStatus(EquipmentStatus::Active);
    }

    std::vector<Equipment> EquipmentTrackerService::findEquipmentInArea(
        double lat1, double lon1,
        double lat2, double lon2) const
    {

        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<Equipment> result;

        for (const auto &[_, equipment] : equipment_map_)
        {
            auto position = equipment.getLastPosition();

            if (!position)
            {
                continue;
            }

            double lat = position->getLatitude();
            double lon = position->getLongitude();

            // Check if position is within bounds
            if (lat >= std::min(lat1, lat2) && lat <= std::max(lat1, lat2) &&
                lon >= std::min(lon1, lon2) && lon <= std::max(lon1, lon2))
            {
                result.push_back(equipment);
            }
        }

        return result;
    }

    bool EquipmentTrackerService::setGeofence(
        const EquipmentId &id,
        double lat1, double lon1,
        double lat2, double lon2)
    {

        // This is a placeholder for a real geofencing implementation
        // In a real application, would store geofence information and
        // trigger alerts when equipment leaves the geofence

        std::cout << "Setting geofence for equipment " << id << ":" << std::endl;
        std::cout << "  Southwest corner: " << lat1 << ", " << lon1 << std::endl;
        std::cout << "  Northeast corner: " << lat2 << ", " << lon2 << std::endl;

        return true;
    }

    void EquipmentTrackerService::loadEquipment()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::cout << "Loading equipment from storage..." << std::endl;

        auto equipment_list = data_storage_->getAllEquipment();

        equipment_map_.clear();
        for (const auto &equipment : equipment_list)
        {
            equipment_map_[equipment.getId()] = equipment;
            std::cout << "  Loaded " << equipment.toString() << std::endl;
        }

        std::cout << "Loaded " << equipment_map_.size() << " equipment items." << std::endl;
    }

    void EquipmentTrackerService::handlePositionUpdate(
        double latitude, double longitude,
        double altitude, Timestamp timestamp)
    {

        // Create position object
        Position position(latitude, longitude, altitude, DEFAULT_POSITION_ACCURACY, timestamp);

        // Find the current equipment based on some criteria
        // (in a real app, would need to identify which device is which equipment)
        auto equipment_id = determineEquipmentId();

        if (!equipment_id)
        {
            // Unknown equipment
            std::cerr << "Position update received but could not determine equipment ID." << std::endl;
            return;
        }

        std::cout << "Position update received for equipment " << *equipment_id << "." << std::endl;

        // Update equipment position
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = equipment_map_.find(*equipment_id);
        if (it == equipment_map_.end())
        {
            std::cerr << "Equipment with ID " << *equipment_id << " not found." << std::endl;
            return;
        }

        // Update equipment
        it->second.recordPosition(position);
        it->second.setStatus(EquipmentStatus::Active);

        // Save to database
        data_storage_->savePosition(*equipment_id, position);
        data_storage_->updateEquipment(it->second);

        // Send to server
        network_manager_->sendPositionUpdate(*equipment_id, position);
    }

    void EquipmentTrackerService::handleRemoteCommand(const std::string &command)
    {
        std::cout << "Remote command received: " << command << std::endl;

        // Handle different commands
        if (command == "STATUS_REQUEST")
        {
            // Send status of all equipment
            auto equipment_list = getAllEquipment();

            // In a real app, would format status data and send to server
            std::cout << "Sending status of " << equipment_list.size() << " equipment items." << std::endl;
        }
        // Add other command handlers as needed
    }

    std::optional<EquipmentId> EquipmentTrackerService::determineEquipmentId()
    {
        // In a real app, would determine which equipment this is based on
        // device ID, connection, etc.

        // For now, return the first equipment in the map, or a mock ID if empty
        std::lock_guard<std::mutex> lock(mutex_);

        if (equipment_map_.empty())
        {
            return "FORKLIFT-001";
        }

        return equipment_map_.begin()->first;
    }

} // namespace equipment_tracker