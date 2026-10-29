# Tier 3: local code indexes and scan storage

Research date: 2026-10-28. Scope: a local, statically linked CLI, with no server,
platform account, background indexer, or daemon required. The repository is
**lmok**; its executable is **lmok**. This is a design proposal, not a
claim that the commands or database described below already exist.

**Recommendation:** embed SQLite for versioned per-file facts and lint history;
add optional FTS5 trigram indexing for text-search candidates. Keep source,
rules, and export artifacts as files. A database makes existing cross-file
set operations persistent and queryable; it does not supply name resolution,
types, alias analysis, or a sound taint engine. Those remain separate Tier-3
work, as described in [the rule inventory](../rule-tiers.md).

## 1. Semgrep: what its local engine actually stores

### Matching and the rule model

The examined `semgrep-core` driver receives rules and a mapping of targets to
rules, then processes targets with resource limits. Its target abstraction
lazily holds source text and the parsed generic AST. Parsing includes language
frontends and subsequent name-resolution/constant-propagation passes; this is
more than a tree-sitter CST query. Applicable rules are filtered, patterns are
matched, and formulas combine ranges and metavariable bindings into findings.
Rules can share a target's parse rather than reparsing for each rule. The
implementation distinguishes parse time, matching time, and formula evaluation.
[Core scan][sg-core], [target lifetime][sg-target], [parsing][sg-parse],
[rule dispatch][sg-match].

The YAML has a top-level `rules` list. A rule has `id`, `languages`, `message`,
`severity`, and a matching expression: `pattern`, conjunctive `patterns`,
alternative `pattern-either`, or `pattern-regex`. Operators constrain context,
exclude matches, or inspect metavariables; paths, metadata, and fixes are
separate concerns. `mode: taint` describes sources, sinks, sanitizers, and
propagators. This is a declarative analysis specification, **not a schema for a
persistent index**. For lmok, copy the separation of selector, condition,
reporting, and analysis options; preserve JSON rule packs/lmcode rather than
adding a second YAML frontend just to imitate Semgrep. [Rule syntax][sg-rules],
[taint operators][sg-taint].

### Intrafile, interfile, and join mode are different

| Mechanism | Local computation | Storage implication for lmok |
| --- | --- | --- |
| Structural search | Match syntax within a target; no project-wide semantic database is required. | Cache reusable file facts independently of rule execution. |
| CE taint | Function/method-level dataflow tracks sources through propagation to sinks. Intrafile means within a file; intraprocedural is narrower, within a function. Do not conflate them. | Persisting calls alone does not implement taint propagation. |
| Pro interprocedural/interfile analysis | Cross-function/cross-file analysis is a separate capability; the public docs describe it, but its proprietary implementation was not audited here. | Do not infer its persistent-cache schema from CE or claim lmok can cheaply reproduce it. |
| Experimental join mode | Run constituent rules, then correlate findings using metavariable values or paths. | A useful precedent for SQL joins over extracted facts, with explicit limits on what those joins prove. |

The documented `--pro-intrafile` mode crosses function boundaries within a
file; `--pro` can cross files, with `options: {interfile: true}` on applicable
rules and language support constraints. Neither setting turns a result join
into dataflow. [Analysis modes][sg-taint].

The published join implementation uses Peewee with SQLite **`:memory:`**,
constructs tables for rule results, and loads metavariable contents, paths, and
serialized findings before querying them. It is not a durable repository
symbol database. Equality of two captured names is not proof they resolve to
the same entity. Current documentation says join mode requires login and is
not available in CE; it is architectural evidence, not a proposed offline
runtime dependency. [Taint scope][sg-taint], [join documentation][sg-join-doc],
[join implementation][sg-join].

### Cache audit: distinguish four lifetimes

| Evidence in the inspected source | Contents and lifetime | What it does **not** establish |
| --- | --- | --- |
| `Xtarget` | Lazy source and AST/error values attached to a target during execution. | A reusable on-disk AST indexed by source hash across CLI invocations. |
| `Formula_cache` | In-memory results for repeated taint formulas: ranges and metavariable bindings, specialized to shared formulas. | Historical findings or a project symbol store. |
| `Disk_cache` | An abstraction that marshals values to a temporary directory, or marshals them into memory; reads produce fresh copies; cleanup removes disk files. | A stable public file format or an incremental content-addressed scan cache. |
| CLI environment paths | `~/.semgrep` settings/log paths; Python's version-check cache defaults under XDG cache/home `.cache` as `semgrep_version`. | A `.semgrep/cache` database containing every file's previous findings. |

These are observations from [Xtarget][sg-target], [Formula_cache][sg-formula],
[Disk_cache][sg-disk], [OCaml environment declarations][sg-env], and
[Python environment defaults][sg-pyenv]. In `Disk_cache`, hashing a caller's
key to form a filename is not itself hashing source contents or validating
mtime. Its setup/cleanup contract is temporary storage. Merely finding the
word “cache” in the source is insufficient evidence of cross-run reuse.

**Per-file results:** the examined CE scan path collects file results and
produces the run output; I did not find a general persistent per-file findings
cache in that path. This is a bounded source audit, not a claim about every
historical release, editor extension, or proprietary engine. There is no
verified CE mtime/content-hash invalidation algorithm here to copy wholesale.
Copy lazy parsing, shared intermediate computations, bounded work, and explicit
cache lifetimes. Do not copy OCaml marshaled ASTs into a long-lived C++ format.
[Driver][sg-core], [cache interface][sg-disk-interface].

## 2. Local index precedents

The size comparisons below describe structure, not benchmarks of lmok.
Except for the attributed Zoekt estimate, no universal source-to-index ratio
is asserted.

| Tool / format | What is stored and how it is queried | Size and invalidation tradeoffs |
| --- | --- | --- |
| **Universal Ctags** | A `tags` text file: name, filename, address separated by tabs; extended records add kind/scope and other fields after `;"`. Sorted names permit binary lookup. Address can be a line number or a search pattern. Reference tags exist for some languages; it is not generally a resolved whole-program reference graph. | Mostly selected declarations and snippets, so often much smaller than a full text/occurrence index. Repeating paths/snippets costs space. Regeneration is straightforward; append alone does not remove stale records. Adding history or many relationship queries makes the flat format awkward. [Format][ctags-format], [manual][ctags-man]. |
| **cscope** | `cscope.out` is a purpose-built, normally compressed cross-reference file for definitions, symbol occurrences, calls, and includes. `-c` selects uncompressed ASCII; `-q` adds inverted-index files `cscope.in.out` and `cscope.po.out`. Batch build and one-shot line-oriented search work without the interactive UI. | More relationships than a basic tags file; extra lookup files exchange space for speed. Rebuilds reuse unchanged-file records. Timestamp checks and file-list changes drive updates; `-u` forces rebuilding, `-d` trusts the existing DB. C-centric extraction is not full compiler semantics. [Manual][cscope-man]. |
| **Zoekt** | Memory-mappable binary shards store source, filenames, positional Unicode trigrams, compressed postings, branch masks, and metadata. Regex search extracts required literals to find candidates, then verifies the regex. | Its design document estimates about **3.5× source size** for shards, not a guarantee. Versioned shards and optional Git delta indexing avoid arbitrary in-place graph updates; incompatible options/branch sets require a normal build. A strong search design, but a substantial specialized engine to embed. [Design][zoekt-design], [Git indexing][zoekt-index]. |
| **SCIP** | Protobuf interchange organized by documents: relative paths, occurrence ranges and roles, symbol identifiers, documentation, and relationships. Source text is optional. | Binary encoding and compression reduce transport size; volume follows occurrences and metadata, not just declarations. File-oriented records support independent production, but consumers must implement updates, dependencies, and lookup indexes. Its design explicitly distinguishes transmission from query storage. [Schema][scip-schema], [design][scip-design]. |
| **LSIF** | A graph serialized as JSON lines: document/range/result-set vertices, definition/reference results, monikers, and connecting edges. Consumers ingest it to answer navigation requests without running a language server. | Explicit graph records are verbose; result-set sharing reduces repetition. Project dumps and monikers support composition, but producers/consumers own freshness and replacement. A transport format is not an embedded DB engine. [Specification][lsif-spec], [output guidance][lsif-output]. |
| **CodeQuery** | `cqmakedb` imports cscope and ctags into SQLite. The actual schema includes files, lines/snippets, symbols, calls, inheritance, membership, and configuration; indexes cover names, paths, and relationship endpoints. | Adds SQL indexes and repeats some source text; import requires the extractor artifacts too. The examined builder drops/recreates its tables, so do not advertise automatic per-file SQLite updates as a shipped property. The local CLI/query library is relevant; the GUI is not required for this precedent. [Overview][cq-readme], [schema/builder][cq-schema]. |

**Why ctags won a durable niche:** my interpretation of its documented design
is that a tiny editor-facing contract, inspectable text, sorted lookup, and
backward-compatible extensions made adoption cheap. lmok should copy that
interoperability lesson: export a simple documented format even if internal
storage is richer. It should not mistake broad editor support for evidence
that plain tags files are sufficient for finding history or semantic joins.
[Compatibility rationale][ctags-format].

**Useful boundary from SCIP:** adopt stable symbol identities and explicit
position encodings, not its wire encoding as the SQL layout. Distinguish
file-local identifiers from cross-file identities. A tree-sitter-extracted
spelling such as `foo` must remain an unresolved candidate until a resolver
proves which declaration it denotes. [SCIP schema][scip-schema].

## 3. Storage choices for lmok

| Workload | SQLite | Custom binary index | Plain files | Decision |
| --- | --- | --- | --- | --- |
| Per-file facts / symbols | Indexed joins, transactions, and replaceable file versions fit directly. | Could optimize packed records, but requires a format, reader, updater, and corruption/recovery design. | Easy per-file JSONL shards; project joins require loading or rebuilding another index. | **SQLite** with typed fact tables. |
| Lint finding history | Runs, rule versions, occurrences, and completeness metadata are relational. | Little initial benefit over SQLite; history retention and migrations become custom work. | JSONL/SARIF snapshots are useful exports, but history queries require repeated scans. | **SQLite**, plus explicit export. |
| Text / regex search | FTS5 trigrams narrow substring candidates; host verification completes matching. | A Zoekt-style positional index may eventually improve very large-repository search. | Direct file scanning is simple and avoids build/storage cost for one-off searches. | **Optional SQLite FTS5**, with direct-scan fallback; benchmark before adding a custom engine. |

SQLite's amalgamation can be compiled into the executable; no database server
or SQLite executable is required. Enable FTS5 in the pinned build and verify
its presence at build/test time. This preserves the single statically linked
CLI model. [Amalgamation][sqlite-amalgamation], [FTS5 build instructions][sqlite-fts].

“One database file” describes the main artifact, not an absolute prohibition
on sidecars. SQLite's default rollback journal is sufficient initially; WAL
permits reader/writer concurrency but introduces `-wal`/`-shm` and checkpointing,
with one writer at a time. Start with one CLI writer, short publication
transactions, and a bounded busy timeout. Do not require a daemon just to
manage SQLite. Use SQLite backup APIs or a closed, consistent database when
copying the artifact. [Journaling and WAL][sqlite-wal], [backup API][sqlite-backup].

### Text search: candidate generation is not regex execution

FTS5 trigrams operate on Unicode characters. Short full-text terms below three
characters do not match; unselective LIKE/GLOB queries can scan. Case-sensitive
trigrams support indexed GLOB rather than LIKE. LIKE with `ESCAPE` loses this
optimization. Reducing `detail` saves postings but restricts longer MATCH terms.
External-content FTS tables reference a **SQL table/view**, not filesystem
paths, and must be kept consistent. [Trigram and content options][sqlite-fts].

Proposed first implementation: opt-in `file_text(file_version_id, text)` plus
an FTS table over it, updated transactionally. Store only the current searchable
snapshot; do not retain every historical source revision by default. Start
with default/full detail to avoid unsupported query combinations; measure
whether `detail=none` is worthwhile before changing the query contract.

For regexes, derive only **necessary** literal constraints; preserve alternation
as OR, conjunction as AND, and fall back when no safe constraint is known.
Then run the actual matcher on each candidate's original bytes. A selective
filter may produce false positives, never false negatives. Case folding,
Unicode, invalid UTF-8, and binary input need explicit tests; bypass the Unicode
index when its semantics cannot safely cover the requested byte search.
SQLite's core library does not provide a default `regexp()` implementation;
FTS5 is not a turnkey regex index. [SQLite REGEXP contract][sqlite-expr].

The proposed CLI must distinguish “index snapshot” results from “current
workspace” results. For the latter, reconcile file changes before querying.
Verifying only returned candidates cannot discover a newly edited file that
was excluded by the old index. A no-index mode should scan files directly.

## 4. Invalidation and publication

### Existing behavior versus the proposed policy

Ctags' documented `--append` concatenates new tags; it is not a change detector
or stale-record collector. A wrapper can rebuild everything, or remove all
records for changed/deleted paths and regenerate them. The wrapper must choose
mtime or hashing. Cscope explicitly reuses unchanged-file records and exposes
timestamp/forced-rebuild controls. In the inspected Semgrep CE path, the
identified target/formula caches are execution-scoped, so cross-run mtime/hash
freshness is not established by them. [Ctags append][ctags-man],
[cscope rebuilds][cscope-man], [Semgrep target][sg-target].

| Policy | Cost | Failure modes / proposed use |
| --- | --- | --- |
| mtime + size | Stat each file; read only apparent changes. | Misses same-size edits with restored/coarse timestamps; a rename may change resolution without changing bytes. Offer only an explicit fast mode that does not claim hash-verified freshness. |
| Hash source bytes | Read every included file, but skip extraction for equal hashes. | Still costs O(total bytes) each reconciliation. Does not by itself invalidate changed rules, resolver settings, or dependencies. **Correctness-first default.** |
| Hybrid | Stat hint, hash suspected changes, plus occasional full validation. | Faster common case but inherits stat false negatives between validations. Document the tradeoff; do not market it as equivalent to always hashing. |

Proposed keys and update algorithm:

1. Enumerate the complete eligible path set with deterministic exclusions.
   Record a workspace identity, relative path, language, and content hash.
   Include dirty/untracked files according to explicit CLI scope; a Git commit
   ID alone does not identify the working tree. Detect added/deleted paths too.
2. Read each file once into the bytes used for **both hashing and parsing**.
   Compare before/after metadata and retry or mark unstable files; never publish
   facts attributed to a different byte version. This is a manifest of observed
   file versions, not a magically atomic snapshot of a changing directory.
3. Key extraction by content hash **and** grammar/parser build, extractor
   version/configuration, language, and relevant path/context. Key rule results
   by that fact version plus rule/lmcode bytecode hash, engine semantics,
   options, and applicable dependency/project generation. Include resource
   limits when they affect completeness. Record errors/timeouts explicitly.
4. Reuse identical fact versions, including files with zero facts. A content
   change replaces that file's facts and its current text-index entry. A rule
   change can rerun rules against retained facts when those facts suffice;
   AST-dependent rules must reparse. Deleted files leave the live manifest
   without erasing historical findings.
5. Initially, rerun project-wide rules whenever the input manifest changes.
   Later add dependency edges and reverse invalidation. C include paths/macros,
   Python import resolution, and Bash sourcing require context beyond bytes.
   Unknown dependencies mean a conservative full project-rule rerun, not a
   stale “clean” result. Negative facts, such as a missing definition, also
   depend on the set of files and declarations.
6. Build a new scan generation in staging tables, then atomically publish the
   completed manifest/current-generation pointer. Readers see the last complete
   generation. Interrupted/error scans stay marked incomplete and cannot imply
   that previous findings were fixed. A matching hash plus successful empty
   result is cacheable; a failed scan is not a successful empty result.

This is lmok's proposed policy, not behavior attributed to another tool.
Tree-sitter's in-process incremental reparse remains useful inside a run, but
it does not replace persistent version keys or dependency invalidation.

## 5. Concrete database shape and implementation boundary

Suggested logical tables (a design sketch, not a migration):

| Table | Minimum contents |
| --- | --- |
| `workspace`, `scan` | Root identity; scan id/time; scope/config/tool hashes; completion/errors; optional Git metadata. |
| `file_version`, `scan_file` | Relative path, language, source hash/length, parser/extractor identity; generation-to-version manifest. Avoid destructive overwrites of versions referenced by history. |
| `symbol`, `occurrence` | Qualified/scope-aware identity when known; otherwise spelling and unresolved status; kind; byte span; containing symbol; file version. |
| `relation` | Call/import/include/source edges with endpoints, relation kind, provenance, and resolution status. Unknown targets are representable. |
| `rule_version`, `finding`, `finding_observation` | Rule hash and metadata; reproducible finding key/location/message; observations per scan; complete/partial evaluation status. |
| `file_text` + FTS | Optional current-version text and trigram postings, updated together. Bound file size and expose skipped-file counts. |

Use integer surrogate keys internally, but preserve reproducible external
identities and hashes for export. Index occurrences by `(file_version, start)`
and symbol; relationships by both endpoints; findings by `(scan, rule)` and
`(file_version, rule)`. Hashing a location is only exact-version identity.
Cross-version finding tracking needs a separate, documented heuristic using
rule identity and stable context; line-number changes alone must not imply
new/fixed vulnerabilities.

Keep `.json` rule packs, `.lms` source, `.dslb` artifacts, source files, and
JSONL/SARIF/ctags/SCIP exports outside the database as ordinary files. Persist
rule hashes/provenance, not the only copy of rules. Keep ASTs and any future CFGs in memory for
now; only introduce a separate versioned binary cache after profiling proves
reparse cost warrants it. The DB stores facts and observations, not raw
pointers, tree-sitter handles, or an unversioned serialized AST.

For a first CLI, proposed operations are explicit `index`, fact queries,
`scan --record`, history export, and `index --rebuild`/prune. These are proposed
names, not installed commands. Start with bounded typed host/DSL accessors;
an unrestricted SQL API is not necessary to deliver useful local queries.
Limit schema scope to facts that current extractors can justify. Do not label
CST-derived calls as compiler-resolved calls.

## 6. Cost, sizing, and acceptance plan

**Engineering estimate, not a measured promise:** assuming the current CLI,
parser/query infrastructure, and a small initial fact schema, one experienced
C++ engineer should budget roughly **15–25 working days**:

| Work | Estimate |
| --- | ---: |
| Pinned SQLite/FTS5 static build, connection wrapper, schema/versioning, typed inserts/queries | 3–5 days |
| File manifests, content hashes, transactional generations, delete/retry/error semantics | 4–6 days |
| Initial per-file facts, rule-result history, CLI/DSL integration and exports | 4–7 days |
| Optional trigram candidate search, exact verification, regression/performance tests | 4–7 days |

This excludes a compiler-grade resolver, interfile taint, alias analysis,
SCIP import for every language, and a production-quality general regex query
optimizer. Those are additional projects, not consequences of choosing SQL.

Let B be included source bytes, F extracted facts, and H retained finding
observations. Cold extraction/search indexing consumes O(B) scanning/tokenizing
work plus parser cost and indexed inserts (roughly O(F log F)); FTS postings
scale with text. Hash-verified warm refresh still reads O(B) bytes but parses
only changed files. Selective indexed queries depend on posting sizes and
returned candidates; broad regexes still require O(B) scanning. History grows
with H unless retention/pruning is explicit. No credible SQLite/source size
multiplier can be supplied without a representative corpus and selected fields.

Before committing to default text indexing, measure cold build, unchanged
refresh, 1% edits, rename/delete, rule-only changes, warm/cold queries, database
bytes, and peak RSS on this repo and larger C/Python/Bash trees. Compare direct
scan, default/full-detail FTS, and reduced-detail FTS. Test restored-mtime edits,
UTF-8/byte offsets, duplicate names in separate scopes, new definitions that
invalidate “missing” facts, parser failures, interrupted publication, and
readers during updates. Keep regex budgets and incomplete-result reporting.

## Sources actually fetched

Documentation and raw repository files below were fetched during this survey.
Repository citations use pinned revisions, not moving branch URLs. Reading
source is not a benchmark or proof of every deployed version's behavior.

| Repository | Inspected revision |
| --- | --- |
| semgrep/semgrep | `087e93b3a1fdf44f97bc92b5b9c2ba221cb2488b` |
| sourcegraph/zoekt | `7e4c0f8378b64add296d1820df48080a88304d2a` |
| scip-code/scip (sourcegraph/scip redirects here) | `bbec5345bfae602b31286a9ec74e2c2042713f50` |
| microsoft/language-server-protocol | `112d978951f2223a962bf19516637862f587bf55` |
| ruben2020/codequery | `972192aede93c3d5c66e05201e5c8b43f016cd3d` |

[sg-core]: https://github.com/semgrep/semgrep/blob/087e93b3a1fdf44f97bc92b5b9c2ba221cb2488b/src/core_scan/Core_scan.ml
[sg-target]: https://github.com/semgrep/semgrep/blob/087e93b3a1fdf44f97bc92b5b9c2ba221cb2488b/src/target/Xtarget.ml
[sg-parse]: https://github.com/semgrep/semgrep/blob/087e93b3a1fdf44f97bc92b5b9c2ba221cb2488b/src/parsing/Parse_target.ml
[sg-match]: https://github.com/semgrep/semgrep/blob/087e93b3a1fdf44f97bc92b5b9c2ba221cb2488b/src/engine/Match_rules.ml
[sg-rules]: https://docs.semgrep.dev/writing-rules/rule-syntax
[sg-taint]: https://docs.semgrep.dev/writing-rules/data-flow/taint-mode/overview
[sg-join-doc]: https://semgrep.dev/docs/writing-rules/experiments/join-mode/overview
[sg-join]: https://github.com/semgrep/semgrep/blob/087e93b3a1fdf44f97bc92b5b9c2ba221cb2488b/cli/src/semgrep/join_rule.py
[sg-formula]: https://github.com/semgrep/semgrep/blob/087e93b3a1fdf44f97bc92b5b9c2ba221cb2488b/src/engine/Formula_cache.ml
[sg-disk]: https://github.com/semgrep/semgrep/blob/087e93b3a1fdf44f97bc92b5b9c2ba221cb2488b/src/core/Disk_cache.ml
[sg-disk-interface]: https://github.com/semgrep/semgrep/blob/087e93b3a1fdf44f97bc92b5b9c2ba221cb2488b/src/core/Disk_cache.mli
[sg-env]: https://github.com/semgrep/semgrep/blob/087e93b3a1fdf44f97bc92b5b9c2ba221cb2488b/src/osemgrep/configuring/Semgrep_envvars.mli
[sg-pyenv]: https://github.com/semgrep/semgrep/blob/087e93b3a1fdf44f97bc92b5b9c2ba221cb2488b/cli/src/semgrep/env.py
[ctags-format]: https://docs.ctags.io/en/latest/man/tags.5.html
[ctags-man]: https://docs.ctags.io/en/latest/man/ctags.1.html
[cscope-man]: https://cscope.sourceforge.net/cscope_man_page.html
[zoekt-design]: https://github.com/sourcegraph/zoekt/blob/7e4c0f8378b64add296d1820df48080a88304d2a/doc/design.md
[zoekt-index]: https://github.com/sourcegraph/zoekt/blob/7e4c0f8378b64add296d1820df48080a88304d2a/gitindex/index.go
[scip-schema]: https://github.com/scip-code/scip/blob/bbec5345bfae602b31286a9ec74e2c2042713f50/scip.proto
[scip-design]: https://github.com/scip-code/scip/blob/bbec5345bfae602b31286a9ec74e2c2042713f50/docs/DESIGN.md
[lsif-spec]: https://github.com/microsoft/language-server-protocol/blob/112d978951f2223a962bf19516637862f587bf55/_specifications/lsif/0.6.0/specification.md
[lsif-output]: https://github.com/microsoft/language-server-protocol/blob/112d978951f2223a962bf19516637862f587bf55/_specifications/lsif/0.6.0/implementation.md
[cq-readme]: https://github.com/ruben2020/codequery/blob/972192aede93c3d5c66e05201e5c8b43f016cd3d/README.md
[cq-schema]: https://github.com/ruben2020/codequery/blob/972192aede93c3d5c66e05201e5c8b43f016cd3d/makedb/cs2sq.cpp
[sqlite-amalgamation]: https://www.sqlite.org/amalgamation.html
[sqlite-fts]: https://www.sqlite.org/fts5.html
[sqlite-wal]: https://www.sqlite.org/wal.html
[sqlite-expr]: https://www.sqlite.org/lang_expr.html
[sqlite-backup]: https://www.sqlite.org/backup.html

## Concrete recommendation

Implement **one embedded SQLite database per workspace**, with transactions,
content-hash-identified file versions, typed symbols/occurrences/relations,
and immutable scan/finding observations. Add **optional FTS5 trigram search**
over a current text snapshot, always followed by exact matching where needed
and a correct direct-scan fallback. Keep authoritative source/rules and portable
exports as files; keep ASTs in memory. Use content hashes for correctness,
mtime only for an explicitly weaker fast mode, and rerun project rules until
reliable dependency invalidation exists. Budget **15–25 engineer-days** for the
bounded storage/search foundation; treat semantic resolution and interfile
taint as separate work. This advances Tier 3 without introducing a service,
a platform, or a mandatory background process.
