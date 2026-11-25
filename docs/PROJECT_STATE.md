# PROJECT_STATE

> Current state of the project. Every session starts by reading this file and the ADR log (`docs/adr/`), and ends by updating it.

last_completed_milestone: none
last_updated: 2026-11-25

## Current milestone
M0, Foundations and CI. Exit criteria: see `docs/milestones.md`; status table in `PLAN.md`.

## Done
- Repo skeleton, docs, CMake skeleton, pins, test entry point committed (CI confirmation pending).

## In flight
- CI workflow on main: Linux and Windows jobs first run.

## Known failures
- none recorded yet

## Next five tasks
1. Get Linux and Windows CI green.
2. Android Gradle skeleton and build job.
3. Emulator job and KVM check.
4. Secret scan, Dependabot, CodeQL.
5. WARP spike and GNOME spike start.

## Where CI logs and artifacts live
https://github.com/Captain3BoOd/lumora/actions

## Evidence labels in use
CI-verified, WARP software evidence, owner-hardware-verified. Never claim hardware evidence that has not been run on the owner's device.

## Decisions pending with the owner
None block M0.
