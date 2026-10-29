# Secrets as pack content

Four opt-in `rules/secrets-{json,bash,python,c}.json` packs use the existing
string-content/query dispatcher. Bash additionally handles single-quoted raw
strings. They intentionally do not scan comments, identifiers, binary blobs,
interpolated assembled values or arbitrary unquoted shell words.

Provider checks recognize shapes only: AWS AKIA IDs, ghp_/github_pat_, Slack
xox[baprs], PEM private-key headers and JWT triples. Generic credentials require
an assignment with a sensitive name plus 24–128 allowed bytes and Shannon
entropy >4.3. Entropy alone would classify normal random IDs as secrets.

`#entropy-gt?` is a host query predicate: capture plus quoted numeric threshold,
finite in [0,8], strict comparison, bytes rather than Unicode code points,
O(n+256), captures capped at 16 KiB. Out-of-domain/malformed arguments are query
errors; overlong and empty content do not match.

`redact: true` is a boolean pack field; redaction occurs in `maybe_emit` before
message expansion and persistence. It exposes at most four content bytes plus
[REDACTED], never stores the matched secret in a finding. Positions still span
the source node. Source files and explicit raw `query` output are not scrubbed.

Verification: `python3 tests/check-secrets.py build/lmok` runs provider-shaped
fakes assembled at runtime, plaintext-redaction/DB checks, low-entropy negative,
1000 seeded random ordinary strings per language, query threshold/domain/limit
checks and a tracked-repository zero-finding scan. Tests avoid checking in
real-shaped fake credentials so repository scans remain a useful oracle.

Baseline integration: a redacted finding's persisted `snippet` is
`sha256:<digest>` of the normalized node content, using the existing SDK SHA-256.
A raw snippet would bypass message redaction; using only the visible four-byte
prefix would hide a rotated credential from new-code detection. Normal findings
retain ordinary normalized snippets. Tests scan SQLite/WAL bytes for complete
secrets, pin the digest, and verify a same-prefix rotation becomes new. Native
SARIF messages are also checked for plaintext absence. Older raw-snippet rows are
not rewritten automatically by this change.
