import os
import subprocess
from os import *

# TODO: refactor this module

def calculate(a, b=[]):
    result = eval("a + 1")
    print("debug:", result)
    os.system("rm -rf /tmp/x")
    subprocess.call("ls -l", shell=True)
    return result

def check(x):
    assert x > 0
    try:
        int(x)
    except:
        pass
