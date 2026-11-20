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
"""Generate the C++ Line_Break property tables from the committed UCD data.

This is stage 0 of the UAX #14 work described in
``docs/Unicode文本断行与Bidi架构设计.md``. It deliberately contains no rule
compiler: it only turns ``LineBreak.txt`` plus ``DerivedGeneralCategory.txt``
into a deterministic range table and the LB1-resolved class table.

The generated files are committed. Re-run with ``--check`` in CI to prove the
checkout is reproducible without touching the working tree.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path

# The enum order is the uppercase alphabetical UCD order. Appending a class in a
# future Unicode version is fine; reordering changes the generated table's
# on-disk values and is treated as a deliberate regeneration.
CLASS_NAMES = [
    "AI", "AK", "AL", "AP", "AS", "B2", "BA", "BB", "BK", "CB", "CJ", "CL", "CM",
    "CP", "CR", "EB", "EM", "EX", "GL", "H2", "H3", "HH", "HL", "HY", "ID", "IN",
    "IS", "JL", "JT", "JV", "LF", "NL", "NS", "NU", "OP", "PO", "PR", "QU", "RI",
    "SA", "SG", "SP", "SY", "VF", "VI", "WJ", "XX", "ZW", "ZWJ",
]
CLASS_INDEX = {name: index for index, name in enumerate(CLASS_NAMES)}
GENERAL_CATEGORIES = [
    "Cc", "Cf", "Cn", "Co", "Cs", "Ll", "Lm", "Lo", "Lt", "Lu", "Mc", "Me", "Mn",
    "Nd", "Nl", "No", "Pc", "Pd", "Pe", "Pf", "Pi", "Po", "Ps", "Sc", "Sk", "Sm",
    "So", "Zl", "Zp", "Zs",
]
EAST_ASIAN_WIDTHS = ["A", "F", "H", "N", "Na", "W"]
FLAG_PI = 1 << 0
FLAG_PF = 1 << 1
FLAG_EAST_ASIAN = 1 << 2
FLAG_EXTENDED_PICTOGRAPHIC_UNASSIGNED = 1 << 3
FLAG_DOTTED_CIRCLE = 1 << 4
FLAG_ZWJ_TAIL = 1 << 5
MAX_CODE_POINT = 0x10FFFF
CODE_POINT_COUNT = MAX_CODE_POINT + 1
UNSET = 0xFF

PROPERTY_LINE = re.compile(
    r"^([0-9A-Fa-f]+)(?:\.\.([0-9A-Fa-f]+))?\s*;\s*([A-Za-z0-9_]+)"
)
MISSING_LINE = re.compile(
    r"^#\s*@missing:\s*([0-9A-Fa-f]+)(?:\.\.([0-9A-Fa-f]+))?\s*;\s*([A-Za-z0-9_]+)"
)
EMOJI_VERSION_LINE = re.compile(r"^#\s*Version:\s*([0-9]+\.[0-9]+\.[0-9]+)", re.MULTILINE)
HEADER_VERSION = {
    "LineBreak.txt": re.compile(r"^#\s*LineBreak-([0-9]+\.[0-9]+\.[0-9]+)\.txt"),
    "DerivedGeneralCategory.txt": re.compile(
        r"^#\s*DerivedGeneralCategory-([0-9]+\.[0-9]+\.[0-9]+)\.txt"
    ),
    "EastAsianWidth.txt": re.compile(
        r"^#\s*EastAsianWidth-([0-9]+\.[0-9]+\.[0-9]+)\.txt"
    ),
    "emoji-data.txt": EMOJI_VERSION_LINE,
    "LineBreakTest.txt": re.compile(r"^#\s*LineBreakTest-([0-9]+\.[0-9]+\.[0-9]+)\.txt"),
}


class GenerationError(RuntimeError):
    pass


@dataclass(frozen=True)
class SourceRange:
    first: int
    last: int
    value: str


@dataclass(frozen=True)
class Input:
    path: Path
    sha256: str
    date: str
    url: str


def fail(message: str) -> None:
    raise GenerationError(message)


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as error:
        fail(f"cannot read {path}: {error}")


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def header_version(path: Path, name: str) -> str:
    text = read_text(path)
    if name == "emoji-data.txt":
        match = EMOJI_VERSION_LINE.search(text)
        if match is None:
            fail(f"{path}: missing '# Version: <version>' line")
        return match.group(1)
    first_line = text.splitlines()[0] if text else ""
    match = HEADER_VERSION[name].match(first_line)
    if match is None:
        fail(f"{path}: expected a '# {name.rsplit('.', 1)[0]}-<version>.txt' header, got {first_line!r}")
    return match.group(1)


def parse_ranges(path: Path) -> tuple[list[SourceRange], list[SourceRange]]:
    """Return (explicit ranges, @missing ranges) in file order."""
    ranges: list[SourceRange] = []
    missing: list[SourceRange] = []
    for number, line in enumerate(read_text(path).splitlines(), 1):
        missing_match = MISSING_LINE.match(line)
        if missing_match is not None:
            first = int(missing_match.group(1), 16)
            last = int(missing_match.group(2) or missing_match.group(1), 16)
            missing.append(SourceRange(first, last, missing_match.group(3)))
            continue
        match = PROPERTY_LINE.match(line)
        if match is None:
            continue
        first = int(match.group(1), 16)
        last = int(match.group(2) or match.group(1), 16)
        if first > last or last > MAX_CODE_POINT:
            fail(f"{path}:{number}: invalid range U+{first:04X}..U+{last:04X}")
        ranges.append(SourceRange(first, last, match.group(3)))
    return ranges, missing


def fill_property(
    path: Path, value_names: list[str] | None, default_name: str | None
) -> bytearray:
    """Expand a property file into one byte per code point.

    ``value_names`` is the closed vocabulary (class names or GC names). A file
    with no ``@missing`` directives is initialized to ``default_name``; otherwise
    the missing directives must cover every code point.
    """
    ranges, missing = parse_ranges(path)
    vocabulary = value_names or []
    for source in ranges + missing:
        if source.value not in vocabulary:
            fail(f"{path}: unknown value {source.value!r} in U+{source.first:04X}..U+{source.last:04X}")

    table = bytearray([UNSET]) * CODE_POINT_COUNT
    if default_name is not None:
        table[:] = bytes([vocabulary.index(default_name)]) * CODE_POINT_COUNT

    for source in missing:
        start = vocabulary.index(source.value)
        table[source.first:source.last + 1] = bytes([start]) * (source.last - source.first + 1)

    used = bytearray(CODE_POINT_COUNT)
    for source in ranges:
        start = vocabulary.index(source.value)
        for code_point in range(source.first, source.last + 1):
            if used[code_point]:
                fail(
                    f"{path}: overlapping ranges at U+{code_point:04X} "
                    f"({source.value} after an earlier range)"
                )
            used[code_point] = 1
            table[code_point] = start

    if UNSET in table:
        first = table.index(UNSET)
        fail(f"{path}: U+{first:04X} has no value (missing @missing coverage)")
    return table


def resolve_lb1(raw: str, general_category: str) -> str:
    """UAX #14 LB1: resolve AI/SG/XX, SA and CJ before any other rule runs."""
    if raw in ("AI", "SG", "XX"):
        return "AL"
    if raw == "CJ":
        return "NS"
    if raw == "SA":
        return "CM" if general_category in ("Mn", "Mc") else "AL"
    return raw


def parse_boolean_property(path: Path, property_name: str) -> bytearray:
    """Expand one boolean UCD property (e.g. Extended_Pictographic) to a byte per code point."""
    table = bytearray(CODE_POINT_COUNT)
    for number, line in enumerate(read_text(path).splitlines(), 1):
        match = PROPERTY_LINE.match(line)
        if match is None or match.group(3) != property_name:
            continue
        first = int(match.group(1), 16)
        last = int(match.group(2) or match.group(1), 16)
        if first > last or last > MAX_CODE_POINT:
            fail(f"{path}:{number}: invalid range U+{first:04X}..U+{last:04X}")
        table[first:last + 1] = bytes([1]) * (last - first + 1)
    return table


def build_flags(
    categories: bytearray, east_asian_widths: bytearray, extended_pictographic: bytearray
) -> bytearray:
    """Derive the UAX #14 context bits that are not part of Line_Break itself."""
    flags = bytearray(CODE_POINT_COUNT)
    for code_point in range(CODE_POINT_COUNT):
        value = 0
        category = categories[code_point]
        if GENERAL_CATEGORIES[category] == "Pi":
            value |= FLAG_PI
        elif GENERAL_CATEGORIES[category] == "Pf":
            value |= FLAG_PF
        if EAST_ASIAN_WIDTHS[east_asian_widths[code_point]] in ("F", "W", "H"):
            value |= FLAG_EAST_ASIAN
        if extended_pictographic[code_point] and GENERAL_CATEGORIES[category] == "Cn":
            value |= FLAG_EXTENDED_PICTOGRAPHIC_UNASSIGNED
        if code_point == 0x25CC:
            value |= FLAG_DOTTED_CIRCLE
        flags[code_point] = value
    return flags


def build_flag_ranges(flags: bytearray) -> list[tuple[int, int, int]]:
    """Run-length encode only the ranges that carry at least one flag bit."""
    ranges: list[tuple[int, int, int]] = []
    start = 0
    previous = flags[0]
    for code_point in range(1, CODE_POINT_COUNT):
        current = flags[code_point]
        if current != previous:
            if previous != 0:
                ranges.append((start, code_point - 1, previous))
            start = code_point
            previous = current
    if previous != 0:
        ranges.append((start, MAX_CODE_POINT, previous))
    return ranges


def build_ranges(raw: bytearray, resolved: bytearray) -> list[tuple[int, int, str, str]]:
    ranges: list[tuple[int, int, str, str]] = []
    start = 0
    previous = (raw[0], resolved[0])
    for code_point in range(1, CODE_POINT_COUNT):
        current = (raw[code_point], resolved[code_point])
        if current != previous:
            ranges.append(
                (
                    start,
                    code_point - 1,
                    CLASS_NAMES[previous[0]],
                    CLASS_NAMES[previous[1]],
                )
            )
            start = code_point
            previous = current
    ranges.append(
        (start, MAX_CODE_POINT, CLASS_NAMES[previous[0]], CLASS_NAMES[previous[1]])
    )
    if ranges[0][0] != 0 or ranges[-1][1] != MAX_CODE_POINT:
        fail("generated ranges do not cover the full code point space")
    return ranges


def emit_header(
    version: str,
    inputs: dict[str, Input],
    range_count: int,
    flag_range_count: int,
    ascii_properties: list[tuple[str, int]],
) -> str:
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
        f" * Generated from Unicode {version} LineBreak.txt and DerivedGeneralCategory.txt.",
        " * DO NOT EDIT BY HAND. Regenerate with:",
        " *",
        " *   python tools/unicode/generate_linebreak_properties.py",
        " *",
        f" * LineBreak.txt:           {inputs['LineBreak.txt'].url}",
        f" *                          sha256 {inputs['LineBreak.txt'].sha256}",
        f" * DerivedGeneralCategory:  {inputs['DerivedGeneralCategory.txt'].url}",
        f" *                          sha256 {inputs['DerivedGeneralCategory.txt'].sha256}",
        f" * EastAsianWidth:           {inputs['EastAsianWidth.txt'].url}",
        f" *                          sha256 {inputs['EastAsianWidth.txt'].sha256}",
        f" * emoji-data:              {inputs['emoji-data.txt'].url}",
        f" *                          sha256 {inputs['emoji-data.txt'].sha256}",
        " */",
        "",
        "#ifndef CORAL_UI_UNICODE_LINEBREAKPROPERTIES_GENERATED_H",
        "#define CORAL_UI_UNICODE_LINEBREAKPROPERTIES_GENERATED_H",
        "",
        "#include <cstddef>",
        "#include <cstdint>",
        "#include <string_view>",
        "",
        "namespace coral::ui::unicode {",
        "",
        f"/** Line_Break classes of Unicode {version}. The order is the UCD alphabetical order. */",
        "enum class LineBreakClass : uint8_t {",
    ]
    for name in CLASS_NAMES:
        lines.append(f"  {name},")
    lines.extend(
        [
            "  Count,",
            "};",
            "",
            "inline constexpr size_t kLineBreakClassCount =",
            "  static_cast<size_t>(LineBreakClass::Count);",
            "",
            "/** One run of code points that share the raw and LB1-resolved Line_Break class. */",
            "struct LineBreakRange {",
            "  uint32_t first;",
            "  uint32_t last;",
            "  LineBreakClass value;",
            "  LineBreakClass resolved;",
            "};",
            "",
            "/** @return the raw Line_Break property; XX for unassigned or invalid code points. */",
            "[[nodiscard]] LineBreakClass lineBreakClass(uint32_t codePoint) noexcept;",
            "",
            "/**",
            " * @return the class after UAX #14 LB1 (AI/SG/XX -> AL, CJ -> NS, SA -> CM or AL).",
            " * Invalid code points return XX; LB9/LB10 attachment is a runtime concern, not LB1.",
            " */",
            "[[nodiscard]] LineBreakClass resolveLineBreakClass(uint32_t codePoint) noexcept;",
            "",
            "/** @return the UCD name, e.g. \"ID\"; never null. */",
            "[[nodiscard]] const char* lineBreakClassName(LineBreakClass value) noexcept;",
            "",
            "/** @return true and sets @p value when @p name is a known class name. */",
            "[[nodiscard]] bool lineBreakClassFromName(std::string_view name,",
            "  LineBreakClass& value) noexcept;",
            "",
            f"/**",
            f" * @return range @p index of the {range_count} contiguous ranges, materialized on demand.",
            " * The generated table stores only end boundaries and class-pair ids to stay compact.",
            " */",
            "[[nodiscard]] LineBreakRange lineBreakRangeAt(size_t index) noexcept;",
            "[[nodiscard]] size_t lineBreakRangeCount() noexcept;",
            "",
            "/** Context bits from properties other than Line_Break (UAX #14 LB15a/b, LB19a, LB30, LB30b). */",
            "inline constexpr uint8_t kLineBreakFlagPi = 1u << 0;",
            "inline constexpr uint8_t kLineBreakFlagPf = 1u << 1;",
            "inline constexpr uint8_t kLineBreakFlagEastAsian = 1u << 2;",
            "inline constexpr uint8_t kLineBreakFlagExtendedPictographicUnassigned = 1u << 3;",
            "inline constexpr uint8_t kLineBreakFlagDottedCircle = 1u << 4;",
            "inline constexpr uint8_t kLineBreakFlagZwjTail = 1u << 5;",
            "",
            "struct LineBreakFlagRange {",
            "  uint32_t first;",
            "  uint32_t last;",
            "  uint8_t flags;",
            "};",
            "",
            "/** @return the UAX #14 context bits for @p codePoint; 0 when none apply. */",
            "[[nodiscard]] uint8_t lineBreakFlags(uint32_t codePoint) noexcept;",
            "",
            f"/** @return the {flag_range_count} non-zero flag ranges. */",
            "[[nodiscard]] const LineBreakFlagRange* lineBreakFlagRanges() noexcept;",
            "[[nodiscard]] size_t lineBreakFlagRangeCount() noexcept;",
            "",
            "/** Hot-path table for ASCII: resolved class and context flags without a range search. */",
            "struct AsciiLineBreakProperty {",
            "  LineBreakClass value;",
            "  uint8_t flags;",
            "};",
            "",
            "inline constexpr AsciiLineBreakProperty kAsciiLineBreakProperties[128] = {",
        ]
    )
    for index, (value, flag_value) in enumerate(ascii_properties):
        lines.append(f"  {{ LineBreakClass::{value}, 0x{flag_value:02X}u }}, // U+{index:04X}")
    lines.extend(
        [
            "};",
            "",
            "} // namespace coral::ui::unicode",
            "",
            "#endif // CORAL_UI_UNICODE_LINEBREAKPROPERTIES_GENERATED_H",
            "",
        ]
    )
    return "\n".join(lines)


def emit_source(
    version: str,
    inputs: dict[str, Input],
    ranges: list[tuple[int, int, str, str]],
    flag_ranges: list[tuple[int, int, int]],
) -> str:
    # Store the class table as end boundaries plus a deduplicated class-pair id.
    # Ranges are contiguous and sorted, so range i starts at end[i-1] + 1; this
    # keeps the table at roughly a fifth of a full (first, last, raw, resolved)
    # record array while the binary search also scans a much smaller array.
    class_pairs: list[tuple[str, str]] = []
    pair_ids: dict[tuple[str, str], int] = {}
    range_pair_ids: list[int] = []
    range_ends: list[int] = []
    for _, last, value, resolved in ranges:
        key = (value, resolved)
        pair_id = pair_ids.get(key)
        if pair_id is None:
            pair_id = len(class_pairs)
            pair_ids[key] = pair_id
            class_pairs.append(key)
        range_pair_ids.append(pair_id)
        range_ends.append(last)

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
        f" * Generated from Unicode {version} LineBreak.txt and DerivedGeneralCategory.txt.",
        " * DO NOT EDIT BY HAND; see tools/unicode/generate_linebreak_properties.py.",
        " */",
        "",
        '#include "unicode/LineBreakProperties.generated.h"',
        "",
        "#include <algorithm>",
        "#include <cstddef>",
        "",
        "namespace coral::ui::unicode {",
        "namespace {",
        "",
        f"constexpr uint32_t kRangeEnds[{len(range_ends)}] = {{",
    ]
    for start in range(0, len(range_ends), 12):
        lines.append("  " + ", ".join(f"0x{value:04X}u" for value in range_ends[start:start + 12]) + ",")
    lines.extend(["};", "", f"constexpr uint8_t kRangePairIndex[{len(range_pair_ids)}] = {{"])
    for start in range(0, len(range_pair_ids), 24):
        lines.append("  " + ", ".join(str(value) for value in range_pair_ids[start:start + 24]) + ",")
    lines.extend(["};", "", f"constexpr LineBreakClass kPairValue[{len(class_pairs)}] = {{"])
    lines.append("  " + ", ".join(f"LineBreakClass::{value}" for value, _ in class_pairs) + ",")
    lines.extend(["};", "", f"constexpr LineBreakClass kPairResolved[{len(class_pairs)}] = {{"])
    lines.append("  " + ", ".join(f"LineBreakClass::{resolved}" for _, resolved in class_pairs) + ",")
    lines.extend(
        [
            "};",
            "",
            "constexpr size_t kRangeCount = sizeof(kRangeEnds) / sizeof(kRangeEnds[0]);",
            "",
            f"constexpr LineBreakFlagRange kFlagRanges[] = {{",
        ]
    )
    for first, last, flags in flag_ranges:
        lines.append(f"  {{ 0x{first:04X}u, 0x{last:04X}u, 0x{flags:02X}u }},")
    lines.extend(
        [
            "};",
            "",
            "constexpr size_t kFlagRangeCount = sizeof(kFlagRanges) / sizeof(kFlagRanges[0]);",
            "static_assert(sizeof(LineBreakFlagRange) == 12,",
            '  "the flag range size budget assumes 12-byte (first, last, flags) entries");',
            "static_assert(sizeof(kRangeEnds) + sizeof(kRangePairIndex) + sizeof(kPairValue) + "
            "sizeof(kPairResolved) + sizeof(kFlagRanges) <= 48 * 1024,",
            '  "property tables must stay within the 48 KiB mobile budget");',
            "",
            "constexpr const char* kClassNames[] = {",
        ]
    )
    for name in CLASS_NAMES:
        lines.append(f'  "{name}",')
    lines.extend(
        [
            "};",
            "",
            "LineBreakClass lookup(const uint32_t codePoint, const bool resolved) noexcept {",
            "  if (codePoint > 0x10FFFFu) return LineBreakClass::XX;",
            "  const uint32_t* end = std::lower_bound(kRangeEnds, kRangeEnds + kRangeCount, codePoint);",
            "  if (end == kRangeEnds + kRangeCount) return LineBreakClass::XX;",
            "  const size_t range = static_cast<size_t>(end - kRangeEnds);",
            "  const size_t pair = kRangePairIndex[range];",
            "  return resolved ? kPairResolved[pair] : kPairValue[pair];",
            "}",
            "",
            "} // namespace",
            "",
            "LineBreakClass lineBreakClass(const uint32_t codePoint) noexcept {",
            "  return lookup(codePoint, false);",
            "}",
            "",
            "LineBreakClass resolveLineBreakClass(const uint32_t codePoint) noexcept {",
            "  return lookup(codePoint, true);",
            "}",
            "",
            "const char* lineBreakClassName(const LineBreakClass value) noexcept {",
            "  const auto index = static_cast<size_t>(value);",
            "  return index < kLineBreakClassCount ? kClassNames[index] : \"??\";",
            "}",
            "",
            "bool lineBreakClassFromName(const std::string_view name,",
            "    LineBreakClass& value) noexcept {",
            "  for (size_t index = 0; index < kLineBreakClassCount; ++index) {",
            "    if (name == kClassNames[index]) {",
            "      value = static_cast<LineBreakClass>(index);",
            "      return true;",
            "    }",
            "  }",
            "  return false;",
            "}",
            "",
            "LineBreakRange lineBreakRangeAt(const size_t index) noexcept {",
            "  if (index >= kRangeCount) return {};",
            "  const uint32_t first = index == 0 ? 0u : kRangeEnds[index - 1] + 1u;",
            "  const size_t pair = kRangePairIndex[index];",
            "  return LineBreakRange{ first, kRangeEnds[index], kPairValue[pair], kPairResolved[pair] };",
            "}",
            "",
            "size_t lineBreakRangeCount() noexcept {",
            "  return kRangeCount;",
            "}",
            "",
            "uint8_t lineBreakFlags(const uint32_t codePoint) noexcept {",
            "  if (codePoint > 0x10FFFFu || kFlagRangeCount == 0) return 0;",
            "  const auto found = std::upper_bound(kFlagRanges, kFlagRanges + kFlagRangeCount, codePoint,",
            "    [](const uint32_t value, const LineBreakFlagRange& range) {",
            "      return value < range.first;",
            "    });",
            "  if (found == kFlagRanges) return 0;",
            "  const LineBreakFlagRange& range = *(found - 1);",
            "  return codePoint <= range.last ? range.flags : 0;",
            "}",
            "",
            "const LineBreakFlagRange* lineBreakFlagRanges() noexcept {",
            "  return kFlagRanges;",
            "}",
            "",
            "size_t lineBreakFlagRangeCount() noexcept {",
            "  return kFlagRangeCount;",
            "}",
            "",
            "} // namespace coral::ui::unicode",
            "",
        ]
    )
    return "\n".join(lines)


def emit_version(version: str, inputs: dict[str, Input], generator_version: str) -> str:
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
        f" * Generated from Unicode {version} UCD data.",
        " * DO NOT EDIT BY HAND; see tools/unicode/generate_linebreak_properties.py.",
        " */",
        "",
        "#ifndef CORAL_UI_UNICODE_UNICODEVERSION_GENERATED_H",
        "#define CORAL_UI_UNICODE_UNICODEVERSION_GENERATED_H",
        "",
        "#include <string_view>",
        "",
        "namespace coral::ui::unicode {",
        "",
        f'inline constexpr std::string_view kUnicodeVersion = "{version}";',
        f'inline constexpr std::string_view kLineBreakDataSha256 =',
        f'  "{inputs["LineBreak.txt"].sha256}";',
        f'inline constexpr std::string_view kGeneralCategoryDataSha256 =',
        f'  "{inputs["DerivedGeneralCategory.txt"].sha256}";',
        f'inline constexpr std::string_view kEastAsianWidthDataSha256 =',
        f'  "{inputs["EastAsianWidth.txt"].sha256}";',
        f'inline constexpr std::string_view kEmojiDataSha256 =',
        f'  "{inputs["emoji-data.txt"].sha256}";',
        f'inline constexpr std::string_view kLineBreakTestSha256 =',
        f'  "{inputs["LineBreakTest.txt"].sha256}";',
        f'inline constexpr std::string_view kUnicodeDataGeneratorVersion = "{generator_version}";',
        "",
        "} // namespace coral::ui::unicode",
        "",
        "#endif // CORAL_UI_UNICODE_UNICODEVERSION_GENERATED_H",
        "",
    ]
    return "\n".join(lines)


def load_inputs(
    data_dir: Path, test_data: Path
) -> tuple[str, str, dict[str, Input]]:
    version_path = data_dir / "version.json"
    try:
        manifest = json.loads(read_text(version_path))
    except json.JSONDecodeError as error:
        fail(f"{version_path}: invalid JSON: {error}")

    version = manifest.get("unicodeVersion")
    if not isinstance(version, str) or not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version):
        fail(f"{version_path}: missing or invalid unicodeVersion")
    generator_version = str(manifest.get("generatorVersion", "1"))

    paths = {
        "LineBreak.txt": data_dir / "LineBreak.txt",
        "DerivedGeneralCategory.txt": data_dir / "DerivedGeneralCategory.txt",
        "EastAsianWidth.txt": data_dir / "EastAsianWidth.txt",
        "emoji-data.txt": data_dir / "emoji-data.txt",
        "LineBreakTest.txt": test_data,
    }
    inputs: dict[str, Input] = {}
    files = manifest.get("files", {})
    for name, path in paths.items():
        entry = files.get(name)
        if not isinstance(entry, dict):
            fail(f"{version_path}: missing files.{name}")
        expected_version = header_version(path, name)
        if expected_version != version:
            fail(
                f"{path}: file is {expected_version}, version.json pins {version}; "
                "upgrade every UCD file and version.json together"
            )
        actual_sha = file_sha256(path)
        if actual_sha != entry.get("sha256"):
            fail(
                f"{path}: sha256 {actual_sha} does not match version.json "
                f"{entry.get('sha256')}; update version.json only for a deliberate UCD change"
            )
        inputs[name] = Input(
            path=path,
            sha256=actual_sha,
            date=str(entry.get("date", "")),
            url=str(entry.get("url", "")),
        )
    return version, generator_version, inputs


def compare_and_write(path: Path, content: str, check: bool) -> bool:
    expected = content.encode("utf-8")
    if check:
        if not path.exists():
            print(f"missing: {path}")
            return False
        actual = path.read_bytes()
        if actual == expected:
            return True
        actual_lines = actual.splitlines()
        expected_lines = expected.splitlines()
        for index, (actual_line, expected_line) in enumerate(
            zip(actual_lines, expected_lines)
        ):
            if actual_line != expected_line:
                print(f"{path}: first difference at line {index + 1}")
                print(f"  committed: {actual_line.decode('utf-8', 'replace')}")
                print(f"  generated: {expected_line.decode('utf-8', 'replace')}")
                break
        if len(actual_lines) != len(expected_lines):
            print(f"{path}: {len(actual_lines)} lines committed, {len(expected_lines)} generated")
        return False

    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_bytes() == expected:
        return True
    path.write_bytes(expected)
    return True


def main(argv: list[str]) -> int:
    script = Path(__file__).resolve()
    ui_root = script.parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--data-dir",
        type=Path,
        default=script.parent / "data",
        help="directory containing LineBreak.txt, DerivedGeneralCategory.txt and version.json",
    )
    parser.add_argument(
        "--test-data",
        type=Path,
        default=ui_root / "tests/unicode/LineBreakTest.txt",
        help="LineBreakTest.txt whose version and checksum are pinned",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=ui_root / "src/unicode",
        help="directory for the generated headers and source",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="do not write; fail when the committed generated files differ from a fresh run",
    )
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args(argv)

    data_dir = args.data_dir.resolve()
    test_data = args.test_data.resolve()
    output_dir = args.output_dir.resolve()

    try:
        version, generator_version, inputs = load_inputs(data_dir, test_data)

        raw = fill_property(inputs["LineBreak.txt"].path, CLASS_NAMES, None)
        categories = fill_property(
            inputs["DerivedGeneralCategory.txt"].path, GENERAL_CATEGORIES, "Cn"
        )
        east_asian_widths = fill_property(
            inputs["EastAsianWidth.txt"].path, EAST_ASIAN_WIDTHS, "N"
        )
        extended_pictographic = parse_boolean_property(
            inputs["emoji-data.txt"].path, "Extended_Pictographic"
        )
        flags = build_flags(categories, east_asian_widths, extended_pictographic)

        resolved = bytearray(CODE_POINT_COUNT)
        category_to_name = {index: name for index, name in enumerate(GENERAL_CATEGORIES)}
        sa_to_cm = 0
        for code_point in range(CODE_POINT_COUNT):
            raw_name = CLASS_NAMES[raw[code_point]]
            resolved_name = resolve_lb1(raw_name, category_to_name[categories[code_point]])
            resolved[code_point] = CLASS_INDEX[resolved_name]
            if raw_name == "SA" and resolved_name == "CM":
                sa_to_cm += 1

        ranges = build_ranges(raw, resolved)
        flag_ranges = build_flag_ranges(flags)
        ascii_properties = [
            (CLASS_NAMES[resolved[code_point]], flags[code_point])
            for code_point in range(128)
        ]
        header = emit_header(version, inputs, len(ranges), len(flag_ranges), ascii_properties)
        source = emit_source(version, inputs, ranges, flag_ranges)
        version_header = emit_version(version, inputs, generator_version)

        files = {
            output_dir / "LineBreakProperties.generated.h": header,
            output_dir / "LineBreakProperties.generated.cpp": source,
            output_dir / "UnicodeVersion.generated.h": version_header,
        }
        ok = True
        for path, content in files.items():
            if not compare_and_write(path, content, args.check):
                ok = False

        if not args.quiet:
            unique_pairs = len({(value, resolved) for _, _, value, resolved in ranges})
            # end boundaries (u32) + class-pair ids (u8) + deduplicated pairs (2×u8)
            table_bytes = len(ranges) * 4 + len(ranges) + unique_pairs * 2
            flag_bytes = len(flag_ranges) * 12
            print(
                f"Unicode {version}: {len(ranges)} table ranges, {unique_pairs} class pairs, "
                f"{len(CLASS_NAMES)} classes, SA->CM {sa_to_cm} code points"
            )
            print(f"range table: {table_bytes} bytes ({table_bytes / 1024.0:.1f} KiB)")
            print(
                f"flag ranges: {len(flag_ranges)} ({flag_bytes} bytes), "
                f"property total {(table_bytes + flag_bytes) / 1024.0:.1f} KiB"
            )
            for path, content in files.items():
                print(f"{'verified' if args.check else 'wrote'} {path} ({len(content)} bytes)")
        return 0 if ok else 1
    except GenerationError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
