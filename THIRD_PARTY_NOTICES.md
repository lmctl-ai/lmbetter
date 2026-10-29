# Third-Party Notices

## tree-sitter

This repository builds against, and statically links, **tree-sitter** — an
incremental parsing library.

- Upstream: https://tree-sitter.github.io / https://github.com/tree-sitter/tree-sitter
- Version: 0.27.1 (v0.27.1), pinned in `deps.lock`
- License: **MIT License**
- Copyright (c) 2018 Max Brunsfeld and contributors
- Full license text: the upstream `LICENSE` file, vendored at
  `third_party/tree-sitter/LICENSE` after running `scripts/sync-deps.sh`

## tree-sitter-json

The JSON grammar compiled into the executable (`third_party/tree-sitter-json`).

- Upstream: https://github.com/tree-sitter/tree-sitter-json
- Revision: 254c42a6476413b776221e03982ac8ae159eeb72, pinned in `deps.lock`
- License: **MIT License**
- Copyright (c) 2014 Max Brunsfeld and contributors
- Full license text: the upstream `LICENSE` file, vendored at
  `third_party/tree-sitter-json/LICENSE` after running `scripts/sync-deps.sh`

## tree-sitter-c

The C grammar compiled into the executable (`third_party/tree-sitter-c`).

- Upstream: https://github.com/tree-sitter/tree-sitter-c
- Revision: b780e47fc780ddc8da13afa35a3f4ed5c157823d (master), pinned in
  `deps.lock`
- License: **MIT License**
- Copyright (c) 2014 Max Brunsfeld and contributors
- Full license text: the upstream `LICENSE` file, vendored at
  `third_party/tree-sitter-c/LICENSE` after running `scripts/sync-deps.sh`

## lmcode-cpp SDK

`cpp/dslvm.hpp`, the `cpp/stdlib/*.hpp` headers, and
`cpp/std/lmcode-std.schema.json` are vendored from lmcode-cpp (MIT License,
Lattice Systems LLC), pinned by `sdk.lock` and synced by
`scripts/sync-sdk.sh`.

## SQLite

The SQLite 3.54.1 amalgamation is compiled into the executable with FTS5.

- Upstream: https://www.sqlite.org/
- Source: https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip
- License: public domain (https://www.sqlite.org/copyright.html)
- Archive SHA-256 pinned in `deps.lock`; source at `third_party/sqlite/`.
