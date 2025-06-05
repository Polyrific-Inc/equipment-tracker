#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include "equipment_tracker/data_storage.h"

namespace equipment_tracker
{
    // Helper function to get current timestamp (replaces missing time_utils.h)
    Timestamp getCurrentTimestampHelper()
    {
        return std::chrono::system_clock::now();
    }

    // Implementation
    DataStorage::DataStorage(const std::string &db_path,
                             size_t equipment_cache_size,
                             size_t position_cache_size)
        : db_path_(db_path),
          equipment_cache_(equipment_cache_size, std::chrono::seconds(600)), // 10 min TTL
          position_cache_(position_cache_size, std::chrono::seconds(300))    // 5 min TTL
    {
    }

    bool DataStorage::initialize()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return initializeInternal();
    }

    bool DataStorage::initializeInternal()
    {
        if (is_initialized_)
        {
            return true;
        }

        try
        {
            // Create the database directory if it doesn't exist
            std::filesystem::path dir_path = std::filesystem::path(db_path_).parent_path();
            if (!dir_path.empty() && !std::filesystem::exists(dir_path))
            {
                std::filesystem::create_directories(dir_path);
            }

            // Initialize the database structure
            initDatabase();

            is_initialized_ = true;
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "DataStorage initialization error: " << e.what() << std::endl;
            return false;
        }
    }

    bool DataStorage::saveEquipment(const Equipment &equipment)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!is_initialized_ && !initializeInternal())
        {
            return false;
        }

        try
        {
            // Create equipment directory
            std::string equipment_dir = db_path_ + "/equipment";
            if (!std::filesystem::exists(equipment_dir))
            {
                std::filesystem::create_directory(equipment_dir);
            }

            // Save equipment to file
            std::string filename = equipment_dir + "/" + equipment.getId() + ".txt";
            std::ofstream file(filename);

            if (!file.is_open())
            {
                std::cerr << "Failed to open file for writing: " << filename << std::endl;
                return false;
            }

            // Write equipment data
            file << "id=" << equipment.getId() << std::endl;
            file << "name=" << equipment.getName() << std::endl;
            file << "type=" << static_cast<int>(equipment.getType()) << std::endl;
            file << "status=" << static_cast<int>(equipment.getStatus()) << std::endl;

            // Write last position if available
            auto last_pos = equipment.getLastPosition();
            if (last_pos)
            {
                file << std::fixed << std::setprecision(10);
                file << "last_position=" << last_pos->getLatitude() << ","
                     << last_pos->getLongitude() << ","
                     << last_pos->getAltitude() << ","
                     << last_pos->getAccuracy() << ","
                     << std::chrono::system_clock::to_time_t(last_pos->getTimestamp())
                     << std::endl;
            }

            file.close();

            // Update cache
            equipment_cache_.put(equipment.getId(), equipment);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "DataStorage saveEquipment error: " << e.what() << std::endl;
            return false;
        }
    }

    std::optional<Equipment> DataStorage::loadEquipment(const EquipmentId &id)
    {
        // Try cache first (no lock needed, cache is thread-safe)
        auto cached = equipment_cache_.get(id);
        if (cached)
        {
            equipment_cache_hits_++;
            return *cached;
        }

        equipment_cache_misses_++;

        std::lock_guard<std::mutex> lock(mutex_);
        auto equipment = loadEquipmentInternal(id);

        if (equipment)
        {
            // Cache the result
            equipment_cache_.put(id, *equipment);
        }

        return equipment;
    }

    std::optional<Equipment> DataStorage::loadEquipmentInternal(const EquipmentId &id)
    {
        if (!is_initialized_ && !initializeInternal())
        {
            return std::nullopt;
        }

        try
        {
            // Check if equipment file exists
            std::string filename = db_path_ + "/equipment/" + id + ".txt";
            if (!std::filesystem::exists(filename))
            {
                return std::nullopt;
            }

            // Open file for reading
            std::ifstream file(filename);
            if (!file.is_open())
            {
                std::cerr << "Failed to open file for reading: " << filename << std::endl;
                return std::nullopt;
            }

            // Read equipment data
            std::string line;
            std::string name;
            EquipmentType type = EquipmentType::Other;
            EquipmentStatus status = EquipmentStatus::Unknown;
            std::optional<Position> last_position;

            while (std::getline(file, line))
            {
                size_t pos = line.find('=');
                if (pos != std::string::npos)
                {
                    std::string key = line.substr(0, pos);
                    std::string value = line.substr(pos + 1);

                    if (key == "name")
                    {
                        name = value;
                    }
                    else if (key == "type")
                    {
                        type = static_cast<EquipmentType>(std::stoi(value));
                    }
                    else if (key == "status")
                    {
                        status = static_cast<EquipmentStatus>(std::stoi(value));
                    }
                    else if (key == "last_position")
                    {
                        // Parse position data
                        std::stringstream ss(value);
                        std::string token;
                        std::vector<std::string> tokens;

                        while (std::getline(ss, token, ','))
                        {
                            tokens.push_back(token);
                        }

                        if (tokens.size() >= 5)
                        {
                            double lat = std::stod(tokens[0]);
                            double lon = std::stod(tokens[1]);
                            double alt = std::stod(tokens[2]);
                            double acc = std::stod(tokens[3]);
                            time_t timestamp = std::stoull(tokens[4]);

                            last_position = Position(
                                lat, lon, alt, acc,
                                std::chrono::system_clock::from_time_t(timestamp));
                        }
                    }
                }
            }

            file.close();

            // Create equipment object
            Equipment equipment(id, type, name);
            equipment.setStatus(status);

            if (last_position)
            {
                equipment.setLastPosition(*last_position);
            }

            // Load position history (call internal version without locking)
            auto history = getPositionHistoryInternal(id);
            for (const auto &pos : history)
            {
                equipment.recordPosition(pos);
            }

            return std::optional<Equipment>(std::move(equipment));
        }
        catch (const std::exception &e)
        {
            std::cerr << "DataStorage loadEquipmentInternal error: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    bool DataStorage::updateEquipment(const Equipment &equipment)
    {
        bool success = saveEquipment(equipment);
        if (success)
        {
            // Cache is already updated in saveEquipment
        }
        return success;
    }

    bool DataStorage::deleteEquipment(const EquipmentId &id)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!is_initialized_ && !initializeInternal())
        {
            return false;
        }

        try
        {
            // Delete equipment file
            std::string filename = db_path_ + "/equipment/" + id + ".txt";
            if (std::filesystem::exists(filename))
            {
                std::filesystem::remove(filename);
            }

            // Delete position history directory
            std::string history_dir = db_path_ + "/positions/" + id;
            if (std::filesystem::exists(history_dir))
            {
                std::filesystem::remove_all(history_dir);
            }

            // Remove from cache
            equipment_cache_.invalidate(id);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "DataStorage deleteEquipment error: " << e.what() << std::endl;
            return false;
        }
    }

    bool DataStorage::savePosition(const EquipmentId &id, const Position &position)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!is_initialized_ && !initializeInternal())
        {
            return false;
        }

        try
        {
            // Create positions directory
            std::string positions_dir = db_path_ + "/positions";
            if (!std::filesystem::exists(positions_dir))
            {
                std::filesystem::create_directory(positions_dir);
            }

            // Create equipment directory
            std::string equipment_dir = positions_dir + "/" + id;
            if (!std::filesystem::exists(equipment_dir))
            {
                std::filesystem::create_directory(equipment_dir);
            }

            // Create filename based on timestamp
            auto timestamp = std::chrono::system_clock::to_time_t(position.getTimestamp());
            std::string filename = equipment_dir + "/" + std::to_string(timestamp) + ".txt";

            // Save position to file
            std::ofstream file(filename);
            if (!file.is_open())
            {
                std::cerr << "Failed to open file for writing: " << filename << std::endl;
                return false;
            }

            file << std::fixed << std::setprecision(10);
            file << "latitude=" << position.getLatitude() << std::endl;
            file << "longitude=" << position.getLongitude() << std::endl;
            file << "altitude=" << position.getAltitude() << std::endl;
            file << "accuracy=" << position.getAccuracy() << std::endl;
            file << "timestamp=" << timestamp << std::endl;

            file.close();

            // Invalidate related equipment cache since position changed
            equipment_cache_.invalidate(id);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "DataStorage savePosition error: " << e.what() << std::endl;
            return false;
        }
    }

    std::vector<Position> DataStorage::getPositionHistory(
        const EquipmentId &id,
        const Timestamp &start,
        const Timestamp &end)
    {
        // Handle default timestamp values
        Timestamp actual_start = (start == Timestamp{}) ? std::chrono::system_clock::time_point::min() : start;
        Timestamp actual_end = (end == Timestamp{}) ? getCurrentTimestampHelper() : end;

        // Try cache first
        PositionQueryKey key{id, actual_start, actual_end};
        auto cached = position_cache_.get(key);
        if (cached)
        {
            position_cache_hits_++;
            return *cached;
        }

        position_cache_misses_++;

        std::lock_guard<std::mutex> lock(mutex_);
        auto positions = getPositionHistoryInternal(id, actual_start, actual_end);

        // Cache the result
        position_cache_.put(key, positions);

        return positions;
    }

    std::vector<Position> DataStorage::getPositionHistoryInternal(
        const EquipmentId &id,
        const Timestamp &start,
        const Timestamp &end)
    {
        std::vector<Position> result;

        if (!is_initialized_ && !initializeInternal())
        {
            return result;
        }

        try
        {
            // Check if equipment position directory exists
            std::string directory = db_path_ + "/positions/" + id;
            if (!std::filesystem::exists(directory))
            {
                return result;
            }

            // Convert timestamps to time_t
            time_t start_time = std::chrono::system_clock::to_time_t(start);
            time_t end_time = std::chrono::system_clock::to_time_t(end);

            // Collect all position files
            std::vector<std::filesystem::path> position_files;
            for (const auto &entry : std::filesystem::directory_iterator(directory))
            {
                if (entry.is_regular_file())
                {
                    position_files.push_back(entry.path());
                }
            }

            // Sort files by name (which is based on timestamp)
            std::sort(position_files.begin(), position_files.end());

            // Load positions within time range
            for (const auto &path : position_files)
            {
                std::string filename = path.filename().string();

                // Extract timestamp from filename
                size_t dot_pos = filename.find('.');
                if (dot_pos != std::string::npos)
                {
                    std::string timestamp_str = filename.substr(0, dot_pos);
                    time_t timestamp = std::stoull(timestamp_str);

                    // Check if within time range
                    if (timestamp >= start_time && timestamp < end_time)
                    {
                        // Load position from file
                        std::ifstream file(path);
                        if (file.is_open())
                        {
                            double latitude = 0.0;
                            double longitude = 0.0;
                            double altitude = 0.0;
                            double accuracy = DEFAULT_POSITION_ACCURACY;

                            std::string line;
                            while (std::getline(file, line))
                            {
                                size_t pos = line.find('=');
                                if (pos != std::string::npos)
                                {
                                    std::string key = line.substr(0, pos);
                                    std::string value = line.substr(pos + 1);

                                    if (key == "latitude")
                                    {
                                        latitude = std::stod(value);
                                    }
                                    else if (key == "longitude")
                                    {
                                        longitude = std::stod(value);
                                    }
                                    else if (key == "altitude")
                                    {
                                        altitude = std::stod(value);
                                    }
                                    else if (key == "accuracy")
                                    {
                                        accuracy = std::stod(value);
                                    }
                                }
                            }

                            file.close();

                            // Create position and add to result
                            result.emplace_back(
                                latitude, longitude, altitude, accuracy,
                                std::chrono::system_clock::from_time_t(timestamp));
                        }
                    }
                }
            }

            return result;
        }
        catch (const std::exception &e)
        {
            std::cerr << "DataStorage getPositionHistoryInternal error: " << e.what() << std::endl;
            return result;
        }
    }

    std::vector<Equipment> DataStorage::getAllEquipment()
    {
        std::vector<Equipment> result;

        std::lock_guard<std::mutex> lock(mutex_);

        if (!is_initialized_ && !initializeInternal())
        {
            return result;
        }

        try
        {
            // Check if equipment directory exists
            std::string directory = db_path_ + "/equipment";
            if (!std::filesystem::exists(directory))
            {
                return result;
            }

            // Iterate through all equipment files
            for (const auto &entry : std::filesystem::directory_iterator(directory))
            {
                if (entry.is_regular_file())
                {
                    std::string filename = entry.path().filename().string();

                    // Extract equipment ID from filename
                    size_t dot_pos = filename.find('.');
                    if (dot_pos != std::string::npos)
                    {
                        std::string id = filename.substr(0, dot_pos);

                        // Try cache first
                        auto cached = equipment_cache_.get(id);
                        if (cached)
                        {
                            result.push_back(*cached);
                        }
                        else
                        {
                            // Load equipment (call internal version without locking)
                            auto equipment = loadEquipmentInternal(id);
                            if (equipment)
                            {
                                // Cache the loaded equipment
                                equipment_cache_.put(id, *equipment);
                                result.push_back(std::move(*equipment));
                            }
                        }
                    }
                }
            }

            return result;
        }
        catch (const std::exception &e)
        {
            std::cerr << "DataStorage getAllEquipment error: " << e.what() << std::endl;
            return result;
        }
    }

    std::vector<Equipment> DataStorage::findEquipmentByStatus(EquipmentStatus status)
    {
        std::vector<Equipment> result;

        // Get all equipment
        auto all_equipment = getAllEquipment();

        // Filter by status
        for (auto &equipment : all_equipment)
        {
            if (equipment.getStatus() == status)
            {
                result.push_back(std::move(equipment));
            }
        }

        return result;
    }

    std::vector<Equipment> DataStorage::findEquipmentByType(EquipmentType type)
    {
        std::vector<Equipment> result;

        // Get all equipment
        auto all_equipment = getAllEquipment();

        // Filter by type
        for (auto &equipment : all_equipment)
        {
            if (equipment.getType() == type)
            {
                result.push_back(std::move(equipment));
            }
        }

        return result;
    }

    std::vector<Equipment> DataStorage::findEquipmentInArea(
        double lat1, double lon1,
        double lat2, double lon2)
    {
        std::vector<Equipment> result;

        // Get all equipment
        auto all_equipment = getAllEquipment();

        // Filter by area
        for (auto &equipment : all_equipment)
        {
            auto position = equipment.getLastPosition();

            if (position)
            {
                double lat = position->getLatitude();
                double lon = position->getLongitude();

                // Check if position is within bounds
                if (lat >= std::min(lat1, lat2) && lat <= std::max(lat1, lat2) &&
                    lon >= std::min(lon1, lon2) && lon <= std::max(lon1, lon2))
                {
                    result.push_back(std::move(equipment));
                }
            }
        }

        return result;
    }

    void DataStorage::clearCache()
    {
        equipment_cache_.clear();
        position_cache_.clear();
    }

    void DataStorage::clearEquipmentCache()
    {
        equipment_cache_.clear();
    }

    void DataStorage::clearPositionCache()
    {
        position_cache_.clear();
    }

    DataStorage::CacheStats DataStorage::getCacheStats() const
    {
        CacheStats stats;
        stats.equipment_hits = equipment_cache_hits_.load();
        stats.equipment_misses = equipment_cache_misses_.load();
        stats.position_hits = position_cache_hits_.load();
        stats.position_misses = position_cache_misses_.load();
        stats.equipment_cache_size = equipment_cache_.size();
        stats.position_cache_size = position_cache_.size();

        size_t total_equipment = stats.equipment_hits + stats.equipment_misses;
        size_t total_position = stats.position_hits + stats.position_misses;

        stats.equipment_hit_rate = total_equipment > 0 ? static_cast<double>(stats.equipment_hits) / total_equipment : 0.0;
        stats.position_hit_rate = total_position > 0 ? static_cast<double>(stats.position_hits) / total_position : 0.0;

        return stats;
    }

    void DataStorage::resetCacheStats()
    {
        equipment_cache_hits_ = 0;
        equipment_cache_misses_ = 0;
        position_cache_hits_ = 0;
        position_cache_misses_ = 0;
    }

    void DataStorage::initDatabase()
    {
        // Create database directory structure
        std::filesystem::create_directory(db_path_);
        std::filesystem::create_directory(db_path_ + "/equipment");
        std::filesystem::create_directory(db_path_ + "/positions");
    }

    void DataStorage::prepareStatements()
    {
        // Placeholder for SQL statement preparation
        // In a real implementation, this would prepare SQL statements
        // for improved performance
    }

    bool DataStorage::executeQuery(const std::string &query)
    {
        // This is a placeholder for a real database query
        // In a real implementation, this would execute an SQL query

        // For now, just print the query for debugging
        std::cout << "Query: " << query << std::endl;

        return true;
    }

} // namespace equipment_tracker