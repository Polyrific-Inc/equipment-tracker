# Equipment Tracker

**THIS IS A DUMMY C++ APPLICATION TO BE USED FOR THE POC OF CODE REVIEW IMPLEMENTATION**

A modern C++ application for real-time tracking of heavy equipment such as forklifts, cranes, and bulldozers with GPS technology. This application demonstrates professional C++ coding standards and practices in a real-world use case.

## Overview

Equipment Tracker is designed to help companies monitor and manage their fleet of heavy equipment efficiently. It provides real-time position tracking, equipment status monitoring, and historical data for analytics.

Key features include:

- Real-time GPS tracking of equipment
- Position history recording and analysis
- Equipment status monitoring
- Geofencing capabilities
- Visualization of equipment locations
- Network connectivity for centralized tracking

## Prerequisites

- C++17 compatible compiler (GCC 8+, Clang 6+, MSVC 2019+)
- CMake 3.14 or higher
- Visual Studio 2019/2022 (for Windows) or other IDE/editor of choice

## Dependencies

- NMEAParser library (included as Git submodule) for GPS data parsing
- GoogleTest (automatically downloaded by CMake) for unit tests

## Building the Project

### Clone the Repository

```bash
git clone https://github.com/your-username/equipment-tracker.git
cd equipment-tracker
git submodule update --init --recursive
```

### Building with CMake

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Building with Visual Studio

1. Open the project folder in Visual Studio with "Open Folder"
2. Visual Studio should detect the CMake configuration
3. Select "Build All" from the Build menu

## Running the Application

After successful build:

```bash
# Run the main application
./bin/equipment_tracker_app

# Run the examples
./bin/examples/basic_tracking
./bin/examples/geofencing
./bin/examples/realtime_monitoring
```

## Running the Tests

```bash
cd build
ctest
```

Or run individual tests:

```bash
./bin/test_position
./bin/test_equipment
```

## License

This project is licensed under the MIT License - see the LICENSE file for details.
