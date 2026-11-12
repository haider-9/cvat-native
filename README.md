# CVAT Native

CVAT Native is a Qt 6 Widgets client for an existing CVAT server. It is a
native desktop application for Linux and Windows, not a replacement CVAT
backend and not an embedded browser. The current implementation provides a modular API client, local
SQLite storage, recovery snapshots, bounded frame caching, annotation models,
and an annotation-focused dashboard and workspace.

The native UI is intentionally non-administrative: projects, tasks, and jobs
are fetched and browsed read-only; users open existing jobs, select existing
CVAT label classes, draw rectangle annotations, edit or delete selected
annotations, and save changes through CVAT's annotation API. Project/task/job/
label creation, mutation, deletion, and task upload controls are not part of the
application build. An export dialog source file contains annotation and dataset
export flows, but it is excluded from the CMake target and has no UI entry point;
dataset export is therefore not reachable in the application.

## Current status

The project builds as C++20 with Qt 6.12 and CMake 4.4 on the target Arch
Linux workstation. A Windows build recipe using MSYS2 UCRT64 is in `BUILD.md`.
The source tree contains API modules for
authentication, users, projects, tasks, jobs, annotations, labels, media and
server information, plus the first native annotation canvas and settings UI.

The current test suite contains 34 CTest-discovered cases covering result
handling, API request behavior, annotation serialization, geometry, local
storage and frame caching. Network-facing tests require the local test server
to finish before their result can be assessed; see `BUILD.md`.

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
rechecked against the server's generated schema when upgrading CVAT.# cvat-native
