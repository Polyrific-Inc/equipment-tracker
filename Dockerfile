FROM alpine:latest

# Install build tools
RUN apk add --no-cache \
    clang \
    clang-dev \
    cmake \
    make \
    ninja \
    libc-dev

# Set the working directory
WORKDIR /app

# Copy the entire project
COPY . .

# Make sure we're not using any existing CMake cache
RUN rm -rf build && mkdir -p build

# Configure with CMake and build
RUN cd build && \
    cmake .. \
    -G "Ninja" \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Release \
    && \
    cmake --build .

# Set the entrypoint to run the application
ENTRYPOINT ["/app/build/equipment_tracker_app"]