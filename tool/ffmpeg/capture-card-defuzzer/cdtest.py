#!/usr/bin/python3
#
# Tests for capture-card-defuzzer.
#
# Each test makes a short video of a sharp picture: edges, a line one line high, text, a ramp and some
# noise, and makes each line of it the mean of it and the line above, as the capture card did (avg2y).
# Then it checks that capture-card-defuzzer brings back something near the sharp picture, nearer than the
# card's, with its edges sharp and nothing either side of them (no ringing), and that the frames, their
# timing and the audio are as they were.
# It needs numpy, and ffmpeg and ffprobe to make and look at the test videos.
#
# Usage (after "make" has built linux-host/capture-card-defuzzer):
#
#   python3 cdtest.py [path to capture-card-defuzzer]
#
# Exit status is 0 if all tests pass, 1 otherwise.
import os
import subprocess
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'linux-host', 'capture-card-defuzzer')

WIDTH = 320
HEIGHT = 240
FRAMES = 30
RATE = '30000/1001'

failures = []

def check(cond, what):
    print(('  ok    ' if cond else '  FAIL  ') + what)
    if not cond:
        failures.append(what)

def sharp_picture(rng, k):
    # a picture with horizontal edges of all sizes, lines one line high, a ramp, and a little noise
    y = np.full((HEIGHT, WIDTH), 60.0)
    y[20:60, 20:140] = 200                  # a big bright box
    y[30:50, 40:120] = 40                   # a dark box in it
    y[80:81, 20:300] = 220                  # a bright line, one line high
    y[100:101, 20:300] = 20                 # a dark one
    y[120:140, 160:300] = 90                # a small step
    for i in range(8):                      # bars of all heights
        h = i + 1
        y[150 + (i * 10):150 + (i * 10) + h, 20:140] = 180
    y[150:230, 160:300] = np.linspace(30, 200, 80)[:, None]    # a ramp
    y[60 + (k % 3):64 + (k % 3), 200:260] = 230                # something moving
    y += rng.normal(0, 1.5, y.shape)
    return np.clip(y, 0, 255)

def card(y):
    # each line the mean of it and the line above, as the card did
    return (np.vstack([y[:1], y[:-1]]) + y) / 2

def write_video(path, frames, codec_args, audio=True):
    raw = path + '.yuv'
    with open(raw, 'wb') as f:
        for y in frames:
            f.write(np.clip(np.round(y), 0, 255).astype(np.uint8).tobytes())
            f.write(np.full(WIDTH * HEIGHT // 2, 128, np.uint8).tobytes())
    cmd = ['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'yuv420p', '-s', '%dx%d' % (WIDTH, HEIGHT), '-r', RATE, '-i', raw]
    if audio:
        cmd += ['-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=44100:duration=%f' % (FRAMES * 1001 / 30000)]
    cmd += codec_args + (['-c:a', 'mp2'] if audio else []) + [path]
    subprocess.run(cmd, check=True)
    os.unlink(raw)

def read_video(path):
    # the luma as it is, not made full range as -pix_fmt gray would
    r = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-f', 'rawvideo', '-pix_fmt', 'yuv420p', '-'], capture_output=True, check=True)
    fs = WIDTH * HEIGHT * 3 // 2
    v = np.frombuffer(r.stdout, np.uint8)
    return v[:(len(v) // fs) * fs].reshape(-1, fs)[:, :WIDTH * HEIGHT].reshape(-1, HEIGHT, WIDTH).astype(np.float64)

def run_tool(args):
    r = subprocess.run([TOOL, '-y', '-q'] + args, capture_output=True, text=True)
    sys.stdout.write(''.join('    | ' + l + '\n' for l in r.stderr.splitlines()))
    return r.returncode

def probe(path, entries):
    r = subprocess.run(['ffprobe', '-v', 'error', '-show_entries', entries, '-of', 'csv=p=0', path], capture_output=True, text=True)
    return r.stdout.split()

def main():
    rng = np.random.default_rng(1)
    sharp = [sharp_picture(rng, k) for k in range(FRAMES)]
    blurred = [card(y) for y in sharp]

    with tempfile.TemporaryDirectory(prefix='cdtest') as tmp:
        print('avg2y, from lossless video')
        src = os.path.join(tmp, 'card.mkv')
        write_video(src, blurred, ['-c:v', 'ffv1'])
        out = os.path.join(tmp, 'out.mkv')
        r = run_tool(['-deblock', 'none', src, out])
        check(r == 0, 'it ran')
        if r == 0:
            got = read_video(out)
            want = np.array(sharp)
            card_v = np.array([np.round(b) for b in blurred])
            check(len(got) == FRAMES, 'all %d frames are there (%d)' % (FRAMES, len(got)))
            inner = (slice(None), slice(2, HEIGHT - 2), slice(4, WIDTH - 4))
            e_out = np.abs(got - want)[inner].mean()
            e_card = np.abs(card_v - want)[inner].mean()
            check(e_out < e_card * 0.6, 'nearer the sharp picture than the card\'s: mean error %.2f, the card\'s %.2f' % (e_out, e_card))
            # each edge as sharp as it was: the line under the top of the big box is bright, the one above dark
            col = got[:, :, 30:130].mean(axis=(0, 2))
            check(col[19] < 75 and col[20] > 185, 'the top of the bright box is sharp (%.0f, %.0f; was 60, 200)' % (col[19], col[20]))
            check(col[59] > 185 and col[60] < 75, 'the bottom of it is sharp (%.0f, %.0f)' % (col[59], col[60]))
            # a line one line high comes back, which the card made two lines of half its brightness
            line = got[:, 80, 30:290].mean()
            check(line > 180, 'a bright line one line high comes back (%.0f; was 220, the card made it 140)' % line)
            # no ringing: nothing either side of an edge brighter or darker than what is there
            above = got[:, 10:18, 30:130]
            below = got[:, 62:72, 30:130]
            ring = max(np.abs(above.mean(axis=(0, 2)) - 60).max(), np.abs(below.mean(axis=(0, 2)) - 60).max())
            check(ring < 2.5, 'no ringing above or below the bright box (at most %.2f levels off)' % ring)
            inside = got[:, 22:28, 30:130].mean(axis=(0, 2))
            check(np.abs(inside - 200).max() < 3, 'nor inside it (at most %.2f levels off)' % np.abs(inside - 200).max())
            # the ramp is a ramp, not steps
            ramp = got[:, 155:225, 170:290].mean(axis=(0, 2))
            want_ramp = np.array(sharp)[:, 155:225, 170:290].mean(axis=(0, 2))
            check(np.abs(ramp - want_ramp).max() < 3, 'the ramp is as it was (at most %.2f levels off)' % np.abs(ramp - want_ramp).max())
            # noise stays noise, about as much down as across, not columns of streaks
            flat = got[:, 104:118, 20:150]
            hd = np.abs(np.diff(flat, axis=2)).mean()
            vd = np.abs(np.diff(flat, axis=1)).mean()
            check(vd > 0.5 * hd, 'the noise is as much down as across, not streaks (%.2f down, %.2f across)' % (vd, hd))
            check(probe(out, 'stream=codec_type') == ['video', 'audio'], 'the audio is copied')
            check(probe(out, 'stream=codec_name')[1] == 'mp2', 'as it was')
            d_in, d_out = float(probe(src, 'format=duration')[0]), float(probe(out, 'format=duration')[0])
            check(abs(d_in - d_out) < 0.1, 'as long as the input (%.3f, %.3f seconds)' % (d_in, d_out))

        print('avg2y, from MPEG-1, taking out the blocks first')
        src = os.path.join(tmp, 'card.mpg')
        write_video(src, blurred, ['-c:v', 'mpeg1video', '-b:v', '1000k', '-f', 'mpeg'])
        for deblock in ('auto', 'none', 'pp7'):
            out = os.path.join(tmp, 'out-%s.mkv' % deblock)
            r = run_tool(['-deblock', deblock, src, out])
            check(r == 0, '-deblock %s: it ran' % deblock)
            if r == 0:
                got = read_video(out)
                card_v = read_video(src)
                n = min(len(got), len(card_v), FRAMES)
                inner = (slice(None), slice(2, HEIGHT - 2), slice(4, WIDTH - 4))
                want = np.array(sharp[:n])
                e_out = np.abs(got[:n] - want)[inner].mean()
                e_card = np.abs(card_v[:n] - want)[inner].mean()
                check(e_out < e_card, '-deblock %s: nearer the sharp picture than the card\'s MPEG: %.2f, the card\'s %.2f' % (deblock, e_out, e_card))

        print('Options')
        out = os.path.join(tmp, 'out-t.mkv')
        r = run_tool(['-t', '0.5', os.path.join(tmp, 'card.mkv'), out])
        check(r == 0 and abs(len(read_video(out)) - 15) <= 1, '-t 0.5: half a second of it')
        check(run_tool(['-filter', 'nosuch', os.path.join(tmp, 'card.mkv'), out]) != 0, '-filter nosuch: refused')
        check(run_tool(['-filter', 'avg2y', '-deblock', 'nosuchfilter', os.path.join(tmp, 'card.mkv'), out]) != 0, '-deblock nosuchfilter: refused')

    print()
    if failures:
        print('%d FAILED' % len(failures))
        for f in failures:
            print('  ' + f)
        return 1
    print('All passed')
    return 0

if __name__ == '__main__':
    sys.exit(main())
