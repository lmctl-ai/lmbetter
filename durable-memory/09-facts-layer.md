# Facts layer (E6, tier 3 spike wave 1A)

`lmok facts [--lang L] FILE|DIR...` is the per-file extraction half of the
tier-3 knowledge tree (docs/tier3/deep-analysis.md §4, README "Facts").

## Mechanism

- `queries/<lang>/facts.scm` (c, python, bash, go, javascript, typescript, tsx, json) are embedded at build
  time by `cmake/embed_schema.cmake` (same path as the schema) and evaluated
  by `host.cpp`'s shared query runner. New host primitive:
  `query_matches()` keeps CAPTURE NAMES and collects `(#set! key "value")`
  directives per pattern (`QueryMatches.properties`); `node_query()` is the
  same runner without names. `node_start_byte/node_end_byte` were added.
- Vocabulary: `@def.<kind>`/`@ref.<kind>` on the fact node, `@name` on the
  name node, helper captures like `@_cmd` are ignored, `(#set! confidence
  "external"|"candidate"|"unresolved")` overrides the default (def.* exact,
  ref.call/ref.read candidate, literal edges exact).
- `cpp/facts.cpp` host logic: scope chain = names of enclosing
  scope-defining definitions (def.function/method/prototype/class/struct/
  union/enum/key) found by walking `node_parent` from the fact's anchor and
  looking up anchors by `node_id`; Python `def.function` whose nearest scope
  def is `def.class` becomes `def.method` (decorated_definition is
  transparent); `ref.include` strips `<>`; bash `ref.source` strips quotes
  and is `unresolved` when the raw target contains `$` or a backtick.
  Output sorted by (start_byte, kind, name, end_byte), exact duplicates
  removed (a pattern with a repeated field, e.g. `name:` on
  import_from_statement, matches once per child).
- `main.cpp`: `expand_targets()` is lint's directory expansion factored out
  (byte-identical lint behaviour); `facts` uses it with an optional grammar
  filter for `--lang`.

## Known edges (documented, not bugs)

- C: function-pointer variables (`int (*fp)(int)`) are not extracted
  (parenthesized_declarator); declarators nest at most two levels
  (`char **argv`, `char *names[3]`); `#if` branches are all reported;
  macro-generated definitions are invisible.
- Python: function-local assignments and `self.x = ...` are not symbols;
  `import a.b` binds `a` but is recorded as written (`a.b`); wildcard
  imports add a `ref.import_wildcard` candidate next to the exact
  `ref.import_from`.
- Bash: every command word is a `ref.call` candidate (builtins such as
  `return`/`echo` included); `$1` is a `ref.read` of `1`.
- Go: function/method/type/package-variable/short-variable definitions, explicit
  import aliases and literal import paths, and direct/selector calls are emitted;
  receiver/type resolution and ordinary identifier reads are not inferred.
- TypeScript: function/method/class/type/variable/import definitions and calls;
  type resolution and ordinary identifier reads are not inferred.
- TSX: functions, interfaces and calls inside JSX; JSX element names and component
  resolution are not inferred as calls.
- JavaScript: function/method/class/variable/import definitions, literal import
  paths and direct/selector calls; dynamic property resolution is not inferred.
- JSON: the empty key `""` has no `string_content` node and is skipped;
  arrays do not contribute to the scope chain.
- Incremental check (facts after edit+reparse == cold facts) waits for the
  E8 `ts.facts` builtin — there is no CLI path to a reparsed tree.
