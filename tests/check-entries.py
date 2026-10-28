#!/usr/bin/env python3
"""Native v6 rule dispatch: deterministic fixtures, no network or installed host."""
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
HOST = str(Path(sys.argv.pop(1)).resolve()) if len(sys.argv) > 1 else str(ROOT / 'build/lmbetter')
COMPILER = os.environ.get('LMSCRIPT_BIN', 'lmscript')

class EntryChecks(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory(prefix='entry-test-', dir=ROOT / 'build')
        self.work = Path(self.scratch.name)
        self.source = self.work / 'input.json'
        self.source.write_text('{"a":1,"b":2}')
    def tearDown(self):
        self.scratch.cleanup()
    def compile(self, body):
        src = self.work / 'rules.lms'
        binary = self.work / 'rules.dslb'
        src.write_text('p = _lmcode_ schema=' + json.dumps(str(ROOT / 'schema/lmbetter.host.json')) + ';\n' + body + '\nend _lmcode_\nsavedsl(p, ARGV[0]);\n')
        r = subprocess.run([COMPILER, str(src), str(binary)], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        return binary
    def run_host(self, binary, *files, lang='json'):
        return subprocess.run([HOST, 'check', '--lang', lang, str(binary), *(str(f) for f in (files or (self.source,)))], capture_output=True, text=True)
    def arrays(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, '')
        return [json.loads(line) for line in result.stdout.splitlines() if line.startswith('[')]
    def test_duplicate_annotations_in_source_order_and_body_once(self):
        binary = self.compile('''println("configured");
@pair fn first(n: Node) -> void implements Rule { ts.report(n, "first", "ID_A", "minor"); }
@pair fn second(n: Node) -> void implements Rule { ts.report(n, "second", "ID_B", "major"); }
''')
        r = self.run_host(binary, self.source, self.source)
        arrays = self.arrays(r)
        self.assertEqual(len(arrays), 2)
        self.assertEqual(arrays[0], arrays[1], 'findings reset for each input')
        self.assertEqual([f['rule'] for f in arrays[0]], ['ID_A','ID_B','ID_A','ID_B'])
        self.assertEqual(r.stdout.splitlines().count('configured'), 1)
    def test_wrong_kind_finds_nothing(self):
        binary = self.compile('@absent_kind fn ignored(n: Node) -> void implements Rule { ts.report(n, "wrong"); }')
        self.assertEqual(self.arrays(self.run_host(binary)), [[]])
    def test_anonymous_keyword_kind(self):
        self.source.write_text('int f(void) { if (1) return 0; return 1; }')
        binary = self.compile('@if fn keyword(n: Node) -> void implements Rule { if (!n.is_named()) { ts.report(n, n.text(), "KW", "minor"); } }')
        findings = self.arrays(self.run_host(binary, lang='c'))[0]
        self.assertEqual([(f['rule'],f['message']) for f in findings], [('KW','if')])
    def test_entry_failure_is_located_run_error(self):
        binary = self.compile('@pair fn bad(n: Node) -> void implements Rule { n.child(999); }')
        r = self.run_host(binary)
        self.assertEqual(r.returncode,3,r.stdout+r.stderr)
        error = json.loads(r.stdout.splitlines()[-1])
        self.assertEqual(error['phase'],'run')
        self.assertIn('out of range',error['message'])
        self.assertGreater(error['line'],0)
        self.assertGreater(error['column'],0)
    def test_v5_refused_before_body(self):
        binary = self.compile('println("must not run");')
        b=bytearray(binary.read_bytes()); b[4:6]=b'\0\5'; binary.write_bytes(b)
        r=self.run_host(binary)
        self.assertEqual(r.returncode,3,r.stdout+r.stderr)
        error=json.loads(r.stdout)
        self.assertEqual(error['code'],'LOAD_BAD_BYTECODE')
        self.assertIn('unsupported bytecode version 5',error['message'])
    def test_duplicate_entry_names_refused(self):
        binary=self.compile('@pair fn first(n: Node) -> void implements Rule {}\n@pair fn second(n: Node) -> void implements Rule {}')
        b=bytearray(binary.read_bytes()); p=38
        def u16():
            nonlocal p
            n=struct.unpack_from('>H',b,p)[0];p+=2;return n
        def u32():
            nonlocal p
            n=struct.unpack_from('>I',b,p)[0];p+=4;return n
        for _ in range(u32()):
            p+=1;n=u32();p+=n
        n=u16();p+=4*n;n=u16();p+=8*n
        def region():
            nonlocal p
            n=u16();p+=5*n+2;n=u32();p+=n;n=u32();p+=10*n
        region();self.assertEqual(u16(),2);first=p
        p+=14;region();b[p:p+4]=b[first:first+4];binary.write_bytes(b)
        r=self.run_host(binary)
        self.assertEqual(r.returncode,3,r.stdout+r.stderr)
        self.assertIn("duplicate entry name 'first'",json.loads(r.stdout)['message'])
    def test_missing_input_and_bad_usage(self):
        binary=self.compile('@pair fn first(n: Node) -> void implements Rule {}')
        r=self.run_host(binary,self.work/'missing.json')
        self.assertEqual(r.returncode,3,r.stdout+r.stderr)
        self.assertIn('cannot read',r.stdout+r.stderr)
        r=subprocess.run([HOST,'check'],capture_output=True)
        self.assertEqual(r.returncode,2)

if __name__ == '__main__':
    unittest.main()
