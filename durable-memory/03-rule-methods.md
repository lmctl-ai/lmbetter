# Rule methods (R47/R48)

lmbetter exposes the tree-sitter CST to lmscript so analysis rules are
user-redefinable scripts, not compiled per-rule classes (the SonarQube
contrast is studied in docs/rule-snippets.md). Shipped in v0.2.0; needs
`lmscript` >= 0.2.442 (first npm release containing the R47/R48 compiler
feature — the exact number is a placeholder pending the staging publish, see
the TODO in scripts/select-compiler.sh).

## Mechanism

- Schema v2 carries `"interfaces"`: `{"Rule": {"params": [{"name": "n",
  "type": "Node"}], "returns": "void"}}` — a SAM interface (one signature,
  named parameters). Interfaces are compiler-facing metadata: no canonical
  signature, no import, no bytecode section; they only join the schema SHA-256
  that binds bytecode to its schema, so adding one invalidates previously
  compiled bytecode.
- A top-level `fn` may carry `@NAME` (the tree-sitter CST node kind the rule
  subscribes to; compile-time metadata only — nothing dispatches on it yet)
  and `implements Rule` (signature checked exactly against the interface at
  compile time; a mismatch is a located error, e.g.
  `function 'wrong_sig' does not implement interface Rule (Node) -> void; found (Node, int) -> void`).
- Host-side CST navigation: `Tree.root() -> Node`; `Node.kind()`, `.text()`
  (source slice; the Tree keeps a source copy so slices stay valid),
  `.is_named()`, `.start_row()/.start_col()/.end_row()/.end_col()` (0-based,
  tree-sitter convention), `.child_count()` / `.child(i)` (NAMED children
  only; `child` is bounds-checked in the registry and throws the usual
  "out of range" run-time error, exit 3). Host boxes TSNode values into arena
  Node handles (`alignas` byte storage + static_assert fit in host.cpp).
- Demo: `examples/cst-rules.lms` — two rules (`@PAIR no_password_key`,
  `@STRING no_placeholder`) run over a nested JSON fixture; wired into
  `./test.sh` like the other examples.

## Known limitations / follow-ups

- **No dispatch on annotations** (by design in this slice): rules self-filter
  with `n.kind()` and the walk calls every rule on every node — O(rules ×
  nodes). The stated future direction is a DSLB v6 rule entry table
  (annotation + interface per rule; the host walks once and dispatches by
  node kind).
- **No recursion in lmcode**: `examples/cst-rules.lms` walks the CST with
  fixed-depth nested loops. LANDED in v0.2.1: `Tree.nodes() -> NodeList`
  (flat pre-order list of NAMED nodes, root first; `len()` + bounds-checked
  `at(i)`, which is exactly the shape R36 foreach needs) replaces the manual
  walk for whole-tree scans — see `examples/lint-json.lms`. It is flattened
  into the host arena on every call (O(tree) time and memory per call; fine
  at this scale, cache it in a local if looping). Remaining reason for the v6
  entry table is dispatch cost/ergonomics, not reach.
- **Compiler/SDK coupling**: the vendored lmcode-cpp pin (`sdk.lock`) must
  exactly match the implicit std set of the compiler in use, or test.sh's
  registry/compiler signature parity fails. Current pin `d70a12c` (181 std
  signatures) matches npm `@lmctl-ai/lmctl` >= 0.2.442; the older `ee1f084`
  (72 signatures) matched <= 0.2.440.
- **Compiler version numbering**: the lmctl-mono build reports its own version
  (e.g. 0.2.38) unrelated to npm release numbering; the numeric floor in
  select-compiler.sh targets npm numbering, with the functional probe (DSLB
  v5, schema-hash binding, implicit std) as the real gate.
- **`savedsl` does not create parent directories** — compiling into a fresh
  subdirectory needs a `mkdir -p` first.

## Verification

```sh
./test.sh   # needs lmscript >= 0.2.442 on PATH; released 0.2.420 fails at
            # check 8: schema error "unknown top-level key interfaces"
lmscript examples/cst-rules.lms build/cst-rules.dslb
build/lmbetter --dsl build/cst-rules.dslb
# finding: password key "password" at row 4 col 4
# finding: placeholder string "TODO" at row 6 col 9
# {"ok": true, "result": null}
```
