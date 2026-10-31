# Real `$return` compiler fixtures

These lmcode bytecode (DSLB v6) programs were generated, not patched, using Luna's prepared
slice-3 compiler at mono commit `a9df5a8b793ddc90f572d5c5be5800ba8dc448dc`.
The compiler build was read only. Canonical SDK: `b8615eda9fc56cd19eb5ffef62cd05e0d78cd977`.
`fixtures.lock` pins source and binary bytes; the host test additionally checks
each binary's actual v6 schema digest against the generated lmok schema.

The body fixture executes `0.1 + 0.2`; entry fixtures exercise ownership, retained
body state, repeated node/file dispatch, finding IDs, and errors after writes.
These are separate from the dynamically compiled ordinary-import tests: the
installed compiler need not understand `$return` until compiler slice 3 lands.

Explicit regeneration from the repo root:

```sh
RETURN_COMPILER_BIN=/absolute/path/to/prepared/lmscript python3 tests/fixtures/returns/generate.py
```

Do not regenerate expected values from host output. Update binary/source locks
only after reviewing a changed source or canonical schema. The ordinary
`./test.sh` gate executes the pinned bytecode and never silently skips it.
