#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include "equipment_tracker/position.h"
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/utils/time_utils.h"

using namespace equipment_tracker;

int main()
{
    std::cout << "Equipment Tracker - Test Application" << std::endl;
    std::cout << "===================================" << std::endl;

    // Create a position using regular constructor
    Position position1(37.7749, -122.4194, 10.0);

    // Create a position using builder pattern
    Position position2 = Position::builder()
                             .withLatitude(34.0522)
                             .withLongitude(-118.2437)
                             .withAltitude(50.0)
                             .withAccuracy(1.5)
                             .build();

    // Print positions
    std::cout << "Position 1 (San Francisco): " << position1.toString() << std::endl;
    std::cout << "Position 2 (Los Angeles): " << position2.toString() << std::endl;

    // Calculate distance
    double distance = position1.distanceTo(position2);
    std::cout << "Distance: " << distance / 1000.0 << " km" << std::endl;

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

    // Record multiple positions to test history
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
    }

    // Check movement status
    std::cout << "Is equipment moving? " << (forklift.isMoving() ? "Yes" : "No") << std::endl;

    // Get position history
    auto history = forklift.getPositionHistory();
    std::cout << "Position history size: " << history.size() << std::endl;

    return 0;
}