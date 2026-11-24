# CVAT Native

CVAT Native is a Qt 6 Widgets client for an existing CVAT server. It is a
native desktop application for Linux and Windows, not a replacement CVAT
backend and not an embedded browser. The current implementation provides a modular API client, local
SQLite storage, recovery snapshots, bounded frame caching, annotation models,
and an annotation-focused dashboard and workspace.

The dashboard opens on a **Home** landing page with a local history of
recently opened projects, tasks and jobs. The workspace supports rectangle
annotation editing plus **multi-label area copy/paste**: enable *Multi-label*
in the job footer, drag a rectangle fully around several labels, press **C**,
then **V** to reproduce them with their original classes, attributes and
relative positions.

The native UI is intentionally non-administrative: projects, tasks, and jobs
are fetched and browsed read-only; users open existing jobs, select existing
CVAT label classes, draw rectangle annotations, edit or delete selected
annotations, and save changes through CVAT's annotation API. Project/task/job/
label creation, mutation, deletion, and task upload controls are not part of the
application build. An export dialog source file contains annotation and dataset
export flows, but it is excluded from the CMake target and has no UI entry point;
dataset export is therefore not reachable in the application.

## Current status

The project builds as C++20 with Qt 6.12 and CMake 4.5 on the target Arch
Linux workstation (CMake 4.4+ works). A Windows build recipe using MSYS2 UCRT64
is in `BUILD.md`. The source tree contains API modules for
authentication, users, projects, tasks, jobs, annotations, labels, media and
server information, plus the native annotation canvas, settings UI and the
recent-activity landing page.

The current test suite contains 34 CTest-discovered cases covering result
handling, API request behavior, annotation serialization, geometry, local
storage and frame caching. Network-facing tests require the local test server
to finish before their result can be assessed; see `BUILD.md`.

## Installation

### Linux

Install a C++20 compiler, CMake, Ninja, Qt 6 development packages, SQLite and
GoogleTest (GoogleTest is optional if you build without tests):

```sh
# Arch Linux
sudo pacman -S --needed base-devel cmake ninja qt6-base gtest sqlite desktop-file-utils

# Debian / Ubuntu
sudo apt install build-essential cmake ninja-build qt6-base-dev libgtest-dev libsqlite3-dev desktop-file-utils

# Fedora
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel gtest sqlite-devel desktop-file-utils
```

Qt 6.5 or newer with the Core, Gui, Widgets, Network, Sql and Concurrent
modules is required. Then clone, build, test and install into your user account:

```sh
git clone https://github.com/haider-9/cvat-native.git
cd cvat-native
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
  -DCVATNATIVE_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
cmake --install build
```

Make sure `~/.local/bin` is on your `PATH`; add this to your shell profile if
it is not:

```sh
export PATH="$HOME/.local/bin:$PATH"
```

Refresh desktop launcher caches, then launch **CVAT Native** from your app
menu or run `~/.local/bin/cvat-native` directly:

```sh
update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
kbuildsycoca6 2>/dev/null || true
```

The installed `.desktop` entry is generated with the configured install
prefix, so its `Exec=` line points at the binary you just installed. For a
system-wide install, configure with `-DCMAKE_INSTALL_PREFIX=/usr/local` and run
`sudo cmake --install build`; the launcher, icon and binary are then installed
under `/usr/local`.

If GoogleTest is not available, configure with
`-DCVATNATIVE_BUILD_TESTS=OFF` to build the application without the test suite.

### Windows

Use the MSYS2 **UCRT64** terminal. Do not mix packages from MSYS, MINGW64,
CLANG64 or a separate Qt installation.

Install MSYS2 from [msys2.org](https://www.msys2.org/) (default
`C:\msys64`), open **MSYS2 UCRT64**, and update it:

```bash
pacman -Syu
```

If the terminal asks to close, reopen **MSYS2 UCRT64** and repeat the update
until it completes. Then install the toolchain and dependencies:

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-qt6-base \
  mingw-w64-ucrt-x86_64-gtest \
  mingw-w64-ucrt-x86_64-pkgconf
```

Configure, build and test from the repository checkout:

```bash
git clone https://github.com/haider-9/cvat-native.git
cd cvat-native
cmake -S . -B build-windows -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCVATNATIVE_BUILD_TESTS=ON
cmake --build build-windows
ctest --test-dir build-windows --output-on-failure
```

Run the app from the same UCRT64 shell:

```bash
./build-windows/cvat-native.exe
```

To copy the executable to a prefix of your choosing:

```bash
cmake --install build-windows --prefix "$HOME/.local"
```

On Windows the install rule places `cvat-native.exe` at the root of the
prefix. It does **not** create a self-contained installer. For a portable
distribution, bundle Qt's Windows platform and SQL plugins together with the
required Qt and MinGW runtime DLLs (for example with `windeployqt`) and test
the result on a clean machine. Saved CVAT tokens use Windows Credential
Manager; if it is unavailable, the application warns that its fallback token
file is unencrypted.

Use a distinct `build-windows` directory; CMake build directories are tied to
their compiler and platform and cannot be shared with Linux builds. The
sanitizer option is not supported by MSVC and is left disabled in the
documented MinGW-w64 build.

## Documents

- `ARCHITECTURE.md` describes the module boundaries and data flow.
- `API_REFERENCE.md` records the verified CVAT contract and native mapping.
- `CVAT_BRIDGE_INVENTORY.md` classifies every reusable bridge component.
- `BUILD.md` contains Arch Linux and Windows setup and validation commands.
- `ROADMAP.md` separates implemented work from the next buildable milestones.

## Scope

The client connects to a CVAT deployment configured by the user. It does not
start or modify CVAT services. The checked-out CVAT repository used during the
initial audit is documented in `API_REFERENCE.md`; endpoint behavior must be
rechecked against the server's generated schema when upgrading CVAT.
