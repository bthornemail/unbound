// atomic.ts — the Atomic family as one invariant and five overlays (a draft template literal).
//
// The five components in TSX/src/templates/Atomic*.tsx share these values; each view adds
// only its own overlay. Run: node TS/atomic.ts — it checks the invariant and every overlay.
import assert from "node:assert";

// ── The invariant: identical in all five files ─────────────────
export const MASTER_PERIOD = 5040; // 7!
export const FANO_PERIOD = 7;
export const FANO_LINES = [[0, 1, 3], [0, 2, 5], [0, 4, 6], [1, 2, 4], [1, 5, 6], [2, 3, 6], [3, 4, 5]] as const;
export const CHANNELS = ["1-BIT HEX", "6-DOT BRAILLE", "8-DOT BRAILLE", "BRAILLE/HEX PAIR"] as const; // the shared head of each file's list
export const PLANES = { braille: [0x2800, 0x28ff], aegean: [0x10100, 0x1013f] } as const;

// ── The overlays: what each view adds (the mask between them) ──
export const VIEWS = {
  "Atomic ALU Surface":        { GOAL_CYCLE: 360 },
  "Atomic Incidence Monitor":  { GOAL_CYCLE: 360, SONAR_PERIOD: 60 },
  // SONAR_PERIOD 75 is intentional: the component's own comment reads "5 channels * 15 lanes".
  // It does not divide 5040; 5040 mod 75 = 15, one channel, so each master cycle the sonar starts
  // one channel later and all realigns after lcm(5040, 75) = 25200 = 5 x 5040. A precession, not
  // drift. (Briefly corrected to 72 on 2026-10-26, then restored: 72 would leave the fifth
  // channel 12 lanes wide.)
  "Atomic Incidence Surface":  { GOAL_CYCLE: 360, SONAR_PERIOD: 75, CHANNEL_WIDTH: 15 },
  "Atomic Tape Reader":        {},
  "Atomic Wallis Surface":     { GOAL_CYCLE: 360, SONAR_PERIOD: 60, SYNC_UNIT: 120 },
} as const;

// ── The template literal: one invariant, any overlay ──────────
export const atomic = (view: keyof typeof VIEWS) =>
  `${view}: period ${MASTER_PERIOD}, Fano ${FANO_LINES.map((l) => l.join("")).join(" ")}, ` +
  Object.entries(VIEWS[view]).map(([k, v]) => `${k} ${v}`).join(", ");

// ── Checks ─────────────────────────────────────────────────────
const pairs = [];
for (let a = 0; a < 7; a++) for (let b = a + 1; b < 7; b++) pairs.push([a, b]);
assert.ok(pairs.every(([a, b]) => FANO_LINES.filter((l) => l.includes(a as never) && l.includes(b as never)).length === 1),
  "every pair of points lies on exactly one line");
assert.equal(MASTER_PERIOD % FANO_PERIOD, 0, "the Fano period divides the master period");

const PERIODS = ["GOAL_CYCLE", "SONAR_PERIOD", "SYNC_UNIT"];
const broken: string[] = [];
for (const [view, overlay] of Object.entries(VIEWS))
  for (const [k, v] of Object.entries(overlay))
    if (PERIODS.includes(k) && MASTER_PERIOD % (v as number) !== 0) {
      const shift = MASTER_PERIOD % (v as number), width = (overlay as Record<string, number>).CHANNEL_WIDTH;
      const g = (a: number, b: number): number => (b ? g(b, a % b) : a);
      const lcm = (MASTER_PERIOD * (v as number)) / g(MASTER_PERIOD, v as number);
      broken.push(width && shift % width === 0
        ? `${view}: ${k} ${v} precesses ${shift / width} channel(s) per master cycle and realigns after ${lcm} = ${lcm / MASTER_PERIOD} x ${MASTER_PERIOD}`
        : `${view}: ${k} ${v} does not divide ${MASTER_PERIOD} and falls between channels`);
    }

for (const view of Object.keys(VIEWS)) console.log(atomic(view as keyof typeof VIEWS));
console.log(broken.length ? `\nperiods that do not close on the master clock:\n  ${broken.join("\n  ")}` : "\nevery period closes on the master clock");
