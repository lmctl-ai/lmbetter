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
ok "third_party/ tree-sitter and tree-sitter-json match deps.lock"

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
for f in parse-string parse-file detect-error cst-rules; do
  "$LMSCRIPT" "examples/$f.lms" "build/$f.dslb" >"$WORK/$f.compile.log" 2>&1 || fail "$f example compile"
  "$HOST" --dsl "build/$f.dslb" >"$WORK/$f.out" || fail "$f example run"
  # every '# expect:' line must appear in the program output
  while IFS= read -r line; do
    grep -qxF "${line:2}" "$WORK/$f.out" || fail "$f example output missing: ${line:2} (got $(cat "$WORK/$f.out"))"
  done < <(grep '^#|' "examples/$f.lms")
done
ok "examples compile and their expected output matches"

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
