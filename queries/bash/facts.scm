; lmok facts extraction for Bash (tree-sitter-bash). Vocabulary as in
; queries/c/facts.scm. Command names are candidates (a word may be a
; function here, a sourced function, an alias or a PATH lookup); sourcing
; through an expansion is unresolved. Host rule: a ref.source target string
; has its quotes stripped and becomes unresolved when it contains `$` or a
; backtick.

(function_definition name: (word) @name) @def.function
; Plain assignments and `local`/`export`/`declare` ones (declaration_command
; wraps a variable_assignment).
(variable_assignment name: (variable_name) @name) @def.variable

((command name: (command_name (word) @name)) @ref.call
  (#not-any-of? @name "source" "."))

((command name: (command_name (word) @_cmd) argument: [(word) (string) (raw_string)] @name) @ref.source
  (#any-of? @_cmd "source" "."))
((command name: (command_name (word) @_cmd)
          argument: [(concatenation) (simple_expansion) (expansion) (command_substitution)] @name) @ref.source
  (#any-of? @_cmd "source" ".")
  (#set! confidence "unresolved"))

(simple_expansion (variable_name) @name) @ref.read
(expansion (variable_name) @name) @ref.read
(expansion (subscript name: (variable_name) @name)) @ref.read
