#!/usr/bin/env python3
"""Accepted and rejected examples for the mechanical C/C++ spacing checks."""

import os
import subprocess
import sys
import tempfile
import unittest
import uuid

import check_style


class SpacingTests(unittest.TestCase):

    def assertAccepted(self, source):
        self.assertEqual(check_style.check_spacing(source), [])

    def assertRejected(self, rule, source, line=None):
        issues = [issue for issue in check_style.check_spacing(source) if issue.rule == rule]
        self.assertTrue(issues, "Expected {} for:\n{}".format(rule, source))
        self.assertTrue(all(issue.line > 0 and issue.message for issue in issues))
        if line is not None:
            self.assertIn(line, [issue.line for issue in issues])

    def test_cast_requires_space(self):
        self.assertRejected("cast-spacing", "U64 value = (U64)input;\n", 1)

    def test_nested_cast_requires_space(self):
        self.assertRejected("cast-spacing", "F32 value = (F32) ((U64)input);\n", 1)

    def test_pointer_cast_requires_space(self):
        self.assertRejected("cast-spacing", "C8 *text = (C8*)ptr;\n", 1)

    def test_qualified_pointer_cast_requires_space(self):
        self.assertRejected("cast-spacing", "const C8 *text = (const C8*)ptr;\n", 1)

    def test_cast_spacing_accepts_scalar_pointer_and_compound_literal(self):
        self.assertAccepted(
            "U64 value = (U64) input;\n"
            "C8 *text = (C8*) ptr;\n"
            "F32 *values = (F32[]) { 1, 2 };\n"
        )

    def test_parentheses_and_type_queries_are_not_casts(self):
        self.assertAccepted(
            "U64 size = sizeof(U64)*count;\n"
            "U64 align = _Alignof(U64)*count;\n"
            "U64 other = alignof(U64)*count;\n"
            "U64 value = (count)*scale;\n"
            "U64 rounded = (count + 1);\n"
        )

    def test_function_and_callback_declarations_are_not_casts(self):
        self.assertAccepted(
            "U64 readValue(U64 value);\n"
            "void registerCallback(void (*callback)(U64));\n"
            "void invoke(\n"
            "\tvoid (*callback)(U64),\n"
            "\tU64 value\n"
            ");\n"
        )

    def test_function_declarations_are_not_checked_as_call_layout(self):
        self.assertAccepted("void consume(\nU64 value);\n")

    def test_comment_and_string_contents_are_not_code(self):
        self.assertAccepted(
            "//U64 value = (U64)input;\n"
            "/* Call(\n"
            "bad); */\n"
            'const C8 *text = "(U64)input; Call(\\\"bad\\\")";\n'
            "C8 marker = ')';\n"
        )

    def test_raw_string_contents_are_not_code(self):
        self.assertAccepted(
            'const char *text = R"fixture(\n'
            "U64 value = (U64)input;\n"
            "if(value)\n"
            "Call(\n"
            "bad);\n"
            ')fixture";\n'
        )

    def test_comment_does_not_hide_following_cast(self):
        self.assertRejected("cast-spacing", "/* prefix */ U64 value = (U64)input;\n", 1)

    def test_continued_line_comment_contents_are_not_code(self):
        self.assertAccepted(
            "//Continued comment \\\n"
            "U64 ignored = (U64)input;\n"
            "U64 value = (U64) input;\n"
        )

    def test_macro_continuations_are_exempt(self):
        self.assertAccepted(
            "#define CONVERT(value) \\\n"
            "\t((U64)value)\n"
            "#define CHECK(value) do { \\\n"
            "\tU64 count = value; \\\n"
            "\tif(count) Call( \\\n"
            "bad); \\\n"
            "} while(0)\n"
        )

    def test_comment_before_preprocessor_directive_is_whitespace(self):
        self.assertAccepted("/*Macro body*/ #define CONVERT(value) ((U64)value)\n")

    def test_multiline_comment_before_preprocessor_directive_is_whitespace(self):
        self.assertAccepted(
            "/*Macro body\n"
            "*/ #define CONVERT(value) \\\n"
            "\t((U64)value)\n"
            "U64 value = (U64) input;\n"
        )

    def test_multiline_comment_inside_preprocessor_directive_is_whitespace(self):
        self.assertAccepted(
            "#define CONVERT(value) /*Macro comment\n"
            "ends here*/ ((U64)value)\n"
            "CONVERT(input)\n"
        )

    def test_quoted_comment_marker_in_directive_does_not_hide_following_code(self):
        self.assertRejected(
            "cast-spacing",
            '#define COMMENT_MARKER "/*"\n'
            "U64 value = (U64)input;\n",
            2,
        )

    def test_checks_resume_after_macro_continuation(self):
        self.assertRejected(
            "cast-spacing",
            "#define CONVERT(value) \\\n"
            "\t((U64)value)\n"
            "U64 value = (U64)input;\n",
            3,
        )

    def test_declaration_requires_blank_before_control_statement(self):
        for statement in ("if(value)", "for(U64 i = 0; i < value; ++i)", "while(value)"):
            with self.subTest(statement=statement):
                self.assertRejected(
                    "declaration-spacing",
                    "void run() {\n"
                    "\tU64 value = readValue();\n"
                    "\t" + statement + "\n"
                    "\t\tconsume(value);\n"
                    "}\n",
                    3,
                )

    def test_declaration_group_has_one_blank_before_check(self):
        self.assertAccepted(
            "void run() {\n"
            "\tU64 value = readValue();\n"
            "\tconst U64 limit = 2;\n"
            "\n"
            "\tif(value < limit)\n"
            "\t\tconsume(value);\n"
            "}\n"
        )

    def test_array_declarations_require_blank_before_loop(self):
        self.assertRejected(
            "declaration-spacing",
            "void run() {\n"
            "\tconst U64 values[] = { 1, 2, 3 };\n"
            "\tconst I64 expected[] = { 1, 2, 3 };\n"
            "\tfor(U64 i = 0; i < 3; ++i)\n"
            "\t\tconsume(values[i]);\n"
            "}\n",
            4,
        )

    def test_multiline_declaration_requires_blank_before_check(self):
        self.assertRejected(
            "declaration-spacing",
            "void run() {\n"
            "\tconst U64 value = readValue(\n"
            "\t\tinput\n"
            "\t);\n"
            "\tif(value)\n"
            "\t\tconsume(value);\n"
            "}\n",
            5,
        )

    def test_for_header_declaration_is_not_a_declaration_statement(self):
        self.assertAccepted(
            "void run() {\n"
            "\tfor(U64 i = 0; i < count; ++i)\n"
            "\t\tif(values[i])\n"
            "\t\t\tconsume(values[i]);\n"
            "}\n"
        )

    def test_preprocessor_boundary_does_not_join_statements(self):
        self.assertAccepted(
            "void run() {\n"
            "\tU64 value = readValue();\n"
            "\t#if ENABLED\n"
            "\tif(value)\n"
            "\t\tconsume(value);\n"
            "\t#endif\n"
            "}\n"
        )

    def test_compact_do_body_is_allowed(self):
        self.assertAccepted(
            "void run() {\n"
            "\tdo {\n"
            "\t\tdigits[count++] = (C8) ('0' + exponent % 10);\n"
            "\t\texponent /= 10;\n"
            "\t} while(exponent);\n"
            "}\n"
        )

    def test_single_line_calls_and_conditions_are_allowed(self):
        self.assertAccepted(
            "void run() {\n"
            "\tif(ready)\n"
            "\t\tconsume(readValue(input));\n"
            "}\n"
        )

    def test_wrapped_call(self):
        self.assertAccepted(
            "void run() {\n"
            "\tconsume(\n"
            "\t\tfirst, second,\n"
            "\t\tthird\n"
            "\t);\n"
            "}\n"
        )

    def test_wrapped_call_argument_expression_may_indent_continuation(self):
        self.assertAccepted(
            "void run() {\n"
            "\tconsume(\n"
            "\t\tfirst ==\n"
            "\t\t\tsecond\n"
            "\t);\n"
            "}\n"
        )

    def test_wrapped_call_closer_cannot_trail_argument(self):
        self.assertRejected(
            "call-layout",
            "void run() {\n"
            "\tconsume(\n"
            "\t\tfirst, second);\n"
            "}\n",
        )

    def test_wrapped_call_arguments_are_indented(self):
        self.assertRejected(
            "call-layout",
            "void run() {\n"
            "\tconsume(\n"
            "\tfirst, second\n"
            "\t);\n"
            "}\n",
        )

    def test_wrapped_call_closer_aligns_with_opening_line(self):
        self.assertRejected(
            "call-layout",
            "void run() {\n"
            "\tconsume(\n"
            "\t\tfirst, second\n"
            "\t\t);\n"
            "}\n",
        )

    def test_nested_calls_with_grouped_closers(self):
        self.assertAccepted(
            "void run() {\n"
            "\tgotoIfError3(clean, createBuffer(\n"
            "\t\tdevice, size, e_rr\n"
            "\t));\n"
            "}\n"
        )

    def test_nested_call_closer_may_continue_the_outer_argument_expression(self):
        self.assertAccepted(
            "void run() {\n"
            '\tTest_assert(test, "mat ortho maps depth", F32_abs(\n'
            "\t\tF32x4_z(projected) - 1\n"
            "\t) < 1e-5f);\n"
            "}\n"
        )

    def test_nested_call_closer_may_continue_with_outer_arguments(self):
        self.assertAccepted(
            "void run() {\n"
            "\tconsume(readValue(\n"
            "\t\tinput\n"
            "\t), other);\n"
            "}\n"
        )

    def test_nested_call_closer_may_finish_error_check_wrappers(self):
        self.assertAccepted(
            "void run() {\n"
            "\tgotoIfError3(clean, dxCheck(deviceExt->residencyFence->lpVtbl->SetEventOnCompletion(\n"
            "\t\tdeviceExt->residencyFence, value, NULL\n"
            "\t), e_rr));\n"
            "}\n"
        )

    def test_nested_calls_with_different_opening_lines(self):
        self.assertAccepted(
            "void run() {\n"
            "\tconsume(\n"
            "\t\treadValue(\n"
            "\t\t\tinput\n"
            "\t\t),\n"
            "\t\tother\n"
            "\t);\n"
            "}\n"
        )

    def test_aggregate_initializer_in_call_is_not_an_argument_indent(self):
        self.assertAccepted(
            "void run() {\n"
            "\tconsume((Value) {\n"
            "\t\t.count = 1,\n"
            "\t\t.values = { 2, 3 }\n"
            "\t});\n"
            "}\n"
        )

    def test_wrapped_condition(self):
        self.assertAccepted(
            "void run() {\n"
            "\tif (\n"
            "\t\tfirst &&\n"
            "\t\tsecond\n"
            "\t)\n"
            "\t\tconsume(input);\n"
            "}\n"
        )

    def test_condition_may_wrap_only_positive_call_arguments(self):
        self.assertAccepted(
            "void run() {\n"
            "\tif(MemoryStream_createFromBufferRegion(\n"
            "\t\tbuffer, 0, length, &stream, e_rr\n"
            "\t))\n"
            "\t\tconsume(stream);\n"
            "}\n"
        )

    def test_condition_may_wrap_only_negated_call_arguments(self):
        self.assertAccepted(
            "void run() {\n"
            "\tif(!MemoryStream_createFromBufferRegion(\n"
            "\t\tbuffer, 0, length, &stream, e_rr\n"
            "\t))\n"
            "\t\treturn;\n"
            "}\n"
        )

    def test_condition_may_wrap_only_qualified_call_arguments(self):
        for name in ("c::JobQueue_push", "oxc::c::JobQueue_push", "::JobQueue_push"):
            with self.subTest(name=name):
                self.assertAccepted(
                    "void run() {\n"
                    "\tif(!" + name + "(\n"
                    "\t\t&queue, callback, e_rr\n"
                    "\t))\n"
                    "\t\treturn;\n"
                    "}\n"
                )

    def test_condition_may_compare_on_nested_call_closing_line(self):
        self.assertAccepted(
            "void run() {\n"
            "\tif(F32_abs(\n"
            "\t\tvalue\n"
            "\t) < tolerance)\n"
            "\t\tconsume(value);\n"
            "}\n"
        )

    def test_condition_may_wrap_only_logical_prefix_call_arguments(self):
        for prefix in ("Buffer_length(r->attributes)", "(ready || available)"):
            with self.subTest(prefix=prefix):
                self.assertAccepted(
                    "void run() {\n"
                    "\tif(" + prefix + " && !MemoryStream_createFromBufferRegion(\n"
                    "\t\tr->attributes, 0, length, &stream, e_rr\n"
                    "\t))\n"
                    "\t\treturn;\n"
                    "}\n"
                )

    def test_wrapped_call_does_not_hide_a_wrapped_condition_operand(self):
        self.assertRejected(
            "condition-layout",
            "void run() {\n"
            "\tif(create(\n"
            "\t\tbuffer, length\n"
            "\t) &&\n"
            "\t\tready\n"
            "\t)\n"
            "\t\tconsume(buffer);\n"
            "}\n",
        )

    def test_condition_of_one_wrapped_call_keeps_the_call_on_the_if_line(self):
        self.assertRejected(
            "condition-layout",
            "void run() {\n"
            "\tif (\n"
            "\t\t!check(a, make(\n"
            "\t\t\tfirst, second\n"
            "\t\t))\n"
            "\t)\n"
            "\t\treturn;\n"
            "}\n"
        )

    def test_wrapped_condition_opener_has_no_operand(self):
        self.assertRejected(
            "condition-layout",
            "void run() {\n"
            "\tif(first &&\n"
            "\t\tsecond\n"
            "\t)\n"
            "\t\tconsume(input);\n"
            "}\n",
        )

    def test_wrapped_condition_closer_cannot_trail_operand(self):
        self.assertRejected(
            "condition-layout",
            "void run() {\n"
            "\tif (\n"
            "\t\tfirst && second)\n"
            "\t\tconsume(input);\n"
            "}\n",
        )

    def test_wrapped_condition_operands_are_indented(self):
        self.assertRejected(
            "condition-layout",
            "void run() {\n"
            "\tif (\n"
            "\tfirst && second\n"
            "\t)\n"
            "\t\tconsume(input);\n"
            "}\n",
        )

    def test_wrapped_condition_closer_aligns_with_if(self):
        self.assertRejected(
            "condition-layout",
            "void run() {\n"
            "\tif (\n"
            "\t\tfirst && second\n"
            "\t\t)\n"
            "\t\tconsume(input);\n"
            "}\n",
        )

    def test_nested_call_in_wrapped_condition(self):
        self.assertAccepted(
            "void run() {\n"
            "\tif (\n"
            "\t\tTest_assert(test, \"create\", create(\n"
            "\t\t\tfirst, second, e_rr\n"
            "\t\t)) &&\n"
            "\t\tready\n"
            "\t)\n"
            "\t\tconsume(input);\n"
            "}\n"
        )

    def test_command_line_reports_spacing_violation(self):
        license_text = "/*\n" + "\n".join(check_style.LICENSE_LINES) + "\n*/\n\n//spacing_fixture.c\n\n"
        command = [sys.executable, check_style.__file__]

        # Inherit directory permissions for Windows build runners and child checker processes.
        directory = os.path.join(tempfile.gettempdir(), "oxc3_style_test_" + uuid.uuid4().hex)
        os.mkdir(directory)
        path = os.path.join(directory, "spacing_fixture.c")

        try:
            for cast, expected_code in (("(U64) input", 0), ("(U64)input", 1)):
                with self.subTest(cast=cast):
                    with open(path, "w", encoding="ascii", newline="\n") as source:
                        source.write(license_text + "U64 value = " + cast + ";\n")

                    result = subprocess.run(
                        command + [directory, "--no-todo-report"],
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        universal_newlines=True,
                        check=False,
                    )
                    self.assertEqual(result.returncode, expected_code, result.stdout)
                    if expected_code:
                        self.assertIn("[cast-spacing]", result.stdout)

        finally:
            if os.path.exists(path):
                os.remove(path)
            os.rmdir(directory)


if __name__ == "__main__":
    unittest.main()
