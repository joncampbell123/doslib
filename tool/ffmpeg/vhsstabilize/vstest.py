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

def field_offsets(rng, count, jitter, tilt, same_fields):
    # how far up (in lines of the frame) the picture is taken from the scene, for each field,
    # or for each frame if same_fields. Like VHS, most fields are where they should be, but
    # some jump up or down, now and then several in a row, and so does the first frame's
    # second field, so that the fields of the first frame are not lined up with each other.
    t = np.arange(count) / 59.94
    slow = np.round(tilt * np.sin(2 * np.pi * t / 6.0))
    fast = np.where(rng.random(count) < 0.4, rng.integers(-jitter, jitter + 1, count), 0)
    for start in rng.integers(0, count - 6, max(1, count // 100)):
        fast[start:start + 4] += rng.choice([-1, 1]) * (jitter + 3)
    fast[1] = 3
    off = (slow + fast).astype(int)
    if same_fields:
        off[1::2] = off[0::2]
    return off

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

def render(rng, scene, offsets, warps):
    # interlaced frames of 8-bit YUV 4:2:2, top field first: field n of the video is
    # frame n // 2, lines n % 2, n % 2 + 2, ..., with the picture on each line moved right by warps
    sy, su, sv = scene
    frames = []
    for k in range(len(offsets) // 2):
        fy = np.empty((HEIGHT, WIDTH))
        fu = np.empty((HEIGHT, WIDTH // 2))
        fv = np.empty((HEIGHT, WIDTH // 2))
        for p in range(2):
            rows = np.arange(p, HEIGHT, 2) + PAD + offsets[2 * k + p]
            h = warps[2 * k + p]
            fy[p::2] = shift_rows(sy[rows], h)
            fu[p::2] = shift_rows(su[rows, 0::2], h / 2)
            fv[p::2] = shift_rows(sv[rows, 0::2], h / 2)
        # VHS: noise, black at the top, head switching noise at the bottom
        fy += rng.normal(0, 2.5, fy.shape)
        fy[:6] = 16
        fy[-8:] = rng.integers(16, 236, (8, WIDTH))
        frames.append(b''.join(np.clip(np.round(a), 0, 255).astype(np.uint8).tobytes() for a in (fy, fu, fv)))
    return frames

def make_video(path, frames, vcodec, extra=()):
    # with two audio streams, so that copying them is tested
    cmd = ['ffmpeg', '-v', 'error', '-y',
           '-f', 'rawvideo', '-pix_fmt', 'yuv422p', '-s', '%dx%d' % (WIDTH, HEIGHT), '-r', '30000/1001', '-i', '-',
           '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=48000',
           '-f', 'lavfi', '-i', 'sine=frequency=1000:sample_rate=48000',
           '-map', '0:v', '-map', '1:a', '-map', '2:a', '-shortest',
           '-c:v', vcodec] + list(extra) + [
           '-field_order', 'tt', '-c:a:0', 'pcm_s16le', '-c:a:1', 'pcm_s24le', path]
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

#------------------------------------------------------------------------
# Tests
#------------------------------------------------------------------------

failures = 0

def check(name, cond, detail=''):
    global failures
    print('%s: %s%s' % ('ok  ' if cond else 'FAIL', name, (' (' + detail + ')') if detail else ''))
    if not cond:
        failures += 1

def test(name, vcodec, extra=(), tool_args=(), seed=1, jitter=3, tilt=12, same_fields=False, blanking=True,
         max_err=0.25, max_out_jitter=0.25, max_remeasure=0.35, max_bent=0.5):
    rng = np.random.default_rng(seed)
    scene = make_scene(rng, HEIGHT + 2 * PAD, blanking)
    offsets = field_offsets(rng, FRAMES * 2, jitter, tilt, same_fields)
    warps = line_warps(rng, FRAMES * 2, same_fields)
    frames = render(rng, scene, offsets, warps)

    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, 'in.mov')
        dst = os.path.join(tmp, 'out.mov')
        log_in = os.path.join(tmp, 'in.csv')
        log_out = os.path.join(tmp, 'out.csv')

        make_video(src, frames, vcodec, extra)
        run_tool(list(tool_args) + ['-log', log_in, src, dst])
        rows = read_log(log_in)
        names = ['top', 'bottom'] if 'top_pos' in rows[0] else ['frame']

        check(name + ': every frame measured', len(rows) == FRAMES, '%d frames' % len(rows))

        # the picture moved down by -offset
        truth = -offsets.reshape(-1, 2)[:, :len(names)].astype(float)
        p95, worst = within(column(rows, names, 'pos') - truth)
        check(name + ': measured the jitter', p95 <= max_err, '95%% within %.3f lines, worst %.3f' % (p95, worst))

        # where the picture really is in the output, if it was moved as far as the log says
        placed = truth + column(rows, names, 'shift')
        before, after = jitter_of(truth), jitter_of(placed)
        check(name + ': jitter taken out', after <= max_out_jitter, 'from %.3f to %.3f lines' % (before, after))

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
test('nearest', 'v210', tool_args=('-i', 'nearest'), seed=2, max_out_jitter=0.8, max_remeasure=0.25, max_bent=0.7)
test('progressive', 'v210', tool_args=('-P',), seed=3, same_fields=True, max_bent=0.55)
test('not straightened', 'v210', tool_args=('-H', '0'), seed=4)
test('no blanking', 'v210', seed=5, blanking=False)
test_errors()

print('%d failure%s' % (failures, '' if failures == 1 else 's'))
sys.exit(1 if failures else 0)
