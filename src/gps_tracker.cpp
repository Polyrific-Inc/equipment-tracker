#include <iostream>
#include <chrono>
#include <cstring>
#include <sstream>
#include "equipment_tracker/gps_tracker.h"
#include "equipment_tracker/position.h"

namespace equipment_tracker
{

    // ====== EquipmentNMEAParser Implementation ======

    void EquipmentNMEAParser::OnError(CNMEAParserData::ERROR_E nError, char *pCmd)
    {
        std::cerr << "NMEA Parser Error: " << nError;
        if (pCmd)
        {
            std::cerr << " for command: " << pCmd;
        }
        std::cerr << std::endl;
    }

    void EquipmentNMEAParser::LockDataAccess()
    {
        mutex_.lock();
    }

    void EquipmentNMEAParser::UnlockDataAccess()
    {
        mutex_.unlock();
    }

    // ====== GPSTracker Implementation ======

    GPSTracker::GPSTracker(int update_interval_ms)
        : update_interval_ms_(update_interval_ms),
          is_running_(false),
          nmea_parser_(std::make_unique<EquipmentNMEAParser>())
    {
    }

    GPSTracker::~GPSTracker()
    {
        stop();
    }

    void GPSTracker::start()
    {
        if (is_running_)
            return;

        is_running_ = true;
        worker_thread_ = std::thread(&GPSTracker::workerThreadFunction, this);
    }

    void GPSTracker::stop()
    {
        if (!is_running_)
            return;

        is_running_ = false;

        if (worker_thread_.joinable())
        {
            worker_thread_.join();
        }

        closeSerialPort();
    }

    void GPSTracker::setUpdateInterval(int milliseconds)
    {
        update_interval_ms_ = milliseconds;
    }

    void GPSTracker::registerPositionCallback(PositionCallback callback)
    {
        position_callback_ = std::move(callback);

        // Also register with NMEA parser for GPS sentence updates
        if (nmea_parser_)
        {
            nmea_parser_->setPositionCallback([this](double lat, double lon, double alt)
                                              { this->handlePositionUpdate(lat, lon, alt); });
        }
    }

    void GPSTracker::simulatePosition(double latitude, double longitude, double altitude)
    {
        // Create an NMEA GGA sentence with the provided coordinates
        std::stringstream ss;

        // Converting decimal degrees to NMEA format (DDMM.MMMM)
        double lat_deg = static_cast<int>(std::abs(latitude));
        double lat_min = (std::abs(latitude) - lat_deg) * 60.0;
        char lat_dir = (latitude >= 0.0) ? 'N' : 'S';

        double lon_deg = static_cast<int>(std::abs(longitude));
        double lon_min = (std::abs(longitude) - lon_deg) * 60.0;
        char lon_dir = (longitude >= 0.0) ? 'E' : 'W';

        // Format: $GPGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,x.x,xxxx*hh
        // Where:
        // - hhmmss.ss is UTC time
        // - llll.ll is latitude in degrees and minutes
        // - a is N or S
        // - yyyyy.yy is longitude in degrees and minutes
        // - a is E or W
        // - x is GPS quality indicator
        // - xx is number of satellites
        // - x.x is horizontal dilution of precision
        // - x.x is altitude
        // - M is units (meters)
        // - x.x is height of geoid
        // - M is units (meters)
        // - x.x is time since last DGPS update
        // - xxxx is DGPS reference station ID

        // Get current time
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::gmtime(&time_t);

        ss << "$GPGGA,"
           << std::setfill('0') << std::setw(2) << tm.tm_hour
           << std::setfill('0') << std::setw(2) << tm.tm_min
           << std::setfill('0') << std::setw(2) << tm.tm_sec
           << ".00,"
           << std::setfill('0') << std::setw(2) << lat_deg
           << std::fixed << std::setprecision(4) << lat_min << ","
           << lat_dir << ","
           << std::setfill('0') << std::setw(3) << lon_deg
           << std::fixed << std::setprecision(4) << lon_min << ","
           << lon_dir << ","
           << "1,08,0.9," // GPS quality, satellites, HDOP
           << std::fixed << std::setprecision(1) << altitude << ","
           << "M,0.0,M,," // Altitude units, geoid separation, units, empty fields
           << "*";

        // Calculate checksum (XOR of all characters between $ and *)
        std::string data = ss.str();
        unsigned char checksum = 0;
        for (size_t i = 1; i < data.length() - 1; i++)
        {
            checksum ^= data[i];
        }

        // Add checksum and line ending
        ss.str(std::string());
        ss << data << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
           << static_cast<int>(checksum) << "\r\n";

        // Process the NMEA data
        processNMEAData(ss.str());

        // If there's no NMEA parser or it fails, directly call the position callback
        if (position_callback_)
        {
            position_callback_(latitude, longitude, altitude, getCurrentTimestamp());
        }
    }

    bool GPSTracker::processNMEAData(const std::string &data)
    {
        if (!nmea_parser_)
        {
            return false;
        }

        // Pass data to NMEA parser
        auto result = nmea_parser_->ProcessNMEABuffer(
            const_cast<char *>(data.c_str()),
            static_cast<int>(data.length()));

        return (result == CNMEAParserData::ERROR_OK);
    }

    void GPSTracker::workerThreadFunction()
    {
        // Try to open the serial port if provided
        if (!serial_port_.empty() && !openSerialPort(serial_port_))
        {
            std::cerr << "Failed to open serial port: " << serial_port_ << std::endl;
        }

        std::string buffer;

        while (is_running_)
        {
            if (is_port_open_)
            {
                // Read data from serial port
                if (readSerialData(buffer) && !buffer.empty())
                {
                    // Process NMEA data
                    processNMEAData(buffer);
                    buffer.clear();
                }
            }
            else
            {
                // No real device, simulate data
                simulatePosition(37.7749, -122.4194, 10.0); // Default: San Francisco
            }

            // Sleep for update interval
            std::this_thread::sleep_for(std::chrono::milliseconds(update_interval_ms_));
        }
    }

    bool GPSTracker::openSerialPort(const std::string &port_name)
    {
        // In a real implementation, would open a serial port
        // For now, just store the port name
        serial_port_ = port_name;
        is_port_open_ = false; // Would be true if actually opened

        return is_port_open_;
    }

    void GPSTracker::closeSerialPort()
    {
        // In a real implementation, would close the serial port
        is_port_open_ = false;
    }

    bool GPSTracker::readSerialData(std::string &data)
    {
        // In a real implementation, would read from the serial port
        // For now, return empty data
        data.clear();
        return true;
    }

    void GPSTracker::handlePositionUpdate(double latitude, double longitude, double altitude)
    {
        // Extract GGA information from NMEA parser
        CNMEAParserData::GGA_DATA_T ggaData;
        if (nmea_parser_->GetGPGGA(ggaData) == CNMEAParserData::ERROR_OK)
        {
            // Convert from NMEA format to decimal degrees
            double lat = ggaData.dLatitude;
            double lon = ggaData.dLongitude;
            double alt = ggaData.dAltitudeMSL;

            // Call position callback
            if (position_callback_)
            {
                position_callback_(lat, lon, alt, getCurrentTimestamp());
            }
        }
    }

} // namespace equipment_tracker