#pragma once

#include <string>
#include <cstdint>
#include "equipment_tracker/utils/types.h"

namespace equipment_tracker
{
    // Get current timestamp
    Timestamp getCurrentTimestamp();
    
    // Timestamp formatting utilities
    std::string formatTimestamp(const Timestamp &timestamp, const std::string &format);
    
    // Timestamp parsing utilities  
    Timestamp parseTimestamp(const std::string &str, const std::string &format);
    
    // Timestamp difference calculations
    int64_t timestampDiffSeconds(const Timestamp &t1, const Timestamp &t2);
    int64_t timestampDiffMinutes(const Timestamp &t1, const Timestamp &t2);
    int64_t timestampDiffHours(const Timestamp &t1, const Timestamp &t2);
    int64_t timestampDiffDays(const Timestamp &t1, const Timestamp &t2);
    
    // Timestamp addition utilities
    Timestamp addSeconds(const Timestamp &timestamp, int64_t seconds);
    Timestamp addMinutes(const Timestamp &timestamp, int64_t minutes);
    Timestamp addHours(const Timestamp &timestamp, int64_t hours);
    Timestamp addDays(const Timestamp &timestamp, int64_t days);

} // namespace equipment_tracker