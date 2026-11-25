# AI usage

AI coding assistants (Claude Code) wrote a large share of the first draft of this repository: C++ components, the Python CLI, the React viewer, Terraform and the shell scripts.

How the output was reviewed and validated:

- **Spec first.** `SPECS.md` and `CLAUDE.md` fixed the architecture, interfaces and acceptance criteria before any code was generated; generated code was checked against them rather than the other way round.
- **Tests as the gate.** Every feature has a happy-path test (GoogleTest, pytest, Vitest, Playwright). Code was kept only when `scripts/test-all.sh` was green.
- **Determinism checks.** The reproducibility check (same digest across repeats and worker counts) catches silent nondeterminism that an assistant could introduce, such as unordered iteration or wall-clock use.
- **Reading the diff.** Generated Terraform was read for IAM scope, public exposure, cost (no NAT, small Fargate sizes) and teardown behaviour before `apply`; `verify-destroyed.sh` is an independent check that nothing is left running.
- **Real data in the proof.** Screenshots and logs come from scripted runs with real data flowing through the system, so claims in the docs can be checked against artifacts.
- **Secrets and privacy.** Nothing secret is committed; AWS account IDs are redacted from proof logs by the capture script.

Known limits: assistants tend to over-build and to hide errors behind broad fallbacks, so the code was reviewed for dead code and swallowed exceptions, and the tests are shallow by design (one happy path per feature).
