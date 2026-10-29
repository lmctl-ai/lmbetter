; lmok facts extraction for Python (tree-sitter-python). Vocabulary as in
; queries/c/facts.scm. Host rule: a def.function whose nearest enclosing
; scope definition is a def.class is reported as def.method (decorators are
; transparent: decorated_definition is not a scope).

; ---- definitions ----------------------------------------------------------
(class_definition name: (identifier) @name) @def.class
(function_definition name: (identifier) @name) @def.function

; Module- and class-level bindings only; function-local assignments are not
; symbols. `self.x = ...` (attribute targets) is not extracted.
(module (expression_statement (assignment left: (identifier) @name) @def.variable))
(class_definition body: (block (expression_statement (assignment left: (identifier) @name) @def.variable)))

(parameters [
  (identifier) @name
  (default_parameter name: (identifier) @name)
  (typed_parameter (identifier) @name)
  (typed_default_parameter name: (identifier) @name)
  (list_splat_pattern (identifier) @name)
  (dictionary_splat_pattern (identifier) @name)
] @def.parameter)

; Imported bindings (the name made visible in this module, as written).
(import_statement name: (dotted_name) @name @def.import)
(import_statement name: (aliased_import alias: (identifier) @name @def.import))
(import_from_statement name: (dotted_name) @name @def.import)
(import_from_statement name: (aliased_import alias: (identifier) @name @def.import))

; ---- references and edges -------------------------------------------------
(call function: (identifier) @name) @ref.call
(call function: (attribute attribute: (identifier) @name)) @ref.call
; The object of an attribute access is a read of that name (so `os` in
; `os.path.join` is still findable). Candidate: no type information.
(attribute object: (identifier) @name @ref.read)

; Module edges. `module_name` is a dotted_name or a relative_import whose
; text keeps the leading dots (`.`, `..up`).
(import_statement name: (dotted_name) @name) @ref.import
(import_statement name: (aliased_import name: (dotted_name) @name)) @ref.import
(import_from_statement module_name: (_) @name) @ref.import_from
((import_from_statement module_name: (_) @name (wildcard_import)) @ref.import_wildcard
  (#set! confidence "candidate"))
