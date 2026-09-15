#!/usr/bin/env python3
"""Write /mach_servers/bootstrap.conf into a Minix v1 (14-char) image."""
import struct, sys, time

IMG='/tmp/minix.img'; BS=1024
d=bytearray(open(IMG,'rb').read())

def u16(o): return struct.unpack_from('<H', d, o)[0]
def u32(o): return struct.unpack_from('<I', d, o)[0]
def p16(o,v): struct.pack_into('<H', d, o, v)
def p32(o,v): struct.pack_into('<I', d, o, v)

sb=1024
ninodes,nzones,imapb,zmapb,firstdz,logzs = struct.unpack_from('<6H', d, sb)
print(f'  ninodes={ninodes} nzones={nzones} imap={imapb} zmap={zmapb} firstdz={firstdz}')

imap_off = 2*BS                       # after boot block + superblock
zmap_off = imap_off + imapb*BS
itab_off = zmap_off + zmapb*BS
INOSZ    = 32                         # minix v1 inode

def alloc_bit(base, nbits):
    for i in range(1, nbits):
        byte = base + i//8
        if not (d[byte] >> (i%8)) & 1:
            d[byte] |= 1 << (i%8)
            return i
    raise RuntimeError('no free bit')

def inode(n): return itab_off + (n-1)*INOSZ

def set_inode(n, mode, size, zones):
    o=inode(n)
    p16(o+0, mode); p16(o+2, 0); p32(o+4, size)
    p32(o+8, int(time.time())); d[o+12]=1; d[o+13]=1   # nlinks, uid-ish
    for i,z in enumerate(zones[:7]): p16(o+14+2*i, z)

def zone_off(z): return z*BS

# --- allocate: dir inode, file inode, one zone each
dino = alloc_bit(imap_off, ninodes+1)
fino = alloc_bit(imap_off, ninodes+1)
dz   = alloc_bit(zmap_off, nzones) + firstdz - 1
fz   = alloc_bit(zmap_off, nzones) + firstdz - 1
print(f'  dir inode={dino} file inode={fino} dir zone={dz} file zone={fz}')

conf=b'name_server name_server\ndefault_pager default_pager\n'
d[zone_off(fz):zone_off(fz)+len(conf)] = conf
set_inode(fino, 0o100644, len(conf), [fz])

# directory /mach_servers : entries . .. bootstrap.conf  (16 bytes each: 2 ino + 14 name)
ents=[(dino,b'.'),(1,b'..'),(fino,b'bootstrap.conf')]
buf=bytearray()
for ino,name in ents:
    buf += struct.pack('<H', ino) + name.ljust(14, b'\0')
d[zone_off(dz):zone_off(dz)+len(buf)] = buf
set_inode(dino, 0o040755, len(buf), [dz])

# root inode 1: append entry for mach_servers
ro=inode(1); rsize=u32(ro+4); rz=u16(ro+14)
rbuf=bytearray(d[zone_off(rz):zone_off(rz)+rsize])
rbuf += struct.pack('<H', dino) + b'mach_servers'.ljust(14, b'\0')
d[zone_off(rz):zone_off(rz)+len(rbuf)] = rbuf
p32(ro+4, len(rbuf))
print(f'  root zone={rz} new root size={len(rbuf)}')

open(IMG,'wb').write(d)
print('  written')
