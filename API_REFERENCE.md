# API Reference

## Audited CVAT version

The companion checkout at `/home/haider/Projects/cvat` was audited at commit
`fe6312e` on branch `develop`. The checked-out source defines the schema route
in `cvat/apps/engine/urls.py`:

- `GET /api/schema/` provides the generated OpenAPI schema.
- `/api/swagger/` and `/api/docs/` expose interactive documentation.
- The main router registers `projects`, `tasks`, `jobs`, `users`, `server`,
  `issues`, `comments`, `labels`, `cloudstorages`, `assets` and `guides`.
- IAM and organization routes are included under `/api/`.
- Access-token, events, webhooks, quality-control, consensus and related
  routes are conditionally included by installed Django apps.

These paths are observations from the checked-out source, not a promise that
all deployments enable every optional app. The client should discover the
schema from the configured server and surface unsupported operations clearly.

## Authentication

The native client keeps authentication behind `AuthApi`, `Credentials` and
`ApiClient`. Credentials are never compiled into the application. Login,
logout, current-user lookup, token use, error reporting and session headers
belong in this layer; widgets receive results rather than tokens.

## Native modules

`ProjectsApi`, `TasksApi`, `JobsApi`, `AnnotationsApi`, `LabelsApi`,
`MediaApi`, `UsersApi` and `ServerApi` are present in the source tree. Their
request and response structures must be checked against `/api/schema/` during
each CVAT upgrade, especially for annotation bulk operations, dataset import
and export, upload chunking and permission-dependent fields.

## Verification policy

Do not add endpoint paths or request bodies from memory. For a new operation:

1. Fetch `/api/schema/` from the target server.
2. Confirm method, path parameters, query parameters, body schema and
   pagination envelope.
3. Add a focused API test using a local Qt HTTP test server.
4. Add cancellation, timeout, non-2xx and malformed-JSON coverage where the
   operation is long-running or mutating.