#!/usr/bin/python3
#
# Regression tests for palquant.
#
# Each test makes small BMP files by hand, runs palquant on them, and checks
# the paletted BMP files it writes.
#
# Usage (after "make" has built linux-host/palquant):
#
#   python3 pqtest.py [path to palquant]
#
# Exit status is 0 if all tests pass, 1 otherwise.
import os
import random
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))

#------------------------------------------------------------------------
# Writing truecolor BMP files
#------------------------------------------------------------------------

# Header sizes by name. core12 is OS/2's BITMAPCOREHEADER.
HEADERS = {'core12': 12, 'info40': 40, 'v2_52': 52, 'v3_56': 56, 'v4_108': 108, 'v5_124': 124}

def channel(v, mask):
    # put an 8-bit value into a mask's bits, scaled to the mask's width
    if mask == 0:
        return 0
    shift = (mask & -mask).bit_length() - 1
    top = mask >> shift
    return (round(v * top / 255) << shift) & mask

def make_bmp(pixels, width, height, bpp=24, header='info40', compression=0, masks=None, topdown=False):
    # pixels are (r, g, b), top row first
    hsize = HEADERS[header]
    if masks is None:
        masks = (0x00FF0000, 0x0000FF00, 0x000000FF, 0)
    if header == 'core12':
        hdr = struct.pack('<IHHHH', 12, width, height, 1, bpp)
    else:
        hdr = struct.pack('<IiiHHIIiiII', hsize, width, -height if topdown else height, 1, bpp, compression, 0, 0, 0, 0, 0)
        if hsize > 40:
            hdr += struct.pack('<IIII', *masks)[:hsize - 40]
        hdr += bytes(hsize - len(hdr))
    if compression in (3, 6) and hsize == 40:
        hdr += struct.pack('<IIII', *masks)[:12 if compression == 3 else 16]
    stride = (width * bpp + 31) // 32 * 4
    rows = []
    for y in range(height):
        row = bytearray()
        for (r, g, b) in pixels[y * width:(y + 1) * width]:
            if bpp == 24:
                row += bytes([b, g, r])
            elif compression == 0:
                row += bytes([b, g, r, 0xA5])           # the 4th byte must be ignored
            else:
                v = channel(r, masks[0]) | channel(g, masks[1]) | channel(b, masks[2]) | (0x5A5A5A5A & masks[3])
                row += struct.pack('<I', v)
        row += bytes(stride - len(row))
        rows.append(bytes(row))
    if not topdown:
        rows.reverse()
    data = b''.join(rows)
    offset = 14 + len(hdr)
    return b'BM' + struct.pack('<IHHI', offset + len(data), 0, 0, offset) + hdr + data

#------------------------------------------------------------------------
# Reading the paletted BMP files that palquant writes
#------------------------------------------------------------------------

class Paletted:
    pass

def read_paletted(path):
    with open(path, 'rb') as f:
        b = f.read()
    check(b[0:2] == b'BM', 'output is not a BMP file')
    p = Paletted()
    offset = struct.unpack_from('<I', b, 10)[0]
    hsize, p.width, height, planes, p.bpp, comp, _, _, _, used, _ = struct.unpack_from('<IiiHHIIiiII', b, 14)
    check(hsize == 40 and comp == 0 and planes == 1, 'output header is not a plain BITMAPINFOHEADER')
    check(p.bpp in (1, 4, 8), 'output has %u bits per pixel' % p.bpp)
    check(used >= 1 and used <= (1 << p.bpp), 'output says it uses %u colors' % used)
    p.height = abs(height)
    p.palette = [(b[54 + i * 4 + 2], b[54 + i * 4 + 1], b[54 + i * 4]) for i in range(used)]
    check(offset == 54 + 4 * used, 'output pixels do not follow the palette')
    stride = (p.width * p.bpp + 31) // 32 * 4
    check(len(b) == offset + stride * p.height, 'output file is the wrong size')
    p.pixels = []
    for y in range(p.height):
        row = b[offset + stride * (p.height - 1 - y if height > 0 else y):]
        for x in range(p.width):
            if p.bpp == 8:
                i = row[x]
            elif p.bpp == 4:
                i = (row[x >> 1] >> (0 if x & 1 else 4)) & 15
            else:
                i = (row[x >> 3] >> (7 - (x & 7))) & 1
            check(i < used, 'pixel uses palette entry %u of %u' % (i, used))
            p.pixels.append(i)
    p.colors = [p.palette[i] for i in p.pixels]
    return p

#------------------------------------------------------------------------
# Running palquant
#------------------------------------------------------------------------

class TestFailed(Exception):
    pass

def check(cond, msg):
    if not cond:
        raise TestFailed(msg)

def distance(a, b):
    # the same color distance palquant uses
    return 3 * (a[0] - b[0]) ** 2 + 4 * (a[1] - b[1]) ** 2 + 2 * (a[2] - b[2]) ** 2

def write(tmp, name, data):
    path = os.path.join(tmp, name)
    with open(path, 'wb') as f:
        f.write(data)
    return path

def run(args, cwd=None):
    r = subprocess.run([PALQUANT] + args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, cwd=cwd)
    return r.returncode, r.stdout

def run_ok(args, cwd=None):
    rc, out = run(args, cwd)
    check(rc == 0, 'palquant failed: ' + out.strip())
    return out

def run_fails(args, message, cwd=None):
    rc, out = run(args, cwd)
    check(rc != 0, 'palquant did not fail: ' + ' '.join(args))
    check(message in out, 'palquant failed without saying "%s": %s' % (message, out.strip()))

def palette_image(colors, width, height):
    return [colors[i % len(colors)] for i in range(width * height)]

def random_image(width, height, seed):
    rnd = random.Random(seed)
    return [(rnd.randrange(256), rnd.randrange(256), rnd.randrange(256)) for i in range(width * height)]

#------------------------------------------------------------------------
# Tests
#------------------------------------------------------------------------

EIGHT = [(0, 0, 0), (255, 255, 255), (255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0), (12, 34, 56), (200, 100, 50)]

# If the images use no more colors than asked for, the palette is exactly those colors.
def test_exact_colors(tmp):
    pixels = palette_image(EIGHT, 9, 7)
    src = write(tmp, 'exact.bmp', make_bmp(pixels, 9, 7))
    out = os.path.join(tmp, 'exact_out.bmp')
    run_ok(['-c', '8', src, '-o', out])
    p = read_paletted(out)
    check(p.bpp == 4, '8 colors written with %u bits per pixel, not 4' % p.bpp)
    check(sorted(p.palette) == sorted(EIGHT), 'palette is not the 8 colors of the image: %s' % p.palette)
    check(p.colors == pixels, 'pixels are not exactly the colors of the image')

# The output has the fewest bits per pixel that hold the palette, unless -8 says 8.
def test_bits_per_pixel(tmp):
    for ncolors, args, want in [(2, [], 1), (16, [], 4), (17, [], 8), (2, ['-8'], 8), (300, ['-c', '2'], 1), (300, ['-c', '16'], 4)]:
        colors = [(i * 7 % 256, i * 13 % 256, i * 29 % 256) for i in range(ncolors)] if ncolors != 2 else [(0, 0, 0), (255, 255, 255)]
        pixels = palette_image(colors, 20, 15)
        src = write(tmp, 'bpp.bmp', make_bmp(pixels, 20, 15))
        out = os.path.join(tmp, 'bpp_out.bmp')
        run_ok(args + [src, '-o', out])
        p = read_paletted(out)
        check(p.bpp == want, '%u colors with %s: %u bits per pixel, not %u' % (ncolors, args, p.bpp, want))
        if ncolors <= 256:
            check(p.colors == pixels, '%u colors were not kept exactly' % ncolors)

# Every kind of 24bpp and 32bpp header and mask reads as the same colors.
# The odd width also checks that the rows' padding is skipped.
def test_input_formats(tmp):
    pixels = palette_image(EIGHT, 7, 5)
    rgba = (0x0000FF00, 0x00FF0000, 0xFF000000, 0x000000FF)           # R, G, B, A in other places
    cases = [
        ('24bpp OS/2 core header',              dict(bpp=24, header='core12')),
        ('24bpp',                               dict(bpp=24)),
        ('24bpp top row first',                 dict(bpp=24, topdown=True)),
        ('24bpp V5 header',                     dict(bpp=24, header='v5_124')),
        ('32bpp',                               dict(bpp=32)),
        ('32bpp top row first',                 dict(bpp=32, topdown=True)),
        ('32bpp BI_BITFIELDS after the header', dict(bpp=32, compression=3)),
        ('32bpp BI_BITFIELDS V3 header, RGBA',  dict(bpp=32, compression=3, header='v3_56', masks=rgba)),
        ('32bpp BI_BITFIELDS V4 header',        dict(bpp=32, compression=3, header='v4_108', masks=(0xFF0000, 0xFF00, 0xFF, 0xFF000000))),
        ('32bpp BI_BITFIELDS V5 header, RGBA',  dict(bpp=32, compression=3, header='v5_124', masks=rgba)),
        ('32bpp BI_ALPHABITFIELDS',             dict(bpp=32, compression=6, masks=rgba)),
    ]
    for name, kw in cases:
        src = write(tmp, 'fmt.bmp', make_bmp(pixels, 7, 5, **kw))
        out = os.path.join(tmp, 'fmt_out.bmp')
        run_ok([src, '-o', out])
        p = read_paletted(out)
        check((p.width, p.height) == (7, 5), '%s: size is %ux%u' % (name, p.width, p.height))
        check(p.colors == pixels, '%s: colors are wrong' % name)

    # 5-bit masks are scaled to 8 bits
    masks = (0x7C00, 0x03E0, 0x001F, 0)
    src = write(tmp, 'fmt5.bmp', make_bmp(pixels, 7, 5, bpp=32, compression=3, masks=masks))
    out = os.path.join(tmp, 'fmt5_out.bmp')
    run_ok([src, '-o', out])
    want = [tuple(round(round(v * 31 / 255) * 255 / 31) for v in c) for c in pixels]
    check(read_paletted(out).colors == want, '5-bit masks are not scaled to 8 bits')

# Without dithering, each pixel is the nearest palette entry to its color.
def test_nearest(tmp):
    pixels = random_image(32, 32, 1)
    src = write(tmp, 'near.bmp', make_bmp(pixels, 32, 32))
    out = os.path.join(tmp, 'near_out.bmp')
    run_ok(['-c', '16', src, '-o', out])
    p = read_paletted(out)
    check(len(p.palette) <= 16, '%u palette entries, not 16 or fewer' % len(p.palette))
    for c, got in zip(pixels, p.colors):
        best = min(distance(c, e) for e in p.palette)
        check(distance(c, got) == best, '%s became %s, but the palette has a nearer color' % (c, got))

# Several images share one palette, made from the colors of all of them.
def test_shared_palette(tmp):
    reds = [(100 + i, 10, 20) for i in range(150)]
    blues = [(10, 20, 100 + i) for i in range(150)]
    a = write(tmp, 'reds.bmp', make_bmp(palette_image(reds, 30, 10), 30, 10))
    b = write(tmp, 'blues.bmp', make_bmp(palette_image(blues, 30, 10), 30, 10, bpp=32))
    run_ok(['-c', '4', a, b])
    pa = read_paletted(os.path.join(tmp, 'reds_pq.bmp'))
    pb = read_paletted(os.path.join(tmp, 'blues_pq.bmp'))
    check(pa.palette == pb.palette, 'the two images do not have the same palette')
    check(any(r > b for (r, g, b) in pa.palette) and any(b > r for (r, g, b) in pa.palette),
        'palette does not have both a red and a blue: %s' % pa.palette)

def gray_gradient(width, height):
    return [(x * 255 // (width - 1),) * 3 for y in range(height) for x in range(width)]

def block_means(colors, width, height, bw):
    return [sum(colors[y * width + x][0] for y in range(height) for x in range(bx, bx + bw)) / (bw * height)
            for bx in range(0, width, bw)]

# Dithering keeps the average color of each area close to the image's, where plain mapping cannot.
def test_dither(tmp):
    w, h = 256, 32
    pixels = gray_gradient(w, h)
    src = write(tmp, 'grad.bmp', make_bmp(pixels, w, h))
    plain = os.path.join(tmp, 'grad_plain.bmp')
    dith = os.path.join(tmp, 'grad_dith.bmp')
    run_ok(['-c', '2', src, '-o', plain])
    run_ok(['-c', '2', '-d', src, '-o', dith])
    pp = read_paletted(plain)
    pd = read_paletted(dith)
    check(pp.palette == pd.palette and len(pp.palette) == 2, 'palettes are not the same 2 colors')
    # dithering can only make averages between the two palette colors, so check the areas in that range
    lo = min(c[0] for c in pd.palette) + 8
    hi = max(c[0] for c in pd.palette) - 8
    want = block_means(pixels, w, h, 16)
    got_plain = block_means(pp.colors, w, h, 16)
    got_dith = block_means(pd.colors, w, h, 16)
    inside = [i for i in range(len(want)) if lo <= want[i] <= hi]
    check(len(inside) >= 4, 'only %u areas are between the palette colors %s' % (len(inside), pd.palette))
    worst_dith = max(abs(want[i] - got_dith[i]) for i in inside)
    worst_plain = max(abs(want[i] - got_plain[i]) for i in inside)
    check(worst_dith < 12, 'dithered areas are up to %.1f away from the image' % worst_dith)
    check(worst_plain > 40, 'plain areas are only up to %.1f away, so the test shows nothing' % worst_plain)

def read_truecolor(path):
    # the sample picture is a plain 24bpp BMP
    with open(path, 'rb') as f:
        b = f.read()
    offset = struct.unpack_from('<I', b, 10)[0]
    w, h = struct.unpack_from('<ii', b, 18)
    stride = (w * 3 + 3) // 4 * 4
    return w, h, [tuple(b[offset + stride * (h - 1 - y) + x * 3 + k] for k in (2, 1, 0)) for y in range(h) for x in range(w)]

def blur(pixels, w, h, r=2):
    out = []
    for y in range(h):
        for x in range(w):
            s = [0, 0, 0]
            n = 0
            for yy in range(max(0, y - r), min(h, y + r + 1)):
                for xx in range(max(0, x - r), min(w, x + r + 1)):
                    p = pixels[yy * w + xx]
                    s[0] += p[0]
                    s[1] += p[1]
                    s[2] += p[2]
                    n += 1
            out.append((s[0] / n, s[1] / n, s[2] / n))
    return out

# On a photo, dithering brings the colors of each area, seen from a distance (blurred), closer to the photo's.
def test_dither_photo(tmp):
    src = os.path.join(HERE, '..', 'palq', 'cat1_36.bmp')
    if not os.path.exists(src):
        return
    w, h, pixels = read_truecolor(src)
    want = blur(pixels, w, h)
    errs = []
    for args in ([], ['-d']):
        out = os.path.join(tmp, 'photo.bmp')
        run_ok(args + ['-c', '16', src, '-o', out])
        got = blur(read_paletted(out).colors, w, h)
        errs.append(sum(distance(a, b) for a, b in zip(want, got)))
    check(errs[1] < errs[0] * 0.9, 'dithered error %u is not well below the plain error %u' % (errs[1], errs[0]))

# On a smooth gradient, the palette must do better than an even 16-color grid.
def test_quality(tmp):
    w, h = 64, 64
    pixels = [(x * 4, y * 4, (x + y) * 2) for y in range(h) for x in range(w)]
    src = write(tmp, 'q.bmp', make_bmp(pixels, w, h))
    out = os.path.join(tmp, 'q_out.bmp')
    run_ok(['-c', '16', src, '-o', out])
    ours = sum(distance(a, b) for a, b in zip(pixels, read_paletted(out).colors))
    grid = [(r, g, b) for r in (64, 191) for g in (32, 96, 160, 223) for b in (64, 191)]
    even = sum(min(distance(c, e) for e in grid) for c in pixels)
    check(ours < even, 'error %u is not less than an even grid of colors (%u)' % (ours, even))

# The same input gives the same output.
def test_deterministic(tmp):
    src = write(tmp, 'det.bmp', make_bmp(random_image(40, 30, 2), 40, 30))
    outs = []
    for i in range(2):
        out = os.path.join(tmp, 'det_out%u.bmp' % i)
        run_ok(['-c', '37', '-d', src, '-o', out])
        with open(out, 'rb') as f:
            outs.append(f.read())
    check(outs[0] == outs[1], 'two runs wrote different files')

# Without -o, the output is the input's name with _pq added.
def test_default_output_names(tmp):
    data = make_bmp(palette_image(EIGHT, 4, 4), 4, 4)
    for name, want in [('a.bmp', 'a_pq.bmp'), ('b.BMP', 'b_pq.BMP'), ('c', 'c_pq.bmp')]:
        write(tmp, name, data)
        run_ok([name], cwd=tmp)
        check(os.path.exists(os.path.join(tmp, want)), '%s was not written to %s' % (name, want))

# Bad input and bad options are errors that say what is wrong, and write nothing.
def test_errors(tmp):
    good = write(tmp, 'good.bmp', make_bmp(palette_image(EIGHT, 4, 4), 4, 4))
    with open(good, 'rb') as f:
        good_data = f.read()
    pal8 = bytearray(good_data)
    struct.pack_into('<H', pal8, 28, 8)
    rle = bytearray(good_data)
    struct.pack_into('<I', rle, 30, 1)
    cases = [
        ('pal8.bmp', bytes(pal8), '8 bits per pixel'),
        ('rle.bmp', bytes(rle), 'compressed BMP'),
        ('short.bmp', good_data[:-20], 'too short'),
        ('notbmp.bmp', b'GIF89a' + bytes(60), 'not a BMP file'),
    ]
    for name, data, message in cases:
        src = write(tmp, name, data)
        out = os.path.join(tmp, 'err_out.bmp')
        run_fails([src, '-o', out], message)
        check(not os.path.exists(out), '%s: an output file was written' % name)

    run_fails(['-c', '1', good], 'number of colors')
    run_fails(['-c', '257', good], 'number of colors')
    run_fails(['-c', 'many', good], 'number of colors')
    run_fails(['-o', os.path.join(tmp, 'x.bmp'), good], '-o goes after')
    run_fails([good, '-o', good], 'write over the input')
    run_fails([good, '-o', os.path.join(tmp, '.', 'good.bmp')], 'write over the input')
    two = write(tmp, 'two.bmp', good_data)
    run_fails([good, '-o', os.path.join(tmp, 'same.bmp'), two, '-o', os.path.join(tmp, 'same.bmp')], 'same output file')
    run_fails([os.path.join(tmp, 'missing.bmp')], 'No such file')
    with open(good, 'rb') as f:
        check(f.read() == good_data, 'the input file was changed')

# The sample pictures in ../palq, a 24bpp one and a 32bpp BI_BITFIELDS one, together.
def test_samples(tmp):
    a = os.path.join(HERE, '..', 'palq', 'cat1_36.bmp')
    b = os.path.join(HERE, '..', 'palq', 'cat1b.bmp')
    if not (os.path.exists(a) and os.path.exists(b)):
        return
    oa = os.path.join(tmp, 'cat_a.bmp')
    ob = os.path.join(tmp, 'cat_b.bmp')
    run_ok(['-c', '64', '-d', a, '-o', oa, b, '-o', ob])
    pa = read_paletted(oa)
    pb = read_paletted(ob)
    check((pa.width, pa.height, pb.width, pb.height) == (120, 96, 1280, 1024), 'sample sizes are wrong')
    check(pa.palette == pb.palette and len(pa.palette) <= 64 and pa.bpp == 8, 'sample palettes are wrong')

TESTS = [
    test_exact_colors,
    test_bits_per_pixel,
    test_input_formats,
    test_nearest,
    test_shared_palette,
    test_dither,
    test_dither_photo,
    test_quality,
    test_deterministic,
    test_default_output_names,
    test_errors,
    test_samples,
]

def main():
    global PALQUANT
    PALQUANT = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, 'linux-host', 'palquant')

    failed = 0
    for test in TESTS:
        with tempfile.TemporaryDirectory() as tmp:
            try:
                test(tmp)
                print('PASS ' + test.__name__)
            except Exception as e:
                print('FAIL ' + test.__name__ + ': ' + str(e))
                failed += 1

    print('%u of %u tests passed' % (len(TESTS) - failed, len(TESTS)))
    return 1 if failed else 0

sys.exit(main())
