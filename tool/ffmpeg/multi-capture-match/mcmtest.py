#!/usr/bin/python3
#
# Tests for multi-capture-match.
#
# It makes a "tape": a video of scenes from FFmpeg's test sources, cut together, with sound. Then it makes
# captures of it, each of a different part of it, as a different VCR and capture card would: brighter or
# darker, noisier, one at another size, with frames dropped (and the frame before repeated, as a capture
# card does), dropouts on some frames, black before and after, and one that stopped recording for a while.
# It knows which frame of the tape each frame of each capture is. Then it checks, from the -log of
# multi-capture-match, that the sequence it makes has each frame of the tape that any capture has, once,
# in order, and none of the damaged or repeated frames where another capture has that frame undamaged;
# and that the .prproj and .xml it writes are well formed and say the same.
# It needs numpy, and ffmpeg to make the test videos.
#
# Usage (after "make" has built linux-host/multi-capture-match):
#
#   python3 mcmtest.py [path to multi-capture-match]
#
# Exit status is 0 if all tests pass, 1 otherwise.
import csv
import gzip
import os
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'linux-host', 'multi-capture-match')

WIDTH = 360
HEIGHT = 240
SCENE = 180         # frames of each scene
RATE = '60000/1001'
FPS = 60000 / 1001
AUDIO_RATE = 48000

failures = []

def check(cond, what):
    print(('  ok    ' if cond else '  FAIL  ') + what)
    if not cond:
        failures.append(what)

#------------------------------------------------------------------------
# The tape
#------------------------------------------------------------------------

def make_tape(tmp):
    # scenes of different sources, cut together, and a sound of tones and noise that changes all the time
    s = 's=%dx%d:rate=%s' % (WIDTH, HEIGHT, RATE)
    srcs = ['mandelbrot=%s' % s,
            'life=%s:mold=10:ratio=0.3:death_color=#203060:life_color=#e0c080' % s,
            'testsrc2=%s,scroll=h=0.004:v=0.003' % s,
            'gradients=%s:speed=0.03:n=5:c0=red:c1=blue:c2=white:c3=black:c4=yellow:seed=5' % s,
            'mandelbrot=%s:start_x=-0.75:start_y=0.1:start_scale=0.5' % s]
    graph = ''.join('%s,trim=end_frame=%d,setpts=PTS-STARTPTS,format=yuv420p[v%d];' % (src, SCENE, i) for i, src in enumerate(srcs))
    graph += ''.join('[v%d]' % i for i in range(len(srcs))) + 'concat=n=%d:v=1:a=0[v]' % len(srcs)
    frames = SCENE * len(srcs)
    yuv = os.path.join(tmp, 'tape.yuv')
    pcm = os.path.join(tmp, 'tape.pcm')
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-filter_complex', graph, '-map', '[v]', '-f', 'rawvideo', '-pix_fmt', 'yuv420p', yuv], check=True)
    secs = frames / FPS + 1
    aud = ("aevalsrc='0.25*sin(2*PI*(300+200*sin(2*PI*0.37*t))*t)*(0.6+0.4*sin(2*PI*1.3*t))|"
           "0.25*sin(2*PI*(700+300*sin(2*PI*0.23*t))*t)*(0.6+0.4*sin(2*PI*0.9*t))':s=%d:d=%f[a];"
           "anoisesrc=r=%d:color=pink:amplitude=0.15:seed=7:d=%f,aformat=channel_layouts=stereo[n];"
           "[a][n]amix=inputs=2:normalize=0,volume='0.5+0.5*sin(2*PI*0.7*t)':eval=frame[out]") % (AUDIO_RATE, secs, AUDIO_RATE, secs)
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-filter_complex', aud, '-map', '[out]', '-f', 's16le', '-ac', '2', '-ar', str(AUDIO_RATE), pcm], check=True)
    fsize = WIDTH * HEIGHT * 3 // 2
    v = np.fromfile(yuv, dtype=np.uint8)
    v = v[:(len(v) // fsize) * fsize].reshape(-1, fsize)
    a = np.fromfile(pcm, dtype=np.int16).reshape(-1, 2)
    return v, a

#------------------------------------------------------------------------
# A capture of it
#------------------------------------------------------------------------

def samples_of(frame):
    return int(round(frame * AUDIO_RATE / FPS))

def make_capture(tmp, name, tape, audio, segments, drops=(), damage=(), lead=0, trail=0,
                 bright=0, noise=0.0, size=None, sar='8/9', seed=1):
    # segments: (first, count) of frames of the tape, recorded one after the other. drops: frames of the
    # tape the capture card dropped, repeating the frame before. damage: frames of the tape with dropouts.
    # The truth: the frame of the tape each frame of the capture is a picture of, -1 for black.
    rng = np.random.default_rng(seed)
    truth = [-1] * lead
    pcm = [np.zeros((samples_of(lead), 2), dtype=np.int16)]
    for first, count in segments:
        truth += list(range(first, first + count))
        pcm.append(audio[samples_of(first):samples_of(first + count)])
    truth += [-1] * trail
    pcm.append(np.zeros((samples_of(trail), 2), dtype=np.int16))
    pic = list(truth)
    for k in range(1, len(truth)):
        if truth[k] in drops:
            pic[k] = pic[k - 1]
    ysize = WIDTH * HEIGHT
    frames = []
    damaged = set()
    for k, t in enumerate(pic):
        if k > 0 and truth[k] in drops:
            # a dropped frame: the frame before it again, as it is
            frames.append(frames[-1].copy())
            if k - 1 in damaged:
                damaged.add(k)
            continue
        if t < 0:
            f = np.concatenate([np.full(ysize, 16, np.uint8), np.full(ysize // 2, 128, np.uint8)])
        else:
            f = tape[t].copy()
        y = f[:ysize].reshape(HEIGHT, WIDTH).astype(np.float32)
        if t >= 0:
            y = y + bright + rng.normal(0, noise, y.shape) if noise > 0 else y + bright
        if truth[k] in damage:
            # dropouts: white streaks, and lines of static, across most of the picture
            for row in (60, 61, 130, 170):
                y[row, 20:WIDTH - 20] = 235
            y[100:103, 30:WIDTH - 60] = rng.integers(16, 235, (3, WIDTH - 90))
            damaged.add(k)
        f[:ysize] = np.clip(y, 0, 255).astype(np.uint8).ravel()
        frames.append(f)
    path = os.path.join(tmp, name)
    vf = 'setsar=%s' % sar
    if size:
        vf = 'scale=%d:%d,setsar=1' % size
    raw = os.path.join(tmp, name + '.yuv')
    np.concatenate(frames).tofile(raw)
    praw = os.path.join(tmp, name + '.pcm')
    np.concatenate(pcm).tofile(praw)
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'yuv420p', '-s', '%dx%d' % (WIDTH, HEIGHT), '-r', RATE, '-i', raw,
                    '-f', 's16le', '-ar', str(AUDIO_RATE), '-ac', '2', '-i', praw, '-vf', vf, '-c:v', 'ffv1', '-c:a', 'pcm_s16le', path], check=True)
    os.unlink(raw)
    os.unlink(praw)
    # what each frame is a picture of, and whether it is usable: not a repeat, not damaged, not black
    good = [pic[k] >= 0 and pic[k] == truth[k] and k not in damaged for k in range(len(truth))]
    return {'path': path, 'truth': pic, 'good': good}

#------------------------------------------------------------------------
# Checking what it made
#------------------------------------------------------------------------

def run_tool(args):
    r = subprocess.run([TOOL, '-y'] + args, capture_output=True, text=True)
    sys.stdout.write(''.join('    | ' + l + '\n' for l in r.stderr.splitlines()))
    return r.returncode

def read_log(path):
    with open(path) as f:
        return [row for row in csv.DictReader(f)]

def check_sequence(rows, caps, what):
    # each frame of the tape that a capture has usable, once, in order, from a capture that has it usable
    seq = []
    bad = []
    for row in rows:
        if row['capture'] == '':
            continue
        c = caps[int(row['capture']) - 1]
        k = int(row['capture_frame'])
        seq.append(c['truth'][k])
        if not c['good'][k]:
            bad.append((int(row['sequence_frame']), int(row['capture']), k, c['truth'][k]))
    have = sorted(set(t for c in caps for k, t in enumerate(c['truth']) if c['good'][k]))
    inorder = all(seq[i] < seq[i + 1] for i in range(len(seq) - 1))
    missing = sorted(set(have) - set(seq))
    check(inorder, '%s: the frames of the tape are in order, each once' % what)
    if not inorder:
        print('        out of order at', [(i, seq[i], seq[i + 1]) for i in range(len(seq) - 1) if seq[i] >= seq[i + 1]][:10])
    check(not missing, '%s: every frame of the tape that a capture has is there (%d of %d)' % (what, len(have) - len(missing), len(have)))
    if missing:
        print('        missing', missing[:20])
    # a damaged or repeated frame only where no capture has that frame usable
    avoidable = [b for b in bad if b[3] in have]
    check(not avoidable, '%s: no damaged, repeated or black frame where a capture has it usable' % what)
    if avoidable:
        print('        (sequence frame, capture, frame, tape frame)', avoidable[:10])
    return seq

def check_prproj(path, rows, caps, name, nest=False):
    # returns the start, end, in and out of each clip of the top track
    data = gzip.open(path).read()
    root = ET.fromstring(data)
    ids, uids = {}, {}
    for e in root:
        if 'ObjectID' in e.attrib:
            ok = e.attrib['ObjectID'] not in ids
            ids[e.attrib['ObjectID']] = e
            if not ok:
                check(False, 'prproj: ObjectID %s is used once' % e.attrib['ObjectID'])
        if 'ObjectUID' in e.attrib:
            uids[e.attrib['ObjectUID']] = e
    refs = [x.attrib['ObjectRef'] for x in root.iter() if 'ObjectRef' in x.attrib and x not in list(root)]
    urefs = [x.attrib['ObjectURef'] for x in root.iter() if 'ObjectURef' in x.attrib]
    # the Project's own ProjectViewState.List numbers objects of its own, inside it
    proj = [e for e in root if e.tag == 'Project' and 'ObjectID' in e.attrib][0]
    inner = set(x.attrib['ObjectRef'] for x in proj.iter('Second')) | set(x.attrib['ObjectRef'] for x in proj.iter() if x.tag == 'Columns.List')
    inner |= set(x.attrib['ObjectRef'] for x in proj.iter('Column'))
    proj_refs = set(x.attrib['ObjectRef'] for x in proj.iter() if 'ObjectRef' in x.attrib and x.tag not in ('Second', 'Columns.List', 'Column'))
    dangling = [r for r in refs if r not in ids and r not in inner]
    check(not dangling, 'prproj: every ObjectRef is an object (%d of them)' % len(refs))
    if dangling:
        print('        ', dangling[:10])
    udangling = [r for r in urefs if r not in uids]
    check(not udangling, 'prproj: every ObjectURef is an object (%d of them)' % len(urefs))
    # the top video track is the sequence: its clips, end to end
    seqs = [e for e in root if e.tag == 'Sequence']
    seq = [e for e in seqs if e.findtext('Name') == name][0]
    vgroup = ids[seq.find('TrackGroups').findall('TrackGroup')[0].find('Second').attrib['ObjectRef']]
    tracks = [uids[t.attrib['ObjectURef']] for t in vgroup.find('TrackGroup').find('Tracks')]
    top = tracks[-1]
    items = [ids[i.attrib['ObjectRef']] for i in top.iter('TrackItem') if 'ObjectRef' in i.attrib]
    ticks = 254016000000 * 1001 // 60000
    pos = 0
    cont = True
    frames = 0
    got = []
    sources = set()
    for it in items:
        ti = it.find('ClipTrackItem').find('TrackItem')
        start = int(ti.findtext('Start', '0'))
        end = int(ti.find('End').text)
        cont = cont and start == pos
        pos = end
        clip = ids[ids[it.find('ClipTrackItem').find('SubClip').attrib['ObjectRef']].find('Clip').attrib['ObjectRef']]
        cin = int(clip.find('Clip').findtext('InPoint'))
        cout = int(clip.find('Clip').findtext('OutPoint'))
        cont = cont and (cout - cin) == (end - start)
        frames += (end - start) // ticks
        got.append((start, end, cin, cout))
        sources.add(ids[clip.find('Clip').find('Source').attrib['ObjectRef']].tag)
    nseq = len([r for r in rows if r['capture'] != ''])
    what = 'prproj -nest' if nest else 'prproj'
    check(cont, '%s: the clips of the top track are end to end, as long as what they are of' % what)
    check(frames == nseq, '%s: the top track is as long as the sequence (%d frames)' % (what, nseq))
    check(len(tracks) == 1 + len(caps), '%s: a track for the best, and one for each capture' % what)
    if nest:
        others = [e for e in seqs if e is not seq]
        check(sources == {'VideoSequenceSource'}, 'prproj -nest: the clips are of sequences, not of the files (%s)' % ', '.join(sorted(sources)))
        check(len(others) == len(caps), 'prproj -nest: a sequence for each capture (%d)' % len(others))
        whole = True
        for o in others:
            vg = ids[o.find('TrackGroups').findall('TrackGroup')[0].find('Second').attrib['ObjectRef']]
            trs = [uids[t.attrib['ObjectURef']] for t in vg.find('TrackGroup').find('Tracks')]
            its = [ids[i.attrib['ObjectRef']] for t in trs for i in t.iter('TrackItem') if 'ObjectRef' in i.attrib]
            if len(its) != 1:
                whole = False
                continue
            c = ids[ids[its[0].find('ClipTrackItem').find('SubClip').attrib['ObjectRef']].find('Clip').attrib['ObjectRef']]
            whole = whole and ids[c.find('Clip').find('Source').attrib['ObjectRef']].tag == 'VideoMediaSource'
            whole = whole and int(c.find('Clip').findtext('InPoint')) == 0
        check(whole, 'prproj -nest: each sequence of a capture is one clip of all of the file')
    else:
        check(sources == {'VideoMediaSource'}, 'prproj: the clips are of the files')
    return got

def check_xml(path, rows, nest=False):
    root = ET.parse(path).getroot()
    tracks = root.find('sequence').find('media').find('video').findall('track')
    top = tracks[-1]
    pos = 0
    cont = True
    got = []
    for ci in top.findall('clipitem'):
        s, e = int(ci.findtext('start')), int(ci.findtext('end'))
        i, o = int(ci.findtext('in')), int(ci.findtext('out'))
        cont = cont and s == pos and (e - s) == (o - i)
        pos = e
        got.append((s, e, i, o))
    nseq = len([r for r in rows if r['capture'] != ''])
    what = 'xml -nest' if nest else 'xml'
    check(cont and pos == nseq, '%s: the clips of the top track are end to end, %d frames' % (what, pos))
    if nest:
        nested = [q for q in root.iter('sequence') if q.find('media') is not None and q.get('id') != 'sequence-1']
        refs = [ci.find('sequence') for ci in top.findall('clipitem')]
        check(all(r is not None for r in refs) and not top.findall('clipitem/file'), 'xml -nest: the clips of the top track are of sequences, not of the files')
        check(len(nested) == len(set(q.get('id') for q in nested)) and len(nested) >= 2, 'xml -nest: each sequence of a capture is defined once (%d)' % len(nested))
    files = [f for f in root.iter('file') if f.find('pathurl') is not None]
    check(all(f.findtext('pathurl').startswith('file://localhost/') for f in files) and len(files) >= 2, '%s: each file is defined, with its path' % what)
    return got

#------------------------------------------------------------------------

def main():
    keep = os.environ.get('MCMTEST_KEEP')   # a directory to keep the test files in, to look at
    with tempfile.TemporaryDirectory(prefix='mcmtest') as tmpdir:
        tmp = keep or tmpdir
        if keep:
            os.makedirs(keep, exist_ok=True)
        cache = os.path.join(tmp, 'cache')
        if keep and os.path.isdir(cache):
            for f in os.listdir(cache):
                os.unlink(os.path.join(cache, f))
        print('Making the tape')
        tape, audio = make_tape(tmp)
        n = len(tape)
        print('Making the captures')
        a = make_capture(tmp, 'a.mkv', tape, audio, [(0, 750)], drops={100, 101, 400}, damage={200, 201, 202, 203, 204, 205, 500},
                         lead=20, noise=2.0, seed=1)
        b = make_capture(tmp, 'b.mkv', tape, audio, [(30, 370), (470, n - 470)], drops={650}, damage={250, 600, 601, 602, 603},
                         bright=12, noise=5.0, size=(320, 240), seed=2)
        c = make_capture(tmp, 'c.mkv', tape, audio, [(300, n - 300)], damage={700}, lead=10, trail=15,
                         bright=-10, noise=8.0, seed=3)
        caps = [a, b, c]
        if keep:
            import json
            json.dump(caps, open(os.path.join(tmp, 'truth.json'), 'w'))
        paths = [x['path'] for x in caps]

        for by in ('video', 'audio', 'both'):
            print('-match %s' % by)
            log = os.path.join(tmp, 'log-%s.csv' % by)
            out = os.path.join(tmp, 'out-%s.prproj' % by)
            r = run_tool(['-cache', cache, '-match', by, '-log', log, '-o', out] + paths)
            check(r == 0, '-match %s: it ran' % by)
            if r != 0:
                continue
            rows = read_log(log)
            check_sequence(rows, caps, '-match %s' % by)
            if by == 'video':
                flat = check_prproj(out, rows, caps, 'out-video')
                # -nest: the same clips, of a sequence of each capture
                out = os.path.join(tmp, 'out-nest.prproj')
                r = run_tool(['-cache', cache, '-nest', '-o', out] + paths)
                check(r == 0, '-nest: it ran')
                if r == 0:
                    nested = check_prproj(out, rows, caps, 'out-nest', nest=True)
                    check(nested == flat, 'prproj -nest: the clips start, end, go in and out where they do without it')
                out = os.path.join(tmp, 'out-nest.xml')
                r = run_tool(['-cache', cache, '-nest', '-o', out] + paths)
                check(r == 0, '-nest, xml: it ran')
                if r == 0:
                    check_xml(out, rows, nest=True)

        print('Final Cut Pro XML, -ref 3, -path-map')
        log = os.path.join(tmp, 'log-xml.csv')
        out = os.path.join(tmp, 'out.xml')
        r = run_tool(['-cache', cache, '-ref', '3', '-path-map', tmp + '=D:\\caps', '-log', log, '-o', out] + paths)
        check(r == 0, 'xml: it ran')
        if r == 0:
            rows = read_log(log)
            check_sequence(rows, caps, '-ref 3')
            check_xml(out, rows)
            check('file://localhost/D%3a/caps/a.mkv' in open(out).read(), 'xml: -path-map makes the paths Windows paths')

        print('-match audio, a capture whose pictures do not match')
        # the same as a part of the tape, but its pictures a negative, which only the sound can match
        neg = make_capture(tmp, 'neg.mkv', tape, audio, [(200, 400)], lead=5, noise=1.0, seed=4)
        negv = os.path.join(tmp, 'negv.mkv')
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', neg['path'], '-vf', 'lutyuv=y=negval', '-c:v', 'ffv1', '-c:a', 'copy', negv], check=True)
        neg['path'] = negv
        for by, used in (('video', False), ('audio', True)):
            log = os.path.join(tmp, 'log-neg-%s.csv' % by)
            r = run_tool(['-cache', cache, '-match', by, '-ref', '1', '-log', log, '-o', os.path.join(tmp, 'neg-%s.xml' % by), paths[0], negv])
            # with nothing else to match, a capture that does not match is an error
            check((r == 0) == used, 'negative, -match %s: %s' % (by, 'it ran' if used else 'nothing matches'))
            if r == 0:
                rows = read_log(log)
                got = sum(1 for row in rows if row['capture2_frame'] != '')
                check((got >= 350) == used, 'negative, -match %s: %s (%d frames of it are in the timeline)' % (by, 'it is matched' if used else 'it is not matched', got))
                check_sequence(rows, [a, neg], 'negative, -match %s' % by)

        print('A capture of something else')
        other = os.path.join(tmp, 'other.mkv')
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'lavfi', '-i', 'testsrc=s=%dx%d:rate=%s:d=5' % (WIDTH, HEIGHT, RATE),
                        '-vf', 'hue=h=t*90', '-c:v', 'ffv1', other], check=True)
        log = os.path.join(tmp, 'log-other.csv')
        r = run_tool(['-cache', cache, '-tracks', 'best', '-log', log, '-o', os.path.join(tmp, 'other.prproj'), paths[0], other, paths[1]])
        check(r == 0, 'other: it ran')
        if r == 0:
            check_sequence(read_log(log), [a, {'truth': [], 'good': []}, b], 'other: left out')

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
