#!/usr/bin/env bash
# From repository root, after building. Explicitly opt in to one language pack.
set -eu
build/lmok lint rules/secrets-json.json examples/data/sample.json
build/lmok lint rules/secrets-python.json tests/fixtures/python-tier1-clean.py
