# lmok durable memory

Technical documentation and operational notes for `lmok`.
`lmok` embeds tree-sitter 0.27.1 with JSON, C, Python, Bash and Go grammars as a standalone CLI host for lmscript.
Repository: https://github.com/lmctl-ai/lmok.

## Document map

| File | Contents |
| --- | --- |
| [01-purpose-and-place.md](01-purpose-and-place.md) | What lmok is, the pinned parts, the minimal DSL surface |
| [02-build-run-test.md](02-build-run-test.md) | Build, run, and test commands; adding examples and functions |
| [03-rule-methods.md](03-rule-methods.md) | Rule methods: @annotation + implements Rule, Node navigation, limitations |
| [04-dispatcher.md](04-dispatcher.md) | lint rule packs, the kind-indexed walk dispatcher, v6 entry-point trajectory |
| [05-rule-tiers.md](05-rule-tiers.md) | Three-tier rule strategy, shipped packs, dropped candidates, tier-2 plan |
| [06-realworld-validation.md](06-realworld-validation.md) | c-tier1 validated against real-world C codebases (sds, kilo, cJSON) |
| [07-robustness.md](07-robustness.md) | Bounded queries/regexes, input hardening, finding output escaping |
| [08-v6-entry-dispatch.md](08-v6-entry-dispatch.md) | v6 lmcode entry loading, CST dispatch, findings and CLI contract |
| [09-project-ci.md](09-project-ci.md) | .lmokignore, inline suppression, project config, and `lmok ci` |
| [09-facts-layer.md](09-facts-layer.md) | E6 facts extraction: facts.scm vocabulary, query_matches, scope chains, confidence, known edges |
| [10-lint-db-findings.md](10-lint-db-findings.md) | lint --db run recording + supersession, findings verb, DB-create/side-channel decisions |
| [11-pattern-rules.md](11-pattern-rules.md) | Code-shaped (semgrep-style) patterns compiled to queries: `query --pattern`, pack `pattern` fields, `ts.match_*`, corner cases |

| [12-autofix.md](12-autofix.md) | Whole-node fix metadata, verified edits and unified previews |

Operational name: **lmok** (binary, schemas, CMake and install paths). Tier-3
proposal documents are owned by the other team and were intentionally retained
verbatim; old product names there are historical, not current CLI instructions.

- [Rule metadata and pack-author tests](12-rule-contracts.md)
