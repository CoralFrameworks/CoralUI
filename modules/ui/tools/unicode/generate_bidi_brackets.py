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
"""Generate the Bidi_Paired_Bracket table for UAX #9 N0."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

HEADER = re.compile(r"^#\s*BidiBrackets-([0-9]+\.[0-9]+\.[0-9]+)\.txt")
ROW = re.compile(r"^([0-9A-Fa-f]+)\s*;\s*([0-9A-Fa-f]+)\s*;\s*([oc])")


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

    manifest = json.loads((data_dir / "version.json").read_text(encoding="utf-8"))
    version = manifest["unicodeVersion"]
    entry = manifest["files"]["BidiBrackets.txt"]
    path = data_dir / "BidiBrackets.txt"
    text = path.read_text(encoding="utf-8")
    header_match = HEADER.match(text.splitlines()[0])
    if header_match is None or header_match.group(1) != version:
        print(f"error: {path}: version does not match {version}", file=sys.stderr)
        return 1
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != entry["sha256"]:
        print(f"error: {path}: sha256 does not match version.json", file=sys.stderr)
        return 1

    rows = []
    for line in text.splitlines():
        match = ROW.match(line)
        if match is None:
            continue
        rows.append((int(match.group(1), 16), int(match.group(2), 16), match.group(3) == "o"))
    rows.sort()

    output = [
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
        f" * Generated from Unicode {version} BidiBrackets.txt.",
        f" * Source: {entry['url']}",
        " * DO NOT EDIT BY HAND; see tools/unicode/generate_bidi_brackets.py.",
        " */",
        "",
        "#ifndef CORAL_UI_UNICODE_BIDIBRACKETS_GENERATED_H",
        "#define CORAL_UI_UNICODE_BIDIBRACKETS_GENERATED_H",
        "",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        "namespace coral::ui::unicode {",
        "",
        "struct BidiBracket {",
        "  uint32_t codePoint;",
        "  uint32_t pair;",
        "  bool open;",
        "};",
        "",
        f"inline constexpr BidiBracket kBidiBrackets[{len(rows)}] = {{",
    ]
    for code_point, pair, is_open in rows:
        output.append(
            f"  {{ 0x{code_point:04X}u, 0x{pair:04X}u, {'true' if is_open else 'false'} }},"
        )
    output.extend(
        [
            "};",
            "",
            "inline constexpr size_t kBidiBracketCount =",
            "  sizeof(kBidiBrackets) / sizeof(kBidiBrackets[0]);",
            "",
            "/** @return the bracket record for @p codePoint, or null. */",
            "[[nodiscard]] inline const BidiBracket* bidiBracket(const uint32_t codePoint) noexcept {",
            "  size_t first = 0;",
            "  size_t last = kBidiBracketCount;",
            "  while (first < last) {",
            "    const size_t middle = first + (last - first) / 2;",
            "    if (kBidiBrackets[middle].codePoint < codePoint) {",
            "      first = middle + 1;",
            "    } else {",
            "      last = middle;",
            "    }",
            "  }",
            "  if (first >= kBidiBracketCount || kBidiBrackets[first].codePoint != codePoint) {",
            "    return nullptr;",
            "  }",
            "  return &kBidiBrackets[first];",
            "}",
            "",
            "} // namespace coral::ui::unicode",
            "",
            "#endif // CORAL_UI_UNICODE_BIDIBRACKETS_GENERATED_H",
            "",
        ]
    )
    target = output_dir / "BidiBrackets.generated.h"
    content = "\n".join(output).encode("utf-8")
    if args.check:
        if not target.exists() or target.read_bytes() != content:
            print(f"mismatch: {target}", file=sys.stderr)
            return 1
    else:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)
    if not args.quiet:
        print(f"Bidi brackets {version}: {len(rows)} entries")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
