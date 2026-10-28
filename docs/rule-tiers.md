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
- JSON: duplicate keys in one object — scriptable in lmcode (StrMap over
  sibling keys), not expressible as a single pack rule

## Tier 2 — needs a named extension

- **E1: host-side evaluation of query predicates (`#eq?`, `#match?`).**
  Unlocks: case-insensitive credential-key matching (`(#match? @key
  "(?i)password")`); value-aware rules (key equals "password" AND value
  empty); "call X with literal argument Y" without a post-regex.
- **E2: `Node.parent()` upward navigation.** Unlocks: nesting-depth rules
  (walk up to count ancestors); context-sensitive rules ("identifier X only
  inside function Y"); assignment-in-loop-body (not just conditions);
  commented-out-code refinement (comment inside a function body vs. file
  top level).
- **E3: same-file symbol index.** Unlocks: unused function detection;
  calls to undefined functions; direct recursion detection; duplicate
  function definitions.
- **E4: multi-file / directory lint + include resolution.** Unlocks:
  project-wide pack runs (`lmbetter lint rules/ src/`); unused-include
  hygiene; cross-file duplicate symbols.
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
