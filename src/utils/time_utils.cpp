#include "equipment_tracker/utils/time_utils.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "equipment_tracker/utils/types.h"

namespace equipment_tracker
{

    Timestamp getCurrentTimestamp()
    {
        return std::chrono::system_clock::now();
    }

    std::string formatTimestamp(const Timestamp &timestamp, const std::string &format)
    {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        std::tm tm = {};
#ifdef _WIN32
        // Windows-specific code
        localtime_s(&tm, &time_t);
#else
        // Linux/Unix/macOS code
        localtime_r(&time_t, &tm);
#endif

        std::stringstream ss;
        ss << std::put_time(&tm, format.c_str());

        return ss.str();
    }

    Timestamp parseTimestamp(const std::string &str, const std::string &format)
    {
        std::tm tm = {};
        std::stringstream ss(str);

        ss >> std::get_time(&tm, format.c_str());

        auto time_t = std::mktime(&tm);
        return std::chrono::system_clock::from_time_t(time_t);
    }

    int64_t timestampDiffSeconds(const Timestamp &t1, const Timestamp &t2)
    {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(t1 - t2);
        return duration.count();
    }

    int64_t timestampDiffMinutes(const Timestamp &t1, const Timestamp &t2)
    {
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(t1 - t2);
        return duration.count();
    }

    int64_t timestampDiffHours(const Timestamp &t1, const Timestamp &t2)
    {
        auto duration = std::chrono::duration_cast<std::chrono::hours>(t1 - t2);
        return duration.count();
    }

    int64_t timestampDiffDays(const Timestamp &t1, const Timestamp &t2)
    {
        auto duration = std::chrono::duration_cast<std::chrono::hours>(t1 - t2);
        return duration.count() / 24;
    }

    Timestamp addSeconds(const Timestamp &timestamp, int64_t seconds)
    {
        return timestamp + std::chrono::seconds(seconds);
    }

    Timestamp addMinutes(const Timestamp &timestamp, int64_t minutes)
    {
        return timestamp + std::chrono::minutes(minutes);
    }

    Timestamp addHours(const Timestamp &timestamp, int64_t hours)
    {
        return timestamp + std::chrono::hours(hours);
    }

    Timestamp addDays(const Timestamp &timestamp, int64_t days)
    {
        return timestamp + std::chrono::hours(days * 24);
    }

} // namespace equipment_tracker