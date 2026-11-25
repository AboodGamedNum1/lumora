# Golden images (M1 harness)

Normative detail for Section 16.1 as implemented so far.

- Baselines are binary PPM files under `tests/golden/<renderer>/<name>.ppm`. Each renderer has its own directory: today only `lavapipe` (Mesa software Vulkan on the Linux runner). Baselines from one renderer are never compared with another renderer.
- Comparison is per channel with a tolerance (2 for lavapipe), implemented in `tests/golden.h` and unit-tested.
- On failure or a missing baseline the test writes `<name>.actual.ppm` and `<name>.diff.ppm` to `build/golden-out`, and the Linux CI job uploads them as the artifact `linux-golden-and-logs`.
- Blessing: set `LUMORA_BLESS_GOLDEN=1` to overwrite baselines, then review the images by eye before committing. The first baseline was taken from the CI artifact of a run that failed only on the missing baseline, and was checked visually (blue background, red triangle).
- Evidence label: CI-verified, software rendering. Never device evidence.
