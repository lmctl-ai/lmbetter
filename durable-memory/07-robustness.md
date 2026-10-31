# Adversarial robustness pass (2026-10-28)

Workspace: `lmok-astra1`, branch `fix/robustness`. Linux x86-64,
GCC/libstdc++ 13.3, system lmscript 0.2.442. Baseline: 53 checks passed.
No version, workflow, dependency-pin, or vendored SDK changes.

## Findings and disposition

| Attack | Original result | Disposition / fix |
| --- | --- | --- |
| Zero bytes, whitespace, truncated JSON, bytes 0..255, invalid UTF-8; parse and lint with JSON and C | No crashes; syntax errors represented in trees | Acceptable, preserve tolerant parsing |
| Rule pack top-level null/array/number, missing rules, wrong rules type, null rule, wrong language/on types | Located pack errors, exit 2 | No change |
| 42 field/type variants across id/on/query/message/severity/text_matches/lang | 41 exit 2; empty message accepted | Acceptable; empty messages are strings |
| 10,000-level rule-pack JSON | Exit 2, SDK JSON depth limit at byte 256 | No change |
| `(a+)+$` against 30 `a`s followed by `!` | Still running after 5 seconds | Real bug: bounded regex execution, exit 3 with rule id |
| `a+$` against 100,000 `a`s | SIGSEGV in recursive regex matching | Same execution budget prevents stack exhaustion |
| 50,000-byte literal regex, 10,000 nested regex groups, `a{30000}` | Release survived this corpus; unsafe compilation scale | Bound regex source and counted expansion before compiling; exit 2 for packs |
| Regex query predicates, including invalid `[` | Errors could bypass query/cursor destruction | RAII ownership releases both on all exits; matching uses same budget |
| Empty query | Exit 0, no captures | Acceptable |
| Nested array queries at 100/1,000/10,000/20,000/50,000 levels | 50,000 levels SIGSEGV | Limit structural query nesting to 256 before tree-sitter compilation; exit 3, byte offset |
| Parse a directory | Exit 0 with `(document)` | Real bug: reject non-regular named inputs, exit 3 |
| FIFO inside lint directory | Skipped | Acceptable, pinned |
| Explicit FIFO as input | Blocked until timeout | Reject non-regular named inputs before opening; stdin `-` remains available for streaming parse/query |
| Unreadable regular file in directory | Exit 3, cannot read path | Acceptable, pinned |
| Unreadable subdirectory | Could misreport empty directory or silently truncate traversal depending on entry order | Report traversal failure, exit 3; prune hidden directories before descending |
| Invalid UTF-8 captured into `{text}` | Invalid UTF-8 in JSON findings | Replace each malformed byte with `\ufffd`; preserve valid UTF-8, escape controls normally |
| Node.child(-1), Node.child(2^32), Node.child(INT64_MAX) | Located runtime bounds errors | No implementation change; exact JSON errors pinned |
| Every truncation and every single-byte XOR-255 mutation of a 658-byte compiled parse-string example | 1,115 exit 3; 201 exit 0; no signal or timeout | Acceptable: valid mutations can change strings/line metadata; loader need not reject all mutations |
| 50 MiB JSON array and 20,000-function C file | Parse and lint completed | No crash; measurements below |

## Implementation and compatibility

`cpp/bounded_regex.hpp` retains ECMAScript std::regex semantics while charging
iterator dereferences, comparisons, and movement against a shared 4,096-operation
budget per candidate search. Copies share the budget, including backtracking.
This conservatively prevents the reproduced hangs and recursive stack exhaustion.
Compilation accepts at most 1,024 pattern bytes and a conservative product of
counted-repeat bounds at most 1,024. This can reject expensive **valid** patterns
and long candidate searches; failure is explicit rather than a missing finding.
The operation count depends on the standard-library implementation. It is not a
wall-clock guarantee or a claim that arbitrary regex workloads are now cheap.
A future linear-time engine could relax these limits, at the cost of dependency
and compatibility decisions outside this patch.

Query nesting ignores strings, escaped quotes, and semicolon comments. Limits
apply equally through CLI query, lint query rules, and lmcode query methods.
Pack regex compilation errors exit 2; matching/query errors exit 3 with a rule
id, query offset, or lmcode line/column where available. Node index bounds remain
64-bit and checked before the tree-sitter uint32 conversion.

`cpp/input.hpp` shares regular-file validation and checked binary reads between
CLI source files, rule packs and bytecode; bytecode load failures retain their
`LOAD_BAD_BYTECODE` JSON convention. The pre-open filesystem check is not an
atomic defense against concurrent path replacement. No filesystem-race attack
was performed. `-` still reads stdin for parse/query.

## Large-input recipe and measurements

The JSON input is 52,428,801 bytes: an array of 51,200 strings, each containing
1,021 `x` characters (`'[' + ','.join(json.dumps('x'*1021) for _ in range(51200)) + ']'`).
The C input contains 20,000 distinct definitions generated with
`'int f%d(void) { return %d; }\n' % (i, i)` (677,780 bytes).
JSON lint used a single `on: null` rule to exercise parsing and the named-node
walk without generating 51,200 findings. C lint used `rules/c-tier1.json`.
Outputs went to `/dev/null`; `/usr/bin/time -f 'elapsed=%e maxrss_kb=%M'` measured
wall time and maximum resident memory. Initial release measurements:

| Input / operation | Seconds | Peak RSS KiB |
| --- | ---: | ---: |
| JSON parse | 1.69 | 133,816 |
| JSON lint | 1.11 | 142,676 |
| C parse | 0.37 | 43,976 |
| C lint | 1.03 | 47,012 |

These characterize large byte input with moderate node counts, not a claim that
an array of tens of millions of tiny nodes uses the same memory.

## Validation

Seven appended `test.sh` checks cover the fixes with byte-exact output and exit
status assertions and subprocess timeouts. Permission checks run on non-root
POSIX; FIFO checks run where `os.mkfifo` exists. The INT64_MAX lmcode argument is
constructed as `9007199254740991 * 1024 + 1023`, because lmscript rejects integer
literals beyond its exact-number range. Original 53 checks remain in place.
The link check honors explicit `LMOK_FULL_STATIC=OFF` for sanitizer builds;
the default release gate still requires a fully static binary.

ASan setup (separate build directory):

```sh
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DLMOK_FULL_STATIC=OFF \
  '-DCMAKE_C_FLAGS=-fsanitize=address -fno-omit-frame-pointer -fno-pie' \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address -fno-omit-frame-pointer -fno-pie' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address -no-pie'
cmake --build build-asan --clean-first -j2
BUILD_DIR=build-asan ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./test.sh
```

Clean release build plus `./test.sh`: **60/60 checks passed** (53 original + 7
new). Clean ASan build plus the whole suite: **60/60 checks passed**, with
`detect_leaks=1:halt_on_error=1` and no ASan/LeakSanitizer report. An earlier
instrumented permission-test discrepancy disappeared after rebuilding all
objects from clean state; the clean result is the reported gate.

The small-input attack corpus also completed under the clean ASan binary with
only expected exits 0/2/3 and no sanitizer reports. Raw logs and generated inputs
are in the ignored `build/robustness/` directory in this workspace. The durable
regressions are in `test.sh`, so the fixes do not depend on those scratch files.

The final clean ASan bytecode mutation run repeated all 1,316 cases: 1,115
controlled load/run errors (exit 3), 201 successful valid mutations (exit 0),
no timeout, signal, or sanitizer report. Regex boundary probes (500 nested
groups within the source limit, 1,024-byte literals, counted optional repeats,
empty groups and overlapping alternations) also completed without reports.

Large ASan measurements so far: JSON parse 65.25 s / 310,168 KiB, JSON lint
55.85 s / 321,296 KiB, C parse 71.08 s / 82,392 KiB. The first full C lint
run exceeded the harness's 120-second cutoff without reporting a sanitizer
fault; it was rerun separately with a longer allowance. These timings are
instrumented runs, not performance targets.

The extended full C lint ASan run completed with **exit 0**, **146.17 seconds**,
**129,480 KiB peak RSS**, and no sanitizer output. Thus all four large-input
parse/lint cases completed under ASan, as well as the whole 60-check suite and
mutation/small-input corpora. No remaining blocker. The known compatibility
tradeoff is the explicit conservative regex resource budget described above.

## Merge integration notes (merged onto main after Fable's review fixes)

This branch was merged onto a main that already contained an independent
review-fix pass (Fable, 8c64380: regex text cap, reparse contract, arena caps,
directory-walk hardening, structured query predicates). Where the two
hardened the same spot, the merge reconciled as follows:

- **Regex defense is layered**: Fable's 16 KiB text cap first (over-cap node
  text is skipped with one stderr note, treated as no match), then this
  branch's operation budget within the cap — raised from 4,096 to 65,536 in
  the merge: libc++ (macOS) charges several times more iterator ops per scan
  position than libstdc++, so a legitimate anchored scan of a ~150-char
  container text (shipped rule LBJ009) tripped 4,096 on macOS CI. The
  catastrophic short-text cases still trip any finite budget quickly. Consequence for one branch
  pin: `a+$` against 100,000 characters now hits the cap (exit 0 + note), not
  the budget (exit 3). Catastrophic short-text cases (`(a+)+$`) still hit the
  budget — the cap alone never stopped those.
- **Compile-time limits** (1,024 pattern bytes / 1,024 repeat expansion) apply
  to pack `text_matches` AND `#match?` regexes (via `bounded_regex` at both
  compile sites).
- **Messages unified with main's**: directory-as-input says "cannot read
  PATH: is a directory"; a traversal failure names the failing entry
  ("cannot read ENTRY: Permission denied"); an invalid `#match?` regex says
  "#match? has an invalid regex '...'" inside the pattern-scoped predicate
  error. The branch's pins were updated accordingly.
- **Nesting limit (256)** ported verbatim into the structured-predicate
  `node_query` (main already had RAII query/cursor ownership; the branch's
  unique_ptr version was not needed).
- **Directory walk**: hidden entries are dot-components BELOW the argument
  root (main's fix) AND hidden directories are pruned without descending
  (this branch); the hidden check must run BEFORE the regular-file filter or
  pruning never happens (ordering bug found by this branch's own pin).
