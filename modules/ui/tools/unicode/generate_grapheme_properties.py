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
"""Generate UAX #29 grapheme-cluster property tables."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

MAX_CODE_POINT = 0x10FFFF
CODE_POINT_COUNT = MAX_CODE_POINT + 1
GCB_CLASSES = [
    "CR", "LF", "Control", "Extend", "ZWJ", "Regional_Indicator", "Prepend",
    "SpacingMark", "L", "V", "T", "LV", "LVT", "Other",
]
GCB_INDEX = {name: index for index, name in enumerate(GCB_CLASSES)}
INCB_CLASSES = ["None", "Linker", "Consonant", "Extend"]
INCB_INDEX = {name: index for index, name in enumerate(INCB_CLASSES)}
UNSET = 0xFF


def read_text(path: Path) -> str:
    return path.read_bytes().decode("utf-8")


def parse_ranges(path: Path, value_filter=None):
    rows = []
    for number, line in enumerate(read_text(path).splitlines(), 1):
        body = line.split("#", 1)[0].strip()
        if not body or ";" not in body:
            continue
        fields = [field.strip() for field in body.split(";")]
        if value_filter is not None and len(fields) >= 3 and fields[1] != value_filter:
            continue
        if len(fields) == 2:
            value = fields[1]
        elif len(fields) >= 3:
            value = fields[2]
        else:
            continue
        match = re.match(r"^([0-9A-Fa-f]+)(?:\.\.([0-9A-Fa-f]+))?$", fields[0])
        if match is None:
            continue
        first = int(match.group(1), 16)
        last = int(match.group(2) or match.group(1), 16)
        if first > last or last > MAX_CODE_POINT:
            raise ValueError(f"{path}:{number}: invalid range")
        rows.append((first, last, value))
    return rows


def build_ranges(table: bytearray, default: int):
    for index, value in enumerate(table):
        if value == UNSET:
            table[index] = default
    ranges = []
    start = 0
    previous = table[0]
    for code_point in range(1, CODE_POINT_COUNT):
        current = table[code_point]
        if current != previous:
            ranges.append((start, code_point - 1, previous))
            start = code_point
            previous = current
    ranges.append((start, MAX_CODE_POINT, previous))
    return ranges


def emit_range_table(lines, name, ranges):
    lines.append(f"constexpr GraphemeRange {name}[] = {{")
    for first, last, value in ranges:
        lines.append(f"  {{ 0x{first:04X}u, 0x{last:04X}u, {value} }},")
    lines.extend([
        "};",
        f"constexpr size_t {name}Count = sizeof({name}) / sizeof({name}[0]);",
        "",
    ])


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    script = Path(__file__).resolve()
    ui_root = script.parents[2]
    parser.add_argument("--data-dir", type=Path, default=script.parent / "data")
    parser.add_argument("--output-dir", type=Path, default=ui_root / "src/unicode")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args(argv)
    data_dir = args.data_dir.resolve()
    output_dir = args.output_dir.resolve()
    manifest = json.loads((data_dir / "version.json").read_text(encoding="utf-8"))
    version = manifest["unicodeVersion"]

    paths = {
        "gcb": data_dir / "GraphemeBreakProperty.txt",
        "ep": data_dir / "emoji-data.txt",
        "incb": data_dir / "DerivedCoreProperties.txt",
    }
    for name, path in paths.items():
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != manifest["files"][path.name]["sha256"]:
            print(f"error: {path}: sha256 does not match version.json", file=sys.stderr)
            return 1

    gcb = bytearray([UNSET]) * CODE_POINT_COUNT
    for first, last, value in parse_ranges(paths["gcb"]):
        if value not in GCB_INDEX:
            print(f"error: unknown GCB class {value}", file=sys.stderr)
            return 1
        value_index = GCB_INDEX[value]
        for code_point in range(first, last + 1):
            gcb[code_point] = value_index
    gcb_ranges = build_ranges(gcb, GCB_INDEX["Other"])

    ep = bytearray(CODE_POINT_COUNT)
    for first, last, value in parse_ranges(paths["ep"]):
        if value == "Extended_Pictographic":
            ep[first:last + 1] = b"\x01" * (last - first + 1)
    ep_ranges = build_ranges(bytearray(1 if value else 0 for value in ep), 0)

    incb = bytearray([UNSET]) * CODE_POINT_COUNT
    for first, last, value in parse_ranges(paths["incb"], value_filter="InCB"):
        if value not in INCB_INDEX:
            continue
        value_index = INCB_INDEX[value]
        for code_point in range(first, last + 1):
            incb[code_point] = value_index
    incb_ranges = build_ranges(incb, INCB_INDEX["None"])

    header = [
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
        f" * Generated from Unicode {version} GraphemeBreakProperty/emoji-data/DerivedCoreProperties.",
        " * DO NOT EDIT BY HAND; see tools/unicode/generate_grapheme_properties.py.",
        " */",
        "",
        "#ifndef CORAL_UI_UNICODE_GRAPHEMEBREAKPROPERTIES_GENERATED_H",
        "#define CORAL_UI_UNICODE_GRAPHEMEBREAKPROPERTIES_GENERATED_H",
        "",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        "namespace coral::ui::unicode {",
        "",
        "enum class GraphemeBreakClass : uint8_t {",
    ]
    header.extend(f"  {name}," for name in GCB_CLASSES)
    header.extend([
        "  Count,",
        "};",
        "",
        "enum class GraphemeInCB : uint8_t {",
    ])
    header.extend(f"  {name}," for name in INCB_CLASSES)
    header.extend([
        "  Count,",
        "};",
        "",
        "struct GraphemeRange {",
        "  uint32_t first;",
        "  uint32_t last;",
        "  uint8_t value;",
        "};",
        "",
        "[[nodiscard]] GraphemeBreakClass graphemeBreakClass(uint32_t codePoint) noexcept;",
        "[[nodiscard]] bool isExtendedPictographic(uint32_t codePoint) noexcept;",
        "[[nodiscard]] GraphemeInCB graphemeInCB(uint32_t codePoint) noexcept;",
        "[[nodiscard]] const GraphemeRange* graphemeRanges() noexcept;",
        "[[nodiscard]] size_t graphemeRangeCount() noexcept;",
        "",
        "inline constexpr GraphemeBreakClass kAsciiGraphemeClasses[128] = {",
    ])
    header.extend(f"  GraphemeBreakClass::{GCB_CLASSES[gcb[index]]}," for index in range(128))
    header.extend([
        "};",
        "",
        "} // namespace coral::ui::unicode",
        "",
        "#endif // CORAL_UI_UNICODE_GRAPHEMEBREAKPROPERTIES_GENERATED_H",
        "",
    ])

    source = [
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
        '#include "unicode/GraphemeBreakProperties.generated.h"',
        "",
        "#include <algorithm>",
        "",
        "namespace coral::ui::unicode {",
        "namespace {",
        "",
    ]
    emit_range_table(source, "kGraphemeRanges", gcb_ranges)
    emit_range_table(source, "kExtendedPictographicRanges", ep_ranges)
    emit_range_table(source, "kInCBRanges", incb_ranges)
    source.extend([
        "template<typename Range>",
        "uint8_t rangeValue(const Range* ranges, const size_t count, const uint32_t codePoint) {",
        "  const auto found = std::upper_bound(ranges, ranges + count, codePoint,",
        "    [](const uint32_t value, const Range& range) { return value < range.first; });",
        "  if (found == ranges) return 0;",
        "  const Range& range = *(found - 1);",
        "  return codePoint <= range.last ? range.value : 0;",
        "}",
        "",
        "} // namespace",
        "",
        "GraphemeBreakClass graphemeBreakClass(const uint32_t codePoint) noexcept {",
        "  if (codePoint < 128) return kAsciiGraphemeClasses[codePoint];",
        "  if (codePoint > 0x10FFFFu) return GraphemeBreakClass::Other;",
        "  return static_cast<GraphemeBreakClass>(",
        "    rangeValue(kGraphemeRanges, kGraphemeRangesCount, codePoint));",
        "}",
        "",
        "bool isExtendedPictographic(const uint32_t codePoint) noexcept {",
        "  if (codePoint > 0x10FFFFu) return false;",
        "  return rangeValue(kExtendedPictographicRanges, kExtendedPictographicRangesCount, codePoint) != 0;",
        "}",
        "",
        "GraphemeInCB graphemeInCB(const uint32_t codePoint) noexcept {",
        "  if (codePoint > 0x10FFFFu) return GraphemeInCB::None;",
        "  return static_cast<GraphemeInCB>(",
        "    rangeValue(kInCBRanges, kInCBRangesCount, codePoint));",
        "}",
        "",
        "const GraphemeRange* graphemeRanges() noexcept { return kGraphemeRanges; }",
        "size_t graphemeRangeCount() noexcept { return kGraphemeRangesCount; }",
        "",
        "} // namespace coral::ui::unicode",
        "",
    ])

    files = {
        output_dir / "GraphemeBreakProperties.generated.h": "\n".join(header),
        output_dir / "GraphemeBreakProperties.generated.cpp": "\n".join(source),
    }
    for target, content in files.items():
        data = content.encode("utf-8")
        if args.check:
            if not target.exists() or target.read_bytes() != data:
                print(f"mismatch: {target}", file=sys.stderr)
                return 1
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
    if not args.quiet:
        print(f"Grapheme {version}: {len(gcb_ranges)} GCB ranges")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
