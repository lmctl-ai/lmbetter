; lmok facts extraction for JSON (tree-sitter-json). JSON has no symbols in
; the code sense; the knowledge tree treats key paths as structure: every
; object key is a def.key whose scope is the dotted chain of enclosing keys
; (`server.tls` for `port` in {"server":{"tls":{"port":443}}}). Arrays are
; transparent (no index in the chain); the empty key "" has no
; string_content node and is not extracted.
(pair key: (string (string_content) @name)) @def.key
