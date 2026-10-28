#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"
BUILD_DIR="${BUILD_DIR:-build}"
HOST="$BUILD_DIR/lmbetter"
PATH="$HERE/$BUILD_DIR:$PATH"
export PATH
UNAME_S="$(uname -s)"
case "$UNAME_S" in MINGW*|MSYS*|CYGWIN*) HOST="$HOST.exe";; esac
stat_size(){ stat -c %s "$1" 2>/dev/null || stat -f%z "$1"; }
# Scratch lives under the (git-ignored) build dir: /tmp is an MSYS virtual
# path in Git Bash, and the Node-based lmscript compiler resolves it against
# a Windows drive where it does not exist. On Windows the path is converted
# to a native forward-slash path so it can be embedded in .lms sources.
mkdir -p "$HERE/$BUILD_DIR"
WORK="$(mktemp -d "$HERE/$BUILD_DIR/test-tmp-XXXXXX")"
case "$UNAME_S" in MINGW*|MSYS*|CYGWIN*) WORK="$(cd "$WORK" && pwd -W | tr '\\' '/')";; esac
trap 'rm -rf "$WORK"' EXIT
pass=0
ok(){ pass=$((pass+1)); echo "ok $pass - $1"; }
fail(){ echo "FAIL: $*" >&2; exit 1; }

LMSCRIPT="$(scripts/select-compiler.sh)"
scripts/sync-sdk.sh --verify >/dev/null || fail "SDK header pin mismatch"
ok "SDK headers match sdk.lock"
scripts/sync-deps.sh --verify >/dev/null || fail "deps pin mismatch"
ok "third_party/ tree-sitter, tree-sitter-json and tree-sitter-c match deps.lock"

cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release >"$WORK/cmake.log" 2>&1 ||
  { tail -30 "$WORK/cmake.log" >&2; fail "cmake configure ($BUILD_DIR)"; }
cmake --build "$BUILD_DIR" -j2 >"$WORK/build.log" 2>&1 ||
  { grep -n -B 3 -A 4 "error:" "$WORK/build.log" >&2 || true; tail -30 "$WORK/build.log" >&2; fail "cmake build ($BUILD_DIR)"; }
ok "host build ($BUILD_DIR)"

want_version="lmbetter $(sed -n 's/^project(lmbetter VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
[ "$("$HOST" --version)" = "$want_version" ] || fail "--version prints $want_version"
set +e; "$HOST" --version extra >/dev/null 2>&1; code=$?; set -e
[ "$code" = 2 ] || fail "--version with an extra argument exits 2 (got $code)"
ok "--version prints exactly '$want_version'"

"$HOST" --notices >"$WORK/notices.txt" || fail "--notices print"
cmp THIRD_PARTY_NOTICES.md "$WORK/notices.txt" || fail "--notices differs from THIRD_PARTY_NOTICES.md"
ok "--notices prints byte-identical THIRD_PARTY_NOTICES.md"

"$HOST" --dsl-schema --json >"$WORK/embedded-schema.json" || fail "embedded schema print"
cmp schema/lmbetter.dsl.json "$WORK/embedded-schema.json" || fail "embedded schema differs from source bytes"
ok "--dsl-schema --json prints byte-identical embedded schema bytes"
"$HOST" --dsl-schema >"$WORK/schema-summary.txt" || fail "schema summary print"
python3 - "$WORK/schema-summary.txt" <<'PY'
import re,subprocess,sys
summary=open(sys.argv[1]).read()
sigs=subprocess.run(['build/lmbetter','--dsl-signatures'],capture_output=True,text=True).stdout.splitlines()
missing=[]
for line in sigs:
  if line.startswith('method '): needle=line[7:]
  elif line.startswith('fn '): needle=line[3:]
  elif line.startswith('context '): needle=line[8:]
  else: needle=line
  if needle not in summary: missing.append(needle)
assert not missing, missing
print('all', len(sigs), 'registry members listed in --dsl-schema summary')
PY
ok "--dsl-schema summary lists every registry member"
diff -u <("$LMSCRIPT" --dsl-signatures schema/lmbetter.dsl.json) <("$HOST" --dsl-signatures) || fail "host/compiler signatures differ"
cmp schema/lmbetter.host.json schema/lmbetter.dsl.json || fail "generated schema differs from host-only input"
python3 scripts/merge-std.py --host-only schema/lmbetter.dsl.json "$WORK/host-only-schema.json"
cmp schema/lmbetter.dsl.json "$WORK/host-only-schema.json" || fail "embedded schema still declares standard members"
"$HOST" --dsl-signatures >"$WORK/registry-signatures.txt"
"$LMSCRIPT" --dsl-signatures cpp/std/lmcode-std.schema.json >"$WORK/standard-signatures.txt"
python3 - "$WORK" <<'PYHOST'
from pathlib import Path
import sys
work=Path(sys.argv[1])
registry=set((work/'registry-signatures.txt').read_text().splitlines())
standard=set((work/'standard-signatures.txt').read_text().splitlines())
assert len(standard)==181, len(standard)  # POC branch: sdk.lock pins lmcode-cpp d70a12c, matching the POC compiler's implicit std
(work/'host-signatures.txt').write_bytes(''.join(line+'\n' for line in sorted(registry-standard)).encode('utf-8'))
PYHOST
diff -u <("$LMSCRIPT" --dsl-signatures schema/lmbetter.dsl.json --without-std) "$WORK/host-signatures.txt" || fail "host-only registry/compiler signatures differ"
ok "typed registry signatures match compiler schema signatures (full and --without-std)"

# --- parse ---
cat > "$WORK/sample.json" <<'EOF'
{"tool": "lmbetter", "ok": true, "items": [1, null]}
EOF
out="$("$HOST" parse "$WORK/sample.json")"
[ "$out" = '(document (object (pair key: (string (string_content)) value: (string (string_content))) (pair key: (string (string_content)) value: (true)) (pair key: (string (string_content)) value: (array (number) (null)))))' ] \
  || fail "parse sexp: $out"
ok "parse: object/array/true/null sexp"

out="$(printf '{"a": [1, 2]}' | "$HOST" parse -)"
[ "$out" = '(document (object (pair key: (string (string_content)) value: (array (number) (number)))))' ] \
  || fail "parse stdin sexp: $out"
ok "parse: reads stdin with '-'"

printf '{"a": 1,,}' > "$WORK/bad.json"
out="$("$HOST" parse "$WORK/bad.json")"
[ "$out" = '(document (object (pair key: (string (string_content)) value: (number)) (ERROR)))' ] \
  || fail "parse malformed sexp: $out"
ok "parse: malformed input still prints a tree with an ERROR node (exit 0)"

set +e; out="$("$HOST" parse "$WORK/no-such-file" 2>&1)"; code=$?; set -e
[ "$code" = 3 ] && [[ "$out" == *'cannot read'* ]] || fail "missing input file (exit $code): $out"
ok "parse missing file refused (exit 3)"

set +e; "$HOST" parse >/dev/null 2>&1; code=$?; set -e
[ "$code" = 2 ] || fail "parse without arguments exits 2 (got $code)"
ok "parse without arguments refused (exit 2)"

# --- query ---
out="$("$HOST" query '(pair key: (string (string_content) @key))' examples/data/sample.json)"
[ "$out" = 'string_content 1:3-1:7
string_content 2:3-2:10
string_content 3:3-3:5
string_content 4:3-4:12' ] || fail "query captures: $out"
ok "query: captured nodes printed as kind start-end (0-based)"

set +e; out="$("$HOST" query '(pair' examples/data/sample.json 2>&1)"; code=$?; set -e
[ "$code" = 3 ] && [[ "$out" == *'query error at byte offset'*'syntax'* ]] || fail "bad query pattern (exit $code): $out"
ok "query: invalid pattern refused (exit 3, offset + reason)"

out="$("$HOST" query '(pair (string))' examples/data/sample.json)"
[ -z "$out" ] || fail "zero-capture query must print nothing: $out"
ok "query: zero-capture pattern returns an empty list (exit 0)"

set +e; "$HOST" query '(x)' >/dev/null 2>&1; code=$?; set -e
[ "$code" = 2 ] || fail "query without a file argument exits 2 (got $code)"
ok "query without a file argument refused (exit 2)"

# --- --lang c ---
out="$("$HOST" parse --lang c examples/data/sample.c)"
echo "$out" | grep -qF '(translation_unit (preproc_include path: (system_lib_string)) (function_definition type: (primitive_type)' \
  || fail "parse --lang c sexp head: $out"
echo "$out" | grep -qF '(call_expression function: (identifier) arguments: (argument_list (identifier)))' \
  || fail "parse --lang c gets() call: $out"
ok "parse --lang c: translation_unit with preproc_include and calls"

out="$("$HOST" query --lang c '(call_expression function: (identifier) @fn)' examples/data/sample.c)"
[ "$out" = 'identifier 4:2-4:6
identifier 5:2-5:8' ] || fail "query --lang c captures: $out"
ok "query --lang c: called-function identifiers (gets, printf)"

set +e; out="$("$HOST" parse --lang rust examples/data/sample.c 2>&1)"; code=$?; set -e
[ "$code" = 2 ] && [[ "$out" == *"unknown language 'rust' (known: json, c)"* ]] || fail "unknown --lang (exit $code): $out"
ok "unknown --lang refused with the known-languages list (exit 2)"

# --- query predicates (E1) ---
printf '{"pass": 1, "word": 2}' > "$WORK/mini.json"
out="$("$HOST" query '(pair key: (string (string_content) @k) (#eq? @k "pass"))' "$WORK/mini.json")"
[ "$out" = 'string_content 0:2-0:6' ] || fail "predicate eq exact: $out"
ok "query predicate: #eq? exact text match fires"

out="$("$HOST" query '(pair key: (string (string_content) @k) (#eq? @k "pas"))' "$WORK/mini.json")"
[ -z "$out" ] || fail "predicate eq substring must not fire: $out"
ok "query predicate: #eq? substring does not fire"

out="$("$HOST" query '(pair key: (string (string_content) @k) (#not-eq? @k "pass"))' "$WORK/mini.json")"
[ "$out" = 'string_content 0:13-0:17' ] || fail "predicate not-eq: $out"
ok "query predicate: #not-eq? excludes the equal capture"

# upstream tree-sitter evaluates #match? UNANCHORED (regex is_match); authors
# anchor with ^...$ themselves — pin both halves of that contract.
out="$("$HOST" query '(pair key: (string (string_content) @k) (#match? @k "pas"))' "$WORK/mini.json")"
[ "$out" = 'string_content 0:2-0:6' ] || fail "predicate match unanchored: $out"
out="$("$HOST" query '(pair key: (string (string_content) @k) (#match? @k "^pas$"))' "$WORK/mini.json")"
[ -z "$out" ] || fail "predicate match anchored must not fire on 'pass': $out"
ok "query predicate: #match? is unanchored (substring fires, ^...\$ does not)"

out="$("$HOST" query '(pair key: (string (string_content) @k) (#not-match? @k "^pas"))' "$WORK/mini.json")"
[ "$out" = 'string_content 0:13-0:17' ] || fail "predicate not-match: $out"
ok "query predicate: #not-match? excludes regex hits"

out="$("$HOST" query '((string_content) @c (#set! "role" "x"))' "$WORK/mini.json")"
[ "$(grep -c string_content <<<"$out")" = 2 ] || fail "unknown directive must be ignored: $out"
ok "query predicate: unknown directives (#set!) are ignored, match kept"

# --- lint (rule-pack dispatcher) ---
out="$("$HOST" lint examples/rules/c-security.json examples/data/sample.c)"
[ "$out" = '{"row":4,"col":2,"end_row":4,"end_col":6,"rule":"LBC001","severity":"critical","message":"unsafe call '"'"'gets'"'"'"}
1 findings in examples/data/sample.c' ] || fail "lint c-security: $out"
ok "lint: c-security pack over sample.c (query + text_matches)"

out="$("$HOST" lint examples/rules/json-config.json examples/data/bad-config.json)"
[ "$out" = '{"row":2,"col":3,"end_row":2,"end_col":11,"rule":"LBJ001","severity":"critical","message":"password key \"password\""}
{"row":3,"col":13,"end_row":3,"end_col":17,"rule":"LBJ002","severity":"minor","message":"placeholder string TODO"}
2 findings in examples/data/bad-config.json' ] || fail "lint json-config: $out"
ok "lint: json-config pack over bad-config.json (sorted by row, col)"

printf 'int main(void) { return 0; }\n' > "$WORK/clean.c"
out="$("$HOST" lint examples/rules/c-security.json "$WORK/clean.c")"
[ "$out" = "0 findings in $WORK/clean.c" ] || fail "lint clean file: $out"
ok "lint: zero findings still prints the summary line (exit 0)"

printf '{"lang":"c","rules":[{"id":"X","query":"(pair","message":"m"}]}' > "$WORK/badquery.json"
set +e; out="$("$HOST" lint "$WORK/badquery.json" examples/data/sample.c 2>&1)"; code=$?; set -e
[ "$code" = 3 ] && [[ "$out" == *'rule X: query error at byte offset'* ]] || fail "lint bad query (exit $code): $out"
ok "lint: bad query in pack refused (exit 3, rule id + offset)"

printf '{"lang":"c","rules":[{"id":"Y","on":"comment","query":"(comment)","message":"m"}]}' > "$WORK/both.json"
set +e; out="$("$HOST" lint "$WORK/both.json" examples/data/sample.c 2>&1)"; code=$?; set -e
[ "$code" = 2 ] && [[ "$out" == *"rule Y: 'on' and 'query' are mutually exclusive"* ]] || fail "lint on+query (exit $code): $out"
ok "lint: on+query together refused (exit 2)"

printf 'not json' > "$WORK/broken.json"
set +e; out="$("$HOST" lint "$WORK/broken.json" examples/data/sample.c 2>&1)"; code=$?; set -e
[ "$code" = 2 ] && [[ "$out" == *'invalid JSON'* ]] || fail "lint malformed pack (exit $code): $out"
ok "lint: malformed pack refused (exit 2)"

set +e; out="$("$HOST" lint examples/rules/c-security.json "$WORK/no-such.c" 2>&1)"; code=$?; set -e
[ "$code" = 3 ] && [[ "$out" == *'cannot read'* ]] || fail "lint missing file (exit $code): $out"
ok "lint: unreadable file aborts fast (exit 3)"

# --- tier-1 rule packs (canonical library in rules/) ---
"$HOST" lint rules/c-tier1.json tests/fixtures/c-tier1-violations.c >"$WORK/c-tier1.out" || fail "lint c-tier1 violations run"
cat >"$WORK/c-tier1.expected" <<'EXPECTED_C'
{"row":3,"col":0,"end_row":4,"end_col":0,"rule":"LBC007","severity":"minor","message":"object-like macro (prefer const or enum)"}
{"row":5,"col":4,"end_row":5,"end_col":11,"rule":"LBC009","severity":"minor","message":"function name not snake_case: BadName"}
{"row":6,"col":2,"end_row":6,"end_col":10,"rule":"LBC008","severity":"minor","message":"register storage class"}
{"row":8,"col":2,"end_row":8,"end_col":6,"rule":"LBC001","severity":"critical","message":"unsafe call 'gets'"}
{"row":9,"col":2,"end_row":9,"end_col":8,"rule":"LBC001","severity":"critical","message":"unsafe call 'strcat'"}
{"row":10,"col":2,"end_row":10,"end_col":8,"rule":"LBC004","severity":"critical","message":"command exec call 'system'"}
{"row":11,"col":2,"end_row":11,"end_col":7,"rule":"LBC004","severity":"critical","message":"command exec call 'popen'"}
{"row":12,"col":6,"end_row":12,"end_col":11,"rule":"LBC005","severity":"major","message":"assignment in condition (meant ==?)"}
{"row":13,"col":9,"end_row":13,"end_col":14,"rule":"LBC005","severity":"major","message":"assignment in condition (meant ==?)"}
{"row":14,"col":2,"end_row":14,"end_col":12,"rule":"LBC003","severity":"minor","message":"goto used"}
{"row":15,"col":2,"end_row":15,"end_col":19,"rule":"LBC010","severity":"minor","message":"commented-out code"}
{"row":16,"col":2,"end_row":16,"end_col":29,"rule":"LBC002","severity":"minor","message":"unresolved FIXME/TODO comment"}
{"row":20,"col":0,"end_row":22,"end_col":13,"rule":"LBC006","severity":"major","message":"K&R function definition (use ANSI prototypes)"}
13 findings in tests/fixtures/c-tier1-violations.c
EXPECTED_C
cmp "$WORK/c-tier1.expected" "$WORK/c-tier1.out" || { diff "$WORK/c-tier1.expected" "$WORK/c-tier1.out" >&2; fail "c-tier1 violations output"; }
out="$("$HOST" lint rules/c-tier1.json tests/fixtures/c-tier1-clean.c)"
[ "$out" = "0 findings in tests/fixtures/c-tier1-clean.c" ] || fail "c-tier1 clean: $out"
ok "lint: c-tier1 pack, all 10 rules fire on violations, clean fixture silent"

"$HOST" lint rules/json-tier1.json tests/fixtures/json-tier1-violations.json >"$WORK/json-tier1.out" || fail "lint json-tier1 violations run"
cat >"$WORK/json-tier1.expected" <<'EXPECTED_J'
{"row":1,"col":3,"end_row":1,"end_col":14,"rule":"LBJ004","severity":"minor","message":"key not snake_case: ServiceName"}
{"row":3,"col":5,"end_row":3,"end_col":13,"rule":"LBJ001","severity":"critical","message":"credential key \"password\""}
{"row":3,"col":5,"end_row":3,"end_col":13,"rule":"LBJ104","severity":"critical","message":"exact password key"}
{"row":5,"col":3,"end_row":5,"end_col":16,"rule":"LBJ001","severity":"critical","message":"credential key \"password_hint\""}
{"row":6,"col":3,"end_row":6,"end_col":13,"rule":"LBJ001","severity":"critical","message":"credential key \"api_secret\""}
{"row":7,"col":13,"end_row":7,"end_col":17,"rule":"LBJ002","severity":"minor","message":"placeholder string TODO"}
{"row":8,"col":11,"end_row":8,"end_col":15,"rule":"LBJ003","severity":"minor","message":"null value present"}
{"row":9,"col":11,"end_row":9,"end_col":15,"rule":"LBJ005","severity":"minor","message":"number encoded as string: 8080"}
8 findings in tests/fixtures/json-tier1-violations.json
EXPECTED_J
cmp "$WORK/json-tier1.expected" "$WORK/json-tier1.out" || { diff "$WORK/json-tier1.expected" "$WORK/json-tier1.out" >&2; fail "json-tier1 violations output"; }
out="$("$HOST" lint rules/json-tier1.json tests/fixtures/json-tier1-clean.json)"
[ "$out" = "0 findings in tests/fixtures/json-tier1-clean.json" ] || fail "json-tier1 clean: $out"
ok "lint: json-tier1 pack, all 5 rules fire on violations, clean fixture silent"

# --- DSL bytecode ---
cat > "$WORK/smoke.lms" <<EOF
p = _dsl_ cli="lmbetter" install="github:lmctl-ai/lmbetter@latest";
let tree = ts.parse("{\"name\": \"lmbetter\", \"tags\": [\"tree-sitter\", \"json\"]}");
println(tree.sexp());
println("has_error -> " + str(tree.has_error()));
println("nodes -> " + str(tree.node_count()));
end _dsl_
savedsl(p, "$WORK/smoke.dslb");
EOF
"$LMSCRIPT" "$WORK/smoke.lms" "$WORK/smoke.dslb" >/dev/null || fail "smoke compile"
[ "$(od -An -t u1 -j 5 -N 1 "$WORK/smoke.dslb" | tr -d ' ')" = 5 ] || fail "smoke was not compiled as bytecode v5"
out="$("$HOST" --dsl "$WORK/smoke.dslb")"
[ "$(tail -1 <<<"$out")" = '{"ok": true, "result": null}' ] || fail "smoke run: $out"
grep -qxF '(document (object (pair key: (string (string_content)) value: (string (string_content))) (pair key: (string (string_content)) value: (array (string (string_content)) (string (string_content))))))' <<<"$out" || fail "smoke sexp line: $out"
grep -q '^has_error -> false$' <<<"$out" || fail "smoke has_error line: $out"
grep -q '^nodes -> 34$' <<<"$out" || fail "smoke node_count line: $out"
ok "bytecode: ts.parse/sexp/has_error/node_count through the DSL"

cat > "$WORK/error-tree.lms" <<EOF
p = _dsl_ cli="lmbetter" install="github:lmctl-ai/lmbetter@latest";
let tree = ts.parse("{\"a\": 1,,}");
if (!tree.has_error()) { println("bad"); }
println("error flagged");
end _dsl_
savedsl(p, "$WORK/error-tree.dslb");
EOF
"$LMSCRIPT" "$WORK/error-tree.lms" "$WORK/error-tree.dslb" >/dev/null || fail "error-tree compile"
out="$("$HOST" --dsl "$WORK/error-tree.dslb")"
grep -q '^error flagged$' <<<"$out" || fail "malformed JSON must set has_error: $out"
ok "bytecode: malformed JSON flags Tree.has_error"

cat > "$WORK/wrong-arg.lms" <<EOF
p = _dsl_ cli="lmbetter" install="github:lmctl-ai/lmbetter@latest";
ts.parse(42);
end _dsl_
savedsl(p, "$WORK/wrong-arg.dslb");
EOF
set +e; "$LMSCRIPT" "$WORK/wrong-arg.lms" "$WORK/wrong-arg.dslb" >"$WORK/type-error.txt" 2>&1; code=$?; set -e
[ "$code" = 2 ] || fail "wrong-argument call must be a compile error (exit $code)"
ok "type mismatch refused at compile (exit 2)"

cat > "$WORK/oob.lms" <<EOF
p = _dsl_ cli="lmbetter" install="github:lmctl-ai/lmbetter@latest";
let names = str_list().add("a");
names.at(5);
end _dsl_
savedsl(p, "$WORK/oob.dslb");
EOF
"$LMSCRIPT" "$WORK/oob.lms" "$WORK/oob.dslb" >/dev/null || fail "compile"
set +e; out="$("$HOST" --dsl "$WORK/oob.dslb" 2>&1)"; code=$?; set -e
[ "$code" = 3 ] && [[ "$out" == *'out of range'* ]] || fail "StrList.at bounds (exit $code): $out"
ok "StrList.at bounds refused at runtime (exit 3)"

cat > "$WORK/root-parent.lms" <<EOF
p = _dsl_ cli="lmbetter" install="github:lmctl-ai/lmbetter@latest";
let r = ts.parse("{}").root();
if (r.has_parent()) { println("bad"); }
r.parent();
end _dsl_
savedsl(p, "$WORK/root-parent.dslb");
EOF
"$LMSCRIPT" "$WORK/root-parent.lms" "$WORK/root-parent.dslb" >/dev/null || fail "root-parent compile"
set +e; out="$("$HOST" --dsl "$WORK/root-parent.dslb" 2>&1)"; code=$?; set -e
[ "$code" = 3 ] && [[ "$out" == *'root node has no parent'* ]] || fail "Node.parent on root (exit $code): $out"
ok "Node.parent: has_parent() false at root, parent() on root refused (exit 3)"

set +e; out="$("$HOST" 2>&1)"; code=$?; set -e
[ "$code" = 2 ] && [[ "$out" == *'usage: lmbetter'* ]] || fail "bad args refusal (exit $code): $out"
ok "bad arguments refused (exit 2)"
set +e; out="$("$HOST" --dsl "$WORK/no-such.dslb" 2>&1)"; code=$?; set -e
[ "$code" = 3 ] && [[ "$out" == *'"phase": "load"'* ]] || fail "missing bytecode refusal (exit $code): $out"
ok "missing bytecode file refused at load (exit 3)"

"$HOST" --dsl "$WORK/smoke.dslb" "$WORK/error-tree.dslb" >"$WORK/multi.jsonl" || fail "multi-bytecode run"
[ "$(grep -c '^{"ok": true, "result": null}$' "$WORK/multi.jsonl")" = 2 ] || fail "multiple bytecode files did not run in order"
ok "multiple bytecode files run sequentially"

# --- examples compile and run ---
mkdir -p build
for f in parse-string parse-file detect-error cst-rules lint-json report-json query-json lint-c rule-catalog suppress-findings sarif-report; do
  "$LMSCRIPT" "examples/$f.lms" "build/$f.dslb" >"$WORK/$f.compile.log" 2>&1 || fail "$f example compile"
  "$HOST" --dsl "build/$f.dslb" >"$WORK/$f.out" || fail "$f example run"
  # every '# expect:' line must appear in the program output
  while IFS= read -r line; do
    grep -qxF "${line:2}" "$WORK/$f.out" || fail "$f example output missing: ${line:2} (got $(cat "$WORK/$f.out"))"
  done < <(grep '^#|' "examples/$f.lms")
done
ok "examples compile and their expected output matches"

# --- SARIF artifact (written by examples/sarif-report.lms above) ---
python3 - <<'PY'
import json
doc = json.load(open("build/sarif-results.sarif"))
assert doc["version"] == "2.1.0", doc["version"]
run = doc["runs"][0]
assert run["tool"]["driver"]["name"] == "lmbetter"
assert [r["id"] for r in run["tool"]["driver"]["rules"]] == ["LBC001", "LBC002"]
results = run["results"]
assert len(results) == 4, len(results)
region = results[0]["locations"][0]["physicalLocation"]["region"]
# first finding is the gets() call at 0-based row 5 col 2 -> 1-based 6:3
assert (region["startLine"], region["startColumn"]) == (6, 3), region
assert results[0]["level"] == "error" and results[-1]["level"] == "note"
assert results[-1]["locations"][0]["physicalLocation"]["artifactLocation"]["uri"] == "src/sample.c"
print("SARIF 2.1.0: 4 results, regions and levels correct")
PY
ok "SARIF artifact is valid JSON with the expected structure"

# --- lint-repo.lms (outer lmscript driving the CLI dispatcher) ---
"$LMSCRIPT" examples/lint-repo.lms >"$WORK/lint-repo.out" 2>"$WORK/lint-repo.err" ||
  { cat "$WORK/lint-repo.err" >&2; fail "lint-repo example run"; }
while IFS= read -r line; do
  grep -qxF "${line:2}" "$WORK/lint-repo.out" || fail "lint-repo output missing: ${line:2} (got $(cat "$WORK/lint-repo.out"))"
done < <(grep '^#|' examples/lint-repo.lms)
ok "lint-repo: lmscript drives lmbetter lint and counts findings"

# --- rules/lmcode (lmcode-native tier-1 rules) ---
mkdir -p build/rules-lmcode
for f in c-function-metrics c-switch-style json-duplicate-keys json-size-limits c-nesting-depth json-nesting-depth c-scanf-width c-symbol-rules; do
  "$LMSCRIPT" "rules/lmcode/$f.lms" "build/rules-lmcode/$f.dslb" >"$WORK/$f.compile.log" 2>&1 || fail "$f rule compile"
  "$HOST" --dsl "build/rules-lmcode/$f.dslb" >"$WORK/$f.out" || fail "$f rule run"
  while IFS= read -r line; do
    grep -qxF "${line:2}" "$WORK/$f.out" || fail "$f output missing: ${line:2} (got $(cat "$WORK/$f.out"))"
  done < <(grep '^#|' "rules/lmcode/$f.lms")
  # clean-fixture variant: substitute the fixture path AND the savedsl target
  # into a scratch copy, expect an empty findings array and a zero count
  sed -e 's/-violations\./-clean./' -e "s|build/rules-lmcode/$f.dslb|$WORK/$f-clean.dslb|" \
    "rules/lmcode/$f.lms" > "$WORK/$f-clean.lms"
  "$LMSCRIPT" "$WORK/$f-clean.lms" "$WORK/$f-clean.dslb" >/dev/null 2>&1 || fail "$f clean compile"
  out="$("$HOST" --dsl "$WORK/$f-clean.dslb")"
  grep -qxF '[]' <<<"$out" || fail "$f clean fixture findings: $out"
  grep -qxF '0 findings' <<<"$out" || fail "$f clean fixture count: $out"
done
ok "rules/lmcode: eight rule files fire on violations fixtures, silent on clean"

if [ "$UNAME_S" = "Darwin" ]; then
  BAD_LIBS="$(otool -L "$HOST" | tail -n +2 | awk '{print $1}' | grep -v '^/usr/lib/\|^/System/Library/' || true)"
  [ -z "$BAD_LIBS" ] || fail "host links non-system libraries: $BAD_LIBS"
  ok "host links only system libraries ($(stat_size "$HOST") bytes, otool -L clean)"
elif [[ "$UNAME_S" == MINGW* || "$UNAME_S" == MSYS* || "$UNAME_S" == CYGWIN* ]]; then
  # Windows binaries always reference system DLLs; the MinGW link is otherwise
  # fully static (-static -static-libstdc++ -static-libgcc, see CMakeLists).
  [ -f "$HOST" ] || fail "host executable missing: $HOST"
  ok "host runs on Windows ($(stat_size "$HOST") bytes, static C++ runtime)"
else
  LDD_OUT="$(ldd "$HOST" 2>&1 || true)"
  [[ "$LDD_OUT" == *'not a dynamic executable'* ]] || fail "host is not fully static: $LDD_OUT"
  ok "host is fully static ($(stat_size "$HOST") bytes, ldd clean)"
fi

echo "PASS: $pass checks"
