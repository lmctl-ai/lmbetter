import os
import subprocess
from os import * # lmok-expect: LBP006

# TODO: refactor this module # lmok-expect: LBP005

def calculate(a, b=[]): # lmok-expect: LBP009
    result = eval("a + 1") # lmok-expect: LBP003
    print("debug:", result) # lmok-expect: LBP008
    os.system("rm -rf /tmp/x") # lmok-expect: LBP001
    subprocess.call("ls -l", shell=True) # lmok-expect: LBP002
    return result

def check(x):
    assert x > 0 # lmok-expect: LBP004
    try:
        int(x)
    except: # lmok-expect: LBP007
        pass

import os
import subprocess
from os import path

# Refactored and stable.

def calculate(a, b=None):
    if b is None:
        b = []
    result = int(a) + 1
    os.path.exists("/tmp")
    subprocess.call(["ls", "-l"], shell=False)
    return result

def check(x):
    if x <= 0:
        raise ValueError("x must be positive")
    try:
        int(x)
    except ValueError:
        pass
