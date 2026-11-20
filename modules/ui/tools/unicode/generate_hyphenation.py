#!/usr/bin/env python3
"""
Compile TeX Liang hyphenation patterns into compact DAFSAs for the UI runtime.

Each input file comes from the hyph-utf8 project (`data/hyphenation/*.tex`). The
output is one minimized acyclic trie per language plus nibble-packed Liang
weights: every node has a sorted edge range and nodes sharing both their weight
vector and child set are merged. The runtime never reads the `.tex` files.

Usage:
  python tools/unicode/generate_hyphenation.py
  python tools/unicode/generate_hyphenation.py --check --quiet
"""

from __future__ import annotations

import argparse
import json
import sys
import unicodedata
from pathlib import Path

sys.setrecursionlimit(100000)

import generate_linebreak_properties as props

GENERATOR_VERSION = "2"

# Patterns only cover ASCII letters plus the apostrophe (French/Italian).
LANGUAGES = [
    {
        "tag": "en",
        "name": "English",
        "data": "hyph-en-us.tex",
        "left": 2,
        "right": 3,
    },
    {
        "tag": "it",
        "name": "Italian",
        "data": "hyph-it.tex",
        "left": 2,
        "right": 2,
    },
    {
        "tag": "es",
        "name": "Spanish",
        "data": "hyph-es.tex",
        "left": 2,
        "right": 2,
    },
    {
        "tag": "de",
        "name": "German",
        "data": "hyph-de-1996.tex",
        "left": 2,
        "right": 2,
    },
    {
        "tag": "fr",
        "name": "French",
        "data": "hyph-fr.tex",
        "left": 2,
        "right": 2,
    },
    {
        "tag": "pt",
        "name": "Portuguese",
        "data": "hyph-pt.tex",
        "left": 2,
        "right": 3,
    },
    {
        "tag": "ru",
        "name": "Russian",
        "data": "hyph-ru.tex",
        "left": 2,
        "right": 2,
    },
    {
        "tag": "el",
        "name": "Greek",
        "data": "hyph-el-monoton.tex",
        "left": 1,
        "right": 1,
    },
    {
        "tag": "nl",
        "name": "Dutch",
        "data": "hyph-nl.tex",
        "left": 2,
        "right": 2,
    },
    {
        "tag": "sv",
        "name": "Swedish",
        "data": "hyph-sv.tex",
        "left": 1,
        "right": 2,
    },
    {
        "tag": "da",
        "name": "Danish",
        "data": "hyph-da.tex",
        "left": 2,
        "right": 2,
    },
    {
        "tag": "fi",
        "name": "Finnish",
        "data": "hyph-fi.tex",
        "left": 2,
        "right": 2,
    },
    {
        "tag": "ca",
        "name": "Catalan",
        "data": "hyph-ca.tex",
        "left": 2,
        "right": 2,
    },
]

ALPHABET = "abcdefghijklmnopqrstuvwxyz.'"
# Russian is transliterated one-to-one onto ASCII labels so the byte DAFSA and
# runtime offset mapping stay unchanged.
CYRILLIC = "абвгдеёжзийклмнопрстуфхцчшщъыьэюя"
CYRILLIC_SYMBOLS = "abvgde;jziyklmnoprstufhcwx,/*+=?A"
assert len(CYRILLIC) == len(CYRILLIC_SYMBOLS) == 33
GREEK_ORDER = "αβγδεζηθικλμνξοπρσςτυφχψω"
GREEK_SYMBOLS = "abgdezhqiklmnxoprsstufcyw"
GREEK = {letter: GREEK_SYMBOLS[index] for index, letter in enumerate(GREEK_ORDER)}
GREEK.update({
    "ά": "a", "έ": "e", "ή": "h", "ί": "i", "ό": "o", "ύ": "u", "ώ": "w",
    "ϊ": "i", "ϋ": "u", "ΐ": "i", "ΰ": "u",
    "Ά": "a", "Έ": "e", "Ή": "h", "Ί": "i", "Ό": "o", "Ύ": "u", "Ώ": "w",
    "Ϊ": "i", "Ϋ": "u", "ϲ": "s", "Ͻ": "s", "Ͼ": "s", "Ͽ": "s",
    "ά": "a", "έ": "e", "ή": "h", "ί": "i", "ό": "o", "ύ": "u", "ώ": "w",
    "\u1f71": "a", "\u1f73": "e", "\u1f75": "h", "\u1f77": "i",
    "\u1f79": "o", "\u1f7b": "u", "\u1f7d": "w",
    "\u02bc": "'", "\u2019": "'",
})
for upper, lower in zip("ΑΒΓΔΕΖΗΘΙΚΛΜΝΞΟΠΡΣΤΥΦΧΨΩ", GREEK_ORDER):
    GREEK[upper] = GREEK[lower]


def fold_letter(character: str) -> str | None:
    """Folds Latin/ligature/Cyrillic/Greek letters to one ASCII label (1:1)."""
    index = CYRILLIC.find(character)
    if index >= 0:
        return CYRILLIC_SYMBOLS[index]
    if character in GREEK:
        return GREEK[character]
    if character in ".'":
        return character
    # Ligatures are approximated by their first base letter. This keeps the
    # byte-offset mapping 1:1; ß becomes s, æ becomes a, œ becomes o.
    ligatures = {"ß": "s", "æ": "a", "œ": "o", "þ": "t"}
    if character in ligatures:
        return ligatures[character]
    if character.isascii():
        return character.lower()
    decomposed = unicodedata.normalize("NFKD", character)
    base = "".join(item for item in decomposed if not unicodedata.combining(item))
    if len(base) == 1 and base.isascii() and base.isalpha():
        return base.lower()
    return None


def parse_patterns(path: Path) -> tuple[list[tuple[str, tuple[int, ...]]], int]:
    """Returns patterns plus the count skipped for using non-ASCII letters."""
    text = props.read_text(path)
    # Strip TeX comments but keep line boundaries.
    body = "\n".join(line.split("%", 1)[0] for line in text.splitlines())
    marker = "\\patterns{"
    start = body.find(marker)
    if start < 0:
        raise props.GenerationError(f"{path}: missing \\patterns{{ block")
    index = start + len(marker)
    depth = 1
    end = index
    while end < len(body):
        character = body[end]
        if character == "{":
            depth += 1
        elif character == "}":
            depth -= 1
            if depth == 0:
                break
        end += 1
    if depth != 0:
        raise props.GenerationError(f"{path}: unterminated \\patterns{{ block")

    patterns: list[tuple[str, tuple[int, ...]]] = []
    skipped = 0
    # TeX pattern blocks are whitespace separated; some files put several
    # patterns on one line (Spanish) and some use apostrophes (French/Italian).
    for token in body[index:end].split():
        line = token.strip()
        if not line:
            continue
        letters = "".join(character for character in line if not character.isdigit())
        if not letters:
            continue
        folded: list[str] = []
        for character in letters:
            base = fold_letter(character)
            if base is None:
                folded = []
                break
            folded.append(base)
        if not folded:
            # Ligatures such as ß or æ expand to two letters; the runtime's 1:1
            # offset mapping cannot represent them, so the pattern is skipped.
            skipped += 1
            continue
        letters = "".join(folded)
        weights = [0] * (len(letters) + 1)
        position = 0
        for character in line:
            if character.isdigit():
                weights[position] = int(character)
            else:
                position += 1
        patterns.append((letters, tuple(weights)))
    if not patterns:
        raise props.GenerationError(f"{path}: no patterns")
    return patterns, skipped


def build_dafsa(
    patterns: list[tuple[str, tuple[int, ...]]]
) -> tuple[list[tuple[tuple[int, ...] | None, tuple[tuple[str, int], ...]]], int]:
    """Builds and minimizes the pattern trie; returns (nodes, root)."""
    children: list[dict[str, int]] = [{}]
    weights_at: list[tuple[int, ...] | None] = [None]
    for letters, weights in patterns:
        node = 0
        for character in letters:
            child = children[node].get(character)
            if child is None:
                child = len(children)
                children[node][character] = child
                children.append({})
                weights_at.append(None)
            node = child
        weights_at[node] = weights

    memo: dict[
        tuple[tuple[int, ...] | None, tuple[tuple[str, int], ...]], int
    ] = {}
    nodes: list[tuple[tuple[int, ...] | None, tuple[tuple[str, int], ...]]] = []

    def canonical(node: int) -> int:
        edges = tuple(
            sorted((character, canonical(child)) for character, child in children[node].items())
        )
        key = (weights_at[node], edges)
        found = memo.get(key)
        if found is not None:
            return found
        node_id = len(nodes)
        memo[key] = node_id
        nodes.append(key)
        return node_id

    root = canonical(0)
    return nodes, root


def pack_weights(vectors: list[tuple[int, ...]]) -> tuple[list[int], list[int], list[int]]:
    """Packs weight vectors into one continuous nibble stream."""
    offsets: list[int] = []
    lengths: list[int] = []
    nibble = 0
    for weights in vectors:
        offsets.append(nibble)
        lengths.append(len(weights))
        nibble += len(weights)
    if nibble > 0xFFFF:
        raise props.GenerationError("hyphenation weight table no longer fits 16-bit offsets")
    bits = [0] * ((nibble + 1) // 2)
    nibble = 0
    for weights in vectors:
        for value in weights:
            index = nibble >> 1
            if (nibble & 1) == 0:
                bits[index] |= value & 0x0F
            else:
                bits[index] |= (value & 0x0F) << 4
            nibble += 1
    return offsets, lengths, bits


def parse_exceptions(path: Path) -> list[tuple[str, tuple[int, ...]]]:
    """Parses TeX \\hyphenation{...} exception words (a-b-c marks breaks)."""
    text = props.read_text(path)
    body = "\n".join(line.split("%", 1)[0] for line in text.splitlines())
    exceptions: dict[str, tuple[int, ...]] = {}
    marker = "\\hyphenation{"
    search = 0
    while True:
        start = body.find(marker, search)
        if start < 0:
            break
        index = start + len(marker)
        depth = 1
        end = index
        while end < len(body):
            character = body[end]
            if character == "{":
                depth += 1
            elif character == "}":
                depth -= 1
                if depth == 0:
                    break
            end += 1
        if depth != 0:
            raise props.GenerationError(f"{path}: unterminated \\hyphenation{{ block")
        for token in body[index:end].split():
            token = token.strip()
            if not token:
                continue
            letters: list[str] = []
            positions: list[int] = []
            for character in token:
                if character == "-":
                    if letters:
                        positions.append(len(letters))
                    continue
                folded = fold_letter(character)
                if folded is None:
                    letters = []
                    break
                letters.append(folded)
            if not letters:
                continue
            word = "".join(letters)
            exceptions[word] = tuple(position for position in positions
                                     if 0 < position < len(word))
        search = end + 1
    return sorted(exceptions.items())


def build_arrays(language: dict, nodes, root: int) -> dict:
    edge_start: list[int] = []
    edge_count: list[int] = []
    edge_labels: list[int] = []
    edge_children: list[int] = []
    node_weight: list[int] = []
    weight_ids: dict[tuple[int, ...], int] = {}
    weight_vectors: list[tuple[int, ...]] = []
    for weights_at, edges in nodes:
        edge_start.append(len(edge_labels))
        edge_count.append(len(edges))
        for label, child in edges:
            edge_labels.append(ord(label))
            edge_children.append(child)
        if weights_at is None:
            node_weight.append(0xFFFF)
        else:
            weight_id = weight_ids.get(weights_at)
            if weight_id is None:
                weight_id = len(weight_vectors)
                weight_ids[weights_at] = weight_id
                weight_vectors.append(weights_at)
            node_weight.append(weight_id)
    offsets, lengths, bits = pack_weights(weight_vectors)
    return {
        "edge_start": edge_start,
        "edge_count": edge_count,
        "edge_labels": edge_labels,
        "edge_children": edge_children,
        "node_weight": node_weight,
        "offsets": offsets,
        "lengths": lengths,
        "bits": bits,
        "root": root,
        "node_count": len(nodes),
        "edge_total": len(edge_labels),
        "weight_count": len(offsets),
    }


def emit_header(version: str, languages: list[dict]) -> str:
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
        f" * Generated from hyph-utf8 pattern files ({version}).",
        " * DO NOT EDIT BY HAND; see tools/unicode/generate_hyphenation.py.",
        " */",
        "",
        "#ifndef CORAL_UI_UNICODE_HYPHENATIONPATTERNS_GENERATED_H",
        "#define CORAL_UI_UNICODE_HYPHENATIONPATTERNS_GENERATED_H",
        "",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        "namespace coral::ui::unicode {",
        "",
        "/** One TeX \\\\hyphenation{...} exception; positions are letter indices. */",
        "struct HyphenationException {",
        "  const char* word;",
        "  const uint8_t* positions;",
        "  uint8_t count;",
        "};",
        "",
        "/** Minimized Liang pattern DAFSA with nibble-packed weights. */",
        "struct HyphenationTable {",
        "  const uint16_t* nodeEdgeStart;",
        "  const uint8_t* nodeEdgeCount;",
        "  const uint16_t* nodeWeight;   // 0xFFFF when the node is not a pattern terminal",
        "  const uint8_t* edgeLabel;",
        "  const uint16_t* edgeChild;",
        "  const uint16_t* weightOffset; // nibble index into weightBits",
        "  const uint8_t* weightLength;  // number of weights, pattern letters + 1",
        "  const uint8_t* weightBits;",
        "  uint16_t nodeCount;",
        "  uint16_t edgeCount;",
        "  uint16_t weightCount;",
        "  uint16_t root;",
        "  uint8_t leftMin;",
        "  uint8_t rightMin;",
        "  const HyphenationException* exceptions;",
        "  uint16_t exceptionCount;",
        "};",
        "",
        "enum class HyphenationLanguage : uint8_t {",
    ]
    for language in languages:
        lines.append(f"  {language['name']},")
    lines.extend(
        [
            "  Count,",
            "};",
            "",
            "[[nodiscard]] const HyphenationTable& hyphenationTable(HyphenationLanguage language) noexcept;",
            "[[nodiscard]] const char* hyphenationLanguageTag(HyphenationLanguage language) noexcept;",
            "[[nodiscard]] size_t hyphenationLanguageCount() noexcept;",
            "",
            "inline constexpr uint16_t kHyphenationNodeCount = "
            + str(sum(language["node_count"] for language in languages)) + ";",
            "",
            "} // namespace coral::ui::unicode",
            "",
            "#endif // CORAL_UI_UNICODE_HYPHENATIONPATTERNS_GENERATED_H",
            "",
        ]
    )
    return "\n".join(lines)


def emit_array(lines: list[str], name: str, values: list[int], width: int = 16,
    hex_values: bool = False) -> None:
    lines.append(f"constexpr uint16_t {name}[{len(values)}] = {{"
                 if width == 16 else f"constexpr uint8_t {name}[{len(values)}] = {{")
    for start in range(0, len(values), 24):
        chunk = values[start:start + 24]
        rendered = [f"0x{value:04X}" if hex_values else str(value) for value in chunk]
        lines.append("  " + ", ".join(rendered) + ",")
    lines.append("};")
    lines.append("")


def emit_source(languages: list[dict]) -> str:
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
        " * Generated from hyph-utf8 pattern files.",
        " * DO NOT EDIT BY HAND; see tools/unicode/generate_hyphenation.py.",
        " */",
        "",
        '#include "unicode/HyphenationPatterns.generated.h"',
        "",
        "namespace coral::ui::unicode {",
        "namespace {",
        "",
    ]
    total_bytes = 0
    for language in languages:
        prefix = "k" + language["name"]
        arrays = language["arrays"]
        emit_array(lines, prefix + "NodeEdgeStart", arrays["edge_start"], hex_values=False)
        emit_array(lines, prefix + "NodeEdgeCount", arrays["edge_count"], width=8)
        emit_array(lines, prefix + "NodeWeight", arrays["node_weight"], hex_values=True)
        emit_array(lines, prefix + "EdgeLabel", arrays["edge_labels"], width=8)
        emit_array(lines, prefix + "EdgeChild", arrays["edge_children"])
        emit_array(lines, prefix + "WeightOffset", arrays["offsets"])
        emit_array(lines, prefix + "WeightLength", arrays["lengths"], width=8)
        emit_array(lines, prefix + "WeightBits", arrays["bits"], width=8)
        exceptions = language.get("exceptions", [])
        for exception_index, (_, positions) in enumerate(exceptions):
            if positions:
                emit_array(lines, f"{prefix}Exception{exception_index}", list(positions), width=8)
        lines.append(f"constexpr HyphenationException {prefix}Exceptions[] = {{")
        for exception_index, (word, positions) in enumerate(exceptions):
            pointer = f"{prefix}Exception{exception_index}" if positions else "nullptr"
            lines.append(f'  {{ "{word}", {pointer}, {len(positions)} }},')
        if not exceptions:
            lines.append('  { "", nullptr, 0 },')
        lines.append("};")
        lines.append("")
        total_bytes += (len(arrays["edge_start"]) * 2 + len(arrays["edge_count"]) +
                        len(arrays["node_weight"]) * 2 + len(arrays["edge_labels"]) +
                        len(arrays["edge_children"]) * 2 + len(arrays["offsets"]) * 2 +
                        len(arrays["lengths"]) + len(arrays["bits"]))
    lines.extend(
        [
            f"constexpr size_t kTableBytes = {total_bytes};",
            'static_assert(kTableBytes <= 512 * 1024,',
            '  "hyphenation tables must stay within the 512 KiB budget");',
            "",
            "const HyphenationTable kTables[] = {",
        ]
    )
    for language in languages:
        prefix = "k" + language["name"]
        arrays = language["arrays"]
        lines.append(f"  {{ {prefix}NodeEdgeStart, {prefix}NodeEdgeCount, {prefix}NodeWeight,")
        lines.append(f"    {prefix}EdgeLabel, {prefix}EdgeChild, {prefix}WeightOffset,")
        lines.append(f"    {prefix}WeightLength, {prefix}WeightBits,")
        lines.append(f"    {arrays['node_count']}, {arrays['edge_total']}, "
                     f"{arrays['weight_count']}, {arrays['root']},")
        lines.append(f"    {language['left']}, {language['right']},")
        lines.append(f"    {prefix}Exceptions, {len(language.get('exceptions', []))} }},")
    lines.extend(
        [
            "};",
            "",
            'static_assert(sizeof(kTables) / sizeof(kTables[0]) == '
            'static_cast<size_t>(HyphenationLanguage::Count),',
            '  "hyphenation table count must match HyphenationLanguage");',
            "",
            "const char* const kLanguageTags[] = {",
        ]
    )
    for language in languages:
        lines.append(f'  "{language["tag"]}",')
    lines.extend(
        [
            "};",
            "",
            "} // namespace",
            "",
            "const HyphenationTable& hyphenationTable(const HyphenationLanguage language) noexcept {",
            "  const size_t index = static_cast<size_t>(language);",
            "  return index < static_cast<size_t>(HyphenationLanguage::Count) ? kTables[index] : kTables[0];",
            "}",
            "",
            "const char* hyphenationLanguageTag(const HyphenationLanguage language) noexcept {",
            "  const size_t index = static_cast<size_t>(language);",
            '  return index < static_cast<size_t>(HyphenationLanguage::Count) ? kLanguageTags[index] : "en";',
            "}",
            "",
            "size_t hyphenationLanguageCount() noexcept {",
            "  return static_cast<size_t>(HyphenationLanguage::Count);",
            "}",
            "",
            "} // namespace coral::ui::unicode",
            "",
        ]
    )
    return "\n".join(lines)


def load_manifest(data_dir: Path) -> dict:
    version_path = data_dir / "version.json"
    try:
        manifest = json.loads(props.read_text(version_path))
    except json.JSONDecodeError as error:
        raise props.GenerationError(f"{version_path}: invalid JSON: {error}") from error
    files = manifest.get("files")
    if not isinstance(files, dict):
        raise props.GenerationError(f"{version_path}: missing files object")
    return manifest


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
        manifest = load_manifest(data_dir)
        version = str(manifest.get("unicodeVersion", "unknown"))
        files = manifest["files"]

        prepared: list[dict] = []
        for language in LANGUAGES:
            path = data_dir / "hyphenation" / language["data"]
            entry = files.get(language["data"])
            if not isinstance(entry, dict):
                raise props.GenerationError(
                    f"{data_dir / 'version.json'}: missing files.{language['data']}"
                )
            actual = props.file_sha256(path)
            if actual != entry.get("sha256"):
                raise props.GenerationError(
                    f"{path}: sha256 {actual} does not match version.json "
                    f"{entry.get('sha256')}; update version.json only for a deliberate pattern change"
                )
            patterns, skipped = parse_patterns(path)
            nodes, root = build_dafsa(patterns)
            if len(nodes) > 0xFFFF:
                raise props.GenerationError(f"{path}: DAFSA no longer fits 16-bit node ids")
            arrays = build_arrays(language, nodes, root)
            prepared.append({**language, "arrays": arrays,
                             "node_count": arrays["node_count"],
                             "exceptions": parse_exceptions(path)})
            if not args.quiet:
                print(
                    f"{language['data']}: {len(patterns)} patterns"
                    f"{f' ({skipped} non-ASCII skipped)' if skipped else ''}, "
                    f"{arrays['node_count']} nodes, "
                    f"{arrays['edge_total']} edges, {arrays['weight_count']} weight vectors"
                )

        header = emit_header(version, prepared)
        source = emit_source(prepared)
        outputs = {
            "HyphenationPatterns.generated.h": header,
            "HyphenationPatterns.generated.cpp": source,
        }
        for name, content in outputs.items():
            if not props.compare_and_write(output_dir / name, content, args.check):
                return 1
        return 0
    except props.GenerationError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
