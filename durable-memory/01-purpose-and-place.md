# Purpose and place

lmok repackages tree-sitter as one statically linked, scriptable CLI: the
upstream C library (v0.27.1, pinned in `deps.lock`) plus the tree-sitter-json
(commit `254c42a`) and tree-sitter-c (commit `b780e47`, master) grammars (same
lock) behind `parse`/`query` subcommands and an lmscript DSL host. Sibling of
lmproxy (Squid) and lmcurl (curl); same house conventions: vendored sources
in git-ignored `third_party/`, per-file SDK hashes in `sdk.lock`, schema
embedded by `cmake/embed_schema.cmake`.

The tree-sitter core is compiled through its amalgamation `lib/src/lib.c`
alone (never together with the individual `lib/src/*.c` files); each grammar
contributes only `src/parser.c` (neither pinned grammar has an external
scanner). Upstream feature-test defines `-D_POSIX_C_SOURCE=200112L
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
after v0.4.0 (first scanner grammars — `src/scanner.c` in CMake). The
per-grammar
surface is explicit on purpose:
adding a FIFTH grammar should trigger generalizing to a language parameter
instead of another parse_x/query_x pair. No cursor API yet.

The lmcode-cpp SDK pin (`sdk.lock`) must match the implicit std set of the
compiler in use. Since v0.2.0 the pin is `d70a12c` (181 standard signatures),
matching npm `lmscript` >= 0.2.442; the previous pin `ee1f084` (72 signatures)
matched <= 0.2.440. Sync with
`LMCODE_CPP=/path/to/lmcode-cpp LMCODE_CPP_REF=<commit> scripts/sync-sdk.sh`.
