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
lmbetter parse FILE           # '-' reads stdin
lmbetter --dsl FILE [FILE ...]
```

`parse` prints the concrete syntax tree as a single-line s-expression, like
`tree-sitter parse` does. Malformed input still prints its tree — broken
regions appear as `(ERROR ...)` nodes — and still exits 0; tree-sitter always
produces a tree. Exit codes: 0 ok, 2 usage error, 3 input/DSL error,
4 stdout failure.

```
$ echo '{"a": [1, null]}' | lmbetter parse -
(document (object (pair key: (string (string_content)) value: (array (number) (null)))))
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
require `lmscript` >= 0.2.442. See `examples/` for runnable scripts.

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
