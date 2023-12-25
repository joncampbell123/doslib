#!/usr/bin/python3
#
# Microsoft Visual C++ 1.5 won't do anything beyond limited ICO formats,
# but ImageMagick and the GIMP are quite willing to make them anyway.
# We just have to merge them together into one to get what we really want
# from the ICO file. Hah.
import struct
import sys
import os

dst_ico = None
src_ico = [ ]

it = iter(sys.argv)
next(it) # skip argv[0]

try:
    while True:
        x = next(it)
        if x == "-o":
            if not dst_ico == None:
                raise Exception("Dest already specified")
            dst_ico = str(next(it))
        elif x == "-i":
            src_ico.append(str(next(it)))
        else:
            raise Exception("Unknown param")
except StopIteration:
    True

if dst_ico == None:
    raise Exception("No destination specified")
if len(src_ico) == None:
    raise Exception("No source files specified")

class WinIcon:
    direntry = None
    icobits = None
    def __init__(self):
        direntry = None
        icobits = None
    def __str__(self):
        return "[dirent="+str(self.direntry)+", icobits="+str(self.icobits)+"]"

iconlist = [ ]

for srcicon in src_ico:
    sico = open(srcicon,"rb")
    dirhdr = sico.read(6)
    x = struct.unpack("<HHH",dirhdr)
    if not x[0] == 0 or not x[1] == 1: # idReserved == 0 idType == 1
        print(srcicon + " is not an ICO file")
        sico.close()
        continue
    scount = x[2] # idCount
    #
    sdirents = [ ]
    for di in range(0,scount):
        sdirents.append(sico.read(0x10)) # ICODIRENTRY
    #
    for dirent in sdirents:
        [ressize,imgoffset] = struct.unpack("<ll",dirent[8:16])
        w = WinIcon()
        w.direntry = dirent
        sico.seek(imgoffset)
        w.icobits = sico.read(ressize)
        iconlist.append(w)
    #
    sico.close()

sico.close()

icon_offset = 6 + (len(iconlist) * 16)
for icon in iconlist:
    icon.icon_offset = icon_offset
    icon.direntry = icon.direntry[0:8] + struct.pack("<ll",len(icon.icobits),icon_offset)
    icon_offset += len(icon.icobits)

dico = open(dst_ico,"wb")
dico.write(struct.pack("<HHH",0,1,len(iconlist))) # ICONDIR
for icon in iconlist:
    dico.write(icon.direntry)
for icon in iconlist:
    if not dico.tell() == icon.icon_offset:
        raise Exception("Ooops")
    dico.write(icon.icobits)
dico.close()

