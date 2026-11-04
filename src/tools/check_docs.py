#!/usr/bin/env python3
# check_docs.py
# Documentation checks, over all .md files except the excluded ones listed below.
#
# Check 1, names exist in the code: every OxC3-style identifier (uppercase first letter, an underscore and a
# lowercase letter, e.g. CharString_parseDec) inside an inline code span, or inside a fenced block tagged
# c/cpp/c++/h/hpp/hlsl/cmake/python (or untagged), must occur as a word in the code. A name ending in an
# underscore, or followed by '*', is a prefix and matches any known word starting with it. Deliberate
# placeholders go in PLACEHOLDER_NAMES.
#
# Check 2, relative links resolve: the target of every inline link or image that is not external must exist
# (file or directory, with the case GitHub would see), and a #anchor must match a heading of the target .md
# using GitHub's slug rules. #L<n> and #L<n>-L<m> are accepted on files that are not markdown.
#
# Check 3, the CLI and its doc agree: every category, operation, format, flag and parameter name that
# src/tools/oxc3_cli/operations.c defines appears in docs/OxC3_tool.md, and every flag that doc names exists in
# the CLI.
#
# Not checked: docs/proposals/ (design proposals name things that do not exist yet), docs/roadmap.md (open items name
# future and external things) and Changelog.md. Directories are pruned the way check_style.py prunes them, plus
# packages/ and third party directories.
#
# The walk uses the file system rather than git, so a new doc is checked before it is committed.
#
# Usage:
#   python check_docs.py <root_dir>
#
# Prints path:line: message per finding. Exits with code 0 on success, 1 if any finding is reported.

from __future__ import annotations        # PEP 585/604 style hints must not be evaluated on older Python (3.7/3.8)

import os
import re
import sys
import argparse
from urllib.parse import unquote

from check_style import prune_dirs

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

# Docs that are not checked, as paths relative to the root. A directory ends in '/'.
EXCLUDED_DOCS = ('docs/proposals/', 'docs/roadmap.md', 'Changelog.md')

# Directories that hold other people's files. They never contain docs that describe this repository.
EXTERNAL_DIRS = ('packages', 'external', 'third_party', 'thirdparty', 'submodules')

# Fence tags whose content is code in this repository's languages. An untagged fence is checked as well.
CODE_FENCE_TAGS = {'', 'c', 'cpp', 'c++', 'h', 'hpp', 'hlsl', 'cmake', 'python'}

# Code that names can be defined in.
CODE_DIRS = ('include', 'src', 'tools', 'cmake', 'test_package')
CODE_EXTENSIONS = {
    '.c', '.cpp', '.h', '.hpp', '.inc', '.hlsl', '.hlsli', '.py', '.txt', '.cmake', '.json', '.yml', '.in',
    '.bat', '.sh', '.cs', '.m', '.mm', '.java', '.kt', '.js', '.html',
}
MAX_CODE_FILE_BYTES = 8 * 1024 * 1024

# Files under the code directories whose words aren't code: this check's own tests name things that don't exist on
# purpose, so reading them would let a doc name pass the check that exists nowhere else.
NOT_CODE_FILES = {'test_check_docs.py'}

# Names that look like OxC3 identifiers but are placeholders in a doc about how names are built. Each one is
# listed with the reason, so a stale real name never hides behind this list.
PLACEHOLDER_NAMES = {
    'X_free': 'the free function of a type called X, in the naming rules',
    'Type_verb': 'the shape of a function name: the type, then what it does',
    'EEnumName_Value': 'the shape of an enum constant: the enum type, then the value',
    'Module_functionName': 'the shape of a function name that belongs to a module',
    'MyThing_frobnicate': 'an example function of an example type, in the error handling idiom',
    'MyResult_createNull': 'an example null constructor of an example type',
    'MyResult_createx': 'an example constructor that takes the default allocator, of an example type',
    'MyResult_freex': 'an example destructor that takes the default allocator, of an example type',
    'List_length': 'an example list accessor in the declaration spacing examples',
    'List_capacity': 'an example list accessor in the declaration spacing examples',
    'Lfunc_end0': 'a label in LLVM assembler output, quoted from a tool that is not part of this repository',
}

# Flags the doc names that belong to other tools, as prefixes. The CLI check only compares OxC3's own flags.
FOREIGN_FLAG_PREFIXES = (
    '-fspv-',    # DXC options such as -fspv-preserve-bindings
    '-fhlsl-',   # DXC options such as -fhlsl-unused-resource-bindings
    '-mode',     # build.py
)

CLI_DOC = 'docs/OxC3_tool.md'
CLI_SOURCE = 'src/tools/oxc3_cli/operations.c'

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_WORD_RE      = re.compile(r'[A-Za-z_][A-Za-z0-9_]*')
_FENCE_RE     = re.compile(r'^ {0,3}(`{3,}|~{3,})\s*([^\s`]*)')
_HEADING_RE   = re.compile(r'^ {0,3}(#{1,6})[ \t]+(.*?)(?:[ \t]+#+)?[ \t]*$')
_SPAN_RE      = re.compile(r'(?<!`)(`+)(?!`)((?:(?!\n[ \t]*\n).)+?)(?<!`)\1(?!`)', re.DOTALL)
_LINK_RE      = re.compile(r'!?\[[^\]\n]*\]\(\s*(<[^>\n]*>|[^)\s]*)(?:\s+(?:"[^"]*"|\'[^\']*\'))?\s*\)')
_LINE_ANCHOR  = re.compile(r'^L\d+(-L\d+)?$')
_EXTERNAL_RE  = re.compile(r'^(https?:|mailto:)', re.IGNORECASE)


def _norm(p: str) -> str:
    """Normalise path separators to forward slashes."""
    return p.replace(os.sep, '/')


def is_ox_name(word: str) -> bool:
    """An OxC3-style identifier: uppercase first letter, an underscore and a lowercase letter."""
    return word[0].isupper() and '_' in word and any(c.islower() for c in word)


def is_excluded(rel: str) -> bool:
    for ex in EXCLUDED_DOCS:
        if ex.endswith('/'):
            if rel.startswith(ex):
                return True
        elif rel == ex:
            return True
    return False


def find_docs(root: str) -> list[str]:
    """Relative paths of every checked .md file, sorted."""

    docs: list[str] = []

    for dirpath, dirs, filenames in os.walk(root):
        prune_dirs(dirpath, dirs)
        dirs[:] = [d for d in dirs if d.lower() not in EXTERNAL_DIRS]
        for fname in filenames:
            if fname.lower().endswith('.md'):
                rel = _norm(os.path.relpath(os.path.join(dirpath, fname), root))
                if not is_excluded(rel):
                    docs.append(rel)

    return sorted(docs)


def read_text(path: str) -> str:
    with open(path, 'rb') as f:
        return f.read().decode('utf-8', errors='replace').replace('\r\n', '\n')


def blank(m: re.Match) -> str:
    """Replacement that keeps newlines, so offsets still map to the original line numbers."""
    return re.sub(r'[^\n]', ' ', m.group(0))


class Doc:
    """One markdown file split into prose, fenced blocks, headings and code spans."""

    def __init__(self, text: str) -> None:

        self.fences: list[tuple[int, str, str]] = []     # (first content line, tag, content)
        self.headings: list[str] = []                    # raw heading text, in document order
        prose_lines: list[str] = []

        fence = None                                     # (marker char, marker length, tag, first line, lines)

        for lineno, line in enumerate(text.split('\n'), start=1):

            m = _FENCE_RE.match(line)

            if fence is None:
                if m:
                    fence = (m.group(1)[0], len(m.group(1)), m.group(2).lower(), lineno + 1, [])
                    prose_lines.append('')
                    continue
            else:
                closes = m and m.group(1)[0] == fence[0] and len(m.group(1)) >= fence[1] and not m.group(2)
                if closes:
                    self.fences.append((fence[3], fence[2], '\n'.join(fence[4])))
                    fence = None
                else:
                    fence[4].append(line)
                prose_lines.append('')
                continue

            h = _HEADING_RE.match(line)
            if h:
                self.headings.append(h.group(2))

            prose_lines.append(line)

        if fence is not None:
            self.fences.append((fence[3], fence[2], '\n'.join(fence[4])))

        self.prose = '\n'.join(prose_lines)

        # Code spans, as (line of the span, content)
        self.spans: list[tuple[int, str]] = []
        for m in _SPAN_RE.finditer(self.prose):
            self.spans.append((self.prose.count('\n', 0, m.start(2)) + 1, m.group(2)))

        self.prose_no_code = _SPAN_RE.sub(blank, self.prose)


# ---------------------------------------------------------------------------
# Check 1: names exist in the code
# ---------------------------------------------------------------------------

def collect_code_words(root: str) -> set[str]:
    """Every word in the code, plus the base names of the files that hold it."""

    words: set[str] = set()

    def add_file(path: str, fname: str, read: bool) -> None:

        for part in fname.split('.'):
            if part:
                words.add(part)

        if not read:
            return

        try:
            if os.path.getsize(path) > MAX_CODE_FILE_BYTES:
                return
            words.update(_WORD_RE.findall(read_text(path)))
        except OSError:
            return

    def walk(top: str, only_ext: set | None) -> None:
        for dirpath, dirs, filenames in os.walk(top):
            prune_dirs(dirpath, dirs)
            for fname in filenames:
                if fname in NOT_CODE_FILES:
                    continue
                ext = os.path.splitext(fname)[1].lower()
                if only_ext is None:
                    add_file(os.path.join(dirpath, fname), fname, ext in CODE_EXTENSIONS)
                elif ext in only_ext:
                    add_file(os.path.join(dirpath, fname), fname, True)

    for d in CODE_DIRS:
        if os.path.isdir(os.path.join(root, d)):
            walk(os.path.join(root, d), None)

    for fname in sorted(os.listdir(root)):
        path = os.path.join(root, fname)
        if os.path.isfile(path) and (fname.lower().endswith('.py') or fname == 'CMakeLists.txt'):
            add_file(path, fname, True)

    packages = os.path.join(root, 'packages')
    if os.path.isdir(packages):
        for pkg in sorted(os.listdir(packages)):
            conanfile = os.path.join(packages, pkg, 'conanfile.py')
            if os.path.isfile(conanfile):
                add_file(conanfile, 'conanfile.py', True)

    walk(root, {'.hlsl', '.hlsli'})

    return words


def check_names(rel: str, doc: Doc, words: set[str], sorted_words: list[str]) -> list[str]:

    issues: list[str] = []
    seen: set[tuple[int, str]] = set()

    def known(name: str, following: str) -> bool:

        if name in PLACEHOLDER_NAMES or name in words:
            return True

        # A prefix such as Buffer_ or Buffer_* stands for every name that starts with it.
        if name.endswith('_') or following.startswith('*'):
            return any(w.startswith(name) for w in sorted_words)

        return False

    def scan(lineno: int, content: str) -> None:
        for m in _WORD_RE.finditer(content):

            name = m.group(0)

            if not is_ox_name(name):
                continue

            line = lineno + content.count('\n', 0, m.start())

            if known(name, content[m.end():m.end() + 1]) or (line, name) in seen:
                continue

            seen.add((line, name))
            issues.append(f"{rel}:{line}: '{name}' does not occur in the code")

    for lineno, content in doc.spans:
        scan(lineno, content)

    for lineno, tag, content in doc.fences:
        if tag in CODE_FENCE_TAGS:
            scan(lineno, content)

    return sorted(issues, key=lambda issue: int(issue.split(':')[1]))


# ---------------------------------------------------------------------------
# Check 2: relative links resolve
# ---------------------------------------------------------------------------

def github_slug(heading: str) -> str:
    """GitHub's anchor for a heading, before the -1, -2 suffixes that duplicates get."""

    text = re.sub(r'!?\[([^\]]*)\]\([^)]*\)', r'\1', heading)      # a link keeps only its text
    text = text.replace('`', '')                                   # code markup
    text = re.sub(r'\*+', '', text)                                # emphasis
    text = re.sub(r'(?<!\w)_+(.+?)_+(?!\w)', r'\1', text)          # _emphasis_ only; snake_case keeps its underscores
    text = text.strip().lower()
    text = re.sub(r'[^\w\- ]', '', text)

    return text.replace(' ', '-')


def heading_anchors(headings: list[str]) -> set[str]:

    anchors: set[str] = set()
    counts: dict[str, int] = {}

    for heading in headings:

        base = github_slug(heading)
        slug = base
        n = counts.get(base, 0)

        while slug in anchors:
            n += 1
            slug = f"{base}-{n}"

        counts[base] = n
        anchors.add(slug)

    return anchors


def exists_exact(root: str, rel: str) -> bool:
    """Whether root/rel exists with the exact case of every component, as it does on GitHub."""

    cur = root

    for part in rel.split('/'):

        if part in ('', '.'):
            continue

        try:
            if part not in os.listdir(cur):
                return False
        except OSError:
            return False

        cur = os.path.join(cur, part)

    return True


def check_links(root: str, rel: str, doc: Doc, anchor_cache: dict[str, set[str]]) -> list[str]:

    issues: list[str] = []

    def anchors_of(target_rel: str) -> set[str]:
        if target_rel not in anchor_cache:
            anchor_cache[target_rel] = heading_anchors(Doc(read_text(os.path.join(root, target_rel))).headings)
        return anchor_cache[target_rel]

    for m in _LINK_RE.finditer(doc.prose_no_code):

        target = m.group(1)

        if target.startswith('<') and target.endswith('>'):
            target = target[1:-1]

        if not target or _EXTERNAL_RE.match(target):
            continue

        line = doc.prose_no_code.count('\n', 0, m.start()) + 1
        path, _, anchor = target.partition('#')
        path = unquote(path)

        if not path:
            target_rel = rel
        elif path.startswith('/'):
            target_rel = os.path.normpath(path.lstrip('/'))
        else:
            target_rel = os.path.normpath(os.path.join(os.path.dirname(rel), path))

        target_rel = _norm(target_rel)

        if target_rel.startswith('..') or not exists_exact(root, target_rel):
            issues.append(f"{rel}:{line}: link target '{path}' does not exist")
            continue

        if not anchor:
            continue

        full = os.path.join(root, target_rel)

        if os.path.isfile(full) and target_rel.lower().endswith('.md'):
            if unquote(anchor).lower() not in anchors_of(target_rel):
                issues.append(f"{rel}:{line}: no heading for anchor '#{anchor}' in {target_rel}")

        elif os.path.isfile(full):
            if not _LINE_ANCHOR.match(anchor):
                issues.append(f"{rel}:{line}: anchor '#{anchor}' on {target_rel}, which is not markdown (only #L<n> works)")

        else:
            issues.append(f"{rel}:{line}: anchor '#{anchor}' on directory {target_rel}")

    return issues


# ---------------------------------------------------------------------------
# Check 3: the CLI and its doc agree
# ---------------------------------------------------------------------------

_STRING_RE = re.compile(r'"([^"\\]*)"')


def parse_cli(text: str) -> dict:
    """Categories, operations, formats, flags and parameters defined in operations.c, each with its line."""

    def line_of(pos: int) -> int:
        return text.count('\n', 0, pos) + 1

    def names_of(array: str) -> list[tuple[str, int]]:
        m = re.search(array + r'\[[^\]]*\]\s*=\s*\{(.*?)\n\};', text, re.DOTALL)
        if not m:
            return []
        body = re.sub(r'//[^\n]*', '', m.group(1))
        return [(s.group(1), line_of(m.start(1) + s.start())) for s in _STRING_RE.finditer(body)]

    cli: dict = {
        'categories': names_of('EOperationCategory_names'),
        'parameters': names_of('EOperationHasParameter_names'),
        'flags': names_of('EOperationFlags_names'),
        'operations': [],
        'formats': [],
    }

    for m in re.finditer(r'Operation_values\[\w+\]\s*=\s*\(Operation\)\s*\{(.*?)\n\t+\};', text, re.DOTALL):
        cat = re.search(r'\.category\s*=\s*EOperationCategory_(\w+)', m.group(1))
        name = re.search(r'\.name\s*=\s*"([^"]*)"', m.group(1))
        if cat and name:
            cli['operations'].append((cat.group(1).lower(), name.group(1), line_of(m.start(1) + name.start())))

    for m in re.finditer(r'Format_values\[\w+\]\s*=\s*\(Format\)\s*\{(.*?)\n\t+\};', text, re.DOTALL):
        name = re.search(r'\.name\s*=\s*"([^"]*)"', m.group(1))
        if name:
            cli['formats'].append((name.group(1), line_of(m.start(1) + name.start())))

    return cli


def doc_flags(text: str) -> list[tuple[str, int]]:
    """Every -name or --name token in the doc, with its line. A dash inside a word or after a path is prose."""

    flags: list[tuple[str, int]] = []

    for lineno, line in enumerate(text.split('\n'), start=1):
        for m in re.finditer(r'(?<![\w\-/.])(--?[A-Za-z][\w-]*)', line):
            flags.append((m.group(1), lineno))

    return flags


def check_cli(root: str) -> list[str]:

    doc_path = os.path.join(root, *CLI_DOC.split('/'))
    src_path = os.path.join(root, *CLI_SOURCE.split('/'))

    if not os.path.isfile(doc_path) or not os.path.isfile(src_path):
        return []

    doc_text = read_text(doc_path)
    cli = parse_cli(read_text(src_path))
    issues: list[str] = []

    def mentions(word: str) -> bool:
        return re.search(r'(?<![\w-])' + re.escape(word) + r'(?![\w-])', doc_text) is not None

    for kind, noun in (('categories', 'category'), ('formats', 'format'), ('parameters', 'parameter'), ('flags', 'flag')):
        for name, line in cli[kind]:
            if not mentions(name):
                issues.append(f"{CLI_SOURCE}:{line}: {noun} '{name}' is not mentioned in {CLI_DOC}")

    # An operation is documented as its category followed by its name on one line (OxC3 file list, profile aes256/aes128).
    doc_lines = doc_text.split('\n')

    for cat, name, line in cli['operations']:

        pattern = re.compile(r'(?<![\w-])' + re.escape(cat) + r'(?![\w-]).*(?<![\w-])' + re.escape(name) + r'(?![\w-])')

        if not any(pattern.search(l) for l in doc_lines):
            issues.append(f"{CLI_SOURCE}:{line}: operation '{cat} {name}' is not mentioned in {CLI_DOC}")

    defined = {n for n, _ in cli['parameters']} | {n for n, _ in cli['flags']}
    reported: set[str] = set()

    for flag, line in doc_flags(doc_text):
        if flag in defined or flag in reported or flag.startswith(FOREIGN_FLAG_PREFIXES):
            continue
        reported.add(flag)
        issues.append(f"{CLI_DOC}:{line}: flag '{flag}' is not defined by the CLI ({CLI_SOURCE})")

    return issues


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def check_tree(root: str) -> list[str]:

    issues: list[str] = []
    docs = find_docs(root)
    words = collect_code_words(root)
    sorted_words = sorted(words)
    anchor_cache: dict[str, set[str]] = {}

    for rel in docs:
        doc = Doc(read_text(os.path.join(root, rel)))
        issues += check_names(rel, doc, words, sorted_words)
        issues += check_links(root, rel, doc, anchor_cache)

    issues += check_cli(root)

    return issues


def main() -> None:

    if sys.stdout.encoding.lower() != 'utf-8':
        sys.stdout.reconfigure(encoding='utf-8')

    parser = argparse.ArgumentParser(
        description="OxC3 documentation checker: names exist in the code, relative links resolve, CLI doc matches the CLI"
    )
    parser.add_argument("root", help="Root directory to scan")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    if not os.path.isdir(root):
        print(f"ERROR: '{root}' is not a directory.", file=sys.stderr)
        sys.exit(1)

    issues = check_tree(root)

    for issue in issues:
        print(issue)

    if issues:
        print(f"Docs check FAILED -- {len(issues)} finding(s).")
        sys.exit(1)

    print("Docs check passed.")
    sys.exit(0)


if __name__ == "__main__":
    main()
