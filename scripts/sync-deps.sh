#!/usr/bin/env bash
# Download the pinned tree-sitter core and grammar tarballs and unpack them
# into third_party/ (git-ignored). Idempotent; --verify only checks the pins.
#
# Trust model: deps.lock pins the SHA-256 of each upstream archive (trust on
# first use — tree-sitter publishes no signed checksums, and GitHub-generated
# archives are not contractually byte-stable, so a pin rotting is a loud
# failure here, never a silent substitution). Each extracted tree carries two
# markers: the archive hash it came from and a digest of the extracted files,
# so --verify catches a locally edited third_party/ as well as a stale
# archive.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCK="$ROOT/deps.lock"
DEST="$ROOT/third_party"
mkdir -p "$DEST"
if command -v sha256sum >/dev/null 2>&1; then SHA_CMD=sha256sum; else SHA_CMD="shasum -a 256"; fi
sha256() { $SHA_CMD "$1" | awk '{print $1}'; }
# Digest of every regular file under a tree (sorted paths, hash+name lines,
# hashed again). Excludes our own marker files.
tree_digest() {
  (cd "$1" && find . -type f ! -name '.lmbetter-*' -print | LC_ALL=C sort | tr '\n' '\0' | xargs -0 $SHA_CMD | $SHA_CMD | awk '{print $1}')
}
# GNU timeout does not exist on stock macOS (coreutils installs gtimeout).
if command -v timeout >/dev/null 2>&1; then with_timeout() { timeout "$@"; }
elif command -v gtimeout >/dev/null 2>&1; then with_timeout() { gtimeout "$@"; }
else with_timeout() { shift; "$@"; }
fi
fetch_one() {
  local name="$1" url="$2" want="$3" archive="$4" dir="$5"
  local tarball="$DEST/.${archive}"
  if [[ ! -f "$tarball" || "$(sha256 "$tarball")" != "$want" ]]; then
    # Download to a side file and move into place only when complete, so an
    # interrupted transfer never masquerades as the cached archive.
    rm -f "$tarball.part"
    with_timeout 300 curl --fail --location --proto '=https' --retry 3 --retry-delay 2 "$url" -o "$tarball.part"
    mv -f "$tarball.part" "$tarball"
  fi
  [[ "$(sha256 "$tarball")" == "$want" ]] || { echo "$name archive hash mismatch" >&2; exit 1; }
  if [[ -f "$DEST/$dir/.lmbetter-archive-sha256" && "$(cat "$DEST/$dir/.lmbetter-archive-sha256")" == "$want" \
        && -f "$DEST/$dir/.lmbetter-tree-sha256" ]]; then return; fi
  rm -rf "$DEST/$dir"
  mkdir -p "$DEST/$dir"
  tar -xzf "$tarball" -C "$DEST/$dir" --strip-components=1
  printf '%s\n' "$want" > "$DEST/$dir/.lmbetter-archive-sha256"
  tree_digest "$DEST/$dir" > "$DEST/$dir/.lmbetter-tree-sha256"
}
get() { sed -n "s/^$1\.$2=//p" "$LOCK"; }
if [[ "${1:-}" == "--verify" ]]; then
  for spec in \
    'tree_sitter tree-sitter-0.27.1.tar.gz tree-sitter' \
    'tree_sitter_json tree-sitter-json-254c42a.tar.gz tree-sitter-json' \
    'tree_sitter_c tree-sitter-c-b780e47.tar.gz tree-sitter-c' \
    'tree_sitter_python tree-sitter-python-0.23.6.tar.gz tree-sitter-python' \
    'tree_sitter_bash tree-sitter-bash-0.25.1.tar.gz tree-sitter-bash'; do
    read -r name archive dir <<<"$spec"
    [[ -f "$DEST/$dir/.lmbetter-archive-sha256" && -f "$DEST/$dir/.lmbetter-tree-sha256" ]] || { echo "$name missing or synced by an older script; run scripts/sync-deps.sh" >&2; exit 1; }
    [[ -f "$DEST/.$archive" && "$(sha256 "$DEST/.$archive")" == "$(get "$name" sha256)" ]] || { echo "$name cached archive differs from deps.lock" >&2; exit 1; }
    [[ "$(cat "$DEST/$dir/.lmbetter-archive-sha256")" == "$(get "$name" sha256)" ]] || { echo "$name pin mismatch" >&2; exit 1; }
    [[ "$(tree_digest "$DEST/$dir")" == "$(cat "$DEST/$dir/.lmbetter-tree-sha256")" ]] || { echo "$name extracted sources differ from the archive (edited third_party/$dir?); run scripts/sync-deps.sh after removing it" >&2; exit 1; }
  done
  echo "lmbetter third-party archives match deps.lock"
  exit 0
fi
fetch_one tree_sitter "$(get tree_sitter url)" "$(get tree_sitter sha256)" tree-sitter-0.27.1.tar.gz tree-sitter
fetch_one tree_sitter_json "$(get tree_sitter_json url)" "$(get tree_sitter_json sha256)" tree-sitter-json-254c42a.tar.gz tree-sitter-json
fetch_one tree_sitter_c "$(get tree_sitter_c url)" "$(get tree_sitter_c sha256)" tree-sitter-c-b780e47.tar.gz tree-sitter-c
fetch_one tree_sitter_python "$(get tree_sitter_python url)" "$(get tree_sitter_python sha256)" tree-sitter-python-0.23.6.tar.gz tree-sitter-python
fetch_one tree_sitter_bash "$(get tree_sitter_bash url)" "$(get tree_sitter_bash sha256)" tree-sitter-bash-0.25.1.tar.gz tree-sitter-bash
echo "synced pinned lmbetter dependencies"
