# Build

## Arch Linux dependencies

Install the native toolchain and Qt modules with pacman:

```sh
sudo pacman -S --needed base-devel cmake ninja qt6-base gtest sqlite
```

The current workstation was verified with GCC 16.2, CMake 4.4 and Qt 6.12.
OpenCV is optional and is not required by the current target.

## Windows dependencies (MSYS2 UCRT64)

The Windows build uses MSYS2's 64-bit UCRT64 MinGW-w64 toolchain. Install
MSYS2 from [msys2.org](https://www.msys2.org/) using its default location,
`C:\msys64`, then open **MSYS2 UCRT64** from the Start menu. Do not mix packages
from MSYS, MINGW64, CLANG64, or a separate Qt installation in this build.

Update MSYS2:

```bash
pacman -Syu
```

If prompted to close the terminal, reopen **MSYS2 UCRT64** and repeat the
update until it completes. Install the compiler, Qt, CMake, Ninja, GoogleTest,
and pkgconf:

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-qt6-base \
  mingw-w64-ucrt-x86_64-gtest \
  mingw-w64-ucrt-x86_64-pkgconf
```

Qt 6.5 or newer is required. The UCRT64 shell puts the matching compiler and
Qt tools on `PATH`, which is also needed when CMake discovers and runs the
test executable.

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

## Windows configure, build, and test

Run these commands from **MSYS2 UCRT64**, in the repository directory. For
example, a checkout under your Windows profile is at
`/c/Users/<username>/Projects/cvat-native`:

```bash
cd /c/Users/<username>/Projects/cvat-native
cmake -S . -B build-windows -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCVATNATIVE_BUILD_TESTS=ON
cmake --build build-windows
ctest --test-dir build-windows --output-on-failure
```

Use a distinct `build-windows` directory; CMake build directories are tied to
their compiler and platform and cannot be shared with Linux builds. To build
the application without GoogleTest, use
`-DCVATNATIVE_BUILD_TESTS=OFF` during configuration.

Launch the native Windows application from the same UCRT64 shell:

```bash
./build-windows/cvat-native.exe
```

The shell provides Qt and MinGW runtime DLL locations. Windows Credential
Manager is used for saved CVAT tokens. If it is unavailable, the application
reports that its fallback token file is unencrypted; protect that file or do
not save a token. The CMake install target installs the executable only and
does not create a self-contained Windows installer. A portable distribution
must also bundle Qt's Windows platform and SQL plugins and all required Qt and
MinGW runtime DLLs, then be tested on a clean Windows machine.

The AddressSanitizer/UndefinedBehaviorSanitizer option is not supported by the
MSVC compiler. The documented MSYS2 MinGW-w64 build leaves it disabled.

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

If CMake cannot find Qt on Arch Linux, verify `qmake6 -v` and the `qt6-base` installation.
In MSYS2, run CMake and the build from the **MSYS2 UCRT64** terminal and check
that `which cmake`, `which qmake6`, and `which g++` resolve under `/ucrt64/bin`.
If GTest is unavailable, configure with `-DCVATNATIVE_BUILD_TESTS=OFF` only to
build the application; install GTest before claiming the test suite passes.