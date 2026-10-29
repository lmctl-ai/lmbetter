# Java grammar and tier-1 rules

lmok parses Java with the vendored tree-sitter-java grammar. The Java tier-1
pack is intentionally syntactic: it flags empty and generic catches,
printStackTrace, direct System.out.println calls, string literals used with
==, credential-looking field initializers, variable Runtime.exec commands,
MD5/SHA-1 MessageDigest requests, and TODO/FIXME comments.

The System.out.println rule reports one finding per non-main method containing
a matching call spelling, including nested bodies; methods named main are exempt.
This is a source-text heuristic inside a syntactically identified method, so a
matching comment or literal can also be reported; no type resolution is claimed. Likewise the credential and digest rules identify source text,
not runtime secrets or cryptographic usage. Use java-tier1.json with
--lang java or a .java target after the Java grammar is registered.

## Facts and native contracts

Java uses tree-sitter-java v0.23.5 (MIT, archive hash in deps.lock). Classes, records, interfaces, methods/constructors, fields, parameters and imported names are exact syntactic definitions. Calls are candidate references, not overload/type resolution; import edges retain qualified source targets. Interface and class anchors participate in enclosing scopes.

Native Java oracles run from test.sh through tests/check-java-os.py: parse/query byte positions with CRLF and Unicode paths, extension-based facts, nine rule counts and zero clean findings, and compiled annotated method dispatch. Output-corruption controls must fail before the normal oracles pass.

String comparisons cover literal operands only, not inferred String variables. println is one finding per non-main method (name-only exemption). Runtime.exec covers direct Runtime.getRuntime().exec(identifier), not aliased receivers; credential fields require string literals and recognized key spellings. Rules are LJV001–LJV009 to avoid collisions with JSON LBJ identifiers.
