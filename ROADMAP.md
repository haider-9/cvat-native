# Roadmap

## Completed foundation

- C++20/CMake target split into `cvat_core`, `cvat_ui` and the application.
- Qt Network API client with resource-specific modules.
- Authentication, settings, credentials and structured results.
- Annotation model, geometry transforms, local SQLite storage and recovery.
- Bounded frame cache and media service.
- Initial dashboard, login/settings dialogs and annotation workspace.
- Annotation-only UI scope: read-only resource browsing, rectangle creation,
  existing label classes, and annotation save API.
- Focused tests for API behavior, annotations, geometry, storage and caching.
- CVAT backend and `cvat-bridge` audit documented.

## Remaining work, in order

1. Run the full test suite with deterministic local HTTP fixtures and remove
   any network-test timing ambiguity.
2. Validate every API module against the generated schema from CVAT `fe6312e`,
   including pagination, permissions, token authentication and server errors.
3. Complete task media upload/download, chunking, video frame retrieval and
   job status polling with cancellation and progress reporting.
4. Complete CVAT annotation shapes and attributes, bulk save/merge semantics,
   conflict detection and recovery replay.
5. Add native selection commands, resize handles, keyboard shortcuts,
   undo/redo integration and bridge-inspired copy/paste/move/relabel/grow
   behavior.
6. Implement dataset import/export adapters driven by the server-supported
   formats rather than duplicating CVAT conversion logic.
7. Add integration fixtures against a disposable CVAT deployment, then
   benchmark frame loading, annotation editing, synchronization and idle RAM
   on the Dell Latitude 5400 target.
8. Package for Arch Linux and document operational logging and credential
   storage hardening.

The next concrete implementation milestone is item 1, followed by schema
conformance tests for `AuthApi`, `ProjectsApi` and `TasksApi`.