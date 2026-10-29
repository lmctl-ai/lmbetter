import os


def build(path):
    return os.path.join(path, "x")


build(".")
