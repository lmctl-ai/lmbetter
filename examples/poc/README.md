# POC examples

Proof-of-concept scripts that need the POC compiler (branch
`poc/lmcode-rule-methods` of lmctl-mono-poc), NOT the released npm compiler —
the released 0.2.420 rejects the schema's `"interfaces"` key. They are not
wired into `../test.sh` for that reason.

```sh
PATH=build:$PATH /path/to/poc/lmscript examples/poc/cst-rules.lms build/poc/cst-rules.dslb
build/lmbetter --dsl build/poc/cst-rules.dslb
```

- `cst-rules.lms` — two SonarQube-style rules (`@PAIR fn no_password_key`,
  `@STRING fn no_placeholder`, both `implements Rule`) run over the JSON CST
  via `Tree.root()` and the `Node` navigation methods.

## Limitations of this slice

- **No recursion in lmcode**, so `cst-rules.lms` walks the tree with
  fixed-depth nested loops (root + 5 levels). A generic deep walk needs either
  DSL object-lists (an explicit node stack) or the future host-dispatch entry
  table.
- **Annotations are compile-time metadata only**: `@PAIR` records the intended
  CST node-kind subscription but nothing dispatches on it. The chunk body must
  call every rule on every visited node, and each rule re-checks `n.kind()`
  itself. A DSLB v6 rule entry table (annotation + interface per rule, host
  walks once and dispatches by node kind) is a later slice.
- Node rows/columns are **0-based** (tree-sitter convention).
