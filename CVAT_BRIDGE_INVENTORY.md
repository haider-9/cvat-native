# CVAT Bridge Inventory

The additional repository `/home/haider/Documents/cvat-bridge` was inspected
without modifying it. It is an Astro/TypeScript browser selection editor, not
a CVAT API client or annotation backend.

| Component | Classification | Integration decision |
| --- | --- | --- |
| `src/components/SelectionEditor.astro` | Useful reference | Documents the browser editor's selection workflow and controls. Reimplement the interaction in Qt rather than embedding Astro. |
| `src/scripts/copy.js` | Adaptable reference | Copy selected annotation data; translate the operation to native `AnnotationModel` clipboard commands. |
| `src/scripts/paste.js` | Adaptable reference | Paste and restore selections; use CVAT-compatible geometry and label validation in the native model. |
| `src/scripts/delete.js` | Adaptable reference | Maps directly to a native delete command with undo support. |
| `src/scripts/grow.js` | Adaptable reference | Candidate for a geometry helper after its coordinate assumptions are covered by tests. |
| `src/scripts/move.js` | Adaptable reference | Candidate for keyboard movement commands in image coordinates. |
| `src/scripts/relabel.js` | Adaptable reference | Candidate for a label reassignment command using the native label cache. |
| `src/styles/global.css` | Reference only | Browser presentation rules do not belong in the Qt application. |
| `src/pages/index.astro` and `src/layouts/Layout.astro` | Reference only | Page composition only; no native dependency. |
| `public/*`, Astro config, TypeScript config and npm metadata | Independent | Build assets and configuration for the bridge site; preserve as a standalone utility. |
| `src/scripts/copy.js`, `paste.js`, `delete.js`, `grow.js`, `move.js`, `relabel.js` | Adaptable reference | These are the only operation scripts found; they contain no reusable C++ library. |

The bridge has no server credentials, CVAT REST implementation, annotation
serializer, computer-vision algorithm or native library to copy. Its useful
future role is as a behavioral reference for selection editing and as a
possible export/import helper specification. The original repository remains
independent and untouched.