#!/usr/bin/env python3
"""Compile real host faults using the configured Ninja build's existing objects.

Run after configuring with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON and building.
Each mutation compiles and links successfully; only a failing Python assertion
counts as caught. SDK vendor bytes and the normal host binary remain untouched.
"""
import json, os, shlex, subprocess, sys, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BUILD=Path(sys.argv[1] if len(sys.argv)>1 else ROOT/'build').resolve()
entry=next(e for e in json.loads((BUILD/'compile_commands.json').read_text()) if e['file'].endswith('/cpp/main.cpp'))
compile_cmd=shlex.split(entry['command'])
commands=subprocess.check_output(['ninja','-C',str(BUILD),'-t','commands','lmok'],text=True).splitlines()
link_cmd=shlex.split(commands[-1].removeprefix(': && ').removesuffix(' && :'))
source=(ROOT/'cpp/main.cpp').read_text()
mutations=[
 ('missing-frame', 'dslvm::write_result_frame(std::cout, invocation_result);', '(void)invocation_result;'),
 ('reset-per-entry', 'for (size_t entry : found->second) program.call(entry, context, reg, {handle});', 'for (size_t entry : found->second) { invocation_result.reset(); program.call(entry, context, reg, {handle}); }'),
 ('premature-frame', 'invocation_result.reset(); // each program is its own invocation', 'invocation_result.reset(); if (const int rc = emit_result()) return rc; // incorrect premature emission'),
 ('leaked-program', 'invocation_result.reset(); // each program is its own invocation', '// incorrect: prior program result leaks'),
 ('frame-before-sarif', 'if (sarif_path) {\n      // driver.rules:', 'if (const int rc = emit_result()) return rc;\n    if (sarif_path) {\n      // driver.rules:'),
]
def verify(binary, caught):
    run=subprocess.run([sys.executable,str(ROOT/'tests/check-returns.py'),str(binary)],capture_output=True,text=True)
    if caught:
        assert run.returncode==1 and 'FAIL:' in run.stdout+run.stderr, run.stdout+run.stderr
    else:
        assert run.returncode==0, run.stdout+run.stderr
verify(BUILD/('lmok.exe' if os.name=='nt' else 'lmok'),False)
with tempfile.TemporaryDirectory(prefix='return-controls-',dir=BUILD) as tmp:
    work=Path(tmp)
    for name,old,new in mutations:
        assert source.count(old)==1, (name,source.count(old))
        changed=work/(name+'.cpp'); changed.write_text(source.replace(old,new))
        obj=work/(name+'.o'); binary=work/(name+('.exe' if os.name=='nt' else ''))
        cmd=[str(changed) if arg==entry['file'] else ('-O0' if arg=='-O3' else arg) for arg in compile_cmd]
        cmd[cmd.index('-o')+1]=str(obj)
        subprocess.run(cmd,cwd=BUILD,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
        cmd=[str(obj) if arg.endswith('CMakeFiles/lmok.dir/cpp/main.cpp.o') else arg for arg in link_cmd]
        cmd[cmd.index('-o')+1]=str(binary)
        subprocess.run(cmd,cwd=BUILD,check=True,capture_output=True)
        verify(binary,True)
        print(name+': CAUGHT',flush=True)
