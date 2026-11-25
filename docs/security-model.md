# Security model for shared scenes (Section 15)

> Normative document of the Lumora Master Plan. Section numbers are stable IDs shared by all files and are not contiguous inside one file. Read `LUMORA_MASTER.md` first.

## 15. Security model for shared scenes (friends' wallpapers)

Friends will run each other's scenes; this is a designed-for threat model, not an afterthought. The full spec is `docs/security-model.md` (M3, expanded at M5).

**Three distinct properties:**

1. **Integrity** - the package is not corrupted since packing: SHA-256 per file, verified before load; corrupt packages rejected with a clear message. `checksums.txt` lives unsigned inside the same ZIP, so it detects **accidental corruption only**; deliberate tampering is undetectable until the reserved signature block (item 2) is used, and the covered set is defined explicitly as every archive entry except `checksums.txt` itself. Checksums do **not** prove who made a package.
2. **Authenticity** - proof a package came from a particular publisher. v1 has **no publisher-signature ecosystem**: friend-shared packages are treated as untrusted and contained by the sandbox + capability system below. Optional package signatures are a post-v1 candidate (recorded, not promised); the documented migration path is: package format reserves a signature block now, so a future signature system layers on without a format break. **Semantics defined now:** the block lives in a reserved ZIP entry (or the ZIP comment), signs the sorted entry list with hashes plus the manifest, and a package that claims a signature that fails verification is rejected. A v1 loader that ignores the block gets no protection, and a downgrade (stripping the block) is possible until a version that requires it; the plan says plainly that `checksums.txt` gives no security, only corruption detection.
3. **Trust** - the user's decision. The app's job is to make untrusted packages safe to run, not to vouch for them.

**Layers:**

1. **Lua sandbox** as specified in Section 9.1, with release-build enforcement of memory quotas and instruction budgets, caps on handlers/coroutines/re-entrancy, and a defined error/suspension policy.
2. **Archive defenses:** vetted ZIP library (minizip-ng) with a complete validation policy: ZIP64 container rules; encrypted and unsupported-compression entries rejected; CRC verified per entry; absolute/traversal paths, drive/UNC prefixes, symlinks and special files, nested archives, duplicate paths, Windows case-folding collisions, and Unicode normalization collisions all rejected; hard limits on package size, total uncompressed size, per-file size, entry count, compression ratio (ZIP-bomb defense), and streaming-extraction throughput; free-space check before extraction; integer-overflow-safe size handling; extraction is cancellable with full cleanup of partial output. Fuzzed (Section 16.1). **:** overlapping or shared local-header data between entries (zip quines and overlap bombs that defeat per-entry ratio limits), so data must be non-overlapping and in order and the total declared uncompressed size is capped before any inflate; central-directory versus local-header mismatches (name, sizes, flags, data descriptors) with the declared size enforced as a hard stop while inflating; multiple end-of-central-directory records and prepended or appended payloads (polyglots); ZIP64 field mismatches; entry-name rules (control characters, backslashes, trailing dots or spaces, Windows reserved names such as CON and NUL). An entry is never read before its hash passes. `settings.json` and `scene.json` have explicit nesting, size, string-length and duplicate-key limits. minizip-ng is pinned and its CVEs tracked. Import (SAF, share sheet) validates off the UI thread with progress and cleans up on cancel.
3. **Native parsers are attack surface:** the statement that no executable code other than Lua ever ships in a package is misleading. Shaders are GPU programs, and glTF/KTX2/image/audio/video/ZIP parsers and the font stack all process untrusted bytes. Every parser boundary gets strict validation, malformed-input and fuzz tests, dependency security updates, and graceful failure. SPIR-V is validated before it reaches the driver.
4. **Runtime capability enforcement:** a manifest declaration never grants anything by itself. The app explains each requested capability; the user grants or denies per scene; Lua sees only granted capabilities and gets explicit `permission_denied`/`unavailable` results otherwise; revocation while a scene runs is handled live; the permissions center shows and revokes per-scene access.
5. **Resource limits:** load-time budgets (texture memory, script size, node count, audio/video size and duration) with graceful rejection. Add a per-frame GPU time cap and a GPU-hang and timeout recovery rule (TDR on Windows, GPU reset on Android). Video in packages is off by default per scene until fuzzed, with size, duration, resolution and codec-profile caps, unknown containers rejected, decoding kept in the platform decoder.


### 15.1 Shader static limits

A per-frame GPU time cap cannot interrupt a shader that is already executing, and on a mobile GPU shared with the compositor a runaway shader can freeze the display for seconds. Packages carry precompiled shaders from untrusted sources, and runtime compilation is forbidden, so limits are static and enforced at export and again at load:

- Constant-bounded loops only, a maximum trip count, a maximum instruction count per stage, a maximum texture-sample count, no recursion.
- A package with custom shaders shows a clear warning at import, and its first run happens at the lowest quality tier.
- Adversarial shaders are part of the fuzz and test corpus.
- The load-time check runs on the SPIR-V (SPIRV-Tools or an equivalent; its size and time cost in the runtime is measured at M3).

**Open technical question TQ1.** The package also carries GLSL ES and HLSL produced at export. Limits checked on the SPIR-V do not prove those other forms equal it. Options: (a) regenerate GLSL ES from the validated SPIR-V at import with SPIRV-Cross (a translation step, not a compiler, but a change from "cross-compiled at export" in D8); (b) ship only SPIR-V plus hashes and generate the other forms in the packer, accepting that friend-made packages cannot be re-verified. Not decided here. It is decided at M3 and reported to the owner because it touches D8.
