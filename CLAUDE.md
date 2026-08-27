# CLAUDE.md

Operational knowledge for AI contributors to OxC3. The human-facing rules live in
[docs/code_style.md](docs/code_style.md), [FOR_CONTRIBUTORS.md](FOR_CONTRIBUTORS.md) (its PR checklist
applies to every change, not only PRs) and [docs/graphics_api.md](docs/graphics_api.md); this file only
adds what those don't say and what is easy to get wrong.

## What this is

OxC3: a C11 cross-platform framework (types, platforms, formats, shader compiler, graphics over Vulkan
and D3D12). The `oxc::`/`gfx::` C++ layers in `include/**/*.hpp` are hand-written wrappers, committed,
and updated together with the C headers they wrap.

## Build, test, and the consumer trap

- `python build.py -mode Debug -tests True` (and `Release`). The style check
  (`src/tools/check_style.py`, cmake target `check_style`) runs on every build and fails it.
- Graphics interface tests: `OxC3_graphics_interface_test`. A new graphics feature adds a test under
  `src/graphics/test/interface/` and registers it in `test_graphics_shared.h` plus
  `test_graphics_interface.cpp`.
- The debug build's tracked allocator reports leaks with stacktraces at shutdown; tests stay clean,
  also under ASan.
- Consumers fetch core3 from the github main branch through conan. A local edit is INVISIBLE to a
  consumer build until pushed: a consumer links the cached library, not this tree. After a push the
  consumer needs a fresh conan resolve, because conan caches the clone per recipe revision.

## Error handling traps

- `Error *e_rr` is the last parameter; use `retError`/`gotoIfError3` with the `clean:` label, error
  strings as `"Function()::param reason"`.
- A defaulted `Bool` parameter sitting before `e_rr` silently converts a misplaced `Error *` argument
  to `true` and drops the error channel. At call sites, spell out every defaulted argument before
  `e_rr` rather than relying on defaults.
- Overflow-safe bounds checks: write `offset > size || size - offset < needed`, never `offset + needed`.

## Style beyond code_style.md

- Comments state rules and reasons, never incidents: no session narration, no "changed X to Y", no line
  numbers, no dashes as punctuation. Continuation lines start with `//` and break at sentence
  boundaries.
- Empty parameter lists are `()`, not `(void)`, matching the tree.
- When in doubt, grep a neighbouring file and mirror it; consistency with the tree beats external
  convention.

## Graphics features

- A feature lands in `graphics/generic` with validation, plus BOTH backends, or gates itself behind a
  capability bit with graceful degradation. An arm that has never executed (typically D3D12 on a Linux
  dev box) is written to spec and SAID to be blind, in the docs section and the commit.
- Every feature ships as a set: validation, both backends, an interface test, and a section in
  `docs/graphics_api.md`.
- Adding an optional Vulkan extension touches four index-coupled sites, append-only:
  `optExtensionsName[]` (vk_instance.c), `EOptExtensions` (vk_instance.h), the `getDeviceFeatures`
  query + capability consume (vk_instance.c), and the extension-enable switch case (vk_device.c). The
  switch case is the one that gets forgotten; without it the extension is never enabled and device
  creation fails with no extension named anywhere.
- All shader permutations of one entrypoint must reflect identical inputs; an entry whose signature
  changes is a new entry, not a permutation.

## Numerics and shared helpers

- `F32x4_normalize3` and relatives use the SSE `rsqrt` approximation (an axis normalizes to ~0.9998).
  Where exactness matters, divide by the exact length instead.
- CPU twins of shader helpers (packing, math) live in `types/math/` (e.g. `pack.h`) and are added as a
  PAIR with their HLSL counterpart, kept bit-identical.
- Codecs are stream-to-stream on both sides; a convenience wrapper that materializes a whole buffer
  must say so in its comment.

## GPU testing on the Linux/NVIDIA dev box

- If validation layers flag bindless descriptors at index >= 1, suspect the VVL version before the
  code; some SDK releases false-positive there where upstream main is fixed.
- Functional tests that present to a physical swapchain are run by a human, never automated; measured
  or headless runs go through a nested compositor, not the desktop session.
