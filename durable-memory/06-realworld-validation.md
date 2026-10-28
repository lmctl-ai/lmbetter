# Real-world validation of rules/c-tier1.json (Kimi3)

Validated the C tier-1 pack against three real C projects (shallow clones in
/tmp, linted as directories with `lmbetter lint rules/c-tier1.json <dir>`):

- **sds** (antirez, 1.7k LOC) — string library, modern C99
- **kilo** (antirez, 1.3k LOC) — terminal editor, old-school C
- **cJSON** (DaveGamble, 22.4k lintable LOC, ~15k of it the vendored unity
  test framework; library proper ~5.6k) — JSON parser, camelCase codebase

Total ~25k LOC — over the ~20k suggestion because cJSON vendors unity, but
unity's macro torture tests are exactly where the interesting false
positives lived.

## Findings before → after hardening

| repo | before | after |
| --- | --- | --- |
| sds | 41 | 27 |
| kilo | 63 | 62 |
| cJSON | 1510 | 858 (+15 from new LBC011) |

## TP/FP triage (per rule, all three repos combined)

| rule | before | verdict | action |
| --- | --- | --- | --- |
| LBC001 unsafe calls | 33 | TP (sprintf/strcpy/strcat in cJSON, e.g. version formatting, string duplication) | kept |
| LBC002 FIXME/TODO | 4 | TP (real markers, incl. one mid-block-comment TODO) | kept |
| LBC003 goto | 116 | TP (error-cleanup idiom everywhere; the rule flags all gotos by design) | kept |
| LBC005 assign-in-condition | 0 | no real-world hits; fixture-verified | kept |
| LBC006 K&R definition | 12 | **FP class**: parser error-recovery artifacts. unity's statement-like macros used without semicolons (`EXPECT_ABORT_BEGIN` / `VERIFY_FAILS_END`) make tree-sitter recover a fake `function_definition` (macro name as return type, the next macro call as declarator, swallowed declarations as direct children) that structurally mimics K&R. No query-level distinguisher: the artifact has type, declarator, declaration children and body in the right order. | kept, documented in rules/README.md (genuine K&R is near-nonexistent in modern code; 12 FPs in one vendored torture-test file is acceptable) |
| LBC007 object-like macro | 159 | **FP class**: value-less defines — include guards (`#define __SDS_H`) and feature macros (`#define _CRT_SECURE_NO_DEPRECATE`) — can never be const/enum | tightened: `text_matches` now requires a replacement value (`^#define[ \t]+name[ \t]+\S`). 159→122. Residual noise: value-having feature-test macros (`_POSIX_C_SOURCE 200809L`); documented |
| LBC008 register | 0 | no real-world hits; fixture-verified | kept |
| LBC009 snake_case fn | 1279 | convention rule — camelCase codebases (kilo, cJSON) fire a lot BY DESIGN (TP by the rule's definition). **FP/noise classes fixed**: (a) every function reported twice, once at the prototype in the header and once at the definition; (b) error-recovery artifacts fired on macro names (`TEST_ASSERT…` as fake declarators, same nodes as the LBC006 FPs) | tightened: query now matches `function_definition` declarators only (two patterns, incl. pointer-returning `type *f(...)`), plus `#not-match? @name "^[A-Z_][A-Z0-9_]*$"` (ALL-CAPS = macro-style convention). 1279→725, one finding per function |
| LBC010 commented-out code | 11 | **FP class**: 8 of 11 were doc/prose comments — sds doc blocks with usage examples (`mystring = sdsnewlen("abc",3);` mid-comment), prose containing `=…;` | tightened: same `(=…;)\|(;\s*$)` code pattern but anchored to the comment's FIRST line. Killed all 8 FPs, kept all 3 genuine commented-out-code TPs (incl. `/* CHECK(m1 != m2); …*/`). Trade-off: `/*`-blocks whose code starts on line 2+ are missed; documented |
| (removed) implicit-int | 0 | **dead rule removed**: the pack carried a SECOND rule with id `LBC007` — query `(function_definition !type) @f`, "function definition without return type". durable-memory/05 documented this rule as dropped (tree-sitter-c always yields a type, type-less K&R produces ERROR nodes); it fires on nothing, ever. Removed from the pack. | removed |
| LBC011 malloc-cast (NEW) | 15 | TP by construction (`cast_expression` wrapping a malloc/calloc/realloc call): cJSON tests/fuzzing cast malloc results in 15 places. Was a documented tier-1 candidate; real code proved it fires | added, fixture-extended |

## Pack changes (before → after)

- **LBC007**: `on: preproc_def` (no filter) → + `text_matches:
  "^#define[ \\t]+[A-Za-z_][A-Za-z0-9_]*[ \\t]+\\S"` (replacement value
  required).
- **LBC009**: `(function_declarator declarator: (identifier) @name)` +
  `[A-Z]` → two definition-only patterns (plain + pointer-returning
  declarator), each with `(#not-match? @name "^[A-Z_][A-Z0-9_]*$")`, same
  `text_matches: "[A-Z]"`.
- **LBC010**: `text_matches: "(=.*;)|(;[ \\t]*$)"` (searched the whole
  comment, multi-line) → `"^(//|/\\*)(([^\\n]*=[^\\n]*;)|([^\\n]*;[ \\t]*$))"`
  (first line of the comment only). First iteration `[=;{}]` on the first
  line was tried and REJECTED — prose contains `=>`, `type==X`, `{`
  constantly (cJSON LBC010 went 5→159); the tightened form must keep the
  original code-ish shape, only re-anchored.
- **LBC011 added**: `(cast_expression value: (call_expression function:
  (identifier) @fn) (#match? @fn "^(malloc|calloc|realloc)$"))`, severity
  minor, CERT MEM02-C.
- **Dead duplicate-id LBC007 "implicit int" query rule removed.**

## Fixtures / tests

- `tests/fixtures/c-tier1-violations.c`: appended `alloc_cast()`
  (malloc-cast) so LBC011 fires; expected bytes in test.sh re-derived from
  a real run (13 → 14 findings; all pre-existing findings byte-identical —
  no rule tightening changed the fixture output).
- `tests/fixtures/c-tier1-clean.c`: added uncast `malloc`/`free` (LBC011
  clean-side guard); still 0 findings.
- `./test.sh`: PASS, all checks green.
