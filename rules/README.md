# rules/ — the lmbetter rule library

Three delivery forms, by how much logic a rule needs:

- **Rule packs** (`rules/*.json`) — conditions as data for the `lmbetter lint`
  dispatcher: a tree-sitter `query` pattern or an `on` node kind, an optional
  `text_matches` regex, message, severity. No code. See [rule tiers](../docs/rule-tiers.md)
  for the framework and [Rule packs](../README.md#rule-packs) for the format.
  Shipped: `c-tier1.json` (10 rules), `json-tier1.json` (6 rules).

- **lmcode rules** (`rules/lmcode/*.lms`) — rules that need LOGIC beyond
  selector+regex: arithmetic (`end_row - start_row`), counting children,
  per-node bookkeeping (StrMap for duplicate keys). Most use annotated
  `implements Rule` functions over the CST, reporting via `ts.report` with a
  rule key and severity; multi-node and set-based checks use ordinary
  functions or chunk-level logic. Shipped: `c-function-metrics.lms` (LBC101 function too
  long, LBC102 too many parameters), `c-switch-style.lms` (LBC103 switch
  without default, LBC104 if without braces), `json-duplicate-keys.lms`
  (LBJ101), `json-size-limits.lms` (LBJ102), `c-nesting-depth.lms` (LBC105)
  and `json-nesting-depth.lms` (LBJ103) — the last two need `Node.parent()`
  (tier-2 extension E2, landed in v0.3.2) — plus `c-scanf-width.lms` (LBC106,
  needs E1 query predicates) and `c-symbol-rules.lms` (LBC107 unused static
  function, LBC108 undefined call, LBC109 direct recursion — set algebra in
  lmcode, no host index needed), and `c-cross-file.lms` (LBC110 calls to
  functions defined in none of the input files).

- **Teaching examples** (`examples/*.lms`) — the same techniques in
  narrative order; read these first.

## Using an lmcode rule

The `.lms` files in `rules/lmcode/` are self-contained demos: each reads a
known fixture from `tests/fixtures/` and prints findings. Copy the rule
functions and their dispatch or set logic into your own script, then point
`file_read` at your input files.

First follow the [build and setup](../README.md#build-and-run). Run these
commands from the repository root; the fixtures are local files, not fetched
with a raw script URL:

```sh
export PATH="$PWD/build:$PATH"
mkdir -p build/rules-lmcode
lmscript rules/lmcode/c-function-metrics.lms
build/lmbetter --dsl build/rules-lmcode/c-function-metrics.dslb
```

Expect a findings array containing LBC101 and LBC102, `2 findings`, then
`{"ok": true, "result": null}`. The directory creation is required:
`savedsl` does not create parent directories. Its path in the script chooses
the bytecode filename; passing another argument to `lmscript` does not
change it.

Run all nine rule demos:

```sh
for source in rules/lmcode/*.lms; do
  lmscript "$source" || exit 1
  build/lmbetter --dsl "build/rules-lmcode/$(basename "$source" .lms).dslb" || exit 1
done
```

Expected finding counts: function metrics 2, switch style 2, duplicate keys
2, size limits 1, C nesting 1, JSON nesting 1, scanf width 1, same-file
symbols 3, cross-file calls 1. Exact findings appear in each script's
`# expect:` / `#|` comments. Findings do not make the process fail.

For the data-only packs, no lmscript compilation is needed:

```sh
build/lmbetter lint rules/c-tier1.json tests/fixtures/c-tier1-clean.c
build/lmbetter lint rules/json-tier1.json tests/fixtures/json-tier1-clean.json
```

Each prints `0 findings in <fixture path>`. The JSON pack includes LBJ104
(exact password key) as well as LBJ001–LBJ005; both credential-key rules can
report the same `password` key.

`./test.sh` compiles and runs every rule against its violations fixture
(expected finding lines asserted) and a clean fixture (`[]` and `0 findings`
asserted). No separate SDK checkout is required.
