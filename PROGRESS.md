# PROGRESS

Resume rule: read this file first, continue at the first unticked item.

## Phases
- [x] Phase 0: scaffold
- [ ] Phase 1: local build + proof
- [ ] Phase 2: AWS showcase + proof
- [ ] Phase 3: destroy + verify
- [ ] Phase 4: write-up

## Requirement coverage (SPECS §2)
- [x] Modern C++ and Python in a shared codebase: `core/` + `simeval/` via pybind11 (ctest + pytest reports)
- [x] Multiple platforms: KinematicBicycle (car), bicycle + latency (van), DiffDrive (robot): viewer run pages for all 3
- [x] Simulation configs, high-scale open-loop: `configs/*.yaml` + batch of 200 open-loop runs
- [x] On-road vs sim differences across interfaces: F4 divergence tracer, Diff page screenshot
- [x] Testing, observability, reproducibility, tooling: tests, JSON logs, interface traces, repro-check, CLI
- [x] Profiling: F5 profiler (Chrome trace + summary), worker scaling table
- [x] Debugging timing/failure modes: injected sensor dropout and actuator latency visible in traces
- [x] GenAI dev tools with review: `docs/AI_USAGE.md`

## Stack coverage (CLAUDE.md §2)
- [x] C++20, CMake >= 3.28 + CMakePresets.json, Eigen 3.4, nlohmann/json, spdlog JSON pattern
- [x] GoogleTest via ctest
- [x] pybind11 + scikit-build-core wheel `simcore`
- [x] Python 3.12, uv, numpy, pandas, typer, pytest, ruff
- [x] ProcessPoolExecutor local batch; AWS Batch Fargate array job
- [x] Scoped timers -> Chrome Trace JSON + summary table
- [x] JSON logs, per-run metrics JSON, interface trace records
- [x] React 19 + TS + Vite, Recharts, canvas replay, Vitest + RTL, Playwright
- [x] Docker multi-stage (C++ build -> wheel -> slim runtime)
- [ ] AWS: ECR, Batch (Fargate CE, queue, array job def), S3, S3+CloudFront, CloudWatch Logs, IAM
- [x] Terraform >= 1.9 (AWS provider 6.x), GitHub Actions CI
- [x] `docs/AI_USAGE.md`

## Features (SPECS §5)
- [x] F1 scenario generation + single run
- [x] F2 open-loop batch + bench-scaling
- [x] F3 repro-check
- [x] F4 divergence tracing (+ `--inject sensor_dropout@tick=40`)
- [x] F5 profiling
- [x] F6 platforms car|van|robot
- [x] F7 viewer pages (/, batch, run, diff, profile, repro)
- [ ] F8 cloud fan-out

## Phase 1 proof (`proof/phase1-local/`)
- [x] tests/: ctest JUnit XML
- [x] tests/: pytest JUnit XML
- [x] tests/: Vitest report
- [x] tests/: Playwright HTML report
- [x] logs/batch-local-001.log (JSON lines incl. throughput)
- [x] logs/scaling.json
- [x] screenshots/01-batches.png
- [x] screenshots/02-batch-overview.png
- [x] screenshots/03-run-replay-car.png
- [x] screenshots/04-run-replay-robot.png
- [x] screenshots/05-diff-divergence.png
- [x] screenshots/06-profile.png
- [x] screenshots/07-repro-green.png
- [x] logs/perfetto-trace.json
- [x] `scripts/test-all.sh` green
- [ ] CI green on `main`
- [x] proof/README.md index complete

## Phase 2 proof (`proof/phase2-cloud/`)
- [ ] logs/batch-describe-jobs.json (array job SUCCEEDED, 20/20 children, redacted)
- [ ] logs/cloudwatch-sample.log
- [x] screenshots/01-cloud-batch-overview.png
- [x] screenshots/02-cloud-run-replay.png
- [x] screenshots/03-cloud-diff.png
- [ ] tests/playwright-cloud/
- [ ] resources.txt

## Phase 3 (`proof/phase3-destroy/`)
- [ ] `scripts/destroy.sh` run
- [ ] `verify-destroyed.txt` shows nothing billable

## Phase 4
- [ ] `docs/PORTFOLIO.md`
- [ ] `README.md` current (diagram, quickstart, deploy/destroy, badge, links)
- [ ] everything committed and pushed

## Notes
