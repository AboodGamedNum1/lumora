# Coding standards (M0)

- Language level: C++23 today. C++26 features are used only after `docs/cpp-standard.md` lists them as accepted by every pinned compiler.
- Formatting: 2-space indent, 100 columns, `snake_case` for functions and variables, `PascalCase` for types, `kConstant` for constants, namespace `lumora`.
- Headers use `#pragma once`. Public headers live under `<module>/include/lumora/`.
- No exceptions across module boundaries that touch the RHI or the Android JNI layer. No raw owning pointers.
- Warnings are on (`-Wall -Wextra -Wpedantic`, `/W4`). New code adds no warnings.
- Anything that parses untrusted scene data follows `docs/security-model.md` and gets fuzz coverage.
- Non-core components reuse a maintained library first (`docs/build-and-deps.md`); a decision to build instead is written in `docs/dependency-manifest.md`.
- Documents are English. The check `python3 tools/check_docs.py` must pass before any document change is committed.
- Evidence labels (CI-verified, WARP software evidence, owner-hardware-verified) are used exactly as defined in `docs/testing.md`.
