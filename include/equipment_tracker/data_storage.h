#pragma once

#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <list>
#include "utils/types.h"
#include "utils/constants.h"
#include "equipment.h"
#include "position.h"

namespace equipment_tracker
{
    // LRU Cache template class (must be in header for template instantiation)
    template <typename Key, typename Value>
    class LRUCache
    {
    private:
        struct CacheItem
        {
            Value value;
            std::chrono::steady_clock::time_point timestamp;
            typename std::list<Key>::iterator list_iter;

            // Explicit constructors to avoid default constructor issues
            CacheItem(const Value &v, std::chrono::steady_clock::time_point ts, typename std::list<Key>::iterator iter)
                : value(v), timestamp(ts), list_iter(iter) {}

            CacheItem(Value &&v, std::chrono::steady_clock::time_point ts, typename std::list<Key>::iterator iter)
                : value(std::move(v)), timestamp(ts), list_iter(iter) {}
        };

        size_t capacity_;
        std::chrono::seconds ttl_;
        mutable std::mutex mutex_;
        std::unordered_map<Key, CacheItem> cache_;
        std::list<Key> usage_order_; // Most recent at front

    public:
        LRUCache(size_t capacity, std::chrono::seconds ttl = std::chrono::seconds(300))
            : capacity_(capacity), ttl_(ttl) {}

        std::optional<Value> get(const Key &key)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = cache_.find(key);
            if (it == cache_.end())
            {
                return std::nullopt;
            }

            // Check if expired
            auto now = std::chrono::steady_clock::now();
            if (now - it->second.timestamp > ttl_)
            {
                // Remove expired item
                usage_order_.erase(it->second.list_iter);
                cache_.erase(it);
                return std::nullopt;
            }

            // Move to front (most recently used)
            usage_order_.erase(it->second.list_iter);
            usage_order_.push_front(key);
            it->second.list_iter = usage_order_.begin();

            return it->second.value;
        }

        void put(const Key &key, const Value &value)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = cache_.find(key);
            if (it != cache_.end())
            {
                // Update existing item
                it->second.value = value;
                it->second.timestamp = std::chrono::steady_clock::now();

                // Move to front
                usage_order_.erase(it->second.list_iter);
                usage_order_.push_front(key);
                it->second.list_iter = usage_order_.begin();
                return;
            }

            // Add new item
            if (cache_.size() >= capacity_)
            {
                // Remove least recently used
                Key lru_key = usage_order_.back();
                usage_order_.pop_back();
                cache_.erase(lru_key);
            }

            usage_order_.push_front(key);

            // Use emplace to avoid requiring default constructor
            cache_.emplace(key, CacheItem(
                                    value,
                                    std::chrono::steady_clock::now(),
                                    usage_order_.begin()));
        }

        void invalidate(const Key &key)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = cache_.find(key);
            if (it != cache_.end())
            {
                usage_order_.erase(it->second.list_iter);
                cache_.erase(it);
            }
        }

        void clear()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cache_.clear();
            usage_order_.clear();
        }

        size_t size() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return cache_.size();
        }
    };

    // Position query key for caching position history
    struct PositionQueryKey
    {
        EquipmentId equipment_id;
        std::chrono::system_clock::time_point start;
        std::chrono::system_clock::time_point end;

        bool operator==(const PositionQueryKey &other) const
        {
            return equipment_id == other.equipment_id &&
                   start == other.start && end == other.end;
        }
    };

    struct PositionQueryKeyHash
    {
        size_t operator()(const PositionQueryKey &key) const
        {
            auto h1 = std::hash<std::string>{}(key.equipment_id);
            auto h2 = std::hash<long long>{}(key.start.time_since_epoch().count());
            auto h3 = std::hash<long long>{}(key.end.time_since_epoch().count());
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

} // namespace equipment_tracker

// Specialize std::hash for PositionQueryKey
namespace std
{
    template <>
    struct hash<equipment_tracker::PositionQueryKey>
    {
        size_t operator()(const equipment_tracker::PositionQueryKey &key) const
        {
            return equipment_tracker::PositionQueryKeyHash{}(key);
        }
    };
}

namespace equipment_tracker
{

    /**
     * @brief Manages persistent storage of equipment and position data with LRU caching
     *
     * Enhanced with caching features:
     * - Equipment caching with configurable size and TTL
     * - Position history caching for frequently accessed queries
     * - Thread-safe operations with proper cache invalidation
     * - Cache statistics and monitoring
     */
    class DataStorage
    {
    private:
        std::string db_path_;
        mutable std::mutex mutex_;
        bool is_initialized_{false};

        // Cache layers
        LRUCache<EquipmentId, Equipment> equipment_cache_;
        LRUCache<PositionQueryKey, std::vector<Position>> position_cache_;

        // Cache statistics
        mutable std::atomic<size_t> equipment_cache_hits_{0};
        mutable std::atomic<size_t> equipment_cache_misses_{0};
        mutable std::atomic<size_t> position_cache_hits_{0};
        mutable std::atomic<size_t> position_cache_misses_{0};

        // Private helper methods
        void initDatabase();
        bool executeQuery(const std::string &query);
        bool initializeInternal(); // Internal initialization without mutex lock

        // Internal method that doesn't acquire mutex (for use when mutex is already locked)
        std::optional<Equipment> loadEquipmentInternal(const EquipmentId &id);
        std::vector<Position> getPositionHistoryInternal(
            const EquipmentId &id,
            const Timestamp &start = Timestamp(),
            const Timestamp &end = Timestamp());

        // SQL statement preparation
        void prepareStatements();

        // In a real implementation, would have database connection
        // and prepared statement objects here

    public:
        // Constructor (enhanced with cache configuration)
        explicit DataStorage(const std::string &db_path = DEFAULT_DB_PATH,
                             size_t equipment_cache_size = 1000,
                             size_t position_cache_size = 500);

        // Database initialization
        bool initialize();

        // Equipment CRUD operations (cache-enabled)
        bool saveEquipment(const Equipment &equipment);
        std::optional<Equipment> loadEquipment(const EquipmentId &id);
        bool updateEquipment(const Equipment &equipment);
        bool deleteEquipment(const EquipmentId &id);

        // Position history operations (cache-enabled)
        bool savePosition(const EquipmentId &id, const Position &position);
        std::vector<Position> getPositionHistory(
            const EquipmentId &id,
            const Timestamp &start = Timestamp(),
            const Timestamp &end = Timestamp());

        // Query operations (cache-optimized)
        std::vector<Equipment> getAllEquipment();
        std::vector<Equipment> findEquipmentByStatus(EquipmentStatus status);
        std::vector<Equipment> findEquipmentByType(EquipmentType type);
        std::vector<Equipment> findEquipmentInArea(
            double lat1, double lon1,
            double lat2, double lon2);

        // Cache management
        /**
         * @brief Clear all caches
         */
        void clearCache();

        /**
         * @brief Clear only equipment cache
         */
        void clearEquipmentCache();

        /**
         * @brief Clear only position cache
         */
        void clearPositionCache();

        // Cache statistics and monitoring
        struct CacheStats
        {
            size_t equipment_hits;       // Number of equipment cache hits
            size_t equipment_misses;     // Number of equipment cache misses
            size_t position_hits;        // Number of position cache hits
            size_t position_misses;      // Number of position cache misses
            size_t equipment_cache_size; // Current equipment cache size
            size_t position_cache_size;  // Current position cache size
            double equipment_hit_rate;   // Equipment cache hit rate (0.0 - 1.0)
            double position_hit_rate;    // Position cache hit rate (0.0 - 1.0)
        };

        /**
         * @brief Get cache performance statistics
         * @return CacheStats structure with current statistics
         */
        CacheStats getCacheStats() const;

        /**
         * @brief Reset cache statistics counters
         */
        void resetCacheStats();
    };

} // namespace equipment_tracker