# Examples

Runnable lmscript programs, one per scenario. Each compiles to bytecode and
runs against the `lmbetter` host:

```sh
lmscript examples/parse-string.lms build/parse-string.dslb
lmbetter --dsl build/parse-string.dslb
```

The `install="github:lmctl-ai/lmbetter@latest"` line in each script pins the
host CLI the same way lmscript resolves any other tool.

- `parse-string.lms` — parse an inline JSON string, print the tree
- `parse-file.lms` — read `examples/data/sample.json`, print tree stats
- `detect-error.lms` — parse malformed JSON, flag the ERROR node
- `cst-rules.lms` — two `@`-annotated rules `implements Rule` over the JSON CST
  (`Tree.root()` + `Node` navigation with manual fixed-depth loops)
- `lint-json.lms` — a tiny JSON linter: three annotated rules run over
  `Tree.nodes()` (flat pre-order named-node list) with one foreach — the
  idiomatic way; no manual walk
- `report-json.lms` — the same linter on the host findings sink: each rule is
  just `ts.report(n, "...")`; the script prints `ts.report_json()` and a
  summary count. No predicate helpers, no manual counting
- `query-json.lms` — selector-based rules: `ts.query(root, pattern)` picks
  candidate nodes with a tree-sitter S-expression pattern (`@key` captures),
  the rule text-checks in lmcode and reports
- `lint-c.lms` — the same over real C code (tree-sitter-c grammar):
  `ts.parse_c` + `ts.query_c`, flagging `gets`/`strcpy`/`sprintf` calls and
  FIXME comments, with rule keys + severities (`LBC001` critical,
  `LBC002` minor)
- `rule-catalog.lms` — user-side policy: parse `ts.report_json()` back with
  the std json functions, summarize by severity, and apply a quality gate
  (`critical > 0` → `gate=FAIL`) as a plain script-side `if`

`cst-rules.lms` shows manual `Node.child(i)` navigation (fixed-depth nested
loops, because lmcode has no recursion); `lint-json.lms` shows the `NodeList`
idiom that replaces it for whole-tree scans; `report-json.lms` shows the
`ts.report` sink that replaces the predicate-helper/counting idiom from
`lint-json.lms`; `query-json.lms` shows S-expression selectors replacing
structural navigation. Rule annotations (`@PAIR`, `@STRING`, `@NUMBER`) are
compile-time metadata — nothing dispatches on them yet, so every rule runs on
every node and re-checks `n.kind()` itself (or trusts the query's selection).
Node rows/columns are 0-based (tree-sitter convention); `ts.report` findings
carry row/col/end_row/end_col plus the message, and the 4-arg overload adds
free-form `rule`/`severity` strings (emitted by `report_json` only when
present; the host validates nothing — policy is script-side). `ts.query`
returns captured nodes in match/byte order (duplicates across
captures/matches kept) and does NOT evaluate query predicates (`#eq?` and
friends) — text checks live in lmcode.

`./test.sh` compiles and runs every example as part of the check suite.
