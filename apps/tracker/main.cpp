#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <string>
#include "equipment_tracker/position.h"
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/utils/time_utils.h"

using namespace equipment_tracker;

void printSeparator()
{
    std::cout << "---------------------------------------------------" << std::endl;
}

void printUsage(const char *programName)
{
    std::cout << "Usage: " << programName << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --pos1 lat,lon,alt     Set position 1 (default: 37.7749,-122.4194,10.0)" << std::endl;
    std::cout << "  --pos2 lat,lon,alt     Set position 2 (default: 34.0522,-118.2437,50.0)" << std::endl;
    std::cout << "  --help                 Show this help message" << std::endl;
}

bool parsePosition(const std::string &str, double &lat, double &lon, double &alt)
{
    size_t first_comma = str.find(',');
    size_t second_comma = str.find(',', first_comma + 1);

    if (first_comma == std::string::npos || second_comma == std::string::npos)
    {
        return false;
    }

    try
    {
        lat = std::stod(str.substr(0, first_comma));
        lon = std::stod(str.substr(first_comma + 1, second_comma - first_comma - 1));
        alt = std::stod(str.substr(second_comma + 1));
        return true;
    }
    catch (const std::exception &e)
    {
        return false;
    }
}

int main(int argc, char *argv[])
{
    // Default positions
    double pos1_lat = 37.7749, pos1_lon = -122.4194, pos1_alt = 10.0;
    double pos2_lat = 34.0522, pos2_lon = -118.2437, pos2_alt = 50.0;

    // Parse command-line arguments
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];

        if (arg == "--help")
        {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "--pos1" && i + 1 < argc)
        {
            if (!parsePosition(argv[i + 1], pos1_lat, pos1_lon, pos1_alt))
            {
                std::cerr << "Error: Invalid position format for --pos1" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
            i++;
        }
        else if (arg == "--pos2" && i + 1 < argc)
        {
            if (!parsePosition(argv[i + 1], pos2_lat, pos2_lon, pos2_alt))
            {
                std::cerr << "Error: Invalid position format for --pos2" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
            i++;
        }
        else
        {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "Equipment Tracker - Test Application" << std::endl;
    std::cout << "===================================" << std::endl;

    // Create a position using regular constructor
    Position position1(pos1_lat, pos1_lon, pos1_alt);

    // Create a position using builder pattern
    Position position2 = Position::builder()
                             .withLatitude(pos2_lat)
                             .withLongitude(pos2_lon)
                             .withAltitude(pos2_alt)
                             .withAccuracy(1.5)
                             .build();

    // Print positions
    printSeparator();
    std::cout << "Position 1 (San Francisco): " << position1.toString() << std::endl;
    std::cout << "Position 2 (Los Angeles): " << position2.toString() << std::endl;

    // Calculate distance
    double distance = position1.distanceTo(position2);
    std::cout << "Distance: " << distance / 1000.0 << " km" << std::endl;
    printSeparator();

    // Create equipment
    Equipment forklift("FORKLIFT-001", EquipmentType::Forklift, "Warehouse Forklift 1");

    // Set position
    forklift.setLastPosition(position1);

    // Print equipment information
    std::cout << "Equipment: " << forklift.toString() << std::endl;

    // Check if equipment has position
    auto lastPosition = forklift.getLastPosition();
    if (lastPosition)
    {
        std::cout << "Last position: " << lastPosition->toString() << std::endl;
    }
    else
    {
        std::cout << "No position recorded yet." << std::endl;
    }
    printSeparator();

    // Test movement detection
    std::cout << "Testing movement detection:" << std::endl;
    std::cout << "Initial movement status: " << (forklift.isMoving() ? "Moving" : "Stationary") << std::endl;

    // Record multiple positions to test history and movement detection
    for (int i = 0; i < 5; ++i)
    {
        // Create a position with slight offset
        double latOffset = i * 0.001;
        double lonOffset = i * 0.002;
        Position newPos(
            37.7749 + latOffset,
            -122.4194 + lonOffset,
            10.0 + i);

        // Record position
        forklift.recordPosition(newPos);

        // Wait a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::cout << "Recorded position " << (i + 1) << ": " << newPos.toString() << std::endl;

        // Check movement status after each position
        if (i > 0)
        {
            std::cout << "Movement status: " << (forklift.isMoving() ? "Moving" : "Stationary") << std::endl;
        }
    }
    printSeparator();

    // Get position history
    auto history = forklift.getPositionHistory();
    std::cout << "Position history size: " << history.size() << std::endl;

    // Calculate total distance traveled
    double totalDistance = 0.0;
    for (size_t i = 1; i < history.size(); i++)
    {
        totalDistance += history[i - 1].distanceTo(history[i]);
    }
    std::cout << "Total distance traveled: " << totalDistance << " meters" << std::endl;
    printSeparator();

    return 0;
}