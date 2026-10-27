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

The DSL surface is deliberately minimal (v0.1.0): context `ts` (type `Ts`)
with `ts.parse(text) -> Tree`; `Tree.sexp() -> string`, `Tree.has_error() ->
bool`, `Tree.node_count() -> int` (total nodes including the root). No query
API, no cursor API, no other grammars yet.

The lmcode-cpp SDK is pinned to commit `ee1f084` (same as lmproxy), not HEAD:
that commit's 72 standard signatures are exactly the set `lmscript >= 0.2.420`
supplies implicitly, which keeps the registry/compiler signature parity checks
in `test.sh` green. Sync with
`LMCODE_CPP=/path/to/lmcode-cpp LMCODE_CPP_REF=ee1f084... scripts/sync-sdk.sh`.
