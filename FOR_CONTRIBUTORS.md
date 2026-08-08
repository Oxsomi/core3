# Contributing to OxC3

Thanks for considering a contribution. This document covers the legal terms (unchanged from before) and, new, the practical expectations that get a PR merged quickly.

## Legal / licensing terms

Pull requests must follow the code style of this repository and may be rejected for any reason by Oxsomi or the developers. Copyright has to be transferred from the developer to us and/or we should be allowed to distribute it with our own custom license. Copyright doesn't have to be forfeited as long as we're allowed to make modifications, redistribute and relicense it as we see fit. Oxsomi is under no obligation to provide payment for this, though could decide to do so if it's deemed critical or a major improvement by the team. External PRs require a signed CLA before merge.

## Before you start

- Read [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), especially the **error-handling idiom** section. PRs that use raw `return`s on failure paths, skip input validation, or invent a third error convention will be bounced on style alone.
- Read [docs/code_style.md](docs/code_style.md) for formatting (tabs, brace style, naming: `Module_functionName`, `EEnumName_Value`, `s_uccess`/`e_rr` are intentional).
- Check [STATUS.md](STATUS.md) so you're not building on a 📄-status feature, and update it in the same PR when your change moves a cell.

## PR checklist

1. **Builds green locally**: `python build.py -mode Debug -tests True` (and `Release`), on at least one platform; state which platforms you tested in the PR description. If you touched platform code, the per-OS backends must stay symmetrical: stub + `Error_unimplemented` beats silent divergence.
2. **Tests**: new behavior comes with tests in `src/<module>/test/`. Formats/compiler changes should add or update golden fixtures. Bug fixes add the regression case that would have caught them.
3. **No leaks**: run the debug build; the tracked allocator prints `Leaked N bytes in M allocations` with stacktraces at shutdown. A PR that introduces leaks in tests won't merge.
4. **Validate all inputs** in any new public function: null, bounds, enum `< _Count`, overflow via `types/math/type_cast_safe.h`. Error strings follow `"Function()::param reason"`.
5. **Both graphics backends or neither**: a feature landing in `graphics/generic` lands in `vulkan/generic` *and* `d3d12/generic` (or gates itself with a capability + clear error).
6. **Docs**: public API changes update the relevant `docs/*.md`; new formats need a spec doc + entry in `docs/id_registry.md` **with a Status line**.
7. **Commit hygiene**: imperative subject, explain *why* in the body; link issues. Keep unrelated reformatting out of functional PRs.

## Good first contributions (as of v3.2.103)

Real, wanted items: see the reviews/TODOs in tree for context:

- A generic hash map in `types/container` (unblocks debug-allocator lookup, command-list resource dedup).
- Fuzz targets (libFuzzer, behind a CMake flag) for oiCA/oiDL/oiSH/DDS readers.
- Unit tests for the API-independent graphics command recorder/validator.
- CLI: `-aes -` (stdin) / `-aes-env` / `-aes-file` key input.
- Touch-input documentation/mapping notes for Android.
- Software AES-GCM fallback (or explicit capability error) for CPUs without crypto extensions.

## Communication

- One feature/fix per PR. Open an issue first for anything touching public API shape, file formats, or the graphics spec: formats especially, since shipped bytes are forever.
- CI status: if CI is red on main (it can be during large refactors, see README), say so in your PR and include local test output instead.
