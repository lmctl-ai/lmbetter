; Facts for the lmscript outer language.  lmcode bodies are deliberately
; represented by their enclosing marker here; S5 will reparse those byte ranges
; and rebase inner facts instead of making this query depend on injection.

; Declarations and module/package edges.
(function_declaration "fn" (identifier) @name) @def.function
(function_declaration "function" (identifier) @name) @def.function
(import_statement (plain_string) @name) @ref.import
(package_statement (identifier) @name) @def.package

; Calls and schema descriptors.
(call_expression (identifier) @name) @ref.call
(call_expression (member_expression (_) "." (identifier) @name .)) @ref.call
(load_schema_expression (schema_clause (plain_string) @name)) @ref.schema

; Marker kinds are useful to rules even though the marker token itself is not
; a declaration.  The host records the marker text as the edge name.
(marker_expression (block_marker) @name) @ref.marker

; The outer handle is a schema binding, not a declaration inside embedded code.
(assignment_expression (identifier) @name "=" (load_schema_expression)) @def.schema
(block_option (plain_string) @name) @ref.schema
