# OxC3 on the web (emscripten, wasm64)

The web target builds the whole engine minus graphics as wasm64, with the shader compiler
**enabled**: DXC itself cross compiles to wasm (see the dxc conan recipe), so OxC3 can compile
HLSL to DXIL and SPIR-V inside node or a browser. Verified 2026-08-07 by the bundled test suite
(`OxC3_wtest`) compiling the full shader corpus under node.

## Build

```bash
python3 build_web.py                     # Release + test bundle (needs emsdk at ~/emsdk or $EMSDK)
python3 build_web.py --run_tests         # ... and run the suite under node
python3 build_web.py -suite shader_compiler --run_tests --skip_build
```

Same shape as `build_android.py`: a host oxc3 (with `OxC3_package`) is built first and
tool_required, since the cross build's own packager is a `.js` the build machine can't run.
Everything web-specific in conan lives in `packages/conan/profiles/emscripten_wasm64.jinja`;
`-fwasm-exceptions` and `-m64` are ABI and must match in every consumer link.

Outputs land in `build/<mode>/web/wasm64/bin`: `OxC3.js/.wasm` (CLI, runs under node),
`OxC3_package.js/.wasm`, `OxC3_wtest.js/.wasm` (every test suite in one module, node exec,
also wired into ctest via `CMAKE_CROSSCOMPILING_EMULATOR`).

## Platform notes

- **Single-threaded** for now: no `-pthread`, `Platform_getThreads()` returns 1, JobQueue runs
  inline (a designed mode, see `include/types/container/job_queue.h`). Multithreading needs
  `-pthread` on *every* object including the dxc package (SharedArrayBuffer + COOP/COEP in
  browsers) and is future work.
- **Graphics off** (`EnableGraphics` coerced off): no WebGPU backend yet. The platform layer is
  headless: `WindowManager_createNative` returns false, which the generic layer supports.
- **No dynamic linking**: wasm dlopen needs MAIN_MODULE, so `SUPPORTS_DYNAMIC_LINKING` is not
  defined on web (`include/platforms/dynamic_library.h`) and dependent tests self-skip.
- **Virtual files**: no readable executable to parse sections from, so the android model applies:
  `src/platforms/web` scans `packages/<target>/<name>.oiCA` from the mounted filesystem
  (NODERAWFS under node; a browser shell would preload into MEMFS instead).
- **Audio**: emscripten ships its own OpenAL (WebAudio-backed), so `openal_soft` isn't required.
  Under node there is no Web Audio at all: `AudioInterface_getDeviceInfos` reports an empty
  device list (graceful), and device-dependent behavior only exists in a browser.
- **Allocator alignment (important)**: OxC3's widest type `I32x4` is `alignas(16)` on *every*
  backend, so allocators must return 16-byte aligned memory. `malloc` does that on the x64/arm64
  ABIs (`alignof(max_align_t) == 16`) but **not on wasm**, where it is 8 - verified: 34 of 64 small
  `malloc`s come back 8-aligned. Under-aligned memory is UB for every heap-stored vector/matrix and
  makes AES-GCM reject its own target/AAD buffers. OxC3's three raw allocators therefore use
  `aligned_alloc(16, ...)` on web (`platforms/unix/uplatform.c`,
  `types/container/test/shared/basic_alloc.c`, `types/container/perf/exec/perf_main.c`); the
  contract is documented in `types/base/allocator.h`. **Consumers supplying their own allocator on
  web must do the same.** The AES kernels' alignment precondition is unchanged and still enforced on
  all platforms - encryption is verified against the full AES-128/256-GCM vector set under wasm64.
  A regression test (`Platform/Allocator` in platforms_interface) locks the invariant.

## Known issue: LLVM wasm64 -O3 miscompile

LLVM's wasm64 backend (emsdk 6.0.6, clang 24) miscompiles certain shift/xor reduction loops at
`-O3`: `I32x4_clmul64Fallback` returns all zeros. Facts established by bisection:

- x64 (gcc): correct at every -O level. wasm64: correct at -O0/-O1/-O2, wrong at -O3.
- Compile-side: an object compiled `-c -O3` stays wrong when linked `-O0`; an object compiled
  `-c -O2` stays right when linked `-O3` - binaryen/wasm-opt is exonerated.
- Not `__int128`-specific (a pure-U64 rewrite fails identically), not inlining-specific
  (`noinline` fails), not constant folding (volatile-laundered runtime inputs fail).
- A *minimal* standalone loop is correct; the miscompile needs the surrounding I32x4/U128
  conversion context, so the repro is the fallback function itself:

```c
// emcc -O3 -m64 repro.c -o repro.js && node repro.js   -> wrong (zeros)
// emcc -O2 -m64 repro.c -o repro.js && node repro.js   -> correct
// Take I32x4_clmul64Fallback + U128 helpers from include/types/math/u128_base.h with
// _ENABLE_SIMD=0 and compare against the vectors in test_types_math_vec4i.c.
```

Mitigation: the root CMakeLists compiles web Release at **-O2** (link-time optimization
unaffected). Re-test on emsdk upgrades by building the types_math suite at -O3; if it passes,
the pin can go. Worth reducing further and filing against LLVM.

Note: the DXC conan package builds with its own flags (default Release -O3 objects) and passes
its full corpus + oiSH tests under wasm64, so the miscompile evidently doesn't hit its code
paths; if DXC ever misbehaves on web only, rebuild it at -O2 first.

## Performance (measured 2026-08-07, 1 GiB workloads, `OxC3 profile`)

Three configurations, to separate "wasm is slower" from "we turned SIMD off":

| op | desktop SSE | desktop "scalar" | web | web vs desktop-scalar |
|---|---|---|---|---|
| memcpy   | 13.9 GB/s   | 13.9 GB/s | 5.11 GB/s  | 2.7x |
| crc32c   | 22.9 GB/s   | 3.27 GB/s | 2.29 GB/s  | 1.4x |
| fnv1a64  | 23.3 GB/s   | 3.28 GB/s | 2.29 GB/s  | 1.4x |
| cast     | -           | 14.1 ns/op| 17.2 ns/op | 1.2x |
| md5      | 668 MB/s    | 660 MB/s  | 311 MB/s   | 2.1x |
| rng      | 572 MB/s    | 549 MB/s  | 206 MB/s   | 2.7x |
| sha256   | 2352 MB/s   | 380 MB/s  | **27.5 MB/s** | **13.8x** |
| aes128   | 8472 MB/s   | 5.96 MB/s | **1.27 MB/s** | 4.7x |

Most ops sit at 1.2-2.7x, which is the ordinary wasm tax (bounds-checked linear memory, no native
codegen) and is not worth chasing. Two results are not ordinary:

**1. The `SIMD_NONE` path was never really scalar on desktop.** The non-SIMD x86 build still gets
`-mfpmath=sse -msse` (root CMakeLists), and gcc auto-vectorizes the fallbacks: disassembling
`Buffer_sha256Fallback` shows **138 xmm instructions** in the `EnableSIMD=OFF` build. On wasm clang
emits **0** v128 instructions in that same function - verified with `wasm-dis`, and still 0 when
`-msimd128` is forced (it emits ~20k v128 elsewhere in the module but never reaches the hash loop,
and no profile op moved by more than 3%). So web is the first platform to run these fallbacks
genuinely scalar, and SHA-256 pays 13.8x for it.

**2. AES without AES-NI is a cliff that exists on every platform.** Desktop drops from 8472 MB/s to
5.96 MB/s (1400x) the moment `EnableSIMD=OFF`; web then pays another 4.7x on top, landing at
**1.27 MB/s**. Encrypted oiCA/oiDL on web is therefore impractical for anything beyond small files
(a 10 MB archive takes ~8s). Plain (unencrypted) archives are unaffected.

Also measured: the `-O2` mitigation above costs **0-11%** (sha256 is identical at `-O3`, md5 is the
worst case), so it is cheap insurance rather than a real performance problem.

The fix for both is the same and is real work: a hand written **`SIMD_WASM` backend**
(`vec4_wasm.inc.h` plus the crypto `.inc.h` equivalents, using `wasm_simd128.h`), shaped like the
existing SSE/NEON ones. wasm has no AES/SHA/CRC instructions so it cannot reach hardware numbers,
but hand written v128 should move SHA-256 towards the ~380 MB/s that auto-vectorized C already
reaches, and a bit-sliced AES would beat 1.27 MB/s by a wide margin. Only worth doing if the web
target needs to hash or encrypt in bulk; the shader compiler path is DXC's own code and unaffected.

## Host crypto routing (-DEnableHostCrypto=ON, off by default)

wasm has no AES/SHA/CLMUL instructions, so OxC3's own kernels run at 1.27 MB/s (AES-128-GCM) and
27.5 MB/s (SHA-256) on the web. With this option `Buffer_sha256` and `Buffer_{encrypt,decrypt}Advanced`
hand the work to the host's hardware backed crypto instead, and fall back to the built in kernels
whenever that isn't possible. Output is identical either way; the `HostCrypto` test asserts the
digest matches `Buffer_sha256Fallback` byte for byte and that AES round trips.

```bash
python3 build_web.py --host_crypto        # or -DEnableHostCrypto=ON / -o "&:enableHostCrypto=True"
```

**How, and why it looks like this.** The web platform only exposes `crypto.subtle`, which is
asynchronous, and OxC3's crypto API is synchronous. The three ways to bridge that were measured:

| bridge | works with `-fwasm-exceptions` | browsers | cost |
|---|---|---|---|
| Asyncify | **no** (emcc refuses to mix) | all | `-fexceptions` instead: **+22% binary, -22% DXC speed** |
| JSPI | yes | **Chrome/Edge 137+ only** | module won't instantiate elsewhere |
| **SharedArrayBuffer + Atomics.wait** | yes | all (Safari 15.2+) | needs COOP/COEP + a Worker |

The third was chosen: it keeps `-fwasm-exceptions` (so DXC is untouched), keeps one binary, and
costs nothing when unused. The module posts the request to a helper worker and parks on
`Atomics.wait`; the helper awaits `crypto.subtle` and notifies. `postMessage` happens before the
wait, so the helper's event loop is free while this thread is blocked.

**Requirements at runtime** (all probed once, and simply not used when absent):
`crossOriginIsolated` (so, COOP/COEP headers), the module running in a **Worker** (browsers forbid
`Atomics.wait` on the main thread), and `crypto.subtle` (secure context only, so https/localhost).
node satisfies all of this on its main thread, which is how the test suite exercises it.

**Measured** (bridge microbenchmark, node/OpenSSL) at the chunk sizes `EncryptionStream` actually
uses; the bridge round trip itself costs 0.004 ms:

| chunk | SHA-256 | AES-128-GCM | vs OxC3 kernels |
|---|---|---|---|
| 64 KiB | 1391 MB/s | 1819 MB/s | 50x / 1400x |
| 1 MiB | 1712 MB/s | 3423 MB/s | 62x / 2700x |

**Staging.** crypto.subtle has no incremental API (`digest`/`encrypt` are one shot, with no
`update()` and no way to resume state), so a request cannot be split across several passes through a
fixed window: it has to be handed over whole. The staging buffer therefore starts at
`HOST_CRYPTO_STAGING_INIT` (1 MiB) and doubles on demand up to `HOST_CRYPTO_STAGING_MAX` (256 MiB),
past which the call falls back to OxC3's kernels. Growth is capped because a SharedArrayBuffer stays
allocated once grown. Typical `EncryptionStream` chunks never grow it. A single `Buffer_sha256` over
something larger than the ceiling (the 1 GiB `OxC3 profile sha256` benchmark, say) still takes the
software path by design.

That one shot restriction is deliberate for AEAD decryption, incidentally: streaming it would mean
handing back plaintext before the tag is verified. Framing data into independently authenticated
chunks is the sanctioned answer, which is exactly what `EncryptionStream` already does (derived IV
plus its own tag per chunk) - and why every streaming call maps cleanly onto one `subtle.encrypt`.

**Other behaviour.** AES key/IV generation stays in OxC3 (its own CSPRNG); only the GCM itself is
handed over. A tag mismatch comes back as a failure and the software path then rejects it for the
same reason, so authentication failures stay failures. `crypto.subtle` refuses views backed by a
SharedArrayBuffer, so the copy out of the staging buffer is mandatory rather than an oversight (and
is included in the measurements above); sharing the wasm heap directly via `-pthread` would not
avoid it.

**Conan footgun to fix.** Emscripten's exception mode is ABI, but it lives in profile `conf`, which
does not feed into `package_id`: switching it silently reuses a dxc binary built for the other mode
and only fails at link time with undefined `__resumeException`. The fix is to make the EH mode a
conan option that drives the flag in both the dxc and oxc3 recipes rather than a profile setting.

## Future work

- WebGPU graphics backend (then `EnableGraphics` on web, browser windowing via
  emscripten/html5.h in `src/platforms/web/webwindow.c`).
- `-pthread` build flavor (dxc package + engine + consumers must all agree).
- Browser test shell (MEMFS preload of packages/ + the shader corpus).
- Reduce + upstream the -O3 miscompile.
- `SIMD_WASM` backend (see Performance above): raises the floor for pages that can't be cross
  origin isolated, which is exactly where host crypto routing can't help.
- Grow the host crypto staging buffer on demand instead of falling back past 16 MiB.
- Make the emscripten exception mode a conan option so package_id tracks it.
