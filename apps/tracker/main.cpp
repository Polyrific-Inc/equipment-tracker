#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <iomanip>
#include "equipment_tracker/position.h"
#include "equipment_tracker/equipment.h"
#include "equipment_tracker/utils/time_utils.h"

using namespace equipment_tracker;

void printSeparator(const std::string &title)
{
    std::cout << std::endl
              << "=== " << title << " ===" << std::endl;
}

int main()
{
    std::cout << "Equipment Tracker - Test Application" << std::endl;
    std::cout << "===================================" << std::endl;

    // Create a position using regular constructor
    Position position1(37.7749, -122.4194, 10.0);

    // Create a position using builder pattern with explicit timestamp
    auto now = getCurrentTimestamp();
    Position position2 = Position::builder()
                             .withLatitude(34.0522)
                             .withLongitude(-118.2437)
                             .withAltitude(50.0)
                             .withAccuracy(1.5)
                             .withTimestamp(now)
                             .build();

    // Print positions
    std::cout << "Position 1 (San Francisco): " << position1.toString() << std::endl;
    std::cout << "Position 2 (Los Angeles): " << position2.toString() << std::endl;

    // Calculate distance
    double distance = position1.distanceTo(position2);
    std::cout << "Distance: " << std::fixed << std::setprecision(2)
              << distance / 1000.0 << " km" << std::endl;

    printSeparator("Equipment Creation and Setup");

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

    printSeparator("Movement Simulation");

    // Record multiple positions to test history with more realistic timing
    auto baseTime = getCurrentTimestamp();

    for (int i = 0; i < 5; ++i)
    {
        // Create a position with slight offset
        double latOffset = i * 0.001; // ~111 meters per 0.001 degrees
        double lonOffset = i * 0.002; // ~222 meters per 0.002 degrees

        // Create position with proper timestamp spacing (2 seconds apart)
        Position newPos = Position::builder()
                              .withLatitude(37.7749 + latOffset)
                              .withLongitude(-122.4194 + lonOffset)
                              .withAltitude(10.0 + i)
                              .withTimestamp(baseTime + std::chrono::seconds(i * 2))
                              .build();

        // Record position
        forklift.recordPosition(newPos);

        // Wait a bit to simulate real-time updates
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::cout << "Recorded position " << (i + 1) << ": " << newPos.toString() << std::endl;

        // Show current speed after we have at least 2 positions
        if (i > 0)
        {
            double currentSpeed = forklift.getCurrentSpeed();
            std::cout << "  Current speed: " << std::fixed << std::setprecision(2)
                      << currentSpeed << " m/s (" << currentSpeed * 3.6 << " km/h)" << std::endl;
        }
    }

    printSeparator("Movement Analysis");

    // Check movement status using both methods
    std::cout << "Legacy movement detection: " << (forklift.isMoving() ? "Yes" : "No") << std::endl;
    std::cout << "Enhanced movement detection: " << (forklift.isMovingEnhanced() ? "Yes" : "No") << std::endl;

    // Show detailed movement analysis
    auto analysis = forklift.getMovementAnalysis();
    std::cout << "\nDetailed Movement Analysis:" << std::endl;
    std::cout << "  Status: ";
    switch (analysis.status)
    {
    case MovementStatus::Moving:
        std::cout << "Moving";
        break;
    case MovementStatus::Stationary:
        std::cout << "Stationary";
        break;
    default:
        std::cout << "Unknown";
        break;
    }
    std::cout << std::endl;

    std::cout << "  Current Speed: " << std::fixed << std::setprecision(2)
              << analysis.currentSpeed << " m/s (" << analysis.currentSpeed * 3.6 << " km/h)" << std::endl;
    std::cout << "  Average Speed: " << std::fixed << std::setprecision(2)
              << analysis.averageSpeed << " m/s (" << analysis.averageSpeed * 3.6 << " km/h)" << std::endl;
    std::cout << "  Total Distance: " << std::fixed << std::setprecision(2)
              << analysis.totalDistance << " m" << std::endl;
    std::cout << "  Significant Movement: " << (analysis.hasSignificantMovement ? "Yes" : "No") << std::endl;

    printSeparator("Advanced Movement Tests");

    // Test with custom thresholds
    std::cout << "Movement with 1m threshold: "
              << (forklift.isMovingEnhanced(1.0) ? "Yes" : "No") << std::endl;
    std::cout << "Movement with 5m threshold: "
              << (forklift.isMovingEnhanced(5.0) ? "Yes" : "No") << std::endl;
    std::cout << "Movement with 1 minute time window: "
              << (forklift.isMovingEnhanced(2.0, std::chrono::minutes(1)) ? "Yes" : "No") << std::endl;

    // Check if equipment is stationary
    std::cout << "Is stationary: " << (forklift.isStationary() ? "Yes" : "No") << std::endl;

    // Show total distance traveled
    double totalDistance = forklift.getTotalDistanceTraveled();
    std::cout << "Total distance traveled (5 min window): " << std::fixed << std::setprecision(2)
              << totalDistance << " m" << std::endl;

    printSeparator("Position History");

    // Get position history
    auto history = forklift.getPositionHistory();
    std::cout << "Position history size: " << history.size() << std::endl;

    if (history.size() >= 2)
    {
        std::cout << "First recorded position: " << history.front().toString() << std::endl;
        std::cout << "Last recorded position: " << history.back().toString() << std::endl;

        double totalPathDistance = history.front().distanceTo(history.back());
        std::cout << "Direct distance from start to end: " << std::fixed << std::setprecision(2)
                  << totalPathDistance << " m" << std::endl;
    }

    printSeparator("Stationary Test");

    // Test stationary detection by recording same position multiple times
    std::cout << "Testing stationary detection..." << std::endl;
    Position stationaryPos(37.7749, -122.4194, 10.0);

    for (int i = 0; i < 3; ++i)
    {
        auto timestamp = getCurrentTimestamp() + std::chrono::seconds(i);
        Position samePos = Position::builder()
                               .withLatitude(37.7749)
                               .withLongitude(-122.4194)
                               .withAltitude(10.0)
                               .withTimestamp(timestamp)
                               .build();

        forklift.recordPosition(samePos);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "After recording stationary positions:" << std::endl;
    std::cout << "  Enhanced movement detection: " << (forklift.isMovingEnhanced() ? "Yes" : "No") << std::endl;
    std::cout << "  Is stationary: " << (forklift.isStationary() ? "Yes" : "No") << std::endl;
    std::cout << "  Current speed: " << std::fixed << std::setprecision(2)
              << forklift.getCurrentSpeed() << " m/s" << std::endl;

    std::cout << std::endl
              << "Test completed successfully!" << std::endl;
    return 0;
}