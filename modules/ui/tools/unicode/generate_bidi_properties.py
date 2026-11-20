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
"""Generate the Bidi_Class range table (UAX #9 B0 baseline).

This is deliberately only the data layer plus a fast class lookup. The full UBA
resolver (explicit levels, weak/neutral rules and bracket pairs) is a separate
stage and must be validated against BidiTest.txt/BidiCharacterTest.txt.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

CLASS_NAMES = [
    "L", "R", "AL", "EN", "ES", "ET", "AN", "CS", "NSM", "BN", "B", "S", "WS",
    "ON", "LRE", "LRO", "RLE", "RLO", "PDF", "LRI", "RLI", "FSI", "PDI",
]
CLASS_INDEX = {name: index for index, name in enumerate(CLASS_NAMES)}
MAX_CODE_POINT = 0x10FFFF
CODE_POINT_COUNT = MAX_CODE_POINT + 1
UNSET = 0xFF

ALIASES = {
    "Left_To_Right": "L",
    "Right_To_Left": "R",
    "Arabic_Letter": "AL",
    "European_Number": "EN",
    "European_Separator": "ES",
    "European_Terminator": "ET",
    "Arabic_Number": "AN",
    "Common_Separator": "CS",
    "Nonspacing_Mark": "NSM",
    "Boundary_Neutral": "BN",
    "Paragraph_Separator": "B",
    "Segment_Separator": "S",
    "White_Space": "WS",
    "Other_Neutral": "ON",
    "Left_To_Right_Embedding": "LRE",
    "Left_To_Right_Override": "LRO",
    "Right_To_Left_Embedding": "RLE",
    "Right_To_Left_Override": "RLO",
    "Pop_Directional_Format": "PDF",
    "Left_To_Right_Isolate": "LRI",
    "Right_To_Left_Isolate": "RLI",
    "First_Strong_Isolate": "FSI",
    "Pop_Directional_Isolate": "PDI",
}

HEADER = re.compile(r"^#\s*DerivedBidiClass-([0-9]+\.[0-9]+\.[0-9]+)\.txt")
RANGE = re.compile(r"^([0-9A-Fa-f]+)(?:\.\.([0-9A-Fa-f]+))?\s*;\s*([A-Za-z0-9_]+)")
MISSING = re.compile(r"^#\s*@missing:\s*([0-9A-Fa-f]+)(?:\.\.([0-9A-Fa-f]+))?\s*;\s*([A-Za-z0-9_]+)")


def fail(message: str) -> None:
    raise RuntimeError(message)


def normalize(value: str) -> str:
    value = ALIASES.get(value, value)
    if value not in CLASS_INDEX:
        fail(f"unknown Bidi class {value!r}")
    return value


def read_file(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as error:
        fail(f"cannot read {path}: {error}")


def main(argv: list[str]) -> int:
    script = Path(__file__).resolve()
    ui_root = script.parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-dir", type=Path, default=script.parent / "data")
    parser.add_argument("--output-dir", type=Path, default=ui_root / "src/unicode")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args(argv)
    data_dir = args.data_dir.resolve()
    output_dir = args.output_dir.resolve()

    try:
        manifest = json.loads(read_file(data_dir / "version.json"))
        version = manifest["unicodeVersion"]
        entry = manifest["files"]["DerivedBidiClass.txt"]
        path = data_dir / "DerivedBidiClass.txt"
        text = read_file(path)
        match = HEADER.match(text.splitlines()[0])
        if match is None or match.group(1) != version:
            fail(f"{path}: version does not match {version}")
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != entry["sha256"]:
            fail(f"{path}: sha256 {digest} does not match version.json")
        url = entry["url"]

        table = bytearray([UNSET]) * CODE_POINT_COUNT
        explicit = bytearray(CODE_POINT_COUNT)
        missing = []
        rows = []
        for number, line in enumerate(text.splitlines(), 1):
            missing_match = MISSING.match(line)
            if missing_match is not None:
                missing.append(
                    (
                        int(missing_match.group(1), 16),
                        int(missing_match.group(2) or missing_match.group(1), 16),
                        normalize(missing_match.group(3)),
                    )
                )
                continue
            row = RANGE.match(line)
            if row is None:
                continue
            first = int(row.group(1), 16)
            last = int(row.group(2) or row.group(1), 16)
            if first > last or last > MAX_CODE_POINT:
                fail(f"{path}:{number}: invalid range")
            rows.append((first, last, normalize(row.group(3))))

        for first, last, value in missing:
            table[first:last + 1] = bytes([CLASS_INDEX[value]]) * (last - first + 1)
        for first, last, value in rows:
            for code_point in range(first, last + 1):
                if explicit[code_point]:
                    fail(f"{path}: overlapping range at U+{code_point:04X}")
                explicit[code_point] = 1
                table[code_point] = CLASS_INDEX[value]
        if UNSET in table:
            fail(f"{path}: U+{table.index(UNSET):04X} has no Bidi class")

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
            f" * Generated from Unicode {version} DerivedBidiClass.txt.",
            f" * Source: {url}",
            " * DO NOT EDIT BY HAND; see tools/unicode/generate_bidi_properties.py.",
            " */",
            "",
            "#ifndef CORAL_UI_UNICODE_BIDIPROPERTIES_GENERATED_H",
            "#define CORAL_UI_UNICODE_BIDIPROPERTIES_GENERATED_H",
            "",
            "#include <cstddef>",
            "#include <cstdint>",
            "#include <string_view>",
            "",
            "namespace coral::ui::unicode {",
            "",
            f"enum class BidiClass : uint8_t {{",
        ]
        header.extend(f"  {name}," for name in CLASS_NAMES)
        header.extend(
            [
                "  Count,",
                "};",
                "",
                "inline constexpr size_t kBidiClassCount = static_cast<size_t>(BidiClass::Count);",
                "",
                "struct BidiRange {",
                "  uint32_t first;",
                "  uint32_t last;",
                "  BidiClass value;",
                "};",
                "",
                "[[nodiscard]] BidiClass bidiClass(uint32_t codePoint) noexcept;",
                "[[nodiscard]] const char* bidiClassName(BidiClass value) noexcept;",
                "[[nodiscard]] bool bidiClassFromName(std::string_view name, BidiClass& value) noexcept;",
                f"[[nodiscard]] const BidiRange* bidiRanges() noexcept;",
                "[[nodiscard]] size_t bidiRangeCount() noexcept;",
                "",
                "/** ASCII fast path: class without a range search. */",
                "inline constexpr BidiClass kAsciiBidiClasses[128] = {",
            ]
        )
        for code_point in range(128):
            header.append(f"  BidiClass::{CLASS_NAMES[table[code_point]]},")
        header.extend(
            [
                "};",
                "",
                "} // namespace coral::ui::unicode",
                "",
                "#endif // CORAL_UI_UNICODE_BIDIPROPERTIES_GENERATED_H",
                "",
            ]
        )

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
            "/*",
            f" * Generated from Unicode {version} DerivedBidiClass.txt.",
            " * DO NOT EDIT BY HAND; see tools/unicode/generate_bidi_properties.py.",
            " */",
            "",
            '#include "unicode/BidiProperties.generated.h"',
            "",
            "#include <algorithm>",
            "",
            "namespace coral::ui::unicode {",
            "namespace {",
            "",
            f"constexpr BidiRange kRanges[] = {{",
        ]
        for first, last, value in ranges:
            source.append(f"  {{ 0x{first:04X}u, 0x{last:04X}u, BidiClass::{CLASS_NAMES[value]} }},")
        source.extend(
            [
                "};",
                "",
                "constexpr size_t kRangeCount = sizeof(kRanges) / sizeof(kRanges[0]);",
                "static_assert(sizeof(kRanges) <= 24 * 1024,",
                '  "Bidi_Class ranges must stay within their mobile budget");',
                "",
                "constexpr const char* kClassNames[] = {",
            ]
        )
        source.extend(f'  "{name}",' for name in CLASS_NAMES)
        source.extend(
            [
                "};",
                "",
                "} // namespace",
                "",
                "BidiClass bidiClass(const uint32_t codePoint) noexcept {",
                "  if (codePoint < 128) return kAsciiBidiClasses[codePoint];",
                "  if (codePoint > 0x10FFFFu) return BidiClass::L;",
                "  const auto found = std::upper_bound(kRanges, kRanges + kRangeCount, codePoint,",
                "    [](const uint32_t value, const BidiRange& range) {",
                "      return value < range.first;",
                "    });",
                "  if (found == kRanges) return BidiClass::L;",
                "  const BidiRange& range = *(found - 1);",
                "  return codePoint <= range.last ? range.value : BidiClass::L;",
                "}",
                "",
                "const char* bidiClassName(const BidiClass value) noexcept {",
                "  const auto index = static_cast<size_t>(value);",
                '  return index < kBidiClassCount ? kClassNames[index] : "?";',
                "}",
                "",
                "bool bidiClassFromName(const std::string_view name, BidiClass& value) noexcept {",
                "  for (size_t index = 0; index < kBidiClassCount; ++index) {",
                "    if (name == kClassNames[index]) {",
                "      value = static_cast<BidiClass>(index);",
                "      return true;",
                "    }",
                "  }",
                "  return false;",
                "}",
                "",
                "const BidiRange* bidiRanges() noexcept { return kRanges; }",
                "size_t bidiRangeCount() noexcept { return kRangeCount; }",
                "",
                "} // namespace coral::ui::unicode",
                "",
            ]
        )

        files = {
            output_dir / "BidiProperties.generated.h": "\n".join(header),
            output_dir / "BidiProperties.generated.cpp": "\n".join(source),
        }
        if not args.quiet:
            print(f"Bidi_Class {version}: {len(ranges)} ranges")
        ok = True
        for path, content in files.items():
            expected = content.encode("utf-8")
            if args.check:
                if not path.exists() or path.read_bytes() != expected:
                    print(f"mismatch: {path}", file=sys.stderr)
                    ok = False
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(expected)
        return 0 if ok else 1
    except RuntimeError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
