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

`cst-rules.lms` shows manual `Node.child(i)` navigation (fixed-depth nested
loops, because lmcode has no recursion); `lint-json.lms` shows the `NodeList`
idiom that replaces it for whole-tree scans. Rule annotations (`@PAIR`,
`@STRING`, `@NUMBER`) are compile-time metadata — nothing dispatches on them
yet, so every rule runs on every node and re-checks `n.kind()` via its
predicate helper. Node rows/columns are 0-based (tree-sitter convention).

`./test.sh` compiles and runs every example as part of the check suite.
