# Examples

Run from the repository root after the [build and setup](../README.md#build-and-run).
Use `lmscript` >= 0.2.444 with the matching host on PATH:

```sh
export PATH="$PWD/build:$PATH"
mkdir -p build
lmscript examples/parse-string.lms
build/lmok --dsl build/parse-string.dslb
```

The outer script's `savedsl` call chooses the output filename. A second
positional argument to `lmscript` is a script argument, not an output-path
option. Compilation is normally silent. Execution prints the JSON text,
its tree, `has_error -> false`, `nodes -> 34`, and the host success record
`{"ok": true, "result": null}`. Each script's `# expect:` / `#|` comments
list its expected program output; `--dsl` adds the success record.

The `install="github:lmctl-ai/lmok@latest"` declaration can resolve a
published host when one is not on PATH, once maintainers publish renamed
`lmok-OS-ARCH` release assets. The repo rename alone does not create those assets. It follows the latest release,
not a fixed version pin. Compilation is offline once the local host and
compiler are available. Raw URL invocation and its working-directory
requirements are covered in the [README](../README.md#published-host-and-raw-urls).

Run all 13 examples (12 bytecode demos and one outer-script CLI demo):

```sh
for source in examples/*.lms; do
  lmscript "$source" || exit 1
  case "$source" in
    examples/lint-repo.lms) ;; # runs subprocesses directly; no saved bytecode
    *) build/lmok --dsl "build/$(basename "$source" .lms).dslb" || exit 1 ;;
  esac
done
```

`lint-repo.lms` prints `repo lint: c-security 1 findings, json-config 2 findings`.
`sarif-report.lms` also writes `build/sarif-results.sarif` (four results).
Findings and the catalog demo's `gate=FAIL` are demonstration output, not a
nonzero process exit status.

- `parse-string.lms` — parse an inline JSON string, print the tree
- `parse-file.lms` — read [examples/data/sample.json](../examples/data/sample.json), print tree stats
- `detect-error.lms` — parse malformed JSON, flag the ERROR node
- `cst-rules.lms` — two `@`-annotated rules `implements Rule` over the JSON CST
  (`Tree.root()` + `Node` navigation with manual fixed-depth loops)
- `lint-json.lms` — a tiny JSON linter: three annotated rules run over
  `Tree.nodes()` (flat pre-order named-node list) with one foreach — the
  idiomatic way; no manual walk
- `report-json.lms` — the same linter on the host findings sink: each rule is
  just `ts.report(n, "...")`; the script prints `ts.report_json()` and a
  summary count. No predicate helpers, no manual counting
- `query-json.lms` — selector-based rules: `ts.query(root, pattern)` picks
  candidate nodes with a tree-sitter S-expression pattern (`@key` captures),
  the rule text-checks in lmcode and reports
- `lint-c.lms` — the same over real C code (tree-sitter-c grammar):
  `ts.parse_c` + `ts.query_c`, flagging `gets`/`strcpy`/`sprintf` calls and
  FIXME comments, with rule keys + severities (`LBC001` critical,
  `LBC002` minor)
- `rule-catalog.lms` — user-side policy: parse `ts.report_json()` back with
  the std json functions, summarize by severity, and apply a quality gate
  (`critical > 0` → `gate=FAIL`) as a plain script-side `if`
- `suppress-findings.lms` — `// nosonar`-style suppression as pure lmcode
  policy: rules report everything; a chunk-level post-filter drops findings
  on rows whose comment contains "nosonar" (StrMap + `json_parse`/`to_json`)
- `sarif-report.lms` — analyzer interop: emit a minimal valid SARIF 2.1.0
  document to `build/sarif-results.sarif` via string building; severity→level
  mapping, the 0-based→1-based region conversion, and the driver rule catalog
  (derived from the findings) are all script-side
- `incremental-reparse.lms` — the incremental loop: edit the source text,
  `Tree.edit` + `ts.reparse_c` re-parse against the old tree,
  `ts.changed_ranges` shows what moved, and the re-linted NEW tree finds the
  inserted unsafe call
- `lint-repo.lms` — outer-lmscript composition: drives the `lmok lint`
  CLI dispatcher as a subprocess via `dsl_cli` + `system`, asserts exit
  codes, counts findings in stdout
- `rules/` — demo rule packs for the `lint` subcommand (`c-security.json`:
  unsafe-call query + `on: comment`/`on: goto_statement` kind rules;
  `json-config.json`: JSON password-key/placeholder rules as data). The
  canonical, larger library lives in the repo-root `rules/` (tier-1 packs,
  see [rule tiers](../docs/rule-tiers.md)); `examples/rules/` stays small on purpose.

`cst-rules.lms` shows manual `Node.child(i)` navigation (fixed-depth nested
loops, because lmcode has no recursion); `lint-json.lms` shows the `NodeList`
idiom that replaces it for whole-tree scans; `report-json.lms` shows the
`ts.report` sink that replaces the predicate-helper/counting idiom from
`lint-json.lms`; `query-json.lms` shows S-expression selectors replacing
structural navigation. Rule annotations (`@PAIR`, `@STRING`, `@NUMBER`) are
v6 entry dispatch keys when the compiled file is run through `lmok check`.
The ordinary `--dsl` examples remain body-driven and keep their explicit loops.
Node rows/columns are 0-based (tree-sitter convention); `ts.report` findings
carry row/col/end_row/end_col plus the message, and the 4-arg overload adds
free-form `rule`/`severity` strings (emitted by `report_json` only when
present; the host validates nothing — policy is script-side). `ts.query`
returns captured nodes in match/byte order (duplicates across
captures/matches kept) and evaluates the standard query predicates
(`#eq?`/`#not-eq?`/`#match?`/`#not-match?`; `#match?` is unanchored like
upstream tree-sitter); unknown directives are ignored.

`./test.sh` compiles and runs every example as part of the check suite.

To run the entry dispatcher over an existing rule file and a C input, use the
explicit language flag:

```sh
lmok check --lang c build/c-function-metrics.dslb examples/data/sample.c
```

The command parses each named input once and resets transient trees/findings
between inputs. Cross-file aggregation remains a body-level `--dsl` concern.
