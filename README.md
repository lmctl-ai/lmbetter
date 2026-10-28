# lmok

tree-sitter's incremental parsing engine as a single statically-linked,
scriptable CLI.

lmok wraps [tree-sitter](https://tree-sitter.github.io) 0.27.1 together
with the [tree-sitter-json](https://github.com/tree-sitter/tree-sitter-json),
[tree-sitter-c](https://github.com/tree-sitter/tree-sitter-c),
[tree-sitter-python](https://github.com/tree-sitter/tree-sitter-python) and
[tree-sitter-bash](https://github.com/tree-sitter/tree-sitter-bash) grammars in one dependency-free executable that can be driven from the command
line or scripted with [lmscript](https://github.com/lmctl-ai). Feed it source text
and it prints the concrete syntax tree; from lmscript you get a typed `Tree`
object to inspect.

## Build and run

The standalone CLI needs no Node runtime. Scripting and the test suite use
[lmctl](https://lmctl.com), which supplies the `lmscript` command.

Prerequisites for the commands below (Bash on Linux or macOS):

- Git, curl, tar, and SHA-256 tools (`sha256sum` or macOS `shasum`); internet
  access for the pinned source archives and npm package.
- CMake >= 3.24, Ninja, and C/C++ compilers with C++17 support (GCC or Clang).
  Linux builds also need static C/C++ runtime libraries; on Ubuntu these
  come with `build-essential`. On macOS install Xcode Command Line Tools.
- Node.js >= 24.15.0 and npm for `@lmctl-ai/lmctl@0.2.444`.
- Python 3 for `./test.sh`, plus the usual Unix shell utilities. The SDK
  headers are already checked in; no sibling `lmcode-cpp` checkout is needed.

On Ubuntu, `sudo apt-get install build-essential cmake ninja-build git curl
python3` installs the build tools (check CMake's version). Install Node
separately if your distribution's package is older than 24.15.0. On macOS,
install CMake and Ninja as well as the command-line compiler tools.

```sh
git clone https://github.com/lmctl-ai/lmok.git
cd lmok
npm install --prefix /tmp/lmok-tools @lmctl-ai/lmctl@0.2.444
export PATH="/tmp/lmok-tools/node_modules/.bin:$PATH"
lmscript --version   # lmscript 0.2.444
scripts/sync-deps.sh   # pinned tree-sitter sources (third_party/, git-ignored)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
export PATH="$PWD/build:$PATH"
./test.sh
```

Run all repository examples from this directory, in the same shell. The
absolute PATH entry lets lmscript find the locally built host's schema and
makes the `lmok` commands below work. If npm's commands are not found,
add its global executable directory to PATH too.

Produces `build/lmok`: one statically linked executable (`ldd` reports
"not a dynamic executable" on Linux; macOS links only system libraries).
The test suite rebuilds the host, checks dependency pins, and runs examples
and rule fixtures, including native OS byte/path/linkage checks on every matrix runner.

```sh
lmscript examples/parse-string.lms
build/lmok --dsl build/parse-string.dslb
```

The first command saves bytecode; the second prints the JSON text, its tree,
`has_error -> false`, `nodes -> 34`, then the host's success record
`{"ok": true, "result": null}`. See [all examples](examples/README.md).

## Published host and raw URLs

Every push to `main` builds and tests the five platform binaries. After they pass,
CI compares committed HEAD with the latest `v*` tag and creates a release only if
binary inputs changed: `cpp/`, `rules/`, `schema/`, `CMakeLists.txt`, `cmake/`,
`deps.lock`, `sdk.lock`, embedded notices, or the build/stamp shell scripts.
Tests/docs/CI-only changes since that tag log `SKIP release`; the rolling `dist`
branch still updates. Eligible changes publish a GitHub Release (next patch after
the latest `v*` tag, or the `VERSION` in `CMakeLists.txt` when that is higher;
`lmok --version` in the release prints that tag) and updates the rolling `dist`
branch. Pushing a `v*` tag by hand publishes that tag the same way.
`lmscript install github:lmctl-ai/lmok` fetches the newest release.

To use a published binary without building C++, install Node/npm and lmctl
as above, then:

```sh
lmscript install github:lmctl-ai/lmok@latest
```

The install command resolves a GitHub release, checks its host version, and
prints the executable path and checksum. It does not add `lmok` to your PATH.
The repository rename alone does not rename existing release assets: maintainers
must publish a matching release containing `lmok-linux-x64`, `lmok-linux-arm64`,
`lmok-macos-x64`, `lmok-macos-arm64` and `lmok-windows-x64.exe` plus checksums
before this renamed `@latest` path works. No release is created by this change.
The rolling `dist` branch is separate; `@latest` selects a release, not `dist`.
Use an explicit `@TAG` for a reproducible dependency once that release exists.
Local builds on PATH work now without downloading a host.

From the repository root (or an empty directory for this inline-data example):

```sh
mkdir -p build
lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmok/main/examples/parse-string.lms
lmok --dsl build/parse-string.dslb
```

With a local build, use `build/lmok` for the second command. The local
form `lmctl autorun examples/parse-string.lms` works too. `autorun` runs the
outer script, whose `savedsl` call writes `build/parse-string.dslb`; it does
not execute that bytecode or print the tree. The `install=` declaration lets
lmscript resolve a host when one is not on PATH. `@latest` is not a fixed
version pin, and initial resolution needs network access.

Raw URLs fetch the script, not a repository checkout or its data files.
Examples reading `examples/data/` or `tests/fixtures/` still need the clone
and repository-root working directory. The examples' output directories
must exist before compilation.

## CLI

```
lmok --version
lmok --notices
lmok --dsl-schema [--json]
lmok --dsl-signatures
lmok parse [--lang json|c|python|bash] FILE          # '-' reads stdin
lmok query [--lang json|c|python|bash] PATTERN FILE  # '-' reads stdin
lmok lint RULES.json FILE...
lmok check [--lang json|c|python|bash] RULES.dslb FILE...
lmok --dsl FILE [FILE ...]
```

`parse` prints the concrete syntax tree as a single-line s-expression, like
`tree-sitter parse` does. Malformed input still prints its tree — broken
regions appear as `(ERROR ...)` nodes — and still exits 0; tree-sitter always
produces a tree. `query` runs a tree-sitter S-expression pattern over the
document and prints one line per captured node, `kind
start_row:start_col-end_row:end_col` (0-based). Both default to the JSON
grammar; `--lang c|python|bash` switches grammars (an unknown language exits
2 with the known-languages list). `lint` runs a rule pack (below) over each file and
prints one JSON finding per line plus `N findings in <file>`; exit codes:
0 ok (findings never fail the run — gating is the caller's job), 2 usage or
rule-pack error, 3 input/DSL error (unreadable file aborts the run fast; an
invalid query pattern names the rule id, byte offset and reason), 4 stdout
failure.

```
$ echo '{"a": [1, null]}' | lmok parse -
(document (object (pair key: (string (string_content)) value: (array (number) (null)))))
$ lmok query '(pair key: (string (string_content) @key))' examples/data/sample.json
string_content 1:3-1:7
string_content 2:3-2:10
string_content 3:3-3:5
string_content 4:3-4:12
$ lmok lint examples/rules/c-security.json examples/data/sample.c
{"row":4,"col":2,"end_row":4,"end_col":6,"rule":"LBC001","severity":"critical","message":"unsafe call 'gets'"}
1 findings in examples/data/sample.c
```

The `--dsl` interface runs the body of lmscript bytecode against the same engine
with structured access to parsed trees. `check RULES.dslb FILE...` is the v6
entry path: it loads one DSLB file, indexes entries by annotation, runs its
body once, then parses and walks each explicit input file once. It dispatches
matching entries in source order, including anonymous tree-sitter kinds, and
emits the existing `ts.report_json()` findings plus `N findings` and
the standard success record for each input. Files are explicit; directories
and stdin are not accepted. A fresh host arena and findings sink are used per
input. Rule and severity metadata come from `ts.report` payloads; entry names
are not silently used as finding ids. Every annotation is checked against the
selected grammar at load: a misspelled kind, a supertype (abstract, e.g. `@expression`) or a hidden
symbol is refused with exit 3 and a `LOAD_UNBOUND` record naming the entry, so a rule can never be
silently dead; a rules file is therefore specific to one `--lang`. DSLB v6 is the only accepted runtime
format, so old bytecode must be recompiled. Install the private compiler with
`npm install --prefix /tmp/lmok-tools @lmctl-ai/lmctl@0.2.444` when needed.

## Rule packs

`lmok lint` reads a pack — conditions as data. Format:

```json
{
  "lang": "c",
  "rules": [
    {"id": "LBC001",
     "query": "(call_expression function: (identifier) @fn)",
     "text_matches": "^(gets|strcpy|sprintf)$",
     "message": "unsafe call '{text}'",
     "severity": "critical"}
  ]
}
```

- `id` (required string), `message` (required; `{text}` expands to the
  matched node's text), `severity` (optional, default `"minor"` — free-form,
  the pack owns the vocabulary).
- Selector — exactly one of (both present is a pack error, exit 2):
  - `on`: a tree-sitter node kind, matched against NAMED nodes. This mirrors
    the `@ANNOTATION` value of an lmcode rule fn. The `check` subcommand uses
    the v6 entry table and this same kind-indexed dispatcher.
  - `query`: an S-expression pattern; every captured node of every match is a
    candidate.
- `text_matches`: optional `std::regex` (ECMAScript) applied with
  `regex_search` (unanchored — anchor with `^`/`$` yourself) to the
  candidate's text as a post-match filter. Regex matching is bounded: node
  texts longer than 16 KiB are not searched (treated as no match, one note
  on stderr), because `std::regex` backtracks recursively and a long comment
  or string would otherwise overflow the stack. The same bound applies to
  query `#match?`.
- Language: `rule.lang` overrides the pack-level `lang`; a rule with neither
  is a pack error. Known languages: `json`, `c`, `python`, `bash`.
- Dispatch: per file, parse once per grammar used. `on` rules share ONE
  pre-order walk over the named nodes with a kind→rules index; `query` rules
  run one query cursor each over the root (merging them into the walk is a
  later optimization — deliberately no Rete). Findings print sorted by
  (row, col), one JSON object per line with the same field order as
  `ts.report_json` (row, col, end_row, end_col, rule, severity, message),
  then `N findings in <file>`.
- **Directories**: a FILE argument that is a directory expands recursively to
  its regular files — hidden files/dirs BELOW the argument skipped (the
  argument itself may live under a dot-path), only `.c`/`.h` (c), `.json`
  (json), `.py` (python) and `.sh`/`.bash` (bash) are lintable, paths sorted
  for byte-stable output, a file whose
  language has no rules in the pack is skipped silently, and a directory
  with no lintable files is a usage error (exit 2). A directory the walk
  cannot read aborts the run (exit 3, like an unreadable file) — never a
  silent partial list. Symlinks to files are followed, symlinks to
  directories are not. File and directory args mix freely. Rule ids must be
  unique within a pack (exit 2 otherwise).

Packs live in `examples/rules/` (`c-security.json`, `json-config.json`) as
small demos; the canonical rule library is `rules/` (see below).

## Rule tiers

Rules are classified by the information they need: tier 1 (CST shape + text)
ships as packs in `rules/` — `c-tier1.json` (11 rules: unsafe calls, command
exec, assignment-in-condition, K&R definitions, goto, FIXME/TODO, object-like
macros, register, naming, commented-out code, malloc-result casts) and `json-tier1.json` (11 rules:
credential keys, placeholders, nulls, key naming, numbers-as-strings, exact
password key, sensitive credential keys, HTTP URLs, unpinned versions, empty
containers, long strings), `python-tier1.json` (9 rules: os shell-out,
shell=True, eval/exec, assert, FIXME/TODO, wildcard imports, bare except,
print(), mutable default args) and `bash-tier1.json` (6 rules: rm -rf,
FIXME/TODO, backticks, unguarded cd, chmod 777, eval) — plus lmcode-native rules in `rules/lmcode/` for
checks that need logic (function metrics, switch/if style, duplicate JSON
keys and sibling string values, object size, nesting depth via `Node.has_parent()`/`parent()`, scanf width via
query predicates, same-file symbol rules via set algebra, unquoted bash
variables via the parent chain). Since v0.4.0,
trees are incremental: `Tree.edit(start, old_end, new_end)` records a
byte-offset edit, `ts.reparse`/`ts.reparse_c(old, new_text)` re-parse against
the old tree (which stays alive), and `ts.changed_ranges(old, new)` returns
the changed ranges as a JSON array string — the edit → reparse → re-lint loop
is [examples/incremental-reparse.lms](examples/incremental-reparse.lms). Tier-2 extensions E1 (query
predicates) and E2 (parent) are landed; E3 re-scoped to
performance/cross-file after the symbol rules proved scriptable; E4/E5 landed
in v0.3.5/v0.4.0; tier 3 (real semantics) is deferred.
The framework, inventory, and extension plan: [rule tiers](docs/rule-tiers.md).

```sh
lmok lint rules/c-tier1.json examples/data/sample.c
```

## Scripting

Save this as `build/show.lms` after the setup above, run
`lmscript build/show.lms`, then `build/lmok --dsl build/show.dslb`.
The script saves bytecode; `--dsl` executes it.

```lmscript
p = _dsl_ cli="lmok" install="github:lmctl-ai/lmok@latest";
let tree = ts.parse("{\"name\": \"lmok\", \"tags\": [\"tree-sitter\", \"json\"]}");
println(tree.sexp());
println("has_error -> " + str(tree.has_error()));
println("nodes -> " + str(tree.node_count()));
end _dsl_
savedsl(p, "build/show.dslb");
```

The DSL surface is deliberately small: `ts.parse(text)` returns a `Tree` with
`.sexp()`, `.has_error()`, `.node_count()`, and `.root()`. The `root()` handle
is a `Node` with `.kind()`, `.text()`, `.is_named()`, `.is_missing()` (zero-width placeholder tree-sitter inserted for a missing token),
`.start_row()/.start_col()/.end_row()/.end_col()` (all **0-based**,
tree-sitter convention), `.child_count()`, and `.child(i)` (named children
only, bounds-checked), plus upward navigation: `.has_parent()` (false at the
root) and `.parent()` (a located run-time error at the root). The schema also declares the SAM interface
`Rule(n: Node) -> void`: top-level lmcode functions can carry a `@NODE-KIND`
annotation and an `implements Rule` clause (checked at compile time), so
analysis rules are user-redefinable scripts over the CST — see
[examples/cst-rules.lms](examples/cst-rules.lms) and [durable-memory/03-rule-methods.md](durable-memory/03-rule-methods.md). The v6 `check` command
dispatches these entries by annotation; ordinary `--dsl` execution remains body-driven. Rule methods
require `lmscript` >= 0.2.444. Since v0.2.1, `Tree.nodes()` returns a
`NodeList` (flat pre-order list of named nodes, root first; `.len()` and
bounds-checked `.at(i)`), so `for (n in tree.nodes())` runs a whole-tree scan
in one foreach — see [examples/lint-json.lms](examples/lint-json.lms). Since v0.2.2, the host keeps a
per-run findings sink: `ts.report(node, message)` appends a finding
{row, col, end_row, end_col, message} (0-based positions, capped at 65536 per
run), read back with `ts.report_count()` and `ts.report_json()` (a JSON array
string with stable key order) — the script decides the rendering, see
[examples/report-json.lms](examples/report-json.lms). Since v0.2.5, `ts.report(node, message, rule,
severity)` also attaches rule metadata: `rule`/`severity` are free-form
strings (policy belongs to the script; the host validates nothing), emitted
by `report_json()` only when present (key order row, col, end_row, end_col,
rule, severity, message) — see [examples/rule-catalog.lms](examples/rule-catalog.lms) for per-severity
summaries and a quality gate computed in lmcode, [examples/suppress-findings.lms](examples/suppress-findings.lms)
for `// nosonar`-style suppression implemented entirely script-side, and
[examples/sarif-report.lms](examples/sarif-report.lms) for SARIF 2.1.0 emission (GitHub code scanning /
VS Code interop) in pure lmcode. Since v0.2.3, `ts.query(root, pattern)` runs a
tree-sitter S-expression query over the subtree at `root` and returns every
captured node as a `NodeList` (match/byte order; duplicates kept; a pattern
with zero captures yields an empty list; an invalid pattern is a runtime
error naming the byte offset and reason). Since v0.3.3 query predicates are
evaluated per match with upstream tree-sitter's semantics: `#eq?`/`#not-eq?`/
`#any-eq?`/`#any-not-eq?` (capture text vs literal or another capture),
`#match?`/`#not-match?`/`#any-match?`/`#any-not-match?`
(ECMAScript regex over capture text, UNANCHORED like upstream — anchor with
`^...$` yourself; texts over 16 KiB are not searched), and
`#any-of?`/`#not-any-of?`; the non-`any` forms must hold for every node of a
quantified capture. `#is?`/`#is-not?` and directives (`#set!`) are accepted
and ignored. A malformed predicate or an unknown `#…?` predicate is a
runtime error (exit 3) rather than a silent over-match. Published query files
using these predicates work as written. Since v0.2.4, a
second grammar is vendored: tree-sitter-c (pinned in `deps.lock`) with
`ts.parse_c(text)` / `ts.query_c(root, pattern)` mirroring the JSON pair — see
[examples/lint-c.lms](examples/lint-c.lms) for real security rules over C. Two more grammars
follow the same pattern: tree-sitter-python (`ts.parse_python` /
`ts.query_python`) and tree-sitter-bash (`ts.parse_bash` / `ts.query_bash`),
both pinned in `deps.lock` — these are the first grammars with an external
scanner (`src/scanner.c`), wired into the CMake source list. See `examples/` for
runnable scripts.

## Repository layout

- `cpp/` — the CLI host: DSL VM integration plus the tree-sitter wrapper
- `docs/` — design documents (e.g. `rule-snippets.md`: analysis rules as lmscript snippets)
- `schema/lmok.host.json` — the host-only DSL schema (copied to `schema/lmok.dsl.json` and embedded into the binary; `lmscript` >= 0.2.444 supplies the built-in standard library implicitly)
- `examples/` — lmscript examples (plus `examples/rules/` demo packs)
- `rules/` — the canonical rule library: tier-1 packs plus `rules/lmcode/` logic rules (see [rules/README.md](rules/README.md), [docs/rule-tiers.md](docs/rule-tiers.md))
- `tests/fixtures/` — lint fixtures (violations fire every rule; clean guards false positives)
- `scripts/` — vendoring (tree-sitter sources pinned in `deps.lock`, `lmcode-cpp` SDK pinned in `sdk.lock`) and compiler selection
- `third_party/` — downloaded upstream sources (git-ignored, pinned by `deps.lock`)

## License and company

lmok-authored code is MIT licensed (see `LICENSE`). It builds against
tree-sitter, tree-sitter-json, and tree-sitter-c, all MIT licensed (`THIRD_PARTY_NOTICES.md`;
also embedded in the binary, see `lmok --notices`). Made by
**Lattice Systems LLC**.

## Native OS regression checks

Every matrix runner executes tests/check-native-os.py through bash ./test.sh.
The checks use byte output, not Python newline translation: file/stdin parsing,
CRLF/LF/BOM, NUL/0xff/^Z, empty and multi-buffer inputs, UTF-8 paths, drive/separator
handling, diagnostics, executable discovery output and bounded queries. Windows
uses wide CRT arguments converted to UTF-8; filesystem paths cross through u8path.
Symlink assertions run when the OS grants that capability, with an explicit note
when Windows lacks the privilege.

Executable headers must match the matrix's x64/arm64 target. Linux ldd must show
no dynamic dependencies, macOS otool may list only system libraries, and Windows
PE imports may list only system DLLs. Each test group rejects a deliberately
corrupted output or architecture header; Linux also rejects a real dynamically
linked control executable. These tests and the disposable Git repositories used
by tests/release-hygiene.py need no network, models or installed lmok command.
