#!/usr/bin/python3
#
# Tests for video-motion-interpolate.
#
# It makes a scene at 60 frames a second: a textured background panning, faster and slower, and a
# textured disk going round a curve in front of it, speeding up. Every fourth frame of it is a video of
# 15 frames a second, as an old capture card made, and the tests make that 60 frames a second again and
# compare the frames made with the frames of the scene that were left out: nearer than blending the frames
# either side, and nearer than repeating them. Then the same with a frame the capture missed, the file's
# frame rate the mean of the frames it has over the time, as the AVI of a capture card says it, and with a
# frame the capture repeated for one it missed: with those found, the frames after them are as near as
# before; and -near 2, the paths through the frames before and after, is nearer than -near 1 for the
# disk that speeds up. And -upscale, the frames, timing and audio of the output.
# It needs numpy, and ffmpeg and ffprobe to make and look at the test videos.
#
# Usage (after "make" has built linux-host/video-motion-interpolate):
#
#   python3 vmitest.py [path to video-motion-interpolate]
#
# Exit status is 0 if all tests pass, 1 otherwise.
import os
import subprocess
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'linux-host', 'video-motion-interpolate')

WIDTH = 320
HEIGHT = 240
SECONDS = 4
FAST = 60               # frames a second of the scene
SLOW = 15               # and of the capture of it

failures = []

def check(cond, what):
    print(('  ok    ' if cond else '  FAIL  ') + what)
    if not cond:
        failures.append(what)

def blur(a, sigma):
    k = np.exp(-np.arange(-6, 7) ** 2 / (2 * sigma * sigma))
    k /= k.sum()
    for axis in (0, 1):
        p = np.pad(a, [(6, 6) if i == axis else (0, 0) for i in range(a.ndim)], mode='wrap')
        a = sum(k[i] * np.take(p, np.arange(i, i + a.shape[axis]), axis) for i in range(13))
    return a

def sample(img, x, y):
    # img at (x,y), bilinear, wrapping around
    h, w = img.shape[:2]
    x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = (x - x0)[..., None], (y - y0)[..., None]
    g = lambda yy, xx: img[yy % h, xx % w]
    return (g(y0, x0) * (1 - fx) * (1 - fy) + g(y0, x0 + 1) * fx * (1 - fy) +
            g(y0 + 1, x0) * (1 - fx) * fy + g(y0 + 1, x0 + 1) * fx * fy)

def make_scene(rng):
    # the frames at FAST frames a second, RGB
    bg = np.stack([blur(rng.normal(0, 1, (512, 768)), s) for s in (2.0, 3.0, 2.5)], axis=2)
    bg = 128 + 60 * bg / bg.std()
    for _ in range(40):
        y, x = rng.integers(0, 480), rng.integers(0, 720)
        bg[y:y + 30, x:x + 40] = rng.integers(30, 225, 3)
    disk = np.stack([blur(rng.normal(0, 1, (64, 64)), 1.5) for _ in range(3)], axis=2)
    disk = np.clip(160 + 50 * disk / disk.std(), 0, 255)
    yy, xx = np.mgrid[0:HEIGHT, 0:WIDTH].astype(float)
    frames = []
    n = SECONDS * FAST
    for k in range(n):
        t = k / FAST
        # the camera pans, faster then slower
        px = 60 * t + 25 * np.sin(1.3 * t)
        py = 10 * np.sin(0.9 * t)
        f = sample(bg, xx + px, yy + py)
        # the disk goes round, speeding up
        a = 0.6 * t * t
        cx, cy = 160 + 90 * np.cos(a), 120 + 70 * np.sin(a)
        r2 = (xx - cx) ** 2 + (yy - cy) ** 2
        inside = r2 < 28 * 28
        d = sample(disk, (xx - cx + 32) % 64, (yy - cy + 32) % 64)
        f[inside] = d[inside]
        frames.append(np.clip(f, 0, 255))
    return frames

def write_video(path, frames, rate, audio=True):
    raw = path + '.rgb'
    with open(raw, 'wb') as f:
        for fr in frames:
            f.write(np.round(fr).astype(np.uint8).tobytes())
    cmd = ['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', '%dx%d' % (WIDTH, HEIGHT), '-r', rate, '-i', raw]
    if audio:
        cmd += ['-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=22050:duration=%d' % SECONDS, '-c:a', 'pcm_s16le']
    cmd += ['-c:v', 'ffv1', '-pix_fmt', 'gbrp', path]
    subprocess.run(cmd, check=True)
    os.unlink(raw)

def read_video(path, w=WIDTH, h=HEIGHT):
    r = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True, check=True)
    v = np.frombuffer(r.stdout, np.uint8)
    return v[:(len(v) // (w * h * 3)) * (w * h * 3)].reshape(-1, h, w, 3).astype(np.float64)

def psnr(a, b):
    return 10 * np.log10(255 ** 2 / max(1e-9, ((a - b) ** 2).mean()))

def run_tool(args, lossless=True):
    # the frames are measured from the output, so it is lossless, FFV1, unless asked otherwise (the GPU version
    # encodes H.264 on the GPU by default)
    r = subprocess.run([TOOL, '-y', '-q'] + (['-c', 'ffv1'] if lossless else []) + args, capture_output=True, text=True)
    sys.stdout.write(''.join('    | ' + l + '\n' for l in r.stderr.splitlines()))
    return r.returncode

def probe(path, entries):
    r = subprocess.run(['ffprobe', '-v', 'error', '-show_entries', entries, '-of', 'csv=p=0', path], capture_output=True, text=True)
    return r.stdout.split()

def score(got, scene, frames):
    # the mean PSNR of the frames made, of those given, against the scene, leaving out the first and last
    # second, where there is no frame before or after to go by
    n = min(len(got), len(scene))
    ks = [k for k in frames if FAST // 2 <= k < n - FAST // 2]
    return np.mean([psnr(got[k], scene[k]) for k in ks])

def main():
    rng = np.random.default_rng(1)
    scene = make_scene(rng)
    step = FAST // SLOW
    made = [k for k in range(len(scene)) if k % step != 0]      # the frames that are not in the capture

    with tempfile.TemporaryDirectory(prefix='vmitest') as tmp:
        print('15 frames a second to 60')
        src = os.path.join(tmp, 'slow.mkv')
        write_video(src, scene[::step], str(SLOW))
        out = os.path.join(tmp, 'out.mkv')
        r = run_tool(['-r', '60', src, out])
        check(r == 0, 'it ran')
        if r == 0:
            got = read_video(out)
            check(len(got) == SECONDS * FAST, '%d frames, %d seconds at 60 a second (%d)' % (SECONDS * FAST, SECONDS, len(got)))
            mc = score(got, scene, made)
            hold = np.mean([psnr(scene[k - k % step], scene[k]) for k in made if FAST // 2 <= k < len(scene) - FAST // 2])
            blend = np.mean([psnr(scene[k - k % step] * (1 - (k % step) / step) + scene[min(len(scene) - 1, k - k % step + step)] * ((k % step) / step), scene[k])
                             for k in made if FAST // 2 <= k < len(scene) - FAST // 2])
            check(mc > blend + 3 and mc > hold + 3, 'the frames made are nearer the scene than blending or repeating: %.2f dB, blending %.2f, repeating %.2f' % (mc, blend, hold))
            kept = score(got, scene, [k for k in range(len(scene)) if k % step == 0])
            check(kept > 40, 'the frames of the capture are as they were (%.2f dB)' % kept)
            check(probe(out, 'stream=codec_type') == ['video', 'audio'], 'the audio is copied')
            check(abs(float(probe(out, 'format=duration')[0]) - SECONDS) < 0.1, 'as long as the input')

            out1 = os.path.join(tmp, 'out1.mkv')
            if run_tool(['-r', '60', '-near', '1', src, out1]) == 0:
                # the disk, which speeds up as it goes round, more than the pan
                got1 = read_video(out1)
                mc1 = score(got1, scene, made)
                check(mc >= mc1, '-near 2 is as near as -near 1, or nearer: %.2f dB, -near 1 %.2f' % (mc, mc1))

        print('A frame the capture missed')
        # the capture's frames without one of them, at the mean rate of those it has over the time
        missing = 30
        frames = [f for i, f in enumerate(scene[::step]) if i != missing]
        src = os.path.join(tmp, 'missed.mkv')
        write_video(src, frames, '%d/%d' % (len(frames), SECONDS))
        log = os.path.join(tmp, 'missed.csv')
        out = os.path.join(tmp, 'missed-out.mkv')
        r = run_tool(['-r', '60', '-log', log, src, out])
        check(r == 0, 'it ran')
        if r == 0:
            rows = [l.split(',') for l in open(log).read().split('\n')[1:] if l]
            found = [int(x[0]) for x in rows if int(x[9]) > 0]
            check(found == [missing], 'it is found, before frame %d of the capture (%s)' % (missing, found))
            t = float(rows[missing][2])
            check(abs(t - (missing + 1) / SLOW) < 0.01, 'the frame after it is at the time it was captured (%.4f, was %.4f)' % (t, (missing + 1) / SLOW))
            got = read_video(out)
            mc = score(got, scene, made)
            out2 = os.path.join(tmp, 'missed-nodrops.mkv')
            if run_tool(['-r', '60', '-nodrops', src, out2]) == 0:
                nd = score(read_video(out2), scene, made)
                check(mc > nd + 2, 'the frames made are nearer the scene than taking the frames as the file times them: %.2f dB, %.2f' % (mc, nd))

        print('A frame the capture repeated')
        rep = 25
        frames = [f for f in scene[::step]]
        frames[rep] = frames[rep - 1]
        src = os.path.join(tmp, 'repeat.mkv')
        write_video(src, frames, str(SLOW))
        log = os.path.join(tmp, 'repeat.csv')
        out = os.path.join(tmp, 'repeat-out.mkv')
        r = run_tool(['-r', '60', '-log', log, src, out])
        check(r == 0, 'it ran')
        if r == 0:
            rows = [l.split(',') for l in open(log).read().split('\n')[1:] if l]
            found = [int(x[0]) for x in rows if x[8] == '1']
            check(found == [rep], 'it is found, frame %d of the capture (%s)' % (rep, found))
            got = read_video(out)
            k = rep * step
            check(psnr(got[k], scene[k]) > psnr(scene[k - step], scene[k]) + 3, 'the frame made for it is the scene there, not the frame before (%.2f dB, the frame before %.2f)'
                  % (psnr(got[k], scene[k]), psnr(scene[k - step], scene[k])))

        print('-smooth, frames of uneven moments')
        # Film at 24 frames a second, on TV at 30 (each film frame shown for two or three fields), captured at
        # 15: frame c of the capture is of film frame 1.6 c, down to a whole frame, which is a step of one film
        # frame or two from frame to frame, the file saying 15 a second, even. And one frame the same as the
        # one before, as a capture card that was late makes.
        film = [int(np.floor(1.6 * c)) for c in range(int(SECONDS * SLOW))]
        film[21] = film[20]
        idx = [min(len(scene) - 1, int(round(FAST * f / 24.0))) for f in film]
        src = os.path.join(tmp, 'uneven.mkv')
        write_video(src, [scene[i] for i in idx], str(SLOW))
        log = os.path.join(tmp, 'uneven.csv')
        out0 = os.path.join(tmp, 'uneven-0.mkv')
        out8 = os.path.join(tmp, 'uneven-8.mkv')
        outd = os.path.join(tmp, 'uneven-default.mkv')
        if (run_tool(['-r', '60', '-nodrops', '-smooth', '0', src, out0]) == 0 and run_tool(['-r', '60', '-nodrops', '-smooth', '8', '-log', log, src, out8]) == 0 and
            run_tool(['-r', '60', '-nodrops', src, outd]) == 0):
            rows = [l.split(',') for l in open(log).read().split('\n')[1:] if l]
            reps = [int(x[0]) for x in rows if x[8] == '1']
            check(21 in reps, 'the frame that is the same moment as the one before is left out (%s)' % reps)
            # how far each frame is put from the time of what is in it, from frame to frame (the times are kept
            # by the frames' own, which film on TV is behind, by half a film frame as a rule, which does not show)
            times = [float(x[2]) for x in rows]
            mid = [j for j in range(4, len(idx) - 4) if j not in reps]
            err0 = np.std([j / SLOW - idx[j] / FAST for j in mid])
            err8 = np.std([times[j] - idx[j] / FAST for j in mid])
            check(err8 < 0.5 * err0, 'each frame is put at the time of what is in it, to within %.1f ms, against %.1f ms at the even times' % (1000 * err8, 1000 * err0))
            # the frames made, against the scene at their times, the scene a few frames either way
            got0, got8 = read_video(out0), read_video(out8)
            ks = range(4 * step, (len(idx) - 8) * step)
            def best(got):
                return max(np.mean([psnr(got[k], scene[k + d]) for k in ks if k < len(got) and 0 <= k + d < len(scene)]) for d in range(-3, 4))
            s0, s8 = best(got0), best(got8)
            check(s8 > s0 + 1, 'the frames are nearer the scene at their times: %.2f dB, against %.2f without it' % (s8, s0))
            check(np.array_equal(read_video(outd), got0), 'off by default: the same as -smooth 0')
        else:
            check(False, 'it ran')

        print('Fading in from black')
        # a picture that does not move, black, then fading in over a second: the frames made are of the
        # picture where it is, between as bright as the frames either side, not moved anywhere
        still = scene[0]
        fade = [still * min(1.0, max(0.0, (k - 8) / 15.0)) for k in range(SLOW * 2)]
        src = os.path.join(tmp, 'fade.mkv')
        write_video(src, fade, str(SLOW))
        log = os.path.join(tmp, 'fade.csv')
        out = os.path.join(tmp, 'fade-out.mkv')
        r = run_tool(['-r', '60', '-log', log, src, out])
        check(r == 0, 'it ran')
        if r == 0:
            rows = [l.split(',') for l in open(log).read().split('\n')[1:] if l]
            moved = max(float(x[4]) for x in rows)
            check(moved < 1.0, 'nothing moves, from frame to frame (at most %.2f pixels)' % moved)
            got = read_video(out)
            ks = [k for k in range(8 * step, 24 * step) if k % step != 0 and k < len(got)]
            blend = [fade[k // step] * (1 - (k % step) / step) + fade[min(len(fade) - 1, k // step + 1)] * ((k % step) / step) for k in ks]
            worst = min(psnr(got[k], b) for k, b in zip(ks, blend))
            check(worst > 38, 'the frames made are the picture, brighter, where it is (worst %.2f dB from blending)' % worst)

        print('8-bit paletted video')
        # as 8-bit video for Windows is: an index into a palette of colors for each pixel
        src = os.path.join(tmp, 'pal8.avi')
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', os.path.join(tmp, 'slow.mkv'), '-filter_complex',
                        '[0:v]split[a][b];[a]palettegen=max_colors=256[p];[b][p]paletteuse=dither=none', '-c:v', 'rawvideo', '-pix_fmt', 'pal8',
                        '-c:a', 'copy', src], check=True)
        out = os.path.join(tmp, 'pal8.mkv')
        r = run_tool(['-r', '60', '-t', '2', src, out])
        check(r == 0, 'it ran')
        if r == 0:
            got = read_video(out)
            want = read_video(src)
            sat = np.abs(got - got.mean(axis=3, keepdims=True)).mean()
            want_sat = np.abs(want - want.mean(axis=3, keepdims=True)).mean()
            check(sat > 0.8 * want_sat, 'it is in color, as the palette is (%.1f levels from gray, the input %.1f)' % (sat, want_sat))
            kept = np.mean([psnr(got[k * step], want[k]) for k in range(1, 7)])
            check(kept > 35, 'the frames of the input are as they were (%.2f dB)' % kept)

        print('-upscale 2')
        src = os.path.join(tmp, 'slow.mkv')
        out = os.path.join(tmp, 'up.mkv')
        r = run_tool(['-r', '30', '-upscale', '2', '-t', '1', src, out])
        check(r == 0, 'it ran')
        if r == 0:
            check(probe(out, 'stream=width,height')[0] == '640,480', 'the output is 640x480 (%s)' % probe(out, 'stream=width,height')[0])
            got = read_video(out, 640, 480)
            check(abs(len(got) - 30) <= 1, '-r 30 -t 1: 30 frames (%d)' % len(got))

        if '-hwaccel' in subprocess.run([TOOL, '-h'], capture_output=True, text=True).stderr:
            print('Decoding and encoding on the GPU (-hwaccel)')
            src = os.path.join(tmp, 'slow-h264.mkv')
            subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', os.path.join(tmp, 'slow.mkv'), '-c:v', 'libx264', '-crf', '10', '-pix_fmt', 'yuv420p', '-c:a', 'copy', src], check=True)
            ref = os.path.join(tmp, 'hw-ref.mkv')
            out = os.path.join(tmp, 'hw.mkv')
            if run_tool(['-r', '60', '-hwaccel', 'none', src, ref]) == 0 and run_tool(['-r', '60', src, out], lossless=False) == 0:
                def luma(p):
                    r = subprocess.run(['ffmpeg', '-v', 'error', '-i', p, '-f', 'rawvideo', '-pix_fmt', 'gray', '-'], capture_output=True, check=True)
                    v = np.frombuffer(r.stdout, np.uint8)
                    return v[:(len(v) // (WIDTH * HEIGHT)) * (WIDTH * HEIGHT)].reshape(-1, HEIGHT, WIDTH).astype(np.float64)
                a, b = luma(out), luma(ref)
                n = min(len(a), len(b))
                ps = [psnr(a[k], b[k]) for k in range(n)]
                drift = max(abs((a[k] - b[k]).mean()) for k in range(n))
                check(len(a) == len(b), 'as many frames as decoding and encoding on the CPU (%d, %d)' % (len(a), len(b)))
                check(min(ps) > 35 and drift < 2, 'the same frames, as near as encoding allows: worst %.1f dB, brightness at most %.2f off' % (min(ps), drift))
            else:
                check(False, 'it ran')

        print('Options')
        check(run_tool(['-r', 'fast', src, out]) != 0, '-r fast: refused')
        check(run_tool(['-upscale', '0.5', src, out]) != 0, '-upscale 0.5: refused')
        check(run_tool(['-near', '3', src, out]) != 0, '-near 3: refused')

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
