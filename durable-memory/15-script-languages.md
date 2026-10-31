# lmscript and lmcode syntax analysis (S4)

Both grammars are independently pinned to immutable MIT source archives in
`deps.lock`. `scripts/sync-deps.sh` checks archive SHA-256 and extracted-tree
hashes. CMake compiles **both parser.c and scanner.c** for each grammar. Do not
omit scanners: lmscript blocks/command literals and lmcode heredocs depend on
external scanner state. No Tree-sitter generator or Node runtime is needed to
run the installed binary.

`--lang lmscript` selects the outer language; `.lms` inference selects it too.
`--lang lmcode` selects standalone inner language; `.lmcode` is the local
standalone convention. Existing `.lms` rule packs remain outer lmscript files,
not automatically extracted inner programs. parse/query, JSON lint packs,
compiled `check --lang`, and facts use the same registered grammars.

Facts include function definitions/calls, lmscript imports/packages/schema
loads and markers, and lmcode implements edges. These are syntactic candidate
references, not schema/type checking. Grammar limitations and source parser
pins are recorded in the upstream grammars' DIVERGENCES.md and MONO-PIN.

## S5 seam: host-side injection, not a combined grammar

S4 intentionally treats embedded bodies as opaque. The lmscript grammar exposes
named `lmcode_body` ranges for `_lmcode_` and `_dsl_`. S5 can walk these nodes,
parse the corresponding source bytes with Grammar::Lmcode and rebase byte/row/
column spans before emitting query/lint/fact output. Empty bodies may have no
body node. Retain source ownership and handle CRLF/UTF-8 byte offsets. Never
rewrite the input or infer that every `.lms` file is inner lmcode.

Tests: `tests/check-script-languages.py`, plus the native-OS and rule-OS suites,
run from `test.sh`. The scanner build control links each pinned parser with its scanner first,
then omits that scanner and requires language-specific undefined external
scanner link symbols; a generic failed build is not proof.
