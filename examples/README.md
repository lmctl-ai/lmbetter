# Examples

Runnable lmscript programs, one per scenario. Each compiles to bytecode and
runs against the `lmbetter` host:

```sh
lmscript examples/parse-string.lms build/parse-string.dslb
lmbetter --dsl build/parse-string.dslb
```

The `install="github:lmctl-ai/lmbetter@latest"` line in each script pins the
host CLI the same way lmscript resolves any other tool.

- `parse-string.lms` — parse an inline JSON string, print the tree
- `parse-file.lms` — read `examples/data/sample.json`, print tree stats
- `detect-error.lms` — parse malformed JSON, flag the ERROR node

`./test.sh` compiles and runs every example as part of the check suite.
