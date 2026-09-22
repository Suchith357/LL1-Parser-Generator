# Report Folder

This folder contains the final project document and supporting material.

## Files

| File | Purpose |
|---|---|
| `project_report.md` | Complete draft of the final project report (structure follows the faculty template). |

## Before submission

1. Fill in the actual `Actual Output` and `Pass/Fail` columns of the test
   table from a real run (`make test` and the demo command in
   `docs/user_guide.md` §5) — do not mark anything passed that was not run.
2. Capture screenshots into `docs/screenshots/` and reference them where the
   report has `[SCREENSHOT: ...]` placeholders:
   - grammar display, FIRST sets, FOLLOW sets (one combined shot)
   - parsing table and LL(1) verdict
   - accepted parse of `id + id * id` with trace
   - syntax-error diagnostic for `id + * id`
   - conflict report for the non-LL(1) grammar
   - postfix + three-address code for `a + b * c`
   - an invalid-grammar diagnostic (e.g. `undefined non-terminal`)
3. Add the plagiarism/AI similarity report as a separate file here when
   generated (do not commit the tool's full output if the faculty wants a
   PDF; keep the summary only).

## Integrity rules (faculty requirement: similarity < 10%, AI < 20%)

- This draft was generated from the team's actual implementation
  (`PROJECT_SPECIFICATION.txt`, `docs/algorithms.md`). Rewriting sections in
  your own words during submission prep further reduces similarity.
- Do not paste tutorial or repository code into the report; refer to the
  project's own files instead.
- Every claim in the report must match the working program — verify each
  section against a real run before submission.
