# lint --db + findings verb (spike wave 2B)

Run recording and supersession per `docs/tier3/db-design.md` §1/§5 and the
two hard rules in `docs/tier3/local-index-survey.md` §4 (a failed/interrupted
scan is never a successful empty result; superseded history is kept).

## What landed

- `lmok lint RULES.json FILE... [--db PATH]` — stdout and exit codes
  byte-for-byte unchanged. With `--db`, per-file findings are COLLECTED
  during the run and written only after every target linted cleanly, in one
  transaction (`record_lint_run` in cpp/lint.cpp): one `runs` row
  (`started_at`, full command string), then per file `UPDATE findings SET
  superseded=1` for that file's live rows + insert the fresh set
  (superseded=0). A mid-run lint error (exit 3) records nothing; a DbError
  mid-record rolls everything back.
- `lmok findings [--rule ID] [--file PATH] [--db PATH]` — live
  (superseded=0) findings as lint's JSON lines plus a `"path"` field (the
  one deliberate format extension, sorted by path/row/col). Missing DB is
  exit 2 with "run `lmok index` first" (the ReadOnly open path).
- Fixtures `tests/fixtures/db/{a.c,b.c}`; additive test.sh section at the
  end (byte-exact findings pin, --rule/--file filters, supersession counts
  via python3 sqlite3, missing-DB exit 2, recording-failure-is-not-a-lint-
  failure, `--db` without a value exits 2).

## Decisions (the brief asked for these)

- **Missing DB on `lint --db`: CREATE it**, via the same resolution chain as
  `index` (`resolve_db_path(path, write=true)` + Mode::Create). Rationale:
  `--db` is an explicit recording request and lint --db is a write
  operation; db-design.md §1's chain creates automatic project/cache dirs
  for write operations (explicit paths still require an existing parent —
  enforced by sqlite open failing, caught, demoted to a stderr note).
- **Recording failure never fails the lint**: any DbError while
  opening/recording becomes one stderr line ("lint: run not recorded in DB:
  ..."), exit code and stdout stay the lint's. The lint contract is primary;
  the DB is a side channel.
- **Unindexed linted files get a minimal `files` row** (path as printed,
  lang from extension or "unknown", real sha256/mtime/size, indexed_at=now)
  via INSERT OR IGNORE — findings never dangle, and an existing (indexed)
  row is never clobbered. CAVEAT for the index wave: lint stores the path
  spelling exactly as printed (as-given, forward slashes); if `index`
  normalizes to project-root-relative paths, the same file can appear twice
  until a path convention is shared.

## Notes

- `load_rule_pack` now rejects duplicate rule ids — the dead duplicate-id
  LBC007 removed in the c-tier1 hardening (durable-memory/06) would have
  been a pack error here.
- `status` counts findings including superseded rows; `last_run` shows the
  recorded lint command.
