# Decisions

One line each: date, decision, reason.

- 2026-11-25 | C++ builds and the Python CLI run inside a Linux Docker image; the host `.venv` is used only for ruff/lockfile | The host has no C++ toolchain (no MSVC/gcc); CLAUDE.md §3 makes the Linux container the default.
- 2026-11-25 | Host tools found: Docker 29.7.2, uv 0.10.3, Node 24.12.0, Terraform 1.17.2, AWS CLI 2.42.1, gh logged in | Recorded per CLAUDE.md §3; CMake/g++ are not on the host and are installed in the build image only.
- 2026-11-25 | Eigen, nlohmann/json, GoogleTest from apt (Debian bookworm / Ubuntu 24.04) with FetchContent fallback; spdlog via FetchContent (bundled fmt, static) | Avoids a shared fmt/spdlog runtime dependency in the slim image.
- 2026-11-25 | Pipeline config `components.vehicle: log_replay` and `controller: log_replay` implement open-loop and the diff replay inside the one engine loop | Keeps a single code path as required by CLAUDE.md §11.
- 2026-11-25 | Trajectory rows are `[x,y,yaw,v,steer,accel]` arrays, state at tick start plus the command issued at that tick | Compact JSON; digest is SHA-256 of that canonical array.
- 2026-11-25 | One S3 `site` bucket holds the viewer and `results/`; a second `data` bucket holds scenarios | CloudFront OAC serves the site bucket only; scenarios stay private.
- 2026-11-25 | viewer devDependencies pin @emnapi/core and @emnapi/runtime 2.0.0-beta.2 | The Windows-generated npm lockfile omitted them (optional peers of the rolldown wasm runtime), which made `npm ci` fail on Linux CI.
- 2026-11-25 | Viewer hosted with S3 static website hosting (public-read site bucket, HTTP) instead of S3 + CloudFront (OAC) | CloudFront creation failed with 403 "account must be verified" (needs AWS Support, not possible here); S3 website is the closest AWS equivalent for a public showcase endpoint.
