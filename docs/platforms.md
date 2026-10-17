# OxC3 platforms (id: 0x1C32)

OxC3 platforms is what differs per operating system: the program's entry point, memory, files, the environment,
windows, input, monitors, dynamic libraries and child processes. Everything above it (formats, graphics, the tools)
is written once against this layer.

This page is a map, not a reference: the API is the headers under `include/platforms/` (C++ wrappers in `file.hpp`,
`window.hpp` and `platform.hpp`). Which of these each platform supports is kept in one place, the
[Platforms table in STATUS.md](../STATUS.md#platforms).

## The platform

An application's entry point is `Platform_defineEntrypoint()`, which is `main` on most platforms and `android_main`
on Android, with `Platform_argc`, `Platform_argv` and `Platform_getData()` standing in for what each platform hands
it. The application calls `Platform_create` first, which also refuses a CPU without the SIMD level OxC3 was built
for, returns through `Platform_return` (Android has no exit code to return) and calls `Platform_cleanup` last. An
application whose entry point belongs to someone else (a Java or C# host) calls `Platform_create` the same way.

What the platform provides from then on:

- **The allocator.** Every allocation goes through it and is tracked, so the leaks left at exit are reported, in a
  debug build with where they were allocated. A custom allocator reports its own allocations to the same tracking.
- **The machine.** CPU vendor, features and topology (physical and logical cores, hybrid core counts, caches),
  and installed and available memory.
- **The environment.** Variables read as UTF-8 on every platform, Windows included, with typed readers that refuse
  a malformed value rather than reading it as a default. The C++ layer (`oxc::env`) reads any integer or float
  width with its range checked, comma separated lists, and a settings form that logs a malformed value by name. An empty variable is unset everywhere, since Windows cannot
  hold one. OxC3 reads the environment and never writes it.
- **The command line**, and the working and application directories.

## Files

Paths are OxC3's own, forward slashes on every platform, and resolve under one root: the working directory or the
application directory, chosen at `Platform_create`. A path escaping that root is refused, which is what makes it
safe to open a path that came out of a file.

There are two file systems behind one API:

- **Physical**: the files under that root. Not every platform has a writable one (web is read only).
- **Virtual**: files embedded in the executable (or the apk), under `//library/section/...`. The build packs a
  folder into an oiCA per section and links it in; the application loads a section when it needs it, which
  decrypts it into memory, and unloads it when it does not. Virtual files are read only. Libraries and sections
  let a large application load only what one part of it uses, and a library can depend on another's sections.

The library names `access`, `function` and `network` are reserved: for files the user picks outside the root, for
file systems an application mounts itself, and for network paths.

Every call that opens a file takes a timeout: how long a failed open is retried before it fails. It defaults to 0
everywhere, `FileHandle::open` and `FileStream::open` included, so a missing file fails at once. Pass a timeout only
to wait out another process holding the file; `U64_MAX` retries a file that never appears forever.

In CMake, a section is added with `add_virtual_files(TARGET t NAME section ROOT folder SELF dir)`, a dependency on
another target's sections with `add_virtual_dependencies`, and both are applied by `apply_dependencies`.
Dependencies may not overlap: two dependencies that both include a third cannot be combined.

## Windows and monitors

A `WindowManager` owns windows and is bound to the thread that created it; everything about a window happens on
that thread. There are two kinds of window:

- **Physical**: a native window the platform shows, which receives input and knows the monitors it is on.
- **Virtual**: a render target with no display behind it, for headless rendering, tests and servers. It behaves
  like a physical window to everything that renders into it.

A window hint asks for behavior (resizable, full screen, a CPU side buffer to write pixels into) and may be
ignored where it does not apply. Creating windows beyond the first is only possible on desktop platforms; elsewhere
a window fills what the platform already provides (the app's surface, a canvas).

A monitor describes what a physical window is shown on: its size in pixels and inches, orientation, refresh rate,
and its subpixel layout, for rendering that depends on it.

## Input

Input devices are a generic set of buttons and axes. A button's state says whether it is up or down and whether it
changed this update; an axis holds its value; and both raise events as they change. Keyboards and mice are created
by the window that receives them; a custom device uses the same interface.

Keyboard keys are physical positions (scan codes on a QWERTY layout), not characters: the key named W is where W
is on QWERTY, whatever the layout prints on it. `Keyboard_remap` gives the character the user's layout puts there,
for showing a key in a settings screen, and text entry goes through the window's type character callback, never
through keys.

Polling a device's state can miss a press shorter than a frame; the events do not.

## Dynamic libraries and processes

Loading a library and spawning a child process exist only where the platform allows them: `SUPPORTS_DYNAMIC_LINKING`
and `SUPPORTS_PROCESS` say whether they do. Sandboxed targets (web, mobile) have neither.

## Extended functions

A function whose name ends in `x` (`Log_debugx`, `Log_errorx`) is the extended form of one that takes an allocator:
it uses the platform's instead, for code that has no reason to choose.
