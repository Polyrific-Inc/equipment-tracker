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

- [Clang](https://releases.llvm.org/download.html)
- [CMake](https://cmake.org/download/)
- [Ninja](https://github.com/ninja-build/ninja/releases)

## Cross-Platform Support

Equipment Tracker is designed to be fully cross-platform compatible, supporting:

- Windows with Clang
- Linux (including Alpine-based containers)
- No Visual Studio dependency required

## Building the Project

### Clone the Repository

```bash
git clone https://github.com/your-username/equipment-tracker.git
cd equipment-tracker
```

### Quick Build (Windows)

The easiest way to build on Windows is using the provided build script:

```bash
./build.bat
```

### Manual Build with CMake

```bash
mkdir build
cd build
cmake .. -G "Ninja" -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build .
```

### Running with Docker

The project includes Docker support for building and running in a container:

```bash
# Build and run with Docker Compose
docker compose up

# Or manually build and run
docker build -t equipment-tracker .
docker run equipment-tracker
```

## Running the Application

After successful build:

```bash
# Run the main application
./build/equipment_tracker_app

# Alternative location depending on build configuration
./build/bin/equipment_tracker_app
```

## Development Notes

- The codebase uses platform-independent constructs wherever possible
- Platform-specific code is isolated using preprocessor macros
- All code follows modern C++17 practices

## License

This project is licensed under the MIT License - see the LICENSE file for details.
