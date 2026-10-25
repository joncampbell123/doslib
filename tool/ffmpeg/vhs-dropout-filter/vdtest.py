#!/usr/bin/python3
#
# Tests for vhs-dropout-filter.
#
# Each test makes a short video of a scene the camera pans across slowly, puts dropouts in it
# that it knows, like those of VHS: runs of a line repeated over the lines below it, as the
# dropout compensator of a VCR does, and lines of static, each in one field (or frame) only.
# Then it checks that vhs-dropout-filter finds them (from the holes of -mode 2), and finds little
# else, that -mode 1 and -mode 3 fill them in with something near what was there, that nothing else
# is changed, and that the audio is copied as it is.
# It needs numpy, and ffmpeg and ffprobe to make and look at the test videos.
#
# Usage (after "make" has built linux-host/vhs-dropout-filter):
#
#   python3 vdtest.py [path to vhs-dropout-filter]
#
# Exit status is 0 if all tests pass, 1 otherwise.
import csv
import os
import re
import signal
import subprocess
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'linux-host', 'vhs-dropout-filter')

WIDTH = 720
HEIGHT = 480
FRAMES = 60
PAD = 40            # scene around the picture, for the camera to pan across

#------------------------------------------------------------------------
# Making a test video
#------------------------------------------------------------------------

def blur(a, sigma, axis):
    k = np.exp(-np.arange(-4, 5) ** 2 / (2 * sigma * sigma))
    k /= k.sum()
    p = np.pad(a, [(4, 4) if i == axis else (0, 0) for i in range(a.ndim)], mode='edge')
    return sum(k[i] * np.take(p, np.arange(i, i + a.shape[axis]), axis) for i in range(9))

def make_scene(rng):
    # a still picture with things in it: boxes of texture, edges, a gradient, in Y, U and V
    h, w = HEIGHT + 2 * PAD, WIDTH + 2 * PAD
    y = np.tile(np.linspace(50, 190, h)[:, None], (1, w))
    u = np.full((h, w), 128.0)
    v = np.full((h, w), 128.0)
    tex = blur(blur(rng.normal(0, 30, (h, w)), 1.0, 0), 1.0, 1)
    for _ in range(120):
        bh = rng.integers(8, 120)
        bw = rng.integers(10, 300)
        top = rng.integers(0, h - bh)
        left = rng.integers(0, w - bw)
        y[top:top + bh, left:left + bw] = rng.integers(30, 220) + tex[top:top + bh, left:left + bw] * rng.uniform(0.2, 1.0)
        u[top:top + bh, left:left + bw] = rng.integers(80, 176)
        v[top:top + bh, left:left + bw] = rng.integers(80, 176)
    # as soft as a camera makes it
    return [blur(blur(a, 0.7, 0), 0.9, 1) for a in (y, u, v)]

def take(a, oy, ox, rows):
    # rows of a, moved by (oy, ox) to a fraction of a pixel, from PAD in
    y = rows[:, None] + PAD + oy
    x = np.arange(WIDTH)[None, :] + PAD + ox
    y0, x0 = np.floor(y).astype(int), np.floor(x).astype(int)
    fy, fx = y - y0, x - x0
    g = lambda yy, xx: a[np.clip(yy, 0, a.shape[0] - 1), np.clip(xx, 0, a.shape[1] - 1)]
    return (g(y0, x0) * (1 - fx) * (1 - fy) + g(y0, x0 + 1) * fx * (1 - fy) +
            g(y0 + 1, x0) * (1 - fx) * fy + g(y0 + 1, x0 + 1) * fx * fy)

def render(rng, scene, interlaced, dropouts=True):
    # frames of YUV 4:2:2, 60 a second progressive or 30 interlaced (each field 1/60 second later),
    # the camera panning across, and the dropouts put in each picture, field or frame. The truth
    # is each frame as it is without dropouts, and where the dropouts are in each frame.
    sy, su, sv = scene
    pics = FRAMES * 2 if interlaced else FRAMES
    nf = 2 if interlaced else 1
    clean, dirty, masks, drops = [], [], [], []
    for k in range(FRAMES):
        fy = np.empty((HEIGHT, WIDTH))
        fu = np.empty((HEIGHT, WIDTH))
        fv = np.empty((HEIGHT, WIDTH))
        for p in range(nf):
            n = k * nf + p
            oy, ox = 3.0 * np.sin(n / pics * 3.0), 0.6 * n
            rows = np.arange(p, HEIGHT, nf).astype(float)
            fy[p::nf], fu[p::nf], fv[p::nf] = (take(a, oy, ox, rows) for a in (sy, su, sv))
            # and things moving about on their own, as people do, which make each frame unlike the next
            for j, (level, speed) in enumerate(((225, 5.0), (30, -4.0), (170, 3.0))):
                cx = int(100 + j * 220 + speed * n) % (WIDTH - 80)
                cy = int(90 + j * 120 + 25 * np.sin(n / 7.0 + j))
                r = np.arange(p, HEIGHT, nf)
                sel = r[(r >= cy) & (r < cy + 50)]
                fy[sel, cx:cx + 60] = level
                fu[sel, cx:cx + 60] = 100 + 30 * j
        fy += rng.normal(0, 1.5, fy.shape)
        clean.append([fy.copy(), fu.copy(), fv.copy()])

        mask = np.zeros((HEIGHT, WIDTH), bool)
        if dropouts:
            for p in range(nf):
                for _ in range(rng.integers(2, 6)):
                    # in lines of the picture: a field, or the frame
                    lines = HEIGHT // nf
                    h = int(rng.integers(1, 4 if interlaced else 7))
                    y0 = int(rng.integers(30 // nf, lines - 30 // nf - h))
                    x0 = int(rng.integers(20, WIDTH - 140))
                    x1 = min(WIDTH - 20, x0 + int(rng.integers(60, 400)))
                    rows = [p + nf * (y0 + i) for i in range(h)]
                    kind = 'repeat' if rng.random() < 0.7 else 'static'
                    drops.append((k, rows, x0, x1, kind))
                    if kind == 'repeat':
                        # the line above repeated
                        src = p + nf * (y0 - 1)
                        for r in rows:
                            for a in (fy, fu, fv):
                                a[r, x0:x1] = a[src, x0:x1]
                    else:
                        # static: bright, noisy streaks
                        for r in rows:
                            fy[r, x0:x1] = np.clip(rng.normal(190, 40, x1 - x0), 16, 235)
                            fu[r, x0:x1] = 128 + rng.normal(0, 10, x1 - x0)
                            fv[r, x0:x1] = 128 + rng.normal(0, 10, x1 - x0)
                    mask[rows, x0:x1] = True
        dirty.append([fy, fu, fv])
        masks.append(mask)
    return clean, dirty, np.array(masks), drops

def pack(frames, bits):
    # 4:2:2 planar, 8 or 10 bits
    out = []
    scale = 4.0 if bits == 10 else 1.0
    dt = np.uint16 if bits == 10 else np.uint8
    for y, u, v in frames:
        u2, v2 = (a[:, 0::2] * 0.5 + a[:, 1::2] * 0.5 for a in (u, v))
        out.append(b''.join(np.clip(np.round(a * scale), 0, 255 * scale).astype(dt).tobytes() for a in (y, u2, v2)))
    return b''.join(out)

def make_video(path, frames, interlaced, bits=8, vcodec='ffv1'):
    # with two audio streams, so that copying them is tested
    pix = 'yuv422p10le' if bits == 10 else 'yuv422p'
    rate = '30000/1001' if interlaced else '60000/1001'
    cmd = ['ffmpeg', '-v', 'error', '-y',
           '-f', 'rawvideo', '-pix_fmt', pix, '-s', '%dx%d' % (WIDTH, HEIGHT), '-r', rate, '-i', '-',
           '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=48000',
           '-f', 'lavfi', '-i', 'sine=frequency=1000:sample_rate=48000',
           '-map', '0:v', '-map', '1:a', '-map', '2:a', '-shortest',
           '-c:v', vcodec, '-field_order', 'tt' if interlaced else 'progressive',
           '-c:a:0', 'pcm_s16le', '-c:a:1', 'pcm_s24le', path]
    subprocess.run(cmd, input=pack(frames, bits), check=True)

#------------------------------------------------------------------------
# Looking at the results
#------------------------------------------------------------------------

def run_tool(args, ok=True):
    r = subprocess.run([TOOL, '-q'] + args, capture_output=True, text=True)
    if ok and r.returncode != 0:
        raise RuntimeError('vhs-dropout-filter %s failed: %s' % (' '.join(args), r.stderr))
    return r

def plane(path, bits=8):
    # the luma of each frame, as it is, from 4:2:2 (asking for gray would make it full range)
    fmt = 'yuv422p10le' if bits > 8 else 'yuv422p'
    r = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-f', 'rawvideo', '-pix_fmt', fmt, '-'],
                       capture_output=True, check=True)
    dt = np.uint16 if bits > 8 else np.uint8
    a = np.frombuffer(r.stdout, dt).reshape(-1, HEIGHT * WIDTH * 2)[:, :HEIGHT * WIDTH]
    return a.reshape(-1, HEIGHT, WIDTH).astype(float)

def alpha(path):
    r = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-vf', 'alphaextract,format=gray', '-f', 'rawvideo', '-'],
                       capture_output=True, check=True)
    return np.frombuffer(r.stdout, np.uint8).reshape(-1, HEIGHT, WIDTH).astype(float)

def grown(m, gx, gy):
    out = m.copy()
    for dy in range(-gy, gy + 1):
        for dx in range(-gx, gx + 1):
            out |= np.roll(np.roll(m, dy, 1), dx, 2)
    return out

def audio_hashes(path):
    r = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-map', '0:a', '-c', 'copy', '-f', 'framemd5', '-'],
                       capture_output=True, text=True, check=True)
    return [l.split(',')[-1].strip() for l in r.stdout.splitlines() if l and not l.startswith('#')]

def probe(path, what):
    r = subprocess.run(['ffprobe', '-v', 'error', '-show_entries', what, '-of', 'csv=p=0', path],
                       capture_output=True, text=True, check=True)
    return r.stdout.split()

#------------------------------------------------------------------------
# Tests
#------------------------------------------------------------------------

failures = 0

def check(name, cond, detail=''):
    global failures
    print('%s: %s%s' % ('ok  ' if cond else 'FAIL', name, (' (' + detail + ')') if detail else ''))
    if not cond:
        failures += 1

def test(name, interlaced, bits=8, tool_args=(), seed=1, min_found=0.85, max_extra=0.002, max_hold=0.3, max_motion=0.15):
    rng = np.random.default_rng(seed)
    scene = make_scene(rng)
    clean, dirty, truth, drops = render(rng, scene, interlaced)
    clean_y = np.array([f[0] for f in clean])
    dirty_y = np.array([f[0] for f in dirty])
    args = list(tool_args) + (['-I'] if interlaced else [])
    scale = 4.0 if bits == 10 else 1.0

    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, 'in.mkv')
        make_video(src, dirty, interlaced, bits)
        got_in = plane(src, bits) / scale

        # -mode 2: the holes are where the dropouts are, and only there (allowing for the pixels
        # and lines they are grown by)
        holes = os.path.join(tmp, 'holes.mov')
        log = os.path.join(tmp, 'holes.csv')
        run_tool(args + ['-mode', '2', '-log', log, src, holes])
        a = alpha(holes)
        found = a < 128
        rows = list(csv.DictReader(open(log)))
        check(name + ': -mode 2 every frame', len(rows) == FRAMES and len(a) == FRAMES, '%d frames, %d in the log' % (len(a), len(rows)))
        check(name + ': -mode 2 ProRes 4444 with alpha', probe(holes, 'stream=codec_name,profile,pix_fmt')[0].startswith('prores,4444,yuva444p'),
              ' '.join(probe(holes, 'stream=codec_name,profile,pix_fmt')))
        check(name + ': holes match the log', all(int(r['dropout_pixels']) == int(f.sum()) for r, f in zip(rows, found)))
        # A line repeated where the picture is the same from one line to the next is much the same as
        # what was there, and need not be found. The rest should be.
        k = np.ones(9) / 9
        off = np.apply_along_axis(lambda r: np.convolve(r, k, 'same'), 2, np.abs(dirty_y - clean_y))
        visible = truth & (off >= 6)
        recall = (found & visible).sum() / visible.sum()
        check(name + ': dropouts found', recall >= min_found,
              '%.1f%% of the pixels of those that can be seen, %.1f%% of all' % (recall * 100, (found & truth).sum() * 100 / truth.sum()))
        extra = (found & ~grown(truth, 8, 3)).sum() / (~truth).sum()
        check(name + ': little else found', extra <= max_extra, '%.3f%% of the rest of the picture' % (extra * 100))

        # the pixels of the dropouts, how far from what was there
        def err(y):
            return np.abs(y - clean_y)[truth].mean()
        before = err(dirty_y)

        # -mode 1 compares with fewer pictures, unless told, so finds its own dropouts
        if '-near' in args:
            found1 = found
        else:
            run_tool(args + ['-mode', '2', '-near', '1', '-y', src, holes])
            found1 = alpha(holes) < 128

        for mode, most, where in (('1', max_hold, found1), ('3', max_motion, found)):
            dst = os.path.join(tmp, 'out%s.mkv' % mode)
            run_tool(args + ['-mode', mode, src, dst])
            out = plane(dst, bits) / scale
            after = err(out)
            check(name + ': -mode %s fills in the dropouts' % mode, after <= most * before,
                  'the dropouts %.2f levels from what was there, from %.2f' % (after, before))
            same = np.array_equal(out[~where], got_in[~where])
            check(name + ': -mode %s changes nothing else' % mode, same,
                  '%.0f pixels changed' % (np.abs(out - got_in)[~where] > 0).sum())
            check(name + ': -mode %s same video format' % mode,
                  probe(src, 'stream=codec_name,pix_fmt,field_order')[0] == probe(dst, 'stream=codec_name,pix_fmt,field_order')[0],
                  '%s -> %s' % (probe(src, 'stream=codec_name,pix_fmt,field_order')[0], probe(dst, 'stream=codec_name,pix_fmt,field_order')[0]))
            ha, hb = audio_hashes(src), audio_hashes(dst)
            check(name + ': -mode %s audio copied as it is' % mode, len(ha) > 0 and ha == hb, '%d packets' % len(ha))

def test_clean(name, seed=20, max_extra=0.002):
    # a video with no dropouts: little is found, and -mode 3 leaves it all but the same
    rng = np.random.default_rng(seed)
    clean, dirty, truth, drops = render(rng, make_scene(rng), False, dropouts=False)
    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, 'in.mkv')
        holes = os.path.join(tmp, 'holes.mov')
        make_video(src, dirty, False)
        run_tool(['-mode', '2', src, holes])
        extra = (alpha(holes) < 128).mean()
        check(name + ': little found', extra <= max_extra, '%.3f%% of the picture' % (extra * 100))

def test_range(name, ss, t, seed=30):
    # -ss and -t, as FFmpeg's: the frames of that part, and the audio of it
    rng = np.random.default_rng(seed)
    clean, dirty, truth, drops = render(rng, make_scene(rng), False)
    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, 'in.mkv')
        dst = os.path.join(tmp, 'out.mkv')
        make_video(src, dirty, False)
        run_tool(['-mode', '1', '-ss', ss, '-t', t, src, dst])
        y_in = plane(src)
        y_out = plane(dst)
        n = len(y_out)
        check(name + ': the frames of that part', abs(n - 30) <= 1, '%d frames' % n)
        # its first frame is the frame 0.25 seconds in, outside the dropouts
        first = int(round(0.25 * 60000 / 1001))
        d = [np.abs(y_out[0] - y_in[k])[~grown(truth[k:k + 1], 8, 3)[0]].mean() for k in (first - 1, first, first + 1)]
        check(name + ': starts at the frame asked for', int(np.argmin(d)) == 1, 'differences from frames around it %s' % np.round(d, 2))
        dur = [float(x) for x in probe(dst, 'stream=start_time')]
        check(name + ': video and audio start at 0', all(abs(x) < 0.05 for x in dur), str(dur))

def run_interrupted(args, src, dst, what, at=60):
    # run the tool, and once it has written about at frames, CTRL+C (SIGINT) it, as at a terminal
    p = subprocess.Popen([TOOL] + list(args) + [src, dst], stderr=subprocess.PIPE)
    seen = b''
    pattern = re.compile(what.encode() + rb': frame (\d+)')
    sent = False
    while True:
        b = p.stderr.read1(4096) if hasattr(p.stderr, 'read1') else p.stderr.read(1)
        if not b:
            break
        seen += b
        if not sent:
            m = [int(x) for x in pattern.findall(seen)]
            if m and max(m) >= at:
                p.send_signal(signal.SIGINT)
                sent = True
    p.wait()
    return p.returncode, sent, seen.decode(errors='replace')

def test_interrupt(name, args, what, ext='.mov', vcodec=None, audio=True):
    # CTRL+C while it writes: it stops, finishes the file with what it has, and exits as it would at the end
    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, 'in.mov')
        dst = os.path.join(tmp, 'out' + ext)
        cmd = ['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i', 'testsrc2=size=720x480:rate=60000/1001,noise=alls=8:allf=t']
        if audio:
            cmd += ['-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=48000', '-c:a', 'pcm_s16le']
        cmd += ['-t', '30', '-c:v', 'ffv1', '-pix_fmt', 'yuv422p', src]
        subprocess.run(cmd, check=True)
        extra = ['-c', vcodec] if vcodec else []
        rc, sent, err = run_interrupted(list(args) + extra, src, dst, what)
        check(name + ': CTRL+C sent while writing', sent)
        check(name + ': exits normally', rc == 0, 'exit status %d: %s' % (rc, err.strip().splitlines()[-1] if err.strip() else ''))
        check(name + ': says it stopped', 'Stopped by CTRL+C' in err)
        r = subprocess.run(['ffprobe', '-v', 'error', '-count_frames', '-select_streams', 'v', '-show_entries', 'stream=nb_read_frames',
                            '-of', 'csv=p=0', dst], capture_output=True, text=True)
        n = int(r.stdout.strip() or 0) if r.returncode == 0 else 0
        check(name + ': output is complete and plays', r.returncode == 0 and 0 < n < 1798, '%d of 1798 frames, %s' % (n, r.stderr.strip()))
        d = subprocess.run(['ffmpeg', '-v', 'error', '-i', dst, '-f', 'null', '-'], capture_output=True, text=True)
        check(name + ': decodes without errors', d.returncode == 0 and d.stderr.strip() == '', d.stderr.strip()[:200])

def add_creases(rng, frames, count=2, length=25):
    # dropouts like those a crease in the tape makes: a band of lines, the line above repeated, in the same
    # lines for a run of frames, the ends of it moving a little; which the frames around cannot show
    mask = np.zeros((len(frames), HEIGHT, WIDTH), bool)
    for _ in range(count):
        h = int(rng.integers(4, 9))
        y0 = int(rng.integers(60, HEIGHT - 60 - h))
        x0 = int(rng.integers(40, WIDTH - 340))
        x1 = x0 + int(rng.integers(200, 300))
        k0 = int(rng.integers(0, len(frames) - length))
        for k in range(k0, k0 + length):
            a, b = x0 + int(rng.integers(-20, 21)), x1 + int(rng.integers(-20, 21))
            for plane in frames[k]:
                plane[y0:y0 + h, a:b] = plane[y0 - 1, a:b]
            mask[k, y0:y0 + h, a:b] = True
    return mask

def as_copy(frames):
    # a second generation copy: a little dimmer and softer, the picture 2 pixels to the right
    right = lambda a: np.concatenate([np.repeat(a[:, :1], 2, 1), a[:, :-2]], 1)
    out = []
    for y, u, v in frames:
        y2 = right(y * 0.93 + 6)
        y2 = (y2 + np.concatenate([y2[:, :1], y2[:, :-1]], 1) + np.concatenate([y2[:, 1:], y2[:, -1:]], 1)) / 3
        out.append([y2, right(u), right(v)])
    return out

def visible(dirty_y, clean_y, mask):
    k = np.ones(9) / 9
    off = np.apply_along_axis(lambda r: np.convolve(r, k, 'same'), 2, np.abs(dirty_y - clean_y))
    return mask & (off >= 6)

def test_donor(name, seed=40):
    # a donor edited from the same tape, with dropouts of its own: it finds and fills in the creases,
    # which the frames around cannot, for the frames it has, and is not used for those it has not
    rng = np.random.default_rng(seed)
    scene = make_scene(rng)
    clean, dirty, truth, drops = render(rng, scene, False)
    crease = add_creases(rng, dirty)
    dclean, ddirty, dtruth, ddrops = render(rng, scene, False)
    other = render(rng, make_scene(rng), False, dropouts=False)[1][:20]
    copy = other + as_copy(ddirty[0:25] + ddirty[35:60])
    clean_y = np.array([f[0] for f in clean])
    dirty_y = np.array([f[0] for f in dirty])
    vis = visible(dirty_y, clean_y, crease)

    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, 'in.mkv')
        don = os.path.join(tmp, 'donor.mkv')
        make_video(src, dirty, False)
        make_video(don, copy, False)
        cache = ['-cache', os.path.join(tmp, 'cache')]

        found = {}
        for tag, extra in (('without', []), ('with', ['-donor', don])):
            holes = os.path.join(tmp, 'holes-%s.mov' % tag)
            log = os.path.join(tmp, 'holes-%s.csv' % tag)
            run_tool(cache + extra + ['-mode', '2', '-log', log, src, holes])
            found[tag] = alpha(holes) < 128
        rows = list(csv.DictReader(open(log)))
        has = [r['donor1_time'] != '' for r in rows]
        check(name + ': the frames the donor has are found', sum(has[0:25]) + sum(has[35:60]) >= 48 and sum(has[25:35]) <= 1,
              '%d of 50 found, %d of the 10 it has not' % (sum(has[0:25]) + sum(has[35:60]), sum(has[25:35])))
        times = [float(r['donor1_time']) for r in rows[:25] if r['donor1_time']]
        check(name + ': and where they are in it', all(abs(t - (20 + k) * 1001 / 60000) < 0.002 for k, t in enumerate(times) if t),
              'first at %.3f s' % (times[0] if times else -1))
        inside = np.zeros(FRAMES, bool)
        inside[0:25] = inside[35:60] = True
        r0 = (found['without'] & vis)[inside].sum() / vis[inside].sum()
        r1 = (found['with'] & vis)[inside].sum() / vis[inside].sum()
        check(name + ': creases found with the donor', r1 >= 0.75 and r1 >= r0 + 0.3, '%.1f%%, from %.1f%% without it' % (r1 * 100, r0 * 100))
        extra = (found['with'] & ~grown(truth | crease, 8, 3)).sum() / (~(truth | crease)).sum()
        check(name + ': little else found with the donor', extra <= 0.004, '%.3f%% of the rest of the picture' % (extra * 100))

        err = {}
        for tag, extra in (('without', []), ('with', ['-donor', don])):
            dst = os.path.join(tmp, 'out-%s.mkv' % tag)
            run_tool(cache + extra + ['-mode', '3', src, dst])
            out = plane(dst)
            err[tag] = np.abs(out - clean_y)[crease & inside[:, None, None]].mean()
        before = np.abs(dirty_y - clean_y)[crease & inside[:, None, None]].mean()
        check(name + ': creases filled in from the donor', err['with'] <= 0.4 * err['without'],
              '%.2f levels from what was there, %.2f without it, %.2f before' % (err['with'], err['without'], before))
        check(name + ': thumbnails kept in the cache', len(os.listdir(os.path.join(tmp, 'cache'))) == 2,
              str(os.listdir(os.path.join(tmp, 'cache'))))

def test_self_donor(name, seed=50):
    # the tape played twice in the same file, with other dropouts and creases each time: the input is its own donor
    rng = np.random.default_rng(seed)
    scene = make_scene(rng)
    clean, play1, truth1, _ = render(rng, scene, False)
    crease1 = add_creases(rng, play1)
    _, play2, truth2, _ = render(rng, scene, False)
    add_creases(rng, play2)
    gap = render(rng, make_scene(rng), False, dropouts=False)[1][:20]
    clean_y = np.array([f[0] for f in clean])
    dirty_y = np.array([f[0] for f in play1])

    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, 'in.mkv')
        make_video(src, play1 + gap + play2, False)
        err = {}
        for tag, extra in (('without', []), ('with', ['-donor', src, '-donor-gap', '0.5'])):
            dst = os.path.join(tmp, 'out-%s.mkv' % tag)
            log = os.path.join(tmp, 'out-%s.csv' % tag)
            run_tool(['-nocache', '-t', '1'] + extra + ['-mode', '3', '-log', log, src, dst])
            out = plane(dst)
            err[tag] = np.abs(out - clean_y)[crease1].mean()
        rows = list(csv.DictReader(open(log)))
        times = [float(r['donor1_time']) for r in rows if r['donor1_time']]
        check(name + ': the second playback found', len(times) >= 58 and all(t > 1.0 for t in times), '%d frames' % len(times))
        check(name + ': creases filled in from it', err['with'] <= 0.4 * err['without'],
              '%.2f levels from what was there, %.2f without it' % (err['with'], err['without']))

def test_errors():
    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, 'in.mov')
        subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i', 'testsrc2=size=320x240:rate=60000/1001',
                        '-frames:v', '10', '-c:v', 'ffv1', src], check=True)
        dst = os.path.join(tmp, 'out.mov')
        r = run_tool([src, dst], ok=False)
        check('needs -mode', r.returncode != 0 and '-mode' in r.stderr, r.stderr.strip())
        r = run_tool(['-mode', '1', src, src], ok=False)
        check('refuses to write over the input', r.returncode != 0 and 'write over the input' in r.stderr, r.stderr.strip())
        open(dst, 'w').close()
        r = run_tool(['-mode', '1', src, dst], ok=False)
        check('refuses to write over an existing file without -y', r.returncode != 0 and os.path.getsize(dst) == 0, r.stderr.strip())
        r = run_tool(['-mode', '1', '-y', src, dst], ok=False)
        check('writes over it with -y, with no audio', r.returncode == 0 and os.path.getsize(dst) > 0, r.stderr.strip())
        r = run_tool(['-mode', '2', '-y', '-c', 'mpeg2video', src, dst], ok=False)
        check('-mode 2 refuses an encoder without alpha', r.returncode != 0 and 'alpha' in r.stderr, r.stderr.strip())

test('progressive', False)
test('interlaced', True, seed=2)
test('10-bit', False, bits=10, seed=3)
test('-near 1', False, tool_args=('-near', '1'), seed=4)
test_clean('no dropouts')
test_range('-ss and -t in seconds', '0.25', '0.5')
test_range('-ss and -t as MM:SS', '00:00.25', '0:0.5')
test_donor('-donor')
test_self_donor('-donor, the input itself')
test_interrupt('CTRL+C, -mode 3', ['-mode', '3'], 'Filtering')
test_interrupt('CTRL+C, -mode 2', ['-mode', '2'], 'Filtering')
test_interrupt('CTRL+C, -mode 1, MP4', ['-mode', '1'], 'Filtering', ext='.mp4', vcodec='libx264', audio=False)
test_errors()

print('%d failure%s' % (failures, '' if failures == 1 else 's'))
sys.exit(1 if failures else 0)
