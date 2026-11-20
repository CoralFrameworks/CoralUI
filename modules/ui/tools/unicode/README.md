# Unicode line-breaking data and generators

This directory owns the **build-time** Unicode data and rule compiler for `modules/ui`. Runtime code never
reads these files and never downloads anything; the generated tables under `modules/ui/src/unicode/` are
committed.

The design and invariants are recorded in the application's Unicode design document; test counts and
performance baselines live in its GUI baseline data.
Stages 0–4 of line breaking are implemented: pinned Unicode 18.0.0 data, LB1/context properties, a JSON
rule DSL, an NFA→DFA compiler, the runtime iterator, and the full `LineBreakTest.txt` gate. Bidi
implements the full UBA (P2/P3, X1–X10, W1–W7, N0–N2, I1–I2, L1–L2) and passes both official conformance
files. `TextLayout`/`RichText`/`TextInput` expose and draw per-line L2 visual runs with RTL shaping and
L4 mirroring, TextInput maps caret/pointer/selection through those runs, and `TextBidi` caches resolved
paragraphs in an LRU. UAX #29 grapheme segmentation passes GraphemeBreakTest.txt 853/853, word boundaries
pass WordBreakTest.txt 1,944/1,944, and both drive TextLayout/RichText/TextInput editing. The CSS-like `LineBreakPolicy` and Thai/Khmer/Lao/Burmese dictionary
backends are in place, `TextLayout` has incremental reflow plus paragraph/line/run sharing, and the
`test_ui` suite is at 217 tests. Remaining work is tracked in the design document §15 (dictionary
quality, vertical typography, performance).

## Pinned version

Unicode **18.0.0**, pinned by `data/version.json`. The manifest records the SHA-256, date and source
URL of every input, and all generators refuse to run when a file's header version or checksum does
not match. Upgrading Unicode is one atomic change:

1. replace the UCD inputs with the same release:
   `LineBreak.txt`, `DerivedGeneralCategory.txt`, `EastAsianWidth.txt`, `emoji-data.txt`,
   `DerivedBidiClass.txt`, `BidiBrackets.txt`, `WordBreakProperty.txt` and
   `modules/ui/tests/unicode/LineBreakTest.txt` plus `WordBreakTest.txt`;
2. update `unicodeVersion`, the dates and every `sha256` in `data/version.json`;
3. run all generators, commit the regenerated files, and run `test_ui` plus CTest.

Never mix files from two releases: `LineBreak.txt` and `LineBreakTest.txt` are a pair, and the LB1 /
context rules need the other three; `DerivedBidiClass.txt` must be the same release.

## Files

| File | Purpose |
|---|---|
| `data/LineBreak.txt` | UCD `Line_Break` property, with its `@missing: 0000..10FFFF; XX` default |
| `data/DerivedGeneralCategory.txt` | `General_Category`, for LB1 `SA` and the Pi/Pf flags |
| `data/EastAsianWidth.txt` | East_Asian_Width `F`/`W`/`H`, for LB19a/LB21a/LB30 |
| `data/emoji-data.txt` | Extended_Pictographic, for LB30b and emoji ZWJ sequences |
| `data/DerivedBidiClass.txt` | Bidi_Class, for the full UBA resolver |
| `data/BidiBrackets.txt` | Bidi_Paired_Bracket/Type, for N0 |
| `data/BidiMirroring.txt` | Bidi_Mirroring_Glyph, for L4 |
| `data/GraphemeBreakProperty.txt` | Grapheme_Cluster_Break, for UAX #29 |
| `data/DerivedCoreProperties.txt` | InCB Linker/Consonant/Extend, for GB9c |
| `data/WordBreakProperty.txt` | Word_Break, for UAX #29 word segmentation |
| `data/ThaiDictionary.txt` | ICU Thai word list, for the first BreakDictionary backend |
| `data/KhmerDictionary.txt` | ICU Khmer word list, for the second BreakDictionary backend |
| `data/LaoDictionary.txt` | ICU Lao word list, for the third BreakDictionary backend |
| `data/BurmeseDictionary.txt` | ICU Burmese word list, for the Burmese backend |
| `data/hyphenation/hyph-en-us.tex` | hyph-utf8 American English Liang patterns (licence in the file header) |
| `data/hyphenation/hyph-it.tex` | hyph-utf8 Italian Liang patterns (LPPL/MIT) |
| `data/hyphenation/hyph-es.tex` | hyph-utf8 Spanish Liang patterns (MIT/X11) |
| `data/hyphenation/hyph-fr.tex` | hyph-utf8 French Liang patterns (MIT) |
| `data/hyphenation/hyph-pt.tex` | hyph-utf8 Portuguese Liang patterns (BSD 3-clause) |
| `data/hyphenation/hyph-de-1996.tex` | hyph-utf8 German Liang patterns (MIT) |
| `data/hyphenation/hyph-ru.tex` | hyph-utf8 Russian Liang patterns (LPPL) |
| `data/hyphenation/hyph-el-monoton.tex` | hyph-utf8 Greek monoton Liang patterns (LPPL/MIT) |
| `data/hyphenation/README.md` | Provenance/licence note for the pattern files |
| `../../tests/unicode/GraphemeBreakTest.txt` | Official UAX #29 conformance data (853 cases) |
| `../../tests/unicode/WordBreakTest.txt` | Official UAX #29 word-boundary conformance data (1,944 cases) |
| `data/version.json` | Version pin plus source URLs, dates and SHA-256 checksums |
| `data/UNICODE-LICENSE.txt` | Unicode License V3, required when redistributing the data |
| `generate_linebreak_properties.py` | Deterministic generator for class/flag range tables |
| `generate_uax14_dfa.py` | Compiles `uax14_rules.json` into the runtime DFA |
| `generate_bidi_properties.py` | Deterministic Bidi_Class range table |
| `generate_bidi_brackets.py` | Deterministic Bidi_Paired_Bracket table |
| `generate_bidi_mirroring.py` | Deterministic Bidi_Mirroring_Glyph table |
| `generate_grapheme_properties.py` | GCB, Extended_Pictographic and InCB tables |
| `generate_wordbreak_properties.py` | UAX #29 Word_Break ranges and ASCII fast table |
| `generate_hyphenation.py` | Compiles English/Italian/Spanish/French/Portuguese/German Liang patterns into minimized DAFSAs with nibble-packed weights |
| `generate_word_tries.py` | Thai/Khmer/Lao/Burmese word lists compiled into minimized byte DAFSAs |
| `uax14_rules.json` | Unicode 18 default UAX #14 rules, encoded as boundary-anchored patterns |
| `uax14_rules.schema.json` | Closed schema for the rule file |
| `../../tests/unicode/LineBreakTest.txt` | Official conformance data (19,346 cases, 3.1 MB) |

`LineBreakTest.txt` lives beside the UI tests rather than here so there is exactly one copy and it
ships with `test_ui` sources only (never an install or application package).

## Generators

```sh
python tools/unicode/generate_linebreak_properties.py
python tools/unicode/generate_uax14_dfa.py
python tools/unicode/generate_bidi_properties.py
python tools/unicode/generate_bidi_brackets.py
python tools/unicode/generate_bidi_mirroring.py
python tools/unicode/generate_grapheme_properties.py
python tools/unicode/generate_wordbreak_properties.py
python tools/unicode/generate_hyphenation.py
python tools/unicode/generate_word_tries.py
python tools/unicode/generate_linebreak_properties.py --check --quiet   # CI gate
python tools/unicode/generate_uax14_dfa.py --check --quiet              # CI gate
python tools/unicode/generate_bidi_properties.py --check --quiet        # CI gate
python tools/unicode/generate_bidi_brackets.py --check --quiet          # CI gate
python tools/unicode/generate_bidi_mirroring.py --check --quiet         # CI gate
python tools/unicode/generate_grapheme_properties.py --check --quiet    # CI gate
python tools/unicode/generate_wordbreak_properties.py --check --quiet  # CI gate
python tools/unicode/generate_hyphenation.py --check --quiet            # CI gate
python tools/unicode/generate_word_tries.py --check --quiet             # CI gate
```

Outputs (all committed, never hand-edited):

- `LineBreakProperties.generated.h/.cpp` — 49 `LineBreakClass` values, 3,685 contiguous ranges
  covering `U+0000..U+10FFFF`, 188 non-zero flag ranges, raw, LB1-resolved and context-flag lookups.
  Ranges are stored as a sorted `uint32_t` end-boundary array plus a deduplicated class-pair id
  (`lineBreakRangeAt()` materializes a full range for tests); property tables are **20.3 KiB**,
  under the 48 KiB budget;
- `UnicodeVersion.generated.h` — version, all input checksums and generator version;
- `LineBreakDfa.generated.h` + `.inc` — **154 states, 101 symbols, 125 token codes, 79 rule
  alternatives, 28 unique transition rows**. The `.inc` holds the tables and accessors and is included
  by `LineBreakIterator.cpp` so the hot path inlines; the transition table is 2,982 bytes (28 byte rows
  plus a 154-byte state map) and the generated DFA tables total 36.4 KiB, under the 64 KiB budget
  (static assertions enforce both budgets);
- `BidiProperties.generated.h/.cpp` — 1,272 Bidi_Class ranges plus a 128-entry ASCII fast path;
- `BidiBrackets.generated.h` — 130 Bidi_Paired_Bracket records for N0;
- `BidiMirroring.generated.h` — 438 Bidi_Mirroring_Glyph records for L4;
- `GraphemeBreakProperties.generated.h/.cpp` — 1,746 GCB ranges plus Extended_Pictographic and InCB;
- `WordBreakProperties.generated.h/.cpp` — 1,935 UAX #29 Word_Break ranges (19 classes) plus a
  128-entry ASCII fast table and an on-demand `wordBreakRangeAt()` view; 9.6 KiB;
- `HyphenationPatterns.generated.h/.cpp` — English/Italian/Spanish/French/Portuguese/German Liang
  patterns compiled into minimized DAFSAs: English 4,938 patterns (3,930 nodes, 7,574 edges, 578
  weight vectors), Italian 355 (178), Spanish 4,694 (2,014), French 1,145 (1,279), Portuguese 427
  (90), German 36,709 (16,313), Russian 7,021 (2,331), Greek 549 (64). ~339 KiB of tables in total.
  Patterns and words fold Latin-1 letters to ASCII (é→e, ñ→n, ç→c), approximate ligatures (ß→s,
  æ→a, œ→o) and transliterate Cyrillic/Greek 1:1 to ASCII labels. Nodes store a sorted
  edge range, a terminal weight id (0xFFFF for non-terminals) and weights are nibble-packed;
  `Hyphenation.cpp` walks the selected table with the language's left/right hyphen minima
  (English 2/3, Italian/Spanish/French/German 2/2, Portuguese 2/3); TeX `\hyphenation{}` exception
  words (including no-hyphen entries) are compiled into sorted per-language exception arrays and
  checked before the DAFSA;
- `WordTries.generated.h/.cpp` — minimized byte DAFSAs for Thai (51,744 nodes), Khmer (202,157),
  Lao (74,090) and Burmese (132,848), base64-encoded and decoded once at first use. `scanWordTrie()`
  keeps boundaries on grapheme clusters and falls back to the next cluster for unmatched runs.

Both Bidi tables are data only; `modules/ui/src/unicode/BidiResolver.cpp` implements the UBA and is validated
by the official `BidiTest.txt` and `BidiCharacterTest.txt`.

All generators use only the Python standard library, sort deterministically, emit LF line endings and
never write a timestamp. `--check` regenerates in memory and compares byte-for-byte.

## Rule DSL

`uax14_rules.json` is not a general regex language. Each entry is one UAX rule alternative:

```json
{
  "id": "LB14",
  "priority": 140,
  "action": "no_break",
  "left": [{"classes": ["OP"]}, {"repeat": {"min": 0, "items": [{"classes": ["SP"]}]}}],
  "right": []
}
```

- `left` is the context ending at the boundary, `right` the context starting at it;
- right contexts may be at most three tokens (LB25 `PO × OP IS NU`);
- repeat groups are `*` only (`min: 0`, unordered items), which is all UAX #14 needs after LB9/LB10;
- predicates are closed sets over `classes` / `notClasses` and the flags `pi`, `pf`, `ea`, `epCn`,
  `u25cc`, `zwjTail`; `anyOf` expresses the small unions such as `(AK | U+25CC | AS)`;
- LB2 (sot) and LB3 (eot) are enforced by the iterator; LB9/LB10 are token normalization, not DFA
  rules.

The compiler builds a left-context NFA per alternative, determinizes the union (suffix matching, so
short patterns still match after arbitrary history), minimizes it, merges equivalent symbols and
pre-indexes zero/one-token right contexts. Right contexts of two or three tokens are resolved with a
two-token lookahead queue in the runtime.

## LB1 and context properties

`lineBreakClass()` returns the raw UCD property. `resolveLineBreakClass()` applies UAX #14 LB1:

| Raw class | Resolved |
|---|---|
| `AI`, `SG`, `XX` | `AL` |
| `CJ` | `NS` |
| `SA` + `General_Category` `Mn`/`Mc` | `CM` |
| `SA` otherwise | `AL` |
| everything else | unchanged |

`lineBreakFlags()` exposes the properties needed after LB1:

- `Pi` / `Pf` — LB15a/LB15b/LB19 quotation resolution;
- East Asian `F`/`W`/`H` — LB19a/LB21a/LB30;
- `Extended_Pictographic` & `Cn` — LB30b;
- U+25CC DOTTED CIRCLE — LB28a;
- a trailing ZWJ bit is added by the tokenizer for LB8a (it is not a code-point property).

## Tests

`modules/ui/tests/unicode/` contains:

- `test_LineBreakProperties.cpp` — independently parses all five UCD inputs in C++ and compares
  **all 1,114,112 code points** for classes, LB1 resolution and every context flag; checks range
  ordering and coverage;
- `LineBreakTest.h/.cpp` — strict official-file parser and conformance runner (all cases, capped
  diagnostics with code points, raw/resolved classes, expected/actual boundaries);
- `test_LineBreakTest.cpp` — proves every one of the 19,346 cases is parsed, that a conforming stub
  passes all 80,131 boundaries, and that malformed input is rejected with line numbers;
- `test_LineBreakDfa.cpp` — runs the real generated DFA against the official file and requires
  **19,346/19,346 cases, 80,131/80,131 boundaries**; also covers UTF-8 byte offsets, CRLF, RichText
  CJK wrapping, L2 visual runs and L4 mirroring. Its `UnicodeLineBreakPerformance.*` disabled tests are the
  benchmark source; numbers live in the GUI baseline data, and the gate is `unicode_perf_gate`;
- `test_BidiProperties.cpp` — independently parses `DerivedBidiClass.txt` and compares all 1,114,112
  code points, plus representative strong/weak/isolate classes;
- `test_BidiConformance.cpp` — runs the full UBA against **BidiCharacterTest.txt (91,707 cases)** and
  **BidiTest.txt (490,846 lines / 770,241 direction cases)**, including explicit embeddings, isolates,
  bracket pairs and overflow behavior;
- `test_GraphemeBreak.cpp` — runs the official `GraphemeBreakTest.txt` (853 cases) and checks
  GCB/Extended_Pictographic/InCB representatives;
- `test_WordBreak.cpp` — runs the official `WordBreakTest.txt` (1,944 cases) plus representative
  classes and `wordRangeAt()` cases;
- `test_Hyphenation.cpp` — Liang pattern lookup (hyphenation/com-puter), hyphen minima, non-word
  rejection, the policy opportunities, and TextLayout hyphen/SHY line marking;
- `test_LineBreakPolicy.cpp` — covers break-all, keep-all, strict/loose, overflow-wrap and
  Thai/Khmer/Lao/Burmese dictionary word boundaries, grapheme fallback and empty-language behavior.

CTest registers `unicode_generate_check`, `unicode_dfa_generate_check`, `unicode_bidi_generate_check`,
`unicode_bidi_brackets_check`, `unicode_bidi_mirroring_check`, `unicode_grapheme_generate_check`,
`unicode_wordbreak_generate_check`, `unicode_hyphenation_generate_check`,
`unicode_word_tries_check` and `unicode_perf_gate` as reproducibility/performance gates.

## Tailoring

ui/LineBreakPolicy.h composes CSS-like policy on top of the default opportunities:
strictness, word-break, overflow-wrap, language and an optional BreakDictionary.
computeTailoredLineBreaks() keeps mandatory breaks first, adds break-all grapheme boundaries,
removes CJK–CJK breaks for keep-all, applies the CSS-required normal/loose CJK hyphen,
small-kana, iteration-mark, IN, centered-punctuation and PO/PR rules, and merges dictionary results.
ThaiBreakDictionary, KhmerBreakDictionary, LaoBreakDictionary and BurmeseBreakDictionary share
minimized byte DAFSAs compiled from the ICU word lists; their boundaries stay on grapheme clusters and
unmatched runs fall back to the next cluster. `hyphenation = true` with an English, Italian, Spanish,
French, Portuguese, German, Russian or Greek language tag (`en`, `it-IT`, `es_ES`, `fr`, `pt-BR`,
`de-DE`, `ru`, `el`, ...) adds Liang pattern breaks for Latin-1/Cyrillic/Greek words and marks them
so the layout draws a hyphen; U+00AD soft hyphens also break and render a hyphen, and TextLayout,
RichText, TextLayer and TextInput draw it. CJK uses UAX #14 line breaking; no hyphenation table.
`overflow-wrap` defaults to CSS `normal` (an overlong word
overflows); `BreakWord` performs grapheme emergency breaks and is selected explicitly by `TextInput`,
while `Anywhere` adds all grapheme boundaries. Policy (flags, language, dictionary), alignment and
line height are part of the TextLayout/RichText cache keys, so non-default policies are cached too;
the default path still streams UAX #14 opportunities without a result vector. Multi-line
TextLayout splits at paragraph separators and caches each paragraph layout, so an edit in one
paragraph can reuse the others. TextLine::text is a view into the block TextSource rope, so lines no
longer allocate or copy their UTF-8 bytes; incremental edits reuse old blocks and only copy the
changed span. updateTextLayout() reflows from the affected line,
reusing every complete line before the edit; a suffix line is reused only after probing its next
line confirms the boundary is still exact. LineList shares read-only line ranges from previous layouts. LineRef applies a segment
offsetDelta lazily, so prefix and suffix line blocks are shared without copying or modifying the
old layout. Visual runs are shared through VisualRunList and offset by the same proxy.
Non-Bidi lines keep a shared TextBlob shaped once at layout time, and each Bidi visual run keeps a shared
LazyTextBlob whose mirrored shaping happens on first draw and is then reused by every layout.
Equivalent lazy runs are interned by typeface, size, direction, mirror flag and text, and each
VisualRunList references one VisualRunBlock whose single vector stores each run together with
its lazy holder, so a line needs one run allocation rather than two parallel vectors. maxWidth() scans segment widths without materializing proxies. Bidi edits reflow only the affected paragraph and share the untouched prefix/suffix paragraphs;
the paragraph LRU keys on a precomputed hash plus a shared text-block slice, stores shared level
vectors, and avoids both UBA and paragraph-text copies on hit. Reflowed Bidi lines also reuse a
VisualRunBlock cache keyed by line text, levels and font;
truncation edits fall back to a full layout.
TextInput uses the same path for same-width edits, passes the known byte edit range to skip prefix/suffix scans, and keeps a full layout for font/fallback changes. Bidi detection scans TextSource blocks without concatenating the full text.

Known deviations in this layer: hyphenation covers ASCII + Latin-1, Cyrillic and Greek (English,
Italian, Spanish, French, Portuguese, German, Russian, Greek monoton) with 1:1 ligature/Cyrillic/Greek
approximations (ß→s, æ→a, œ→o); CJK uses UAX #14 instead of hyphenation, and polytonic Greek
diacritics beyond monoton are folded to their base letters. `hyphenate-limit-chars/zone/lines` are
applied in TextLayout.

## Performance

`computeLineBreakActions()` returns every boundary for conformance; `computeLineBreakOpportunities()` is
the layout-facing variant, decodes UTF-8 on the fly and drops NoBreak boundaries. The measured
Debug/Release cost for 1 MiB of ASCII in the disabled `UnicodeLineBreakPerformance` tests is roughly:

| path | Debug | Release |
|---|---:|---:|
| actions (all boundaries) | ≈8.7 ms (all `a`) | ≈2.0–2.4 ms |
| opportunities only (streaming) | ≈43 ms | ≈6.6–7.0 ms |
| boundaries (UTF-8 offsets) | ≈152 ms | ≈10.8–11.0 ms |

Budget and measurement: the design target is <2 ms per 1 MiB for the boundary iterator, and the
current numbers are tracked in the GUI baseline data, with `unicode_perf_gate` as the regression
guard. Two structural costs are known and not yet addressed: the layout-side
per-codepoint `measure()` and the per-line `std::string` accumulation followed by a second full-line
measurement. Data-table compression is optional; the tables already fit the DFA/attribute budget.
