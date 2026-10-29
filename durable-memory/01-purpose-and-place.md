# Purpose and place

lmok repackages tree-sitter as one statically linked, scriptable CLI: the
upstream C library (v0.27.1, pinned in `deps.lock`) plus the tree-sitter-json
(commit `254c42a`), tree-sitter-c (commit `b780e47`), tree-sitter-python
(v0.23.6), tree-sitter-bash (v0.25.1), tree-sitter-go (v0.25.0),
tree-sitter-javascript (v0.25.0) and tree-sitter-typescript (v0.23.2) grammars
(same lock) behind `parse`/`query` subcommands and an lmscript DSL host. Sibling of
lmproxy (Squid) and lmcurl (curl); same house conventions: vendored sources
in git-ignored `third_party/`, per-file SDK hashes in `sdk.lock`, schema
embedded by `cmake/embed_schema.cmake`.

The tree-sitter core is compiled through its amalgamation `lib/src/lib.c`
alone (never together with the individual `lib/src/*.c` files); each grammar
contributes `src/parser.c`, plus `src/scanner.c` for Python and Bash (Go, C and
JSON have no external scanner). Upstream feature-test defines `-D_POSIX_C_SOURCE=200112L
-D_DEFAULT_SOURCE` apply target-wide.

The DSL surface is deliberately minimal: context `ts` (type `Ts`)
with `ts.parse(text) -> Tree`; `Tree.sexp() -> string`, `Tree.has_error() ->
bool`, `Tree.node_count() -> int` (total nodes including the root), plus —
since v0.2.0 — `Tree.root() -> Node`, the `Node` navigation methods and the
`Rule` SAM interface (see [03-rule-methods.md](03-rule-methods.md)); since
v0.2.1 `Tree.nodes() -> NodeList`; since v0.2.2 the `ts.report*` findings
sink; since v0.2.3 `ts.query(root, pattern) -> NodeList` (S-expression
selectors); since v0.2.4 the C grammar via `ts.parse_c` / `ts.query_c` and
`--lang json|c` on the CLI; the python and bash grammars via
`ts.parse_python` / `ts.query_python` / `ts.parse_bash` / `ts.query_bash`
(plus `reparse_python` / `reparse_bash`) and `--lang python|bash` landed
after v0.4.0. Go (v0.25.0), JavaScript (v0.25.0), and TypeScript (v0.23.2) are
CLI grammars for `--lang go` / `--lang javascript` / `--lang typescript` /
`--lang tsx`, `.go` / `.js` / `.ts` / `.tsx` lint/facts inference, and their tier-1 packs. They do not add
`ts.parse_go`/`ts.query_go`, `ts.parse_javascript`/`ts.query_javascript`, or
`ts.parse_typescript`/`ts.query_typescript` or `ts.parse_tsx`/`ts.query_tsx`;
the existing explicit DSL surface stays unchanged for this slice. No cursor API yet.

The lmcode-cpp SDK pin (`sdk.lock`) must match the implicit std set of the
compiler in use. Since v0.2.0 the pin is `d70a12c` (181 standard signatures),
matching npm `lmscript` >= 0.2.442; the previous pin `ee1f084` (72 signatures)
matched <= 0.2.440. Sync with
`LMCODE_CPP=/path/to/lmcode-cpp LMCODE_CPP_REF=<commit> scripts/sync-sdk.sh`.
