; lmok facts extraction for C (tree-sitter-c). Vocabulary: `@def.<kind>` /
; `@ref.<kind>` on the fact node, `@name` on the name node; `(#set! confidence
; "...")` overrides the default confidence (def.* exact, ref.call candidate,
; edges exact). Scope chains come from enclosing def.function / def.prototype /
; def.struct / def.union / def.enum anchors (host logic, cpp/facts.cpp).
; Every shape below was checked against the vendored grammar with `lmok parse`.

; ---- definitions ----------------------------------------------------------
(function_definition
  declarator: [
    (function_declarator declarator: (identifier) @name)
    (pointer_declarator declarator: (function_declarator declarator: (identifier) @name))
    (pointer_declarator declarator: (pointer_declarator declarator: (function_declarator declarator: (identifier) @name)))
  ]) @def.function

; A prototype (declaration whose declarator is a function declarator). A
; function-pointer variable `int (*fp)(int)` has a parenthesized_declarator
; there and is deliberately NOT matched (it is a variable, ambiguous here).
(declaration
  declarator: [
    (function_declarator declarator: (identifier) @name)
    (pointer_declarator declarator: (function_declarator declarator: (identifier) @name))
  ]) @def.prototype

(preproc_def name: (identifier) @name) @def.macro
(preproc_function_def name: (identifier) @name) @def.macro

(struct_specifier name: (type_identifier) @name body: (_)) @def.struct
(union_specifier name: (type_identifier) @name body: (_)) @def.union
(enum_specifier name: (type_identifier) @name body: (_)) @def.enum
(enumerator name: (identifier) @name) @def.enumerator
(type_definition declarator: (type_identifier) @name) @def.typedef

; Variables: plain, pointer, array and initialised declarators, two levels
; deep (`char **argv2`, `char *names[3]`). Deeper nests are not extracted.
(declaration
  declarator: [
    (identifier) @name
    (init_declarator declarator: [
      (identifier) @name
      (pointer_declarator declarator: [
        (identifier) @name
        (pointer_declarator declarator: (identifier) @name)
        (array_declarator declarator: (identifier) @name)
      ])
      (array_declarator declarator: [
        (identifier) @name
        (pointer_declarator declarator: (identifier) @name)
      ])
    ])
    (pointer_declarator declarator: [
      (identifier) @name
      (pointer_declarator declarator: (identifier) @name)
      (array_declarator declarator: (identifier) @name)
    ])
    (array_declarator declarator: [
      (identifier) @name
      (pointer_declarator declarator: (identifier) @name)
    ])
  ]) @def.variable

(parameter_declaration
  declarator: [
    (identifier) @name
    (pointer_declarator declarator: [
      (identifier) @name
      (pointer_declarator declarator: (identifier) @name)
    ])
    (array_declarator declarator: (identifier) @name)
  ]) @def.parameter

(field_declaration
  declarator: [
    (field_identifier) @name
    (pointer_declarator declarator: (field_identifier) @name)
    (array_declarator declarator: (field_identifier) @name)
  ]) @def.field

(labeled_statement label: (statement_identifier) @name) @def.label

; ---- references and edges -------------------------------------------------
(call_expression function: (identifier) @name) @ref.call

; `#include "x.h"` is a literal, resolvable edge; `#include <x.h>` names a
; system/external header (the host strips the angle brackets).
(preproc_include path: (string_literal (string_content) @name)) @ref.include
((preproc_include path: (system_lib_string) @name) @ref.include
  (#set! confidence "external"))
