#!/usr/bin/env python3
"""
Build a Minix v1 filesystem image for the OSFMK 7.3 bootstrap task.

    mkminix.py <image> [file ...]

Formats the image and writes /mach_servers/ containing bootstrap.conf
plus each file given, under its basename.

Formats as well as populates, because mkfs.minix is not available
everywhere -- Debian 13 dropped it from util-linux -- and because the
reader is picky in a way mkfs.minix's defaults get wrong:
file_systems/minixfs/minixfs.c:560 accepts only MINIX_SUPER_MAGIC
0x137F, the original 14-character-name variant, while mkfs.minix -1
defaults to 30-character names and magic 0x138F.

Directories and files are written by hand, with single indirect blocks
for files over 7 KB, because a loop mount needs privileges a build
environment usually lacks.
"""
import os, struct, sys, time

BS      = 1024          # block and zone size
INOSZ   = 32            # minix v1 inode
MAGIC   = 0x137F        # 14-character names
NAMELEN = 14
DIRENT  = 2 + NAMELEN

def build(path, files):
    nzones  = 1440                       # 1.44MB floppy
    ninodes = 480
    imap_blocks = (ninodes + 1 + 8191) // 8192
    zmap_blocks = (nzones  + 1 + 8191) // 8192
    inode_blocks = (ninodes * INOSZ + BS - 1) // BS
    firstdatazone = 2 + imap_blocks + zmap_blocks + inode_blocks

    d = bytearray(nzones * BS)
    struct.pack_into('<6H', d, BS, ninodes, nzones, imap_blocks,
                     zmap_blocks, firstdatazone, 0)
    struct.pack_into('<I', d, BS + 12, 0x10081C00)   # s_max_size
    struct.pack_into('<H', d, BS + 16, MAGIC)

    imap = 2 * BS
    zmap = imap + imap_blocks * BS
    itab = zmap + zmap_blocks * BS

    def setbit(base, i):
        d[base + i // 8] |= 1 << (i % 8)
    setbit(imap, 0)                      # bit 0 is reserved in both maps
    setbit(zmap, 0)

    def alloc_ino():
        for i in range(1, ninodes + 1):
            if not (d[imap + i // 8] >> (i % 8)) & 1:
                setbit(imap, i); return i
        raise SystemExit('out of inodes')

    def alloc_zone():
        for i in range(1, nzones - firstdatazone):
            if not (d[zmap + i // 8] >> (i % 8)) & 1:
                setbit(zmap, i); return i + firstdatazone - 1
        raise SystemExit('out of zones -- the image is full')

    def ino_off(n):  return itab + (n - 1) * INOSZ
    def zone_off(z): return z * BS

    # minix v1 inode: 7 direct zones, then i_zone[7] single indirect and
    # i_zone[8] double indirect. A zone number is 2 bytes, so one
    # indirect block addresses BS/2 = 512 zones.
    PER_IND = BS // 2

    def set_inode(n, mode, size, direct, indirect=0, dbl=0, nlinks=1):
        o = ino_off(n)
        struct.pack_into('<HHII', d, o, mode, 0, size, int(time.time()))
        d[o + 12] = nlinks
        d[o + 13] = 0
        for i in range(7):
            struct.pack_into('<H', d, o + 14 + 2 * i,
                             direct[i] if i < len(direct) else 0)
        struct.pack_into('<H', d, o + 28, indirect)
        struct.pack_into('<H', d, o + 30, dbl)

    def write_indirect(zones):
        """Store up to PER_IND zone numbers in a fresh indirect block."""
        iz = alloc_zone()
        blk = bytearray(BS)
        for i, z in enumerate(zones):
            struct.pack_into('<H', blk, 2 * i, z)
        d[zone_off(iz):zone_off(iz) + BS] = blk
        return iz

    def dirent(n, name):
        b = name.encode()
        if len(b) > NAMELEN:
            raise SystemExit(f'name too long for minix v1: {name}')
        return struct.pack('<H', n) + b.ljust(NAMELEN, b'\0')

    def write_file(data, mode):
        nblk = (len(data) + BS - 1) // BS
        zones = []
        for i in range(nblk):
            z = alloc_zone(); zones.append(z)
            chunk = data[i * BS:(i + 1) * BS]
            d[zone_off(z):zone_off(z) + len(chunk)] = chunk
        n = alloc_ino()
        direct, rest = zones[:7], zones[7:]
        single = dbl = 0
        if rest:
            single = write_indirect(rest[:PER_IND])
            rest = rest[PER_IND:]
        if rest:
            # double indirect: a block of pointers to indirect blocks
            inds = [write_indirect(rest[i:i + PER_IND])
                    for i in range(0, len(rest), PER_IND)]
            dbl = write_indirect(inds)
        set_inode(n, mode, len(data), direct, single, dbl)
        return n

    # root directory, inode 1
    root = alloc_ino()
    assert root == 1
    rz = alloc_zone()

    # /mach_servers
    dino = alloc_ino(); dz = alloc_zone()
    entries = [dirent(dino, '.'), dirent(root, '..')]

    # bootstrap.conf lines are  [-flags] symtab_name path [args...]
    #
    # parse_config_file() in src/bootstrap/bootstrap.c takes the first
    # field as the symbol table name and the second as the path, and
    # parse_path() puts anything after that into the server's argv.
    # Leading -flags are consumed by parse_boot_args() as the bootstrap
    # task's own options (-k, -S, -w ...), NOT passed to the server, so
    # a server flag written there is silently eaten.
    #
    # Servers receive that argv through crt0's __get_arguments(), which
    # calls bootstrap_arguments() over IPC; the stack they start on
    # holds a deliberate dummy zero argc, so this file is the only way
    # to give a server arguments.
    #
    # A file may be given a different name in the image with
    # "path:name", which matters because minix v1 directory entries hold
    # only 14 bytes of name and LITES's binary is called
    # startup.Lites.1.1.u3.STD+WS+osfmach3+ext2fs, which is 43.
    #
    # An argument list may be attached with "=args", after the name if
    # both are given, e.g.
    #     mkminix.py img default_pager=hd1c .../startup.Lites...:startup
    # which writes
    #     default_pager default_pager hd1c
    #     startup startup
    # and is how default_pager is given a paging device: its main()
    # loops over argv calling bs_add_device() on each name, and without
    # one it starts with no backing store at all and every
    # ps_allocate_cluster() fails.
    lines = []
    real  = []
    names = []
    for f in files:
        # NB: not "path" -- that name holds the image being written,
        # and shadowing it here sends the finished image to the last
        # file on the command line instead.
        fpath, _, args = f.partition('=')
        fpath, _, asname = fpath.partition(':')
        base = (asname or os.path.basename(fpath)).encode()
        if len(base) > 14:
            raise SystemExit(
                'name too long for minix v1 (14 bytes): %s\n'
                'give a shorter name with "%s:shortname"'
                % (base.decode(), fpath))
        line = b'%s %s' % (base, base)
        if args:
            line += b' ' + args.encode()
        lines.append(line + b'\n')
        real.append(fpath)
        names.append(base.decode())
    files = real
    conf = b''.join(lines)
    if not conf:
        conf = b'default_pager default_pager\n'
    entries.append(dirent(write_file(conf, 0o100644), 'bootstrap.conf'))

    for f, nm in zip(files, names):
        data = open(f, 'rb').read()
        n = write_file(data, 0o100755)
        entries.append(dirent(n, nm))
        print(f'  {nm:<20} inode {n:3d}  {len(data)} bytes'
              + ('' if nm == os.path.basename(f) else f'   <- {os.path.basename(f)}'))

    buf = b''.join(entries)
    d[zone_off(dz):zone_off(dz) + len(buf)] = buf
    set_inode(dino, 0o040755, len(buf), [dz], nlinks=2)

    rbuf = dirent(root, '.') + dirent(root, '..') + dirent(dino, 'mach_servers')
    d[zone_off(rz):zone_off(rz) + len(rbuf)] = rbuf
    set_inode(root, 0o040755, len(rbuf), [rz], nlinks=3)

    open(path, 'wb').write(d)
    used = sum(1 for i in range(nzones - firstdatazone)
               if (d[zmap + i // 8] >> (i % 8)) & 1)
    print(f'  {path}: magic {MAGIC:#06x}, {used} of '
          f'{nzones - firstdatazone} zones used')

if __name__ == '__main__':
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    build(sys.argv[1], sys.argv[2:])
