# Build

## Arch Linux dependencies

Install the native toolchain and Qt modules with pacman:

```sh
sudo pacman -S --needed base-devel cmake ninja qt6-base gtest sqlite
```

The current workstation was verified with GCC 16.2, CMake 4.4 and Qt 6.12.
OpenCV is optional and is not required by the current target.

## Configure and build

From this directory:

```sh
cmake -S . -B build -G Ninja -DCVATNATIVE_BUILD_TESTS=ON
cmake --build build
```

Run the test suite:

```sh
ctest --test-dir build --output-on-failure
```

Enable sanitizers for core tests when diagnosing memory or undefined behavior:

```sh
cmake -S . -B build-asan -G Ninja \
  -DCVATNATIVE_BUILD_TESTS=ON -DCVATNATIVE_ENABLE_ASAN=ON
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

## Backend

Run or connect to a CVAT server separately. Configure its base URL through the
native settings UI or `Settings`; never put credentials in source or build
files. The companion CVAT checkout uses Docker Compose and includes PostgreSQL,
Redis/KVRocks, ClickHouse, the server, UI and workers. The native client only
needs the reachable HTTP API for its current feature set.

## Troubleshooting

If CMake cannot find Qt, verify `qmake6 -v` and the `qt6-base` installation.
If GTest is unavailable, configure with `-DCVATNATIVE_BUILD_TESTS=OFF` only to
build the application; install GTest before claiming the test suite passes.