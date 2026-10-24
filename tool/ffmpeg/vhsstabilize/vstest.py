#!/usr/bin/python3
#
# Tests for vhsstabilize.
#
# Each test makes a short interlaced video like a VHS capture, with up and down jitter
# that it knows, then checks that vhsstabilize measures the jitter, that the output has
# the jitter taken out, and that the audio in the output is the same as in the input.
# The picture on each line is also a little left or right of where it should be, and the
# tests check that the output has the edges of the picture straight up and down again.
# It needs numpy, and ffmpeg and ffprobe to make and look at the test videos.
#
# Usage (after "make" has built linux-host/vhsstabilize):
#
#   python3 vstest.py [path to vhsstabilize]
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
TOOL = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'linux-host', 'vhsstabilize')

WIDTH = 720
HEIGHT = 486
FRAMES = 240
PAD = 64            # scene lines above and below the picture, for the jitter and tilt
LEFT = 8            # pixels of blanking at the left and right of each line, outside the picture
RIGHT = 6
BLANK = 16          # how bright the blanking is

#------------------------------------------------------------------------
# Making a test video
#------------------------------------------------------------------------

def make_scene(rng, height, blanking=True):
    # a still picture with things in it: boxes, bars, a gradient, in Y, U and V,
    # and the black of the blanking at the left and right
    y = np.tile(np.linspace(40, 200, height)[:, None], (1, WIDTH))
    u = np.full((height, WIDTH), 128.0)
    v = np.full((height, WIDTH), 128.0)
    for _ in range(150):
        h = rng.integers(4, 80)
        w = rng.integers(10, 300)
        top = rng.integers(0, height - h)
        left = rng.integers(0, WIDTH - w)
        y[top:top + h, left:left + w] = rng.integers(16, 236)
        u[top:top + h, left:left + w] = rng.integers(64, 192)
        v[top:top + h, left:left + w] = rng.integers(64, 192)
    for _ in range(40):
        top = rng.integers(0, height - 2)
        y[top:top + rng.integers(1, 3), :] = rng.integers(16, 236)
    if blanking:
        for a, level in ((y, BLANK), (u, 128), (v, 128)):
            a[:, :LEFT] = level
            a[:, WIDTH - RIGHT:] = level
    # as soft as a camera makes it, up and down and across
    return [blur(blur(a, 0.8).T, 1.2).T for a in (y, u, v)]

def blur(a, sigma):
    # gaussian blur down the columns
    k = np.exp(-np.arange(-4, 5) ** 2 / (2 * sigma * sigma))
    k /= k.sum()
    p = np.pad(a, ((4, 4), (0, 0)), mode='edge')
    return sum(k[i] * p[i:i + a.shape[0]] for i in range(9))

def camera_offsets(rng, count, tilt, shake, same_fields):
    # how far up (in lines of the frame) the camera moves the picture, for each field in the order
    # they are shown, or for each frame if same_fields: a slow tilt up and down, and the shaking
    # of a camera held in the hand, a line or two a few times a second, to a fraction of a line
    t = np.arange(count) / 59.94
    cam = tilt * np.sin(2 * np.pi * t / 6.0)
    for f, a in ((1.3, 1.0), (2.9, 0.6), (5.1, 0.3)):
        cam = cam + shake * a * np.sin(2 * np.pi * f * t + rng.uniform(0, 2 * np.pi))
    if same_fields:
        cam[1::2] = cam[0::2]
    return cam

def jitter_offsets(rng, count, jitter, same_fields, big=0):
    # and how far up VHS jitter moves it on top of that, in whole lines. Like VHS, most fields
    # are where they should be, but some jump up or down, now and then several in a row, and so
    # does the first frame's second field, so that the fields of the first frame are not lined
    # up with each other. And big times, a field or three jump much further than the range.
    fast = np.where(rng.random(count) < 0.4, rng.integers(-jitter, jitter + 1, count), 0)
    for start in rng.integers(0, count - 6, max(1, count // 100)):
        fast[start:start + 4] += rng.choice([-1, 1]) * (jitter + 3)
    for start in rng.integers(10, count - 6, big):
        fast[start:start + rng.integers(1, 4)] = rng.choice([-1, 1]) * rng.integers(30, 41)
    fast[1] = 3
    if same_fields:
        fast[1::2] = fast[0::2]
    return fast.astype(float)

def line_warps(rng, count, same_fields):
    # how far right (in pixels) the picture is on each line of each field, like VHS played without
    # a time base corrector: each field a little off from the others, a slow wave down the field,
    # a bend at the top of it, and a little jitter from line to line. Or of each frame, if same_fields.
    out = np.empty((count, HEIGHT // 2))
    for f in range(0, count, 2 if same_fields else 1):
        y = np.arange(HEIGHT if same_fields else HEIGHT // 2) / (2.0 if same_fields else 1.0)
        h = (rng.uniform(-1.5, 1.5)
             + rng.uniform(0.5, 2.0) * np.sin(2 * np.pi * y / rng.uniform(60, 200) + rng.uniform(0, 2 * np.pi))
             + rng.uniform(-3, 3) * np.exp(-y / 8)
             + rng.normal(0, 0.2, y.shape))
        if same_fields:
            out[f], out[f + 1] = h[0::2], h[1::2]
        else:
            out[f] = h
    return out

def shift_rows(a, h):
    # each row of a moved right by h of that row, to a fraction of a pixel, the ends repeating
    x = np.arange(a.shape[1]) - h[:, None]
    i = np.floor(x).astype(int)
    f = x - i
    take = lambda k: np.take_along_axis(a, np.clip(k, 0, a.shape[1] - 1), 1)
    return take(i) * (1 - f) + take(i + 1) * f

def take_rows(a, rows):
    # the rows of a at rows, to a fraction of a row, all the same fraction
    r0 = np.floor(rows).astype(int)
    f = rows[0] - r0[0]
    return a[r0] * (1 - f) + a[r0 + 1] * f

def render(rng, scene, offsets, warps, bff=False):
    # interlaced frames of 8-bit YUV 4:2:2: field n of the video, in the order they are shown, is
    # frame n // 2, lines p, p + 2, ... where p is n % 2 top field first, or the other way round
    # bottom field first, taken offsets[n] lines up the scene, with the picture on each line moved
    # right by warps
    sy, su, sv = scene
    frames = []
    for k in range(len(offsets) // 2):
        fy = np.empty((HEIGHT, WIDTH))
        fu = np.empty((HEIGHT, WIDTH // 2))
        fv = np.empty((HEIGHT, WIDTH // 2))
        for n in (2 * k, 2 * k + 1):
            p = (n % 2) ^ int(bff)
            rows = np.arange(p, HEIGHT, 2) + PAD + offsets[n]
            h = warps[n]
            fy[p::2] = shift_rows(take_rows(sy, rows), h)
            fu[p::2] = shift_rows(take_rows(su[:, 0::2], rows), h / 2)
            fv[p::2] = shift_rows(take_rows(sv[:, 0::2], rows), h / 2)
        # VHS: noise, black at the top, head switching noise at the bottom
        fy += rng.normal(0, 2.5, fy.shape)
        fy[:6] = 16
        fy[-8:] = rng.integers(16, 236, (8, WIDTH))
        frames.append(b''.join(np.clip(np.round(a), 0, 255).astype(np.uint8).tobytes() for a in (fy, fu, fv)))
    return frames

def make_video(path, frames, vcodec, extra=(), field_order='tt'):
    # with two audio streams, so that copying them is tested
    cmd = ['ffmpeg', '-v', 'error', '-y',
           '-f', 'rawvideo', '-pix_fmt', 'yuv422p', '-s', '%dx%d' % (WIDTH, HEIGHT), '-r', '30000/1001', '-i', '-',
           '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=48000',
           '-f', 'lavfi', '-i', 'sine=frequency=1000:sample_rate=48000',
           '-map', '0:v', '-map', '1:a', '-map', '2:a', '-shortest',
           '-c:v', vcodec] + list(extra) + [
           '-field_order', field_order, '-c:a:0', 'pcm_s16le', '-c:a:1', 'pcm_s24le', path]
    subprocess.run(cmd, input=b''.join(frames), check=True)

#------------------------------------------------------------------------
# Looking at the results
#------------------------------------------------------------------------

def run_tool(args):
    r = subprocess.run([TOOL, '-q'] + args, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError('vhsstabilize %s failed: %s' % (' '.join(args), r.stderr))
    return r

def read_log(path):
    with open(path) as f:
        return list(csv.DictReader(f))

def column(rows, names, what):
    # a column for each field, in the order they are shown (top field first)
    return np.array([[float(r[n + '_' + what]) for n in names] for r in rows])

def jitter_of(pos):
    # how much the picture jumps from one field to the next, in lines: the second difference
    # of where it is, field after field, which is small for smooth movement of the camera
    p = pos.reshape(-1)
    return float(np.sqrt(np.mean(np.square(p[1:-1] - (p[:-2] + p[2:]) / 2))))

def fast(pos):
    # the movement faster than about a quarter of a second, field after field
    p = pos.reshape(-1)
    k = np.exp(-np.arange(-45, 46) ** 2 / (2 * 15.0 ** 2))
    k /= k.sum()
    return p - np.convolve(np.pad(p, 45, mode='edge'), k, 'valid')

def rms(a):
    return float(np.sqrt(np.mean(np.square(a))))

def within(err):
    # how close each parity is, once the difference all the way through is taken out
    err = err - np.median(err, axis=0)
    return float(np.percentile(np.abs(err), 95)), float(np.abs(err).max())

def audio_hashes(path):
    r = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-map', '0:a', '-c', 'copy', '-f', 'framemd5', '-'],
                       capture_output=True, text=True, check=True)
    # the hash of each packet, leaving out the timestamps, which can be in another time base
    return [l.split(',')[-1].strip() for l in r.stdout.splitlines() if l and not l.startswith('#')]

def edge_from_outside(a):
    # where each row of a, samples from the edge of the frame inward, rises from the blanking to the
    # picture, halfway up: the samples of blanking before it, with those on the rise counted as part
    # of one, less a half. NaN where the picture is too dark next to the blanking to tell.
    n = a.shape[-1]
    up = a > BLANK + 20
    first = np.argmax(up, -1)
    hi = np.take_along_axis(a, np.minimum(first[..., None] + np.arange(2, 5), n - 1), -1).mean(-1)
    start = np.maximum(first - 3, 0)
    x = np.arange(n)
    part = np.clip((hi[..., None] - a) / np.maximum(hi - BLANK, 1.0)[..., None], 0, 1)
    pos = start + np.where((x >= start[..., None]) & (x < (first + 2)[..., None]), part, 0).sum(-1) - 0.5
    return np.where(up.any(-1) & (hi - BLANK >= 40) & (first + 5 < n), pos, np.nan)

def picture_edges(path):
    # x of the left and right edges of the picture on each line of each frame (frames, lines)
    r = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-f', 'rawvideo', '-pix_fmt', 'yuv422p', '-'],
                       capture_output=True, check=True)
    y = np.frombuffer(r.stdout, np.uint8).reshape(-1, WIDTH * HEIGHT * 2)[:, :WIDTH * HEIGHT]
    y = y.reshape(-1, HEIGHT, WIDTH).astype(float)
    return edge_from_outside(y[..., :24]), (WIDTH - 1) - edge_from_outside(y[..., ::-1][..., :24])

def bent(edges):
    # how far the edges are from where they are on most lines of the video, 90% of the lines within this,
    # leaving out the lines at the top and bottom that the picture can move away from
    d = []
    for e in edges:
        e = e[:, 24:HEIGHT - 24]
        e = e - np.nanmedian(e)
        d.append(np.abs(e[~np.isnan(e)]))
    return float(np.percentile(np.concatenate(d), 90))

def probe(path):
    r = subprocess.run(['ffprobe', '-v', 'error', '-show_entries', 'stream=codec_type,codec_name,pix_fmt,field_order,nb_frames,width,height',
                        '-of', 'csv=p=0', path], capture_output=True, text=True, check=True)
    return r.stdout.split()

def picture_offsets(a, b, x0=60, x1=660, r=6):
    # how far right the picture in the middle of each line of b is from the same line of a, to a
    # fraction of a pixel, for the lines of the frames away from the top and bottom (frames, lines)
    out = []
    for fa, fb in zip(a, b):
        A = fa[40:HEIGHT - 40, x0:x1]
        B = fb[40:HEIGHT - 40, x0 - r:x1 + r]
        cost = np.stack([np.abs(B[:, r + s:r + s + (x1 - x0)] - A).mean(1) for s in range(-r, r + 1)], 1)
        k = np.clip(np.argmin(cost, 1), 1, 2 * r - 1)
        at = np.arange(len(k))
        l, c, h = cost[at, k - 1], cost[at, k], cost[at, k + 1]
        dd = l - 2 * c + h
        out.append(k - r + np.where(dd > 1e-6, np.clip(0.5 * (l - h) / np.maximum(dd, 1e-6), -0.5, 0.5), 0.0))
    return np.array(out)

def luma(path):
    r = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-f', 'rawvideo', '-pix_fmt', 'gray', '-'],
                       capture_output=True, check=True)
    return np.frombuffer(r.stdout, np.uint8).reshape(-1, HEIGHT, WIDTH).astype(float)

#------------------------------------------------------------------------
# Tests
#------------------------------------------------------------------------

failures = 0

def check(name, cond, detail=''):
    global failures
    print('%s: %s%s' % ('ok  ' if cond else 'FAIL', name, (' (' + detail + ')') if detail else ''))
    if not cond:
        failures += 1

def test(name, vcodec, extra=(), tool_args=(), seed=1, jitter=3, big=0, tilt=12, shake=1.5, same_fields=False, blanking=True,
         bff=False, field_order=None, max_err=0.25, max_out_jitter=0.35, min_kept=0.55, max_remeasure=0.6, max_bent=0.5):
    rng = np.random.default_rng(seed)
    scene = make_scene(rng, HEIGHT + 2 * PAD, blanking)
    cam = camera_offsets(rng, FRAMES * 2, tilt, shake, same_fields)
    offsets = cam + jitter_offsets(rng, FRAMES * 2, jitter, same_fields, big)
    warps = line_warps(rng, FRAMES * 2, same_fields)
    frames = render(rng, scene, offsets, warps, bff)
    steady = '-w' in tool_args

    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, 'in.mov')
        dst = os.path.join(tmp, 'out.mov')
        log_in = os.path.join(tmp, 'in.csv')
        log_out = os.path.join(tmp, 'out.csv')

        make_video(src, frames, vcodec, extra, field_order or ('bb' if bff else 'tt'))
        run_tool(list(tool_args) + ['-log', log_in, src, dst])
        rows = read_log(log_in)
        names = ['top', 'bottom'] if 'top_pos' in rows[0] else ['frame']

        check(name + ': every frame measured', len(rows) == FRAMES, '%d frames' % len(rows))

        # The picture moved down by -offset, in each field (top, bottom) of each frame, and by
        # -cam without the jitter. shown() puts the fields in the order they are shown.
        parity = lambda off: np.stack([-off[2 * np.arange(FRAMES) + (p ^ int(bff))] for p in range(len(names))], 1)
        shown = lambda a: a[:, ::-1] if bff and len(names) == 2 else a
        truth, camera = parity(offsets), parity(cam)
        p95, worst = within(column(rows, names, 'pos') - truth)
        check(name + ': measured the jitter', p95 <= max_err, '95%% within %.3f lines, worst %.3f' % (p95, worst))

        # where the picture really is in the output, if it was moved as far as the log says
        placed = truth + column(rows, names, 'shift')
        if steady:
            # the camera steadied too: the picture moves only slowly
            before, after = jitter_of(shown(truth)), jitter_of(shown(placed))
            check(name + ': jitter taken out', after <= max_out_jitter, 'from %.3f to %.3f lines' % (before, after))
            moved, left = rms(fast(shown(camera))), rms(fast(shown(placed)))
            check(name + ': camera steadied', left <= 0.3 * moved,
                  'the camera moving %.3f lines faster than 1/4 second, %.3f left' % (moved, left))
        else:
            # the jumps taken out, and the movement of the camera left as it is
            before, after = jitter_of(shown(truth - camera)), jitter_of(shown(placed - camera))
            check(name + ': jumps taken out', after <= max_out_jitter, 'from %.3f to %.3f lines' % (before, after))
            moved, off = rms(fast(shown(camera))), rms(fast(shown(placed - camera)))
            check(name + ': camera movement kept', 1 - off / moved >= min_kept,
                  'the camera moving %.3f lines faster than 1/4 second, the output %.3f from it' % (moved, off))

        # and measuring the output, it is there
        run_tool(list(tool_args) + ['-n', '-log', log_out, dst])
        p95, worst = within(column(read_log(log_out), names, 'pos') - placed)
        check(name + ': output moved as the log says', p95 <= max_remeasure, '95%% within %.3f lines, worst %.3f' % (p95, worst))

        # the edges of the picture straight up and down, unless not asked to straighten them,
        # or there is no blanking at the sides to go by
        straighten = '-H' not in tool_args or tool_args[list(tool_args).index('-H') + 1] != '0'
        refs = [r[s + '_ref'] for r in rows for s in ('left', 'right')]
        if not blanking:
            check(name + ': no edges of the picture found', all(v == '' for v in refs),
                  '%d of %d found' % (sum(v != '' for v in refs), len(refs)))
        elif not straighten:
            check(name + ': edges of the picture not looked for', all(v == '' for v in refs))
        else:
            check(name + ': edges of the picture found', all(v != '' for v in refs),
                  '%d of %d' % (sum(v != '' for v in refs), len(refs)))
        if blanking:
            before, after = bent(picture_edges(src)), bent(picture_edges(dst))
            if straighten:
                check(name + ': lines straightened', before >= 2.0 and after <= max_bent,
                      '90%% of edges within %.3f pixels, from %.3f' % (after, before))
            else:
                check(name + ': lines left where they are', abs(after - before) <= 0.1 * before,
                      '90%% of edges within %.3f pixels, from %.3f' % (after, before))

        pin, pout = probe(src), probe(dst)
        check(name + ': same video format', pin[0].split(',')[1:] == pout[0].split(',')[1:],
              '%s -> %s' % (pin[0], pout[0]))
        check(name + ': same audio streams', pin[1:] == pout[1:], '%s -> %s' % (pin[1:], pout[1:]))
        ha, hb = audio_hashes(src), audio_hashes(dst)
        check(name + ': audio copied as it is', len(ha) > 0 and ha == hb, '%d packets' % len(ha))

def test_dark_edges(name, seed=13, max_off=0.45, max_far=0.002):
    # Something dark next to the blanking, with the picture bright beside it, on a run of lines at
    # the left or right, which makes the edge of the picture look further in on those lines. The same
    # scene with and without lines moved left and right as VHS does should come out the same.
    rng = np.random.default_rng(seed)
    scene = make_scene(rng, HEIGHT + 2 * PAD)
    for _ in range(14):
        top = rng.integers(0, HEIGHT + 2 * PAD - 40)
        h = rng.integers(10, 40)
        w = rng.integers(3, 9)
        side = rng.integers(0, 2)
        x = slice(LEFT, LEFT + w) if side == 0 else slice(WIDTH - RIGHT - w, WIDTH - RIGHT)
        near = slice(LEFT + w, LEFT + w + 30) if side == 0 else slice(WIDTH - RIGHT - w - 30, WIDTH - RIGHT - w)
        scene[0][top:top + h, x] = BLANK + 4
        scene[0][top:top + h, near] = 220
    cam = camera_offsets(rng, FRAMES, 6, 0.8, False)
    warps = line_warps(rng, FRAMES, False)
    with tempfile.TemporaryDirectory() as tmp:
        out = {}
        for kind, wp, noise in (('warped', warps, seed + 1), ('straight', np.zeros_like(warps), seed + 2)):
            # (with noise of their own, which would line up where nothing is moved)
            frames = render(np.random.default_rng(noise), scene, cam, wp)
            src = os.path.join(tmp, kind + '.mov')
            dst = os.path.join(tmp, kind + '-out.mov')
            make_video(src, frames, 'v210')
            run_tool([src, dst])
            out[kind] = luma(dst)
        # how far the picture on each line is from where it should be, and how many lines are a long way off
        o = picture_offsets(out['straight'], out['warped'])
        far = np.mean(np.abs(o) > 4)
        check(name + ': lines moved back, not out of line', rms(o) <= max_off and far <= max_far,
              'the picture on each line %.3f pixels from where it should be, %.2f%% of lines 4 or more' % (rms(o), far * 100))

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

def test_range(name, ss, t):
    # -ss and -t, as FFmpeg's: both passes do only that part, the output starts at 0, and the log
    # gives where in the input each frame is
    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, 'in.mov')
        dst = os.path.join(tmp, 'out.mov')
        log = os.path.join(tmp, 'out.csv')
        subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i', 'testsrc2=size=720x486:rate=30000/1001',
                        '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=48000', '-c:a', 'pcm_s16le',
                        '-t', '5', '-c:v', 'v210', src], check=True)
        run_tool(['-P', '-ss', ss, '-t', t, '-log', log, src, dst])
        rows = read_log(log)
        r = subprocess.run(['ffprobe', '-v', 'error', '-count_frames', '-select_streams', 'v', '-show_entries', 'stream=nb_read_frames',
                            '-of', 'csv=p=0', dst], capture_output=True, text=True, check=True)
        n = int(r.stdout.strip())
        check(name + ': the frames of that part', abs(len(rows) - 60) <= 1 and n == len(rows), '%d measured, %d written' % (len(rows), n))
        check(name + ': the log says where in the input', abs(float(rows[0]['time']) - 1.0) < 0.02, rows[0]['time'])
        r = subprocess.run(['ffprobe', '-v', 'error', '-show_entries', 'stream=start_time', '-of', 'csv=p=0', dst],
                           capture_output=True, text=True, check=True)
        st = [float(x) for x in r.stdout.split()]
        check(name + ': video and audio start at 0', all(abs(x) < 0.05 for x in st), str(st))

def test_errors():
    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, 'in.mov')
        subprocess.run(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i', 'testsrc2=size=720x486:rate=30000/1001',
                        '-frames:v', '10', '-c:v', 'v210', src], check=True)
        r = subprocess.run([TOOL, '-q', src, src], capture_output=True, text=True)
        check('refuses to write over the input', r.returncode != 0 and 'write over the input' in r.stderr, r.stderr.strip())
        dst = os.path.join(tmp, 'out.mov')
        open(dst, 'w').close()
        r = subprocess.run([TOOL, '-q', src, dst], capture_output=True, text=True)
        check('refuses to write over an existing file without -y', r.returncode != 0 and os.path.getsize(dst) == 0, r.stderr.strip())
        r = subprocess.run([TOOL, '-q', '-y', src, dst], capture_output=True, text=True)
        check('writes over it with -y, with no audio', r.returncode == 0 and os.path.getsize(dst) > 0, r.stderr.strip())

test('v210', 'v210')
test('prores', 'prores_ks', extra=('-profile:v', '3'))
test('2vuy', 'rawvideo', extra=('-pix_fmt', 'uyvy422'))
test('nearest', 'v210', tool_args=('-i', 'nearest'), seed=2, max_out_jitter=0.8, min_kept=0.4, max_remeasure=0.25, max_bent=0.7)
test('progressive', 'v210', tool_args=('-P',), seed=3, same_fields=True, max_bent=0.55)
test('not straightened', 'v210', tool_args=('-H', '0'), seed=4)
test('no blanking', 'v210', seed=5, blanking=False)
test('steadied', 'v210', tool_args=('-w', '15'), seed=6)
test('fast tilt', 'v210', seed=7, tilt=40, max_out_jitter=0.5)
test('jumps further than the range', 'v210', seed=12, big=12)
test('bottom field first', 'v210', seed=8, bff=True)
test('field order not given, top first', 'v210', tool_args=('-I',), seed=9, field_order='progressive')
test('field order not given, bottom first', 'v210', tool_args=('-I',), seed=10, bff=True, field_order='progressive')
test_dark_edges('dark next to the blanking')
test_range('-ss and -t in seconds', '1', '2.002')
test_range('-ss and -t as HH:MM:SS', '00:00:01', '0:02.002')
test_interrupt('CTRL+C', ['-P'], 'Writing')
test_interrupt('CTRL+C, MP4', ['-P'], 'Writing', ext='.mp4', vcodec='libx264', audio=False)
test_errors()

print('%d failure%s' % (failures, '' if failures == 1 else 's'))
sys.exit(1 if failures else 0)
