; Facts for standalone lmcode rule files and extracted lmcode bodies.
(function_declaration name: (identifier) @name) @def.function
(function_declaration (implements_clause (identifier) @name)) @ref.implements

; Host calls are candidate references; schema/cli declarations are represented
; by their string values when supplied through a map literal.
(call_expression function: (identifier) @name) @ref.call
(call_expression function: (member_expression property: (identifier) @name)) @ref.call
((map_entry (identifier) @key (string) @name) @ref.schema
  (#match? @key "^(schema|cli)$"))
