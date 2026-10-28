#!/usr/bin/env python3
"""Decide release eligibility from committed binary inputs, never workspace stamps."""
import argparse
import os
from pathlib import Path
import subprocess

# Only these shell scripts participate in constructing/stamping the binary.
BUILD_SCRIPTS = {'scripts/sync-deps.sh', 'scripts/sync-sdk.sh', 'scripts/next-version.sh'}

def binary_input(path):
    return (path.startswith(('cpp/', 'rules/', 'schema/', 'cmake/')) or
            path in {'CMakeLists.txt', 'deps.lock', 'sdk.lock', 'THIRD_PARTY_NOTICES.md'} or
            path in BUILD_SCRIPTS)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    def git(*argv):
        return subprocess.check_output(['git', '-C', str(args.repo), *argv])
    ref = os.environ.get('GITHUB_REF', '')
    if ref.startswith('refs/tags/v'):
        publish, reason = True, 'explicit v* tag push'
    elif git('rev-parse', '--is-shallow-repository').strip() == b'true':
        raise SystemExit('Refusing release decision from shallow history; checkout fetch-depth: 0 is required')
    else:
        tags = git('tag', '--list', 'v*', '--sort=-version:refname').decode().splitlines()
        if not tags:
            publish, reason = True, 'no v* release tag exists (bootstrap)'
        else:
            latest = tags[0]
            changed = git('diff', '--no-renames', '--name-only', '-z', latest, 'HEAD').decode('utf-8', errors='surrogateescape').split('\0')
            inputs = [p for p in changed if p and binary_input(p)]
            publish = bool(inputs)
            reason = ('binary inputs changed since ' + latest + ': ' + ', '.join(inputs)) if inputs else ('no binary inputs changed since ' + latest + ' (tests/docs/CI only)')
    print(('PUBLISH release: ' if publish else 'SKIP release: ') + reason)
    if os.environ.get('GITHUB_OUTPUT'):
        with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as out:
            out.write('publish=' + str(publish).lower() + '\n')

if __name__ == '__main__':
    main()
