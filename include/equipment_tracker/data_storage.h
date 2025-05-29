#pragma once

#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include "utils/types.h"
#include "utils/constants.h"
#include "equipment.h"
#include "position.h"

namespace equipment_tracker {

/**
 * @brief Manages persistent storage of equipment and position data
 */
class DataStorage {
public:
    // Constructor
    explicit DataStorage(const std::string& db_path = DEFAULT_DB_PATH);
    
    // Database initialization
    bool initialize();
    
    // Equipment CRUD operations
    bool saveEquipment(const Equipment& equipment);
    std::optional<Equipment> loadEquipment(const EquipmentId& id);
    bool updateEquipment(const Equipment& equipment);
    bool deleteEquipment(const EquipmentId& id);
    
    // Position history operations
    bool savePosition(const EquipmentId& id, const Position& position);
    std::vector<Position> getPositionHistory(
        const EquipmentId& id, 
        const Timestamp& start = Timestamp(),
        const Timestamp& end = getCurrentTimestamp()
    );
    
    // Query operations
    std::vector<Equipment> getAllEquipment();
    std::vector<Equipment> findEquipmentByStatus(EquipmentStatus status);
    std::vector<Equipment> findEquipmentByType(EquipmentType type);
    std::vector<Equipment> findEquipmentInArea(
        double lat1, double lon1, 
        double lat2, double lon2
    );
    
private:
    std::string db_path_;
    mutable std::mutex mutex_;
    bool is_initialized_{false};
    
    // Private helper methods
    void initDatabase();
    bool executeQuery(const std::string& query);
    
    // Internal method that doesn't acquire mutex (for use when mutex is already locked)
    std::vector<Position> getPositionHistoryInternal(
        const EquipmentId& id, 
        const Timestamp& start = Timestamp(),
        const Timestamp& end = getCurrentTimestamp()
    );
    
    // SQL statement preparation
    void prepareStatements();
    
    // In a real implementation, would have database connection
    // and prepared statement objects here
};

} // namespace equipment_tracker