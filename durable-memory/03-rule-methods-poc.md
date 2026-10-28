# Rule-methods POC (branch poc/rule-methods)

Demonstrates the lmcode rule-methods spike (lmctl-mono-poc branch
`poc/lmcode-rule-methods`) end-to-end against the lmbetter host: analysis
rules as user-redefinable lmscript snippets over the tree-sitter CST, instead
of SonarQube's compiled, per-rule Java classes (see docs/rule-snippets.md).

## Mechanism

- Schema v2 gains `"interfaces"`: `{"Rule": {"params": [{"name": "n", "type":
  "Node"}], "returns": "void"}}` — a SAM interface (one signature, named
  parameters). Interfaces are compiler-facing metadata: no canonical
  signature, no import, no bytecode section; they only join the schema SHA-256
  that binds bytecode to its schema.
- A top-level `fn` may carry `@NAME` (the tree-sitter CST node kind the rule
  subscribes to, compile-time metadata only — nothing dispatches on it yet)
  and `implements Rule` (signature checked exactly against the interface at
  compile time; mismatch is a located error, e.g.
  `function 'wrong_sig' does not implement interface Rule (Node) -> void; found (Node, int) -> void`).
- The host grew CST navigation: `Tree.root() -> Node`; `Node.kind()`,
  `.text()` (source slice; the Tree keeps a source copy so slices stay valid),
  `.is_named()`, `.start_row()/.start_col()/.end_row()/.end_col()` (0-based,
  tree-sitter convention), `.child_count()` / `.child(i)` (NAMED children
  only; `child` is bounds-checked in the registry and throws the usual
  "out of range" run-time error). Host boxes TSNode values into arena Node
  handles (`alignas` byte storage + static_assert fit in host.cpp).
- lmcode has no recursion and there is no rule entry table yet: the example
  walks the CST with fixed-depth nested loops and calls every rule on every
  node; each rule re-checks `n.kind()`. A DSLB v6 entry table (host walks
  once, dispatches by node kind) is the planned next slice.

## Branch-local deltas from main

- `sdk.lock` pins lmcode-cpp `d70a12c` (12 files, 181 std signatures) instead
  of `ee1f084` (72): the POC compiler's implicit std is the newer set, and
  test.sh's registry/compiler signature parity requires the registry to match
  the compiler in use. The std-count assertion in test.sh is 181 here.
- `scripts/select-compiler.sh` admits `lmscript 0.2.38` (the monorepo's own
  version numbering; the functional v5/schema-hash/std probe is the real
  gate). The npm-released 0.2.420 still passes.

## Verification

```sh
./test.sh                      # released compiler: FAILS at check 8
                               # (schema error: unknown top-level key "interfaces")
PATH=/home/ubuntu/lmctl-mono-poc/lmscript/bin:$PATH ./test.sh   # PASS: 22 checks
POC=/home/ubuntu/lmctl-mono-poc/lmscript/bin/lmscript
PATH=$PWD/build:$PATH $POC examples/poc/cst-rules.lms build/poc/cst-rules.dslb
build/lmbetter --dsl build/poc/cst-rules.dslb
# finding: password key "password" at row 4 col 4
# finding: placeholder string "TODO" at row 6 col 9
# {"ok": true, "result": null}
```
