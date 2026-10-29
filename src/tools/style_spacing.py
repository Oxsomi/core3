"""Conservative token-based spacing checks shared by the source style checker."""

from __future__ import annotations

import bisect
import re
from typing import NamedTuple

from style_layout import check_layout


class Token(NamedTuple):
    text: str
    line: int
    column: int
    start: int
    end: int
    kind: str


class SpacingIssue(NamedTuple):
    line: int
    rule: str
    message: str


_IDENTIFIER = re.compile(r'[A-Za-z_]\w*')
_NUMBER = re.compile(r'(?:\d|\.\d)(?:[eEpP][+-]|[\w.\'])*')
_RAW = re.compile(r'(?:u8|[uUL])?R"([^ ()\\\t\r\n]{0,16})\(')
_QUOTE = re.compile(r'(?:u8|[uUL])?["\']')
_PUNCT = re.compile(r'>>=|<<=|->\*|\.\.\.|##|::|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||'
                    r'\+=|-=|\*=|/=|%=|&=|\|=|\^=|.')
_TYPES = set(('U8 U16 U32 U64 I8 I16 I32 I64 F16 F32 F64 C8 C16 C32 Bool Ns '
              'void char short int long float double signed unsigned bool _Bool '
              'size_t ptrdiff_t intptr_t uintptr_t int8_t int16_t int32_t int64_t '
              'uint8_t uint16_t uint32_t uint64_t').split())
_QUALIFIERS = {'const', 'volatile', 'restrict', '_Atomic'}
_DECL_QUALIFIERS = _QUALIFIERS | {'static', 'extern', 'register', '_Thread_local', 'constexpr'}
_TYPE_OPERATORS = {'sizeof', '_Alignof', 'alignof', 'decltype', 'typeid', '__alignof__'}


def spacing_tokens(text: str) -> list[Token]:
    """Keep physical locations; literals are opaque and directives are syntax barriers.

    Macro bodies and conditional-compilation boundaries are deliberately not parsed as C.
    Comments, including continued line comments, cannot contribute syntax tokens.
    """
    starts = [0] + [m.end() for m in re.finditer('\n', text)]
    tokens = []
    at = 0
    last_code_end = 0

    def append(end: int, kind: str) -> None:
        nonlocal last_code_end
        line_index = bisect.bisect_right(starts, at) - 1
        column = len(text[starts[line_index]:at].expandtabs(4))
        tokens.append(Token(text[at:end], line_index + 1, column, at, end, kind))
        last_code_end = end

    def logical_line_end(begin: int) -> int:
        end = text.find('\n', begin)
        while end >= 0 and text[begin:end].rstrip('\r').endswith('\\'):
            begin = end + 1
            end = text.find('\n', begin)
        return len(text) if end < 0 else end + 1

    def literal_end(begin: int):
        raw = _RAW.match(text, begin)
        if raw:
            ending = ')' + raw.group(1) + '"'
            close = text.find(ending, raw.end())
            return len(text) if close < 0 else close + len(ending)
        quote = _QUOTE.match(text, begin)
        if not quote:
            return None
        delimiter = text[quote.end() - 1]
        end = quote.end()
        while end < len(text):
            if text[end] == '\\':
                end += 2
            elif text[end] == delimiter:
                return end + 1
            else:
                end += 1
        return len(text)

    def directive_end(begin: int) -> int:
        # Block comments are whitespace even when they contain physical newlines.
        # Quoted literals and line comments keep their own delimiter rules.
        cursor = begin
        while cursor < len(text):
            if text[cursor] == '\n':
                return cursor + 1
            if text.startswith('\\\n', cursor):
                cursor += 2
            elif text.startswith('\\\r\n', cursor):
                cursor += 3
            elif text.startswith('//', cursor):
                return logical_line_end(cursor)
            elif text.startswith('/*', cursor):
                close = text.find('*/', cursor + 2)
                cursor = len(text) if close < 0 else close + 2
            else:
                end = literal_end(cursor)
                if end is not None:
                    cursor = end
                else:
                    word = _IDENTIFIER.match(text, cursor) or _NUMBER.match(text, cursor)
                    cursor = word.end() if word else cursor + 1
        return len(text)

    while at < len(text):
        if text[at].isspace():
            at += 1
            continue
        if text.startswith('//', at):
            at = logical_line_end(at)
            continue
        if text.startswith('/*', at):
            end = text.find('*/', at + 2)
            at = len(text) if end < 0 else end + 2
            continue
        line_start = starts[bisect.bisect_right(starts, at) - 1]
        if text[at] == '#' and last_code_end <= line_start:
            end = directive_end(at)
            append(end, 'barrier')
            at = end
            continue
        end = literal_end(at)
        if end is not None:
            append(end, 'literal')
            at = end
            continue
        match = _IDENTIFIER.match(text, at)
        if match:
            append(match.end(), 'identifier')
        else:
            match = _NUMBER.match(text, at)
            if match:
                append(match.end(), 'number')
            else:
                match = _PUNCT.match(text, at)
                append(match.end(), 'punct')
        at = match.end()
    return tokens


def _pairs(tokens: list[Token]) -> dict[int, int]:
    stack = []
    pairs = {}
    opening = {')': '(', ']': '[', '}': '{'}
    for i, token in enumerate(tokens):
        if token.kind == 'barrier':
            stack.clear()
        elif token.text in {'(', '[', '{'}:
            stack.append(i)
        elif token.text in opening:
            if not stack or tokens[stack[-1]].text != opening[token.text]:
                stack.clear()
                continue
            start = stack.pop()
            pairs[start] = i
            pairs[i] = start
    return pairs


def cast_spacing_tokens(tokens: list[Token]) -> list[Token]:
    """Return cast closers directly touching their operands, for known scalar types only."""
    result = []
    pairs = _pairs(tokens)
    for i, token in enumerate(tokens[:-1]):
        if token.text != ')' or i not in pairs:
            continue
        start = pairs[i]
        inside = [t.text for t in tokens[start + 1:i]]
        if not inside or not any(t in _TYPES for t in inside):
            continue
        if any(t not in _TYPES | _QUALIFIERS | {'*'} for t in inside):
            continue
        if start:
            previous = tokens[start - 1]
            if previous.text in _TYPE_OPERATORS:
                continue
            if previous.kind == 'identifier' and previous.text not in {'return', 'co_return', 'case', 'throw'}:
                continue
        operand = tokens[i + 1]
        if (
            operand.kind not in {'identifier', 'number', 'literal'} and
            operand.text not in {'(', '*', '&', '+', '-', '!', '~', '{'}
        ):
            continue
        if operand.start == token.end:
            result.append(token)
    return result


def _declaration(tokens: list[Token]) -> bool:
    """Recognize a simple named declaration; ambiguous declarators are left alone."""
    if not tokens:
        return False
    i = 0
    while i < len(tokens) and tokens[i].text in _DECL_QUALIFIERS:
        i += 1
    if i == len(tokens):
        return False
    if tokens[i].text in {'struct', 'union', 'enum'}:
        i += 1
        if i == len(tokens) or tokens[i].kind != 'identifier':
            return False
        i += 1
    elif tokens[i].text in _TYPES:
        while i < len(tokens) and tokens[i].text in _TYPES | _QUALIFIERS:
            i += 1
    elif tokens[i].kind == 'identifier' and re.match(r'[A-Z][A-Za-z0-9_]*$', tokens[i].text):
        i += 1
    else:
        return False
    while i < len(tokens) and tokens[i].text in _QUALIFIERS | {'*', '&'}:
        i += 1
    if i == len(tokens) or tokens[i].kind != 'identifier':
        return False
    i += 1
    return i < len(tokens) and tokens[i].text in {';', '=', ',', '[', '{'}


def declaration_spacing_tokens(text: str, tokens: list[Token]) -> list[Token]:
    result = []
    pairs = _pairs(tokens)
    for i, token in enumerate(tokens):
        if token.text not in {'if', 'for', 'while'} or not i or tokens[i - 1].text != ';':
            continue
        end = i - 1
        start = end - 1
        while start >= 0:
            current = tokens[start]
            if current.kind == 'barrier' or current.text in {';', '{', '}'}:
                if current.text == '}' and start in pairs:
                    start = pairs[start] - 1
                    continue
                break
            if current.text in {')', ']'} and start in pairs:
                start = pairs[start] - 1
            else:
                start -= 1
        start += 1
        declaration = tokens[start:i]
        if not _declaration(declaration) or declaration[0].column != token.column:
            continue
        gap = text[tokens[end].end:token.start]
        if re.search(r'\n[\t \r]*\n|//|/\*', gap):
            continue
        # Statements sharing a line and for-header components are not declaration groups.
        if tokens[end].line >= token.line:
            continue
        result.append(token)
    return result


def check_spacing(text: str) -> list[SpacingIssue]:
    tokens = spacing_tokens(text)
    issues = [SpacingIssue(t.line, 'cast-spacing', 'needs a space after the cast') for t in cast_spacing_tokens(tokens)]
    issues.extend(SpacingIssue(t.line, 'declaration-spacing', 'needs a blank line after the declaration group')
                  for t in declaration_spacing_tokens(text, tokens))
    issues.extend(SpacingIssue(*issue) for issue in check_layout(text, tokens))
    return sorted(issues)
