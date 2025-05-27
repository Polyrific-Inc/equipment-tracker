#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include "equipment_tracker/utils/types.h"

namespace equipment_tracker
{
    // Forward declarations only - implementations are in time_utils.cpp

    /**
     * @brief Get current timestamp
     * @return Current system timestamp
     */
    Timestamp getCurrentTimestamp();

    /**
     * @brief Format timestamp to string using specified format
     * @param timestamp The timestamp to format
     * @param format Format string (e.g., "%Y-%m-%d %H:%M:%S")
     * @return Formatted timestamp string
     */
    std::string formatTimestamp(const Timestamp &timestamp, const std::string &format = "%Y-%m-%d %H:%M:%S");

    /**
     * @brief Parse timestamp from string
     * @param str String representation of timestamp
     * @param format Format string used for parsing
     * @return Parsed timestamp
     */
    Timestamp parseTimestamp(const std::string &str, const std::string &format = "%Y-%m-%d %H:%M:%S");

    /**
     * @brief Calculate difference between two timestamps in seconds
     * @param t1 First timestamp
     * @param t2 Second timestamp
     * @return Difference in seconds (t1 - t2)
     */
    int64_t timestampDiffSeconds(const Timestamp &t1, const Timestamp &t2);

    /**
     * @brief Calculate difference between two timestamps in minutes
     * @param t1 First timestamp
     * @param t2 Second timestamp
     * @return Difference in minutes (t1 - t2)
     */
    int64_t timestampDiffMinutes(const Timestamp &t1, const Timestamp &t2);

    /**
     * @brief Calculate difference between two timestamps in hours
     * @param t1 First timestamp
     * @param t2 Second timestamp
     * @return Difference in hours (t1 - t2)
     */
    int64_t timestampDiffHours(const Timestamp &t1, const Timestamp &t2);

    /**
     * @brief Calculate difference between two timestamps in days
     * @param t1 First timestamp
     * @param t2 Second timestamp
     * @return Difference in days (t1 - t2)
     */
    int64_t timestampDiffDays(const Timestamp &t1, const Timestamp &t2);

    /**
     * @brief Add seconds to timestamp
     * @param timestamp Base timestamp
     * @param seconds Seconds to add
     * @return New timestamp with added seconds
     */
    Timestamp addSeconds(const Timestamp &timestamp, int64_t seconds);

    /**
     * @brief Add minutes to timestamp
     * @param timestamp Base timestamp
     * @param minutes Minutes to add
     * @return New timestamp with added minutes
     */
    Timestamp addMinutes(const Timestamp &timestamp, int64_t minutes);

    /**
     * @brief Add hours to timestamp
     * @param timestamp Base timestamp
     * @param hours Hours to add
     * @return New timestamp with added hours
     */
    Timestamp addHours(const Timestamp &timestamp, int64_t hours);

    /**
     * @brief Add days to timestamp
     * @param timestamp Base timestamp
     * @param days Days to add
     * @return New timestamp with added days
     */
    Timestamp addDays(const Timestamp &timestamp, int64_t days);

} // namespace equipment_tracker