#!/usr/bin/env python3
"""Build a Minix v1 (14-char) image containing /mach_servers/{bootstrap.conf,servers}."""
import struct, sys, os, time

IMG=sys.argv[1] if len(sys.argv)>1 else '/tmp/minix.img'
BS=1024
d=bytearray(open(IMG,'rb').read())
def u16(o): return struct.unpack_from('<H', d, o)[0]
def u32(o): return struct.unpack_from('<I', d, o)[0]
def p16(o,v): struct.pack_into('<H', d, o, v)
def p32(o,v): struct.pack_into('<I', d, o, v)

sb=1024
ninodes,nzones,imapb,zmapb,firstdz,logzs = struct.unpack_from('<6H', d, sb)
imap_off=2*BS; zmap_off=imap_off+imapb*BS; itab_off=zmap_off+zmapb*BS
INOSZ=32
def inode(n): return itab_off+(n-1)*INOSZ
def zoff(z): return z*BS

def alloc_ino():
    for i in range(1, ninodes+1):
        b=imap_off+i//8
        if not (d[b]>>(i%8))&1:
            d[b]|=1<<(i%8); return i
    raise RuntimeError('out of inodes')

def alloc_zone():
    for i in range(1, nzones):
        b=zmap_off+i//8
        if not (d[b]>>(i%8))&1:
            d[b]|=1<<(i%8); return i+firstdz-1
    raise RuntimeError('out of zones')

def set_inode(n, mode, size, direct, indirect=0):
    o=inode(n)
    p16(o+0,mode); p16(o+2,0); p32(o+4,size); p32(o+8,int(time.time()))
    d[o+12]=1; d[o+13]=0
    for i in range(7): p16(o+14+2*i, direct[i] if i<len(direct) else 0)
    p16(o+28, indirect); p16(o+30, 0)

def write_file(path):
    data=open(path,'rb').read()
    nblk=(len(data)+BS-1)//BS
    zones=[]
    for i in range(nblk):
        z=alloc_zone(); zones.append(z)
        chunk=data[i*BS:(i+1)*BS]
        d[zoff(z):zoff(z)+len(chunk)]=chunk
    ino=alloc_ino()
    if nblk<=7:
        set_inode(ino, 0o100755, len(data), zones)
    else:
        iz=alloc_zone()                      # single indirect block
        ind=bytearray(BS)
        for i,z in enumerate(zones[7:]):
            struct.pack_into('<H', ind, 2*i, z)
        d[zoff(iz):zoff(iz)+BS]=ind
        set_inode(ino, 0o100755, len(data), zones[:7], iz)
    return ino, nblk

def dirent(ino, name):
    return struct.pack('<H', ino) + name.encode().ljust(14, b'\0')

# ---- /mach_servers
dino=alloc_ino(); dz=alloc_zone()
entries=[dirent(dino,'.'), dirent(1,'..')]

conf=b'name_server name_server\ndefault_pager default_pager\n'
cz=alloc_zone(); d[zoff(cz):zoff(cz)+len(conf)]=conf
cino=alloc_ino(); set_inode(cino, 0o100644, len(conf), [cz])
entries.append(dirent(cino,'bootstrap.conf'))

for p in sys.argv[2:]:
    ino,nb = write_file(p)
    entries.append(dirent(ino, os.path.basename(p)))
    print(f'  {os.path.basename(p):16s} inode={ino} blocks={nb} size={os.path.getsize(p)}')

buf=b''.join(entries)
d[zoff(dz):zoff(dz)+len(buf)]=buf
set_inode(dino, 0o040755, len(buf), [dz])

ro=inode(1); rsize=u32(ro+4); rz=u16(ro+14)
rbuf=bytearray(d[zoff(rz):zoff(rz)+rsize]) + dirent(dino,'mach_servers')
d[zoff(rz):zoff(rz)+len(rbuf)]=rbuf
p32(ro+4, len(rbuf))
open(IMG,'wb').write(d)
print(f'  /mach_servers inode={dino} zone={dz} entries={len(entries)}')
