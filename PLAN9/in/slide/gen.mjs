// gen.mjs - on the host, with omi-dom-stack's own shared/clock-sliderule.js (node): its outputs over the
// whole of what it computes, as the port must give them. colors: the 24 of the codex. ticks: every tick of
// the 5040 rule, its phase, cycle and colour. cues: cue times to ticks. clock: a clock driven by steps and
// seeks. rule: a slide rule driven by slides and seeks. sched: cue sets (the four podcasts' WebVTT, and drawn
// sets) to their schedule and windows, under several seeds, rates and window sizes.
//	node gen.mjs <omi-dom-stack> <podcast dir> <out>
import { createRequire } from "module";
import { readFileSync, writeFileSync, mkdirSync, readdirSync } from "fs";
const [STACK, PODS, OUT] = process.argv.slice(2);
const M = createRequire(import.meta.url)(`${STACK}/shared/clock-sliderule.js`);
let seed = 7;
const rnd = (n) => { seed = (seed * 1103515245 + 12345) % 2147483648; return seed % n; };
const w = (f, s) => writeFileSync(`${OUT}/${f}`, s);

w("colors.expect", M.COLOR_CODEX_FULL.map((c) => `${c.index} ${c.hex} ${c.band} ${c.fano} ${c.name}\n`).join(""));
let t = "";
for (let k = 0; k < 5040; k++) { const cl = M.createClock({ start: k }); t += `${k} ${cl.phase} ${cl.cycle} ${cl.color.index} ${cl.color.hex}\n`; }
w("ticks.expect", t);

const times = [0, 0.5, 1, 2.5, 4.999, 5, 5.0208, 62.5, 299.99, 1e3, 3600.125];
for (let k = 0; k < 30; k++) times.push(rnd(400000) / 1000);
const cl = M.createClock();
w("cues.args", times.join(" ") + "\n");
w("cues.expect", times.map((s) => `${s} ${cl.cueToTick(s)} ${cl.cueToTick(s, 30)}\n`).join(""));

mkdirSync(`${OUT}/clock`, { recursive: true });
for (let c = 0; c < 20; c++) {
  const start = c % 4 === 0 ? 0 : rnd(100000);
  const ops = []; for (let k = 0; k < 6; k++) ops.push(rnd(3) ? `step=${rnd(800)}` : `seek=${rnd(20000) - 2000}`);
  const clock = M.createClock({ start });
  const out = ops.map((op) => { const [k, v] = op.split("="); if (k === "step") clock.step(Number(v)); else clock.seek(Number(v)); return `${op} ${clock.tick} ${clock.phase} ${clock.cycle} ${clock.color.index}\n`; });
  w(`clock/k${String(c).padStart(2, "0")}`, `start=${start} ${ops.join(" ")}\n`);
  w(`clock/k${String(c).padStart(2, "0")}.expect`, out.join(""));
}

mkdirSync(`${OUT}/rule`, { recursive: true });
for (let c = 0; c < 20; c++) {
  const length = [5040, 5040, 720, 240, 1000][c % 5], position = c % 3 ? rnd(length) : 0;
  const ops = []; for (let k = 0; k < 6; k++) ops.push(rnd(2) ? `slide=${rnd(12000) - 6000}` : `seek=${rnd(20000) - 10000}`);
  const rule = M.createSlideRule({ length, position });
  const out = ops.map((op) => { const [k, v] = op.split("="); if (k === "slide") rule.slide(Number(v)); else rule.seek(Number(v)); const m = rule.metrics(); return `${op} ${m.position} ${m.fanoGroup} ${m.tetraBlock} ${m.pastBoundary ? 1 : 0}\n`; });
  w(`rule/r${String(c).padStart(2, "0")}`, `length=${length} position=${position} ${ops.join(" ")}\n`);
  w(`rule/r${String(c).padStart(2, "0")}.expect`, out.join(""));
}

// cue sets: each podcast's own cues (id, start, end, text), and drawn sets
const vtt = (text) => {
  const out = []; let id = "", n = 0;
  const sec = (s) => { const p = s.split(":").map(Number); return p.length === 3 ? p[0] * 3600 + p[1] * 60 + p[2] : p[0] * 60 + p[1]; };
  for (const block of text.replace(/\r/g, "").split(/\n\n+/)) {
    const lines = block.split("\n"); const i = lines.findIndex((l) => l.includes("-->"));
    if (i < 0) continue;
    const [a, b] = lines[i].split("-->").map((s) => s.trim().split(" ")[0]);
    out.push({ id: i > 0 ? lines[0].trim() : `cue${n}`, start: sec(a), end: sec(b), text: lines.slice(i + 1).join(" ").replace(/\t/g, " ").trim() || "-" }); n++;
  }
  return out;
};
mkdirSync(`${OUT}/sched`, { recursive: true });
const sets = readdirSync(PODS).filter((f) => f.endsWith(".vtt")).sort().map((f) => vtt(readFileSync(`${PODS}/${f}`, "utf8")));
for (let k = 0; k < 6; k++) { const cs = []; let s = 0; for (let j = 0; j < 4 + rnd(20); j++) { s += rnd(9000) / 1000; cs.push({ id: `d${k}.${j}`, start: s, end: s + 1, text: `t${j}` }); } sets.push(cs); }
let c = 0;
for (const cues of sets) for (const [tps, windowSize, seedArr] of [[48, 240, [1, 2, 3, 4, 5, 6, 7, 8]], [30, 60, [9, 0, 2]], [24, 240, []]]) {
  const name = `s${String(c++).padStart(2, "0")}`;
  w(`sched/${name}.cues`, cues.map((q) => `${q.id}\t${q.start}\t${q.end}\t${q.text}\n`).join(""));
  w(`sched/${name}`, `tps=${tps} window=${windowSize} seed=${seedArr.join(",") || "-"}\n`);
  const res = M.scheduleFromCues(cues, { ticksPerSecond: tps, slideRule: { windowSize, seed: seedArr } });
  w(`sched/${name}.expect`, res.schedule.map((s, i) => { const wi = res.windows[i]; return `${s.id} ${s.tick} ${s.phase} ${s.color.index} ${wi.left} ${wi.fano} ${wi.tetra} ${Array.from(wi.window).join(",")}\n`; }).join("") + `metrics ${res.metrics.cueCount} ${res.metrics.clockPhase} ${res.metrics.rulePosition}\n`);
}
const st = M.selfTest();
w("selftest.expect", `${st.total} ${st.passed ? "passed" : "failed"}\n`);
console.log("colors 24, ticks 5040, cue times", times.length, ", clocks 20, rules 20, schedules", c, ", selfTest", st.total);
