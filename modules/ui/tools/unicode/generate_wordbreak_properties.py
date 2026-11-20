#!/usr/bin/env python3
"""
Generate the UAX #29 Word_Break property table.

The runtime word segmenter (unicode/WordBreak.cpp) implements WB1-WB999 on top of
this table; Extended_Pictographic comes from the existing emoji-data table.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

import generate_linebreak_properties as props

CLASS_NAMES = [
    "ALetter",
    "CR",
    "Double_Quote",
    "Extend",
    "ExtendNumLet",
    "Format",
    "Hebrew_Letter",
    "Katakana",
    "LF",
    "MidLetter",
    "MidNum",
    "MidNumLet",
    "Newline",
    "Numeric",
    "Other",
    "Regional_Indicator",
    "Single_Quote",
    "WSegSpace",
    "ZWJ",
]
CLASS_INDEX = {name: index for index, name in enumerate(CLASS_NAMES)}
MAX_CODE_POINT = 0x10FFFF
CODE_POINT_COUNT = MAX_CODE_POINT + 1
HEADER = re.compile(r"^#\s*WordBreakProperty-([0-9]+\.[0-9]+\.[0-9]+)\.txt")
RANGE = re.compile(r"^([0-9A-Fa-f]+)(?:\.\.([0-9A-Fa-f]+))?\s*;\s*([A-Za-z_]+)")
MISSING = re.compile(r"^#\s*@missing:\s*([0-9A-Fa-f]+)(?:\.\.([0-9A-Fa-f]+))?\s*;\s*([A-Za-z_]+)")


def parse(data: str) -> tuple[list[tuple[int, int, int]], int, str]:
    version = ""
    default = CLASS_INDEX["Other"]
    rows: list[tuple[int, int, int]] = []
    for line in data.splitlines():
        header = HEADER.match(line)
        if header is not None:
            version = header.group(1)
            continue
        missing = MISSING.match(line)
        if missing is not None:
            if missing.group(3) not in CLASS_INDEX:
                props.fail(f"unknown word-break class {missing.group(3)!r}")
            default = CLASS_INDEX[missing.group(3)]
            continue
        if line.startswith("#"):
            continue
        match = RANGE.match(line)
        if match is None:
            continue
        first = int(match.group(1), 16)
        last = int(match.group(2), 16) if match.group(2) else first
        name = match.group(3)
        if name not in CLASS_INDEX:
            props.fail(f"unknown word-break class {name!r}")
        if first > last or last > MAX_CODE_POINT:
            props.fail(f"invalid range {line!r}")
        rows.append((first, last, CLASS_INDEX[name]))
    if not version:
        props.fail("WordBreakProperty.txt has no version header")
    return rows, default, version


def build_table(rows: list[tuple[int, int, int]], default: int) -> list[tuple[int, int, int]]:
    values = bytearray([default]) * CODE_POINT_COUNT
    for first, last, value in rows:
        values[first:last + 1] = bytes([value]) * (last - first + 1)
    ranges: list[tuple[int, int, int]] = []
    start = 0
    current = values[0]
    for code_point in range(1, CODE_POINT_COUNT):
        value = values[code_point]
        if value != current:
            ranges.append((start, code_point - 1, current))
            start = code_point
            current = value
    ranges.append((start, MAX_CODE_POINT, current))
    return ranges


def emit_header(version: str, sha256: str, ascii_classes: list[int]) -> str:
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
        f" * Generated from WordBreakProperty-{version}.txt (UAX #29).",
        " * DO NOT EDIT BY HAND; see tools/unicode/generate_wordbreak_properties.py.",
        " */",
        "",
        "#ifndef CORAL_UI_UNICODE_WORDBREAKPROPERTIES_GENERATED_H",
        "#define CORAL_UI_UNICODE_WORDBREAKPROPERTIES_GENERATED_H",
        "",
        "#include <cstddef>",
        "#include <cstdint>",
        "#include <string_view>",
        "",
        "namespace coral::ui::unicode {",
        "",
        "enum class WordBreakClass : uint8_t {",
    ]
    for name in CLASS_NAMES:
        lines.append(f"  {name},")
    lines.extend(
        [
            "};",
            "",
            "struct WordBreakRange {",
            "  uint32_t first;",
            "  uint32_t last;",
            "  WordBreakClass value;",
            "};",
            "",
            "/** @return the UAX #29 Word_Break class; Other for unassigned code points. */",
            "[[nodiscard]] WordBreakClass wordBreakClass(uint32_t codePoint) noexcept;",
            "",
            "/** @return the UCD class name, e.g. \"ALetter\"; never null. */",
            "[[nodiscard]] const char* wordBreakClassName(WordBreakClass value) noexcept;",
            "",
            "/** One contiguous range, materialized on demand (table tests). */",
            "[[nodiscard]] WordBreakRange wordBreakRangeAt(size_t index) noexcept;",
            "[[nodiscard]] size_t wordBreakRangeCount() noexcept;",
            "",
            "inline constexpr uint8_t kAsciiWordBreakClasses[128] = {",
        ]
    )
    for code_point in range(128):
        lines.append(f"  static_cast<uint8_t>(WordBreakClass::{CLASS_NAMES[ascii_classes[code_point]]}),")
    lines.extend(
        [
            "};",
            "",
            f'inline constexpr std::string_view kWordBreakDataSha256 = "{sha256}";',
            f'inline constexpr std::string_view kWordBreakUnicodeVersion = "{version}";',
            "",
            "} // namespace coral::ui::unicode",
            "",
            "#endif // CORAL_UI_UNICODE_WORDBREAKPROPERTIES_GENERATED_H",
            "",
        ]
    )
    return "\n".join(lines)


def emit_source(version: str, ranges: list[tuple[int, int, int]]) -> str:
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
        f" * Generated from WordBreakProperty-{version}.txt (UAX #29).",
        " * DO NOT EDIT BY HAND; see tools/unicode/generate_wordbreak_properties.py.",
        " */",
        "",
        '#include "unicode/WordBreakProperties.generated.h"',
        "",
        "#include <algorithm>",
        "",
        "namespace coral::ui::unicode {",
        "namespace {",
        "",
        f"constexpr uint32_t kRangeEnds[{len(ranges)}] = {{",
    ]
    for start in range(0, len(ranges), 12):
        lines.append("  " + ", ".join(f"0x{value[1]:04X}u" for value in ranges[start:start + 12]) + ",")
    lines.extend(["};", "", f"constexpr uint8_t kRangeClasses[{len(ranges)}] = {{"])
    for start in range(0, len(ranges), 24):
        lines.append("  " + ", ".join(str(value[2]) for value in ranges[start:start + 24]) + ",")
    lines.extend(
        [
            "};",
            "",
            "constexpr size_t kRangeCount = sizeof(kRangeEnds) / sizeof(kRangeEnds[0]);",
            "",
            "static_assert(sizeof(kRangeEnds) + sizeof(kRangeClasses) + sizeof(kAsciiWordBreakClasses) <= 32 * 1024,",
            '  "word-break tables must stay within the 32 KiB budget");',
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
            "} // namespace",
            "",
            "WordBreakClass wordBreakClass(const uint32_t codePoint) noexcept {",
            "  if (codePoint > 0x10FFFFu) return WordBreakClass::Other;",
            "  const uint32_t* end = std::lower_bound(kRangeEnds, kRangeEnds + kRangeCount, codePoint);",
            "  if (end == kRangeEnds + kRangeCount) return WordBreakClass::Other;",
            "  return static_cast<WordBreakClass>(kRangeClasses[static_cast<size_t>(end - kRangeEnds)]);",
            "}",
            "",
            "const char* wordBreakClassName(const WordBreakClass value) noexcept {",
            "  const size_t index = static_cast<size_t>(value);",
            "  return index < sizeof(kClassNames) / sizeof(kClassNames[0]) ? kClassNames[index] : \"Other\";",
            "}",
            "",
            "WordBreakRange wordBreakRangeAt(const size_t index) noexcept {",
            "  if (index >= kRangeCount) return {};",
            "  const uint32_t first = index == 0 ? 0u : kRangeEnds[index - 1] + 1u;",
            "  return WordBreakRange{ first, kRangeEnds[index],",
            "    static_cast<WordBreakClass>(kRangeClasses[index]) };",
            "}",
            "",
            "size_t wordBreakRangeCount() noexcept {",
            "  return kRangeCount;",
            "}",
            "",
            "} // namespace coral::ui::unicode",
            "",
        ]
    )
    return "\n".join(lines)


def main(argv: list[str]) -> int:
    script = Path(__file__).resolve()
    ui_root = script.parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-dir", type=Path, default=script.parent / "data")
    parser.add_argument("--output-dir", type=Path, default=ui_root / "src/unicode")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args(argv)
    try:
        data_dir = args.data_dir.resolve()
        output_dir = args.output_dir.resolve()
        manifest = json.loads(props.read_text(data_dir / "version.json"))
        version = str(manifest["unicodeVersion"])
        entry = manifest["files"]["WordBreakProperty.txt"]
        path = data_dir / "WordBreakProperty.txt"
        data = props.read_text(path)
        rows, default, file_version = parse(data)
        if file_version != version:
            props.fail(f"{path}: file is {file_version}, version.json pins {version}")
        digest = props.file_sha256(path)
        if digest != entry.get("sha256"):
            props.fail(f"{path}: sha256 {digest} does not match version.json")
        ranges = build_table(rows, default)
        ascii_classes = [0] * 128
        for first, last, value in ranges:
            if first >= 128:
                break
            for code_point in range(first, min(last, 127) + 1):
                ascii_classes[code_point] = value
        header = emit_header(version, digest, ascii_classes)
        source = emit_source(version, ranges)
        if not args.quiet:
            print(f"WordBreakProperty: {len(rows)} source ranges, {len(ranges)} table ranges, "
                  f"{(len(ranges) * 5 + 128) / 1024.0:.1f} KiB")
        files = {
            output_dir / "WordBreakProperties.generated.h": header,
            output_dir / "WordBreakProperties.generated.cpp": source,
        }
        ok = True
        for path_out, content in files.items():
            if not props.compare_and_write(path_out, content, args.check):
                ok = False
        return 0 if ok else 1
    except props.GenerationError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
