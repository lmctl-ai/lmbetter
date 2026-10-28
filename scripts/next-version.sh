#!/usr/bin/env bash
# Print the version (no leading v) this build should carry and release as.
#   - on a v* tag: the tag itself (GITHUB_REF_NAME=vX.Y.Z)
#   - otherwise: the larger of CMakeLists' project VERSION and (latest v* tag + 1 patch)
# CI writes the result into CMakeLists.txt in the ephemeral workspace before building,
# so `lmok --version` equals the release tag (`lmscript install ...@latest` checks that).
# LMOK_TAGS (newline separated tag names) overrides the `git ls-remote` lookup for tests.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
if [ "${GITHUB_REF_TYPE:-}" = tag ]; then echo "${GITHUB_REF_NAME#v}"; exit 0; fi
base="$(sed -n 's/^project(lmok VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
if [ -n "${LMOK_TAGS+x}" ]; then tags="$LMOK_TAGS"
else tags="$(git ls-remote --tags --refs origin 'v*' | sed 's#.*refs/tags/##')"; fi
latest="$(printf '%s\n' "$tags" | sed -n 's/^v\([0-9][0-9.]*\)$/\1/p' | sort -V | tail -1)"
if [ -z "$latest" ] || [ "$(printf '%s\n%s\n' "$base" "$latest" | sort -V | tail -1)" = "$base" ] && [ "$base" != "$latest" ]; then
  echo "$base"
else
  echo "${latest%.*}.$(( ${latest##*.} + 1 ))"
fi
