#!/usr/bin/env python3
"""The CI gate must detect a planted finding, not merely run successfully."""
import json
from pathlib import Path
import subprocess
import sys
host=Path(sys.argv[1]).resolve()
if not host.exists() and host.with_suffix('.exe').exists(): host=host.with_suffix('.exe')
root=Path(__file__).resolve().parent/'fixtures'/'project-ci'
r=subprocess.run([str(host),'ci',str(root)],capture_output=True,timeout=20)
assert r.returncode==1, (r.returncode,r.stdout,r.stderr)
rows=[json.loads(line) for line in r.stdout.splitlines()]
assert [v['rule'] for v in rows if 'rule' in v]==['CI-PLANTED'], rows
assert rows[-1]['summary']['findings']==1, rows
assert rows[-1]['summary']['suppressed']==0, rows
assert b'\r' not in r.stdout, r.stdout
print('ok - lmok ci rejects the planted critical finding with JSONL and exit 1')
