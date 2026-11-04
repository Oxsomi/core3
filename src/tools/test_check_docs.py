#!/usr/bin/env python3
"""Accepted and rejected examples for the documentation checks."""

import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import uuid

import check_docs


OPERATIONS_C = (
    'const C8 *EOperationHasParameter_names[] = {\n'
    '\t"-format",\n'
    '\t"-input"\n'
    '};\n'
    '\n'
    'const C8 *EOperationFlags_names[EOperationFlags_Count] = {\n'
    '\t"--hex",\n'
    '\t"--aes-stdin"\n'
    '};\n'
    '\n'
    'const C8 *EOperationCategory_names[] = {\n'
    '\n'
    '\t"file",\n'
    '\n'
    '\t#ifdef CLI_RGA\n'
    '\t\t"isa",\n'
    '\t#endif\n'
    '\n'
    '\t"help"\n'
    '};\n'
    '\n'
    'void Operations_init() {\n'
    '\n'
    '\tFormat_values[EFormat_oiDL] = (Format) {\n'
    '\t\t.name = "oiDL",\n'
    '\t\t.desc = "Data list."\n'
    '\t};\n'
    '\n'
    '\tOperation_values[EOperation_FileTo] = (Operation) {\n'
    '\t\t.category = EOperationCategory_File,\n'
    '\t\t.name = "to",\n'
    '\t\t.func = &CLI_convertTo\n'
    '\t};\n'
    '\n'
    '\t#ifdef CLI_RGA\n'
    '\t\tOperation_values[EOperation_ISADevices] = (Operation) {\n'
    '\t\t\t.category = EOperationCategory_ISA,\n'
    '\t\t\t.name = "devices",\n'
    '\t\t\t.func = &CLI_isaDevices\n'
    '\t\t};\n'
    '\t#endif\n'
    '}\n'
)

TOOL_DOC = (
    '# OxC3 tool\n'
    '\n'
    '`OxC3 file to -format oiDL -input a.txt --hex`\n'
    '\n'
    'Categories: file, isa and help.\n'
    '\n'
    '`OxC3 isa devices` lists targets. `--aes-stdin` reads a key. The mode is build.py `-mode Debug`.\n'
    'A mid-word dash like well-known or a range like 1-2 is prose.\n'
)


class DocsTests(unittest.TestCase):

    def setUp(self):
        # Inherit directory permissions for Windows build runners and child checker processes.
        self.root = os.path.join(tempfile.gettempdir(), "oxc3_docs_test_" + uuid.uuid4().hex)
        os.mkdir(self.root)

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def write(self, files):
        for rel, text in files.items():
            path = os.path.join(self.root, *rel.split('/'))
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w", encoding="ascii", newline="\n") as f:
                f.write(text)

    def issues(self, files):
        self.write(files)
        return check_docs.check_tree(self.root)

    def assertAccepted(self, files):
        self.assertEqual(self.issues(files), [])

    def assertRejected(self, files, *expected):
        self.assertEqual(self.issues(files), list(expected))

    # Names

    def test_span_name_defined_in_code(self):
        self.assertAccepted({
            "include/a.h": "void CharString_parseDec(void);\n",
            "docs/a.md": "Call `CharString_parseDec` first.\n",
        })

    def test_span_name_missing_from_code(self):
        self.assertRejected(
            {
                "include/a.h": "void CharString_parseDec(void);\n",
                "docs/a.md": "Intro.\n\nCall `CharString_parseHex` first.\n",
            },
            "docs/a.md:3: 'CharString_parseHex' does not occur in the code"
        )

    def test_name_in_prose_outside_code_is_not_checked(self):
        self.assertAccepted({"docs/a.md": "CharString_parseHex is gone.\n"})

    def test_names_that_are_not_oxc3_style_are_ignored(self):
        self.assertAccepted({
            "docs/a.md": "`snake_case` `Word` `UPPER_CASE` `__OXC_MAJOR` `a_B`\n",
        })

    def test_fenced_c_block_is_checked_with_its_line(self):
        self.assertRejected(
            {
                "docs/a.md": "# Title\n\n```c\nint a;\nJsonWriter_f32(w);\n```\n",
                "src/a.c": "int a;\n",
            },
            "docs/a.md:5: 'JsonWriter_f32' does not occur in the code"
        )

    def test_untagged_fence_is_checked(self):
        self.assertRejected(
            {"docs/a.md": "```\nPlatform_create();\n```\n"},
            "docs/a.md:2: 'Platform_create' does not occur in the code"
        )

    def test_fence_tagged_with_another_language_is_not_checked(self):
        self.assertAccepted({"docs/a.md": "```text\nPlatform_create();\n```\n~~~json\n{\"Platform_create\": 1}\n~~~\n"})

    def test_longer_fence_is_closed_only_by_a_matching_fence(self):
        self.assertRejected(
            {"docs/a.md": "````c\n```\nPlatform_create();\n```\n````\n`Platform_free`\n"},
            "docs/a.md:3: 'Platform_create' does not occur in the code",
            "docs/a.md:6: 'Platform_free' does not occur in the code",
        )

    def test_name_defined_by_a_file_base_name(self):
        self.assertAccepted({
            "src/Fancy_file.c": "int a;\n",
            "docs/a.md": "See `Fancy_file`.\n",
        })

    def test_name_defined_in_each_code_location(self):
        self.assertAccepted({
            "tools/a.py": "Tools_name = 1\n",
            "cmake/a.cmake": "set(Cmake_name 1)\n",
            "test_package/a.c": "int Test_package;\n",
            "build.py": "Root_script = 1\n",
            "CMakeLists.txt": "set(Root_list 1)\n",
            "packages/dxc/conanfile.py": "Conan_package = 1\n",
            "include/shaders/a.hlsli": "float Shader_helper;\n",
            "docs/a.md": "`Tools_name` `Cmake_name` `Test_package` `Root_script` `Root_list` `Conan_package` `Shader_helper`\n",
        })

    def test_name_only_in_a_doc_or_a_package_source_is_unknown(self):
        self.assertRejected(
            {
                "packages/dxc/a.c": "int Package_only;\n",
                "docs/b.md": "`Doc_only`\n",
                "docs/a.md": "`Package_only` `Doc_only`\n",
            },
            "docs/a.md:1: 'Package_only' does not occur in the code",
            "docs/a.md:1: 'Doc_only' does not occur in the code",
            "docs/b.md:1: 'Doc_only' does not occur in the code",
        )

    def test_name_is_a_whole_word(self):
        self.assertRejected(
            {"src/a.c": "int Foo_barBaz;\n", "docs/a.md": "`Foo_bar`\n"},
            "docs/a.md:1: 'Foo_bar' does not occur in the code"
        )

    def test_prefix_matches_any_name_that_starts_with_it(self):
        self.assertAccepted({
            "src/a.c": "int Buffer_create;\n",
            "docs/a.md": "`Buffer_` and `Buffer_*` and `Buffer_create`\n",
        })

    def test_prefix_without_a_match_is_rejected(self):
        self.assertRejected(
            {"src/a.c": "int Buffer_create;\n", "docs/a.md": "`Texture_*`\n"},
            "docs/a.md:1: 'Texture_' does not occur in the code"
        )

    def test_placeholder_is_accepted(self):
        self.assertAccepted({"docs/a.md": "Functions are `Type_verb` and `X_free`.\n\n```c\nEEnumName_Value\n```\n"})

    def test_span_over_several_lines_reports_the_line_of_the_name(self):
        self.assertRejected(
            {"docs/a.md": "A span `first\nsecond Not_there` ends.\n"},
            "docs/a.md:2: 'Not_there' does not occur in the code"
        )

    # Which docs

    def test_excluded_docs_are_not_checked(self):
        self.assertAccepted({
            "docs/proposals/p.md": "`Future_thing` [x](missing.md)\n",
            "docs/roadmap.md": "`Future_thing` [x](missing.md)\n",
            "Changelog.md": "`Future_thing` [x](missing.md)\n",
            "packages/dxc/readme.md": "`Future_thing` [x](missing.md)\n",
            "build/out.md": "`Future_thing`\n",
            "vendored/.git/readme.md": "`Future_thing`\n",
        })

    def test_nested_checkout_is_not_checked(self):
        self.assertAccepted({"mesa/.git/HEAD": "ref\n", "mesa/docs/a.md": "`Future_thing`\n"})

    def test_new_doc_in_a_subdirectory_is_checked(self):
        self.assertRejected(
            {"docs/deep/er/a.md": "`Future_thing`\n"},
            "docs/deep/er/a.md:1: 'Future_thing' does not occur in the code"
        )

    # Links

    def test_links_to_files_and_directories(self):
        self.assertAccepted({
            "docs/a.md": "[b](b.md) [dir](sub) [dir](sub/) [up](../src/x.c) ![img](pic.png) [root](/src/x.c)\n",
            "docs/b.md": "# B\n",
            "docs/sub/c.md": "# C\n",
            "docs/pic.png": "x\n",
            "src/x.c": "int a;\n",
        })

    def test_missing_target(self):
        self.assertRejected(
            {"docs/a.md": "x\n[b](nope.md)\n![i](nope.png)\n"},
            "docs/a.md:2: link target 'nope.md' does not exist",
            "docs/a.md:3: link target 'nope.png' does not exist",
        )

    def test_target_must_match_case(self):
        self.assertRejected(
            {"docs/a.md": "[b](B.md)\n", "docs/b.md": "# B\n"},
            "docs/a.md:1: link target 'B.md' does not exist"
        )

    def test_target_outside_the_root(self):
        self.assertRejected(
            {"docs/a.md": "[b](../../outside.md)\n"},
            "docs/a.md:1: link target '../../outside.md' does not exist"
        )

    def test_external_links_are_skipped(self):
        self.assertAccepted({"docs/a.md": "[a](https://x.invalid/a) [b](http://x.invalid) [c](mailto:a@x.invalid)\n"})

    def test_links_in_code_are_ignored(self):
        self.assertAccepted({"docs/a.md": "`[a](nope.md)`\n\n```\n[a](nope.md)\n```\n"})

    def test_spaces_in_a_target(self):
        self.assertAccepted({
            "docs/a.md": "[a](<IEEE 754.md>) [b](IEEE%20754.md#top)\n",
            "docs/IEEE 754.md": "# Top\n",
        })

    def test_title_after_a_target(self):
        self.assertAccepted({"docs/a.md": '[a](b.md "Title") [c](b.md#b)\n', "docs/b.md": "# B\n"})

    def test_anchor_matches_heading(self):
        self.assertAccepted({
            "docs/a.md": "[x](b.md#hello-world) [y](#self-section)\n\n## Self section\n",
            "docs/b.md": "# Hello World\n",
        })

    def test_anchor_without_heading(self):
        self.assertRejected(
            {"docs/a.md": "[x](b.md#nope)\n\n[y](#nope-either)\n", "docs/b.md": "# B\n"},
            "docs/a.md:1: no heading for anchor '#nope' in docs/b.md",
            "docs/a.md:3: no heading for anchor '#nope-either' in docs/a.md",
        )

    def test_anchor_slug_rules(self):
        self.assertAccepted({
            "docs/a.md": (
                "[1](b.md#what-is-f32x4_add) [2](b.md#a-b--c) [3](b.md#under_score) [4](b.md#bold-and-link)\n"
                "[5](b.md#c-and-c) [6](b.md#spaced--out)\n"
            ),
            "src/x.c": "int F32x4_add;\n",
            "docs/x.md": "# X\n",
            "docs/b.md": (
                "# What is `F32x4_add`?\n"
                "## A, **B** & C\n"
                "## Under_score\n"
                "## _Bold_ and [link](x.md)\n"
                "## C++ and C#\n"
                "## Spaced  out ##\n"
            ),
        })

    def test_anchor_case_is_ignored(self):
        self.assertAccepted({"docs/a.md": "[x](b.md#What-Is)\n", "docs/b.md": "# What Is\n"})

    def test_duplicate_headings_get_numbered_anchors(self):
        self.assertAccepted({
            "docs/a.md": "[1](b.md#usage) [2](b.md#usage-1) [3](b.md#usage-2)\n",
            "docs/b.md": "# Usage\n## Usage\n### Usage\n",
        })

    def test_duplicate_heading_anchor_that_does_not_exist(self):
        self.assertRejected(
            {"docs/a.md": "[x](b.md#usage-2)\n", "docs/b.md": "# Usage\n## Usage\n"},
            "docs/a.md:1: no heading for anchor '#usage-2' in docs/b.md"
        )

    def test_heading_in_a_fence_is_not_a_heading(self):
        self.assertRejected(
            {"docs/a.md": "[x](b.md#fake)\n", "docs/b.md": "# Real\n```\n# Fake\n```\n"},
            "docs/a.md:1: no heading for anchor '#fake' in docs/b.md"
        )

    def test_line_anchors_on_code_files(self):
        self.assertAccepted({
            "docs/a.md": "[x](../src/x.c#L12) [y](../src/x.c#L12-L20)\n",
            "src/x.c": "int a;\n",
        })

    def test_other_anchors_on_code_files_and_directories(self):
        self.assertRejected(
            {"docs/a.md": "[x](../src/x.c#top) [y](../src#L1)\n", "src/x.c": "int a;\n"},
            "docs/a.md:1: anchor '#top' on src/x.c, which is not markdown (only #L<n> works)",
            "docs/a.md:1: anchor '#L1' on directory src",
        )

    # CLI

    def cli_files(self, doc=TOOL_DOC, source=OPERATIONS_C):
        return {"docs/OxC3_tool.md": doc, "src/tools/oxc3_cli/operations.c": source}

    def test_cli_and_doc_agree(self):
        self.assertAccepted(self.cli_files())

    def test_cli_check_needs_both_files(self):
        self.assertAccepted({"docs/OxC3_tool.md": "Use `--nothing` and `-nope`.\n"})

    def test_operation_missing_from_doc(self):
        self.assertRejected(
            self.cli_files(doc=TOOL_DOC.replace("`OxC3 isa devices` lists targets. ", "")),
            "src/tools/oxc3_cli/operations.c:38: operation 'isa devices' is not mentioned in docs/OxC3_tool.md"
        )

    def test_operation_is_documented_with_its_category(self):
        self.assertRejected(
            self.cli_files(doc=TOOL_DOC.replace("`OxC3 isa devices`", "`OxC3 devices`")),
            "src/tools/oxc3_cli/operations.c:38: operation 'isa devices' is not mentioned in docs/OxC3_tool.md"
        )

    def test_operations_may_share_a_line(self):
        self.assertAccepted(self.cli_files(doc=TOOL_DOC.replace("`OxC3 isa devices`", "`OxC3 isa dis/devices`")))

    def test_flag_missing_from_doc(self):
        self.assertRejected(
            self.cli_files(doc=TOOL_DOC.replace("`--aes-stdin` reads a key. ", "")),
            "src/tools/oxc3_cli/operations.c:8: flag '--aes-stdin' is not mentioned in docs/OxC3_tool.md"
        )

    def test_parameter_missing_from_doc(self):
        self.assertRejected(
            self.cli_files(doc=TOOL_DOC.replace(" -input a.txt", "")),
            "src/tools/oxc3_cli/operations.c:3: parameter '-input' is not mentioned in docs/OxC3_tool.md"
        )

    def test_a_longer_flag_does_not_document_a_shorter_one(self):
        self.assertRejected(
            self.cli_files(doc=TOOL_DOC.replace("--aes-stdin", "--aes-stdin-file")),
            "src/tools/oxc3_cli/operations.c:8: flag '--aes-stdin' is not mentioned in docs/OxC3_tool.md",
            "docs/OxC3_tool.md:7: flag '--aes-stdin-file' is not defined by the CLI (src/tools/oxc3_cli/operations.c)",
        )

    def test_category_and_format_missing_from_doc(self):
        self.assertRejected(
            self.cli_files(doc=TOOL_DOC.replace("Categories: file, isa and help.\n", "Categories: file and isa.\n").replace(
                "-format oiDL", "-format"
            )),
            "src/tools/oxc3_cli/operations.c:19: category 'help' is not mentioned in docs/OxC3_tool.md",
            "src/tools/oxc3_cli/operations.c:25: format 'oiDL' is not mentioned in docs/OxC3_tool.md",
        )

    def test_doc_flag_not_in_cli(self):
        self.assertRejected(
            self.cli_files(doc=TOOL_DOC + "\nTry `OxC3 file to -bogus --also-bogus`, and -bogus again.\n"),
            "docs/OxC3_tool.md:10: flag '-bogus' is not defined by the CLI (src/tools/oxc3_cli/operations.c)",
            "docs/OxC3_tool.md:10: flag '--also-bogus' is not defined by the CLI (src/tools/oxc3_cli/operations.c)",
        )

    def test_flags_of_other_tools_are_allowed(self):
        self.assertAccepted(self.cli_files(doc=TOOL_DOC + "\nDXC takes `-fspv-preserve-bindings` and `-fhlsl-x`.\n"))

    # Command line

    def test_command_line_reports_findings(self):
        command = [sys.executable, check_docs.__file__, self.root]

        for text, expected_code in (("`CharString_parseDec`\n", 0), ("`CharString_parseHex`\n", 1)):
            with self.subTest(text=text):
                self.write({"include/a.h": "void CharString_parseDec(void);\n", "docs/a.md": text})
                result = subprocess.run(
                    command,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    universal_newlines=True,
                    check=False,
                )
                self.assertEqual(result.returncode, expected_code, result.stdout)
                if expected_code:
                    self.assertIn("docs/a.md:1: 'CharString_parseHex' does not occur in the code", result.stdout)


if __name__ == "__main__":
    unittest.main()
