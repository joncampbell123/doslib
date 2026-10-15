#!/usr/bin/python3
#
# Tests for wifi-if-fix.
#
# Each test makes a short WAV file like a capture from tape: tape hiss, hum and the buzz of
# the vertical sync of a VCR, and music in some parts of it, with silence where the tape was
# stopped. Into some of the parts it puts the thumping of a wireless router as heard through the
# audio: a pulse of the same shape every 102.4 ms, each a little bigger or smaller than the last,
# a little early or late, and some of them late by more, as a router sends a beacon late when it
# waits for the channel to be clear. It checks that wifi-if-fix takes the pulses out, that it leaves
# the audio where there are no pulses exactly as it was, and that it finds nothing in audio that
# has no pulses in it. It needs numpy.
#
# Usage (after "make" has built linux-host/wifi-if-fix):
#
#   python3 wftest.py [path to wifi-if-fix]
#
# Exit status is 0 if all tests pass, 1 otherwise.
import csv
import os
import subprocess
import sys
import tempfile
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'linux-host', 'wifi-if-fix')

PERIOD = 0.1024     # seconds, 100 TU

#------------------------------------------------------------------------
# Making test audio
#------------------------------------------------------------------------

def one_pole_lowpass(x, fc, sr):
    a = np.exp(-2.0 * np.pi * fc / sr)
    y = np.empty_like(x)
    s = 0.0
    for i in range(len(x)):
        s = (1.0 - a) * x[i] + a * s
        y[i] = s
    return y

def pulse_response(sr, length=0.06):
    # what the audio circuits make of a burst of radio 2.5 ms long: the burst as a step down
    # and back up, through AC coupling (a high pass at 25 Hz) and a low pass at about 400 Hz
    n = int(length * sr)
    h = np.zeros(n)
    h[0] = 1.0
    lp = one_pole_lowpass(one_pole_lowpass(h, 400.0, sr), 400.0, sr)
    hp = lp - one_pole_lowpass(lp, 25.0, sr)
    return hp

def place_pulses(n, sr, rng, spans, amp, period=PERIOD, burst=0.0025):
    # the interference: a pulse at each beacon in each span (seconds) of the audio, and where they are
    h = pulse_response(sr)
    y = np.zeros(n)
    times = []
    t = rng.uniform(0.01, period)
    k = 0
    while t < n / sr - 0.1:
        # the period wanders a little, as the capture clock follows the tape
        p = period * (1.0 + 4e-4 * np.sin(2 * np.pi * t / 23.0))
        t_send = t + rng.normal(0, 0.00008)
        if rng.random() < 0.05:
            t_send += rng.uniform(0.0003, 0.0015)
        if any(a <= t_send < b for a, b in spans):
            s = t_send * sr
            i0 = int(np.floor(s))
            f = s - i0
            nb = burst * sr
            # the burst, to a fraction of a sample at each end
            rect = np.ones(int(np.ceil(nb + f)) + 1)
            rect[0] = 1.0 - f
            end = f + nb
            rect[int(np.floor(end))] = end - np.floor(end)
            rect[int(np.floor(end)) + 1:] = 0.0
            pulse = -np.convolve(rect, h)[:len(h)] * amp * (1.0 + rng.normal(0, 0.15))
            j = min(n, i0 + len(pulse))
            y[i0:j] += pulse[:j - i0]
            times.append(t_send)
        t += p
        k += 1
    return y, times

def hiss_hum_buzz(n, sr, rng, level=0.0003):
    t = np.arange(n) / sr
    y = rng.normal(0, level, n)
    y += 0.0004 * np.sin(2 * np.pi * 60.0 * t) + 0.0002 * np.sin(2 * np.pi * 180.0 * t + 1.0)
    # the vertical sync buzz: a short tick every field, at 59.94 Hz
    fp = 1.0 / (60000.0 / 1001.0)
    for s in np.arange(rng.uniform(0, fp), n / sr, fp):
        i = int(s * sr)
        y[i:i + int(0.0005 * sr)] += 0.0015
    return y

def music(n, sr, rng, level=0.05, beat=None):
    # tones that change, notes that start and stop, and drum hits, about level RMS
    t = np.arange(n) / sr
    y = np.zeros(n)
    pos = 0
    while pos < n:
        dur = int(rng.uniform(0.15, 0.6) * sr)
        seg = np.arange(min(dur, n - pos))
        env = np.minimum(1.0, seg / (0.01 * sr)) * np.exp(-seg / (rng.uniform(0.1, 0.5) * sr))
        f0 = rng.choice([55.0, 65.4, 73.4, 82.4, 98.0, 110.0, 130.8, 146.8, 164.8, 196.0, 220.0])
        note = np.zeros(len(seg))
        for hn in range(1, 8):
            note += np.sin(2 * np.pi * f0 * hn * seg / sr + rng.uniform(0, 6.3)) / hn
        y[pos:pos + len(seg)] += note * env
        pos += int(dur * rng.uniform(0.5, 1.0))
    # drum hits: at random, or on a beat
    hits = np.arange(0, n / sr, beat) if beat else np.sort(rng.uniform(0, n / sr, int(n / sr * 3)))
    for s in hits:
        i = int(s * sr)
        m = min(n - i, int(0.15 * sr))
        if m <= 0:
            continue
        seg = np.arange(m)
        y[i:i + m] += (rng.normal(0, 1.0, m) * np.exp(-seg / (0.02 * sr)) +
                       2.0 * np.sin(2 * np.pi * 60.0 * seg / sr) * np.exp(-seg / (0.06 * sr)))
    y += np.cumsum(rng.normal(0, 1.0, n)) * 0.002     # some rumble and low noise
    y -= np.convolve(y, np.ones(4801) / 4801, 'same')
    return y * level / max(1e-12, np.sqrt(np.mean(y * y)))

def write_wav(path, x, sr, fmt='s16'):
    # x is samples by channels, from -1 to 1
    x = np.clip(x, -1.0, 32767.0 / 32768.0)
    if fmt == 's16':
        data = np.round(x * 32768.0).astype('<i2').tobytes()
        width = 2
    elif fmt == 's24':
        v = np.round(x * 8388608.0).astype('<i4')
        b = v.astype('<i4').view(np.uint8).reshape(-1, 4)[:, :3]
        data = b.tobytes()
        width = 3
    else:
        raise ValueError(fmt)
    with wave.open(path, 'wb') as w:
        w.setnchannels(x.shape[1])
        w.setsampwidth(width)
        w.setframerate(sr)
        w.writeframes(data)

def write_wav_float(path, x, sr):
    # 32-bit float WAV, which the wave module does not write
    import struct
    data = x.astype('<f4').tobytes()
    ch = x.shape[1]
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 4 + 26 + 8 + len(data)) + b'WAVE')
        f.write(b'fmt ' + struct.pack('<IHHIIHHH', 18, 3, ch, sr, sr * ch * 4, ch * 4, 32, 0))
        f.write(b'data' + struct.pack('<I', len(data)) + data)

def read_audio(path):
    # through ffmpeg, as 64-bit float, samples by channels
    ch = int(subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'a:0', '-show_entries', 'stream=channels',
                             '-of', 'csv=p=0', path], capture_output=True, text=True, check=True).stdout.strip())
    out = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-f', 'f64le', '-acodec', 'pcm_f64le', '-'], capture_output=True, check=True).stdout
    return np.frombuffer(out, dtype='<f8').reshape(-1, ch)

def sample_fmt(path):
    return subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'a:0', '-show_entries', 'stream=codec_name',
                           '-of', 'csv=p=0', path], capture_output=True, text=True, check=True).stdout.strip()

def run_tool(args):
    r = subprocess.run([TOOL, '-q'] + args, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr)
    return r.returncode

def read_log(path):
    with open(path) as f:
        return list(csv.DictReader(f))

#------------------------------------------------------------------------
# Tests
#------------------------------------------------------------------------

failures = []

def check(cond, what):
    print('  %s: %s' % ('ok' if cond else 'FAILED', what))
    if not cond:
        failures.append(what)

def db(a, b):
    return 10.0 * np.log10(max(1e-30, a) / max(1e-30, b))

def interference_left(clean, noisy, out, spans, sr):
    # how much of the interference is left, in dB, over the spans with pulses in them
    sel = np.zeros(len(clean), bool)
    for a, b in spans:
        sel[int(a * sr):int(b * sr)] = True
    added = noisy[sel] - clean[sel]
    left = out[sel] - clean[sel]
    return db(np.sum(left * left), np.sum(added * added))

def test_capture(tmp, sr=48000, fmt='s16', channels=2):
    # 60 seconds: blank tape, music, tape stopped (silence), louder music, much louder music, blank tape.
    # The pulses are about as big, against the music, as in captures where the router could be heard
    # (their peaks about 8 dB above the RMS of the music), then as big as the RMS, then well under it.
    rng = np.random.default_rng(1234)
    n = 60 * sr
    clean = np.zeros((n, channels))
    base = hiss_hum_buzz(n, sr, rng)
    m = np.zeros(n)
    m[int(10 * sr):int(30 * sr)] = music(int(20 * sr), sr, rng, 0.012)
    m[int(36 * sr):int(44 * sr)] = music(int(8 * sr), sr, rng, 0.035)
    m[int(44 * sr):int(52 * sr)] = music(int(8 * sr), sr, rng, 0.1)
    for c in range(channels):
        clean[:, c] = base + m * (1.0 if c == 0 else 0.9)
    # tape stopped: digital silence
    clean[int(30 * sr):int(35 * sr), :] = 0.0

    spans = [(4.0, 30.0), (35.0, 57.0)]
    pulses, times = place_pulses(n, sr, rng, spans, 0.03)
    noisy = clean.copy()
    for c in range(channels):
        noisy[:, c] += pulses * (1.0 if c == 0 else 0.8)

    name = 'capture-%d-%s-%d' % (sr, fmt, channels)
    inp = os.path.join(tmp, name + '-in.wav')
    outp = os.path.join(tmp, name + '-out.wav')
    logp = os.path.join(tmp, name + '-log.csv')
    np.save(os.path.join(tmp, name + '-clean.npy'), clean)
    np.save(os.path.join(tmp, name + '-times.npy'), np.array(times))
    if fmt == 'f32':
        write_wav_float(inp, noisy, sr)
    else:
        write_wav(inp, noisy, sr, fmt)
    # what was written, as the tool reads it
    noisy_q = read_audio(inp)
    clean_q = noisy_q - (noisy - clean)

    check(run_tool(['-y', '-log', logp, inp, outp]) == 0, 'runs')
    out = read_audio(outp)
    check(out.shape == noisy_q.shape, 'output is as long as the input, with as many channels (%s, %s)' % (out.shape, noisy_q.shape))
    if out.shape != noisy_q.shape:
        return

    want = {'s16': 'pcm_s16le', 's24': 'pcm_s24le', 'f32': 'pcm_f32le'}[fmt]
    check(sample_fmt(outp) == want, 'output samples are %s, as the input' % want)

    log = read_log(logp)
    found = [float(r['seconds']) for r in log if float(r['weight']) >= 0.5]
    spans_seen = [(4.5, 29.5), (52.0, 56.5)]
    in_span = sum(1 for t in times if any(a < t < b for a, b in spans_seen))
    near = sum(1 for t in times if any(a < t < b for a, b in spans_seen) and any(abs(t - f) < 0.003 for f in found))
    check(near >= 0.97 * in_span, 'pulses found where they can be: %d of %d' % (near, in_span))
    outside = [f for f in found if not any(a - 0.2 <= f <= b + 0.2 for a, b in spans)]
    check(len(outside) == 0, 'no pulses found where there are none (%d)' % len(outside))

    for c in range(channels):
        left = interference_left(clean_q[:, c], noisy_q[:, c], out[:, c], [(4.0, 10.0)], sr)
        check(left < -25, 'channel %d, blank tape: interference left %.1f dB' % (c, left))
        left = interference_left(clean_q[:, c], noisy_q[:, c], out[:, c], [(10.0, 30.0)], sr)
        check(left < -6, 'channel %d, music: interference left %.1f dB' % (c, left))
        # too loud to find each pulse under, so it can only be taken out where the train is, not
        # how early or late each beacon is, which leaves some of it; but it must not add to it
        left = interference_left(clean_q[:, c], noisy_q[:, c], out[:, c], [(36.0, 44.0)], sr)
        check(left < 0.5, 'channel %d, louder music: interference left %.1f dB, not more than was there' % (c, left))
        left = interference_left(clean_q[:, c], noisy_q[:, c], out[:, c], [(44.0, 52.0)], sr)
        check(left < 0.5, 'channel %d, much louder music: interference left %.1f dB, not more than was there' % (c, left))
        left = interference_left(clean_q[:, c], noisy_q[:, c], out[:, c], [(52.0, 57.0)], sr)
        check(left < -25, 'channel %d, blank tape again: interference left %.1f dB' % (c, left))

    # where there are no pulses, exactly as it was
    for a, b in ((0.0, 3.5), (30.3, 34.7), (57.5, 59.9)):
        same = np.array_equal(out[int(a * sr):int(b * sr)], noisy_q[int(a * sr):int(b * sr)])
        check(same, 'audio from %.1f to %.1f s, with no pulses, is unchanged' % (a, b))

def test_nothing(tmp, what, x, sr):
    inp = os.path.join(tmp, 'in.wav')
    outp = os.path.join(tmp, 'out.wav')
    logp = os.path.join(tmp, 'log.csv')
    write_wav(inp, x, sr)
    check(run_tool(['-y', '-log', logp, inp, outp]) == 0, 'runs')
    log = read_log(logp)
    check(len(log) == 0, '%s: no pulses found (%d)' % (what, len(log)))
    check(np.array_equal(read_audio(outp), read_audio(inp)), '%s: output is the same as the input' % what)

def main():
    if not os.path.exists(TOOL):
        print('%s not found, run make first' % TOOL)
        return 1

    # WFTEST_KEEP=<directory> keeps the files each test makes there
    keep = os.environ.get('WFTEST_KEEP')
    with tempfile.TemporaryDirectory() as tmpd:
        tmp = keep if keep else tmpd
        print('A capture, 48 kHz stereo 16-bit:')
        test_capture(tmp)
        print('A capture, 44.1 kHz mono 24-bit:')
        test_capture(tmp, sr=44100, fmt='s24', channels=1)
        print('A capture, 48 kHz stereo float:')
        test_capture(tmp, fmt='f32')

        sr = 48000
        rng = np.random.default_rng(99)
        n = 40 * sr
        print('Blank tape with hum and the buzz of the vertical sync, and no router:')
        x = hiss_hum_buzz(n, sr, rng)
        test_nothing(tmp, 'blank tape', np.stack([x, x], axis=1), sr)
        print('Music with a drum every 105 ms, and no router:')
        x = hiss_hum_buzz(n, sr, rng) + music(n, sr, rng, 0.05, beat=0.105)
        test_nothing(tmp, 'music', np.stack([x, x * 0.9], axis=1), sr)
        print('A file shorter than the window it looks in:')
        x = hiss_hum_buzz(3 * sr, sr, rng)
        test_nothing(tmp, 'short file', np.stack([x, x], axis=1), sr)

    if failures:
        print('%d FAILED' % len(failures))
        return 1
    print('All passed')
    return 0

if __name__ == '__main__':
    sys.exit(main())
