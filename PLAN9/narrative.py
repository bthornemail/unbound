#!/usr/bin/env python3
# narrative.py - on the host: read light-garden's canon (NDJSON) into plain lines
# for zvtt.c, which writes the WebVTT inside Plan 9. Reading only; nothing decided here
# that the canon does not state. Out: in/narrative/VOICES and in/narrative/<series>.txt.
#   VOICES: TAG TAB hue                            (from the dome's lights; -1: no hue, the camera)
#   <series>.txt: id TAB tick-ms TAB next-tick-ms TAB voice TAB classes TAB text   (0x1F = newline)
import json, glob, os, sys
CANON = sys.argv[1] if len(sys.argv) > 1 else '/home/main/Programs/boxed/omi-files/light-garden'
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'in', 'narrative')

def tag(name):
    s = name.replace('ʿ', '').upper().replace('YY', 'Y')
    return '/'.join(p.split('-')[0] for p in s.split('/'))

def rows(path):
    return [json.loads(l) for l in open(path, encoding='utf-8') if l.strip()]

# Each agent's hue is its lights' on the dome (dome-leds.ndjson): an agent with an
# unsaturated (white) light has no hue, it is the camera (the Observer). Its number is
# not stated here: zvtt reads it from the camera, the hues' order as seen from it.
hues = {}
for x in rows(os.path.join(CANON, 'dome-leds.ndjson')):
    t = tag(x['fano_name'])
    if x['s'] == 0:
        hues[t] = -1
    else:
        hues.setdefault(t, x['h'])
with open(os.path.join(OUT, 'VOICES'), 'w') as v:
    for t, h in sorted(hues.items()):
        v.write(f"{t}\t{h}\n")

def clean(s):
    return '\x1f'.join(l for l in str(s).replace('\t', ' ').splitlines() if l.strip())

for f in sorted(glob.glob(os.path.join(CANON, 'canon-*.ndjson'))):
    base = os.path.basename(f)
    series = base[len('canon-'):-len('.ndjson')]
    ev = rows(f)
    out = []
    for i, x in enumerate(ev, 1):
        p = x.get('payload') or {}
        # the time is the tick (SPEC-07 I-79, I-84): one cue per event, in canon order;
        # the canon's t/timestamp is UTC and stays out
        if p.get('source') and 'source_line' in p:
            ident = f"{p['source']}#L{p['source_line']}"
        else:
            ident = f"{base}#L{i}"
        if 'seq' in x and 'payload' in x:          # speaker-for-the-untitled
            voice = 'SPEAKER'
        else:
            who = x.get('speaker') or (x.get('name') if x.get('event') in ('character', 'appearance', 'identity') else None)
            voice = tag(who) if who else ''
        text = p.get('text') or x.get('text') or x.get('quote') or x.get('title') or x.get('description') or x.get('subtitle') or x.get('name') or ''
        cls = '.'.join(dict.fromkeys(c for e in [x.get('event', ''), x.get('voice', ''), x.get('type', ''), p.get('kind', '')] for c in e.split('.') if c))
        out.append([ident, voice, cls, clean(text)])
    with open(os.path.join(OUT, series + '.txt'), 'w', encoding='utf-8') as o:
        for k, r in enumerate(out):
            o.write(f"{r[0]}\t{k*1000}\t{(k+1)*1000}\t{r[1]}\t{r[2]}\t{r[3]}\n")
    print(series, len(out))
