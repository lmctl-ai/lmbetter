# Build, run, and test

- Sync sources: `scripts/sync-deps.sh` (tree-sitter + tree-sitter-json into
  `third_party/`, sha256-verified, idempotent; `--verify` checks the pins).
- Vendor SDK: `scripts/sync-sdk.sh` needs an lmcode-cpp checkout
  (`LMCODE_CPP=/path/to/lmcode-cpp`); `sdk.lock` then pins every vendored
  file. `scripts/sync-sdk.sh --verify` checks the pins.
- Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build`.
  CMake copies `schema/lmbetterp.host.json` byte-exact to git-ignored
  `schema/lmbetterp.dsl.json` and embeds it plus `THIRD_PARTY_NOTICES.md`.
- Test: `./test.sh` runs 22 checks (pin verifies, build, `--version`,
  `--notices` byte-compare, embedded-schema byte-compare, full and
  `--without-std` signature parity with `lmscript >= 0.2.420`, CLI parse
  cases, DSL bytecode runs, examples, static-link check). Put `build/` on
  `PATH` before compiling examples by hand, otherwise the compiler tries to
  resolve `install="github:lmctl-ai/lmbetterp@latest"` over the network.
- Full static is the default on GNU/Linux (`LMBETTERP_FULL_STATIC=ON`);
  macOS links only system libraries (no `-static` exists there).

## Adding an example

1. Create `examples/<name>.lms` using `_dsl_ cli="lmbetterp" install="github:lmctl-ai/lmbetterp@latest"`.
2. Compile and run: `PATH=$PWD/build:$PATH lmscript examples/<name>.lms build/<name>.dslb && ./build/lmbetterp --dsl build/<name>.dslb`.
3. Add expected output assertions (`#| <expected>` header lines) for `./test.sh`.

## Adding a host function/method

1. Declare in `schema/lmbetterp.host.json` (CMake copies exact host-only bytes
   to `schema/lmbetterp.dsl.json`; `lmscript >= 0.2.420` supplies std implicitly).
2. Implement in `cpp/host.cpp` (tree-sitter headers stay in that one TU) and
   register in `cpp/main.cpp` via `dslvm::Registry`.
3. Verify `./test.sh` passes and schema signatures match the compiler.

## Releases

Binaries are built by the `ci` workflow (workflow_dispatch only) and published
by force-updating the rolling `dist` orphan branch on main; there is no
release job.
