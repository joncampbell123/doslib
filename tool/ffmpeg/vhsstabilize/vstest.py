#!/usr/bin/python3
#
# Tests for vhsstabilize.
#
# Each test makes a short interlaced video like a VHS capture, with up and down jitter
# that it knows, then checks that vhsstabilize measures the jitter, that the output has
# the jitter taken out, and that the audio in the output is the same as in the input.
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

#------------------------------------------------------------------------
# Making a test video
#------------------------------------------------------------------------

def make_scene(rng, height):
    # a still picture with things in it: boxes, bars, a gradient, in Y, U and V
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

def render(rng, scene, offsets):
    # interlaced frames of 8-bit YUV 4:2:2, top field first: field n of the video is
    # frame n // 2, lines n % 2, n % 2 + 2, ...
    sy, su, sv = scene
    frames = []
    for k in range(len(offsets) // 2):
        fy = np.empty((HEIGHT, WIDTH))
        fu = np.empty((HEIGHT, WIDTH // 2))
        fv = np.empty((HEIGHT, WIDTH // 2))
        for p in range(2):
            rows = np.arange(p, HEIGHT, 2) + PAD + offsets[2 * k + p]
            fy[p::2] = sy[rows]
            fu[p::2] = su[rows, 0::2]
            fv[p::2] = sv[rows, 0::2]
        # VHS: horizontal jitter of each line, noise, black at the top, head switching noise at the bottom
        for r in range(HEIGHT):
            s = int(rng.integers(-2, 3))
            if s:
                fy[r] = np.roll(fy[r], s)
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

def test(name, vcodec, extra=(), tool_args=(), seed=1, jitter=3, tilt=12, same_fields=False,
         max_err=0.25, max_out_jitter=0.25, max_remeasure=0.35):
    rng = np.random.default_rng(seed)
    scene = make_scene(rng, HEIGHT + 2 * PAD)
    offsets = field_offsets(rng, FRAMES * 2, jitter, tilt, same_fields)
    frames = render(rng, scene, offsets)

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
test('nearest', 'v210', tool_args=('-i', 'nearest'), seed=2, max_out_jitter=0.8, max_remeasure=0.25)
test('progressive', 'v210', tool_args=('-P',), seed=3, same_fields=True)
test_errors()

print('%d failure%s' % (failures, '' if failures == 1 else 's'))
sys.exit(1 if failures else 0)
