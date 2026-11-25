# PROGRESS

Resume rule: read this file first, continue at the first unticked item.

## Phases
- [x] Phase 0: scaffold
- [ ] Phase 1: local build + proof
- [ ] Phase 2: AWS showcase + proof
- [ ] Phase 3: destroy + verify
- [ ] Phase 4: write-up

## Requirement coverage (SPECS §2)
- [ ] Modern C++ and Python in a shared codebase: `core/` + `simeval/` via pybind11 (ctest + pytest reports)
- [ ] Multiple platforms: KinematicBicycle (car), bicycle + latency (van), DiffDrive (robot): viewer run pages for all 3
- [ ] Simulation configs, high-scale open-loop: `configs/*.yaml` + batch of 200 open-loop runs
- [ ] On-road vs sim differences across interfaces: F4 divergence tracer, Diff page screenshot
- [ ] Testing, observability, reproducibility, tooling: tests, JSON logs, interface traces, repro-check, CLI
- [ ] Profiling: F5 profiler (Chrome trace + summary), worker scaling table
- [ ] Debugging timing/failure modes: injected sensor dropout and actuator latency visible in traces
- [ ] GenAI dev tools with review: `docs/AI_USAGE.md`

## Stack coverage (CLAUDE.md §2)
- [ ] C++20, CMake >= 3.28 + CMakePresets.json, Eigen 3.4, nlohmann/json, spdlog JSON pattern
- [ ] GoogleTest via ctest
- [ ] pybind11 + scikit-build-core wheel `simcore`
- [ ] Python 3.12, uv, numpy, pandas, typer, pytest, ruff
- [ ] ProcessPoolExecutor local batch; AWS Batch Fargate array job
- [ ] Scoped timers -> Chrome Trace JSON + summary table
- [ ] JSON logs, per-run metrics JSON, interface trace records
- [ ] React 19 + TS + Vite, Recharts, canvas replay, Vitest + RTL, Playwright
- [ ] Docker multi-stage (C++ build -> wheel -> slim runtime)
- [ ] AWS: ECR, Batch (Fargate CE, queue, array job def), S3, S3+CloudFront, CloudWatch Logs, IAM
- [ ] Terraform >= 1.9 (AWS provider 6.x), GitHub Actions CI
- [ ] `docs/AI_USAGE.md`

## Features (SPECS §5)
- [ ] F1 scenario generation + single run
- [ ] F2 open-loop batch + bench-scaling
- [ ] F3 repro-check
- [ ] F4 divergence tracing (+ `--inject sensor_dropout@tick=40`)
- [ ] F5 profiling
- [ ] F6 platforms car|van|robot
- [ ] F7 viewer pages (/, batch, run, diff, profile, repro)
- [ ] F8 cloud fan-out

## Phase 1 proof (`proof/phase1-local/`)
- [ ] tests/: ctest JUnit XML
- [ ] tests/: pytest JUnit XML
- [ ] tests/: Vitest report
- [ ] tests/: Playwright HTML report
- [ ] logs/batch-local-001.log (JSON lines incl. throughput)
- [ ] logs/scaling.json
- [ ] screenshots/01-batches.png
- [ ] screenshots/02-batch-overview.png
- [ ] screenshots/03-run-replay-car.png
- [ ] screenshots/04-run-replay-robot.png
- [ ] screenshots/05-diff-divergence.png
- [ ] screenshots/06-profile.png
- [ ] screenshots/07-repro-green.png
- [ ] logs/perfetto-trace.json
- [ ] `scripts/test-all.sh` green
- [ ] CI green on `main`
- [ ] proof/README.md index complete

## Phase 2 proof (`proof/phase2-cloud/`)
- [ ] logs/batch-describe-jobs.json (array job SUCCEEDED, 20/20 children, redacted)
- [ ] logs/cloudwatch-sample.log
- [ ] screenshots/01-cloud-batch-overview.png
- [ ] screenshots/02-cloud-run-replay.png
- [ ] screenshots/03-cloud-diff.png
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
