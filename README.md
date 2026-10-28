# lmbetter

tree-sitter's incremental parsing engine as a single statically-linked,
scriptable CLI.

lmbetter wraps [tree-sitter](https://tree-sitter.github.io) 0.27.1 together
with the [tree-sitter-json](https://github.com/tree-sitter/tree-sitter-json)
grammar in one dependency-free executable that can be driven from the command
line or scripted with [lmscript](https://github.com/lmctl-ai). Feed it JSON
and it prints the concrete syntax tree; from lmscript you get a typed `Tree`
object to inspect.

## Install lmctl

Everything in this repository runs through [lmctl](https://lmctl.com), the core tool of the lmctl family. Install it once, then run any `.lms` file or raw URL:

```bash
npm install -g @lmctl-ai/lmctl
lmctl autorun examples/parse-string.lms
lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmbetter/main/examples/parse-string.lms
```

## Build

```sh
scripts/sync-deps.sh   # download the pinned tree-sitter sources (third_party/, git-ignored)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Produces `build/lmbetter`: one statically linked executable (`ldd` reports
"not a dynamic executable" on Linux; macOS links only system libraries).

## CLI

```
lmbetter --version
lmbetter --notices
lmbetter --dsl-schema [--json]
lmbetter --dsl-signatures
lmbetter parse [--lang json|c] FILE          # '-' reads stdin
lmbetter query [--lang json|c] PATTERN FILE  # '-' reads stdin
lmbetter --dsl FILE [FILE ...]
```

`parse` prints the concrete syntax tree as a single-line s-expression, like
`tree-sitter parse` does. Malformed input still prints its tree — broken
regions appear as `(ERROR ...)` nodes — and still exits 0; tree-sitter always
produces a tree. `query` runs a tree-sitter S-expression pattern over the
document and prints one line per captured node, `kind
start_row:start_col-end_row:end_col` (0-based). Both default to the JSON
grammar; `--lang c` switches to C (an unknown language exits 2 with the
known-languages list). Exit codes: 0 ok, 2 usage error, 3 input/DSL error (an
invalid query pattern names its byte offset and reason), 4 stdout failure.

```
$ echo '{"a": [1, null]}' | lmbetter parse -
(document (object (pair key: (string (string_content)) value: (array (number) (null)))))
$ lmbetter query '(pair key: (string (string_content) @key))' examples/data/sample.json
string_content 1:3-1:7
string_content 2:3-2:10
string_content 3:3-3:5
string_content 4:3-4:12
```

The `--dsl` interface runs lmscript bytecode against the same engine with
structured access to parsed trees.

## Scripting

```lmscript
p = _dsl_ cli="lmbetter" install="github:lmctl-ai/lmbetter@latest";
let tree = ts.parse("{\"name\": \"lmbetter\", \"tags\": [\"tree-sitter\", \"json\"]}");
println(tree.sexp());
println("has_error -> " + str(tree.has_error()));
println("nodes -> " + str(tree.node_count()));
end _dsl_
savedsl(p, "build/show.dslb");
```

The DSL surface is deliberately small: `ts.parse(text)` returns a `Tree` with
`.sexp()`, `.has_error()`, `.node_count()`, and `.root()`. The `root()` handle
is a `Node` with `.kind()`, `.text()`, `.is_named()`,
`.start_row()/.start_col()/.end_row()/.end_col()` (all **0-based**,
tree-sitter convention), `.child_count()`, and `.child(i)` (named children
only, bounds-checked). The schema also declares the SAM interface
`Rule(n: Node) -> void`: top-level lmcode functions can carry a `@NODE-KIND`
annotation and an `implements Rule` clause (checked at compile time), so
analysis rules are user-redefinable scripts over the CST — see
`examples/cst-rules.lms` and `durable-memory/03-rule-methods.md`. Rule methods
require `lmscript` >= 0.2.442. Since v0.2.1, `Tree.nodes()` returns a
`NodeList` (flat pre-order list of named nodes, root first; `.len()` and
bounds-checked `.at(i)`), so `for (n in tree.nodes())` runs a whole-tree scan
in one foreach — see `examples/lint-json.lms`. Since v0.2.2, the host keeps a
per-run findings sink: `ts.report(node, message)` appends a finding
{row, col, end_row, end_col, message} (0-based positions, capped at 65536 per
run), read back with `ts.report_count()` and `ts.report_json()` (a JSON array
string with stable key order) — the script decides the rendering, see
`examples/report-json.lms`. Since v0.2.5, `ts.report(node, message, rule,
severity)` also attaches rule metadata: `rule`/`severity` are free-form
strings (policy belongs to the script; the host validates nothing), emitted
by `report_json()` only when present (key order row, col, end_row, end_col,
rule, severity, message) — see `examples/rule-catalog.lms` for per-severity
summaries and a quality gate computed in lmcode, `examples/suppress-findings.lms`
for `// nosonar`-style suppression implemented entirely script-side, and
`examples/sarif-report.lms` for SARIF 2.1.0 emission (GitHub code scanning /
VS Code interop) in pure lmcode. Since v0.2.3, `ts.query(root, pattern)` runs a
tree-sitter S-expression query over the subtree at `root` and returns every
captured node as a `NodeList` (match/byte order; duplicates kept; a pattern
with zero captures yields an empty list; an invalid pattern is a runtime
error naming the byte offset and reason). Query predicates (`#eq?` and
friends) are NOT evaluated — do text checks in lmcode, which is the
user-redefinable part anyway; see `examples/query-json.lms`. Since v0.2.4, a
second grammar is vendored: tree-sitter-c (pinned in `deps.lock`) with
`ts.parse_c(text)` / `ts.query_c(root, pattern)` mirroring the JSON pair — see
`examples/lint-c.lms` for real security rules over C. See `examples/` for
runnable scripts.

## Repository layout

- `cpp/` — the CLI host: DSL VM integration plus the tree-sitter wrapper
- `docs/` — design documents (e.g. `rule-snippets.md`: analysis rules as lmscript snippets)
- `schema/lmbetter.host.json` — the host-only DSL schema (copied to `schema/lmbetter.dsl.json` and embedded into the binary; `lmscript` >= 0.2.442 supplies the built-in standard library implicitly)
- `examples/` — lmscript examples
- `scripts/` — vendoring (tree-sitter sources pinned in `deps.lock`, `lmcode-cpp` SDK pinned in `sdk.lock`) and compiler selection
- `third_party/` — downloaded upstream sources (git-ignored, pinned by `deps.lock`)

## License and company

lmbetter-authored code is MIT licensed (see `LICENSE`). It builds against
tree-sitter and tree-sitter-json, both MIT licensed (`THIRD_PARTY_NOTICES.md`;
also embedded in the binary, see `lmbetter --notices`). Made by
**Lattice Systems LLC**.
