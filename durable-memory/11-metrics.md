# Local metrics and cognitive complexity

`lmok metrics [--lang L] FILE|DIR...` emits one JSON line per file with `file`,
`lines`, `functions`, `max_nesting`. Physical lines count LF terminators plus an
unterminated final line (empty input 0, a trailing LF adds no phantom line).
Named function definitions/declarations (including Go), methods and constructors
are counted. Nesting
counts control structures, including loops, if, switch and try/catch, excluding
else-if as an additional level. It is syntactic nesting across the file; it is
not execution or semantic complexity. Existing target expansion handles paths,
ignore files and languages.

`lmok dup [--lang L] [--tokens N] FILE|DIR...` uses parser leaf tokens, collapses
whole literals and identifiers, preserves keywords/operators, and skips comments.
A rolling 64-bit token hash groups equal N-token windows, with token equality
checking to reject hash collisions. Defaults to N=20; nonpositive/noninteger N
is usage error 2. Both copies must be nonoverlapping within one file. It emits
unique repeated window ranges sorted by location, zero-based rows/byte columns,
end exclusive, with `tokens=N`. Adjacent overlapping findings are intentionally
not merged into maximal clones; there is no cross-file matching. Hashing is O(T),
verification O(N) per matching window; memory is O(T). This is syntax similarity,
not proof that two pieces of code have the same behavior. Literal interpolation
is collapsed along with the literal, deliberately making duplication approximate.

The opt-in C/Python `rules/lmcode/*-function-cognitive-complexity.lms` entries
report functions with score >15 (copy/edit the threshold to tune it). Scoring:

- if/for/while, C do/switch/ternary and Python except/ternary/match add 1 plus
  enclosing structural nesting depth within that function.
- else, elif and C goto add 1; C else-if adds 1 without a nesting penalty.
- a run of identical boolean operators adds 1; a change between &&/|| or and/or
  adds another. Parentheses alone do not break a run. Arithmetic adds zero.
- nested named functions and lambdas are excluded from the outer function score;
  named functions have their own entry dispatch. No recursion, macro expansion,
  comprehension or semantic control-flow analysis is attempted.

This is an explicit local profile, not a claim of byte-for-byte Sonar parity.
The ten-function table in `tests/check-metrics.py` checks both languages:
flat 0, branch 1, nested 3, loop 6, alternative 2, chain 3, same_boolean 2,
mixed_boolean 3, siblings 2, deep 21. Their nesting values are 0,1,2,3,1,1,1,1,1,6.
The score=15 boundary remains silent, score=21 fires. A compiled zero-score
mutant is rejected before the actual table oracle. `tests/check-dup.py` pins
ranges, normalization, keyword/operator preservation, nonoverlap, bad thresholds,
CRLF and UTF-8 paths. Both tests run through test.sh on native matrix runners.

Physical line counts for that same table are C: one per function; Python:
2,4,5,6,5,7,4,4,6,9 (52 total). The combined table has ten functions and maximum
nesting 6. These physical-line values are checked alongside scores and nesting.
