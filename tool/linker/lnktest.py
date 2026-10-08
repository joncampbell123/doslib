#!/usr/bin/python3
#
# Regression tests for the lnkdos16 linkers (tool/linker and tool/linkplus).
#
# Each test builds small OMF .OBJ files by hand, links them into a .COM file
# with each linker, and checks the result.
#
# Usage (after both linkers have been built for the Linux host):
#
#   python3 lnktest.py [linker ...]
#
# The linkers default to tool/linker/linux-host/lnkdos16 and
# tool/linkplus/linux-host/lnkdos16.
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

def w(v):
    return bytes([v & 0xFF, v >> 8])

def THEADR(name):
    return omf_record(0x80, lenstr(name))

def LNAMES(names):
    return omf_record(0x96, b''.join(lenstr(n) for n in names))

def SEGDEF(acbp, length, name_index, class_index):
    return omf_record(0x98, bytes([acbp]) + w(length) + bytes([name_index, class_index, 1]))

def GRPDEF(name_index, segdefs):
    return omf_record(0x9A, bytes([name_index]) + b''.join(bytes([0xFF, s]) for s in segdefs))

def EXTDEF(names):
    return omf_record(0x8C, b''.join(lenstr(n) + bytes([0]) for n in names))

def CEXTDEF(lname_indexes):
    return omf_record(0xBC, b''.join(bytes([i, 0]) for i in lname_indexes))

def PUBDEF(group, segdef, name, offset):
    return omf_record(0x90, bytes([group, segdef]) + lenstr(name) + w(offset) + bytes([0]))

def LEDATA(segdef, offset, data):
    return omf_record(0xA0, bytes([segdef]) + w(offset) + bytes(data))

def LIDATA(segdef, offset, blocks):
    return omf_record(0xA2, bytes([segdef]) + w(offset) + bytes(blocks))

# COMDAT: flags, attributes (selection << 4 | allocation), align, data offset, type index,
# public base (group, segment) if explicit allocation, LNAMES index of the name, data
def COMDAT(flags, attributes, name_index, data, offset=0, group=1, segdef=1, align=0):
    base = bytes([group, segdef]) if (attributes & 0x0F) == 0 else b''
    return omf_record(0xC2, bytes([flags, attributes, align]) + w(offset) + bytes([0]) + base + bytes([name_index]) + bytes(data))

# FIXUPP with 16-bit offset fixups. Each fixup is (data record offset, self relative, Fix Data byte, datums...)
def FIXUPP(fixups):
    body = b''
    for ofs, self_rel, fixdat, datums in fixups:
        body += bytes([0x80 | (0x00 if self_rel else 0x40) | (1 << 2) | (ofs >> 8), ofs & 0xFF, fixdat]) + bytes(datums)
    return omf_record(0x9C, body)

FIX_TARGET_EXTDEF = (5 << 4) | 0x04 | 2     # frame by target, target EXTDEF, no displacement
FIX_DGROUP_SEGDEF = (1 << 4) | 0x04 | 0     # frame GRPDEF, target SEGDEF, no displacement

def MODEND_start():
    # main module, start address at DGROUP:_TEXT+0
    return omf_record(0x8A, bytes([0xC1, (1 << 4) | 0, 1, 1]) + w(0))

def MODEND():
    return omf_record(0x8A, bytes([0x00]))

# The same names for every hand made module:
#   LNAMES: 1="" 2=DGROUP 3=_TEXT 4=CODE 5=_DATA 6=DATA, then the ones a test adds
#   SEGDEF: 1=_TEXT, 2=_DATA if there is data, all in DGROUP
NAMES = ['', 'DGROUP', '_TEXT', 'CODE', '_DATA', 'DATA']

def module_header(name, text_length, data_length=None, names=()):
    obj = THEADR(name) + LNAMES(NAMES + list(names))
    obj += SEGDEF(0x28, text_length, 3, 4)                  # byte aligned, public
    if data_length is None:
        return obj + GRPDEF(2, [1])
    return obj + SEGDEF(0x48, data_length, 5, 6) + GRPDEF(2, [1, 2])   # word aligned, public

#------------------------------------------------------------------------
# Running the linkers
#------------------------------------------------------------------------

class TestFailed(Exception):
    pass

def check(cond, msg):
    if not cond:
        raise TestFailed(msg)

# link the objects into a .COM at 0x100 (or an .EXE), return (exit status, output, executable contents)
def link(linker, tmp, objs, fmt='com'):
    args = [linker]
    for i, obj in enumerate(objs):
        path = os.path.join(tmp, 'm%u.obj' % i)
        with open(path, 'wb') as f:
            f.write(obj)
        args += ['-i', path]
    out = os.path.join(tmp, 'out.' + fmt)
    if os.path.exists(out):
        os.unlink(out)
    args += ['-o', out, '-of', fmt]
    if fmt == 'com':
        args += ['-com100']
    r = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
    image = None
    if os.path.exists(out):
        with open(out, 'rb') as f:
            image = f.read()
    return r.returncode, r.stdout, image

def link_ok(linker, tmp, objs, fmt='com'):
    rc, out, image = link(linker, tmp, objs, fmt)
    check(rc == 0 and image is not None, 'link failed: ' + out.strip())
    return image

def link_fails(linker, tmp, objs, message):
    rc, out, image = link(linker, tmp, objs)
    check(rc != 0, 'link did not fail')
    check(message in out, 'link failed without saying "%s": %s' % (message, out.strip()))

#------------------------------------------------------------------------
# Tests
#------------------------------------------------------------------------

# LIDATA is expanded into the segment.
def test_LIDATA(linker, tmp):
    obj = module_header('lidata', 4, 6)
    obj += LEDATA(1, 0, [0xB8, 0, 0, 0xC3])                                         # mov ax,offset _DATA / ret
    obj += FIXUPP([(1, False, FIX_DGROUP_SEGDEF, [1, 2])])
    obj += LIDATA(2, 0, [3, 0, 0, 0, 2, 0xAA, 0xBB])                                # 3 x AA BB
    obj += MODEND_start()
    image = link_ok(linker, tmp, [obj])
    check(image == bytes([0xB8, 0x04, 0x01, 0xC3, 0xAA, 0xBB, 0xAA, 0xBB, 0xAA, 0xBB]), 'wrong image: ' + image.hex())

# Like Open Watcom's linker, fixups in LIDATA are not supported. The linker must say so.
def test_LIDATA_FIXUPP(linker, tmp):
    obj = module_header('lidatafix', 1, 4)
    obj += LEDATA(1, 0, [0xC3])
    obj += LIDATA(2, 0, [2, 0, 0, 0, 2, 0, 0])                                      # 2 x dw offset _DATA
    obj += FIXUPP([(5, False, FIX_DGROUP_SEGDEF, [1, 2])])
    obj += MODEND_start()
    link_fails(linker, tmp, [obj], 'FIXUPP in LIDATA')

# A COMDAT defined in two files is linked in once, from the first file. The call to it from the first
# file must reach it. The second copy has a fixup to a symbol that does not exist, which must be dropped.
def test_COMDAT_first_definition(linker, tmp):
    a = module_header('a', 4, 2, ['inl'])                                           # LNAMES 7 = inl
    a += CEXTDEF([7])                                                               # EXTDEF 1 = inl
    a += LEDATA(1, 0, [0xE8, 0, 0, 0xC3])                                           # call inl / ret
    a += FIXUPP([(1, True, FIX_TARGET_EXTDEF, [1])])
    a += COMDAT(0x00, 0x10, 7, [0xB8, 0, 0, 0xC3])                                  # pick any: mov ax,offset _DATA / ret
    a += FIXUPP([(1, False, FIX_DGROUP_SEGDEF, [1, 2])])
    a += LEDATA(2, 0, b'HI')
    a += MODEND_start()
    b = module_header('b', 0, None, ['inl'])                                        # LNAMES 7 = inl
    b += EXTDEF(['nowhere'])
    b += COMDAT(0x00, 0x10, 7, [0x90, 0x90, 0x90, 0xC3])                            # pick any, different code
    b += FIXUPP([(1, False, FIX_TARGET_EXTDEF, [1])])                               # to a symbol that does not exist
    b += MODEND()
    image = link_ok(linker, tmp, [a, b])
    check(image == bytes([0xE8, 0x01, 0x00, 0xC3, 0xB8, 0x08, 0x01, 0xC3]) + b'HI', 'wrong image: ' + image.hex())

# A NO-MATCH COMDAT may only be defined once.
def test_COMDAT_no_match(linker, tmp):
    a = module_header('a', 1, None, ['dup']) + LEDATA(1, 0, [0xC3]) + COMDAT(0x00, 0x00, 7, [1]) + MODEND_start()
    b = module_header('b', 0, None, ['dup']) + COMDAT(0x00, 0x00, 7, [2]) + MODEND()
    link_fails(linker, tmp, [a, b], "COMDAT 'dup' defined more than once")

# A COMDAT continued in a second record, and an iterated COMDAT. The LEDATA and the PUBDEF of the
# module's own _TEXT come after them, and must still go to the module's own part of _TEXT.
def test_COMDAT_continuation_iterated(linker, tmp):
    obj = module_header('cont', 7, None, ['blob', 'rep'])                           # LNAMES 7 = blob, 8 = rep
    obj += COMDAT(0x00, 0x10, 7, b'AB')
    obj += COMDAT(0x01, 0x10, 7, b'CD', offset=2)                                   # continuation
    obj += COMDAT(0x02, 0x10, 8, [3, 0, 0, 0, 2, ord('x'), ord('y')])               # iterated: 3 x "xy"
    obj += CEXTDEF([8]) + EXTDEF(['here'])                                          # EXTDEF 1 = rep, 2 = here
    obj += LEDATA(1, 0, [0xB8, 0, 0, 0xBB, 0, 0, 0xC3])                             # mov ax,offset rep / mov bx,offset here / ret
    obj += FIXUPP([(1, False, FIX_TARGET_EXTDEF, [1]), (4, False, FIX_TARGET_EXTDEF, [2])])
    obj += PUBDEF(1, 1, 'here', 6)
    obj += MODEND_start()
    image = link_ok(linker, tmp, [obj])
    check(image == bytes([0xB8, 0x0B, 0x01, 0xBB, 0x06, 0x01, 0xC3]) + b'ABCD' + b'xyxyxy', 'wrong image: ' + image.hex())

# Like Open Watcom's linker, only COMDATs that name their segment are supported. The linker must say so.
def test_COMDAT_far_code(linker, tmp):
    obj = module_header('far', 1, None, ['fc']) + LEDATA(1, 0, [0xC3]) + COMDAT(0x00, 0x11, 7, [0xC3]) + MODEND_start()
    link_fails(linker, tmp, [obj], 'FAR-CODE allocation is not supported')

# Local COMDATs of the same name in two modules are different COMDATs. Each module must reach its own.
def test_COMDAT_local(linker, tmp):
    a = module_header('a', 4, None, ['lcl']) + CEXTDEF([7])
    a += COMDAT(0x04, 0x00, 7, b'A1')                                               # local, no match
    a += LEDATA(1, 0, [0xB8, 0, 0, 0xC3]) + FIXUPP([(1, False, FIX_TARGET_EXTDEF, [1])])
    a += MODEND_start()
    b = module_header('b', 4, None, ['lcl']) + CEXTDEF([7])
    b += COMDAT(0x04, 0x00, 7, b'B2')
    b += LEDATA(1, 0, [0xB8, 0, 0, 0xC3]) + FIXUPP([(1, False, FIX_TARGET_EXTDEF, [1])])
    b += MODEND()
    image = link_ok(linker, tmp, [a, b])
    check(image == bytes([0xB8, 0x04, 0x01, 0xC3]) + b'A1' + bytes([0xB8, 0x0A, 0x01, 0xC3]) + b'B2', 'wrong image: ' + image.hex())

# A fixup to a SEGDEF is relative to this module's part of the segment, not the start of it.
# Here the second module refers to its own _DATA.
def test_SEGDEF_target_second_module(linker, tmp):
    a = module_header('a', 4, 4) + LEDATA(1, 0, [0xB8, 0, 0, 0xC3])                # mov ax,offset _DATA / ret
    a += FIXUPP([(1, False, FIX_DGROUP_SEGDEF, [1, 2])]) + LEDATA(2, 0, b'AAAA') + MODEND_start()
    b = module_header('b', 4, 4) + LEDATA(1, 0, [0xB8, 2, 0, 0xC3])                 # mov ax,offset _DATA+2 / ret
    b += FIXUPP([(1, False, FIX_DGROUP_SEGDEF, [1, 2])]) + LEDATA(2, 0, b'BBBB') + MODEND()
    image = link_ok(linker, tmp, [a, b])
    check(image == bytes([0xB8, 0x08, 0x01, 0xC3, 0xB8, 0x0E, 0x01, 0xC3]) + b'AAAABBBB', 'wrong image: ' + image.hex())

# A file that is not OMF (this is the start of a COFF object) must stop the link with an error.
def test_not_OMF(linker, tmp):
    link_fails(linker, tmp, [bytes.fromhex('4c010100ed96386a3c000000050000000000')], "Error reading")

# With nothing to link, an .EXE is just the 32-byte header.
def test_empty_EXE(linker, tmp):
    image = link_ok(linker, tmp, [THEADR('empty') + MODEND()], 'exe')
    check(len(image) == 32 and image[0:2] == b'MZ', 'wrong image: ' + image.hex())

# A segment with no LEDATA for it is zeros.
def test_segment_without_data(linker, tmp):
    obj = module_header('nodata', 1, 4) + LEDATA(1, 0, [0xC3]) + MODEND_start()     # _DATA has no LEDATA
    image = link_ok(linker, tmp, [obj])
    check(image == bytes([0xC3, 0, 0, 0, 0, 0]), 'wrong image: ' + image.hex())

TESTS = [
    test_LIDATA,
    test_LIDATA_FIXUPP,
    test_COMDAT_first_definition,
    test_COMDAT_no_match,
    test_COMDAT_continuation_iterated,
    test_COMDAT_far_code,
    test_COMDAT_local,
    test_SEGDEF_target_second_module,
    test_not_OMF,
    test_empty_EXE,
    test_segment_without_data,
]

def main():
    here = os.path.dirname(os.path.abspath(__file__))
    linkers = sys.argv[1:]
    if not linkers:
        linkers = [os.path.join(here, 'linux-host', 'lnkdos16'),
                   os.path.join(here, '..', 'linkplus', 'linux-host', 'lnkdos16')]

    failed = 0
    count = 0
    with tempfile.TemporaryDirectory() as tmp:
        for linker in linkers:
            for test in TESTS:
                count += 1
                try:
                    test(linker, tmp)
                    print('PASS %s %s' % (linker, test.__name__))
                except Exception as e:
                    print('FAIL %s %s: %s' % (linker, test.__name__, str(e)))
                    failed += 1

    print('%u of %u tests passed' % (count - failed, count))
    return 1 if failed else 0

sys.exit(main())
