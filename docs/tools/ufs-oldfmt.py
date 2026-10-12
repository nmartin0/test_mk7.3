#!/usr/bin/env python3
#
# ufs-oldfmt.py -- turn a 4.4BSD-format UFS1 image into the 4.3BSD
# format that OSF's UFS reader expects.
#
#   python3 docs/tools/ufs-oldfmt.py <ufs.img>
#
# The image is changed in place.  It is meant for an image just written
# by makefs(8) ("makefs -t ffs -o version=1"), which writes the 4.4BSD
# formats; docs/tools/mkbootdisk.sh runs it.
#
# Why.  OSF's UFS reader, osfmk/src/file_systems/ufs, is the one the
# bootstrap task uses to read /mach_servers/.  It is CMU's and OSF's
# code from before 4.4BSD, and reads OSF/1's UFS: a directory entry is
#
#	u_int d_ino; u_short d_reclen; u_short d_namlen; char d_name[];
#
# (file_systems/ufs/defs.h).  4.4BSD split d_namlen into d_type and an
# 8-bit d_namlen.  On a little-endian disk the type lands in the low
# byte of what OSF reads as d_namlen, so every name compares unequal
# and no file is ever found.  4.4BSD kept the old format as an option --
# a file system whose fs_maxsymlinklen is 0 and whose fs_inodefmt is
# FS_42INODEFMT is read with the old directory entries (OFSFMT) -- but
# makefs does not offer it, and newfs needs a device and privileges.
#
# What it changes, and nothing else:
#
#   every directory entry   d_type cleared, d_namlen widened to 16 bits
#   every inode in use      the 16-bit uid and gid at offsets 4 and 6,
#                           where the 4.3BSD dinode keeps them, set from
#                           the 32-bit ones 4.4BSD keeps at 112 and 116
#   the superblock          fs_maxsymlinklen 0, fs_inodefmt
#                           FS_42INODEFMT, so a 4.4BSD reader (Lites,
#                           NetBSD) also reads the image as old format
#
# It refuses an image it cannot convert exactly: a symbolic link (4.4BSD
# stores short targets in the inode, which the old format does not), a
# directory needing indirect blocks, or a superblock that is not UFS1.
#
import struct
import sys

SBOFF = 8192
FS_MAGIC = 0x011954
FS_42INODEFMT = -1
IFMT, IFDIR, IFLNK = 0o170000, 0o040000, 0o120000
DINODE_SIZE = 128
NDADDR = 12

# UFS1 superblock offsets, 4.4BSD struct fs
SB_NCG, SB_BSIZE, SB_FSIZE = 44, 48, 52
SB_IBLKNO, SB_CGOFFSET, SB_CGMASK = 16, 24, 28
SB_IPG, SB_FPG = 184, 188
SB_MAXSYMLINKLEN, SB_INODEFMT, SB_MAGIC = 1320, 1324, 1372


def die(msg):
    sys.exit("ufs-oldfmt: " + msg)


def main():
    if len(sys.argv) != 2:
        die("usage: ufs-oldfmt.py <ufs.img>")
    path = sys.argv[1]
    img = bytearray(open(path, "rb").read())

    def sb(off):
        return struct.unpack_from("<i", img, SBOFF + off)[0]

    if struct.unpack_from("<I", img, SBOFF + SB_MAGIC)[0] != FS_MAGIC:
        die("%s: not a UFS1 file system" % path)
    ncg, bsize, fsize = sb(SB_NCG), sb(SB_BSIZE), sb(SB_FSIZE)
    iblkno, cgoffset, cgmask = sb(SB_IBLKNO), sb(SB_CGOFFSET), sb(SB_CGMASK)
    ipg, fpg = sb(SB_IPG), sb(SB_FPG)

    def inode_offset(ino):
        cg, idx = divmod(ino, ipg)
        cgstart = fpg * cg + cgoffset * (cg & ~cgmask)
        return (cgstart + iblkno) * fsize + idx * DINODE_SIZE

    ndirs = nents = ninodes = 0
    for ino in range(2, ipg * ncg):
        off = inode_offset(ino)
        mode, nlink = struct.unpack_from("<Hh", img, off)
        if mode == 0 or nlink == 0:
            continue
        ninodes += 1
        uid, gid = struct.unpack_from("<II", img, off + 112)
        struct.pack_into("<HH", img, off + 4, uid & 0xFFFF, gid & 0xFFFF)

        if mode & IFMT == IFLNK:
            die("inode %d is a symbolic link; not supported" % ino)
        if mode & IFMT != IFDIR:
            continue

        ndirs += 1
        size = struct.unpack_from("<Q", img, off + 8)[0]
        if size > NDADDR * bsize:
            die("directory inode %d needs indirect blocks" % ino)
        direct = struct.unpack_from("<%di" % NDADDR, img, off + 40)
        left = size
        for blk in direct:
            if left <= 0:
                break
            if blk == 0:
                die("directory inode %d has a hole" % ino)
            base, n, p = blk * fsize, min(left, bsize), 0
            while p < n:
                reclen = struct.unpack_from("<H", img, base + p + 4)[0]
                if reclen == 0:
                    die("directory inode %d: zero-length entry" % ino)
                namlen = img[base + p + 7]
                struct.pack_into("<H", img, base + p + 6, namlen)
                nents += 1
                p += reclen
            left -= n

    struct.pack_into("<i", img, SBOFF + SB_MAXSYMLINKLEN, 0)
    struct.pack_into("<i", img, SBOFF + SB_INODEFMT, FS_42INODEFMT)
    open(path, "wb").write(img)
    print("%s: %d inodes, %d directories, %d entries in 4.3BSD format"
          % (path, ninodes, ndirs, nents))


main()
