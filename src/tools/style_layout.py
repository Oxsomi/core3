"""Conservative layout checks for tokenized C and C++ expressions."""

from __future__ import annotations


_NOT_CALLS = {
    'if', 'for', 'while', 'switch', 'catch', 'sizeof', 'alignof', '_Alignof',
    'alignas', '_Alignas', 'decltype', 'noexcept', 'requires', 'typeid',
    'static_assert', '_Static_assert', '__attribute__', '__declspec',
    'static_cast', 'dynamic_cast', 'const_cast', 'reinterpret_cast',
}
_EXPRESSION_PREFIXES = {
    '=', '+=', '-=', '*=', '/=', '%=', '|=', '&=', '^=', '<<=', '>>=',
    'return', 'co_return', 'throw', 'co_await', 'else', '?', '!', '~', '+', '-',
    '/', '%', '^', '|', '||', '&&', '==', '!=', '<=', '>=',
}
_CONDITIONS = {'if', 'while'}
_OPENERS = {'(': ')', '[': ']', '{': '}'}


def _pair_tokens(tokens):
    """Pair delimiters only inside an uninterrupted preprocessor region."""

    pairs = {}
    parents = {}
    stack = []

    for index, token in enumerate(tokens):
        if token.kind == 'barrier':
            stack.clear()
            continue

        parents[index] = stack[-1] if stack else None

        if token.text in _OPENERS:
            stack.append(index)
        elif token.text in _OPENERS.values():
            if stack and _OPENERS[tokens[stack[-1]].text] == token.text:
                opening = stack.pop()
                pairs[opening] = index
                pairs[index] = opening
            else:
                stack.clear()

    return pairs, parents


def _is_call(index, tokens, pairs, parents, memo):
    """Recognize calls where the preceding syntax establishes an expression."""

    if index in memo:
        return memo[index]

    memo[index] = False

    if index not in pairs or index == 0:
        return False

    name = tokens[index - 1]

    if name.kind != 'identifier' or name.text in _NOT_CALLS:
        return False

    # Definitions and declarations share call-like parentheses. In particular,
    # never infer a call just from the capitalization or spelling of its name.
    closing = pairs[index]
    if closing + 1 < len(tokens) and tokens[closing + 1].text in {'{', 'const', 'noexcept', '->'}:
        return False

    before = index - 2

    # A qualified name (c::Foo, oxc::c::Foo, ::Foo) is a call wherever its first component would be one.
    while before >= 0 and tokens[before].text == '::':
        before -= 2 if before >= 1 and tokens[before - 1].kind == 'identifier' else 1

    if before >= 0 and tokens[before].kind == 'barrier':
        return False

    if before < 0 or tokens[before].text in {';', '{', '}'}:
        memo[index] = True
        return True

    previous = tokens[before].text

    if previous in _EXPRESSION_PREFIXES or previous in {'.', '->'}:
        memo[index] = True
        return True

    if previous == ')' and before in pairs:
        control = pairs[before]
        if control > 0 and tokens[control - 1].text in {'if', 'while', 'for'}:
            memo[index] = True
            return True

    if previous in {'(', ','}:
        parent = parents.get(index)
        if parent is not None and tokens[parent].text == '(':
            keyword = tokens[parent - 1].text if parent else ''
            memo[index] = keyword in _CONDITIONS or _is_call(parent, tokens, pairs, parents, memo)

    return memo[index]


def _condition_wraps_call(opening, closing, tokens, pairs, parents, calls):
    """A final call may wrap its arguments without wrapping the condition itself."""

    first_line = tokens[opening].line

    for index in range(opening + 1, closing):
        token = tokens[index]
        if token.line != first_line:
            break
        if token.text != '(' or not _is_call(index, tokens, pairs, parents, calls):
            continue
        end = pairs[index]
        if end < closing and tokens[end].line > first_line and all(
            tokens[tail].text == ')' or tokens[tail].line == tokens[end].line
            for tail in range(end + 1, closing)
        ):
            return True

    return False


def check_layout(text, tokens):
    """Return (line, rule, message) diagnostics for unambiguous wrapped forms.

    Tokens expose text, kind, line, column, start and end. Comments are omitted,
    and preprocessor directives and continued macro lines are barrier tokens.
    Templates, declarators and calls containing braced expressions are left to
    review rather than guessing how their nested language constructs should fit.
    """

    pairs, parents = _pair_tokens(tokens)
    lines = text.splitlines()
    line_first = {}
    indents = {}

    for index, token in enumerate(tokens):
        if token.kind != 'barrier':
            line_first.setdefault(token.line, index)

    for number, line in enumerate(lines, 1):
        prefix = line[:len(line) - len(line.lstrip(' \t'))]
        indents[number] = len(prefix.expandtabs(4))

    issues = []
    seen = set()
    calls = {}

    def add(line, rule, message):
        issue = (line, rule, message)
        if issue not in seen:
            seen.add(issue)
            issues.append(issue)

    for opening, closing in sorted(pairs.items()):
        if opening > closing or tokens[opening].text != '(':
            continue

        first, last = tokens[opening], tokens[closing]
        if first.line == last.line:
            continue

        keyword = tokens[opening - 1].text if opening else ''
        condition = keyword in _CONDITIONS
        wrapped_call_condition = condition and _condition_wraps_call(opening, closing, tokens, pairs, parents, calls)

        if not condition and not _is_call(opening, tokens, pairs, parents, calls):
            continue

        if any(token.text in {'{', '}'} for token in tokens[opening + 1:closing]):
            continue

        rule = 'condition-layout' if condition else 'call-layout'
        label = 'multiline condition' if condition else 'wrapped call'
        indent = indents[first.line]

        if condition and not wrapped_call_condition:
            previous = tokens[opening - 1]
            gap = text[previous.end:first.start]
            if previous.line != first.line or gap != ' ' or tokens[opening + 1].line == first.line:
                add(first.line, rule, f"{label} must start with '{keyword} (' and put operands on following lines")

        # A condition that is one wrapped call has nothing to separate, so it keeps `if (call(` on one line.
        if condition and not wrapped_call_condition and tokens[closing - 1].text == ')':
            inner = pairs.get(closing - 1)
            operand_first = tokens[opening + 1]
            if (inner is not None and inner > opening and tokens[inner].line == operand_first.line
                    and tokens[closing - 1].line > operand_first.line
                    and _is_call(inner, tokens, pairs, parents, calls)
                    and [t for t in tokens[opening + 1:closing] if t.line == operand_first.line][-1].text == '('
                    and all(parents.get(k) != opening or tokens[k].text not in {'&&', '||'}
                            for k in range(opening + 1, closing))):
                add(first.line, rule, f"{label} of one wrapped call must start with '{keyword} (' on the call's line")

        first_on_close = line_first[last.line]
        close_prefix = tokens[first_on_close:closing]
        owns_line = all(token.text == ')' for token in close_prefix)

        # A nested call's closing line can finish its expression or carry the
        # remaining outer arguments, e.g. `) < tolerance);` or `), e_rr));`.
        # The nested closer already provides the visual separation.
        if not owns_line and tokens[first_on_close].text == ')' and first_on_close in pairs:
            nested = pairs[first_on_close]
            owns_line = opening < nested < first_on_close and tokens[nested].line < last.line

        if not owns_line:
            add(last.line, rule, f"{label} closing ')' must be on its own line")
        elif indents[last.line] != indent:
            add(last.line, rule, f"{label} closing ')' must align with the opening line")

        # Only direct continuations belong to this pair. A nested call or a
        # parenthesized subexpression has its own continuation indentation.
        for index in range(opening + 1, closing):
            token = tokens[index]
            if token.line <= first.line or token.line >= last.line:
                continue
            if line_first[token.line] != index or parents.get(index) != opening:
                continue
            if token.text in {')', ']', '}'}:
                continue
            if not condition and index != opening + 1 and tokens[index - 1].text != ',':
                continue
            if indents[token.line] != indent + 4:
                part = 'operands' if condition else 'arguments'
                add(token.line, rule, f"{label} {part} must be indented one level deeper than the opening line")

    return issues
