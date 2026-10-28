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
- `rules/json-tier1.json` — 11 rules (LBJ001–LBJ010 plus LBJ104): credential
  keys, placeholder strings, null values, snake_case keys, numbers-as-strings,
  exact sensitive credential keys, HTTP URL values, unpinned versions, empty
  containers, long source strings, and exact password key.
- Guard fixtures: `tests/fixtures/{c,json}-tier1-{violations,clean}.*` —
  every rule fires exactly where expected on the violations fixture
  (byte-exact output pinned in test.sh), and the clean fixture must produce
  `0 findings` (false-positive guard).

## lmcode-native tier-1 rules (v0.3.1)

Rules that need LOGIC beyond selector+regex ship as annotated `implements
Rule` fns in `rules/lmcode/*.lms` (see `rules/README.md`): LBC101 function
too long (row arithmetic), LBC102 too many parameters (child counting),
LBC103 switch without default (default = `case_statement` with text prefix
"default" — lmcode can't query `!value`, so the keyword text is the check),
LBC104 if without braces (consequence child kind), LBJ101 duplicate object
keys (per-object StrMap), LBJ102 object too large. Each file reads its
violations fixture via `file_read`; test.sh sed-substitutes the clean-fixture
path (and the savedsl target) into a scratch copy and asserts zero findings.
Nothing turned out non-tier-1 in this batch — all four rule families fit
current lmcode.

## E5 landed: incremental re-parse (v0.4.0)

`Tree.edit(start, old_end, new_end)` (byte offsets only) RECORDS the edit —
it is applied inside `ts.reparse`/`ts.reparse_c`, because ts_tree_edit needs
row/col points and the new-end point exists only once the new text is known
(start/old_end points compute from the old source, new_end from the new).
One pending edit per reparse (a second edit() first is a run-time error, as
are out-of-range offsets). Reparse checks the contract: with no pending edit
the new text must equal the old source byte-for-byte (otherwise tree-sitter
reuses every old subtree and silently returns the OLD tree — observed:
`reparse(parse("{}"), "[1, 2, 3]")` gave `(document (object))`), and with
an edit the new length must equal `old - (old_end - start) + (new_end -
start)`; both violations are exit-3 errors. The old tree stays alive after reparse —
`ts.changed_ranges(old, new) -> string` (JSON `[[start,end],...]`) works
anytime; cost is two trees in the arena, and the old tree's stored source is
pre-edit so old-node text() past the edit point is stale (use the new tree
for reads). Demo: `examples/incremental-reparse.lms` (edit → reparse →
changed ranges [[55,65]] = the inserted statement's syntactic extent →
re-lint finds the inserted `gets` call).

## E4 landed: directory lint + script-side cross-file (v0.3.5)

`lmbetter lint` takes directories: `std::filesystem::recursive_directory_iterator`
(C++17, MinGW-safe — no POSIX-only calls; `starts_with` is C++20 so the hidden
check uses `rfind(".", 0) == 0`, with `.`/`..` exempted so `lint pack .`
works), expansion filtered to `.c`/`.h`/`.json`, hidden paths skipped, paths
sorted for byte-stable output, lang-less files skipped silently, empty
expansion exit 2, fail-fast unreadable exit 3 unchanged. Output paths are
canonicalized to forward slashes (`fs::path::generic_string`) on every OS —
both directory-expanded and explicitly-passed args — so pinned outputs are
platform-independent (Windows CI produced `lintdir\a.c` before this). Cross-file rule
LBC110 (`rules/lmcode/c-cross-file.lms`) unions defined/called sets over two
files in pure lmcode — no host multi-file magic. Include-graph resolution and
header/source pairing stay out of scope (tier 3). Fixtures:
`tests/fixtures/lintdir/` (2 C files + skipped .txt/.json + hidden dir) and
`tests/fixtures/cross/{a,b,clean-a,clean-b}.c`; the rules/lmcode clean-variant
sed chain gained `cross/a.c → cross/clean-a.c` substitutions.

## E3 reality check: symbol rules need no host index (v0.3.4)

Hypothesis confirmed: LBC107 (unused static function), LBC108 (undefined
call, same-file), LBC109 (direct recursion) are all expressible in pure
lmcode — `rules/lmcode/c-symbol-rules.lms` does it with two queries + StrMap
set algebra (defined/called name sets) and a per-definition subtree query for
recursion (`ts.query_c` accepts any Node as root). E3 re-scoped in
docs/rule-tiers.md to performance/cross-file only. Honest edges: LBC107 flags
static functions only (non-statics may be called cross-TU; function pointers
and callbacks are invisible to identifier queries); LBC108 is same-file plus
a script-owned libc whitelist string; LBC107/LBC108 are set-level, so they
live in the chunk body, not Rule fns (the single-node SAM again — the v6
design input list grows).

## E1 landed: query predicates (v0.3.3)

`node_query` evaluates `#eq?`/`#not-eq?` (capture text vs literal/capture)
and `#match?`/`#not-match?` (ECMAScript regex) per match; a failing match
contributes no captures. Semantics follow upstream's
`satisfies_text_predicates` (lib/binding_rust): text equality for eq, and
**unanchored** regex for match (`regex::is_match`) — the "anchored"
convention lives in query AUTHORS writing `^...$`, not in the engine; the
task brief assumed engine-side anchoring, the upstream evidence
(docs examples + binding source) says otherwise, and compatibility with
published queries wins. Unknown operators (`#set!`, `#is?`, `any-of?`,
`any-*`) were initially ignored (over-matching risk for `any-*` noted as
follow-up); the v0.4.0 review fixed that: `#any-eq?`/`#any-not-eq?`/
`#any-match?`/`#any-not-match?`/`#any-of?`/`#not-any-of?` are evaluated
(any-* = at least one node, plain = every node — the semantics of upstream's
web binding `every`/`some`; the Rust binding's loop has a trailing `true`
that lets any-* pass vacuously, which we do NOT copy), `#is?`/`#is-not?` and
`!` directives are accepted and ignored, and malformed or unknown `?`
predicates are refused with exit 3 (an ignored predicate over-matches
silently). Regex text is bounded at 16 KiB (see 04-dispatcher.md). C-API
quirk: predicate steps carry no operator
field — the operator name is the first String step of each predicate.
Caveat: the `Rule` SAM is single-node, so multi-capture rules (LBC106's
fn+fmt pairing) cannot `implements Rule` — a v6 entry-table design note.

Rules: LBC001/LBC004 moved their callee regexes into the query as `#match?`
(same findings, sharper form); new LBJ104 (exact `password` key via `#eq?` —
`password_hint` was added to the violations fixture, where broad LBJ001 fires
and exact LBJ104 does not; it could NOT go in the clean fixture because
LBJ001's intentional substring match would fire there); new LBC106
(scanf-family without field width; `#match?` selects the callee, lmcode does
the width heuristic via the flat capture-pair zip).

## E2 landed: Node.parent() (v0.3.2)

Contract: `Node.has_parent() -> bool` (false at root) + `Node.parent() ->
Node` which throws a located run-time error on the root (exit 3) — no fake
self-node, so `while (n.has_parent()) { n = n.parent(); }` is the idiom.
Parents reuse the existing Node arena (same tree, same lifetime). Unlocked
rules: LBC105 control nesting depth >4 (`c-nesting-depth.lms`) and LBJ103
JSON value nesting >6 (`json-nesting-depth.lms`) — both walk the parent chain
counting kind-matched ancestors; only the deepest offender fires by
construction. The LBC103 innermost-switch caveat from v0.3.1 turned out to be
a non-issue: the rule walks DIRECT children of each switch body, which is
already correct association (an inner switch's default is not a direct child
of the outer body), so no parent() fix was needed.

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

LANDED: E1 query predicates (v0.3.3), E2 Node.parent() (v0.3.2), E4
multi-file/directory lint (v0.3.5, minus include resolution), E5 incremental
re-parse (v0.4.0). Re-scoped: E3 same-file symbol index (performance/
cross-file only — the symbol-ish rules turned out to be scriptable, see "E3
reality check" above). Tier 2 is COMPLETE. Tier 3 (types, CFG, dataflow,
taint) stays deferred.

## JSON expansion

The previous inventory said five JSON rules but omitted the already-shipped
LBJ104: the pack now contains eleven. New LBJ006–LBJ010 use verified vendored
JSON node kinds (`pair`, `string`, `array`, `object`). Whole-string matching
avoids fragmentation at escape sequences. LBJ010 fits a pack regex and counts
>200 source bytes between quotes, not decoded characters. Fixtures include the
200/201 boundary and escaped strings. LBJ008 intentionally treats any pair value
`*`/`latest` as unpinned; this is a configuration heuristic, not schema inference.

`json-duplicate-values.lms` adds LBJ105 with an annotated `implements Rule`
function and a local StrMap per object. Reports each repeated string value after
the first; nested scopes are independent, array values and non-strings excluded.
Equality is source spelling (no JSON unescaping), and repeated values may be
intentional. Its violations/clean fixtures use the existing test.sh sed path
substitution; the explicit rule-file loop now includes ten files.
