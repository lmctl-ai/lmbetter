# Rule tiers

lmbetter is deliberately a simple CLI, not a platform. Rules are classified by
what information they need; the tier decides when (or whether) they ship.

## The membership test

**What information does the rule need?**

- **Tier 1 — CST shape + text only.** A tree-sitter query pattern, a node kind
  plus a text regex, or a small lmcode predicate over `Node` suffices.
  Implement now (rule packs, or lmcode rules where policy belongs to users).
- **Tier 2 — CST plus one named mechanical extension.** The rule is
  syntactically clear but needs a capability the host does not have yet
  (query predicates, upward navigation, a symbol index, multi-file scope,
  incremental re-parse). Each extension is named (E1–E5 below) and planned,
  then implemented.
- **Tier 3 — real semantics.** Type resolution, control-flow graphs,
  dataflow, taint, symbolic execution. Deferred; tree-sitter alone cannot
  provide this, and faking it with regexes produces false-positive noise.

## Tier 1 — shipped (rules/)

### C (`rules/c-tier1.json`, 10 rules)

| id | rule | mechanism | reference |
| --- | --- | --- | --- |
| LBC001 | unsafe calls `gets`/`strcpy`/`sprintf`/`strcat` | query `(call_expression function: (identifier) @fn)` + anchored regex | CWE-242 |
| LBC002 | FIXME/TODO comments | `on: comment` + regex `FIXME\|TODO` | cf. SonarQube S1135 |
| LBC003 | `goto` used | `on: goto_statement` | cf. MISRA C:2012 15.1 |
| LBC004 | command-exec calls `system`/`popen`/`execl*`/`execvp` | same query as LBC001 + anchored regex | CWE-78 |
| LBC005 | assignment inside `if`/`while` condition | two query patterns through `parenthesized_expression` | CERT EXP45-C; cf. MISRA C:2012 13.4 |
| LBC006 | K&R function definition | query `((function_definition (declaration)) @kr)` (a definition with direct `declaration` children) | — |
| LBC007 | object-like `#define` macro | `on: preproc_def` | — |
| LBC008 | `register` storage class | `on: storage_class_specifier` + regex `^register$` | — |
| LBC009 | function name not snake_case | query function declarators + regex `[A-Z]` | — (convention rule) |
| LBC010 | commented-out code | `on: comment` + heuristic regex `(=.*;)\|(;\s*$)` | cf. SonarQube S125 |

### JSON (`rules/json-tier1.json`, 5 rules)

| id | rule | mechanism |
| --- | --- | --- |
| LBJ001 | credential keys (`password`/`secret`) | query pair key `string_content` + regex |
| LBJ002 | placeholder strings (`TODO`/`FIXME`/`CHANGEME`/`XXX`) | query string content + anchored regex |
| LBJ003 | null value present | `on: null` |
| LBJ004 | key not snake_case | query pair key + regex `[A-Z]` |
| LBJ005 | number encoded as string (`"port": "8080"`) | query pair value string + regex `^[0-9]+$` |

### Further tier-1 candidates (not shipped, judged pack/script-able)

- multiple assignment (`a = b = 0`): nested `assignment_expression` query
- empty `if`/`while` body (`;`): `on: expression_statement` + text `;` guard,
  or a query for empty consequences
- `malloc` result cast to a pointer type: `cast_expression` + regex

### Shipped as lmcode rules (rules/lmcode/) — tier 1 with logic

| id | rule | mechanism |
| --- | --- | --- |
| LBC101 | function too long (>60 rows) | `@function_definition` + `end_row-start_row` arithmetic |
| LBC102 | too many parameters (>7) | child-walk count of `parameter_declaration` |
| LBC103 | switch without default | child-walk; default = `case_statement` text prefix |
| LBC104 | if without braces | consequence child kind != `compound_statement` |
| LBJ101 | duplicate keys in one object | `@object` + per-object StrMap over pair keys |
| LBJ102 | object too large (>50 entries) | pair-child counting |
| LBC105 | control structure nested >4 deep | parent() chain walk (needs E2) |
| LBJ103 | JSON value nested >6 levels deep | parent() chain walk (needs E2) |
| LBC106 | `scanf`/`fscanf` without field width | query + `#match?` predicate selecting the callee (needs E1); width heuristic in lmcode |
| LBJ104 | key literally `password` (exact, no `password_hint` false positive) | query + `#eq?` predicate (needs E1) |
| LBC107 | static function never called | two queries + StrMap set difference in lmcode (no host index) |
| LBC108 | call to undefined function (same-file) | call set minus (defined + script-side libc whitelist) |
| LBC109 | direct recursion | per-definition subtree query for self-calls |

(LBC001/LBC004 moved their callee regexes from pack `text_matches` into the
query as `#match?` predicates — same behavior, sharper form.)

## Tier 2 — needs a named extension

- **E1: host-side evaluation of query predicates — LANDED (v0.3.3).**
  `#eq?`/`#not-eq?` (capture text vs literal or another capture) and
  `#match?`/`#not-match?` are evaluated per match in the single query path
  (`node_query`), so DSL `ts.query*`, pack `query` rules, and the CLI `query`
  subcommand all share it. Semantics follow upstream
  (`satisfies_text_predicates` in the Rust binding): text equality for
  `#eq?`, and **unanchored** regex for `#match?` (upstream uses
  `regex::is_match`; the "anchored" convention is that query AUTHORS write
  `^...$` — verified against the upstream docs' own examples). Unknown
  predicates/directives (`#set!`, `#is?`, `any-of?`, `any-*`) are ignored
  like the tree-sitter CLI. Shipped with it: LBC106, LBJ104; LBC001/LBC004
  upgraded to predicate form. Note: pack `text_matches` is now partly
  redundant with `#match?` for query rules — kept, since it still applies to
  `on`-kind rules.
- **E2: `Node.parent()` upward navigation — LANDED (v0.3.2).** Contract:
  `Node.has_parent() -> bool` (false at root) and `Node.parent() -> Node`
  (root call is a located run-time error, exit 3 — no fake self-node).
  Shipped with it: LBC105 (control nesting depth), LBJ103 (JSON nesting
  depth). Remaining ideas in this extension's orbit: context-sensitive rules
  ("identifier X only inside function Y"), assignment-in-loop-body (not just
  conditions), commented-out-code refinement (comment inside a function body
  vs. file top level).
- **E3: same-file symbol index — RE-SCOPED (v0.3.4) after reality check.**
  The symbol-ish rules it was meant to unlock turned out to be expressible in
  pure lmcode today: two queries + StrMap set algebra (see
  `rules/lmcode/c-symbol-rules.lms`: LBC107 unused static function, LBC108
  undefined call, LBC109 direct recursion). No host index was needed. E3
  remains only as a PERFORMANCE optimization for very large files (the
  script-side approach is O(defs × subtree) for recursion) and as an E4
  adjunct. Genuinely still impossible script-side: cross-file references,
  function-pointer/callback targets, typedef-aware name resolution (those are
  E4/tier-3 territory). Conservative choices shipped: LBC107 flags STATIC
  functions only (their calls must be same-file); LBC108 considers plain
  identifier callees only and uses a script-owned libc whitelist.
- **E4: multi-file / directory lint + include resolution.** Unlocks:
  project-wide pack runs (`lmbetter lint rules/ src/`); unused-include
  hygiene; cross-file duplicate symbols; the cross-file half of symbol rules.
- **E5: incremental re-parse.** Unlocks: watch/editor-mode lint (re-lint on
  save reusing unchanged subtrees); pre-commit speedup on large trees.

## Tier 3 — deferred (real semantics)

- Type-aware rules (wrong argument types, format-string type mismatches):
  need a type system and symbol resolution.
- Null dereference: needs CFG construction and path-sensitive analysis.
- Taint / injection tracking (user input → `system`): needs dataflow.
- Dead store (assigned value never read): needs dataflow liveness.
- Use-after-free / double-free: needs alias and lifetime analysis.
- Full MISRA/CERT conformance: many rules need types, CFG, or translation-
  unit-wide semantics — only the syntactic subset lands in tier 1/2.
