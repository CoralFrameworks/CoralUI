#!/usr/bin/env python3
# Copyright (C) 2026 The Coral Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Compile ``uax14_rules.json`` into the runtime UAX #14 DFA table.

The runtime model is a deterministic finite automaton over normalized line-break
tokens. Each rule is a pair of boundary-anchored patterns:

    left context  [ boundary ]  right context

A left NFA is built for every rule; subset construction turns the union into the
DFA that tells which rules match the left side of a boundary. Right contexts are
short (at most three tokens) and are checked when the iterator has enough
lookahead, so they do not need to be part of the state.

Token normalization (LB9/LB10) happens in the iterator, not here: the alphabet
contains only the classes that can remain on a boundary, plus property flags for
Pi/Pf, East Asian width, Extended_Pictographic & Cn, U+25CC and a trailing ZWJ.

Run with ``--check`` in CI; the generated files are committed.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path

import generate_linebreak_properties as props

FLAG_BITS = {
    "pi": props.FLAG_PI,
    "pf": props.FLAG_PF,
    "ea": props.FLAG_EAST_ASIAN,
    "epCn": props.FLAG_EXTENDED_PICTOGRAPHIC_UNASSIGNED,
    "u25cc": props.FLAG_DOTTED_CIRCLE,
    "zwjTail": props.FLAG_ZWJ_TAIL,
}
ACTION_BITS = {"no_break": 0, "allow_break": 1, "mandatory_break": 2}
MAX_RIGHT = 3
# These classes only exist before LB1/LB9/LB10 normalization. A rule that names
# one of them would silently match nothing, which is almost always a typo.
NON_RUNTIME_CLASSES = {"AI", "SA", "SG", "XX", "CJ", "CM", "ZWJ"}


class CompileError(RuntimeError):
    pass


@dataclass(frozen=True)
class Token:
    klass: str
    flags: int

    @property
    def index(self) -> int:
        return props.CLASS_INDEX[self.klass]


@dataclass(frozen=True)
class RightPattern:
    masks: tuple[tuple[int, ...], ...]  # [position][word]


@dataclass
class CompiledRule:
    id: str
    priority: int
    action: int
    right: list[dict]
    index: int


def fail(message: str) -> None:
    raise CompileError(message)


def bit_count(value: int) -> int:
    return value.bit_count()


def predicate_matches(predicate: dict, token: Token | None, kind: str) -> bool:
    if "anyOf" in predicate:
        return any(predicate_matches(sub, token, kind) for sub in predicate["anyOf"])
    if predicate.get("sot"):
        return kind == "sot"
    if predicate.get("eot"):
        return kind == "eot"
    if kind != "token" or token is None:
        return False
    if "classes" in predicate and token.klass not in predicate["classes"]:
        return False
    if "notClasses" in predicate and token.klass in predicate["notClasses"]:
        return False
    for name in predicate.get("flagsAll", []):
        if not token.flags & FLAG_BITS[name]:
            return False
    for name in predicate.get("flagsNone", []):
        if token.flags & FLAG_BITS[name]:
            return False
    return True


def validate_predicate(predicate: dict, path: str) -> None:
    allowed = {"classes", "notClasses", "flagsAll", "flagsNone", "sot", "eot", "anyOf"}
    unknown = set(predicate) - allowed
    if unknown:
        fail(f"{path}: unknown predicate keys {sorted(unknown)}")
    if not predicate:
        fail(f"{path}: empty predicate")
    if "anyOf" in predicate:
        for index, sub in enumerate(predicate["anyOf"]):
            validate_predicate(sub, f"{path}.anyOf[{index}]")
        return
    if predicate.get("sot") and predicate.get("eot"):
        fail(f"{path}: sot and eot are mutually exclusive")
    for name in predicate.get("classes", []):
        if name not in props.CLASS_INDEX:
            fail(f"{path}: unknown class {name}")
        if name in NON_RUNTIME_CLASSES:
            fail(f"{path}: {name} cannot appear after LB1/LB9/LB10 normalization")
    for name in predicate.get("notClasses", []):
        if name not in props.CLASS_INDEX:
            fail(f"{path}: unknown class {name}")
    for name in predicate.get("flagsAll", []) + predicate.get("flagsNone", []):
        if name not in FLAG_BITS:
            fail(f"{path}: unknown flag {name}")


def validate_sequence(sequence: list, path: str, right: bool) -> None:
    for index, item in enumerate(sequence):
        item_path = f"{path}[{index}]"
        if "repeat" in item:
            if set(item) != {"repeat"}:
                fail(f"{item_path}: a repeat group cannot have sibling keys")
            group = item["repeat"]
            if set(group) != {"min", "items"} or group["min"] != 0:
                fail(f"{item_path}: repeat groups must be {{min: 0, items: [...]}}")
            for sub_index, sub in enumerate(group["items"]):
                validate_predicate(sub, f"{item_path}.items[{sub_index}]")
                if right and sub.get("sot"):
                    fail(f"{item_path}: sot cannot appear in a right context")
                if not right and sub.get("eot"):
                    fail(f"{item_path}: eot cannot appear in a left context")
        else:
            validate_predicate(item, item_path)
            if right and item.get("sot"):
                fail(f"{item_path}: sot cannot appear in a right context")
            if not right and item.get("eot"):
                fail(f"{item_path}: eot cannot appear in a left context")


def load_ucd(data_dir: Path, test_data: Path):
    version, _, inputs = props.load_inputs(data_dir, test_data)
    raw = props.fill_property(inputs["LineBreak.txt"].path, props.CLASS_NAMES, None)
    categories = props.fill_property(
        inputs["DerivedGeneralCategory.txt"].path, props.GENERAL_CATEGORIES, "Cn"
    )
    widths = props.fill_property(
        inputs["EastAsianWidth.txt"].path, props.EAST_ASIAN_WIDTHS, "N"
    )
    pictographic = props.parse_boolean_property(
        inputs["emoji-data.txt"].path, "Extended_Pictographic"
    )
    flags = props.build_flags(categories, widths, pictographic)
    category_to_name = {index: name for index, name in enumerate(props.GENERAL_CATEGORIES)}
    resolved = bytearray(props.CODE_POINT_COUNT)
    for code_point in range(props.CODE_POINT_COUNT):
        name = props.resolve_lb1(
            props.CLASS_NAMES[raw[code_point]], category_to_name[categories[code_point]]
        )
        resolved[code_point] = props.CLASS_INDEX[name]
    return version, resolved, flags


def build_token_universe(resolved: bytearray, flags: bytearray):
    base = {
        (props.CLASS_NAMES[resolved[code_point]], flags[code_point] & ~props.FLAG_ZWJ_TAIL)
        for code_point in range(props.CODE_POINT_COUNT)
    }
    tokens: list[Token] = []
    for klass, flag_value in sorted(base, key=lambda item: (props.CLASS_INDEX[item[0]], item[1])):
        tokens.append(Token(klass, flag_value))
        tokens.append(Token(klass, flag_value | props.FLAG_ZWJ_TAIL))
    if len(tokens) >= 255:
        fail(f"token universe has {len(tokens)} codes; the runtime uses a byte code with 255 free")
    return tokens


class NfaBuilder:
    def __init__(self) -> None:
        self.epsilon: list[set[int]] = []
        self.transitions: list[dict[int, set[int]]] = []
        self.accepts: dict[int, list[int]] = {}

    def new_state(self) -> int:
        self.epsilon.append(set())
        self.transitions.append({})
        return len(self.epsilon) - 1

    def add_epsilon(self, source: int, target: int) -> None:
        self.epsilon[source].add(target)

    def add_transition(self, source: int, code: int, target: int) -> None:
        self.transitions[source].setdefault(code, set()).add(target)

    def build_predicate(self, predicate: dict, codes: dict[int, Token]) -> tuple[int, int]:
        start = self.new_state()
        end = self.new_state()
        matched = 0
        for code, token in codes.items():
            if predicate_matches(predicate, token, "token"):
                self.add_transition(start, code, end)
                matched += 1
        if predicate_matches(predicate, None, "sot"):
            # Token code 0 is the virtual start-of-text token fed before the first
            # real token; it is not part of the real token universe.
            self.add_transition(start, 0, end)
            matched += 1
        if matched == 0:
            fail(f"predicate {json.dumps(predicate)} matches no token in the universe")
        return start, end

    def build_sequence(self, sequence: list, codes: dict[int, Token]) -> tuple[int, int]:
        if not sequence:
            start = self.new_state()
            end = self.new_state()
            self.add_epsilon(start, end)
            return start, end
        start = self.new_state()
        current = start
        for item in sequence:
            if "repeat" in item:
                inner_start, inner_end = self.build_sequence(item["repeat"]["items"], codes)
                loop_start = self.new_state()
                loop_end = self.new_state()
                self.add_epsilon(loop_start, inner_start)
                self.add_epsilon(loop_start, loop_end)
                self.add_epsilon(inner_end, inner_start)
                self.add_epsilon(inner_end, loop_end)
                self.add_epsilon(current, loop_start)
                current = loop_end
            else:
                item_start, item_end = self.build_predicate(item, codes)
                self.add_epsilon(current, item_start)
                current = item_end
        end = self.new_state()
        self.add_epsilon(current, end)
        return start, end

    def closure(self, states: set[int]) -> frozenset[int]:
        result = set(states)
        stack = list(states)
        while stack:
            state = stack.pop()
            for target in self.epsilon[state]:
                if target not in result:
                    result.add(target)
                    stack.append(target)
        return frozenset(result)


def build_right_masks(
    right: list[dict], codes: dict[int, Token], eot_bit: int, words: int
) -> tuple[tuple[int, ...], ...]:
    masks: list[tuple[int, ...]] = []
    for predicate in right:
        bits = [0] * words
        for code, token in codes.items():
            if predicate_matches(predicate, token, "token"):
                bits[code // 64] |= 1 << (code % 64)
        if predicate_matches(predicate, None, "eot"):
            bits[eot_bit // 64] |= 1 << (eot_bit % 64)
        masks.append(tuple(bits))
    return tuple(masks)


def compile_rules(
    rule_document: dict, codes: dict[int, Token], version: str
) -> tuple[NfaBuilder, list[CompiledRule]]:
    if rule_document.get("unicodeVersion") != version:
        fail(
            f"uax14_rules.json is for Unicode {rule_document.get('unicodeVersion')}, "
            f"data is {version}"
        )
    rules = rule_document.get("rules")
    if not isinstance(rules, list) or not rules:
        fail("uax14_rules.json has no rules")
    builder = NfaBuilder()
    compiled: list[CompiledRule] = []
    global_start = builder.new_state()
    for index, rule in enumerate(rules):
        path = f"rules[{index}]"
        if set(rule) != {"id", "priority", "action", "left", "right"}:
            fail(f"{path}: expected exactly id, priority, action, left, right")
        if rule["action"] not in ACTION_BITS:
            fail(f"{path}: unknown action {rule['action']}")
        if not isinstance(rule["priority"], int) or rule["priority"] <= 0:
            fail(f"{path}: invalid priority")
        if not rule["id"].startswith("LB"):
            fail(f"{path}: invalid rule id {rule['id']}")
        validate_sequence(rule["left"], f"{path}.left", right=False)
        validate_sequence(rule["right"], f"{path}.right", right=True)
        if len(rule["right"]) > MAX_RIGHT:
            fail(f"{path}: right context longer than {MAX_RIGHT} tokens")
        start, end = builder.build_sequence(rule["left"], codes)
        builder.add_epsilon(global_start, start)
        builder.accepts.setdefault(end, []).append(index)
        compiled.append(
            CompiledRule(
                id=rule["id"],
                priority=rule["priority"],
                action=ACTION_BITS[rule["action"]],
                right=rule["right"],
                index=index,
            )
        )
    return builder, compiled


def determinize(builder: NfaBuilder, compiled: list[CompiledRule], code_count: int):
    start = builder.closure({0})
    states: dict[frozenset[int], int] = {start: 0}
    order: list[frozenset[int]] = [start]
    transitions: list[list[int]] = []
    candidates: list[tuple[int, ...]] = []

    while len(order) > len(transitions):
        subset = order[len(transitions)]
        row: list[int] = []
        for code in range(code_count):
            moved: set[int] = set()
            for state in subset:
                moved.update(builder.transitions[state].get(code, set()))
            moved.add(0)
            target = builder.closure(moved)
            if target not in states:
                states[target] = len(order)
                order.append(target)
            row.append(states[target])
        transitions.append(row)
        found: set[int] = set()
        for state in subset:
            found.update(builder.accepts.get(state, []))
        candidates.append(
            tuple(sorted(found, key=lambda index: (compiled[index].priority, compiled[index].index)))
        )

    return transitions, candidates


def minimize(transitions: list[list[int]], candidates: list[tuple[int, ...]]):
    count = len(transitions)
    candidate_ids: dict[tuple[int, ...], int] = {}
    partition = [0] * count
    for state, candidate in enumerate(candidates):
        if candidate not in candidate_ids:
            candidate_ids[candidate] = len(candidate_ids)
        partition[state] = candidate_ids[candidate]
    while True:
        signatures: dict[tuple, int] = {}
        next_partition = [0] * count
        for state in range(count):
            signature = (partition[state], tuple(partition[target] for target in transitions[state]))
            if signature not in signatures:
                signatures[signature] = len(signatures)
            next_partition[state] = signatures[signature]
        if next_partition == partition:
            break
        partition = next_partition

    group_count = max(partition) + 1
    representatives = [-1] * group_count
    for state, group in enumerate(partition):
        if representatives[group] < 0:
            representatives[group] = state
    group_transitions: list[list[int]] = []
    group_candidates: list[tuple[int, ...]] = []
    for group in range(group_count):
        representative = representatives[group]
        group_transitions.append([partition[target] for target in transitions[representative]])
        group_candidates.append(candidates[representative])
    return partition, group_transitions, group_candidates


def merge_symbols(transitions: list[list[int]], code_count: int):
    signatures: dict[tuple[int, ...], int] = {}
    symbols: list[int] = []
    for code in range(code_count):
        signature = tuple(row[code] for row in transitions)
        if signature not in signatures:
            signatures[signature] = len(signatures)
        symbols.append(signatures[signature])
    symbol_transitions: list[list[int]] = []
    for row in transitions:
        merged = [0] * len(signatures)
        for code, symbol in enumerate(symbols):
            merged[symbol] = row[code]
        symbol_transitions.append(merged)
    return symbols, symbol_transitions


def build_dispatch(
    candidates: list[tuple[int, ...]],
    compiled: list[CompiledRule],
    right_masks: list[RightPattern],
    code_count: int,
):
    """Pre-index the common zero/one-token right contexts.

    Every boundary used to walk the whole candidate list. Most rules have an
    empty or one-token right context, so for each state and first right token we
    store the highest-priority matching candidate. The few multi-token right
    rules stay in a short per-state list and are checked in priority order.
    """
    code_slots = code_count + 1  # EOT is one extra slot
    best = [255] * (len(candidates) * code_slots)
    multi_starts: list[int] = []
    multi_counts: list[int] = []
    multi_rules: list[int] = []
    for state, state_candidates in enumerate(candidates):
        for code in range(code_slots):
            chosen = 255
            for rule in state_candidates:
                length = len(compiled[rule].right)
                if length == 0:
                    chosen = rule
                    break
                if length != 1:
                    continue
                mask = right_masks[rule].masks[0]
                if mask[code // 64] & (1 << (code % 64)):
                    chosen = rule
                    break
            best[state * code_slots + code] = chosen
        multi_starts.append(len(multi_rules))
        state_multi = [rule for rule in state_candidates if len(compiled[rule].right) > 1]
        multi_counts.append(len(state_multi))
        multi_rules.extend(state_multi)

    order = sorted(range(len(compiled)), key=lambda index: (compiled[index].priority, index))
    ranks = [0] * len(compiled)
    for rank, index in enumerate(order):
        ranks[index] = rank
    return best, multi_starts, multi_counts, multi_rules, ranks, code_slots


def emit_header(
    version: str, state_count: int, symbol_count: int, token_count: int, mask_words: int,
    initial_state: int, rule_count: int,
) -> str:
    return "\n".join(
        [
            "/*",
            " * Copyright (C) 2026 The Coral Project",
            " *",
            ' * Licensed under the Apache License, Version 2.0 (the "License");',
            " * you may not use this file except in compliance with the License.",
            " * You may obtain a copy of the License at",
            " *",
            " *      http://www.apache.org/licenses/LICENSE-2.0",
            " *",
            " * Unless required by applicable law or agreed to in writing, software",
            ' * distributed under the License is distributed on an "AS IS" BASIS,',
            " * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.",
            " * See the License for the specific language governing permissions and",
            " * limitations under the License.",
            " */",
            "",
            "/*",
            f" * Generated from Unicode {version} UAX #14 rules (tools/unicode/uax14_rules.json).",
            " * DO NOT EDIT BY HAND; see tools/unicode/generate_uax14_dfa.py.",
            " */",
            "",
            "#ifndef CORAL_UI_UNICODE_LINEBREAKDFA_GENERATED_H",
            "#define CORAL_UI_UNICODE_LINEBREAKDFA_GENERATED_H",
            "",
            "#include <cstdint>",
            "",
            '#include "unicode/LineBreakProperties.generated.h"',
            "",
            "namespace coral::ui::unicode {",
            "",
            "enum class BreakAction : uint8_t {",
            "  NoBreak = 0,",
            "  AllowBreak = 1,",
            "  MandatoryBreak = 2,",
            "};",
            "",
            f"inline constexpr uint16_t kLineBreakDfaStateCount = {state_count};",
            f"inline constexpr uint16_t kLineBreakDfaSymbolCount = {symbol_count};",
            f"inline constexpr uint16_t kLineBreakTokenCodeCount = {token_count};",
            f"inline constexpr uint16_t kLineBreakDfaInitialState = {initial_state};",
            f"inline constexpr uint16_t kLineBreakDfaRuleCount = {rule_count};",
            f"inline constexpr uint8_t kLineBreakDfaMaskWords = {mask_words};",
            "inline constexpr uint8_t kLineBreakSotTokenCode = 0;",
            "inline constexpr uint8_t kLineBreakEotTokenCode = 255;",
            "",
            "/** @return the DFA state after @p symbol from @p state. */",
            "[[nodiscard]] uint16_t lineBreakDfaTransition(uint16_t state, uint8_t symbol) noexcept;",
            "",
            "/** @return the canonical DFA symbol for a token code. */",
            "[[nodiscard]] uint8_t lineBreakDfaTokenSymbol(uint8_t tokenCode) noexcept;",
            "",
            "/**",
            " * @return the token code for a resolved class plus context flags, or 255 when the",
            " * combination is not present in Unicode data (callers should use kLineBreakEotTokenCode).",
            " */",
            "[[nodiscard]] uint8_t lineBreakDfaTokenCode(LineBreakClass klass, uint8_t flags) noexcept;",
            "",
            "/** @return the sorted candidate rule alternatives for a DFA state. */",
            "[[nodiscard]] const uint16_t* lineBreakDfaCandidates(uint16_t state, uint8_t& count) noexcept;",
            "",
            "/**",
            " * @return the highest-priority candidate whose right context is empty or one token,",
            " * for @p code0 (kLineBreakEotTokenCode is the end-of-text token), or 255.",
            " */",
            "[[nodiscard]] uint8_t lineBreakDfaBestRule(uint16_t state, uint8_t code0) noexcept;",
            "",
            "/** @return the candidates with a right context of two or three tokens. */",
            "[[nodiscard]] const uint16_t* lineBreakDfaMultiCandidates(uint16_t state, uint8_t& count) noexcept;",
            "",
            "[[nodiscard]] uint16_t lineBreakDfaRuleRank(uint16_t rule) noexcept;",
            "",
            "[[nodiscard]] uint8_t lineBreakDfaRuleAction(uint16_t rule) noexcept;",
            "[[nodiscard]] uint8_t lineBreakDfaRuleRightLength(uint16_t rule) noexcept;",
            "[[nodiscard]] const uint64_t* lineBreakDfaRuleRightMask(uint16_t rule, uint8_t position) noexcept;",
            "[[nodiscard]] const char* lineBreakDfaRuleId(uint16_t rule) noexcept;",
            "",
            "} // namespace coral::ui::unicode",
            "",
            "#endif // CORAL_UI_UNICODE_LINEBREAKDFA_GENERATED_H",
            "",
        ]
    )


def emit_source(
    version: str,
    state_count: int,
    symbol_count: int,
    token_count: int,
    transitions: list[list[int]],
    symbols: list[int],
    candidates: list[tuple[int, ...]],
    candidate_starts: list[int],
    candidate_counts: list[int],
    candidate_flat: list[int],
    best_rules: list[int],
    multi_starts: list[int],
    multi_counts: list[int],
    multi_rules: list[int],
    rule_ranks: list[int],
    code_slots: int,
    token_codes: list[list[int]],
    compiled: list[CompiledRule],
    right_masks: list[RightPattern],
    mask_words: int,
) -> str:
    # Deduplicate transition rows. The minimized DFA has many states whose whole
    # 125-symbol row is identical (the first ~47 columns are often a linear prefix),
    # and every state fits in one byte, so a byte row table plus a byte state map is
    # several times smaller than the dense uint16 matrix.
    unique_rows: list[list[int]] = []
    row_index: list[int] = []
    row_lookup: dict[tuple[int, ...], int] = {}
    for row in transitions:
        key = tuple(row)
        index = row_lookup.get(key)
        if index is None:
            index = len(unique_rows)
            row_lookup[key] = index
            unique_rows.append(row)
        row_index.append(index)
    row_type = "uint8_t" if state_count <= 0xFF else "uint16_t"
    index_type = "uint8_t" if len(unique_rows) <= 0xFF else "uint16_t"

    lines = [
        "/*",
        " * Copyright (C) 2026 The Coral Project",
        " *",
        ' * Licensed under the Apache License, Version 2.0 (the "License");',
        " * you may not use this file except in compliance with the License.",
        " * You may obtain a copy of the License at",
        " *",
        " *      http://www.apache.org/licenses/LICENSE-2.0",
        " *",
        " * Unless required by applicable law or agreed to in writing, software",
        ' * distributed under the License is distributed on an "AS IS" BASIS,',
        " * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.",
        " * See the License for the specific language governing permissions and",
        " * limitations under the License.",
        " */",
        "",
        "/*",
        f" * Generated from Unicode {version} UAX #14 rules.",
        " * DO NOT EDIT BY HAND; see tools/unicode/generate_uax14_dfa.py.",
        " */",
        "",
        '#include "unicode/LineBreakDfa.generated.h"',
        "",
        "namespace coral::ui::unicode {",
        "namespace {",
        "",
        f"constexpr {row_type} kTransitionRows[{len(unique_rows)}][{symbol_count}] = {{",
    ]
    for row in unique_rows:
        lines.append("  {" + ", ".join(str(value) for value in row) + "},")
    lines.extend(
        [
            "};",
            "",
            f"constexpr {index_type} kTransitionRowForState[{state_count}] = {{",
        ]
    )
    for start in range(0, state_count, 32):
        lines.append("  " + ", ".join(str(value) for value in row_index[start:start + 32]) + ",")
    lines.extend(
        [
            "};",
            "",
            "static_assert(sizeof(kTransitionRows) <= 64 * 1024,",
            '  "the DFA transition table must stay within the 64 KiB mobile budget");',
            "",
            f"constexpr uint8_t kTokenToSymbol[{token_count}] = {{",
        ]
    )
    for start in range(0, token_count, 16):
        lines.append("  " + ", ".join(str(value) for value in symbols[start:start + 16]) + ",")
    lines.extend(["};", "", f"constexpr uint8_t kTokenCodes[{len(token_codes)}][64] = {{"])
    for row in token_codes:
        lines.append("  {" + ", ".join(str(value) for value in row) + "},")
    lines.extend(
        [
            "};",
            "",
            f"constexpr uint16_t kCandidateRules[{len(candidate_flat)}] = {{",
        ]
    )
    for start in range(0, len(candidate_flat), 16):
        lines.append("  " + ", ".join(str(value) for value in candidate_flat[start:start + 16]) + ",")
    lines.extend(["};", "", f"constexpr uint16_t kCandidateStart[{state_count}] = {{"])
    for start in range(0, state_count, 16):
        lines.append("  " + ", ".join(str(value) for value in candidate_starts[start:start + 16]) + ",")
    lines.extend(["};", "", f"constexpr uint8_t kCandidateCount[{state_count}] = {{"])
    for start in range(0, state_count, 32):
        lines.append("  " + ", ".join(str(value) for value in candidate_counts[start:start + 32]) + ",")
    lines.extend(
        [
            "};",
            "",
            f"constexpr uint8_t kBestRule[{state_count} * {code_slots}] = {{",
        ]
    )
    for start in range(0, len(best_rules), 32):
        lines.append("  " + ", ".join(str(value) for value in best_rules[start:start + 32]) + ",")
    lines.extend(["};", "", f"constexpr uint16_t kMultiStart[{state_count}] = {{"])
    for start in range(0, state_count, 16):
        lines.append("  " + ", ".join(str(value) for value in multi_starts[start:start + 16]) + ",")
    lines.extend(["};", "", f"constexpr uint8_t kMultiCount[{state_count}] = {{"])
    for start in range(0, state_count, 32):
        lines.append("  " + ", ".join(str(value) for value in multi_counts[start:start + 32]) + ",")
    lines.extend(["};", "", f"constexpr uint16_t kMultiRules[{len(multi_rules)}] = {{"])
    for start in range(0, len(multi_rules), 16):
        lines.append("  " + ", ".join(str(value) for value in multi_rules[start:start + 16]) + ",")
    lines.extend(
        [
            "};",
            "",
            "static_assert(sizeof(kTransitionRows) + sizeof(kTransitionRowForState) + "
            "sizeof(kBestRule) + sizeof(kMultiRules) <= 64 * 1024,",
            '  "the DFA tables must stay within the 64 KiB mobile budget");',
            "",
            f"constexpr uint16_t kRuleRank[{len(compiled)}] = {{",
            "  " + ", ".join(str(rank) for rank in rule_ranks) + ",",
            "};",
            "",
            f"constexpr uint8_t kRuleAction[{len(compiled)}] = {{",
            "  " + ", ".join(str(rule.action) for rule in compiled) + ",",
            "};",
            "",
            f"constexpr uint8_t kRuleRightLength[{len(compiled)}] = {{",
            "  " + ", ".join(str(len(rule.right)) for rule in compiled) + ",",
            "};",
            "",
            f"constexpr uint64_t kRuleRightMasks[{len(compiled)} * {MAX_RIGHT} * {mask_words}] = {{",
        ]
    )
    for pattern in right_masks:
        for position in range(MAX_RIGHT):
            words = pattern.masks[position] if position < len(pattern.masks) else (0,) * mask_words
            lines.append("  " + ", ".join(f"0x{word:016X}ull" for word in words) + ",")
    lines.extend(
        [
            "};",
            "",
            "constexpr const char* kRuleIds[] = {",
        ]
    )
    for rule in compiled:
        lines.append(f'  "{rule.id}",')
    lines.extend(
        [
            "};",
            "",
            "} // namespace",
            "",
            "uint16_t lineBreakDfaTransition(const uint16_t state, const uint8_t symbol) noexcept {",
            "  if (state >= kLineBreakDfaStateCount || symbol >= kLineBreakDfaSymbolCount) return 0;",
            "  return kTransitionRows[kTransitionRowForState[state]][symbol];",
            "}",
            "",
            "uint8_t lineBreakDfaTokenSymbol(const uint8_t tokenCode) noexcept {",
            "  return tokenCode < kLineBreakTokenCodeCount ? kTokenToSymbol[tokenCode] : 0;",
            "}",
            "",
            "uint8_t lineBreakDfaTokenCode(const LineBreakClass klass, const uint8_t flags) noexcept {",
            "  const auto classIndex = static_cast<size_t>(klass);",
            "  if (classIndex >= kLineBreakClassCount) return kLineBreakEotTokenCode;",
            "  return kTokenCodes[classIndex][flags & 0x3Fu];",
            "}",
            "",
            "const uint16_t* lineBreakDfaCandidates(const uint16_t state, uint8_t& count) noexcept {",
            "  if (state >= kLineBreakDfaStateCount) {",
            "    count = 0;",
            "    return nullptr;",
            "  }",
            "  count = kCandidateCount[state];",
            "  return kCandidateRules + kCandidateStart[state];",
            "}",
            "",
            "uint8_t lineBreakDfaBestRule(const uint16_t state, const uint8_t code0) noexcept {",
            "  if (state >= kLineBreakDfaStateCount) return 255;",
            "  const uint16_t slot = code0 == kLineBreakEotTokenCode",
            "    ? static_cast<uint16_t>(kLineBreakTokenCodeCount)",
            "    : static_cast<uint16_t>(code0);",
            "  if (slot > kLineBreakTokenCodeCount) return 255;",
            "  return kBestRule[static_cast<size_t>(state) * (kLineBreakTokenCodeCount + 1) + slot];",
            "}",
            "",
            "const uint16_t* lineBreakDfaMultiCandidates(const uint16_t state, uint8_t& count) noexcept {",
            "  if (state >= kLineBreakDfaStateCount) {",
            "    count = 0;",
            "    return nullptr;",
            "  }",
            "  count = kMultiCount[state];",
            "  return kMultiRules + kMultiStart[state];",
            "}",
            "",
            "uint16_t lineBreakDfaRuleRank(const uint16_t rule) noexcept {",
            "  return rule < kLineBreakDfaRuleCount ? kRuleRank[rule] : 0xFFFFu;",
            "}",
            "",
            "uint8_t lineBreakDfaRuleAction(const uint16_t rule) noexcept {",
            "  return rule < kLineBreakDfaRuleCount ? kRuleAction[rule] : 0;",
            "}",
            "",
            "uint8_t lineBreakDfaRuleRightLength(const uint16_t rule) noexcept {",
            "  return rule < kLineBreakDfaRuleCount ? kRuleRightLength[rule] : 0;",
            "}",
            "",
            "const uint64_t* lineBreakDfaRuleRightMask(const uint16_t rule, const uint8_t position) noexcept {",
            "  if (rule >= kLineBreakDfaRuleCount || position >= kLineBreakDfaMaskWords * 3) return nullptr;",
            "  return kRuleRightMasks + (static_cast<size_t>(rule) * 3 + position) * kLineBreakDfaMaskWords;",
            "}",
            "",
            "const char* lineBreakDfaRuleId(const uint16_t rule) noexcept {",
            "  return rule < kLineBreakDfaRuleCount ? kRuleIds[rule] : \"?\";",
            "}",
            "",
            "} // namespace coral::ui::unicode",
            "",
        ]
    )
    # candidate_counts is filled by the caller via a closure variable; this is
    # replaced below before emission.
    return "\n".join(lines)


def main(argv: list[str]) -> int:
    script = Path(__file__).resolve()
    ui_root = script.parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-dir", type=Path, default=script.parent / "data")
    parser.add_argument(
        "--rules", type=Path, default=script.parent / "uax14_rules.json"
    )
    parser.add_argument(
        "--test-data",
        type=Path,
        default=ui_root / "tests/unicode/LineBreakTest.txt",
    )
    parser.add_argument("--output-dir", type=Path, default=ui_root / "src/unicode")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args(argv)

    try:
        data_dir = args.data_dir.resolve()
        output_dir = args.output_dir.resolve()
        version, resolved, flags = load_ucd(data_dir, args.test_data.resolve())
        tokens = build_token_universe(resolved, flags)
        codes: dict[int, Token] = {index + 1: token for index, token in enumerate(tokens)}
        code_count = len(codes) + 1  # code 0 is sot
        eot_bit = code_count
        mask_words = (code_count + 1 + 63) // 64

        rule_document = json.loads(args.rules.resolve().read_text(encoding="utf-8"))
        builder, compiled = compile_rules(rule_document, codes, version)
        transitions, candidates = determinize(builder, compiled, code_count)
        partition, group_transitions, group_candidates = minimize(transitions, candidates)
        symbols, symbol_transitions = merge_symbols(group_transitions, code_count)
        # merge_symbols returns one canonical symbol id per token code, but each
        # transition row only has one column per distinct symbol (the canonical
        # ids are dense, so this is max(symbols)+1).
        symbol_width = len(symbol_transitions[0]) if symbol_transitions else 0

        start_group = partition[0]
        # The iterator starts after the virtual sot token: state = transition(start, code 0).
        initial_state = partition[transitions[0][0]]

        candidate_starts: list[int] = []
        candidate_flat: list[int] = []
        candidate_counts: list[int] = []
        for candidate in group_candidates:
            candidate_starts.append(len(candidate_flat))
            candidate_counts.append(len(candidate))
            candidate_flat.extend(candidate)

        token_codes = [[255] * 64 for _ in range(len(props.CLASS_NAMES))]
        for code, token in codes.items():
            token_codes[token.index][token.flags & 0x3F] = code

        right_masks: list[RightPattern] = []
        for rule in compiled:
            right_masks.append(
                RightPattern(build_right_masks(rule.right, codes, eot_bit, mask_words))
                if rule.right
                else RightPattern(())
            )
        (
            best_rules,
            multi_starts,
            multi_counts,
            multi_rules,
            rule_ranks,
            code_slots,
        ) = build_dispatch(group_candidates, compiled, right_masks, code_count)

        header = emit_header(
            version, len(symbol_transitions), symbol_width, code_count, mask_words,
            initial_state, len(compiled),
        )
        source = emit_source(
            version, len(symbol_transitions), symbol_width, code_count, symbol_transitions,
            symbols, group_candidates, candidate_starts, candidate_counts, candidate_flat,
            best_rules, multi_starts, multi_counts, multi_rules, rule_ranks, code_slots,
            token_codes, compiled, right_masks, mask_words,
        )
        files = {
            output_dir / "LineBreakDfa.generated.h": header,
            # The implementation is included by LineBreakIterator.cpp so the
            # compiler can inline the table accessors on the hot path.
            output_dir / "LineBreakDfa.generated.inc": source,
        }
        if not args.quiet:
            print(
                f"UAX #14 DFA: {len(symbol_transitions)} states, {symbol_width} symbols, "
                f"{code_count} token codes, {len(compiled)} rule alternatives, "
                f"{len(set(map(tuple, symbol_transitions)))} transition rows"
            )
        ok = True
        for path, content in files.items():
            if not props.compare_and_write(path, content, args.check):
                ok = False
        return 0 if ok else 1
    except (CompileError, props.GenerationError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
