# Baseline / new-code-only linting and host SARIF (roadmap wave 1, items 3+4)

## Baseline keying (cpp/baseline.*, `lint --new-since/--baseline/--changed/--fail-on-new`)

- NEW-ness key = (rule, file, message, normalized snippet) — positions are
  deliberately excluded, so edits above a finding do not resurrect it. The
  snippet is the matched node's source text with whitespace collapsed
  (`normalize_snippet` in lint.cpp); it is stored in the new
  `findings.snippet` column. Ordinary snippets are never printed; credential
  snippets are SHA-256 identities and are exported in JSON-lines baselines.
- **Multiset semantics**: each baseline entry is consumed once. A file with
  two identical `goto used` findings against a baseline holding one reports
  the second as new. Within one (rule, file, message) group, an unconsumed
  snippet-exact entry is preferred over a snippet-less one.
- **Fallback**: rows recorded before the snippet column (NULL) and
  ordinary `--baseline FILE` entries without snippets match on
  (rule, file, message) alone. A `--baseline` file's finding lines attach to
  the file named by their FOLLOWING `N findings in PATH` line; lines missing
  the key fields never match (their findings report as new — the safe
  direction for a gate). Redacted current findings never match snippet-less
  fallback entries. Suppression-bearing and ambiguous filename groups carry
  escaped JSON `file` keys; legacy suppression footer suffixes are stripped.
- `--new-since RUN`: RUN = `last` (max started_at, then id) or a `runs` id;
  latest completed observation at or before RUN for each file/absolute-pack
  scope supplies its occurrence multiset. `run_files` includes clean scans,
  so repeated observations do not accumulate and clean scans clear allowances.
  Unvisited files retain their observations. Legacy DBs without observation
  history supply no allowances until a fresh scan establishes a baseline.
  No release tags are recorded in the schema, so there is no tag selector.
  The baseline loads BEFORE the run; with `--db` in the same command the
  current run is recorded only at the end, so it never sees its own rows.
  Comparison DB opens ReadOnly — a missing DB is exit 2, not "all new".
- `--changed[=REV]`: `git diff --name-only -z REV` (bare = HEAD: working tree vs
  HEAD) via popen (`git -c core.quotepath=false`, revision characters
  whitelisted to [A-Za-z0-9._/~^-] because the same command line must be safe
  under both sh and cmd.exe). Repo root from `git rev-parse --show-toplevel`;
  target comparison is on absolute, lexically normal, forward-slash paths.
  NUL separators preserve raw names without Git's human quoting or CR trimming.
  Outside a git repo / unknown rev / bad rev chars: exit 2.
- `--fail-on-new` requires a baseline flag; exit 4 when new findings exist
  (4 is shared with stdout failure — documented). Without baseline flags the
  output is byte-identical to before; with them, per-file `N findings in
  FILE` counts printed (new) findings and a trailing `N new, M baseline`
  summary line is added. `--db` recording always stores the UNFILTERED set.
- Suppression integration (wp18a landed first): suppressed findings are
  dropped inside `lint_text` before baseline matching, so they can never
  become "new". No marker plumbing was needed.

## findings.snippet column (schema_version stays 2)

Additive nullable column, added to the embedded schema SQL and applied to
existing DBs by `ALTER TABLE findings ADD COLUMN snippet TEXT` when
`PRAGMA table_info` shows it missing (Create mode only; readers use
`db_has_column` and fall back). A nullable append is compatible with old
binaries both ways, so the version stays 2 — docs/tier3/db-design.md has the
note (and its ```sql block remains byte-identical to cpp/db.cpp;
tests/check-db.py's positional findings insert gained the NULL).

## SARIF (cpp/sarif.*, `--sarif FILE` on lint/check/ci)

- Straight port of examples/sarif-report.lms: version 2.1.0 + schemastore
  `$schema`, one run, driver "lmok" (version = LMOK_VERSION), results with
  ruleId / level / message.text / physicalLocation; 0-based → 1-based
  (endColumn 1-based exclusive after +1). Level map identical to the example:
  critical → error, minor → note, everything else → warning.
- driver.rules: lint/ci take pack metadata (id; shortDescription = pack
  message; helpUri from the rule's top-level `url`, the wp18a metadata
  field). check derives distinct rule ids from the results in first-seen
  order ("rule <id>" description), like the example.
- check "body" findings (no file) are emitted WITHOUT `locations`.
- The file is written after the run completes (ci: also when the threshold
  exits 1), with an empty results array when there are no findings; an
  unwritable path is exit 3. Under baseline filtering the SARIF holds the
  printed (new) findings only.
- `ci --sarif` rides wp18a's `run_project_ci` via an optional SarifSink
  (cpp/project_ci.hpp). Their config `baseline` field stays REFUSED
  ("reserved for baseline support (item 3)") because tests/check-project.py
  pins that refusal — wiring it is a follow-up once wp18a is done.

## Tests

tests/check-baseline-sarif.py (wired at the end of test.sh): a fixture git
repo (two commits: gets() moved down + strcpy added), position-keyed
failing control (the naive row/col diff MUST flag both findings), stripped-
key baseline control, DROP COLUMN snippet fallback control, --changed
contracts (incl. GIT_CEILING_DIRECTORIES for the outside-git case), SARIF
structure validator shared by lint/check/ci plus a broken-SARIF control
(0-based row unconverted must fail validation).
