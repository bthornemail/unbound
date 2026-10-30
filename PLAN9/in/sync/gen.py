#!/usr/bin/env python3
"""gen.py - on the host, with metaverse-build's own sync tools: runtime/sync-transport/receive.sh (fifo
mode: a real fifo, its sender writing the envelopes into it), runtime/sync-world/merge.sh and
apply-merged.sh (each event by runtime/world/apply-event.py). Each case: the world golden's events
split among peers as envelopes {"peer","seq","event"}, put through chaos (dropped, duplicated,
shuffled, by a seeded random, as chaos.sh does) and sometimes a blank line or a last line without its
newline, sent to one or two receivers (logs/NAME.jsonl), merged, and applied to the base twice, as the
tests apply it for two peers. Kept: what each receiver logged, the merged log and merge's stdout,
apply's exit, its stderr line, the snapshot and the halts log; where apply prints the snapshot's
sha256, the snapshot itself. No two different envelopes share a peer and seq, so the order in which
merge.sh lists the logs (os.listdir's) does not show.
	python3 gen.py <metaverse-build> <world golden dir> <out>"""
import json, os, random, shutil, subprocess, sys

MB, WORLD, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
n = [0]

def send(w, log, data):
    fifo = f"{w}/bus.fifo"
    os.mkfifo(fifo)
    r = subprocess.Popen(["bash", f"{MB}/runtime/sync-transport/receive.sh", "fifo", fifo, f"{w}/{log}"])
    s = subprocess.Popen(["bash", f"{MB}/runtime/sync-transport/send.sh", "fifo", fifo], stdin=subprocess.PIPE)
    s.communicate(data.encode()); r.wait(); os.remove(fifo)

def case(base, sends, k):
    d = f"{OUT}/y{n[0]:03d}"; n[0] += 1
    os.makedirs(f"{d}/expect", exist_ok=True)
    open(f"{d}/base.json", "w").write(base)
    w = f"{d}/.run"; shutil.rmtree(w, ignore_errors=True); os.makedirs(f"{w}/logs")
    shutil.copy(f"{d}/base.json", w)
    for i, (log, data) in enumerate(sends):
        open(f"{d}/send.{i}", "w").write(data)
        open(f"{d}/send.{i}.log", "w").write(log + "\n")
        send(w, log, data)
        shutil.copy(f"{w}/{log}", f"{d}/expect/send.{i}.received") if os.path.exists(f"{w}/{log}") else None
    p = subprocess.run(["bash", f"{MB}/runtime/sync-world/merge.sh", f"{w}/logs", f"{w}/merged.jsonl"], capture_output=True, text=True)
    assert p.returncode == 0, p.stderr
    open(f"{d}/expect/merge.stdout", "w").write(p.stdout)
    shutil.copy(f"{w}/merged.jsonl", f"{d}/expect/merged.jsonl")
    for peer in "ab":
        p = subprocess.run(["bash", f"{MB}/runtime/sync-world/apply-merged.sh", "base.json", "merged.jsonl", f"snap.{peer}.json", f"halts.{peer}.log"], cwd=w, capture_output=True, text=True)
        open(f"{d}/expect/status", "w").write(f"{p.returncode}\n")
        open(f"{d}/expect/stderr", "w").write((p.stderr.splitlines() or [""])[0] + "\n")
        out = open(f"{w}/snap.{peer}.json").read() + "\n" if p.returncode == 0 else p.stdout
        open(f"{d}/expect/stdout", "w").write(out)
        for f in [f"snap.{peer}.json", f"halts.{peer}.log"]:
            if os.path.exists(f"{w}/{f}"): shutil.copy(f"{w}/{f}", f"{d}/expect/{f}")
    if p.returncode == 0:
        assert open(f"{w}/snap.a.json").read() == open(f"{w}/snap.b.json").read(), "two peers, one result"
    shutil.rmtree(w)

cases = [l.split("|") for l in open(f"{WORLD}/cases").read().splitlines()]
snap = lambda c: open(f"{WORLD}/{c[1]}").read()
evs = lambda c: [l for l in open(f"{WORLD}/{c[0]}.events").read().splitlines() if l.strip()]
PEERS = ["peer-A", "peer-B", "peer-C"]
dumps = lambda e: json.dumps(e, separators=(",", ":"))

for i, c in enumerate(cases[:60]):
    rnd = random.Random(29 + i)
    k = 1 + i % 3
    seqs = {p: 0 for p in PEERS}
    envs = []
    for j, line in enumerate(evs(c)):
        p = PEERS[(j if i % 4 else 0) % k]; seqs[p] += 1
        envs.append({"peer": p, "seq": seqs[p], "event": json.loads(line)})
    items = list(envs)
    if i % 5 != 0:   # chaos
        items = [e for e in items if rnd.random() > 0.15]
        items += [e for e in items if rnd.random() < 0.2]
        rnd.shuffle(items)
    lines = [dumps(e) for e in items]
    if i % 7 == 3: lines.insert(len(lines) // 2, "")   # a blank line on the wire
    text = "".join(l + "\n" for l in lines)
    if i % 6 == 4 and text: text = text[:-1]   # the last line without its newline
    if i % 3 == 2 and len(lines) > 1:   # two receivers
        h = len(lines) // 2
        case(snap(c), [("logs/east.jsonl", "".join(l + "\n" for l in lines[:h])), ("logs/west.jsonl", "".join(l + "\n" for l in lines[h:]))], k)
    else:
        case(snap(c), [("logs/received.jsonl", text)], k)
# an envelope without its event: apply-event refuses {} and apply stops there
case(snap(cases[0]), [("logs/received.jsonl", dumps({"peer": "peer-A", "seq": 1, "event": json.loads(evs(cases[0])[0])}) + "\n" + dumps({"peer": "peer-A", "seq": 2}) + "\n")], 1)

st = [open(f"{OUT}/{d}/expect/status").read().strip() for d in sorted(os.listdir(OUT)) if d.startswith("y")]
h = sum(1 for d in sorted(os.listdir(OUT)) if d.startswith("y") and os.path.exists(f"{OUT}/{d}/expect/halts.a.log") and os.path.getsize(f"{OUT}/{d}/expect/halts.a.log"))
print(n[0], "cases; apply status:", {s: st.count(s) for s in sorted(set(st))}, "; with halts:", h, "; stderr:", sorted({open(f"{OUT}/{d}/expect/stderr").read().strip() for d in sorted(os.listdir(OUT)) if d.startswith("y")}))
