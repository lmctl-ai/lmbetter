# Project configuration and CI

`lmok ci [--config FILE] [--no-suppress] [ROOT]` is the repository-level
lint entry point. The default root is `.`, and the default configuration is
`ROOT/.lmok/config.json`. Configuration paths are root-relative:

```json
{
  "version": 1,
  "packs": {"c": ["rules/c-tier1.json"], "python": ["rules/python-tier1.json"]},
  "ignore": ["generated/", "!generated/keep.c"],
  "severity_floor": "major",
  "fail_on": "findings",
  "baseline": null
}
```

`packs` names languages and their JSON rule packs. Supported language names
are json, c, python, and bash. `ignore` extends the root `.lmokignore`
rules for this invocation. The severity floor is one of info, minor, major,
or critical; findings below it remain in output but do not fail the gate.
`fail_on` is `findings` or `none`. The baseline field is reserved for the
baseline owner (item 3); a non-null value is currently rejected explicitly.

Output is JSON Lines: one object per retained finding (`path`, position, rule,
severity, and message), followed by a nested `summary` object containing
`findings`, `suppressed`, `severity_floor`, and `fail_on`. Exit 1 means a
retained finding meets the configured floor and `fail_on` is `findings`; 0 is
clean or `fail_on: none`; 2 is configuration/usage, 3 is input or lint, and 4
is output failure.

Directory expansion uses the root `.lmokignore` only (there is no ancestor or
per-directory lookup). Explicit file arguments bypass ignores. Patterns are a
bounded gitignore subset: comments, `!` negation, `*`, `?`, character classes,
`**`, and directory rules; escapes and paths containing `..` are refused.
Hidden files and directories remain skipped, while a negation can reinclude a
child. An all-ignored root succeeds with an empty summary.

Inline suppression is host-side. The token `lmok-disable` may occur anywhere
on its line or the immediately preceding source line; `lmok-disable: ID1,ID2`
limits suppression to those rule ids. Token boundaries prevent accidental
matches, but strings count just like comments. Lint knows its pack ids and
warns for typos; check can warn only for ids not observed dynamically. The
summary line from lint/check appends ` (N suppressed)` when N is nonzero.
`--no-suppress` restores all findings for lint, check, and ci.
