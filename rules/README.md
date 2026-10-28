# rules/ — the lmbetter rule library

Three delivery forms, by how much logic a rule needs:

- **Rule packs** (`rules/*.json`) — conditions as data for the `lmbetter lint`
  dispatcher: a tree-sitter `query` pattern or an `on` node kind, an optional
  `text_matches` regex, message, severity. No code. See `docs/rule-tiers.md`
  for the tier framework and `README.md` "Rule packs" for the format.
  Shipped: `c-tier1.json` (10 rules), `json-tier1.json` (5 rules).

- **lmcode rules** (`rules/lmcode/*.lms`) — rules that need LOGIC beyond
  selector+regex: arithmetic (`end_row - start_row`), counting children,
  per-node bookkeeping (StrMap for duplicate keys). Each is an annotated
  `implements Rule` fn over the CST, reporting via `ts.report` with a rule
  key and severity. Shipped: `c-function-metrics.lms` (LBC101 function too
  long, LBC102 too many parameters), `c-switch-style.lms` (LBC103 switch
  without default, LBC104 if without braces), `json-duplicate-keys.lms`
  (LBJ101), `json-size-limits.lms` (LBJ102), `c-nesting-depth.lms` (LBC105)
  and `json-nesting-depth.lms` (LBJ103) — the last two need `Node.parent()`
  (tier-2 extension E2, landed in v0.3.2).

- **Teaching examples** (`examples/*.lms`) — the same techniques in
  narrative order; read these first.

## Using an lmcode rule

The `.lms` files in `rules/lmcode/` are self-contained demos: each reads a
known fixture from `tests/fixtures/` and prints findings. A rule is the
annotated fn — copy it into your own script and point `file_read` at your
file:

```sh
lmscript rules/lmcode/c-function-metrics.lms build/rules-lmcode/c-function-metrics.dslb
lmbetter --dsl build/rules-lmcode/c-function-metrics.dslb
```

(lmscript >= 0.2.442; put `build/` on PATH or let the `install=` line resolve
lmbetter.) `./test.sh` compiles and runs every rule against its violations
fixture (exact findings asserted) and a clean fixture (zero findings
asserted).
