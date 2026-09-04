# CLAUDE.md

Operational knowledge for AI contributors to OxC3. The human-facing rules live in
[docs/code_style.md](docs/code_style.md), [FOR_CONTRIBUTORS.md](FOR_CONTRIBUTORS.md) (its PR checklist
applies to every change, not only PRs) and [docs/graphics_api.md](docs/graphics_api.md); this file only
adds what those don't say and what is easy to get wrong.

## What this is

OxC3: a C11 cross-platform framework (types, platforms, formats, shader compiler, graphics over Vulkan
and D3D12). The `oxc::`/`gfx::` C++ layers in `include/**/*.hpp` are hand-written wrappers, committed,
and updated together with the C headers they wrap.

## Review your own diff before handing it over

A human review cycle spent on a stale comment or a formatting slip is a wasted one. Read the whole diff
back, as a reviewer would, before presenting it. Both halves matter; the second is the one that gets
skipped.

A change also has to carry what belongs with it:

- A test wherever the behaviour can be tested, in the same change. A rule nothing exercises is one that
  gets broken silently later. See "Graphics features" for what a graphics feature ships as a set.
- The documentation that describes it, updated in the same change, including the places that described the
  behaviour you just replaced. Changelog.md is NOT currently maintained and is years behind; leave it alone
  rather than adding one entry that implies the rest is current.

Against the standards:

- Every comment you added OR that sits above code you changed. A comment does not go stale loudly, so
  changing behaviour and leaving its comment is the default failure. Delete comments for things that no
  longer exist; a doc block for a renamed or removed function is worse than none.
- No history in comments: they state the rule the code follows now, never what it used to do or why the
  previous attempt was wrong. That belongs in a design record.
- Comments in headers state the rule; the reasoning goes at the site that breaks if it is got wrong. Say
  it in ONE place, and check the other places do not already say it.
- Formatting: wrapping, 128 columns, tabs, `()` not `(void)`, no dash punctuation. See docs/code_style.md.
- Nothing in the diff that is unrelated to the change, and no downstream project referenced from core3.

For logic:

- Every error path. A failure between two writes that must both land is the classic one: reserve or
  validate first so the committing part cannot fail halfway.
- Both backends kept in step. Changing a shared rule and converting only the backend you can run leaves
  the other quietly wrong, and the tests you run will not say so.
- Shared state: what lock covers it, at every site that touches it, and whether two locks can nest.
- Docs and identifiers: names in prose still exist, and the described behaviour is the current one.

And run the suite, not just the build:

- `python build.py -mode Release -tests True` builds and runs it. A build alone proves nothing about the
  tests, and a build with tests DISABLED (the option set a consumer package is built with) does not even
  compile them, so "it built clean" is not a result.
- Sanitizers where the change could touch memory or lifetime, which is most C in this tree:
  `-compiler clang -asan True -ubsan True`. clang and gcc only; MSVC has no ASan here, so a Windows-only
  change is checked by the other compilers instead. This is what CI runs, so a leak or a stale pointer
  that only ASan sees fails there rather than locally.
- More than one toolchain where the change is compiler sensitive: macros, alignment, intrinsics, C and C++
  interop, or anything the ABI reaches. `-compiler gcc|clang` on Linux, msvc and clang-cl on Windows.
  A tree that builds under one toolchain says nothing about the other two.
- A new check is worth nothing until it has been seen to fail. Break the thing it guards once, confirm the
  suite catches it on exactly that assert, and put it back.
- Say which suite ran and what it reported. "Tests pass" without the count is not a report.

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
- `e_rr` may always be NULL (probe calls pass NULL deliberately). Never dereference it: a failed callee
  already filled it, so propagate with `gotoIfError3`, or `s_uccess = false; goto clean;` when cleanup
  has to happen first. `retError(clean, *e_rr)` is both a NULL dereference and a double-set.

## Style beyond code_style.md

- Comments state rules and reasons, never incidents: no session narration, no "changed X to Y", no line
  numbers, no dashes as punctuation. Continuation lines start with `//` and break at sentence
  boundaries.
- Empty parameter lists are `()`, not `(void)`, matching the tree.
- Wrapped calls and conditions close on their own line, at the indent of the line that opened them, never
  trailing the last argument; a nested call closes on its own line too. A multi-line condition puts `if (`
  alone, operands one level deeper, `)` back at the `if`'s indent. See docs/code_style.md.
- When in doubt, grep a neighbouring file and mirror it; consistency with the tree beats external
  convention.

## Graphics features

- A feature lands in `graphics/generic` with validation, plus BOTH backends, or gates itself behind a
  capability bit with graceful degradation.
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
