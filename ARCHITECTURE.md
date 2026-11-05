# Architecture

## Layers

`cvat_core` is UI-independent and contains:

- `api`: `ApiClient` owns URL resolution, headers, JSON requests, errors,
  cancellation and request lifecycle. Resource APIs keep endpoint knowledge
  out of widgets.
- `model` and `annotation`: CVAT-compatible annotation values, geometry,
  coordinate transforms and undoable editing state.
- `media`: asynchronous media retrieval and a bounded frame cache.
- `storage`: SQLite-backed local state and crash-recovery snapshots.
- `core`: settings, credentials, result types and logging.

`cvat_ui` contains the Qt Widgets dashboard, login/settings dialogs, entity
dialogs, label panel, export flow, job workspace and `QGraphicsView` canvas.
Widgets call resource APIs and services; they do not construct raw HTTP
requests.

The executable in `src/app/main.cpp` composes the two libraries and owns the
application lifetime.

## Data flow

1. Authentication establishes a server session or token in the API client.
2. Resource APIs request paginated CVAT entities and convert JSON to native
   models.
3. MediaService loads only the active frame and nearby cache entries.
4. AnnotationModel stores editable geometry in image coordinates, independent
   of zoom and viewport scale.
5. RecoveryStore writes local snapshots before synchronization. Remote writes
   remain explicit and must not silently overwrite newer server data.

## Performance constraints

The target is an Intel i5-8350U with integrated graphics and 16 GB RAM. The
design therefore favors bounded caches, asynchronous decoding, Qt implicit
sharing, incremental requests and CPU rendering through `QGraphicsView`.
OpenCV is not currently required by the core build; it should be introduced
only for a measured computer-vision feature that cannot use Qt image APIs.

## Backend boundary

CVAT remains the source of truth for authentication, permissions, projects,
tasks, jobs, labels, annotation persistence and dataset import/export. This
client must not duplicate Django business rules. API details are versioned in
`API_REFERENCE.md` and verified against CVAT's generated schema.