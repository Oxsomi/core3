# OxC3 Architecture

This document explains how the OxC3 modules layer, the conventions every module follows, and where to add new code. It complements the per-module docs in `docs/`.

## Module layering

```
                    ┌───────────────────────────────┐
                    │   OxC3(CLI) / package_cli     │  tools
                    └──────┬──────────────┬─────────┘
              ┌────────────┴───┐   ┌──────┴────────────┐
              │ shader_compiler│   │     graphics      │  (Vulkan | D3D12 backends,
              │  (DXC wrapper) │   │ generic + per-API │   static or dynamic-linked)
              └───────┬────────┘   └──────┬────────────┘
                      │                   │
                 ┌────┴───────────────────┴────┐
                 │          platforms          │  OS: window/input/file/VFS/alloc
                 └────┬───────────────────┬────┘
                      │                   │
                 ┌────┴────┐         ┌────┴────┐
                 │ formats │────────▶│  audio  │ (wav)
                 └────┬────┘         └─────────┘
                      │
      ┌───────────────┴───────────────┐
      │ types_container (allocating)  │  lists/strings/crypto/jobs/refptr
      ├───────────────────────────────┤
      │ types_math (SIMD, flp, casts) │
      ├───────────────────────────────┤
      │ types_base (no allocation)    │  types/error/buffer/atomics/lock/time
      └───────────────────────────────┘
```

Rules of thumb:

- **types_base allocates nothing** and includes no OS headers in its public API. Platform-provided symbols are marked `impl` and implemented under `src/types/base/platforms/*`.
- **types_math** depends only on base. SIMD backend selection happens via `_SIMD` in `types/base/platform_types.h` (`SIMD_SSE`, `SIMD_NEON`, `SIMD_NONE`); every op has a scalar fallback in the `*_none.inc.h` backends. Never include a `*_sse/_neon/_none.inc.h` directly, include the canonical header (`vec4f.h` etc.), the guards will error otherwise.
- **types_container** is where allocation begins; every allocating function takes a `const Allocator*`.
- **platforms deliberately depends on formats**: the virtual file system embeds oiCA archives into the executable/apk, so `platforms/platform.h` includes `formats/oiCA/ca_file.h`. If you only need formats, you can use them without platforms, not the other way around.
- **graphics** never talks to the OS directly except through platforms (surfaces) and never parses files except through formats (oiSH/oiSB, DDS for window icons).
- **Per-API graphics code** lives in `src/graphics/{vulkan,d3d12}/generic` with identical file-per-object structure. Per-OS surface glue lives one level deeper (`vulkan/{windows,linux,android,osx}`). A third API (Metal) should replicate this shape.

## The error-handling idiom (read this before writing any OxC3 code)

OxC3 has two generations of error handling. **New code uses generation 3.**

```c
Bool MyThing_frobnicate(MyThing *t, U64 index, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;              // exact name required by the macros
	Buffer tmp = Buffer_createNull();  // everything freed in clean must be initialized first

	if (!t)
		retError(clean, Error_nullPointer(0, "MyThing_frobnicate()::t is required"));

	// Call another gen-3 function: propagates failure + jumps to clean
	gotoIfError3(clean, Buffer_createEmptyBytes(64, alloc, &tmp, e_rr));

	if (index >= t->count)
		retError(clean, Error_outOfBounds(1, index, t->count, "MyThing_frobnicate()::index"));

	// ... work ...

clean:
	Buffer_free(&tmp, alloc);          // free functions are safe on null/empty
	return s_uccess;
}
```

- `s_uccess` / `e_rr` use those exact odd spellings to avoid shadowing user locals; the macros reference them by name.
- `retError(label, Error_x(...))` records the error (if the caller passed a non-NULL `e_rr`), sets `s_uccess = false`, and jumps.
- `gotoIfError3(label, call)` jumps on a `false` return from another gen-3 function (the callee already filled `e_rr`).
- Passing `e_rr = NULL` is allowed everywhere: you get the Bool, not the details.
- **Every fallible function validates all inputs** (null, bounds, enum ranges, overflow via `types/math/type_cast_safe.h`) and reports through `Error` constructors with a `"Function()::param reason"` string.
- The `Error` struct embeds a stacktrace: full depth in debug, 1 frame in release (`ERROR_STACKTRACE`). **FFI note:** this means `sizeof(Error)` differs between debug and release builds, bindings must match the build configuration.

## Ownership & lifetime conventions

- **`Buffer` / `GenericList` / `CharString` encode ref-ness in-band** (`isRef`, `isConst`). "Ref" instances must never be freed (handled by _free functions themselves); const refs cannot be mutated (checked at runtime, not just by convention). Free functions null the struct so double-free is inert.
- **`RefPtr`** is the shared-ownership primitive (atomic refcount + `RefPtrType` carrying typeId/size/allocator/destructor). The graphics module exposes objects *only* as `RefPtr`s: `X_free` functions are internal. `WeakRefPtr` is a documented non-owning alias, not a true weak reference.
- **Allocators are explicit everywhere.** `Platform_instance->alloc` is the tracked default (leak reports at shutdown, per-allocation stacktraces in debug); tests and tools should prefer it so leaks surface.

## Threading conventions

- Primitives: `AtomicI64`, `SpinLock` (owner-checked unlock), `Thread`.
- **`JobQueue`** is the sanctioned way to parallelize: stable `threadId` per context lets you index per-thread resources (e.g. one shader `Compiler` per thread) without locks; `threadCount <= 1` runs jobs inline and deterministically for debugging; jobs may push jobs (fan-out pipelines).
- Lock what the docs say to lock: e.g. swapchain/command-list/buffer locking rules are specified in `docs/graphics_api.md`, they are contracts, not suggestions.

## Serialization conventions (oiXX family)

- Every Oxsomi format: 4-byte magic (`oiXX`), 1-byte packed version, validated enum fields, optional AES256/128-GCM (only the header stays plaintext, and it is authenticated), and forward-compat extension blocks with explicit skip sizes.
- Parsers are written for hostile input: bounds-check every table index (e.g. oiCA parent dirs `can't >= self`), never trust lengths, prefer streaming (`StreamCursor`) over whole-file buffers.
- New formats: register an id in `docs/id_registry.md`, write the spec in `docs/` **with a Status line** (Draft / Implemented / Stable), then implement read+write+tests together (see `src/formats/oiCA/test` for the expected shape: round-trip, mutation ops, size/edge cases).

## Testing conventions

- Each module ships `src/<module>/test/test_<module>_*.c` with a `_main.c` runner; build with `-tests True`.
- Formats/compiler prefer **golden files** (checked-in `.oiSH`/fixtures): bit-exact comparisons catch cross-platform and dependency-upgrade drift.
- Platform functional tests simulate real input (SendInput / xdotool): extend these when touching window/input code.
- Gaps we know about (contributions welcome): fuzz targets for the format parsers, unit tests for the API-independent graphics command recorder/validator.

## Where things go, quick answers

| You're adding… | It goes in… |
| --- | --- |
| A pure algorithm / math helper | `types_math` (with scalar fallback + tests) |
| A container / allocating utility | `types_container` |
| Anything touching files, windows, input, OS | `platforms` (generic first, per-OS glue minimal) |
| A file format | `formats` + spec doc + id registration + tests |
| A GPU feature | `graphics/generic` first, then both backends symmetrically |
| A new CLI verb | `src/tools/oxc3_cli/<verb>.c` + registration in `operations.c` |
