#!/usr/bin/env python3
"""gen.py - on the host: the algorithmic clock's golden. replay and sweep from atomic-kernel's kernel.py
itself (imported); spins and blocks from dev-docs/CyclicClock.v's definitions, transcribed (is_prime,
mult_order_10, cyclic_number, encode_step, modinv by trying, recover_step with k = 0 read as q), there
being no Coq extraction of them. I-248.
	python3 gen.py <omi-files/atomic-kernel> <out>"""
import sys
sys.path.insert(0, sys.argv[1])
import kernel
OUT = sys.argv[2]

SEEDS = [0x0001, 0x0000, 0xFFFF, 0x4242, 0xAA55, 0x55AA, 0x1D1D, 0x8000,
         0x14BE, 0x278D, 0x41EB, 0x72D8, 0x8D27, 0xBE14, 0xD872, 0xEB41]   # the last eight: 0x1D1D's fixed points (I-248)
with open(f"{OUT}/seeds", "w") as fh:
    for s in SEEDS:
        fh.write(f"0x{s:04X}\n")
        with open(f"{OUT}/replay.0x{s:04X}", "w") as r:
            for d in kernel.replay(s, 40):
                r.write(f"{d['t']} {d['state_hex']} {d['position']} {d['orbit']} {d['offset']} {d['phase']} {d['diff']} {d['band'][0]} {d['band'][1]} {d['band'][2]}\n")

n8 = n16 = 0
for x in range(65536):
    s = x
    for t in range(16):
        s = kernel.tick(s, t)
        if t == 7 and s == x: n8 += 1
    if s == x: n16 += 1
open(f"{OUT}/sweep", "w").write(f"8 {n8}\n16 {n16}\n")

def is_prime(n): return n >= 2 and all(n % k for k in range(2, n))
def mult_order_10(q):
    k = 1
    while pow(10, k, q) != 1: k += 1
    return k
def cyclic_number(q, T): return (10 ** T - 1) // q
def encode_step(q, T, k): return (cyclic_number(q, T) * k) % (10 ** T - 1)
def modinv(a, q):
    for fuel in range(q - 1, 0, -1):
        if (a * (q - fuel)) % q == 1: return q - fuel
    return None
def recover_step(q, T, B):
    inv = modinv(cyclic_number(q, T) % q, q)
    if inv is None: return None
    k = (B % q * inv) % q
    return q if k == 0 else k
with open(f"{OUT}/spins", "w") as fh:
    for q in range(3, 1000):
        if q == 5 or not is_prime(q): continue
        T = mult_order_10(q)
        fh.write(f"{q} {T} {sum(recover_step(q, T, encode_step(q, T, k)) == k for k in range(1, T + 1))}\n")
with open(f"{OUT}/blocks", "w") as fh:
    for q in [7, 11, 13, 17, 19, 23, 31, 37, 41, 73]:
        T = mult_order_10(q)
        fh.write(f"{q} {str(cyclic_number(q, T)).zfill(T)}\n")
