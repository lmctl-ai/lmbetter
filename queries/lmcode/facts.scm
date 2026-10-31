; Facts for standalone lmcode rule files and extracted lmcode bodies.
(function_declaration name: (identifier) @name) @def.function
(function_declaration (implements_clause (identifier) @name)) @ref.implements

; Calls are references; import/schema descriptors belong to outer lmscript.
; An ordinary map with a key named schema or cli is application data, not a
; schema declaration. S5 can associate inner facts with outer descriptor facts.
(call_expression function: (identifier) @name) @ref.call
(call_expression function: (member_expression property: (identifier) @name)) @ref.call
