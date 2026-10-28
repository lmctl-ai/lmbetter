# Purpose and place

lmbetter repackages tree-sitter as one statically linked, scriptable CLI: the
upstream C library (v0.27.1, pinned in `deps.lock`) plus the tree-sitter-json
grammar (commit `254c42a`, same lock) behind a `parse FILE` subcommand and an
lmscript DSL host. Sibling of lmproxy (Squid) and lmcurl (curl); same house
conventions: vendored sources in git-ignored `third_party/`, per-file SDK
hashes in `sdk.lock`, schema embedded by `cmake/embed_schema.cmake`.

The tree-sitter core is compiled through its amalgamation `lib/src/lib.c`
alone (never together with the individual `lib/src/*.c` files); the grammar
contributes only `src/parser.c`. Upstream feature-test defines
`-D_POSIX_C_SOURCE=200112L -D_DEFAULT_SOURCE` apply target-wide.

The DSL surface is deliberately minimal: context `ts` (type `Ts`)
with `ts.parse(text) -> Tree`; `Tree.sexp() -> string`, `Tree.has_error() ->
bool`, `Tree.node_count() -> int` (total nodes including the root), plus —
since v0.2.0 — `Tree.root() -> Node`, the `Node` navigation methods and the
`Rule` SAM interface (see [03-rule-methods.md](03-rule-methods.md)).
No query API, no cursor API, no other grammars yet.

The lmcode-cpp SDK pin (`sdk.lock`) must match the implicit std set of the
compiler in use. Since v0.2.0 the pin is `d70a12c` (181 standard signatures),
matching npm `lmscript` >= 0.2.442; the previous pin `ee1f084` (72 signatures)
matched <= 0.2.440. Sync with
`LMCODE_CPP=/path/to/lmcode-cpp LMCODE_CPP_REF=<commit> scripts/sync-sdk.sh`.
