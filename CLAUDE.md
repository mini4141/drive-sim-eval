# CLAUDE.md — drive-sim-eval

These are the engineering rules for Claude Code on this repository. You work **fully autonomously**. Don't stop to ask the user questions. When something is ambiguous, pick the simplest option that keeps every stack item in §2 covered. Record it in `docs/DECISIONS.md` (one line: date, decision, reason) and keep going. Read `SPECS.md` end to end before writing any code. It is the source of truth for features, architecture and proof.

## 1. Project at a glance

| | |
|---|---|
| Project | Drive Sim Eval: a deterministic open-loop and closed-loop driving simulator with a large-scale evaluation pipeline |
| Repo | public GitHub repo `drive-sim-eval`. `origin` is already configured and the default branch is `main` |
| Target role | Software Engineer, Core Simulation (autonomous driving, modern C++ and Python) |
| Pitch | A C++ simulation core runs a learned driving policy through sensors → inference → routing → control → vehicle dynamics. Python drives batch open-loop evaluation at scale, proves bit-for-bit reproducibility, finds where a sim run diverges from an on-road log, and profiles every component. A web viewer shows it all. |
| Priority | **Breadth of the tech stack beats depth of business logic.** Every technology in §2 must appear in running code and be visible in the proof. Full validation, edge cases and complete domain rules are out of scope. |

## 2. Mandatory tech stack

| Area | Technology |
|---|---|
| Core language | C++20, CMake ≥ 3.28 with `CMakePresets.json`, Eigen 3.4 (header-only), nlohmann/json, spdlog (JSON log pattern) |
| C++ tests | GoogleTest via `ctest` |
| Bindings | pybind11, packaged with scikit-build-core into a Python wheel (`simcore`) |
| Python | Python 3.12, `uv` for env/deps, numpy, pandas, typer (CLI), pytest, ruff |
| Scale-out | Local: `concurrent.futures.ProcessPoolExecutor`. Cloud: AWS Batch on Fargate array jobs |
| Profiling | Built-in per-component scoped timers exported as Chrome Trace Event JSON (opens in Perfetto) plus a summary table |
| Observability | Structured JSON logs, per-run metrics JSON, interface trace records at every component boundary |
| Frontend | React 19 + TypeScript + Vite, Recharts, HTML canvas top-down replay, Vitest + React Testing Library, Playwright |
| Containers | Docker multi-stage build (C++ build → wheel → slim runtime) |
| AWS | ECR, AWS Batch (Fargate compute environment, job queue, array job definition), S3 (scenarios + results), S3 + CloudFront (viewer), CloudWatch Logs, IAM |
| IaC / CI | Terraform ≥ 1.9 (AWS provider 6.x) · GitHub Actions (CMake build + ctest, pytest, viewer build + tests) |
| GenAI tooling | `docs/AI_USAGE.md`: how AI assistants were used and how their output was reviewed and validated (a stated job requirement) |

Don't swap an item for an alternative. If a library is unavailable or broken, use the closest equivalent and log the swap in `docs/DECISIONS.md`.

## 3. Environment

- The host is a **Windows 11 PC**. Claude Code's shell is Git Bash, and the container runtime is Docker Desktop (WSL2 backend). Write automation as POSIX `bash` scripts under `scripts/` and use forward-slash paths. Add a `.gitattributes` with `* text=auto eol=lf` so scripts keep LF line endings.
- Prerequisites: Docker Desktop, Git, CMake, a C++20 toolchain (build inside Docker if MSVC is awkward: a `scripts/build-core.sh` that builds in a Linux container is the default), Python 3.12 + uv, Node.js 24 LTS, Terraform, AWS CLI v2. If one is missing, install it with `winget` or the official installer and record the version in `docs/DECISIONS.md`.
- The `gh` CLI is logged in to the owner's GitHub account, and AWS CLI v2 is installed.
- Don't use Bitnami charts or images (their free catalogue has been retired). Use official upstream images and charts.

## 4. AWS account rules

- Profile: **`drive-sim-eval-deployer`**. Always pass it explicitly, either as `--profile drive-sim-eval-deployer` or by exporting `AWS_PROFILE=drive-sim-eval-deployer` inside the scripts. Never use the default profile or any other profile.
- Region: **`ap-southeast-2`** for everything except global services such as CloudFront and IAM.
- The profile has `PowerUserAccess` + `IAMFullAccess`. Start Phase 2 with `aws sts get-caller-identity --profile drive-sim-eval-deployer`. If that fails, stop and write the reason at the top of `PROGRESS.md`.
- Use AWS only. Where a requirement names Azure, GCP or another hyperscaler, use the AWS equivalent stated in `SPECS.md`.
- IaC: Terraform (`infra/`), local state. Don't create resources by hand in the console: every cloud resource comes from code in `infra/`. Keep IaC state local and gitignored.
- Tag every resource with `Project=drive-sim-eval`, `Ephemeral=true`, `ManagedBy=<tool>` and `Owner=alvieirvansyah`.
- Deployment runs **from this PC via scripts** (`scripts/deploy.sh`, `scripts/destroy.sh`). GitHub Actions only builds and tests, and never holds AWS credentials.

## 5. Phases and gates

Work strictly in order. A phase is done only when its proof exists (§8) and its items are ticked in `PROGRESS.md`. Commit and push at the end of every phase.

1. **Phase 0, scaffold.** Set up the repo layout from `SPECS.md` §4. Create `README.md`, `PROGRESS.md` (copy every checklist item from `SPECS.md` §2 and §9–§11 into it), `docs/DECISIONS.md`, `.gitignore`, `.gitattributes`, `.env.example` and the CI workflow.
2. **Phase 1, local.** Build every feature in `SPECS.md` §5. Everything runs on this PC with Docker and local tooling, and makes **no AWS calls**: use the local stand-ins listed in `SPECS.md`. Run the full test suite, then capture proof into `proof/phase1-local/`. Phase 1 counts as "ready for next" only when `scripts/test-all.sh` is green, the CI run on `main` is green, and the proof index is complete.
3. **Phase 2, AWS showcase.** Deploy with `scripts/deploy.sh`, run the smoke/e2e tests against the cloud endpoints and capture proof into `proof/phase2-cloud/`. Keep the stack alive only as long as capturing takes, with a target of **under 2 hours**.
4. **Phase 3, destroy.** Right after capturing, run `scripts/destroy.sh` and then `scripts/verify-destroyed.sh`, and save the output to `proof/phase3-destroy/`. This phase is **mandatory even if Phase 2 partly failed**. Never end a session with cloud resources running.
5. **Phase 4, write-up.** Write `docs/PORTFOLIO.md`. It needs the problem, an architecture diagram (Mermaid), a stack → requirement table, the key screenshots (relative links into `proof/`), what was hard, the approximate AWS cost and how to reproduce. This file becomes a technical page on alvieirvansyah.com.

## 6. Engineering rules

- Keep it simple and conventional: idiomatic layout for each language, small modules, no premature abstraction, no dead code.
- Configure through environment variables, with a committed `.env.example`. Real `.env` files are gitignored.
- **Never commit secrets.** That includes credentials, IaC state, `.terraform/`, `cdk.out/`, kubeconfigs, private keys and certificates. Generate local dev keys and certs into a gitignored `.local/` folder.
- Every HTTP service exposes a health endpoint (`/healthz` or the framework idiom) and logs structured JSON to stdout.
- Pin versions: commit lockfiles and pin container images to at least a minor tag.
- Use one command for each job:
  - `scripts/dev-up.sh` starts everything locally and `scripts/dev-down.sh` stops it.
  - `scripts/seed.sh` loads demo data. Start-up may run it automatically so that demos never start empty.
  - `scripts/test-all.sh` runs every test layer.
  - `scripts/capture-proof.sh <phase>` captures the proof for a phase.
- Keep `README.md` current. It covers what the project is, the architecture diagram, a quickstart, deploy/destroy, a link to `proof/README.md` and `docs/PORTFOLIO.md`, and the CI badge.
- Don't put the hiring company's name anywhere in the repo. Describe the target role generically.
- `docs/AI_USAGE.md`: briefly note how AI coding tools were used and how their output was reviewed and tested.

## 7. Testing rules

- Tests are **shallow by design**: one happy-path test per feature or flow is enough. Don't write exhaustive validation or edge-case tests.
- Required layers: GoogleTest (one test per C++ component), pytest (bindings plus each CLI command on a tiny batch), Vitest (one component render), Playwright (viewer e2e: batch page → run page → diff page).
- `scripts/test-all.sh` runs every local layer and exits non-zero on failure. CI runs every layer that doesn't need the cloud.
- Save test output as files (JUnit XML, an HTML report or a plain log) under the current phase's `proof/.../tests/`.

## 8. Proof rules

Proof is the deliverable. Use this folder layout:

```
proof/
  README.md                 # index: each artifact on one line, with what it proves
  phase1-local/
    screenshots/            # PNG, named NN-what-it-shows.png
    logs/                   # captured service logs and CLI output
    tests/                  # test reports
  phase2-cloud/
    screenshots/  logs/  tests/
    resources.txt           # deployed AWS resources (from IaC outputs / describe calls)
  phase3-destroy/
    verify-destroyed.txt
```

- Capture screenshots with a script (`scripts/capture-proof.sh`, driving Playwright headless), never by hand, so they can be reproduced. The screenshots listed in `SPECS.md` are the minimum.
- There is no AWS console login, so render AWS-side visuals from the CLI. For example, `aws cloudwatch get-metric-widget-image` renders a metrics graph as a PNG. Otherwise save CLI `describe`/`get` output as logs.
- Before saving logs or CLI output, redact 12-digit AWS account IDs (`sed -E 's/[0-9]{12}/<ACCOUNT_ID>/g'`), access keys, tokens, passwords and the deployer's public IP.
- Proof must show the system **working**, with real data flowing through it, and not just "deployed".

## 9. Cost and safety guardrails (AWS)

- Use the smallest viable sizes: AWS Batch Fargate compute environment max 16 vCPU, job size 1 vCPU / 2 GB, array size ≤ 50, viewer via S3 + CloudFront (PriceClass_100).
- **No NAT gateways.** Also no multi-AZ databases, provisioned IOPS, reserved capacity or Savings Plans, Route 53 domains, or Marketplace subscriptions.
- Databases: `deletion_protection = false`, skip the final snapshot and keep backup retention minimal. S3 buckets are force-destroyable and ECR repos force-deletable. CloudWatch log groups get 1-day retention and are created by IaC, so they get destroyed with it.
- Public app endpoints are fine for the showcase. Keep databases private: their security groups admit only the app tier. Don't open SSH or database ports to `0.0.0.0/0` and don't use SSH at all. Use SSM if you need a shell.
- Don't send real emails or SMS, and don't call paid third-party APIs. Anything external is mocked unless `SPECS.md` says otherwise.
- `scripts/destroy.sh` must be idempotent and safe to re-run. It first deletes anything that would block the IaC destroy: cancel or terminate running Batch jobs, disable then delete the job queue before the compute environment, empty the S3 buckets.
- `scripts/verify-destroyed.sh` lists anything still tagged `Project=drive-sim-eval` (`aws resourcegroupstaggingapi get-resources`) and runs the specific checks in `SPECS.md` §11. It exits non-zero if anything billable remains. The tagging API can show resources for a few minutes after deletion, so confirm each hit with a direct describe call before failing.

## 10. Git and CI

- Use Conventional Commits (`feat:`, `fix:`, `test:`, `docs:`, `infra:`, `chore:`). Commit small and often, and push to `origin main` at least at the end of every phase.
- `.github/workflows/ci.yml` runs build, lint and the cloud-free tests on every push and pull request. Keep it green.
- Never force-push `main`. Never commit build output, dependency folders (`node_modules`, `target`, `bin`, `obj`, `.venv`) or state files.

## 11. Project-specific rules

- **Determinism is a feature.** Use a fixed timestep (`dt = 0.05 s`) and a seeded `std::mt19937_64` per run, with no wall-clock in the simulation logic. Never iterate over `unordered_map` in a way that affects output. The same scenario, config and seed must produce an identical output digest (SHA-256 of the canonical trajectory JSON) across runs and across worker counts.
- Build the C++ core as a library (`core/`) with clean component interfaces (`ISensorModel`, `IPolicy`, `IRouter`, `IController`, `IVehicleModel`). Open-loop and closed-loop are **one engine**: the pipeline config (YAML/JSON) decides which components run and how. Don't fork the code paths.
- The policy is a *learned* model: `tools/train_policy.py` trains a small MLP with numpy by imitating an expert (pure pursuit + speed profile) on generated scenarios and exports the weights as JSON. The C++ `MlpPolicy` loads that JSON and runs inference with Eigen. Commit the trained weights so the build is reproducible.
- Scenarios are synthetic and generated by `simeval gen-scenarios`, which produces a lane-graph map, ego start, goal, actors and a recorded on-road log. Don't download datasets.
- Default to building C++ inside a Linux Docker container so the result doesn't depend on whether MSVC is installed on Windows. CI builds on `ubuntu-latest`.
- Results contract: every run writes `results/<batch_id>/<run_id>.json` (metrics, digest, timings, trace summary), and the batch writes `results/<batch_id>/index.json`. The viewer reads only these files (static hosting), both locally and on S3/CloudFront.

## 12. Definition of done

- [ ] Every stack item in §2 is used in running code. Cross-check with the coverage matrix in `SPECS.md` §2 and tick each row in `PROGRESS.md`.
- [ ] Phase 1 proof is complete, and all tests are green locally and in CI.
- [ ] Phase 2 proof is complete: cloud screenshots, a cloud test run and the resource list.
- [ ] Phase 3: everything is destroyed and `verify-destroyed.txt` shows nothing billable left.
- [ ] `docs/PORTFOLIO.md` and `README.md` are written, and everything is committed and pushed.
