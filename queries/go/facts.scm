; lmok facts extraction for Go (tree-sitter-go): definitions, literal import
; paths and calls. Calls are candidates; definitions and literal imports exact.

(function_declaration name: (identifier) @name) @def.function
(method_declaration name: (field_identifier) @name) @def.method
(type_spec name: (type_identifier) @name) @def.type
(var_spec name: (identifier) @name) @def.variable
(const_spec name: (identifier) @name) @def.variable
(short_var_declaration left: (expression_list (identifier) @name)) @def.variable
(import_spec name: (package_identifier) @name) @def.import

(import_spec path: (interpreted_string_literal) @name) @ref.import
(import_spec path: (raw_string_literal) @name) @ref.import
(call_expression function: (identifier) @name) @ref.call
(call_expression function: (selector_expression field: (field_identifier) @name)) @ref.call
