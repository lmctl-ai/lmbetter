# lmbetter durable memory

Technical documentation and operational notes for `lmbetter`.
`lmbetter` embeds tree-sitter 0.27.1 and the tree-sitter-json grammar as a standalone CLI host for lmscript.
Repository: https://github.com/lmctl-ai/lmbetter.

## Document map

| File | Contents |
| --- | --- |
| [01-purpose-and-place.md](01-purpose-and-place.md) | What lmbetter is, the pinned parts, the minimal DSL surface |
| [02-build-run-test.md](02-build-run-test.md) | Build, run, and test commands; adding examples and functions |
| [03-rule-methods.md](03-rule-methods.md) | Rule methods: @annotation + implements Rule, Node navigation, limitations |
| [04-dispatcher.md](04-dispatcher.md) | lint rule packs, the kind-indexed walk dispatcher, v6 entry-point trajectory |
| [05-rule-tiers.md](05-rule-tiers.md) | Three-tier rule strategy, shipped packs, dropped candidates, tier-2 plan |
