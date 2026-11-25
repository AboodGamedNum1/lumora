# C++ standard and feature matrix (M0)

Rule: use only language features that every pinned compiler accepts. The project is built as C++23 now (`LUMORA_CXX_STANDARD=23`). Moving to C++26 requires an entry here that shows every pinned compiler accepting the features in use.

| Toolchain | Where pinned | Observed language level |
|---|---|---|
| GCC (Ubuntu 24.04 runner) | CI job log, recorded at first green run | C++23 |
| MSVC (windows-2025 runner) | CI job log, recorded at first green run | C++23 (`/std:c++latest` is only partial for C++26) |
| NDK Clang | Android job, pinned NDK revision | C++23 |

The CI jobs print `toolchain_string()` from the editor smoke test, so the observed compiler and language level are in each job log. The automatic check that fails CI when this file disagrees with a feature-test header is planned (see PLAN.md, M0 open items).
