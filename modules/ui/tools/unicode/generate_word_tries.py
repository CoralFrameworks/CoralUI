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
"""Compile ICU Thai/Khmer/Lao word lists into minimized byte DAFSAs."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import struct
import sys
from pathlib import Path

LANGUAGES = {
    "thai": "ThaiDictionary.txt",
    "khmer": "KhmerDictionary.txt",
    "lao": "LaoDictionary.txt",
    "burmese": "BurmeseDictionary.txt",
}


def load_words(path: Path):
    words = []
    for line in path.read_text(encoding="utf-8").splitlines():
        value = line.strip()
        if value and not value.startswith("#"):
            words.append(value.encode("utf-8"))
    return sorted(set(words))


def minimize(words):
    root = {}
    for word in words:
        node = root
        for label in word:
            node = node.setdefault(label, {})
        node[None] = None

    intern = {}
    node_edges = {}
    resolved = {}

    def canonical(node):
        key = id(node)
        if key in resolved:
            return resolved[key]
        children = []
        for label, child in node.items():
            if label is None:
                continue
            children.append((label, canonical(child)))
        children.sort()
        signature = (None in node, tuple(children))
        node_id = intern.get(signature)
        if node_id is None:
            node_id = len(intern)
            intern[signature] = node_id
            node_edges[node_id] = (signature[0], children)
        resolved[key] = node_id
        return node_id

    return canonical(root), node_edges


def serialize(root_id, node_edges):
    nodes = [node_edges[index] for index in range(len(node_edges))]
    starts = []
    counts = []
    terminals = []
    edges = []
    for terminal, children in nodes:
        starts.append(len(edges))
        counts.append(len(children))
        terminals.append(1 if terminal else 0)
        for label, child in children:
            edges.append((child, label))
    data = bytearray()
    data += struct.pack("<I", len(nodes))
    for start, count, terminal in zip(starts, counts, terminals):
        data += struct.pack("<IHB", start, count, terminal)
    data += struct.pack("<I", len(edges))
    for child, label in edges:
        data += struct.pack("<IB", child, label)
    data += struct.pack("<I", root_id)
    return bytes(data), len(nodes), len(edges)


def chunk_literal(data: bytes):
    encoded = base64.b64encode(data).decode("ascii")
    return [encoded[index:index + 4000] for index in range(0, len(encoded), 4000)]


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

    tables = {}
    for prefix, name in LANGUAGES.items():
        path = data_dir / name
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != manifest["files"][name]["sha256"]:
            print(f"error: {path}: sha256 does not match version.json", file=sys.stderr)
            return 1
        root, nodes = minimize(load_words(path))
        data, node_count, edge_count = serialize(root, nodes)
        tables[prefix] = (chunk_literal(data), node_count, edge_count)

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
        " * Generated from the ICU Thai/Khmer/Lao/Burmese break dictionaries.",
        " * DO NOT EDIT BY HAND; see tools/unicode/generate_word_tries.py.",
        " */",
        "",
        "#ifndef CORAL_UI_UNICODE_WORDTRIES_GENERATED_H",
        "#define CORAL_UI_UNICODE_WORDTRIES_GENERATED_H",
        "",
        '#include "unicode/WordTrie.h"',
        "",
        "namespace coral::ui::unicode {",
        "",
        "[[nodiscard]] const WordTrie& thaiTrie();",
        "[[nodiscard]] const WordTrie& khmerTrie();",
        "[[nodiscard]] const WordTrie& laoTrie();",
        "[[nodiscard]] const WordTrie& burmeseTrie();",
        "",
        "} // namespace coral::ui::unicode",
        "",
        "#endif // CORAL_UI_UNICODE_WORDTRIES_GENERATED_H",
        "",
    ]
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
        '#include "unicode/WordTries.generated.h"',
        "",
        "namespace coral::ui::unicode {",
        "namespace {",
        "",
    ]
    for prefix, (chunks, _, _) in tables.items():
        name = prefix.capitalize()
        source.append(f"const char* const k{name}Blob[] = {{")
        source.extend(f'  "{chunk}",' for chunk in chunks)
        source.extend(["};", ""])
    source.extend(["", "} // namespace", ""])
    for prefix in LANGUAGES:
        name = prefix.capitalize()
        source.extend([
            f"const WordTrie& {prefix}Trie() {{",
            f"  static const DecodedWordTrie decoded = decodeWordTrie(k{name}Blob,",
            f"    sizeof(k{name}Blob) / sizeof(k{name}Blob[0]));",
            "  static const WordTrie trie(decoded.edgeStarts.data(), decoded.edgeCounts.data(),",
            "    decoded.terminals.data(), decoded.children.data(), decoded.labels.data(),",
            "    decoded.edgeStarts.size(), decoded.children.size(), decoded.root);",
            "  return trie;",
            "}",
            "",
        ])
    source.extend(["} // namespace coral::ui::unicode", ""])

    files = {
        output_dir / "WordTries.generated.h": "\n".join(header),
        output_dir / "WordTries.generated.cpp": "\n".join(source),
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
        for prefix, (_, nodes, edges) in tables.items():
            print(f"{prefix}: {nodes} nodes, {edges} edges")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
