# Rule tiers (v0.3.0)

The owner-set three-tier strategy for rules — lmbetter stays a simple CLI, no
platform. Framework, inventory, and reference keys: `docs/rule-tiers.md`.
Membership test: "what information does the rule need?" — CST shape + text
(tier 1, ship now as packs), CST + one named mechanical extension (tier 2,
plan then build), real semantics (tier 3, defer).

## Shipped (tier 1)

- `rules/c-tier1.json` — 10 rules (LBC001–LBC010): unsafe calls, FIXME/TODO
  comments, goto, command exec, assignment in if/while condition (two
  patterns in one query string — multi-pattern queries work fine), K&R
  definitions (`((function_definition (declaration)) @kr)` — the extra
  grouping parens make the capture land once on the outer node), object-like
  macros, register, snake_case function names, commented-out code.
- `rules/json-tier1.json` — 5 rules (LBJ001–LBJ005): credential keys,
  placeholder strings, null values, snake_case keys, numbers-as-strings.
- Guard fixtures: `tests/fixtures/{c,json}-tier1-{violations,clean}.*` —
  every rule fires exactly where expected on the violations fixture
  (byte-exact output pinned in test.sh), and the clean fixture must produce
  `0 findings` (false-positive guard).

## Dropped candidates (and why)

- **Implicit-int function definitions** (`!type` query): not expressible.
  tree-sitter-c parses `foo(void) { ... }` as `type: (type_identifier)` +
  `parenthesized_declarator`, and type-less K&R (`foo(a) int a; { ... }`)
  produces an ERROR node — the grammar never yields a type-less
  `function_definition`. Would need E3-style analysis or an upstream grammar
  change.
- **Magic-number array sizes**: any numeric `[]` size is idiomatic C; every
  threshold heuristic false-positives on clean code. Dropped per the
  clean-fixture rule.

## Tier-2 extension plan

E1 query predicates (#eq?/#match?) host-side; E2 Node.parent(); E3 same-file
symbol index; E4 multi-file/directory lint; E5 incremental re-parse. Rules
each unlocks: docs/rule-tiers.md. Tier 3 (types, CFG, dataflow, taint) stays
deferred.
