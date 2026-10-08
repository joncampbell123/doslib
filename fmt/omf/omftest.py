#!/usr/bin/python3
#
# Regression tests for the OMF library, omfdump, and omfsegdg.
#
# Each test builds a small OMF .OBJ or .LIB file by hand, runs the
# Linux host build of omfdump or omfsegdg on it, and checks the result.
#
# Usage (from fmt/omf, after "./make.sh" has built linux-host/):
#
#   python3 omftest.py [directory containing omfdump and omfsegdg]
#
# The directory defaults to linux-host/ next to this script.
# Exit status is 0 if all tests pass, 1 otherwise.
import os
import subprocess
import sys
import tempfile

#------------------------------------------------------------------------
# Writing OMF records
#------------------------------------------------------------------------

def omf_record(rectype, body):
    # record type, 16-bit length (including checksum), body, checksum
    length = len(body) + 1
    rec = bytes([rectype, length & 0xFF, length >> 8]) + bytes(body)
    return rec + bytes([(-sum(rec)) & 0xFF])

def lenstr(s):
    return bytes([len(s)]) + s.encode()

def THEADR(name):
    return omf_record(0x80, lenstr(name))

def LNAMES(names):
    return omf_record(0x96, b''.join(lenstr(n) for n in names))

def LLNAMES(names):
    return omf_record(0xCA, b''.join(lenstr(n) for n in names))

def SEGDEF(acbp, length, name_index, class_index):
    return omf_record(0x98, bytes([acbp, length & 0xFF, length >> 8, name_index, class_index, 1]))

def GRPDEF(name_index, segdefs):
    return omf_record(0x9A, bytes([name_index]) + b''.join(bytes([0xFF, s]) for s in segdefs))

def LEDATA(segdef, offset, data):
    return omf_record(0xA0, bytes([segdef, offset & 0xFF, offset >> 8]) + bytes(data))

def MODEND():
    return omf_record(0x8A, bytes([0x00]))

# FIXUP subrecord "Locat" field: segment relative, location type, data record offset
def LOCAT(location, offset):
    return bytes([0x80 | 0x40 | (location << 2) | (offset >> 8), offset & 0xFF])

LOC_OFFSET16 = 1
LOC_SEGBASE16 = 2

# Fix Data byte fields
FIX_F = 0x80        # frame given by a frame thread
FIX_T = 0x08        # target given by a target thread
FIX_P = 0x04        # no target displacement

# The same header for every hand made module:
#   LNAMES: 1="" 2=DGROUP 3=_TEXT 4=CODE 5=_DATA 6=DATA
#   SEGDEF: 1=_TEXT 2=_DATA, both in DGROUP
SEG_TEXT = 1
SEG_DATA = 2

def module_header(name, text_length):
    return (THEADR(name) +
        LNAMES(['', 'DGROUP', '_TEXT', 'CODE', '_DATA', 'DATA']) +
        SEGDEF(0x28, text_length, 3, 4) +       # byte aligned, public
        SEGDEF(0x48, 2, 5, 6) +                 # word aligned, public
        GRPDEF(2, [SEG_TEXT, SEG_DATA]))

#------------------------------------------------------------------------
# Reading OMF records (just enough to check omfsegdg output)
#------------------------------------------------------------------------

def read_records(data):
    recs = []
    pos = 0
    while pos + 3 <= len(data):
        rectype = data[pos]
        length = data[pos+1] | (data[pos+2] << 8)
        recs.append((rectype, data[pos+3:pos+3+length-1]))
        pos += 3 + length
        if (rectype & 0xFE) == 0x8A:
            break
    return recs

def read_index(body, pos):
    v = body[pos]
    pos += 1
    if v & 0x80:
        v = ((v & 0x7F) << 8) | body[pos]
        pos += 1
    return v, pos

# returns a list of data records (16-bit LEDATA, LIDATA, COMDAT):
#   { 'type', 'segdef', 'offset', 'data', 'fixups': [...] }
# FIXUPPs are attached to the data record before them, THREADs are resolved,
# and frame method F4 is resolved to F0 (SEGDEF) of that record, if it has a SEGDEF.
# FIXUPPs before the first data record are attached to a 'type': None entry.
def read_data_records(data):
    records = [{ 'type': None, 'segdef': 0, 'offset': 0, 'data': b'', 'fixups': [] }]
    frame_threads = {}
    target_threads = {}
    for rectype, body in read_records(data):
        if rectype in (0xA0, 0xA2):
            segdef, pos = read_index(body, 0)
            offset = body[pos] | (body[pos+1] << 8)
            records.append({ 'type': rectype, 'segdef': segdef, 'offset': offset, 'data': body[pos+2:], 'fixups': [] })
        elif rectype == 0xC2:
            attributes = body[1]
            offset = body[3] | (body[4] << 8)
            _, pos = read_index(body, 5)                # type index
            segdef = 0
            if (attributes & 0x0F) == 0:                # explicit allocation: public base
                _, pos = read_index(body, pos)          # group index
                segdef, pos = read_index(body, pos)
                if segdef == 0:
                    pos += 2                            # frame number
            _, pos = read_index(body, pos)              # public name index
            records.append({ 'type': rectype, 'segdef': segdef, 'offset': offset, 'data': body[pos:], 'fixups': [] })
        elif rectype == 0x9C:
            pos = 0
            while pos < len(body):
                b = body[pos]
                pos += 1
                if b & 0x80:
                    fix = { 'location': (b >> 2) & 0xF, 'offset': ((b & 3) << 8) | body[pos] }
                    fixdata = body[pos+1]
                    pos += 2
                    if fixdata & FIX_F:
                        fix['frame_method'], fix['frame_index'] = frame_threads[(fixdata >> 4) & 3]
                    else:
                        fix['frame_method'] = (fixdata >> 4) & 7
                        fix['frame_index'] = 0
                        if fix['frame_method'] < 3:
                            fix['frame_index'], pos = read_index(body, pos)
                    if fixdata & FIX_T:
                        fix['target_method'], fix['target_index'] = target_threads[fixdata & 3]
                    else:
                        fix['target_method'] = fixdata & 3
                        fix['target_index'], pos = read_index(body, pos)
                    if not (fixdata & FIX_P):
                        pos += 2
                    if fix['frame_method'] == 4 and records[-1]['segdef'] != 0:
                        fix['frame_method'] = 0
                        fix['frame_index'] = records[-1]['segdef']
                    records[-1]['fixups'].append(fix)
                else:
                    # THREAD: D bit selects frame or target thread
                    method = (b >> 2) & 7
                    index = 0
                    if b & 0x40:
                        if method < 3:
                            index, pos = read_index(body, pos)
                        frame_threads[b & 3] = (method, index)
                    else:
                        method &= 3
                        index, pos = read_index(body, pos)
                        target_threads[b & 3] = (method, index)
    return records

#------------------------------------------------------------------------
# Tests
#------------------------------------------------------------------------

class TestFailed(Exception):
    pass

def check(cond, msg):
    if not cond:
        raise TestFailed(msg)

def run(args):
    return subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)

def run_omfdump(tools, tmp, name, data):
    path = os.path.join(tmp, name)
    with open(path, 'wb') as f:
        f.write(data)
    r = run([os.path.join(tools, 'omfdump'), '-v', '-i', path])
    check(r.returncode == 0, 'omfdump failed: ' + r.stdout.strip())
    return r.stdout

# run omfsegdg, return the data records it wrote
def run_omfsegdg_records(tools, tmp, name, obj):
    src = os.path.join(tmp, name + '.obj')
    dst = os.path.join(tmp, name + '.out')
    with open(src, 'wb') as f:
        f.write(obj)
    r = run([os.path.join(tools, 'omfsegdg'), '-i', src, '-o', dst])
    check(r.returncode == 0, 'omfsegdg failed: ' + r.stdout.strip())
    with open(dst, 'rb') as f:
        return read_data_records(f.read())

# run omfsegdg, return the LEDATA records it wrote
def run_omfsegdg(tools, tmp, name, obj):
    return [r for r in run_omfsegdg_records(tools, tmp, name, obj) if r['type'] == 0xA0]

# A .LIB module that ends one byte past a page boundary.
# omf_context_next_lib_module_fd() must skip to the next page, not stop on
# that last byte (the MODEND checksum).
def test_lib_module_ends_after_page_boundary(tools, tmp):
    # THEADR of a 7 char name (12 bytes) + MODEND (5 bytes) = 17 bytes = one page + 1
    check_lib_modules(tools, tmp, 16, ['module1', 'mod2', 'mod3'])

# The LIBHEAD record is as long as a .LIB page, which can be larger than
# the OMF library's record buffer.
def test_lib_large_page_size(tools, tmp):
    check_lib_modules(tools, tmp, 8192, ['mod1', 'mod2'])

# Make a .LIB of modules that contain only THEADR and MODEND, and check that omfdump reads them all.
def check_lib_modules(tools, tmp, page, names):
    lib = bytearray(omf_record(0xF0, bytes(page - 4)))  # LIBHEAD, record length makes the page size
    for name in names:
        lib += THEADR(name) + MODEND()
        lib += bytes(-len(lib) % page)
    lib += omf_record(0xF1, bytes(page - 4))            # LIBEND
    path = os.path.join(tmp, 'test.lib')
    with open(path, 'wb') as f:
        f.write(lib)
    r = run([os.path.join(tools, 'omfdump'), '-i', path])
    found = r.stdout.count('type=0x80')
    check(found == len(names), 'omfdump read %u of %u modules: %s' % (found, len(names), r.stdout.strip().split('\n')[-1]))

# Frame method F4 means "the segment of the preceding LEDATA".
# omfsegdg must resolve it against the LEDATA that each FIXUPP follows.
def test_omfsegdg_F4_frame(tools, tmp):
    obj = module_header('f4', 4)
    obj += LEDATA(SEG_TEXT, 0, [0xB8, 0x00, 0x00, 0xC3])                           # mov ax,offset _DATA / ret
    obj += omf_record(0x9C, LOCAT(LOC_OFFSET16, 1) + bytes([(4 << 4) | FIX_P, SEG_DATA]))
    obj += LEDATA(SEG_DATA, 0, [0x00, 0x00])                                       # dw offset _TEXT
    obj += omf_record(0x9C, LOCAT(LOC_OFFSET16, 0) + bytes([(4 << 4) | FIX_P, SEG_TEXT]))
    obj += MODEND()
    for led in run_omfsegdg(tools, tmp, 'f4', obj):
        for fix in led['fixups']:
            check((fix['frame_method'], fix['frame_index']) == (0, led['segdef']),
                'fixup in SEGDEF %u has frame F%u index %u' % (led['segdef'], fix['frame_method'], fix['frame_index']))

# THREADs stay defined until the end of the module, so a FIXUPP record
# may use a THREAD that was defined in an earlier FIXUPP record.
def test_omfsegdg_thread_in_later_FIXUPP(tools, tmp):
    obj = module_header('thread', 6)
    obj += LEDATA(SEG_TEXT, 0, [0xB8, 0x00, 0x00])                                 # mov ax,offset _DATA
    obj += omf_record(0x9C, bytes([0x00, SEG_DATA]) +                              # target thread 0 = SEGDEF _DATA
        LOCAT(LOC_OFFSET16, 1) + bytes([(5 << 4) | FIX_T | FIX_P]))
    obj += LEDATA(SEG_TEXT, 3, [0xBB, 0x00, 0x00])                                 # mov bx,offset _DATA
    obj += omf_record(0x9C, LOCAT(LOC_OFFSET16, 1) + bytes([(5 << 4) | FIX_T | FIX_P]))
    obj += MODEND()
    for led in run_omfsegdg(tools, tmp, 'thread', obj):
        for fix in led['fixups']:
            check((fix['target_method'], fix['target_index']) == (0, SEG_DATA),
                'fixup at %u has target T%u index %u' % (led['offset'] + fix['offset'], fix['target_method'], fix['target_index']))

# "mov cx,seg _DATA" in a code segment, frame F0 = _DATA, which is in DGROUP.
# omfsegdg must see that _DATA is in DGROUP and patch it to "mov cx,cs".
def test_omfsegdg_segdef_in_DGROUP(tools, tmp):
    obj = module_header('dgroup', 4)
    obj += LEDATA(SEG_TEXT, 0, [0xB9, 0x00, 0x00, 0xC3])                           # mov cx,seg _DATA / ret
    obj += omf_record(0x9C, LOCAT(LOC_SEGBASE16, 1) + bytes([(0 << 4) | FIX_P, SEG_DATA, SEG_DATA]))
    obj += MODEND()
    led = run_omfsegdg(tools, tmp, 'dgroup', obj)[0]
    check(led['fixups'] == [], 'segment fixup was not removed')
    check(bytes(led['data']) == bytes([0x8C, 0xC9, 0x90, 0xC3]), 'code was not patched: ' + bytes(led['data']).hex())

# LLNAMES names are numbered along with LNAMES names, so the LNAMES after it continue from it.
# Same as test_omfsegdg_segdef_in_DGROUP, but DGROUP comes after an LLNAMES.
def test_LLNAMES_numbering(tools, tmp):
    obj = THEADR('llnames') + LNAMES(['', 'CODE'])                                 # LNAMES 1, 2
    obj += LLNAMES(['_LOCAL'])                                                     # LNAMES 3
    obj += LNAMES(['DGROUP', '_TEXT', '_DATA', 'DATA'])                            # LNAMES 4, 5, 6, 7
    obj += SEGDEF(0x28, 4, 5, 2) + SEGDEF(0x48, 2, 6, 7) + GRPDEF(4, [SEG_TEXT, SEG_DATA])
    obj += LEDATA(SEG_TEXT, 0, [0xB9, 0x00, 0x00, 0xC3])                           # mov cx,seg _DATA / ret
    obj += omf_record(0x9C, LOCAT(LOC_SEGBASE16, 1) + bytes([(0 << 4) | FIX_P, SEG_DATA, SEG_DATA]))
    obj += MODEND()
    out = run_omfdump(tools, tmp, 'llnames.obj', obj)
    check('[3]: "_LOCAL"' in out, 'LLNAMES name is not LNAMES 3')
    check('name="_TEXT"(5) class="CODE"(2)' in out, 'SEGDEF 1 is not _TEXT')
    check('GRPDEF (1): "DGROUP"(4)' in out, 'GRPDEF 1 is not DGROUP')
    led = run_omfsegdg(tools, tmp, 'llnames', obj)[0]
    check(bytes(led['data']) == bytes([0x8C, 0xC9, 0x90, 0xC3]), 'code was not patched: ' + bytes(led['data']).hex())

# omfdump must print each SEGDEF attribute next to its own label.
def test_omfdump_SEGDEF_fields(tools, tmp):
    obj = THEADR('segdef') + LNAMES(['', 'VIDEO', 'FAR_DATA', 'CODE32', 'CODE'])
    obj += omf_record(0x98, bytes([0x00, 0x00, 0xB8, 0x00, 0x00, 0x10, 2, 3, 1]))  # absolute at B800:0000, length 0x1000
    obj += omf_record(0x99, bytes([0x29, 0x10, 0, 0, 0, 4, 5, 1]))                  # 32-bit, byte aligned, public, length 16
    obj += MODEND()
    out = run_omfdump(tools, tmp, 'segdef.obj', obj)
    check('big=0 frame=47104 offset=0 use16' in out, 'absolute SEGDEF printed wrong')
    check('big=0 frame=0 offset=0 use32' in out, '32-bit SEGDEF printed wrong')

# A 16-bit SEGDEF with the B (big) bit set is 64KB long. Its length field is 0.
def test_SEGDEF_big_bit(tools, tmp):
    obj = THEADR('big') + LNAMES(['', 'BIGSEG', 'FAR_DATA'])
    obj += omf_record(0x98, bytes([0x6A, 0, 0, 2, 3, 1]))  # para aligned, public, B=1, length 0
    obj += MODEND()
    out = run_omfdump(tools, tmp, 'big.obj', obj)
    check('Length=65536 ' in out, 'big SEGDEF length is not 65536')

# A 32-bit SEGDEF with the B (big) bit set is 4GB long. omfdump must say so, and keep reading.
def test_SEGDEF32_big_bit(tools, tmp):
    obj = THEADR('big32') + LNAMES(['', 'FLAT', 'CODE', 'AFTER'])
    obj += omf_record(0x99, bytes([0x6B, 0, 0, 0, 0, 2, 3, 1]))  # para aligned, public, B=1, use32, length 0
    obj += SEGDEF(0x28, 4, 4, 3)
    obj += MODEND()
    out = run_omfdump(tools, tmp, 'big32.obj', obj)
    check('Length=4GB name="FLAT"(2)' in out, '4GB SEGDEF printed wrong')
    check('name="AFTER"(4)' in out, 'SEGDEF after the 4GB SEGDEF was not read')

# COMDEF, LCOMDEF and CEXTDEF entries take EXTDEF indexes, in order, along with EXTDEF and LEXTDEF.
def test_COMDEF_CEXTDEF_numbering(tools, tmp):
    obj = module_header('comdef', 6) + LNAMES(['comdat_e'])                       # LNAMES 7
    obj += omf_record(0x8C, lenstr('ext_a') + bytes([0]))                          # EXTDEF 1
    obj += omf_record(0xB0,
        lenstr('near_b') + bytes([0, 0x62, 0x84, 0x70, 0x11, 0x01]) +              # EXTDEF 2: NEAR, 70000 bytes
        lenstr('far_c') + bytes([0, 0x61, 0x81, 0x2C, 0x01, 0x04]))                # EXTDEF 3: FAR, 300 elements of 4 bytes
    obj += omf_record(0xB8, lenstr('local_d') + bytes([0, 0x62, 0x02]))            # EXTDEF 4: local NEAR, 2 bytes
    obj += omf_record(0xBC, bytes([7, 0]))                                         # EXTDEF 5: COMDAT comdat_e
    obj += omf_record(0x8C, lenstr('ext_f') + bytes([0]))                          # EXTDEF 6
    obj += LEDATA(SEG_TEXT, 0, [0xB8, 0, 0, 0xBB, 0, 0])                           # mov ax,ext_f / mov bx,comdat_e
    obj += omf_record(0x9C,
        LOCAT(LOC_OFFSET16, 1) + bytes([(5 << 4) | FIX_P | 2, 6]) +
        LOCAT(LOC_OFFSET16, 4) + bytes([(5 << 4) | FIX_P | 2, 5]))
    obj += MODEND()
    out = run_omfdump(tools, tmp, 'comdef.obj', obj)
    check('target_index="ext_f"(6)' in out, 'fixup to EXTDEF 6 is not ext_f')
    check('target_index="comdat_e"(5)' in out, 'fixup to EXTDEF 5 is not comdat_e')
    check('"near_b" typeindex=0 GLOBAL COMMUNAL NEAR length=70000' in out, 'near_b printed wrong')
    check('"far_c" typeindex=0 GLOBAL COMMUNAL FAR length=1200' in out, 'far_c printed wrong')
    check('"local_d" typeindex=0 LOCAL COMMUNAL NEAR length=2' in out, 'local_d printed wrong')
    run_omfsegdg(tools, tmp, 'comdef', obj)

# A PUBDEF with base segment 0 defines absolute symbols. The base frame follows the segment index.
def test_PUBDEF_absolute(tools, tmp):
    obj = module_header('abs', 4)
    obj += omf_record(0x90, bytes([0, 0, 0x00, 0xB8]) +                            # absolute, frame 0xB800
        lenstr('vidmem') + bytes([0x10, 0x00, 0]) +
        lenstr('vidmem2') + bytes([0x20, 0x00, 0]))
    obj += omf_record(0x90, bytes([0, SEG_TEXT]) + lenstr('entry') + bytes([0x02, 0x00, 0]))
    obj += MODEND()
    out = run_omfdump(tools, tmp, 'abs.obj', obj)
    check('"vidmem" group=""(0) segment=ABSOLUTE frame=0xB800 offset=0x10(16)' in out, 'vidmem printed wrong')
    check('"vidmem2" group=""(0) segment=ABSOLUTE frame=0xB800 offset=0x20(32)' in out, 'vidmem2 printed wrong')
    check('"entry" group=""(0) segment="_TEXT"(1) offset=0x2(2)' in out, 'entry printed wrong')

# Several FIXUPP records may follow one LEDATA. A later one must still be able to patch it.
def test_omfsegdg_two_FIXUPPs_after_LEDATA(tools, tmp):
    obj = module_header('twofix', 6)
    obj += LEDATA(SEG_TEXT, 0, [0xB8, 0, 0, 0xBB, 0, 0])                           # mov ax,offset _DATA / mov bx,seg _DATA
    obj += omf_record(0x9C, LOCAT(LOC_OFFSET16, 1) + bytes([(5 << 4) | FIX_P, SEG_DATA]))
    obj += omf_record(0x9C, LOCAT(LOC_SEGBASE16, 4) + bytes([(1 << 4) | FIX_P, 1, SEG_DATA]))  # frame GRPDEF 1 (DGROUP)
    obj += MODEND()
    led = run_omfsegdg(tools, tmp, 'twofix', obj)[0]
    check(bytes(led['data']) == bytes([0xB8, 0, 0, 0x8C, 0xCB, 0x90]), 'code was not patched: ' + bytes(led['data']).hex())
    check([f['location'] for f in led['fixups']] == [LOC_OFFSET16], 'wrong fixups left: %s' % led['fixups'])

# FIXUPPs may follow LIDATA. Iterated data can't be patched, so its fixups are kept as they are.
def test_omfsegdg_FIXUPP_after_LIDATA(tools, tmp):
    obj = module_header('lidata', 1)
    obj += LEDATA(SEG_TEXT, 0, [0xC3])
    obj += omf_record(0xA2, bytes([SEG_DATA, 0, 0, 1, 0, 0, 0, 2, 0, 0]))           # _DATA @0: 1 x (2 bytes: dw offset _TEXT)
    obj += omf_record(0x9C, LOCAT(LOC_OFFSET16, 5) + bytes([(4 << 4) | FIX_P, SEG_TEXT]))
    obj += MODEND()
    lid = [r for r in run_omfsegdg_records(tools, tmp, 'lidata', obj) if r['type'] == 0xA2][0]
    check(len(lid['fixups']) == 1, 'LIDATA has %u fixups, not 1' % len(lid['fixups']))
    fix = lid['fixups'][0]
    check((fix['frame_method'], fix['frame_index'], fix['target_method'], fix['target_index']) == (0, SEG_DATA, 0, SEG_TEXT),
        'LIDATA fixup changed: %s' % fix)

# FIXUPPs may follow COMDAT (C++ templates and inline functions). A COMDAT in _TEXT must be
# patched like LEDATA. A COMDAT that leaves its segment to the linker has no known segment,
# so an F4 frame after it must stay F4.
def test_omfsegdg_FIXUPP_after_COMDAT(tools, tmp):
    obj = module_header('comdat', 4) + LNAMES(['tmpl1', 'tmpl2'])                 # LNAMES 7, 8
    # pick any, explicit allocation in _TEXT: mov cx,seg _DATA / ret
    obj += omf_record(0xC2, bytes([0x00, 0x10, 0x00, 0, 0, 0, 0, SEG_TEXT, 7, 0xB9, 0, 0, 0xC3]))
    obj += omf_record(0x9C, LOCAT(LOC_SEGBASE16, 1) + bytes([(1 << 4) | FIX_P, 1, SEG_DATA]))
    # pick any, far code (the linker picks the segment): mov ax,offset _DATA / ret
    obj += omf_record(0xC2, bytes([0x00, 0x11, 0x00, 0, 0, 0, 8, 0xB8, 0, 0, 0xC3]))
    obj += omf_record(0x9C, LOCAT(LOC_OFFSET16, 1) + bytes([(4 << 4) | FIX_P, SEG_DATA]))
    obj += MODEND()
    c1, c2 = [r for r in run_omfsegdg_records(tools, tmp, 'comdat', obj) if r['type'] == 0xC2]
    check(bytes(c1['data']) == bytes([0x8C, 0xC9, 0x90, 0xC3]), 'COMDAT code was not patched: ' + bytes(c1['data']).hex())
    check(c1['fixups'] == [], 'COMDAT segment fixup was not removed')
    check([f['frame_method'] for f in c2['fixups']] == [4], 'F4 frame after COMDAT changed: %s' % c2['fixups'])

# omfdump must print every COMDAT header field, and the data if it is not iterated.
def test_omfdump_COMDAT(tools, tmp):
    obj = module_header('comdatf', 4) + LNAMES(['tmpl1', 'tmpl2', 'tmpl3'])       # LNAMES 7, 8, 9
    # local, exact match, explicit in DGROUP:_TEXT, word aligned: mov ax,1234h / ret
    obj += omf_record(0xC2, bytes([0x04, 0x30, 0x02, 0, 0, 0, 1, SEG_TEXT, 7, 0xB8, 0x34, 0x12, 0xC3]))
    # continuation, pick any, far code, align from SEGDEF, at offset 4: nop
    obj += omf_record(0xC2, bytes([0x01, 0x11, 0x00, 4, 0, 0, 8, 0x90]))
    # iterated, same size, explicit at absolute frame 0xB800: 2 x (1 byte: AAh)
    obj += omf_record(0xC2, bytes([0x02, 0x20, 0x00, 0, 0, 0, 0, 0, 0x00, 0xB8, 9, 2, 0, 0, 0, 1, 0xAA]))
    obj += MODEND()
    out = run_omfdump(tools, tmp, 'comdatf.obj', obj)
    check('COMDAT "tmpl1"(7) flags=0x04 LOCAL\n' in out, 'tmpl1 name or flags printed wrong')
    check('selection=EXACT-MATCH(3) allocation=EXPLICIT(0) align=WORD(2) typeindex=0' in out, 'tmpl1 attributes printed wrong')
    check('group="DGROUP"(1) segment="_TEXT"(1)' in out, 'tmpl1 public base printed wrong')
    check('B8 34 12 C3' in out, 'tmpl1 data not dumped')
    check('COMDAT "tmpl2"(8) flags=0x01 CONTINUATION\n' in out, 'tmpl2 name or flags printed wrong')
    check('selection=PICK-ANY(1) allocation=FAR-CODE(1) align=FROM-SEGDEF(0)' in out, 'tmpl2 attributes printed wrong')
    check('data_offset=0x4(4) length=0x1(1)' in out, 'tmpl2 data offset printed wrong')
    check('COMDAT "tmpl3"(9) flags=0x02 ITERATED\n' in out, 'tmpl3 name or flags printed wrong')
    check('group=""(0) segment=ABSOLUTE frame=0xB800' in out, 'tmpl3 public base printed wrong')
    check('data_offset=0x0(0) length=0x6(6)' in out, 'tmpl3 data length printed wrong')
    check('    repeat=2 bytes=1: AA\n    expanded length=0x2(2)\n' in out, 'tmpl3 iterated data printed wrong')

# omfdump must print the iterated data blocks of LIDATA and LIDATA32, and the data they expand to.
def test_omfdump_LIDATA(tools, tmp):
    obj = module_header('lidatad', 1)
    obj += omf_record(0xA2, bytes([SEG_DATA, 0, 0,
        3, 0, 0, 0, 2, 0xAA, 0xBB,                                                  # 3 x AA BB
        2, 0, 2, 0,                                                                 # 2 x (
            1, 0, 0, 0, 1, 0x01,                                                    #   1 x 01
            2, 0, 0, 0, 1, 0x02]))                                                  #   2 x 02 )
    obj += omf_record(0xA3, bytes([SEG_DATA, 0x10, 0, 0, 0,
        0x00, 0x00, 0x01, 0x00, 0, 0, 1, 0x55]))                                    # 65536 x 55
    obj += omf_record(0xA2, bytes([SEG_DATA, 0, 0, 1, 0, 0, 0, 5, 0xAA]))           # 5 content bytes, only 1 there
    obj += omf_record(0xA3, bytes([SEG_DATA, 0, 0, 0, 0,                            # (4G-1)^3 bytes
        0xFF, 0xFF, 0xFF, 0xFF, 1, 0,
            0xFF, 0xFF, 0xFF, 0xFF, 1, 0,
                0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 1, 0x00]))
    obj += MODEND()
    out = run_omfdump(tools, tmp, 'lidatad.obj', obj)
    check('    repeat=3 bytes=2: AA BB\n'
          '    repeat=2 blocks=2\n'
          '        repeat=1 bytes=1: 01\n'
          '        repeat=2 bytes=1: 02\n'
          '    expanded length=0xC(12)\n' in out, 'LIDATA blocks printed wrong')
    check('0x00000000: AA BB AA BB AA BB 01 02-02 01 02 02 ' in out, 'LIDATA expanded data printed wrong')
    check('    repeat=65536 bytes=1: 55\n    expanded length=0x10000(65536)\n' in out, 'LIDATA32 blocks printed wrong')
    check('0x00000010: 55 55 55 55 55 55 55 55-55 55 55 55 55 55 55 55 ' in out, 'LIDATA32 expanded data printed wrong')
    check('(first 4096 bytes shown)' in out, 'LIDATA32 expanded data was not cut short')
    check('    [invalid data block at 0x0]\n' in out, 'LIDATA with a short block was not caught')
    check('    [data blocks expand to more than ' in out, 'LIDATA32 that expands too far was not caught')

# omfdump must name COMENT classes and decode the bodies it knows.
def test_omfdump_COMENT(tools, tmp):
    def COMENT(ctype, cclass, body):
        return omf_record(0x88, bytes([ctype, cclass]) + bytes(body))
    def line(label, value):
        return '    %-18s%s\n' % (label, value)
    obj = THEADR('coment') + LNAMES(['', '_TEXT', 'CODE']) + SEGDEF(0x28, 1, 2, 3)
    obj += omf_record(0x8C, lenstr('weak') + bytes([0]) + lenstr('strong') + bytes([0]))
    obj += COMENT(0x00, 0x00, b'WATCOM C/C++ V2.0')
    obj += COMENT(0x00, 0x00, lenstr('The Netwide Assembler 2.16.01'))              # NASM puts a length byte first
    obj += COMENT(0x00, 0x00, b'A' + b'x' * 65)                                     # text, and 'A' (65) is the length of the rest
    obj += COMENT(0x80, 0x9F, b'clib3r')
    obj += COMENT(0x80, 0x9B, b'3fOp')
    obj += COMENT(0x80, 0x9E, b'')
    obj += COMENT(0x00, 0xA0, bytes([0x01, 0x00]) + lenstr('MyFunc') + lenstr('MYDLL') + lenstr(''))
    obj += COMENT(0x00, 0xA0, bytes([0x01, 0x01]) + lenstr('F2') + lenstr('MYDLL') + bytes([5, 0]))
    obj += COMENT(0x00, 0xA0, bytes([0x02, 0xC3]) + lenstr('Exp') + lenstr('') + bytes([7, 0]))
    obj += COMENT(0x00, 0xA1, bytes([1]) + b'CV')
    obj += COMENT(0x40, 0xA2, bytes([1]))
    obj += COMENT(0x80, 0xA8, bytes([1, 2]))
    obj += COMENT(0x80, 0xE9, bytes([0x5C, 0x64, 0x6F, 0x58]) + lenstr('foo.h'))      # 2024-03-15 12:34:56
    obj += COMENT(0x80, 0xE9, b'')
    obj += COMENT(0x80, 0xFE, b'D' + bytes([1, 3]) + b'C')
    obj += COMENT(0x80, 0xFE, b'O' + bytes([1]))
    obj += COMENT(0x80, 0xFD, b's' + bytes([1, 0x10, 0, 0x20, 0]))
    obj += COMENT(0x00, 0xA3, lenstr('foo'))
    obj += COMENT(0x00, 0x55, bytes([0x00, 0xFF]))
    obj += MODEND()
    out = run_omfdump(tools, tmp, 'coment.obj', obj)
    for want in [
            line('Comment Class:', '0x00 Translator') + line('Text:', '"WATCOM C/C++ V2.0"'),
            line('Comment Class:', '0x00 Translator') + line('Text:', '"The Netwide Assembler 2.16.01"'),
            line('Comment Class:', '0x00 Translator') + line('Text:', '"A' + 'x' * 65 + '"'),
            line('Comment Type:', '0x80 NO-PURGE') + line('Comment Class:', '0x9F Default library') + line('Library:', '"clib3r"'),
            line('Processor:', '80386') + line('Memory model:', 'Flat') + line('Optimized:', 'yes') + line('Floating point:', 'inline 80x87'),
            line('Comment Class:', '0x9E DOSSEG') + 'OMF record',
            line('Subtype:', '0x01 IMPDEF') + line('Import:', 'internal="MyFunc" module="MYDLL" name=(internal)'),
            line('Import:', 'internal="F2" module="MYDLL" ordinal=5'),
            line('Subtype:', '0x02 EXPDEF') + line('Export:', 'name="Exp" ordinal=7 RESIDENT parameters=3'),
            line('Version:', '1') + line('Style:', '"CV" CodeView'),
            line('Comment Type:', '0x40 NO-LIST') + line('Comment Class:', '0xA2 Link pass separator') + line('Link pass:', '0x01 (end of pass 1)'),
            line('Extern:', '"weak"(1) default="strong"(2)'),
            line('File:', '"foo.h" 2024-03-15 12:34:56'),
            '    End of dependency list\n',
            line('Directive:', "'D' debug information version and source language") + line('Version:', '1.3') + line('Language:', '"C"'),
            line('Directive:', "'O' optimize far calls") + line('Segment:', '"_TEXT"(1)'),
            line('Directive:', "'s' scan table") + line('Segment:', '"_TEXT"(1)') + line('Range:', 'start=0x10 end=0x20'),
            line('Comment Class:', '0xA3 LIBMOD') + line('Data:', '03 66 6F 6F'),
            line('Comment Class:', '0x55 ?') + line('Data:', '00 FF')]:
        check(want in out, 'COMENT output is missing: ' + repr(want))

# The LIBHEAD record gives the dictionary offset, size, and flags. Check a page that fits in
# the record buffer and one that does not.
def test_omfdump_LIBHEAD(tools, tmp):
    for page in [32, 8192]:
        fields = bytes([0x00, 0x34, 0x12, 0x00, 3, 0, 0x01])                          # 0x123400, 3 blocks, case sensitive
        lib = bytearray(omf_record(0xF0, fields + bytes(page - 4 - len(fields))))
        lib += THEADR('m1') + MODEND()
        lib += bytes(-len(lib) % page)
        lib += omf_record(0xF1, bytes(page - 4))
        out = run_omfdump(tools, tmp, 'libhead.lib', bytes(lib))
        check('LIBHEAD page_size=%u dictionary_offset=0x123400 dictionary_blocks=3 flags=0x01 CASE-SENSITIVE\n' % page in out,
            'LIBHEAD with %u byte pages printed wrong' % page)

# omfdump -v must list the .LIB dictionary: each entry's name, and the page and file offset of its module.
def test_omfdump_LIBDICT(tools, tmp):
    page = 16
    mods = bytearray()
    pages = []
    for name in ['m1', 'm2']:
        pages.append((page + len(mods)) // page)
        mods += THEADR(name) + MODEND()
        mods += bytes(-len(mods) % page)
    libend = omf_record(0xF1, bytes(page - 4))
    dict_ofs = page + len(mods) + len(libend)
    blk = bytearray(512)
    blk[38:44] = lenstr('foo') + bytes([pages[0], 0])                               # bucket 5 -> word offset 19
    blk[44:50] = lenstr('m2!') + bytes([pages[1], 0])                               # bucket 20 -> word offset 22
    blk[5], blk[20], blk[30], blk[37] = 19, 22, 0xFF, 25                            # bucket 30 points past the end
    fields = bytes([dict_ofs & 0xFF, dict_ofs >> 8, 0, 0, 1, 0, 0])
    lib = omf_record(0xF0, fields + bytes(page - 4 - len(fields))) + bytes(mods) + libend + bytes(blk)
    out = run_omfdump(tools, tmp, 'libdict.lib', lib)
    for want in [
            'LIBDICT offset=0x%X blocks=1\n' % dict_ofs,
            '    [0.5] "foo" page=%u offset=0x%X\n' % (pages[0], pages[0] * page),
            '    [0.20] "m2!" page=%u offset=0x%X\n' % (pages[1], pages[1] * page),
            '    [0.30] [invalid entry]\n']:
        check(want in out, 'output is missing: ' + repr(want))

# omfdump must print LHEADR, LINNUM, LINSYM, BAKPAT, NBKPAT, ALIAS, VERNUM and VENDEXT records.
def test_omfdump_other_records(tools, tmp):
    def w(v):
        return bytes([v & 0xFF, v >> 8])
    def dw(v):
        return bytes([v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, v >> 24])
    obj = omf_record(0x82, lenstr('libmod'))                                        # LHEADR
    obj += LNAMES(['', 'DGROUP', '_TEXT', 'CODE', '_DATA', 'DATA', 'tmpl1'])        # LNAMES 1-7
    obj += SEGDEF(0x28, 0x20, 3, 4)
    obj += omf_record(0xCC, lenstr('1.0.0'))                                        # VERNUM
    obj += omf_record(0x94, bytes([0, SEG_TEXT]) +                                  # LINNUM
        w(10) + w(0x0) + w(11) + w(0x3) + w(12) + w(0x8) + w(13) + w(0xC) + w(14) + w(0x10))
    obj += omf_record(0x95, bytes([0, SEG_TEXT]) + w(20) + dw(0x12345678))         # LINNUM32
    obj += omf_record(0x94, bytes([0, 0]) + w(0xB800) + w(1) + w(0))                # LINNUM, absolute frame
    obj += omf_record(0xC4, bytes([0x01, 7]) + w(5) + w(0x2))                       # LINSYM
    obj += omf_record(0xB2, bytes([SEG_TEXT, 1]) + w(0x10) + w(0x4) + w(0x20) + w(0x8))  # BAKPAT
    obj += omf_record(0xC9, bytes([2, 7]) + dw(0x100) + dw(0x10))                   # NBKPAT32
    obj += omf_record(0xC6, lenstr('_old') + lenstr('_new') + lenstr('a2') + lenstr('b2'))  # ALIAS
    obj += omf_record(0xCE, w(0x1234) + bytes([1, 2, 3]))                           # VENDEXT
    obj += MODEND()
    out = run_omfdump(tools, tmp, 'other.obj', obj)
    for want in [
            'type=0x82 (LHEADR: Library Module Header Record)',
            '* THEADR: "libmod"',
            'VERNUM "1.0.0"\n',
            'LINNUM group=""(0) segment="_TEXT"(1)\n    10:0x0  11:0x3  12:0x8  13:0xC\n    14:0x10\n',
            'LINNUM group=""(0) segment="_TEXT"(1)\n    20:0x12345678\n',
            'LINNUM group=""(0) frame=0xB800\n    1:0x0\n',
            'LINSYM "tmpl1"(7) flags=0x01 CONTINUATION\n    5:0x2\n',
            'BAKPAT segment="_TEXT"(1) location=WORD(1)\n    offset=0x10 value=0x4\n    offset=0x20 value=0x8\n',
            'NBKPAT "tmpl1"(7) location=DWORD(2)\n    offset=0x100 value=0x10\n',
            'ALIAS:\n    "_old" -> "_new"\n    "a2" -> "b2"\n',
            'VENDEXT vendor=4660\n    0x00000000: 01 02 03 ']:
        check(want in out, 'output is missing: ' + repr(want))

# LEXTDEF32 (0xB5) is parsed like LEXTDEF, and omfdump must name it.
def test_omfdump_LEXTDEF32_name(tools, tmp):
    obj = THEADR('lext32') + LNAMES(['']) + omf_record(0xB5, lenstr('lext') + bytes([0])) + MODEND()
    out = run_omfdump(tools, tmp, 'lext32.obj', obj)
    check('type=0xb5 (LEXTDEF32: Local External Names Definition Record (32-bit))' in out, 'LEXTDEF32 not named')
    check('"lext" typeindex=0 LOCAL' in out, 'LEXTDEF32 symbol printed wrong')

# A FIXUPP record of only THREADs may come before any data record.
def test_omfsegdg_THREAD_before_data(tools, tmp):
    obj = module_header('thrfirst', 3)
    obj += omf_record(0x9C, bytes([0x00, SEG_DATA]))                               # target thread 0 = SEGDEF _DATA
    obj += LEDATA(SEG_TEXT, 0, [0xB8, 0, 0])                                       # mov ax,offset _DATA
    obj += omf_record(0x9C, LOCAT(LOC_OFFSET16, 1) + bytes([(5 << 4) | FIX_T | FIX_P]))
    obj += MODEND()
    fix = run_omfsegdg(tools, tmp, 'thrfirst', obj)[0]['fixups'][0]
    check((fix['target_method'], fix['target_index']) == (0, SEG_DATA), 'fixup has target T%u index %u' % (fix['target_method'], fix['target_index']))

# A LEDATA that no FIXUPP follows must still be written before MODEND.
def test_omfsegdg_LEDATA_before_MODEND(tools, tmp):
    obj = module_header('order', 1)
    obj += LEDATA(SEG_TEXT, 0, [0xC3])                                             # ret
    obj += MODEND()
    leds = run_omfsegdg(tools, tmp, 'order', obj)
    check(len(leds) == 1 and bytes(leds[0]['data']) == bytes([0xC3]), 'LEDATA is not before MODEND')

# The MODEND start address is like a FIXUP: which fields are present, and what the
# frame and target indexes refer to, depend on the End Data byte.
def test_omfdump_MODEND_start_address(tools, tmp):
    base = module_header('modend', 1) + omf_record(0x8C, lenstr('main_') + bytes([0])) + LEDATA(SEG_TEXT, 0, [0xC3])

    # main module, start address: frame GRPDEF 1 (DGROUP), target SEGDEF 1 (_TEXT) + 0x10
    out = run_omfdump(tools, tmp, 'modend1.obj', base + omf_record(0x8A, bytes([0xC1, (1 << 4) | 0, 1, SEG_TEXT, 0x10, 0x00])))
    check(' Start: frame_method=GRPDEF(1) frame_index="DGROUP"(1) target_method=SEGDEF(0) target_index="_TEXT"(1) target_displacement=0x10' in out,
        'GRPDEF frame start address printed wrong')

    # no start address
    out = run_omfdump(tools, tmp, 'modend2.obj', base + omf_record(0x8A, bytes([0x00])))
    check('Start=0' in out and ' Start:' not in out, 'module without a start address printed wrong')

    # frame from target, target EXTDEF 1 (main_), no displacement
    out = run_omfdump(tools, tmp, 'modend3.obj', base + omf_record(0x8A, bytes([0xC1, (5 << 4) | FIX_P | 2, 1])))
    check(' Start: frame_method=by-TARGET(5) target_method=EXTDEF(2) target_index="main_"(1) target_displacement=0x0' in out,
        'EXTDEF start address printed wrong')

TESTS = [
    test_lib_module_ends_after_page_boundary,
    test_lib_large_page_size,
    test_omfsegdg_F4_frame,
    test_omfsegdg_thread_in_later_FIXUPP,
    test_omfsegdg_segdef_in_DGROUP,
    test_LLNAMES_numbering,
    test_omfdump_SEGDEF_fields,
    test_SEGDEF_big_bit,
    test_SEGDEF32_big_bit,
    test_COMDEF_CEXTDEF_numbering,
    test_PUBDEF_absolute,
    test_omfsegdg_two_FIXUPPs_after_LEDATA,
    test_omfsegdg_FIXUPP_after_LIDATA,
    test_omfsegdg_FIXUPP_after_COMDAT,
    test_omfdump_COMDAT,
    test_omfdump_LIDATA,
    test_omfdump_LEXTDEF32_name,
    test_omfdump_COMENT,
    test_omfdump_LIBHEAD,
    test_omfdump_LIBDICT,
    test_omfdump_other_records,
    test_omfsegdg_THREAD_before_data,
    test_omfsegdg_LEDATA_before_MODEND,
    test_omfdump_MODEND_start_address,
]

def main():
    if len(sys.argv) > 1:
        tools = sys.argv[1]
    else:
        tools = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'linux-host')

    failed = 0
    with tempfile.TemporaryDirectory() as tmp:
        for test in TESTS:
            try:
                test(tools, tmp)
                print('PASS ' + test.__name__)
            except Exception as e:
                print('FAIL ' + test.__name__ + ': ' + str(e))
                failed += 1

    print('%u of %u tests passed' % (len(TESTS) - failed, len(TESTS)))
    return 1 if failed else 0

sys.exit(main())
