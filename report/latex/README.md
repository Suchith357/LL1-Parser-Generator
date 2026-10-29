# VIT Project Report — LL(1) Parser Generator (Team 4)

**Deliverable: exactly one self-contained LaTeX file —
[`LL1_Parser_Generator_Team4_Report.tex`](LL1_Parser_Generator_Team4_Report.tex)**
(the compiled reference PDF [`LL1_Parser_Generator_Team4_Report.pdf`](LL1_Parser_Generator_Team4_Report.pdf)
sits beside it).

- **No `\input` / `\include` / external bibliography / external images.** The
  complete preamble, all chapters, all TikZ diagrams, the `thebibliography`
  environment and both annexures live in this single file.
- **True Times New Roman** is used when present (Overleaf / Windows); on
  systems without it the source falls back to TeX Gyre Termes, a
  metric-compatible clone, and the README of the VIT template accepts this.

## Engine and compilation

| Where | Steps |
|---|---|
| **Overleaf** (recommended) | Upload the single `.tex` file → Menu → **Compiler = XeLaTeX** → Recompile twice (second pass fixes TOC/longtable widths). |
| **Local TeX Live / MiKTeX** | `xelatex LL1_Parser_Generator_Team4_Report.tex` — run **twice**. |
| **Local check used here** | tectonic 0.15.0 (`tectonic -X compile`, XeTeX engine): builds with **0 errors** to a 67-page PDF; only cosmetic underfull-hbox notices remain. |

XeLaTeX is required because Times New Roman is set through `fontspec`.

## Template compliance (VIT SCOPE)

A4 · margins top 4 cm / bottom 3 cm / left 4.5 cm / right 2.5 cm ·
Times New Roman · body 12 pt justified 1.5-spaced · main headings 14 pt bold
CAPITALS · subheadings 14 pt bold Title Case · page numbers top-right ·
Roman numerals for preliminary pages, Arabic from Chapter 1 ·
figure captions below / table captions above · abstract 300–500 words ·
IEEE numbered citations in order of first citation ·
hierarchical 1.1 / 1.1.1 numbering · alphabetical abbreviations page.

## Before you submit — fill these placeholders

1. **Names / people** — student names (5), guide name & designation, HOD,
   Dean SCHOOL/Dean SR, examiner fields: search for `<<` in the `.tex`.
2. **Anything marked `XXXX`** — institutional blanks you should review once.
3. **Execution month/year** — Table 5.1 (`20 October 2026` is a placeholder);
   re-run `sh testcases/run_tests.sh` on your own machine before quoting it.

## Context files referenced by the report (kept outside this folder)

`include/`, `src/`, `testcases/` and `Makefile` are the implementation the
report documents; the report text contains the necessary code excerpts, so
the LaTeX compile needs nothing beyond the single file in this folder.
