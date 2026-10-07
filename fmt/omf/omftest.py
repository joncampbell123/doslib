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

# returns a list of LEDATAs: { 'segdef', 'offset', 'data', 'fixups': [...] }
# FIXUPPs are attached to the LEDATA before them, THREADs are resolved,
# and frame method F4 is resolved to F0 (SEGDEF) of that LEDATA.
def read_ledatas(data):
    ledatas = []
    frame_threads = {}
    target_threads = {}
    for rectype, body in read_records(data):
        if rectype == 0xA0:
            segdef, pos = read_index(body, 0)
            offset = body[pos] | (body[pos+1] << 8)
            ledatas.append({ 'segdef': segdef, 'offset': offset, 'data': body[pos+2:], 'fixups': [] })
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
                    if fix['frame_method'] == 4:
                        fix['frame_method'] = 0
                        fix['frame_index'] = ledatas[-1]['segdef']
                    ledatas[-1]['fixups'].append(fix)
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
    return ledatas

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

def run_omfsegdg(tools, tmp, name, obj):
    src = os.path.join(tmp, name + '.obj')
    dst = os.path.join(tmp, name + '.out')
    with open(src, 'wb') as f:
        f.write(obj)
    r = run([os.path.join(tools, 'omfsegdg'), '-i', src, '-o', dst])
    check(r.returncode == 0, 'omfsegdg failed: ' + r.stdout.strip())
    with open(dst, 'rb') as f:
        return read_ledatas(f.read())

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

TESTS = [
    test_lib_module_ends_after_page_boundary,
    test_lib_large_page_size,
    test_omfsegdg_F4_frame,
    test_omfsegdg_thread_in_later_FIXUPP,
    test_omfsegdg_segdef_in_DGROUP,
    test_omfdump_SEGDEF_fields,
    test_SEGDEF_big_bit,
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
