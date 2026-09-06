#!/usr/bin/env python3
# check_style.py
# Style checks (all files):
#   1.  No line longer than 128 columns (a tab advances to the next multiple of 4)
#   2.  No trailing tabs or spaces at end of line
#   3.  No spaces used for indentation (tabs only)
#   4.  No duplicate filenames anywhere in the scanned tree
#   5.  No non-ASCII bytes (UTF-8 / extended characters disallowed)
#   6.  File must end with exactly one newline (no missing, no extra blank lines)
#   7.  No tab in non-indentation position (mid-line tab used for alignment)
#   8.  No semicolon after closing brace of a function/class/struct/enum body
#   9.  #pragma once required in headers (not #ifndef guards)
#   10. OxC3 license header required at top of every file
#   10b.File-path comment required after license header (e.g. //types/container/generic/test.h)
#   11. No #include with absolute path (must be relative or <system>)
#   12. No consecutive blank lines (more than 1 in a row)
#   13. <windows.h> only inside _PLATFORM_TYPE == PLATFORM_WINDOWS guard
#       or inside a src/.../windows directory
#   14. POSIX headers (<unistd.h> <pthread.h> etc.) only inside
#       src/.../unix directories
#   15. No unmerged Git conflict markers (<<<<<<< / ======= / >>>>>>>)
#   16. Aligned case tables stay aligned: once any line of a contiguous `case X: stmt` run pads
#       with 2+ spaces, every statement (and every trailing break;) in the run must share one
#       column; single-space runs are the unaligned style and stay exempt
#
# Banned symbols (with per-path allow-lists):
#   B1.  malloc / free / realloc / calloc
#        Allowed in: src/platforms/unix, src/types/container/platforms/unix, src/platforms/windows,
#                    src/types/container/perf/exec/perf_main.c,
#                    src/types/container/test/shared
#   B2.  new / delete  (fully banned)
#   B3.  printf / fprintf / vprintf / vfprintf / sprintf / vsprintf / vsnprintf
#        Allowed in: src/platforms/unix/ulog.c,
#                    src/types/base/test/shared/shared.c,
#                    src/types/container/string.c
#   B4.  assert(
#        Allowed in: include/types/base/lock.h
#   B5.  gets / scanf / strncpy / strncat  (always banned)
#   B6.  memcpy / memmove
#        Allowed in: src/types/base/buffer_base.c
#        Use Buffer_memcpy / Buffer_memmove
#   B7.  memset
#        Allowed in: src/types/base/buffer_base.c
#        Use Buffer_setBitRange / Buffer_unsetBitRange / Buffer_clearAllSecure
#   B8.  memcmp
#        Allowed in: src/types/base/buffer_base.c
#        Use Buffer_cmp
#   B9.  fopen / fclose / fread / fwrite / fseek
#        Allowed in: src/types/container/perf/container_perf.c,
#                    test_audio_functional_main.c
#        Use File_openStream / FileHandle_openStream
#   B10. strtol / strtod / atoi / atof  (always banned)
#        Use CharString_createDec / createHex / format
#   B11. strlen  (always banned)
#        Use CharString_calcStrLen or CharString_createRefCStrConst
#   B12. strcmp / strstr  (always banned)
#        Use CharString_equalsStringSensitive / Buffer_cmp
#   B13. time() / clock()  (always banned)
#        Use Time_now() / Time_elapsed()
#   B14. system()  (always banned)
#   B15. rand() / srand()  (always banned)
#        Use Random_seed / Random_sample
#
# Informational (no error, printed at end):
#   T.  TODO/FIXME/HACK/NOTE/XXX list with file + line
#
# Usage:
#   python check_style.py <root_dir> [--max-line-length N]
#                                    [--max-warnings-per-file N]
#                                    [--no-todo-report]
#
# Exits with code 0 on success, 1 if any violation is found.

from __future__ import annotations        # PEP 585/604 style hints must not be evaluated on older Python (3.7/3.8)

import os
import re
import sys
import argparse
from collections import defaultdict

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

C_EXTENSIONS      = {'.c', '.cpp', '.hpp', '.h', '.inc'}
HEADER_EXTENSIONS = {'.hpp', '.h'}
ALL_EXTENSIONS    = C_EXTENSIONS

# Files whose full name contains these substrings are include-fragments
# (e.g. foo.inc.h, bar.inc.hpp).  os.path.splitext sees only the last
# extension, so they would otherwise be misclassified as full headers.
# Include-fragments are not required to have #pragma once.
INC_FRAGMENT_MARKERS = ('.inc.h', '.inc.hpp')

MAX_LINE_LENGTH       = 128
TAB_WIDTH             = 4


def prune_dirs(dirpath: str, dirs: list) -> None:
    """Drop directories neither the checker nor the fixer should walk into.

    Both the scanner and the in-place fixer use this, so the fixer can never
    rewrite a directory the checker doesn't even look at, vendored clones most
    of all: rewriting those would produce a huge diff against upstream.

    Nested checkouts (a dependency fork cloned inside the tree, e.g. spirv-reflect or mesa) are
    recognised by their own .git rather than by name, so somebody cloning the next one doesn't have
    to remember to come back here. Their sources are not ours and would never satisfy this checker.
    Modifying dirs[:] in-place is the os.walk-documented way to control recursion.
    """

    dirs[:] = [
        d for d in dirs
        if not d.startswith('.') and d not in ('build', 'VULKAN_SDK', '__pycache__')
        and not os.path.exists(os.path.join(dirpath, d, '.git'))
    ]


def visual_length(line: str, tab_width: int = TAB_WIDTH) -> int:
    """Length of a line in columns, where a tab advances to the next tab stop.

    A tab is up to `tab_width` spaces wide, not one character, so counting raw
    characters lets a deeply indented line sit far past the limit while still
    passing.  Measuring in columns is what the limit actually means.
    """

    col = 0

    for ch in line:
        col = (col // tab_width + 1) * tab_width if ch == '	' else col + 1

    return col

MAX_WARNINGS_PER_FILE = 20

# ---------------------------------------------------------------------------
# License header, canonical text (stripped of comment markers)
# ---------------------------------------------------------------------------
LICENSE_LINES = [
    "OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.",
    "Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)",
    "",
    "This program is free software: you can redistribute it and/or modify",
    "it under the terms of the GNU General Public License as published by",
    "the Free Software Foundation, either version 3 of the License, or",
    "(at your option) any later version.",
    "",
    "This program is distributed in the hope that it will be useful,",
    "but WITHOUT ANY WARRANTY; without even the implied warranty of",
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the",
    "GNU General Public License for more details.",
    "",
    "You should have received a copy of the GNU General Public License",
    "along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.",
    "Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.",
    "To prevent this a separate license will have to be requested at contact@osomi.net for a premium;",
    "This is called dual licensing.",
]

# ---------------------------------------------------------------------------
# Banned-symbol table
#
# Each entry:
#   pattern    : compiled regex matching the banned call in source text
#   message    : human-readable description ("{}" is replaced by matched token)
#   allow_paths: tuple of forward-slash substrings; file is exempt if its
#                relative path contains ANY of them.  () = banned everywhere.
# ---------------------------------------------------------------------------

BANNED_SYMBOLS: list[dict] = [
    # B1, raw heap allocators
    {
        "pattern": re.compile(r'(?<!->)(?<!\.)\b(malloc|free|realloc|calloc)\s*\('),
        "message": "raw allocator '{}', use OxC3 allocator wrappers",
        "allow_paths": (
            "src/platforms/unix",
            "src/platforms/windows",
            "src/types/container/platforms/unix/ulog.c",
            "src/types/container/perf/exec/perf_main.c",
            "src/types/container/test/shared",
        ),
    },
    # B2, C++ new / delete (fully banned)
    {
        "pattern": re.compile(r'\bnew\s+(?:[A-Z(\[]|new\b)|\bdelete(?:\[\])?\s+[^;=\s]'),
        "message": "C++ '{}' operator, forbidden (use OxC3 allocators)",
        "allow_paths": (
            "src/shader_compiler/compiler.cpp",
        ),
        # C++ new/delete cannot appear in a C translation unit, so a hit in a .c file is always a false
        # positive. In practice it is JavaScript inside EM_JS bodies (new Uint8ClampedArray, new Worker),
        # which this rule can't tell from C++. Scoping beats listing every web file that embeds JS.
        "extensions": (".cpp", ".hpp"),
    },
    # B3, printf family
    {
        "pattern": re.compile(r'\b(v?f?printf|v?s?n?printf|vsnprintf)\s*\('),
        "message": "'{}', use CharString_format instead",
        "allow_paths": (
            "src/types/container/platforms/unix/ulog.c",
            "src/types/base/test/shared/shared.c",
            "src/types/container/string.c",
            "src/platforms/unix/ufile.c",
            "src/platforms/test/functional/test_platforms_functional_input.c",
            # F32x4x4_format is a debug dump of 16 floats. OxC3_types_math sits below OxC3_types_container,
            # so CharString_format (the alternative this rule points at) isn't reachable from it.
            "src/types/math/mat.c",
            # The node test runner writes a machine readable protocol on stdout (OXC3_TEST_BEGIN,
            # RUN/PASS/FAIL per suite, OXC3_TEST_END) that the build scripts parse to decide the result.
            # Log_* would decorate those lines with a level prefix and ANSI colour (see FONT_GREEN in
            # ulog.c), so the reporter has to write them undecorated.
            "src/test/web/wtest_main.c",
        ),
    },
    # B4, assert
    {
        "pattern": re.compile(r'\bassert\s*\('),
        "message": "'assert(', forbidden; use OxC3 assertion macros",
        "allow_paths": ("include/types/base/lock.h",),
    },
    # B5, always-banned unsafe C string functions
    {
        "pattern": re.compile(r'\b(gets|scanf|strncpy|strncat)\s*\('),
        "message": "unsafe function '{}', always forbidden",
        "allow_paths": (),
    },
    # B6, memcpy / memmove
    {
        "pattern": re.compile(r'\b(memcpy|memmove)\s*\('),
        "message": "'{}', use Buffer_memcpy / Buffer_memmove",
        "allow_paths": ("src/types/base/buffer_base.c",),
    },
    # B7, memset
    {
        "pattern": re.compile(r'\bmemset\s*\('),
        "message": "'memset', use Buffer_setBitRange / Buffer_unsetBitRange / Buffer_clearAllSecure",
        "allow_paths": ("src/types/base/buffer_base.c",),
    },
    # B8, memcmp
    {
        "pattern": re.compile(r'\bmemcmp\s*\('),
        "message": "'memcmp', use Buffer_cmp",
        "allow_paths": ("src/types/base/buffer_base.c",),
    },
    # B9, CRT file I/O
    {
        "pattern": re.compile(r'\b(fopen|fclose|fread|fwrite|fseek)\s*\('),
        "message": "'{}', use File_openStream / FileHandle_openStream",
        "allow_paths": (
            "src/types/container/perf/container_perf.c",
            "test_audio_functional_main.c",
        ),
    },
    # B10, string-to-number conversions
    {
        "pattern": re.compile(r'\b(strtol|strtod|atoi|atof)\s*\('),
        "message": "'{}', use CharCharString_createDec / createHex / format / parseU64",
        "allow_paths": (),
    },
    # B11, strlen
    {
        "pattern": re.compile(r'\bstrlen\s*\('),
        "message": "'strlen', use CharString_calcStrLen or CharString_createRefCStrConst -> CharString_length",
        "allow_paths": (),
    },
    # B12, strcmp / strstr
    {
        "pattern": re.compile(r'\b(strcmp|strstr)\s*\('),
        "message": "'{}', use CharString_equalsStringSensitive or Buffer_cmp on CharString_bufferConst",
        "allow_paths": (),
    },
    # B13, C time functions
    {
        "pattern": re.compile(r'\b(time|clock)\s*\('),
        "message": "'{}()', use Time_now() / Time_elapsed()",
        "allow_paths": (),
    },
    # B14, system()
    {
        "pattern": re.compile(r'\bsystem\s*\('),
        "message": "'system()', always forbidden",
        "allow_paths": (
            # Functional tests shell out to set up window-manager / input state before asserting on it.
            "src/platforms/test/functional/",
        ),
    },
    # B15, rand / srand
    {
        "pattern": re.compile(r'\b(rand|srand)\s*\('),
        "message": "'{}()', use Random_seed / Random_sample",
        "allow_paths": (),
    },
]

# ---------------------------------------------------------------------------
# POSIX headers banned outside src/.../unix directories
# ---------------------------------------------------------------------------
POSIX_HEADERS = {
    "unistd.h", "pthread.h", "sys/types.h", "sys/stat.h", "sys/socket.h",
    "sys/mman.h", "sys/wait.h", "sys/ioctl.h", "netinet/in.h", "arpa/inet.h",
    "fcntl.h", "dirent.h", "dlfcn.h", "termios.h",
}
_POSIX_INCLUDE_RE = re.compile(r'^\s*#\s*include\s+<([^>]+)>')

# windows.h include line detector
_WINDOWS_H_RE     = re.compile(r'^\s*#\s*include\s+<windows\.h>')
# platform guard that legitimises windows.h in non-windows-folder files
_WIN_GUARD_RE     = re.compile(
    r'#\s*if.*\b_PLATFORM_TYPE\s*==\s*PLATFORM_WINDOWS\b'
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _norm(p: str) -> str:
    """Normalise path separators to forward slashes."""
    return p.replace(os.sep, '/')


_COMMENT_PREFIX = re.compile(
    r'^\s*(?://\s?|#\s?|/\*\s?|\*\s?|/\*\*\s?)?(.*?)\s*(?:\*/\s*)?$'
)

def _strip_comment(line: str) -> str:
    m = _COMMENT_PREFIX.match(line)
    return m.group(1) if m else line.strip()


def check_license(lines: list[str]) -> str | None:
    """Return None if the OxC3 license header is present, else an error string."""
    candidates = [_strip_comment(l) for l in lines[:30]]
    first = LICENSE_LINES[0]
    try:
        start = candidates.index(first)
    except ValueError:
        return "missing license header (first line not found)"
    for i, expected in enumerate(LICENSE_LINES):
        idx = start + i
        if idx >= len(candidates):
            return f"license header truncated at line {i + 1} of expected text"
        actual = candidates[idx].strip()
        if actual != expected:
            return (
                f"license header mismatch at expected line {i + 1}:\n"
                f"    expected : {expected!r}\n"
                f"    got      : {actual!r}"
            )
    return None


def _license_end_line(lines: list[str]) -> int:
    """
    Return the 0-based index of the last line of the license block, or -1
    if the license header cannot be located.

    Strategy: find the line containing the first LICENSE_LINES entry in
    the first 30 stripped lines, then advance by len(LICENSE_LINES) - 1.
    We also skip over any closing comment marker (e.g. ' */') that
    immediately follows.
    """
    candidates = [_strip_comment(l) for l in lines[:30]]
    first = LICENSE_LINES[0]
    try:
        start = candidates.index(first)
    except ValueError:
        return -1

    # The last *content* line of the license block
    last_content = start + len(LICENSE_LINES) - 1

    # Advance past a closing comment marker that may sit on the very next line
    # (handles the '  */' or ' */' line that ends a /* ... */ block).
    probe = last_content + 1
    if probe < len(lines) and re.match(r'^\s*\*/\s*$', lines[probe]):
        return probe

    return last_content


_FILE_PATH_COMMENT_RE = re.compile(r'^//\S')   # starts with // then non-space

def _expected_file_comment(rel: str) -> str:
    """
    Derive the expected file-path comment text from the file's relative path.

    The comment is   //<path-without-leading-src/-or-include/>
    e.g.
      rel = "src/types/container/test.c"     -> "//types/container/test.c"
      rel = "include/types/base/lock.h"      -> "//types/base/lock.h"
      rel = "types/container/test.c"         -> "//types/container/test.c"
    """
    stripped = rel
    for prefix in ("src/", "include/"):
        if stripped.startswith(prefix):
            stripped = stripped[len(prefix):]
            break
    return "//" + stripped


def check_file_path_comment(lines: list[str], rel: str) -> str | None:
    """
    Check 10b: after the license block there must be exactly one blank line
    followed by a comment of the form //<relative-path> (no space after //).

    Returns None on success, or an error string describing the problem.
    """
    end = _license_end_line(lines)
    if end < 0:
        # License itself is missing; check_license will already report that.
        return None

    expected = _expected_file_comment(rel)

    # Collect lines after the license block
    after = lines[end + 1:]

    # We expect: one blank line, then the path comment.
    # Be tolerant of the blank line being absent (report a clear message).
    if len(after) == 0:
        return f"missing file-path comment after license; expected '{expected}' on the line after a blank line"

    # Skip exactly one blank line
    if after[0].strip() == '':
        # Good — blank separator present.  Now check the comment line.
        if len(after) < 2:
            return (
                f"missing file-path comment after license blank line; "
                f"expected '{expected}'"
            )
        comment_line = after[1].rstrip('\r\n')
        if comment_line != expected:
            if not _FILE_PATH_COMMENT_RE.match(comment_line):
                return (
                    f"expected file-path comment '{expected}' after license "
                    f"blank line, but got: {comment_line!r}"
                )
            # It looks like a path comment but doesn't match — report the mismatch.
            return (
                f"file-path comment mismatch:\n"
                f"    expected : {expected!r}\n"
                f"    got      : {comment_line!r}"
            )
    else:
        # No blank line — check if the very first line after the license IS
        # the path comment (missing blank line) or something else entirely.
        comment_line = after[0].rstrip('\r\n')
        if comment_line == expected:
            return (
                f"missing blank line between license block and "
                f"file-path comment '{expected}'"
            )
        return (
            f"expected blank line then file-path comment '{expected}' "
            f"after license block, but got: {comment_line!r}"
        )

    return None


_BARE_BRACE_SEMI    = re.compile(r'^\s*\}\s*;\s*$')
_ABS_INCLUDE        = re.compile(r'^\s*#\s*include\s+"(/[^"]+)"')
_TODO_RE            = re.compile(r'\b(TODO|FIXME|HACK|NOTE|XXX)\b', re.IGNORECASE)
# Exactly 7 '<', '=' or '>' at the start of a line (Git conflict markers).
# The lookahead ensures '=======' is not a substring of a longer run.
_CONFLICT_MARKER_RE = re.compile(r'^(<{7}|>{7}|={7})(?= |$)')

# ---------------------------------------------------------------------------
# Per-file checker
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Rule 16: aligned case tables
# ---------------------------------------------------------------------------

_CASE_LINE  = re.compile(r'^(	+)(case [^:]*?:|default:)( +)(\S.*)$')
_BREAK_TAIL = re.compile(r'^(.*?\S)( +)(break;)$')


def check_case_alignment(lines: list[str]) -> list[str]:
    """A contiguous run of `case X: stmt` lines becomes an aligned table once any line pads its
    statement, or its trailing break;, with two or more spaces; the whole run must then share one
    statement column and one break; column. A single space everywhere is the unaligned style and
    is exempt. Any other line ends the run, so sub-blocks can choose their own columns, and a
    renamed label can no longer silently shear one row out of a table.
    """

    issues: list[str] = []
    run: list[dict] = []
    run_indent: str | None = None

    def flush() -> None:

        nonlocal run, run_indent

        if len(run) >= 2:

            stmt_intent = any(r['stmt_pad'] >= 2 for r in run if r['stmt_col'] is not None)
            brk_intent  = any(r['brk_pad'] >= 2 for r in run if r['brk_col'] is not None)

            stmt_cols = sorted({r['stmt_col'] for r in run if r['stmt_col'] is not None})
            brk_cols  = sorted({r['brk_col'] for r in run if r['brk_col'] is not None})

            span = f"lines {run[0]['lineno']}-{run[-1]['lineno']}"

            if stmt_intent and len(stmt_cols) > 1:
                issues.append(
                    f"  {span}: aligned case table drifted, statements start at "
                    f"columns {', '.join(str(c) for c in stmt_cols)}"
                )

            if brk_intent and len(brk_cols) > 1:
                issues.append(
                    f"  {span}: aligned case table drifted, break; sits at "
                    f"columns {', '.join(str(c) for c in brk_cols)}"
                )

        run = []
        run_indent = None

    for lineno, line in enumerate(lines, start=1):

        m = _CASE_LINE.match(line)

        if not m or (run_indent is not None and m.group(1) != run_indent):
            flush()
            if not m:
                continue

        indent, label, pad, content = m.groups()
        label_end = visual_length(indent + label)

        entry = {
            'lineno': lineno, 'stmt_col': None, 'stmt_pad': 0, 'brk_col': None, 'brk_pad': 0,
        }

        if content == 'break;':
            entry['brk_col'] = label_end + len(pad) + 1
            entry['brk_pad'] = len(pad)
        else:
            entry['stmt_col'] = label_end + len(pad) + 1
            entry['stmt_pad'] = len(pad)
            tail = _BREAK_TAIL.match(content)
            if tail:
                entry['brk_col'] = entry['stmt_col'] + len(tail.group(1)) + len(tail.group(2))
                entry['brk_pad'] = len(tail.group(2))

        run.append(entry)
        run_indent = indent

    flush()
    return issues



def check_file(
    path: str,
    rel: str,
    max_line_len: int,
    max_warn: int,
    is_header: bool,
    collect_todos: bool,
) -> tuple[list[str], list[tuple[int, str, str]]]:
    violations: list[str] = []
    todos: list[tuple[int, str, str]] = []

    try:
        with open(path, 'rb') as f:
            raw = f.read()
    except OSError as e:
        return [f"  ERROR: cannot read file: {e}"], []

    # ── 5. Non-ASCII bytes ────────────────────────────────────────────────
    raw_lines = raw.split(b'\n')
    for lineno, raw_line in enumerate(raw_lines, start=1):
        for bytepos, byte in enumerate(raw_line):
            if byte > 0x7F:
                violations.append(
                    f"  line {lineno}, col {bytepos + 1}: "
                    f"non-ASCII byte 0x{byte:02X} (UTF-8/extended chars disallowed)"
                )
                if len(violations) >= max_warn:
                    violations.append(
                        "  ... (warning limit reached, further violations omitted)"
                    )
                    return violations, todos

    # ── 6. EOF newline ────────────────────────────────────────────────────
    if len(raw) == 0:
        violations.append("  file is empty")
        return violations, todos
    if raw[-1:] != b'\n':
        violations.append("  file does not end with a newline")
    elif raw[-2:] == b'\n\n':
        violations.append("  file ends with more than one newline (extra blank line at EOF)")

    text  = raw.decode('ascii', errors='replace')
    lines = text.splitlines()

    def add(msg: str) -> bool:
        violations.append(msg)
        if len(violations) >= max_warn:
            violations.append(
                "  ... (warning limit reached, further violations omitted)"
            )
            return True
        return False

    # ── 10. License header ────────────────────────────────────────────────
    lic_err = check_license(lines)
    if lic_err:
        if add(f"  {lic_err}"):
            return violations, todos

    # ── 10b. File-path comment after license ──────────────────────────────
    fp_err = check_file_path_comment(lines, rel)
    if fp_err:
        if add(f"  {fp_err}"):
            return violations, todos

    # ── 9. #pragma once (headers only) ───────────────────────────────────
    if is_header:
        has_pragma = any(re.match(r'^\s*#\s*pragma\s+once\b', l) for l in lines)
        if not has_pragma:
            if add("  missing #pragma once (use #pragma once, not #ifndef guards)"):
                return violations, todos
        # Only flag a #ifndef as an include guard when it is immediately
        # followed (on the next non-blank line) by #define <same-macro>.
        # A plain feature-flag #ifndef (e.g. #ifndef GRAPHICS_API_DYNAMIC)
        # does NOT have that #define and must not be flagged.
        def _has_include_guard(ls):
            for gi, gl in enumerate(ls):
                gm = re.match(r'^\s*#\s*ifndef\s+(\w+)', gl)
                if not gm:
                    continue
                macro = gm.group(1)
                for gj in range(gi + 1, min(gi + 3, len(ls))):
                    if ls[gj].strip() == '':
                        continue
                    if re.match(r'^\s*#\s*define\s+' + re.escape(macro) + r'\s*$', ls[gj]):
                        return True
                    break
            return False
        if _has_include_guard(lines):
            if add("  #ifndef include guard found, use #pragma once instead"):
                return violations, todos

    # ── 13. windows.h guard check (file-level) ───────────────────────────
    # Files inside a .../windows/... directory are unconditionally allowed.
    # All other files may only include <windows.h> when it appears after a
    # #if _PLATFORM_TYPE == PLATFORM_WINDOWS line (anywhere above it).
    in_windows_dir = "/windows/" in rel or rel.startswith("windows/")
    if not in_windows_dir:
        # Scan all lines; track whether we have seen the platform guard
        # before each windows.h include.
        guard_seen = False
        for lineno, line in enumerate(lines, start=1):
            if _WIN_GUARD_RE.search(line):
                guard_seen = True
            if _WINDOWS_H_RE.match(line):
                if not guard_seen:
                    if add(
                        f"  line {lineno}: <windows.h> included outside a "
                        f"#if _PLATFORM_TYPE == PLATFORM_WINDOWS guard "
                        f"and outside a windows/ directory"
                    ):
                        return violations, todos

    # ── 14. POSIX headers outside unix directories ────────────────────────
    in_unix_dir  = "/unix/" in rel or rel.startswith("unix/")
    in_unix_dir |= "/linux/" in rel or rel.startswith("linux/")
    in_unix_dir |= "/android/" in rel or rel.startswith("android/")
    in_unix_dir |= "/osx/" in rel or rel.startswith("osx/")
    in_unix_dir |= "/web/" in rel or rel.startswith("web/")     # emscripten is a unix flavor (MEMFS/NODERAWFS)
    if not in_unix_dir:
        for lineno, line in enumerate(lines, start=1):
            m = _POSIX_INCLUDE_RE.match(line)
            if m:
                header = m.group(1)
                # Match on the last component (e.g. sys/types.h -> sys/types.h)
                if header in POSIX_HEADERS:
                    if add(
                        f"  line {lineno}: POSIX header <{header}> included outside "
                        f"a unix/ directory"
                    ):
                        return violations, todos

    # ── 15. Unmerged Git conflict markers ────────────────────────────────
    for lineno, line in enumerate(lines, start=1):
        if _CONFLICT_MARKER_RE.match(line):
            if add(f"  line {lineno}: unmerged Git conflict marker '{line[:7]}'"):
                return violations, todos

    # Pre-compute which banned-symbol rules apply to this file
    active_rules = [
        rule for rule in BANNED_SYMBOLS
        if not any(ap in rel for ap in rule["allow_paths"])
        and (not rule.get("extensions") or rel.endswith(rule["extensions"]))
    ]

    # ── Per-line checks ───────────────────────────────────────────────────
    consecutive_blank = 0

    for lineno, line in enumerate(lines, start=1):

        # ── 12. Consecutive blank lines ───────────────────────────────────
        if line.strip() == "":
            consecutive_blank += 1
            if consecutive_blank > 1:
                if add(f"  line {lineno}: multiple consecutive blank lines"):
                    return violations, todos
            continue
        else:
            consecutive_blank = 0

        # ── TODO collector (informational) ────────────────────────────────
        if collect_todos:
            m = _TODO_RE.search(line)
            if m:
                todos.append((lineno, m.group(1).upper(), line.strip()))

        # ── 1. Line length ────────────────────────────────────────────────
        line_cols = visual_length(line)
        if line_cols > max_line_len:
            if add(f"  line {lineno}, col {max_line_len + 1}: "
                   f"line too long ({line_cols} > {max_line_len} columns, "
                   f"tabs counted to the next multiple of {TAB_WIDTH})"):
                return violations, todos

        # ── 2. Trailing whitespace ────────────────────────────────────────
        rstripped = line.rstrip('\t ')
        if len(rstripped) != len(line):
            trailing_char = 'tab' if line[-1] == '\t' else 'space'
            if add(f"  line {lineno}, col {len(rstripped) + 1}: "
                   f"trailing {trailing_char}(s) at end of line"):
                return violations, todos

        # ── 3. Spaces used for indentation ────────────────────────────────
        indent_len   = len(line) - len(line.lstrip('\t '))
        indent_chars = line[:indent_len]
        if ' ' in indent_chars:
            col = indent_chars.index(' ') + 1
            if add(f"  line {lineno}, col {col}: "
                   f"space used for indentation (use tabs)"):
                return violations, todos

        # ── 7. Mid-line tab ───────────────────────────────────────────────
        rest    = line[indent_len:]
        tab_pos = rest.find('\t')
        if tab_pos != -1:
            col = indent_len + tab_pos + 1
            if add(f"  line {lineno}, col {col}: "
                   f"tab in non-indentation position (mid-line alignment tab)"):
                return violations, todos


        # ── 11. Absolute #include path ────────────────────────────────────
        m = _ABS_INCLUDE.match(line)
        if m:
            if add(f"  line {lineno}: absolute #include path \"{m.group(1)}\" "
                   f"\u2014 use relative paths or <system> headers only"):
                return violations, todos

        # ── Banned symbols (skip pure comment lines & inline comments) ────
        stripped = line.lstrip()
        is_comment = (
            stripped.startswith('//')
            or stripped.startswith('*')
            or stripped.startswith('/*')
        )
        if not is_comment:
            # Strip trailing inline // comment so that words inside it
            # (e.g. 'new view ids') don't trigger banned-symbol checks.
            code_part = line.split('//')[0]
            for rule in active_rules:
                m = rule["pattern"].search(code_part)
                if m:
                    symbol = m.group(0).rstrip('(').strip()
                    if add(f"  line {lineno}, col {m.start() + 1}: "
                           f"{rule['message'].format(symbol)}"):
                        return violations, todos

    # ── 16. Aligned case tables ─────────────────────────────────────────
    for msg in check_case_alignment(lines):
        if add(msg):
            return violations, todos

    return violations, todos


# ---------------------------------------------------------------------------
# Indentation fixer
# ---------------------------------------------------------------------------

def fix_indentation(root: str) -> tuple[int, int]:
    """
    Walk *root* and rewrite files in-place:
      - Replace every run of 4 leading spaces on a line with one tab
        (applied repeatedly until no leading-space run of 4 remains).
      - Replace any tab that appears after the leading-whitespace indent
        region (mid-line tab) with spaces up to the next tab stop.

    Skips files that contain non-ASCII bytes (they would need binary-safe
    handling and are already flagged by check 5).

    Returns (files_fixed, files_skipped).
    """
    fixed = 0
    skipped = 0

    for dirpath, dirs, filenames in os.walk(root):
        prune_dirs(dirpath, dirs)
        for fname in filenames:
            ext = os.path.splitext(fname)[1].lower()
            if ext not in ALL_EXTENSIONS:
                continue
            full = os.path.join(dirpath, fname)
            raw = open(full, 'rb').read()

            # Skip files with non-ASCII bytes
            if any(b > 0x7F for b in raw):
                skipped += 1
                continue

            original = raw.decode('ascii')
            out_lines: list[str] = []
            changed = False

            for line in original.splitlines(keepends=True):
                # Preserve the line ending
                ending = ''
                body   = line
                if line.endswith('\r\n'):
                    ending, body = '\r\n', line[:-2]
                elif line.endswith('\n'):
                    ending, body = '\n',   line[:-1]
                elif line.endswith('\r'):
                    ending, body = '\r',   line[:-1]

                # ── Step 1: leading 4-spaces → tab ───────────────────────
                new_body = body
                while True:
                    # Count existing leading tabs then look for 4 spaces
                    leading_tabs = len(new_body) - len(new_body.lstrip('\t'))
                    rest         = new_body[leading_tabs:]
                    if rest.startswith('    '):
                        new_body = '\t' * leading_tabs + '\t' + rest[4:]
                    else:
                        break

                # Step 2: mid-line tabs -> spaces up to the next tab stop.
                # A tab is *up to* tab_width spaces: it advances to the next
                # multiple of tab_width, so expanding every one to a flat 4
                # spaces shifts whatever the tab was aligning.  Expanding by
                # column preserves that alignment.
                indent_len = len(new_body) - len(new_body.lstrip('\t'))
                indent     = new_body[:indent_len]
                rest       = new_body[indent_len:]

                if '\t' in rest:
                    col      = indent_len * TAB_WIDTH        # the indent is all tabs
                    expanded = []
                    for ch in rest:
                        if ch == '\t':
                            pad = TAB_WIDTH - (col % TAB_WIDTH)
                            expanded.append(' ' * pad)
                            col += pad
                        else:
                            expanded.append(ch)
                            col += 1
                    rest = ''.join(expanded)

                new_body   = indent + rest

                if new_body != body:
                    changed = True
                out_lines.append(new_body + ending)

            if changed:
                open(full, 'w', newline='').write(''.join(out_lines))
                fixed += 1

    return fixed, skipped



# ---------------------------------------------------------------------------
# Embedded shader header size check
#
# The .hlsli files under include/shader_compiler/shaders are #included straight
# into raw string literals in compiler.cpp, and MSVC caps a single string
# literal at 16380 bytes (C2026).  clang and gcc have no such limit, so an
# oversized header builds fine everywhere except the MSVC CI leg, which is a
# miserable way to find out.  A header past the cap is split by closing the raw
# string and reopening it (see extension.CoopVec.hlsli); adjacent literals
# concatenate, so the embedded content is unchanged.
# ---------------------------------------------------------------------------

MSVC_STRING_LITERAL_MAX = 16380

SHADER_INCLUDE_DIR = 'include/shader_compiler/shaders'

def check_embedded_shader_headers(root: str) -> int:

    shader_dir = os.path.join(root, *SHADER_INCLUDE_DIR.split('/'))

    if not os.path.isdir(shader_dir):
        return 0

    bad = 0

    for fname in sorted(os.listdir(shader_dir)):

        if not fname.endswith('.hlsli'):
            continue

        path = os.path.join(shader_dir, fname)
        text = open(path, encoding='utf-8').read()

        # Each chunk is the text between a R"( opener and its )" closer; a file
        # that never opens one isn't embedded this way and has nothing to check.

        chunks = re.findall(r'R"\((.*?)\)"', text, re.DOTALL)

        for i, chunk in enumerate(chunks):

            size = len(chunk.encode('utf-8'))

            if size <= MSVC_STRING_LITERAL_MAX:
                continue

            if not bad:
                print("EMBEDDED SHADER HEADER TOO BIG FOR MSVC:", flush=True)

            bad += 1
            print(
                f"  {SHADER_INCLUDE_DIR}/{fname}: raw string {i + 1} is {size} bytes, "
                f"over MSVC's {MSVC_STRING_LITERAL_MAX} byte limit (C2026)"
            )
            print("    split it into two adjacent raw strings, see extension.CoopVec.hlsli")

    if bad:
        print()

    return bad


# ---------------------------------------------------------------------------
# Directory scanner
# ---------------------------------------------------------------------------

def scan(
    root: str,
    max_line_len: int,
    max_warn: int,
    collect_todos: bool,
) -> int:
    all_files: list[str]                 = []
    names_to_paths: dict[str, list[str]] = defaultdict(list)

    for dirpath, dirs, filenames in os.walk(root):
        prune_dirs(dirpath, dirs)
        for fname in filenames:
            ext = os.path.splitext(fname)[1].lower()
            if ext not in ALL_EXTENSIONS:
                continue
            full = os.path.join(dirpath, fname)
            all_files.append(full)
            names_to_paths[fname].append(full)

    bad_files = 0
    all_todos: list[tuple[str, int, str, str]] = []

    # ── Embedded shader header size check ─────────────────────────────────
    bad_files += check_embedded_shader_headers(root)

    # ── Duplicate filename check ──────────────────────────────────────────
    dup_found = False
    for fname, paths in sorted(names_to_paths.items()):
        if len(paths) > 1:
            if not dup_found:
                print("DUPLICATE FILENAMES:", flush=True)
                dup_found = True
            print(f"  '{fname}' appears {len(paths)} times:")
            for p in paths:
                print(f"    {p}")
            bad_files += 1
    if dup_found:
        print()

    # ── Per-file checks ───────────────────────────────────────────────────
    for path in sorted(all_files):
        fname_lower     = os.path.basename(path).lower()
        ext             = os.path.splitext(path)[1].lower()
        # Include-fragments (e.g. foo.inc.h) share the .h extension but
        # are not self-contained headers; skip the #pragma once check.
        is_inc_fragment = any(fname_lower.endswith(m) for m in INC_FRAGMENT_MARKERS)
        is_header       = (ext in HEADER_EXTENSIONS) and not is_inc_fragment
        rel             = _norm(os.path.relpath(path, root))
        viols, todos = check_file(
            path, rel, max_line_len, max_warn, is_header, collect_todos
        )
        if viols:
            bad_files += 1
            print(f"FAIL  {rel}")
            for v in viols:
                print(v)
            print()
        for lineno, tag, text in todos:
            all_todos.append((rel, lineno, tag, text))

    # ── TODO report (informational) ───────────────────────────────────────
    if all_todos:
        print(f"\u2500\u2500 TODO/FIXME/HACK/NOTE/XXX report ({len(all_todos)} found) \u2500\u2500")
        cur_file = None
        for rel, lineno, tag, text in sorted(all_todos, key=lambda x: (x[0], x[1])):
            if rel != cur_file:
                print(f"  {rel}")
                cur_file = rel
            print(f"    line {lineno:>5}  [{tag}]  {text}")
        print()

    return bad_files


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:

    if sys.stdout.encoding.lower() != 'utf-8':
        sys.stdout.reconfigure(encoding='utf-8')

    parser = argparse.ArgumentParser(
        description=(
            "OxC3 source style checker, line length, indentation, UTF-8, "
            "mid-line tabs, pragma once, license, EOF newline, brace-semi, "
            "absolute includes, consecutive blank lines, platform headers, "
            "and banned symbols (allocators, printf, assert, mem*, file I/O, "
            "string utils, time, system, rand)"
        )
    )
    parser.add_argument("root", help="Root directory to scan")
    parser.add_argument(
        "--max-line-length", type=int, default=MAX_LINE_LENGTH,
        metavar="N", help=f"Maximum allowed line length in columns (default: {MAX_LINE_LENGTH})"
    )
    parser.add_argument(
        "--max-warnings-per-file", type=int, default=MAX_WARNINGS_PER_FILE,
        metavar="N",
        help=f"Stop reporting after N violations per file (default: {MAX_WARNINGS_PER_FILE})"
    )
    parser.add_argument(
        "--no-todo-report", action="store_true",
        help="Suppress the informational TODO/FIXME/HACK list"
    )
    parser.add_argument(
        "--fix-indentation", action="store_true",
        help=(
            "Auto-fix indentation in-place: replace runs of 4 leading spaces "
            "with tabs, and replace mid-line tabs with 4 spaces. "
            "Rewrites files directly -- commit or back up first."
        )
    )
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    if not os.path.isdir(root):
        print(f"ERROR: '{root}' is not a directory.", file=sys.stderr)
        sys.exit(1)

    print(f"Scanning {root}")
    print(f"  extensions     : {', '.join(sorted(ALL_EXTENSIONS))}")
    print(f"  max line length: {args.max_line_length}")
    print(f"  warn cap/file  : {args.max_warnings_per_file}")
    print()

    if args.fix_indentation:
        fixed, skipped = fix_indentation(root)
        print(f"Fixed indentation in {fixed} file(s), {skipped} file(s) skipped (non-ASCII).")
        print()

    bad = scan(
        root, args.max_line_length, args.max_warnings_per_file,
        collect_todos=not args.no_todo_report,
    )

    if bad:
        print(f"Style check FAILED -- {bad} file(s) with violations.")
        sys.exit(1)
    else:
        print("Style check passed.")
        sys.exit(0)


if __name__ == "__main__":
    main()