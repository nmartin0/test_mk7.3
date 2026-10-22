#!/usr/bin/env python3
#
# mknoemulsysent.py -- write the system call table that the Lites
# server uses for a process it runs without the emulator
# (server/kern/noemul_sysent.c; docs/roadmap.md, Tier 1 item 2).
#
#   python3 docs/tools/mknoemulsysent.py <syscalls.master> <lites-dir> \
#       <server-objdir> > osfmk/src/lites/server/kern/noemul_sysent.c
#
# <syscalls.master> is 4.4BSD-Lite2's sys/kern/syscalls.master
# (@(#)syscalls.master 8.6 (Berkeley) 3/30/95), as it stands in
# github.com/nmartin0/4.4BSD-Lite2, branch dev3, the project's
# maintained Lite2 and its first donor for BSD parts of Lites; at
# 8954f549 the file is the distribution's, unchanged.  NetBSD 1.0's
# programs trap with 4.4BSD's numbers, and its own calls are these
# (NetBSD 1.0's <sys/syscall.h>, compared number by number: the same
# but for 0, 165, 169-171 and 198, which are NetBSD's and are written
# below as Lite2 writes them).  Patience, "Redirecting system calls in
# Mach 3.0" (1993), section 7, ran the integrated kernel's syscall()
# in the server; this is that kernel's table, the 4.4BSD-Lite
# init_sysent.c, for the server's own copies of the calls.
#
# The table is in Lite's first form, the one the server's struct
# sysent has (include/sys/systm.h): { argument words, function }.  An
# off_t or quad_t argument is two words, as Lite counts it for the
# i386, where the pad word before one is in the prototype.
#
# Each call is given the server's function when the server has one
# in a source file that the build in <server-objdir>, a
# STD+WS+osfmach3+noemulator server's object directory, compiled
# (conf/files and conf/i386/files, the files with an object there),
# with the system call's form, "name(p, uap, retval)" with a struct
# proc: the 4.4BSD name, the compatibility call's under Lite's "o"
# prefix (kern/vfs_syscalls.c, ostat; Lite1's init_sysent.c macro
# compat(n, name)), or the server's own name for it, below.  Code in
# "#if 0" is not counted.  Anything else is nosys, which is what Lite
# gives a call it lacks; docs/roadmap.md, item 2, step 4, lists them.
# The master's #ifdef lines are kept, written for Lites' option
# headers, which define each option as 0 or 1.

import os
import re
import sys

# The server's names for 4.4BSD calls it has under another name: its
# syscalls.master (server/kern/syscalls.master) names them so.
SERVER_NAMES = {
    'exit': 'rexit',
    'access': 'saccess',
    'acct': 'sysacct',
    'read': 's_read',
    'write': 's_write',
    'setrlimit': 's_setrlimit',
    'settimeofday': 's_settimeofday',
    'adjtime': 's_adjtime',
}

OPTION_HEADERS = {
    'KTRACE': 'ktrace.h', 'TRACE': 'trace.h', 'NFS': 'nfs.h',
    'SYSVSHM': 'sysvshm.h', 'LFS': 'lfs.h',
}

MACHINE = 'i386'


def strip_if0(text):
    """Drop the lines inside #if 0 ... #endif (nested #ifs counted)."""
    out = []
    depth = 0       # >0 while inside an #if 0
    for line in text.split('\n'):
        s = line.strip()
        if depth:
            if re.match(r'#\s*if', s):
                depth += 1
            elif re.match(r'#\s*endif', s):
                depth -= 1
            elif depth == 1 and re.match(r'#\s*(else|elif)', s):
                depth = 0
            out.append('')
            continue
        if re.match(r'#\s*if\s+0\b', s):
            depth = 1
            out.append('')
            continue
        out.append(line)
    return '\n'.join(out)


def compiled_sources(lites, objdir):
    """The server's sources, as conf/files lists them, that objdir built."""
    objs = set(f[:-2] for f in os.listdir(objdir) if f.endswith('.o'))
    srcs = []
    for lst in ('conf/files', 'conf/%s/files' % MACHINE):
        for line in open(os.path.join(lites, lst)):
            w = line.split()
            if not w or w[0] not in ('file', 'fileif'):
                continue
            path = w[-1]
            base = os.path.splitext(os.path.basename(path))[0]
            src = os.path.join(lites, 'server', path)
            # generated sources (kern/init_sysent.c) are not in the tree
            if path.endswith('.c') and base in objs and os.path.exists(src):
                srcs.append(src)
    return srcs


def server_functions(lites, objdir):
    """Names defined as name(p, uap, retval), with a struct proc."""
    found = {}
    defn = re.compile(
        r'^(?:[A-Za-z_][\w \t*]*[\s*])?([A-Za-z_]\w*)\s*\(([^()]*)\)'
        r'\s*\n?((?:[^{()]*\n){0,8})\s*\{', re.M)
    for path in compiled_sources(lites, objdir):
        text = strip_if0(open(path, errors='replace').read())
        for m in defn.finditer(text):
            params = [x for x in m.group(2).split(',') if x.strip()]
            if len(params) == 3 and 'struct proc' in m.group(0):
                found.setdefault(m.group(1), path)
    return found


def words(args):
    n = 0
    if args in ('', 'void'):
        return 0
    for a in args.split(','):
        if '*' not in a and re.search(r'\b(off_t|quad_t|u_quad_t)\b', a):
            n += 2
        else:
            n += 1
    return n


def cond(line):
    """A master #if line, for option headers that define 0 or 1."""
    m = re.match(r'#\s*ifdef\s+(\w+)', line)
    if m:
        return '#if ' + m.group(1)
    return re.sub(r'defined\((\w+)\)', r'\1', line)


def main():
    master, lites, objdir = sys.argv[1], sys.argv[2], sys.argv[3]
    have = server_functions(lites, objdir)
    text = open(master).read().replace('\\\n', ' ')
    rows = []          # (kind, ...) in master order
    used = set()
    options = set()
    for line in text.split('\n'):
        s = line.strip()
        if s.startswith('#if') or s.startswith('#else') or \
           s.startswith('#endif'):
            c = cond(s) if s.startswith('#if') else s
            options.update(o for o in re.findall(r'\b[A-Z][A-Z0-9_]+\b', c)
                           if o in OPTION_HEADERS)
            rows.append(('cpp', c))
            continue
        m = re.match(r'^(\d+)\s+(\w+)\s*(.*)$', line)
        if not m:
            continue
        num, typ, rest = int(m.group(1)), m.group(2), m.group(3)
        p = re.search(r'\{\s*[\w\s*]+?\s*(\w+)\s*\(([^)]*)\)\s*;\s*\}'
                      r'\s*(\w*)', rest)
        if not p or typ in ('OBSOL', 'UNIMPL'):
            note = rest.strip() or typ.lower()
            if typ == 'OBSOL':
                note = 'obsolete ' + note
            rows.append(('call', num, 0, 'nosys', note))
            continue
        name, args, alias = p.group(1), p.group(2).strip(), p.group(3)
        if typ == 'STD':
            want = SERVER_NAMES.get(name, name)
            label = alias or name
        elif typ == 'COMPAT_43':
            want = 'o' + name
            label = 'old ' + name
        else:
            raise SystemExit('unknown type %s at %d' % (typ, num))
        if want in have:
            fn, note = want, label
            used.add(fn)
        else:
            fn, note = 'nosys', label + ', not in the server'
        rows.append(('call', num, words(args), fn, note))

    out = sys.stdout.write
    out('/*\n'
        ' * System call switch table for a process the server runs\n'
        ' * without the emulator (NO_EMULATOR).\n'
        ' *\n'
        ' * DO NOT EDIT-- generated by docs/tools/mknoemulsysent.py from\n'
        ' * 4.4BSD-Lite2\'s sys/kern/syscalls.master, 8.6 (Berkeley)\n'
        ' * 3/30/95, which is under the University of California\'s\n'
        ' * copyright and its BSD licence; see docs/roadmap.md, Tier 1\n'
        ' * item 2.  Numbers and argument words are that file\'s; the\n'
        ' * functions are this server\'s.\n'
        ' */\n\n')
    out('#include <sys/param.h>\n#include <sys/systm.h>\n')
    for o in sorted(options):
        out('#include "%s"\n' % OPTION_HEADERS[o])
    out('\nint\tnosys();\n')
    for fn in sorted(used):
        if fn != 'nosys':
            out('int\t%s();\n' % fn)
    out('\nstruct sysent noemul_sysent[] = {\n')
    for r in rows:
        if r[0] == 'cpp':
            out(r[1] + '\n')
        else:
            _, num, n, fn, note = r
            entry = '{ %d, %s },' % (n, fn)
            tabs = max(1, (40 - (8 + len(entry))) // 8 + 1)
            out('\t%s%s/* %d = %s */\n' % (entry, '\t' * tabs, num, note))
    out('};\n\nint\tnoemul_nsysent = '
        'sizeof(noemul_sysent) / sizeof(noemul_sysent[0]);\n')


main()
