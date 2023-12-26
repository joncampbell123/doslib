#!/usr/bin/python3
import os
import sys
import stat
import struct

# Windows 1.0 and Windows 2.0 kernel module listing tool.
#
# Pass in as argv[1] the path of the WIN100.BIN or WIN200.BIN file.
# This file contains multiple NE modules joined together end to end
# which comprises the Windows KERNEL, USER, GDI, and any drivers
# that the SETUP program selected and installed.
#
# You know that mystery "32-bit CRC" field in the NE header that
# nobody ever uses? The *.BIN files use that as the offset of the
# next NE module. Byte offset is that value << sector shift, which
# in the Windows kernel, is always 4 (16-byte 8086 paragraph units).

class WinModule:
    name = None
    offset = None # offset of NE header in BIN file
    ovl_nonresident_name_table_offset = None # offset of non-resident data in OVL file
    def __init__(self):
        self.name = None
        self.offset = -1
        self.ovl_nonresident_name_table_offset = -1

modules = [ ]

winbin = sys.argv[1]
st = os.stat(winbin)
if not stat.S_ISREG(st.st_mode):
    raise Exception("Not a file")

fbin = open(winbin,"rb")
# look at MZ header, locate NE header
fbinsize = st.st_size
fbin.seek(0)
hdr = fbin.read(0x40)
if not hdr[0:2] == b"MZ":
    raise Exception("Not an MS-DOS EXE")
[scan_offset] = struct.unpack("<l",hdr[0x3C:0x40])
# follow the list until end or not an NE header
while True:
    # these modules are always paragraph (16-byte) aligned
    if not (scan_offset % 16) == 0:
        raise Exception("Not paragraph aligned")
    if scan_offset >= fbinsize:
        print("WARNING: Linked list ended unexpected")
        break
    #
    fbin.seek(scan_offset)
    hdr = fbin.read(0x40)
    # +0x00 NE header
    # +0x02 linker version
    # +0x04 entry table offset
    # +0x06 entry table length
    # +0x08 file_crc [which in this case is the paragraph in the file of the next module]
    # +0x0C flags
    # +0x0E auto data segment number
    # +0x10 init local heap
    # +0x12 init stack size
    # +0x14 entry ip
    # +0x16 entry cs
    # +0x18 entry sp
    # +0x1A entry ss
    # +0x1C segment table entries
    # +0x1E module reftable entries
    # +0x20 nonresident name table length
    # +0x22 segment table offset
    # +0x24 resource table offset
    # +0x26 resident name table offset
    # +0x28 module reference table offset
    # +0x2A imported name table offset
    # +0x2C nonresident name table offset [Which often has a too-small value like 0 or 2 and seems to signal that it's stored in the OVL file]
    # +0x30 movable entry points
    # +0x32 sector shift
    # +0x34 resource segments
    # +0x36 target os
    # +0x37 other flags
    # +0x38 fastload offset sectors
    # +0x3A fastload length sectors
    # +0x3C minimum code swap area size
    # +0x3E minimum windows version
    # =0x40
    if not hdr[0:2] == b"NE":
        raise Exception("Unexpected end where file offset does not contain NE header at "+str(scan_offset))
    if not struct.unpack("<H",hdr[0x32:0x34])[0] == 4:
        raise Exception("NE header sector shift is "+str(struct.unpack("<H",hdr[0x32:0x34])[0])+" not 4")
    #
    mod = WinModule()
    mod.offset = scan_offset
    mod.name = ""
    # read the resident name table, where the name of the module usually resides in the first entry
    resno = struct.unpack("<H",hdr[0x26:0x28])[0]
    if resno > 0:
        # u8 LENGTH
        # char[LENGTH] TEXT
        # u16 ORDINAL
        fbin.seek(resno + scan_offset)
        reslen = fbin.read(1)[0]
        if reslen > 0:
            mod.name = fbin.read(reslen).decode('iso-8859-1')
    # Non-resident data is not part of the NE image in memory but is stored in the OVL file.
    # When this occurs, the "Non-resident name table offset" is not a byte offset in the BIN file but is a paragraph
    # offset in the OVL file.
    mod.ovl_nonresident_name_table_offset = struct.unpack("<l",hdr[0x2C:0x30])[0] << 4
    #
    modules.append(mod)
    #
    next_offset = struct.unpack("<l",hdr[0x08:0x0C])[0] << 4
    if next_offset == 0: # this would make sense, but Windows doesn't do that. The last entry points into a block of zeros.
        break
    if next_offset <= scan_offset:
        raise Exception("NE next module occurs prior to the current one")
    if (next_offset+40) > fbinsize:
        break
    # WIN100.BIN last module has a next pointer that points at a block of zeros immediately following the module
    fbin.seek(next_offset)
    try:
        hdr = fbin.read(2)
        if hdr[0] == 0 and hdr[1] == 0:
            break
    except:
        break
    #
    scan_offset = next_offset
# done
fbin.close()

# print out the modules and their offsets
for mod in modules:
    print("%12s: resident.bin=%-8u nonresnametbl.ovl=%-8u" % (mod.name, mod.offset, mod.ovl_nonresident_name_table_offset))
