#!/usr/bin/env python3
"""Secrets are generated at runtime so the repository itself stays clean."""
import json
import random
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HOST = Path(sys.argv[1]).resolve()

def run(*args):
    return subprocess.run([str(HOST), *map(str, args)], capture_output=True, timeout=60)

def findings(pack, source, *args):
    r = run('lint', pack, source, *args)
    assert r.returncode == 0, (r.stdout, r.stderr)
    return [json.loads(line) for line in r.stdout.splitlines() if line.startswith(b'{')], r

def reject(actual, expected):
    try:
        assert actual == expected
    except AssertionError:
        return
    raise AssertionError('failing control did not fail')

rng = random.Random(191011)
# Assemble provider prefixes: no real-shaped credentials are committed.
samples = [
    ('AWS', 'AK' + 'IA' + 'A1B2C3D4E5F6G7H8'),
    ('GITHUB', 'gh' + 'p_' + 'aB9' * 12),
    ('GITHUB', 'github' + '_pat_' + 'aB1_' * 22),
    ('SLACK', 'xo' + 'xb-' + '12345678901-12345678901-' + 'Ab9' * 10),
    ('PRIVATE_KEY', '-----BEGIN ' + 'RSA PRIVATE KEY-----'),
    ('JWT', 'ey' + 'JhbGciOiJIUzI1NiJ9.eyJzdWIiOiJmYWtlIn0.' + 'abCD09_-' * 4),
    ('ENTROPY', ''.join(rng.sample('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789', 40))),
]
# Controls precede the real oracles: corrupted expected counts and leaked bytes are rejected.


with tempfile.TemporaryDirectory(prefix='secrets os ', dir=ROOT / 'build') as tmp:
    tmp = Path(tmp)
    for lang, ext in [('json','json'), ('python','py'), ('c','c'), ('bash','sh')]:
        pack = ROOT / 'rules' / ('secrets-' + lang + '.json')
        source = tmp / ('fake 雪.' + ext)
        def write(values):
            if lang == 'json': text = json.dumps(dict(values))
            elif lang == 'c': text = '\n'.join('const char *' + k + ' = ' + json.dumps(v) + ';' for k,v in values)
            elif lang == 'bash': text = '\n'.join(k + "='" + v + "'" for k,v in values)
            else: text = '\n'.join(k + ' = ' + json.dumps(v) for k,v in values)
            source.write_bytes((text + '\r\n').encode('utf-8'))
        write([('long_credential_assignment_password' if name == 'ENTROPY' else 'value'+str(i), value) for i,(name,value) in enumerate(samples)])
        if lang == 'json':
            # Execute broken packs before the positive oracle: omission and raw-text
            # regressions must be rejected by the same assertions used below.
            data = json.loads(pack.read_text())
            bad = tmp / 'mutant.json'
            data['rules'] = data['rules'][1:]
            bad.write_text(json.dumps(data))
            reject(len(findings(bad, source)[0]), len(samples))
            data = json.loads(pack.read_text())
            for rule in data['rules']: rule['redact'] = False
            bad.write_text(json.dumps(data))
            raw = findings(bad, source)[0][0]['message']
            reject(raw, 'AWS secret ' + samples[0][1][:4] + '[REDACTED]')
            print('ok - executed failing controls: omitted detector and disabled redaction')
        db = tmp / (lang + '.db')
        fs, result = findings(pack, source, '--db', db)
        assert len(fs) == len(samples), (lang, fs, result.stderr)
        for (name, secret), finding in zip(samples, fs):
            assert finding['rule'].startswith('SEC_' + name), (lang, finding)
            assert finding['message'].endswith(secret[:4] + '[REDACTED]'), finding
            assert secret.encode() not in result.stdout + result.stderr, (lang, finding)
        stored = sqlite3.connect(db).execute('select message from findings').fetchall()
        assert [x[0] for x in stored] == [f['message'] for f in fs]
        # Random ordinary strings are not credentials; entropy needs a sensitive assignment key.
        write([('value'+str(i), ''.join(rng.choices('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789', k=48))) for i in range(1000)])
        fs, _ = findings(pack, source)
        assert fs == [], (lang, fs)
        # Low-entropy values under sensitive keys must not fire.
        write([('password', 'a' * 40)])
        assert findings(pack, source)[0] == []
        print('ok -', lang, 'provider shapes, redaction/DB, 1000 random strings, low entropy')
    # Predicate arithmetic, strict comparison, arity/domain and bound.
    source = tmp / 'entropy.json'
    source.write_bytes(b'["abab", "aaaaaaaa", ""]')
    pattern = '((string_content) @s (#entropy-gt? @s "0.9"))'
    good = run('query', pattern, source)
    assert good.returncode == 0 and len(good.stdout.splitlines()) == 1, good
    assert run('query', pattern.replace('0.9', '1'), source).stdout == b''
    for value in ['nan', '-1', '8.1', '1x']:
        assert run('query', pattern.replace('0.9', value), source).returncode == 3
    source.write_text(json.dumps(['ab' * 9000]))
    assert run('query', pattern, source).stdout == b''
    assert run('query', '((string_content) @s (#entropy-gt? @s))', source).returncode == 3
    print('ok - entropy strict threshold, domain, arity, 16 KiB bound')
    # Scan only tracked sources (generated build/npm/vendor trees are not the repository).
    paths = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).split(b'\0')
    for lang, extensions in [('json',{'.json'}),('python',{'.py'}),('c',{'.c','.h'}),('bash',{'.sh','.bash'})]:
        files = [ROOT / p.decode() for p in paths if p and Path(p.decode()).suffix in extensions]
        fs, r = findings(ROOT / 'rules' / ('secrets-'+lang+'.json'), files[0])
        assert fs == []
        for file in files[1:]:
            assert findings(ROOT / 'rules' / ('secrets-'+lang+'.json'), file)[0] == [], file
    print('ok - zero findings in tracked lmok sources')
