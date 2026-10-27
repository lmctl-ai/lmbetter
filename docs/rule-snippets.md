# Rules as lmscript snippets (SonarQube study)

Goal: code-analysis rules expressed as small lmscript snippets running over a
tree-sitter AST inside `lmbetter`, instead of SonarQube's compiled, rigid rule
classes.

## What SonarQube actually does (study summary)

Sources: SonarSource/sonarqube, sonar-java, sonar-plugin-api, SonarJS, sonar-text.

- **A rule is a compiled Java class.** Most extend `IssuableSubscriptionVisitor`:
  declare AST node kinds in `nodesToVisit()`, get callbacks in `visitNode(Tree)`,
  raise with `reportIssue(tree, message)`. The engine walks the AST once and
  dispatches by node-kind membership. Example: `ContinueInLoopCheck` (S909) is a
  15-line class — subscribe to `CONTINUE_STATEMENT`, report every hit.
- **Metadata is split from logic**: `@Rule(key = "S909")` on the class, plus
  `S909.json` (title, type BUG/CODE_SMELL/VULNERABILITY, severity, remediation
  cost, tags) and `S909.html` (description) per rule, loaded by
  `RulesDefinition`/`RuleMetadataLoader`.
- **Pipeline**: Sensor → CheckFactory (activates rules from the quality profile)
  → parser (Eclipse JDT for Java) → VisitorsBridge (single merged AST walk) →
  `NewIssue` (primary location, secondary locations, flows, severity/cost
  overrides) → server persistence.
- **What rules can inspect**: 129 `Tree.Kind`s, tokens/trivia, plus full
  semantics — resolved types, symbols (`MethodMatchers`), control-flow graphs,
  and a symbolic-execution engine for bug/vulnerability rules.
- **Configuration**: `@RuleProperty` parameters injected from quality profiles;
  `// NOSONAR` suppression platform-side.
- **Scale/cost**: sonar-java has ~772 rule classes. A custom-rule plugin needs a
  213-line Maven pom, Plugin/RulesDefinition/CheckRegistrar glue classes, jar
  packaging, and a server restart to deploy.

### SonarSource is already converging on the minimal kernel

- **sonar-text secrets rules**: empty Java subclass per rule; all logic lives in
  a YAML spec (pre-filter strings, a matching regex, context patterns, entropy
  post-filter, inline examples as tests) interpreted by a generic matcher.
- **SonarJS**: rules are plain ESLint rule modules over ESTree ASTs in a Node
  bridge process; the subscription is an **esquery CSS-like selector string**,
  the check is a small JS function calling `context.report(...)`.

## The essential kernel of "a rule"

Everything above reduces to four parts:

1. **Metadata record** — key, title, severity, type, tags. Pure data.
2. **Selector/subscription** — node-kind list, esquery selector, or regex
   pre-filter.
3. **Check function** — `f(node, report) -> ()`, interpreted.
4. **Issue record** — {rule key, location/range, message, optional secondary
   locations}.

The plugin packaging, profile plumbing, reflection factories, and server are
incidental weight. What a pure tree-sitter setup genuinely loses vs. sonar-java
is semantic resolution (types, symbols) and CFG/symbolic execution — so scope
snippet rules to syntax-level checks, or feed a separately-built symbol index.

## The lmbetter mapping

tree-sitter's query engine (`ts_query_*`, already compiled into our binary via
`lib/src/lib.c`) is the native selector — S-expression patterns with captures,
the direct analog of esquery selectors and `nodesToVisit()` combined.

- **Rule = one `.lms` snippet.** No compilation, no packaging, no server:
  `lmbetter --dsl rule.dslb` or `lmctl autorun rules/no-foo.lms`.
- **Selector**: `ts.query(tree, pattern)` → matches with captures (planned DSL
  addition; the C API is already linked in).
- **Check**: ordinary lmscript over matches/nodes — node accessors
  (`kind`, `text`, `start/end row/col`, child navigation) exposed on a `Node`
  type.
- **Report**: a `report(node, message)` host sink collecting findings as
  {rule, file, range, message}, emitted as JSON lines (scriptable, diffable,
  CI-gateable).
- **Metadata = data**: a map at the top of the snippet
  (`let rule = {key: "LMB001", title: "...", severity: "minor", tags: [...]}`),
  or a sibling JSON when a catalog is wanted. Activation = "run the snippet";
  parameters = lmscript variables; suppression = a comment-convention the
  snippet itself can check (trivia is reachable via tokens).
- **Testing**: the existing examples convention (`# expect:` / `#|` header
  lines, inline positive/negative fixtures) is exactly sonar-text's
  inline-examples model.

### Sketch (JSON grammar, until more grammars are vendored)

```
# rule: LMB001 — forbid "password" keys in JSON config
p = _dsl_ cli="lmbetter" install="github:lmctl-ai/lmbetter@latest";
let src   = file_read("config.json");
let tree  = ts.parse(src);
let hits  = ts.query(tree, "(pair key: (string (string_content) @k))");
for (h in hits) {
  if (h.text == "password") { report(h, "LMB001: do not store passwords in config"); }
}
end _dsl_
savedsl(p, "build/lmb001.dslb");
```

## Non-goals (v1)

- No quality profiles / severity matrix — the snippet is the policy.
- No semantic resolution, CFG, or symbolic execution.
- No fix-it/quick-fix emission (tree-sitter keeps full text ranges, so
  suggestion payloads are possible later).
