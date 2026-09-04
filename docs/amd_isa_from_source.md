# Building the AMD offline ISA tools from source — recon

> Can `amdllpc` and `amdgpu-dis` stop being Windows/Linux x64 prebuilts, so macOS and Android could build them too?

## TL;DR

- **`amdgpu-dis` is redundant for our use.** `amdllpc --filetype=asm` emits AMD ISA text directly, verified byte-for-byte identical in instruction stream. That drops a 20 MB tool and one subprocess per disassembly, on every platform, with no source build at all.
- **Building `amdllpc` from source is not a practical route.** The LLPC repository was archived 2025-09-15. It needs the AMDVLK multi-repo `repo` manifest and is "pretty much the same requirements as an LLVM build". The docs never mention macOS.
- **You cannot escape LLVM for ISA *text*.** Whatever compiles the shader, turning AMD ISA into text needs LLVM's AMDGPU disassembler. Mesa's ACO has no disassembler of its own; `aco_print_asm.cpp` calls `LLVMDisasmInstruction` or shells out to `clrxdisasm`, and CLRX stops at GCN/Vega so it cannot do gfx11/gfx12.
- A portable stack therefore needs **both** halves: ACO to compile and LLVM's AMDGPU MC/disassembler to print. The disassembler is the invariant piece (same ~20 MB either way, builds cleanly on macOS/Android) but it is useless on its own, so this is one project, not a part that can ship early.

## What the two binaries actually are

| | size | what it is |
|---|---|---|
| `amdllpc` | 114 MB | LLPC + AMD's LLVM fork (`LLPC::Version`, `.llpc_version` in the binary) |
| `amdgpu-dis` | 20 MB | LLVM MC + the AMDGPU target (`_GLOBAL__sub_I_AMDGPUBaseInfo.cpp` in the binary) |

They are both LLVM with the AMDGPU backend. AMD ships them as separate static binaries only because they come from different halves of its stack: `external/vulkan_offline/` versus `external/lc/` (LC = Lightning Compiler, the OpenCL one). Nothing about the design requires two tools, and `amdllpc` already links everything `amdgpu-dis` contains.

## The free win: drop `amdgpu-dis`

`amdllpc` exposes LLVM's own `--filetype=asm` ("Emit an assembly ('.s') file"), and `--emit-llvm` is documented as "Emit LLVM assembly *instead of AMD GPU ISA*" — ISA is the default output.

Verified against the current two-step path (`amdllpc` → ELF → `amdgpu-dis`):

- identical instruction stream on the same module
- works for 7/7 of the corpus modules that compile at all (the 8th, `atomic_f64`, fails identically on the ELF path: gfx1100 genuinely cannot select `BUFFER_ATOMIC_FADD f64`, so it is a target limitation, not an asm-mode one)
- the assembly form additionally carries the register stats inline (`.set _rgen_1.num_vgpr, 1`), which OxC3 currently has to parse back out of the disassembly

Two differences to weigh before switching:

1. The `.s` output has **no hex encodings**; `amdgpu-dis` prints `// 000000000000: 360200FF`. The committed ISA goldens contain those, so switching regenerates all of them.
2. It is assembler syntax (directives, `.Lfunc_end0`, `.size`), so `SpvISA_trimIsa` would need reworking against a different shape.

Worth doing on its own merits — smaller bundle, one less process spawn, stats for free — but it is a *separate* change from portability, and it costs a golden regeneration.

## Why building `amdllpc` is the wrong target

- **Archived.** [llpc was archived on 2025-09-15](https://github.com/GPUOpen-Drivers/llpc). A source build pins you to a tree that no longer takes updates.
- **Multi-repo.** [The amdllpc docs](https://github.com/GPUOpen-Drivers/llpc/blob/dev/llpc/docs/amdllpc.md) build it as part of AMDVLK: `repo init -m build_with_tools.xml`, `repo sync`, then `cmake --build xgl/builds/Release64 --target lgc amdllpc` with `-DVKI_BUILD_TOOLS=ON`. That is llpc + xgl + pal + an LLVM fork + glslang + SPIRV-Tools.
- **LLVM-sized.** The docs say it "pretty much has the same requirements as an LLVM build" — multi-GB, hours, per platform, per config.
- **No macOS story.** The documentation does not mention macOS at all; its runtime notes assume `LD_LIBRARY_PATH`.

## The irreducible piece

Whichever engine compiles the shader, **ISA text needs an AMDGPU disassembler**, and in practice that means LLVM:

- `src/amd/compiler/aco_print_asm.cpp` has exactly two paths, `print_asm_llvm` (guarded by `AMD_LLVM_AVAILABLE`) and `print_asm_clrx` which shells out to `clrxdisasm`. ACO ships no disassembler.
- [CLRadeonExtender](https://github.com/CLRX/CLRX-mirror) covers GCN 1.0/1.1/1.2/1.4 (Vega). It has no gfx11/gfx12, so it cannot serve RDNA3/RDNA4 — which is exactly the range OxC3's offline targets cover.

So printing is always **LLVM built with only the AMDGPU target and the MC/Disassembler libraries**. That is what `amdgpu-dis`'s 20 MB already is, and it builds on macOS and Android without drama. It is invariant across every choice of compiler, which is why it is worth naming separately, but see below: it does nothing until something produces a code object for it to print.

## What to actually do

There are two things here, not four, and only the first is independently shippable.

### 1. Drop `amdgpu-dis` via `--filetype=asm` (do this on its own merits)

Cheap, immediate, works on every platform, −20 MB, one less subprocess, register stats for free. It does **not** help macOS/Android by itself. It costs an ISA golden regeneration and a rework of `SpvISA_trimIsa` for assembler syntax.

### 2. The portable stack, which is one project and not two

Portability needs both halves of the pipeline buildable on the target:

| half | job | portable option | why not the other |
|---|---|---|---|
| A — compile | SPIR-V -> ISA machine code | **ACO** (Mesa, MIT, alive) | `amdllpc` is archived, multi-repo, LLVM-sized, no macOS story |
| B — print | machine code -> text | **LLVM AMDGPU MC/disassembler** (~20 MB) | ACO ships no disassembler; CLRX stops at Vega, no gfx11/gfx12 |

The reason B is worth calling out separately is that it is **invariant**: whichever way A goes, B is the same LLVM subset, so it can be started before the ACO question is settled. But B is **not shippable on its own** — a disassembler with nothing producing code objects does nothing. It only becomes useful paired with A.

Note the interaction with item 1: once `--filetype=asm` lands, the *shipping* stack no longer has a separate B at all, because `amdllpc` self-contains it. B is therefore purely an investment in the future ACO stack, not a fix to anything current.

Work for half A, for scale: today `src/amd/spirv2isa` folds into RADV and pulls `dep_libdrm_amdgpu`, LLVM, Vulkan WSI and runtime. A portable carve-out is spirv_to_nir -> NIR -> ACO -> amd/common + addrlib, none of which needs drm or a device — but the meson wiring assumes RADV, so this is real work rather than a packaging exercise.

### Not worth doing

Building `amdllpc` from source. Archived, multi-repo, LLVM-sized, and no macOS story. Note also that the shipped build accepts the gfx11xx/gfx12xx targets, a narrower range than ACO covers (GFX6-GFX12).

## Reality check on the value

Apple Silicon has no AMD GPUs and neither does Android, so the payoff is not "inspect AMD ISA on a Mac". It is:

- **CI parity** — the ISA corpus tests currently can only run on two platforms.
- **The browser story** — ACO is the only AMD ISA engine that can compile in wasm; `amdllpc` (a 114 MB native LLVM subprocess) never can. See [browser_wasm_plan.md](browser_wasm_plan.md).
- **Licensing** — the vendored blobs ship under their own EULA, whereas ACO is MIT and LLVM is Apache-2.0-with-exceptions, which is simpler to redistribute.

If only one thing is done: option 1, because it is nearly free and shrinks the problem. If portability is genuinely wanted: option 2, because the disassembler is the dependency neither engine can avoid.
