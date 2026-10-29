#!/usr/bin/env python3
"""Semgrep-style pattern rules (roadmap wave 2 item 6): `lmok query --pattern` compiles a code-shaped
pattern to a tree-sitter query; the cases in tests/patterns/cases.json pin the generated query text and
the root matches byte-exactly on every native OS. Usage: check-patterns.py [HOST] [--prove-controls]."""
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
HOST = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 and not sys.argv[1].startswith('--') else ROOT / 'build' / ('lmok.exe' if os.name == 'nt' else 'lmok')
PROVE = '--prove-controls' in sys.argv
if PROVE: sys.argv.remove('--prove-controls')
CASES = json.loads((ROOT / 'tests/patterns/cases.json').read_text(encoding='utf-8'))

def run(*args):
    return subprocess.run([str(HOST), *map(str, args)], capture_output=True, timeout=60)

class Patterns(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.work = Path(ROOT / 'build' / 'pattern-cases'); cls.work.mkdir(parents=True, exist_ok=True)
        for lang in ('c', 'python'):
            (cls.work / ('in.' + lang)).write_bytes(CASES[lang + '_code'].encode('utf-8'))  # LF on every OS
    def test_cases_pin_query_text_and_root_matches(self):
        self.assertGreaterEqual(len(CASES['cases']), 20)
        for case in CASES['cases']:
            with self.subTest(pattern=case['pattern']):
                r = run('query', '--lang', case['lang'], '--pattern', case['pattern'], self.work / ('in.' + case['lang']))
                self.assertEqual(r.returncode, 0, r.stderr)
                expected = ('query: ' + case['query'] + '\n' + ''.join(m + '\n' for m in case['matches'])).encode('utf-8')
                self.assertEqual(r.stdout, expected)
                self.assertEqual(r.stderr, b'')
    def test_whitespace_in_the_pattern_is_irrelevant(self):
        tight = next(c for c in CASES['cases'] if c['pattern'] == 'strcpy($DST, $SRC)')
        loose = next(c for c in CASES['cases'] if c['pattern'] == 'strcpy ( $A ,$B )')
        self.assertEqual(tight['matches'], loose['matches'])
        self.assertEqual(tight['query'].replace('DST', 'A').replace('SRC', 'B'), loose['query'])
    def test_malformed_patterns_exit_2_naming_the_fragment(self):
        for case in CASES['errors']:
            with self.subTest(pattern=case['pattern']):
                r = run('query', '--lang', case['lang'], '--pattern', case['pattern'], self.work / ('in.' + case['lang']))
                self.assertEqual(r.returncode, 2, r.stdout + r.stderr)
                self.assertIn(case['message'].encode('utf-8'), r.stderr)
                self.assertIn(case['pattern'].encode('utf-8'), r.stderr) if 'is only supported' not in case['message'] else None
                self.assertEqual(r.stdout, b'', 'nothing is printed for a pattern that does not compile')
    def test_pattern_flag_needs_a_pattern_and_a_file(self):
        self.assertEqual(run('query', '--lang', 'c', '--pattern').returncode, 2)
        self.assertEqual(run('query', '--lang', 'bash', '--pattern', 'echo', self.work / 'in.c').returncode, 2, 'bash patterns are outside this slice')

    def test_pack_pattern_rules_equal_their_query_rules_byte_for_byte(self):
        # Three tier-1 rules rewritten as patterns (LBC001, LBC004 in C; LBP003, LBP008 in Python) must print the
        # same findings as the original query rules on the violations fixtures, byte for byte.
        for lang, fixture in (('c', 'tests/fixtures/c-tier1-violations.c'), ('python', 'tests/fixtures/python-tier1-violations.py')):
            with self.subTest(lang=lang):
                query = run('lint', ROOT / f'tests/patterns/{lang}-subset-query.json', ROOT / fixture)
                pattern = run('lint', ROOT / f'tests/patterns/{lang}-subset-pattern.json', ROOT / fixture)
                self.assertEqual(query.returncode, 0, query.stderr)
                self.assertEqual(pattern.returncode, 0, pattern.stderr)
                self.assertGreater(query.stdout.count(b'\n'), 1, 'the subset must find something')
                self.assertEqual(pattern.stdout, query.stdout)
    def test_pattern_inside_and_not_inside(self):
        r = run('lint', ROOT / 'tests/patterns/c-inside.json', ROOT / 'tests/patterns/c-inside.c')
        self.assertEqual(r.returncode, 0, r.stderr)
        found = [(json.loads(l)['rule'], json.loads(l)['row']) for l in r.stdout.decode().splitlines() if l.startswith('{')]
        self.assertEqual(found, [('IN1', 1), ('OUT1', 2), ('IN1', 3)])
    def test_pack_pattern_errors_are_pack_errors(self):
        bad = [({'id': 'X', 'pattern': 'strcpy($A', 'message': 'm'}, b"does not parse as c"),
               ({'id': 'X', 'pattern': 'f($A)', 'capture': 'B', 'message': 'm'}, b"'capture' names no metavariable of the pattern"),
               ({'id': 'X', 'pattern': 'f($A)', 'query': '(identifier)', 'message': 'm'}, b"mutually exclusive"),
               ({'id': 'X', 'on': 'identifier', 'pattern-inside': 'f($A)', 'message': 'm'}, b"'pattern-inside' needs 'pattern'")]
        for rule, message in bad:
            with self.subTest(rule=rule):
                pack = self.work / 'bad.json'; pack.write_bytes(json.dumps({'lang': 'c', 'rules': [rule]}).encode())
                r = run('lint', pack, self.work / 'in.c')
                self.assertEqual(r.returncode, 2, r.stdout + r.stderr)
                self.assertIn(message, r.stderr)

if __name__ == '__main__':
    unittest.main()
