#!/usr/bin/python3
#
# Tests for msvideo-1-fixer.
#
# Each test makes a short video of a clean picture: a disc, a square turned at an angle and lines at
# angles of all kinds (the edges CRAM turns into steps), a ramp (which it turns into squares), and some
# noise (which it turns into speckles), and codes it as CRAM: with FFmpeg's Microsoft Video 1 encoder, which
# makes most blocks one color, and with an encoder here in the way of Microsoft's, which splits each block
# at its mean brightness into two colors, or each quarter of it, and skips the blocks that have not changed.
# Then it checks that msvideo-1-fixer brings back something nearer the clean picture than the CRAM, nearer
# still at the edges and in the ramp, with fewer speckles, each block keeping the color it was coded as;
# that it reads how each block was coded as FFmpeg's decoder does; and that the frames, their timing and
# the audio are as they were.
# It needs numpy, and ffmpeg and ffprobe to make and look at the test videos. If the samples of CRAM from
# a capture, in ~/lotm, are there, it tries those too.
#
# Usage (after "make" has built linux-host/msvideo-1-fixer):
#
#   python3 mv1test.py [path to msvideo-1-fixer]
#
# Exit status is 0 if all tests pass, 1 otherwise.
import os
import subprocess
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'linux-host', 'msvideo-1-fixer')

WIDTH = 160
HEIGHT = 120
FRAMES = 24
RATE = '15'
SS = 4                                      # the picture is drawn this many times bigger, and scaled down, for soft edges

failures = []

def check(cond, what):
    print(('  ok    ' if cond else '  FAIL  ') + what)
    if not cond:
        failures.append(what)

# where each part of the picture is, as masks of the full size
def regions():
    yy, xx = np.mgrid[0:HEIGHT, 0:WIDTH]
    edges = np.zeros((HEIGHT, WIDTH), bool)
    edges[4:60, 4:156] = True
    ramp = np.zeros((HEIGHT, WIDTH), bool)
    ramp[68:116, 4:76] = True
    noise = np.zeros((HEIGHT, WIDTH), bool)
    noise[68:116, 84:156] = True
    return edges, ramp, noise

def clean_picture(rng, k):
    # drawn big, then each SSxSS the mean, so that the edges are as soft as a camera's
    h, w = HEIGHT * SS, WIDTH * SS
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64) / SS
    img = np.zeros((h, w, 3))
    img[...] = (40, 50, 90)
    # a disc, moving
    cx, cy = 30 + k * 0.5, 32
    img[((xx - cx) ** 2 + (yy - cy) ** 2) < 20 ** 2] = (230, 200, 60)
    # a square turned 20 degrees, turning
    a = np.radians(20 + k)
    u = (xx - 80) * np.cos(a) + (yy - 32) * np.sin(a)
    v = -(xx - 80) * np.sin(a) + (yy - 32) * np.cos(a)
    img[(np.abs(u) < 16) & (np.abs(v) < 16)] = (220, 60, 50)
    # lines two pixels wide at angles from 10 to 70 degrees
    for i, deg in enumerate((10, 25, 40, 55, 70)):
        a = np.radians(deg)
        x0 = 110 + i * 9
        d = (xx - x0) * np.sin(a) - (yy - 8) * np.cos(a)
        img[(np.abs(d) < 1.0) & (yy > 6) & (yy < 58) & (xx > 104)] = (240, 240, 240)
    img = img.reshape(HEIGHT, SS, WIDTH, SS, 3).mean(axis=(1, 3))
    # a ramp across, a color one way and the brightness the other
    y, x = np.mgrid[68:116, 4:76]
    img[68:116, 4:76, 0] = 30 + (x - 4) * 2.5
    img[68:116, 4:76, 1] = 40 + (y - 68) * 3.0
    img[68:116, 4:76, 2] = 120
    # a flat gray, with noise
    img[68:116, 84:156] = 128 + rng.normal(0, 12, (48, 72, 1))
    return np.clip(img, 0, 255)

def write_video(path, frames, audio=True):
    raw = path + '.rgb'
    with open(raw, 'wb') as f:
        for img in frames:
            f.write(np.clip(np.round(img), 0, 255).astype(np.uint8).tobytes())
    cmd = ['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', '%dx%d' % (WIDTH, HEIGHT), '-r', RATE, '-i', raw]
    if audio:
        cmd += ['-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=22050:duration=%f' % (FRAMES / 15.0)]
    cmd += ['-pix_fmt', 'rgb555le', '-c:v', 'msvideo1'] + (['-c:a', 'pcm_s16le'] if audio else []) + [path]
    subprocess.run(cmd, check=True)
    os.unlink(raw)

# CRAM in the way of Microsoft's encoder, 15-bit: each block one color if it is near enough to its mean, else
# split at its mean brightness into two colors, each the mean of its pixels, if that is near enough, else each
# quarter split into two colors; and the blocks that are as they were in the frame before skipped. Gives the
# packets, and how each block of each frame was coded: 0 skipped, 1 one color, 2 two colors, 3 eight colors.
def ms_encode(frames, solid_mse=30.0, two_mse=250.0):
    def q5(c):
        return np.clip(np.round(np.asarray(c) * 31 / 255), 0, 31).astype(int)
    def ex(q):
        return (q << 3) | (q >> 2)
    def w16(c):
        return int((c[0] << 10) | (c[1] << 5) | c[2])
    def split(px):
        y = px @ np.array([0.299, 0.587, 0.114])
        lab = (y > y.mean()).astype(int)
        if lab.min() == lab.max():
            c = q5(px.mean(0))
            return lab, c, c
        return lab, q5(px[lab == 0].mean(0)), q5(px[lab == 1].mean(0))
    def err(px, rec):
        return ((px - rec) ** 2).mean()
    packets, modes = [], []
    prev = None
    bw, bh = WIDTH // 4, HEIGHT // 4
    for f in frames:
        f = np.round(f)
        out = bytearray()
        mode = np.zeros((bh, bw), int)
        skip = 0
        def flush():
            nonlocal skip
            while skip > 0:
                n = min(skip, 0x3FF)
                out.extend(bytes((n & 0xFF, 0x84 + (n >> 8))))
                skip -= n
        # the blocks go left to right, from the bottom row of blocks up, and the pixels of each block too
        for by in range(bh - 1, -1, -1):
            for bx in range(bw):
                blk = f[by * 4:by * 4 + 4, bx * 4:bx * 4 + 4]
                if prev is not None and np.array_equal(blk, prev[by * 4:by * 4 + 4, bx * 4:bx * 4 + 4]):
                    skip += 1
                    continue
                flush()
                px = blk[::-1].reshape(16, 3)
                cs = q5(px.mean(0))
                lab, c0, c1 = split(px)
                if err(px, ex(cs)) <= solid_mse:
                    # one color, the top bit set; red of 1 would make it a skip code
                    if cs[0] == 1:
                        cs[0] = 0 if px[:, 0].mean() < 8.2 else 2
                    out.extend((w16(cs) | 0x8000).to_bytes(2, 'little'))
                    mode[by, bx] = 1
                elif err(px, np.where(lab[:, None] == 1, ex(c1), ex(c0))) <= two_mse:
                    # a bit of 1 is the first color, 0 the second, and the bit of the last pixel must be 0
                    if lab[15] == 1:
                        lab, c0, c1 = 1 - lab, c1, c0
                    flags = sum(1 << i for i in range(16) if lab[i])
                    out.extend(flags.to_bytes(2, 'little') + w16(c1).to_bytes(2, 'little') + w16(c0).to_bytes(2, 'little'))
                    mode[by, bx] = 2
                else:
                    flags, cols = 0, [None] * 8
                    for qy in range(2):
                        for qx in range(2):
                            idx = [(qy * 2 + yy) * 4 + qx * 2 + xx for yy in range(2) for xx in range(2)]
                            ql, a0, a1 = split(px[idx])
                            if 15 in idx and ql[idx.index(15)] == 1:
                                ql, a0, a1 = 1 - ql, a1, a0
                            for k, i in enumerate(idx):
                                if ql[k]:
                                    flags |= 1 << i
                            ci = qy * 4 + qx * 2
                            cols[ci], cols[ci + 1] = a1, a0
                    ws = [w16(c) for c in cols]
                    ws[0] |= 0x8000
                    out.extend(flags.to_bytes(2, 'little') + b''.join(x.to_bytes(2, 'little') for x in ws))
                    mode[by, bx] = 3
        flush()
        packets.append(bytes(out))
        modes.append(mode)
        prev = f
    return packets, modes

# packets of 15-bit CRAM into an AVI, as Video for Windows wrote them
def write_cram_avi(path, packets, rate=15):
    import struct
    def chunk(fcc, data):
        return fcc + struct.pack('<I', len(data)) + data + (b'\0' if len(data) & 1 else b'')
    def lst(fcc, data):
        return b'LIST' + struct.pack('<I', len(data) + 4) + fcc + data
    n = len(packets)
    big = max(len(p) for p in packets)
    avih = struct.pack('<14I', 1000000 // rate, 0, 0, 0x10, n, 0, 1, big, WIDTH, HEIGHT, 0, 0, 0, 0)
    strh = b'vidsCRAM' + struct.pack('<IHHIIIIIIiI4h', 0, 0, 0, 0, 1, rate, 0, n, big, -1, 0, 0, 0, WIDTH, HEIGHT)
    strf = struct.pack('<IiiHH4sIiiII', 40, WIDTH, HEIGHT, 1, 16, b'CRAM', WIDTH * HEIGHT * 2, 0, 0, 0, 0)
    hdrl = lst(b'hdrl', chunk(b'avih', avih) + lst(b'strl', chunk(b'strh', strh) + chunk(b'strf', strf)))
    movi, idx, off = b'', b'', 4
    for p in packets:
        idx += b'00dc' + struct.pack('<III', 0x10, off, len(p))
        c = chunk(b'00dc', p)
        movi += c
        off += len(c)
    body = b'AVI ' + hdrl + lst(b'movi', movi) + chunk(b'idx1', idx)
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', len(body)) + body)

def read_video(path, w=WIDTH, h=HEIGHT):
    r = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True, check=True)
    fs = w * h * 3
    v = np.frombuffer(r.stdout, np.uint8)
    return v[:(len(v) // fs) * fs].reshape(-1, h, w, 3).astype(np.float64)

def run_tool(args, fails=False):
    r = subprocess.run([TOOL, '-y', '-q'] + args, capture_output=True, text=True)
    if r.returncode != 0 and not fails:
        print(r.stderr)
    return r

def probe(path, entries, stream='v:0'):
    r = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', stream, '-show_entries', entries, '-of', 'csv=p=0', path],
                       capture_output=True, text=True)
    return r.stdout.strip()

def psnr(a, b, mask=None):
    d = (a - b) ** 2
    if mask is not None:
        d = d[:, mask]
    return 10 * np.log10(255.0 ** 2 / max(1e-9, d.mean()))

def block_means(v):
    n, h, w, c = v.shape
    return v[:, :h // 4 * 4, :w // 4 * 4].reshape(n, h // 4, 4, w // 4, 4, c).mean(axis=(2, 4))

def test_restore(tmp):
    print('Restoring a picture coded as CRAM (15-bit RGB):')
    rng = np.random.default_rng(1)
    clean = [clean_picture(rng, k) for k in range(FRAMES)]
    src = os.path.join(tmp, 'cram.avi')
    out = os.path.join(tmp, 'fixed.mkv')
    write_video(src, clean)
    r = run_tool([src, out])
    check(r.returncode == 0, 'runs')
    if r.returncode != 0:
        return

    truth = np.array(clean)
    cram = read_video(src)
    fixed = read_video(out)
    check(len(fixed) == len(cram) == FRAMES, 'all %d frames are there (%d)' % (FRAMES, len(fixed)))
    n = min(len(fixed), len(cram), len(truth))
    truth, cram, fixed = truth[:n], cram[:n], fixed[:n]
    truth = np.round(truth)

    edges, ramp, noise = regions()
    pc, pf = psnr(truth, cram), psnr(truth, fixed)
    check(pf > pc + 1.0, 'nearer the clean picture than the CRAM: %.2f dB, CRAM %.2f dB' % (pf, pc))
    ec, ef = psnr(truth, cram, edges), psnr(truth, fixed, edges)
    check(ef > ec + 0.7, 'the edges nearer: %.2f dB, CRAM %.2f dB' % (ef, ec))
    rc, rf = psnr(truth, cram, ramp), psnr(truth, fixed, ramp)
    check(rf > rc + 2.0, 'the ramp nearer: %.2f dB, CRAM %.2f dB' % (rf, rc))

    # the steps of the ramp at the edges of the blocks: the change across them as little as within them
    def block_steps(v):
        a = v[:, 68:116, 4:76]
        dx = np.abs(np.diff(a, axis=2)).mean(axis=(0, 1, 3))
        cols = np.arange(4, 75)
        across = dx[((cols + 1) % 4) == 0].mean()
        within = dx[((cols + 1) % 4) != 0].mean()
        return across, within
    ca, cw = block_steps(cram)
    fa, fw = block_steps(fixed)
    check(fa < fw * 1.6 and fa < ca * 0.6, 'no steps at the edges of the blocks in the ramp: %.2f across, %.2f within (CRAM %.2f, %.2f)' % (fa, fw, ca, cw))

    # each block keeps the color it was coded as, as its mean
    bd = np.abs(block_means(fixed) - block_means(cram))
    check(bd.mean() < 2.0 and np.percentile(bd, 99) < 8.0, 'each block keeps its color as its mean: off by %.2f levels, %.2f at most for 99%% of them' %
          (bd.mean(), np.percentile(bd, 99)))

    # the frames, their timing and the audio as they were
    check(probe(out, 'stream=r_frame_rate') == probe(src, 'stream=r_frame_rate'), 'the same frame rate (%s)' % probe(out, 'stream=r_frame_rate'))
    check(probe(out, 'stream=codec_name', 'a:0') == 'pcm_s16le', 'the audio is copied as it is')
    da = float(probe(src, 'format=duration') or 0)
    db = float(probe(out, 'format=duration') or 0)
    check(abs(da - db) < 0.15, 'the same length: %.3f s, input %.3f s' % (db, da))

    # -nosolid leaves the blocks of one color as they are
    out2 = os.path.join(tmp, 'nosolid.mkv')
    r = run_tool(['-nosolid', src, out2])
    if r.returncode == 0:
        ns = read_video(out2)[:n]
        # a block of one color is flat in the CRAM; where the CRAM is flat in a block and it was left as it is,
        # it is still flat
        flat = (cram.reshape(n, HEIGHT // 4, 4, WIDTH // 4, 4, 3).std(axis=(2, 4)).max(axis=-1) == 0)
        sflat = (ns.reshape(n, HEIGHT // 4, 4, WIDTH // 4, 4, 3).std(axis=(2, 4)).max(axis=-1) == 0)
        check(flat.sum() > 0 and (sflat[flat]).mean() > 0.99, '-nosolid leaves the blocks of one color flat (%.1f%% of %d)' % (100 * sflat[flat].mean(), flat.sum()))
    else:
        check(False, '-nosolid runs')

def test_ms_style(tmp):
    print('Restoring CRAM in the way of Microsoft\'s encoder (two and eight color blocks, skipped blocks):')
    rng = np.random.default_rng(3)
    frames = 12
    clean = [clean_picture(rng, k) for k in range(frames)]
    # the disc and square still for the last frames, so that their blocks are skipped
    for k in range(8, frames):
        clean[k][:64] = clean[7][:64]
    packets, modes = ms_encode(clean)
    src = os.path.join(tmp, 'ms.avi')
    write_cram_avi(src, packets)
    cram = read_video(src)
    check(len(cram) == frames, 'FFmpeg decodes the %d frames (%d)' % (frames, len(cram)))
    if len(cram) != frames:
        return
    modes = np.array(modes)
    check((modes == 0).mean() > 0.2 and (modes == 2).mean() > 0.05 and (modes == 3).mean() > 0.01,
          'skipped %.0f%% of the blocks, one color %.0f%%, two %.0f%%, eight %.0f%%' % tuple(100 * (modes == k).mean() for k in range(4)))
    truth = np.round(np.array(clean))
    q = np.round(np.round(truth * 31 / 255) * 255 / 31)
    check(psnr(q, cram) > 26, 'and near the picture: %.2f dB' % psnr(q, cram))

    # -show: each block as it was coded, the skipped ones as in the frame they were coded in, dimmed
    out = os.path.join(tmp, 'ms_show.mkv')
    r = run_tool(['-show', src, out])
    if r.returncode == 0:
        mb = block_means(read_video(out))
        seen = np.where(mb[..., 1] > mb[..., 0] + 20, 2, np.where(mb[..., 2] > mb[..., 0] + 20, 3, 1))
        coded = modes.copy()
        for k in range(1, frames):
            coded[k][modes[k] == 0] = coded[k - 1][modes[k] == 0]
        check((seen == coded).mean() > 0.995, 'every block read as it was coded: %.2f%%' % (100 * (seen == coded).mean()))
        dim = mb.max(axis=-1)
        check(np.median(dim[modes == 0]) < np.median(dim[modes != 0]) * 0.75, 'the skipped blocks shown dimmed')
    else:
        check(False, '-show runs')

    out = os.path.join(tmp, 'ms_fixed.mkv')
    r = run_tool([src, out])
    check(r.returncode == 0, 'runs')
    if r.returncode != 0:
        return
    fixed = read_video(out)
    check(len(fixed) == frames, 'all %d frames are there (%d)' % (frames, len(fixed)))
    if len(fixed) != frames:
        return

    edges, ramp, noise = regions()
    pc, pf = psnr(truth, cram), psnr(truth, fixed)
    check(pf > pc + 0.3, 'nearer the clean picture than the CRAM: %.2f dB, CRAM %.2f dB' % (pf, pc))
    ec, ef = psnr(truth, cram, edges), psnr(truth, fixed, edges)
    check(ef > ec + 0.3, 'the edges nearer: %.2f dB, CRAM %.2f dB' % (ef, ec))
    rc, rf = psnr(truth, cram, ramp), psnr(truth, fixed, ramp)
    check(rf > rc + 1.0, 'the ramp nearer: %.2f dB, CRAM %.2f dB' % (rf, rc))

    # the steps of the edges less: a pixel of the edge of the square half way between its color and the
    # background's, as the clean picture has, where the CRAM has one or the other
    def between(v):
        sq = np.array([220, 60, 50.0])
        bg = np.array([40, 50, 90.0])
        a = ((v - bg) @ (sq - bg)) / ((sq - bg) @ (sq - bg))
        part = (a > 0.15) & (a < 0.85)
        return part[:, 10:56, 56:104].mean()
    bt, bc, bf = between(truth), between(cram), between(fixed)
    check(bf > bc * 1.3, 'the edges of the square softened, not in steps: %.1f%% of its pixels between the colors, CRAM %.1f%%, clean %.1f%%' %
          (100 * bf, 100 * bc, 100 * bt))

    # the speckles of the noise less: how much each pixel differs from the one beside it
    def roughness(v, mask):
        return np.abs(np.diff(v, axis=2))[:, mask[:, 1:]].mean()
    nc, nf, nt = roughness(cram, noise), roughness(fixed, noise), roughness(truth, noise)
    check(nf < nc * 0.8, 'the noise less speckled: %.2f levels from pixel to pixel, CRAM %.2f, clean %.2f' % (nf, nc, nt))
    # and -noise more, taking more of the two color blocks of the noise as one color, smoother still
    out2 = os.path.join(tmp, 'ms_noise.mkv')
    r = run_tool(['-noise', '60', src, out2])
    if r.returncode == 0:
        n2 = roughness(read_video(out2), noise)
        check(n2 < nf * 0.8, '-noise 60 smoother still: %.2f levels from pixel to pixel' % n2)
    else:
        check(False, '-noise 60 runs')

    bd = np.abs(block_means(fixed) - block_means(cram))
    check(bd.mean() < 2.5, 'each block keeps its color as its mean: off by %.2f levels' % bd.mean())

def test_options(tmp):
    print('Options:')
    rng = np.random.default_rng(2)
    clean = [clean_picture(rng, k) for k in range(FRAMES)]
    src = os.path.join(tmp, 'cram2.avi')
    write_video(src, clean)

    out = os.path.join(tmp, 'part.mkv')
    r = run_tool(['-ss', '0.4', '-t', '0.6', src, out])
    check(r.returncode == 0 and len(read_video(out)) == 9, '-ss 0.4 -t 0.6 does 9 frames (%d)' % (len(read_video(out)) if r.returncode == 0 else -1))

    out = os.path.join(tmp, 'show.mkv')
    r = run_tool(['-show', src, out])
    ok = r.returncode == 0
    if ok:
        v = read_video(out)
        # the blocks of one color dark gray, the others green or blue
        mb = block_means(v)
        gray = (np.abs(mb[..., 0] - mb[..., 1]) < 2) & (np.abs(mb[..., 1] - mb[..., 2]) < 2)
        colored = (mb[..., 1] > mb[..., 0] + 20) | (mb[..., 2] > mb[..., 0] + 20)
        ok = (gray | colored).mean() > 0.99 and gray.mean() > 0.1 and colored.mean() > 0.05
    check(ok, '-show colors the blocks by how they were coded')

    out = os.path.join(tmp, 'x.mp4')
    r = run_tool(['-pix_fmt', 'yuv420p', src, out])
    check(r.returncode == 0 and probe(out, 'stream=codec_name,pix_fmt') == 'h264,yuv420p', '.mp4 -pix_fmt yuv420p is H.264 in 4:2:0 (%s)' % probe(out, 'stream=codec_name,pix_fmt'))

    # not CRAM
    other = os.path.join(tmp, 'other.avi')
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'lavfi', '-i', 'testsrc2=s=160x120:d=0.5', '-c:v', 'mjpeg', other], check=True)
    r = run_tool([other, os.path.join(tmp, 'other.mkv')], fails=True)
    check(r.returncode != 0 and 'not Microsoft Video 1' in r.stderr, 'video that is not CRAM is refused')

def test_samples(tmp):
    lotm = os.path.expanduser('~/lotm')
    samples = [('POET [MS-CRAM].avi', 'pal8'), ('PNNCL16 [MS-CRAM].avi', 'rgb555le')]
    for name, fmt in samples:
        path = os.path.join(lotm, name)
        if not os.path.exists(path):
            print('(no %s, not tried)' % path)
            continue
        print('The sample %s (%s):' % (name, fmt))
        w, h = (int(x) for x in probe(path, 'stream=width,height').split(','))
        out = os.path.join(tmp, 'sample.mkv')
        r = run_tool(['-t', '3', path, out])
        check(r.returncode == 0, 'runs')
        if r.returncode != 0:
            continue
        a = read_video(path, w, h)
        b = read_video(out, w, h)
        n = min(len(a), len(b))
        check(len(b) > 0 and abs(len(b) - len(a[:len(b)])) == 0, '%d frames' % len(b))
        # in color, as the palette says, and near what was decoded, each block's mean the same
        sat = np.abs(b[:n] - b[:n].mean(axis=-1, keepdims=True)).mean()
        sat0 = np.abs(a[:n] - a[:n].mean(axis=-1, keepdims=True)).mean()
        check(sat > sat0 * 0.6, 'in color: %.1f levels of color, decoded %.1f' % (sat, sat0))
        bd = np.abs(block_means(b[:n]) - block_means(a[:n])).mean()
        check(bd < 3.0, 'each block keeps its color as its mean: off by %.2f levels' % bd)

def main():
    if not os.path.exists(TOOL):
        print('%s is not there: run make first' % TOOL)
        sys.exit(1)
    with tempfile.TemporaryDirectory() as tmp:
        test_restore(tmp)
        test_ms_style(tmp)
        test_options(tmp)
        test_samples(tmp)
    print()
    if failures:
        print('%d FAILED' % len(failures))
        sys.exit(1)
    print('All passed')

if __name__ == '__main__':
    main()
