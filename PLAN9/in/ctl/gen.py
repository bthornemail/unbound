#!/usr/bin/env python3
"""gen.py - on the host, with the atomic kernel's own code (~/devops/genesis-frame/atomic-kernel,
control_plane.py, which follows its docs/CONTROL_PLANE_SPEC.md): write control streams, the events
its parse_control_stream reads from them, the streams it refuses with the code it refuses them by,
and COBS frames from its cobs_encode, with the frames its cobs_decode refuses.
	python3 gen.py <atomic-kernel> <out>
A stream line is its tick, then its bytes in hex, each followed by a space."""
import random, sys

sys.path.insert(0, sys.argv[1])
from control_plane import B, T, ControlPlaneError, cobs_decode, cobs_encode, encode_control, parse_control_stream

out = sys.argv[2]
rnd = random.Random(0x1D1D)
CH = ["FS", "GS", "RS", "US"]
hexs = lambda bs: "".join(f"{b:02X} " for b in bs)


def codepoint():
    while True:
        cp = rnd.choice([rnd.randrange(0x80), rnd.randrange(0x80, 0x800), rnd.randrange(0x800, 0x10000), rnd.randrange(0x10000, 0x110000)])
        if not 0xD800 <= cp <= 0xDFFF:
            return cp


def stream(tick):
    s = bytearray()
    for _ in range(rnd.randrange(1, 9)):
        if rnd.randrange(3) == 0:
            s.append(rnd.choice([b for b in range(256) if b not in (0x1C, 0x1D, 0x1E, 0x1F)]))
            continue
        ctx = rnd.randrange(4)
        ext = None
        if ctx == 2:
            ext = codepoint()
        elif ctx == 3:
            ext = ((B[tick % T] & 0x0F) << 4) | rnd.randrange(16)
        s += encode_control(rnd.choice(CH), rnd.randrange(16), ctx, ext)
    return bytes(s)


def line(e):
    p = e.payload
    if e.payload_kind == "data":
        return f"data {p:02X}"
    w = f"{e.payload_kind} {p['channel_name']} lane {e.lane} mask {p['mask']:02X}"
    if e.payload_kind == "numsys":
        w += f" {p['numsys']} {p['lane_within_ctx']}"
    elif e.payload_kind == "unicode":
        w += f" U+{p['codepoint']:04X} {p['utf8']}"
    elif e.payload_kind == "extended":
        w += f" {p['algorithm_byte']:02X} {p['algorithm_selector']:X} {p['algorithm_param']:X}"
    return w


streams, events = [], []
# the kernel's own reference stream at its reference tick (tests/test_all.py, artifact.py REFERENCE_TICK = 4)
cases = [(4, bytes.fromhex("1C0348656C6C6F1D73DEAD1E3BE288821F174A2F1D0F9A"))]
cases += [(t, stream(t)) for t in (rnd.randrange(16) for _ in range(300))]
for tick, s in cases:
    streams.append(f"{tick} {hexs(s)}")
    events.append(f"stream {tick} {hexs(s)}".rstrip())
    events += [line(e) for e in parse_control_stream(s, tick)]
open(f"{out}/streams", "w").write("\n".join(streams) + "\n")
open(f"{out}/events", "w").write("\n".join(events) + "\n")
# the falsifier: the same bytes one tick later; B[t] and B[t+1] always differ, so each stream with
# an algorithm byte (CT=11) is refused there
open(f"{out}/streams.next", "w").write("\n".join(f"{t + 1} {hexs(s)}" for t, s in cases) + "\n")

# must-reject: the kernel's own three (tests/test_all.py), then a valid stream with one thing broken
rejects = [(4, bytes.fromhex("1C00")), (4, bytes.fromhex("1E3BFF")), (0, bytes.fromhex("1D0F9A"))]
breaks = [
    lambda t: bytes([0x1F]),                                  # a channel byte, no mask
    lambda t: bytes([0x1D, 0x31]),                            # a mask not ending in binary 11
    lambda t: bytes([0x1E, 0x0B]),                            # CT=10, no codepoint
    lambda t: bytes([0x1E, 0x0B, 0xE2, 0x88]),                # CT=10, a codepoint cut short
    lambda t: bytes([0x1E, 0x0B, 0xED, 0xA0, 0x80]),          # CT=10, a surrogate
    lambda t: bytes([0x1E, 0x0B, 0xC0, 0x80]),                # CT=10, an overlong lead
    lambda t: bytes([0x1E, 0x0B, 0xF5, 0x80, 0x80, 0x80]),    # CT=10, past U+10FFFF
    lambda t: bytes([0x1C, 0x0F]),                            # CT=11, no algorithm byte
    lambda t: bytes([0x1C, 0x0F, (((B[t % T] & 0x0F) ^ 1) << 4) | 5]),  # CT=11, the wrong selector for the tick
]
for i, f in enumerate(breaks * 3):
    t = rnd.randrange(16)
    rejects.append((t, stream(t)[:0] + f(t)))
rej, codes = [], []
for t, s in rejects:
    try:
        parse_control_stream(s, t)
        raise SystemExit(f"the kernel accepts {s.hex()} at tick {t}")
    except ControlPlaneError as e:
        rej.append(f"{t} {hexs(s)}")
        codes.append(f"{t} {hexs(s)}error {e.code}")
open(f"{out}/reject", "w").write("\n".join(rej) + "\n")
open(f"{out}/reject.codes", "w").write("\n".join(codes) + "\n")

# COBS: payloads with zeros and runs past 254, and frames the decoder refuses
pay, wire = [], []
for n in [0, 1, 5, 253, 254, 255, 300, 600] + [rnd.randrange(40) for _ in range(60)]:
    p = bytes(rnd.choice([0, rnd.randrange(1, 256)]) if rnd.randrange(2) else rnd.randrange(1, 256) for _ in range(n))
    if n >= 253:
        p = bytes(rnd.randrange(1, 256) for _ in range(n))
    w = cobs_encode(p)
    assert cobs_decode(w) == p
    pay.append(hexs(p) or "-")
    wire.append(hexs(w))
open(f"{out}/cobs.payload", "w").write("\n".join(pay) + "\n")
open(f"{out}/cobs.wire", "w").write("\n".join(wire) + "\n")
bad = []
for w in [b"", b"\x03\x11\x00", b"\x05\x11\x22", b"\x00", b"\x02\x11\x04\x22"]:
    try:
        cobs_decode(w)
        raise SystemExit(f"cobs_decode accepts {w.hex()}")
    except ControlPlaneError as e:
        bad.append(f"{hexs(w) or '- '}error {e.code}")
open(f"{out}/cobs.reject", "w").write("\n".join(bad) + "\n")
print(f"{len(cases)} streams, {sum(1 for x in events if not x.startswith('stream'))} events, {len(rej)} must-reject, {len(pay)} COBS frames, {len(bad)} COBS refusals")
