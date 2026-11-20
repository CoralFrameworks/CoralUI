# Hyphenation pattern data

The `.tex` files are the English, Italian and Spanish Liang pattern sets from the
[hyph-utf8](https://www.hyphenation.org/tex) project. They are stored verbatim,
including their copyright/licence headers. The runtime now ships 13 languages:
`en`, `it`, `es`, `fr`, `pt`, `de`, `ru`, `el`, `nl`, `sv`, `da`, `fi`, `ca`.

| File | Language | Licence |
|---|---|---|
| `hyph-en-us.tex` | American English (`en`) | Copying and distribution permitted in any medium without royalty provided the copyright notice and this notice are preserved (Gerard D.C. Kuiken, 1990-2005) |
| `hyph-it.tex` | Italian (`it`) | LPPL 1.3 or later, or MIT (Claudio Beccari, 2008-2011) |
| `hyph-es.tex` | Spanish (`es`) | MIT/X11 (Javier Bezos, 1993-2019) |
| `hyph-fr.tex` | French (`fr`) | MIT (Daniel Flipo, Bernard Gaulle, Arthur Reutenauer, 1994-2016) |
| `hyph-pt.tex` | Portuguese (`pt`) | BSD 3-clause (Pedro J. de Rezende, J. Joao Dias Almeida, Leonardo Araujo, Aline Benevides, 1987-2024) |
| `hyph-de-1996.tex` | German (`de`, reformed 2006) | MIT (TeX-Trennmuster contributors, 2013-2024) |
| `hyph-ru.tex` | Russian (`ru`) | LPPL 1.2 or later (Alexander I. Lebedev, 1999-2003) |
| `hyph-el-monoton.tex` | Greek monoton (`el`) | LPPL or MIT (Dimitrios Filippou, 2008-2011) |
| `hyph-nl.tex` | Dutch (`nl`) | MIT (Piet Tutelaers, 1996) |
| `hyph-sv.tex` | Swedish (`sv`) | LPPL 1.2 or later (Jan Michael Rynning, 1994) |
| `hyph-da.tex` | Danish (`da`) | LPPL or MIT (Frank Jensen, 1994-2011) |
| `hyph-fi.tex` | Finnish (`fi`) | Patterns may be freely distributed (Kauko Saarinen, 1986-1989) |
| `hyph-ca.tex` | Catalan (`ca`) | LPPL 1.0 or later (Gonçal Badenes, Francina Turon, 1991-2003) |

The files are compiled by `tools/unicode/generate_hyphenation.py` into
`modules/ui/src/unicode/HyphenationPatterns.generated.{h,cpp}`: one minimized DAFSA
per language with nibble-packed Liang weights. The generated tables are
committed; the runtime never reads the `.tex` files.

The runtime folds ASCII + Latin-1 letters to their base letter (é→e, ñ→n, ç→c) and approximates
ligatures 1:1 (ß→s, æ→a, œ→o, þ→t) before matching, so accented words and German ß use the same
ASCII patterns. Cyrillic and Greek letters are transliterated 1:1 to ASCII labels.
TeX `\hyphenation{...}` exception lists are compiled into sorted per-language arrays
and checked before the Liang DAFSA; a word without a hyphen (for example `present`) is a no-break
exception. Adding a language that needs non-Latin-1 characters requires extending
`foldLatinLetter()` and the generator's folding.

To add a language, drop its pattern file here, pin its SHA-256 in
`tools/unicode/data/version.json`, extend the generator's `LANGUAGES` table and
add the language to `Hyphenation.cpp`'s tag dispatch.
