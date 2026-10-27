#!/usr/bin/env bash
# Download the pinned tree-sitter core and tree-sitter-json grammar tarballs
# and unpack them into third_party/ (git-ignored). Idempotent; --verify only
# checks the pins.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCK="$ROOT/deps.lock"
DEST="$ROOT/third_party"
mkdir -p "$DEST"
sha256() { if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1"; else shasum -a 256 "$1"; fi | awk '{print $1}'; }
fetch_one() {
  local name="$1" url="$2" want="$3" archive="$4" dir="$5"
  local tarball="$DEST/.${archive}"
  if [[ ! -f "$tarball" || "$(sha256 "$tarball")" != "$want" ]]; then
    timeout 300 curl --fail --location --retry 3 --retry-delay 2 "$url" -o "$tarball"
  fi
  [[ "$(sha256 "$tarball")" == "$want" ]] || { echo "$name archive hash mismatch" >&2; exit 1; }
  if [[ -f "$DEST/$dir/.lmbetterp-archive-sha256" && "$(cat "$DEST/$dir/.lmbetterp-archive-sha256")" == "$want" ]]; then return; fi
  rm -rf "$DEST/$dir"
  mkdir -p "$DEST/$dir"
  tar -xzf "$tarball" -C "$DEST/$dir" --strip-components=1
  printf '%s\n' "$want" > "$DEST/$dir/.lmbetterp-archive-sha256"
}
get() { sed -n "s/^$1\.$2=//p" "$LOCK"; }
if [[ "${1:-}" == "--verify" ]]; then
  for spec in \
    'tree_sitter tree-sitter-0.27.1.tar.gz tree-sitter' \
    'tree_sitter_json tree-sitter-json-254c42a.tar.gz tree-sitter-json'; do
    read -r name archive dir <<<"$spec"
    [[ -f "$DEST/$dir/.lmbetterp-archive-sha256" ]] || { echo "$name missing; run scripts/sync-deps.sh" >&2; exit 1; }
    [[ -f "$DEST/.$archive" && "$(sha256 "$DEST/.$archive")" == "$(get "$name" sha256)" ]] || { echo "$name cached archive differs from deps.lock" >&2; exit 1; }
    [[ "$(cat "$DEST/$dir/.lmbetterp-archive-sha256")" == "$(get "$name" sha256)" ]] || { echo "$name pin mismatch" >&2; exit 1; }
  done
  echo "lmbetterp third-party archives match deps.lock"
  exit 0
fi
fetch_one tree_sitter "$(get tree_sitter url)" "$(get tree_sitter sha256)" tree-sitter-0.27.1.tar.gz tree-sitter
fetch_one tree_sitter_json "$(get tree_sitter_json url)" "$(get tree_sitter_json sha256)" tree-sitter-json-254c42a.tar.gz tree-sitter-json
echo "synced pinned lmbetterp dependencies"
