#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <iomanip>
#include "utils/types.h"
#include "utils/constants.h"

// Include NMEAParser
#include <NMEAParser.h>

namespace equipment_tracker
{

    /**
     * @brief Custom NMEA Parser that extends the CNMEAParser class
     */
    class EquipmentNMEAParser : public CNMEAParser
    {
    public:
        EquipmentNMEAParser() = default;

        // Set a callback for position updates
        void setPositionCallback(PositionCallback callback)
        {
            position_callback_ = callback;
        }

        // Override ProcessNMEABuffer to trigger position callbacks
        CNMEAParserData::ERROR_E ProcessNMEABuffer(char *pBuffer, int iSize) override;

        // Trigger position callback when position data is available
        void triggerPositionCallback(double latitude, double longitude, double altitude);

    protected:
        // Override error handling
        void OnError(CNMEAParserData::ERROR_E nError, char *pCmd) override;

        // Override data lock/unlock for thread safety
        void LockDataAccess() override;
        void UnlockDataAccess() override;

    private:
        PositionCallback position_callback_;
        std::mutex mutex_;
    };

    /**
     * @brief Handles GPS data acquisition and processing
     */
    class GPSTracker
    {
    public:
        // Constructor
        explicit GPSTracker(int update_interval_ms = DEFAULT_UPDATE_INTERVAL_MS);

        // Destructor to ensure clean shutdown
        ~GPSTracker();

        // Delete copy and move constructors/assignments
        GPSTracker(const GPSTracker &) = delete;
        GPSTracker &operator=(const GPSTracker &) = delete;
        GPSTracker(GPSTracker &&) = delete;
        GPSTracker &operator=(GPSTracker &&) = delete;

        // Control methods
        void start();
        void stop();
        bool isRunning() const { return is_running_; }

        // Configuration
        void setUpdateInterval(int milliseconds);
        int getUpdateInterval() const { return update_interval_ms_; }

        // Callback registration
        void registerPositionCallback(PositionCallback callback);

        // For testing/simulation
        void simulatePosition(double latitude, double longitude, double altitude = 0.0);

        // Process NMEA data
        bool processNMEAData(const std::string &data);

    private:
        int update_interval_ms_;
        std::atomic<bool> is_running_;
        std::thread worker_thread_;
        PositionCallback position_callback_;

        // NMEA parser instance
        std::unique_ptr<EquipmentNMEAParser> nmea_parser_;

        // Serial port handling
        std::string serial_port_;
        [[maybe_unused]] int serial_baud_rate_{9600};
        bool is_port_open_{false};

        // Private methods
        void workerThreadFunction();
        bool openSerialPort(const std::string &port_name);
        void closeSerialPort();
        bool readSerialData(std::string &data);

        // Handle position updates from NMEA parser
        void handlePositionUpdate(double latitude, double longitude, double altitude, Timestamp timestamp);
    };

} // namespace equipment_tracker