#!/usr/bin/env python3
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_windows_host.py PATH/TO/JIANT")

root = Path(sys.argv[1])
p = root / "tests" / "hostsim.c"
s = p.read_text(encoding="utf-8")
old = '''#include <stdio.h>\n#include <unistd.h>\n#include <sys/wait.h>\n#include <time.h>\n'''
new = '''#include <stdio.h>\n#ifdef _WIN32\n#include <time.h>\ntypedef int pid_t;\n#define fork() (-1)\n#define waitpid(pid, status, options) (*(status) = 0, (pid))\n#define WIFEXITED(status) 1\n#define WEXITSTATUS(status) 0\n#define _exit(code) ((void)(code))\n#ifndef CLOCK_MONOTONIC\n#define CLOCK_MONOTONIC 1\n#endif\n#define clock_gettime(clock_id, ts) ((void)(clock_id), (ts)->tv_sec = 0, (ts)->tv_nsec = 0, 0)\n#else\n#include <unistd.h>\n#include <sys/wait.h>\n#include <time.h>\n#endif\n'''
if old not in s:
    raise SystemExit("hostsim include block changed upstream")
p.write_text(s.replace(old, new, 1), encoding="utf-8")
print(f"patched {p}")
