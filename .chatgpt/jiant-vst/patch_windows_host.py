#!/usr/bin/env python3
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_windows_host.py PATH/TO/JIANT")

root = Path(sys.argv[1])

# 1) hostsim.c uses POSIX-only test helpers. They are never used by the VST wrapper,
# but the translation unit still needs Windows-compatible declarations.
p = root / "tests" / "hostsim.c"
s = p.read_text(encoding="utf-8")
old = '''#include <stdio.h>\n#include <unistd.h>\n#include <sys/wait.h>\n#include <time.h>\n'''
new = r'''#include <stdio.h>
#ifdef _WIN32
#include <time.h>
#include <stdlib.h>
typedef int pid_t;
static pid_t jiant_test_fork(void) { return -1; }
static int jiant_test_waitpid(pid_t pid, int *status, int options)
{
    (void)options;
    if (status) *status = 0;
    return pid;
}
static int jiant_test_pipe(int p[2])
{
    if (p) p[0] = p[1] = -1;
    return -1;
}
static int jiant_test_write(int fd, const void *buf, unsigned int n)
{
    (void)fd; (void)buf; return (int)n;
}
static int jiant_test_read(int fd, void *buf, unsigned int n)
{
    (void)fd; (void)buf; (void)n; return 0;
}
static int jiant_test_close(int fd) { (void)fd; return 0; }
#define fork() jiant_test_fork()
#define waitpid(pid, status, options) jiant_test_waitpid((pid), (status), (options))
#define pipe(p) jiant_test_pipe(p)
#define write(fd, buf, n) jiant_test_write((fd), (buf), (unsigned int)(n))
#define read(fd, buf, n) jiant_test_read((fd), (buf), (unsigned int)(n))
#define close(fd) jiant_test_close(fd)
#define WIFEXITED(status) 1
#define WEXITSTATUS(status) 0
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif
static int jiant_test_clock_gettime(int clock_id, struct timespec *ts)
{
    (void)clock_id;
    if (ts) { ts->tv_sec = 0; ts->tv_nsec = 0; }
    return 0;
}
#define clock_gettime(clock_id, ts) jiant_test_clock_gettime((clock_id), (ts))
#else
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#endif
'''
if old not in s:
    raise SystemExit("hostsim include block changed upstream")
p.write_text(s.replace(old, new, 1), encoding="utf-8")
print(f"patched {p}")

# 2) Current JIANT main.c declares extra linker regions for low/hot RAM. The web
# emulator bypasses the hardware reset/copy path, so dummy host symbols are enough
# to satisfy the native Windows compilation just like the existing _rt symbols.
p = root / "web" / "emu" / "felucca_web.c"
s = p.read_text(encoding="utf-8")
needle = '''uint32_t _pool_start[1], _pool_end[1], _rt_start[1], _rt_end[1], _rt_load[1];\n'''
replacement = '''uint32_t _pool_start[1], _pool_end[1], _rt_start[1], _rt_end[1], _rt_load[1];\nuint32_t _low_start[1], _low_end[1];\nuint32_t _hot_start[1], _hot_end[1], _hot_load[1];\n'''
if needle not in s:
    raise SystemExit("felucca_web linker-symbol block changed upstream")
p.write_text(s.replace(needle, replacement, 1), encoding="utf-8")
print(f"patched {p}")
