// agent-commons.js — the interactive entry for the project: a popup book you
// operate. Dependency-free (no CDN, no imports): it serves from `busybox httpd`
// offline and reconstructs anywhere. Each spread is a working toy over a law
// that is checked elsewhere by the BusyBox harness; the UI must not become a
// second, incompatible state engine. It shows shape, not authentication.

const WIDTH = 640;
const HEIGHT = 360;
const AUDIO_RATE = 48000;
const AUDIO_FRAMES = 2048;

function rotl16(value, amount) {
  return ((value << amount) | (value >>> (16 - amount))) & 0xffff;
}

// The browser projection of the law checked exhaustively by the BusyBox probe.
export function nextState(value) {
  return (rotl16(value, 1) ^ rotl16(value, 3) ^ rotl16(value, 14) ^ 0x1d1d) & 0xffff;
}

// --- the Fano law: "mark a XOR b for every marked pair" (one OR of three ANDs
// per point). Every 7-bit state converges in at most two steps to one of the 16
// XOR-closed constants. Checked cold in the proofs; projected here. ---
const LINES = [[1, 2, 3], [1, 4, 5], [1, 6, 7], [2, 4, 6], [2, 5, 7], [3, 4, 7], [3, 5, 6]];
const bit = (m, n) => (m >> (n - 1)) & 1;
export function fanoStep(mask) {
  let o = mask & 0x7f;
  for (const [a, b, c] of LINES) {
    o |= (bit(mask, a) & bit(mask, b)) << (c - 1);
    o |= (bit(mask, a) & bit(mask, c)) << (b - 1);
    o |= (bit(mask, b) & bit(mask, c)) << (a - 1);
  }
  return o;
}
function fanoConverge(mask) {
  let s = mask & 0x7f, steps = 0, trail = [s];
  while (steps < 6) { const t = fanoStep(s); if (t === s) break; s = t; trail.push(s); steps += 1; }
  return { mask: s, steps, trail };
}
const CONSTANTS = [0, 1, 2, 4, 8, 16, 32, 64, 7, 25, 97, 42, 82, 76, 52, 127];
const CONST_NAME = (m) => m === 0 ? 'empty' : m === 127 ? 'full'
  : [1, 2, 4, 8, 16, 32, 64].includes(m) ? 'point ' + (Math.log2(m) + 1) : 'line';

// The E8 root system on its Coxeter plane, read from the verified proof SVG:
// 8 rings of 30 roots at these radii and phase offsets (degrees). The root set
// is invariant under a 12 degree turn, so stepping 12 degrees is the real motion.
const E8_RINGS = [
  { r: 50.17, ph: 6.81 }, { r: 81.18, ph: 6.81 }, { r: 99.80, ph: 0.81 }, { r: 120.66, ph: 0.81 },
  { r: 148.33, ph: 6.81 }, { r: 161.48, ph: 0.81 }, { r: 195.23, ph: 0.81 }, { r: 240.00, ph: 6.81 },
];
const DEG = Math.PI / 180;

const bytesOf = (s) => [...s].map((c) => c.charCodeAt(0));
const hex = (n, w = 2) => '0x' + (n >>> 0).toString(16).padStart(w, '0');
const popcount = (n) => { let c = 0; while (n) { c += n & 1; n >>= 1; } return c; };
const rot13 = (s) => s.replace(/[a-z]/gi, (c) => String.fromCharCode((c <= 'Z' ? 90 : 122) >= (c = c.charCodeAt(0) + 13) ? c : c - 26));

// each agent's seed is its own literal, never a hash: the first two bytes of its tag
const speakerSeed = (name) => (((name.charCodeAt(0) & 0xff) << 8) | (name.charCodeAt(1) & 0xff || 0)) & 0xffff;
const COMMONS_TAGS = ['METATRON', 'SOLOMON', 'SOLON', 'ASABIYAH', 'ENOCH', 'SPEAKER', 'GENESIS', 'OBSERVER'];

const SPREADS = ['cover', 'state', 'commons', 'fano', 'tetra', 'switch', 'join'];

export class AgentCommons extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
    this.shadowRoot.innerHTML = `
      <style>
        :host { display:block; min-height:100vh; box-sizing:border-box; color:#d7fff5; background:#071013;
          font:14px/1.5 ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
          padding-left:max(18px, env(safe-area-inset-left,0)); padding-right:max(18px, env(safe-area-inset-right,0)); }
        main { max-width:720px; margin:auto; padding-block:clamp(20px,5vw,40px); min-height:100vh; display:flex; flex-direction:column; }
        .book { flex:1; display:flex; flex-direction:column; }
        .spread { display:none; flex-direction:column; gap:1rem; animation:fade .25s ease; }
        .spread.on { display:flex; }
        @media (prefers-reduced-motion: reduce){ .spread{animation:none} }
        @keyframes fade { from{opacity:.4} to{opacity:1} }
        h1 { color:#72ffe0; font-size:clamp(1.4rem,5vw,2rem); margin:0; letter-spacing:.02em; }
        h2 { color:#72ffe0; font-size:1.05rem; margin:0; text-transform:uppercase; letter-spacing:.14em; }
        p { margin:0; max-width:60ch; color:#aeded3; }
        .big { font-size:clamp(2.2rem,9vw,3.4rem); color:#72ffe0; line-height:1; }
        .dim { color:#6f9a90; }
        canvas { display:block; width:100%; max-width:${WIDTH}px; aspect-ratio:16/9; border:1px solid #23695d; background:#020607; transform-origin:center; }
        .stage { width:100%; max-width:${WIDTH}px; }
        .stage svg { display:block; width:100%; height:auto; border:1px solid #23695d; background:#020607; border-radius:4px; }
        button { padding:.55rem .8rem; color:#071013; background:#72ffe0; border:0; border-radius:4px; cursor:pointer; font:inherit; }
        button.ghost { color:#72ffe0; background:transparent; border:1px solid #23695d; }
        button:focus-visible { outline:2px solid #72ffe0; outline-offset:2px; }
        .row { display:flex; flex-wrap:wrap; gap:.6rem; align-items:center; }
        output, .read { display:block; color:#9bcfc4; white-space:pre-wrap; font-variant-numeric:tabular-nums; }
        input[type=range]{ width:100%; accent-color:#72ffe0; }
        label { color:#9bcfc4; font-size:.8rem; letter-spacing:.08em; text-transform:uppercase; }
        .grid16 { display:grid; grid-template-columns:repeat(8,1fr); gap:5px; max-width:360px; }
        .cell { aspect-ratio:1; display:flex; align-items:center; justify-content:center; border:1px solid #1d4a42; border-radius:3px; font-size:.7rem; color:#6f9a90; }
        .cell.on { background:#72ffe0; color:#071013; border-color:#72ffe0; font-weight:600; }
        .edges { display:flex; flex-direction:column; gap:4px; font-size:.85rem; }
        .edge { display:flex; justify-content:space-between; gap:1rem; padding:4px 8px; border:1px solid #1d4a42; border-radius:4px; }
        .edge.lit { border-color:#72ffe0; color:#d7fff5; }
        .edge .nc { color:#6f9a90; }
        .lit b { color:#72ffe0; }
        .payload { font-size:clamp(1.1rem,4vw,1.5rem); color:#d7fff5; word-break:break-all; }
        .frame { color:#6f9a90; }
        nav { display:flex; justify-content:space-between; align-items:center; margin-top:1.4rem; padding-top:1rem; border-top:1px solid #16342f; }
        .dots { display:flex; gap:7px; }
        .dot { width:8px; height:8px; border-radius:50%; background:#1d4a42; border:0; padding:0; cursor:pointer; }
        .dot.on { background:#72ffe0; }
        a { color:#72ffe0; }
        code { color:#72ffe0; }
      </style>
      <main>
        <div class="book">

          <section class="spread" data-s="cover">
            <h1>boxedPINE</h1>
            <p class="big">a popup book you operate</p>
            <p>Six spreads, each a working toy over one law. Nothing here asks your belief — every law is checked cold by the BusyBox harness beside it; these are the shapes, live. Turn the page.</p>
            <div class="row"><button data-open>Open &#9654;</button></div>
          </section>

          <section class="spread" data-s="state">
            <h2>The state commons</h2>
            <p>The E8 root system on its Coxeter plane — the <b>actual figure from the proof</b>, 240 roots (each <span style="color:#72ffe0">positive</span> paired with its <span style="color:#9bcfc4">negative reflection</span>) in 8 shells, turning one Coxeter step — 12° — at a time: 30 steps complete the turn, 15 reach −1. A constellation in frozen motion, un-frozen. Pause and resume with the button; the permutation law steps beside it.</p>
            <div class="stage" data-e8></div>
            <canvas width="${WIDTH}" height="${HEIGHT}" hidden></canvas>
            <div class="row">
              <button data-step>Advance state</button>
              <button class="ghost" data-play>Pause</button>
              <button class="ghost" data-render>Render offline audio</button>
            </div>
            <output data-readout>initializing…</output>
            <video muted playsinline hidden></video>
          </section>

          <section class="spread" data-s="commons">
            <h2>The Agent Commons</h2>
            <p>Eight sovereign narrators are the <b>base</b> — each at once a <b>sun</b> (it pulls) and a <b>planet</b> (it orbits), glowing with the <b>f-mean of the hues it holds</b>: the colour of its combination. Their centre of mass, the f-mean of the eight, is the global base; the 144 colour-name agents are the field it holds and sways. The constellation has a <b>point of view</b> — turn the observer and the colour order holds; flip to the other side and it reverses. Same stars, read the other way.</p>
            <canvas data-commons width="${WIDTH}" height="${HEIGHT}"></canvas>
            <div class="row">
              <button data-cstep>Step the commons</button>
              <button class="ghost" data-cwalk>Walk</button>
              <button class="ghost" data-pov>Observer ⟳</button>
              <button class="ghost" data-flip>Flip side</button>
            </div>
            <output data-cout></output>
          </section>

          <section class="spread" data-s="fano">
            <h2>The Fano converger</h2>
            <p>Pick any 7-bit byte. The law marks <code>a&nbsp;XOR&nbsp;b</code> for every lit pair, and in <b>at most two steps</b> it settles on one of 16 constants: the empty set, 7 points, 7 lines, the full set. Checked over all 128; watch one.</p>
            <label for="fbyte">input byte <span data-fval>22</span></label>
            <input type="range" id="fbyte" min="0" max="127" value="22">
            <output data-fout></output>
            <div class="grid16" data-grid></div>
          </section>

          <section class="spread" data-s="tetra">
            <h2>The tetrahedron</h2>
            <p>Four literals as vertices — WORD · GOD · OF · of — and the six edges between them, by raw bytes, with no padding. Only equal-length pairs are comparable. The one literal edge is a single bit.</p>
            <div class="edges" data-edges></div>
            <p class="dim">OF &#8596; of differ by exactly <code>0x20</code> on each letter — the case bit, lower&#8596;upper. The rest need the display layer to line up.</p>
          </section>

          <section class="spread" data-s="switch">
            <h2>The two switches</h2>
            <p>A literal read two ways. <b>Direction</b> reverses it; <b>encoding</b> rot13s the letters. Each switch is its own inverse — flip twice and you are back. The frame never changes, only the reading.</p>
            <div class="payload" data-pl></div>
            <div class="row">
              <button class="ghost" data-rev>Direction ⟲</button>
              <button class="ghost" data-enc>Encoding ⟲</button>
            </div>
            <p class="frame" data-sw></p>
          </section>

          <section class="spread" data-s="join">
            <h2>Open it</h2>
            <p>Free · forkable · asks no one's permission. The data is disposable; the value is that you can rebuild this from first principles and check every step.</p>
            <p class="read">serve it yourself — one binary, no install:
  <b>busybox httpd -f -p 8080 -h web</b>
then open  http://127.0.0.1:8080/

the laws are checked cold:
  <b>sh tests/run.sh</b></p>
            <p class="dim">FREEDOM · AUTONOMY · SOVEREIGNTY · RECIPROCITY — held through GRACE · LOVE · UNDERSTANDING · EMPATHY. And the question stays open: God is ___?</p>
          </section>

        </div>
        <nav>
          <button class="ghost" data-prev>&#9664; back</button>
          <div class="dots"></div>
          <button class="ghost" data-next>next &#9654;</button>
        </nav>
      </main>
    `;
    this.canvas = this.shadowRoot.querySelector('canvas');
    this.readout = this.shadowRoot.querySelector('[data-readout]');
    this.video = this.shadowRoot.querySelector('video');
    this.state = 1;
    this.frame = 0;
    this.index = 0;
    this.playing = null;
    this.sw = { rev: false, enc: false };
    this.swBase = 'The Light Garden';
    this.agents = COMMONS_TAGS.map((t) => ({ tag: t, state: speakerSeed(t) }));  // each its own local state
    this.cwalk = null;
    this.pov = 0;       // the observer's vantage (a turn keeps the colour order)
    this.flip = false;  // the other side (reverses the colour order)
    this.paused = false; // E8 animation intent; it runs only while its own spread is open
    this.offscreen = typeof OffscreenCanvas === 'function' ? new OffscreenCanvas(WIDTH, HEIGHT) : null;
    this.drawContext = this.offscreen ? this.offscreen.getContext('2d') : this.canvas.getContext('2d');
    this.visibleContext = this.canvas.getContext('2d');
  }

  connectedCallback() {
    const $ = (s) => this.shadowRoot.querySelector(s);
    const $$ = (s) => this.shadowRoot.querySelectorAll(s);

    // nav
    const dots = $('.dots');
    SPREADS.forEach((_, i) => {
      const d = document.createElement('button');
      d.className = 'dot'; d.setAttribute('aria-label', 'spread ' + (i + 1));
      d.addEventListener('click', () => this.goto(i));
      dots.appendChild(d);
    });
    $('[data-prev]').addEventListener('click', () => this.goto(this.index - 1));
    $('[data-next]').addEventListener('click', () => this.goto(this.index + 1));
    $('[data-open]').addEventListener('click', () => this.goto(1));

    // state commons
    $('[data-step]').addEventListener('click', () => this.advance());
    $('[data-play]').addEventListener('click', (e) => this.togglePlay(e.target));
    $('[data-render]').addEventListener('click', () => this.renderOfflineAudio());
    this.installTextTrack();

    // fano
    const fb = $('#fbyte');
    fb.addEventListener('input', () => this.drawFano(+fb.value));

    // tetra
    this.drawTetra();

    // switch
    $('[data-rev]').addEventListener('click', () => { this.sw.rev = !this.sw.rev; this.drawSwitch(); });
    $('[data-enc]').addEventListener('click', () => { this.sw.enc = !this.sw.enc; this.drawSwitch(); });

    // the commons: each agent its own local state on a shared plane
    this.commonsCtx = $('[data-commons]').getContext('2d');
    $('[data-cstep]').addEventListener('click', () => this.stepCommons());
    $('[data-cwalk]').addEventListener('click', (e) => this.walkCommons(e.target));
    $('[data-pov]').addEventListener('click', () => { this.pov = (this.pov + 1) % 8; this.drawCommons(); });
    $('[data-flip]').addEventListener('click', () => { this.flip = !this.flip; this.drawCommons(); });

    this.loadE8();
    this.drawFano(22);
    this.drawSwitch();
    this.drawCommons();
    this.goto(0);
  }

  updateReadout() {
    const p = this.state & 0x7f;
    this.readout.textContent = `frame ${this.frame} · state ${hex(this.state, 4)} · fano ${p} · hue ${this.state % 240}`;
  }

  async loadE8() {
    const stage = this.shadowRoot.querySelector('[data-e8]');
    try {
      const res = await fetch('./e8-step.svg'); if (!res.ok) throw 0;
      stage.innerHTML = await res.text();
      this.svgEl = stage.querySelector('svg');
      if (this.svgEl) { this.svgEl.removeAttribute('width'); this.svgEl.removeAttribute('height'); }
      this.canvas.hidden = true;
    } catch (e) {
      this.svgEl = null; this.canvas.hidden = false; this.draw();   // offline fallback: the canvas constellation
    }
    this.syncE8();
    this.updateReadout();
  }

  // the E8 SMIL animation runs only while its spread is open and not paused — never in the background
  syncE8() {
    if (!this.svgEl) return;
    try { (this.index === 1 && !this.paused) ? this.svgEl.unpauseAnimations() : this.svgEl.pauseAnimations(); } catch (e) {}
  }

  goto(i) {
    this.index = Math.max(0, Math.min(SPREADS.length - 1, i));
    this.shadowRoot.querySelectorAll('.spread').forEach((s, k) => s.classList.toggle('on', k === this.index));
    this.shadowRoot.querySelectorAll('.dot').forEach((d, k) => d.classList.toggle('on', k === this.index));
    if (this.index !== 2 && this.cwalk) { clearInterval(this.cwalk); this.cwalk = null; const b = this.shadowRoot.querySelector('[data-cwalk]'); if (b) b.textContent = 'Walk'; }
    this.syncE8();
  }

  // ---- state commons (preserved law + canvas) ----
  installTextTrack() {
    if (!this.video.textTracks || typeof VTTCue !== 'function') return;
    this.track = this.video.addTextTrack('metadata', 'state cues', 'en');
    this.track.mode = 'hidden';
  }
  draw() {
    const ctx = this.drawContext;
    const phase = this.state & 0x7f, hue = this.state % 240;
    ctx.fillStyle = '#020607'; ctx.fillRect(0, 0, WIDTH, HEIGHT);
    // the exact E8 Coxeter constellation, turned 12 degrees per step (the real motion)
    const cx = WIDTH / 2, cy = HEIGHT / 2, scale = (Math.min(WIDTH, HEIGHT) / 2 - 16) / 240;
    for (const { r } of E8_RINGS) {                    // the orbit shells
      ctx.strokeStyle = 'rgba(114,255,224,0.10)'; ctx.lineWidth = 1;
      ctx.beginPath(); ctx.arc(cx, cy, r * scale, 0, Math.PI * 2); ctx.stroke();
    }
    E8_RINGS.forEach(({ r, ph }, i) => {               // the 30 roots on each shell
      const R = r * scale;
      for (let k = 0; k < 30; k++) {
        const a = (ph + k * 12) * DEG;
        ctx.fillStyle = `hsl(${(hue + i * 9) % 360} 85% ${54 + i * 3}%)`;
        ctx.beginPath(); ctx.arc(cx + R * Math.cos(a), cy + R * Math.sin(a), 2.4, 0, Math.PI * 2); ctx.fill();
      }
    });
    ctx.fillStyle = '#72ffe0'; ctx.font = '15px ui-monospace, monospace';
    ctx.fillText(`state ${hex(this.state, 4)} · fano ${phase}`, 20, 28);
    ctx.fillStyle = '#6f9a90'; ctx.font = '12px ui-monospace, monospace';
    ctx.fillText('E8 · 240 roots · 8 rings of 30 · exact Coxeter radii · 12° per step', 20, HEIGHT - 16);
    if (this.offscreen) { this.visibleContext.clearRect(0, 0, WIDTH, HEIGHT); this.visibleContext.drawImage(this.offscreen, 0, 0); }
    // the Coxeter turn as a CSSOM matrix (DOMMatrix) on the canvas element
    const mtx = typeof DOMMatrix === 'function' ? new DOMMatrix().rotate((this.frame * 12) % 360) : null;
    this.canvas.style.transform = mtx
      ? `matrix(${mtx.a}, ${mtx.b}, ${mtx.c}, ${mtx.d}, ${mtx.e}, ${mtx.f})`
      : `rotate(${(this.frame * 12) % 360}deg)`;
    this.readout.textContent = `frame ${this.frame} · state ${hex(this.state, 4)} · fano ${phase} · hue ${hue}`;
  }
  advance() { this.state = nextState(this.state); this.frame += 1; if (!this.svgEl) this.draw(); this.updateReadout(); }
  togglePlay(btn) {
    if (this.svgEl) {
      this.paused = !this.paused;
      this.syncE8();
      btn.textContent = this.paused ? 'Play' : 'Pause';
      return;
    }
    if (this.playing) { clearInterval(this.playing); this.playing = null; btn.textContent = 'Resume'; }
    else { this.playing = setInterval(() => this.advance(), 120); btn.textContent = 'Pause'; }
  }
  async renderOfflineAudio() {
    if (typeof OfflineAudioContext !== 'function') { this.readout.textContent = 'OfflineAudioContext is unavailable here.'; return; }
    const context = new OfflineAudioContext(1, AUDIO_FRAMES, AUDIO_RATE);
    const buffer = context.createBuffer(1, AUDIO_FRAMES, AUDIO_RATE);
    const samples = buffer.getChannelData(0);
    let state = this.state;
    for (let i = 0; i < samples.length; i += 1) { state = nextState(state); samples[i] = (state / 32767.5) - 1; }
    const src = context.createBufferSource(); src.buffer = buffer; src.connect(context.destination); src.start();
    const rendered = await context.startRendering();
    this.readout.textContent = `offline audio rendered: ${rendered.length} frames · final ${hex(state, 4)}`;
  }

  // ---- fano converger ----
  drawFano(v) {
    this.shadowRoot.querySelector('[data-fval]').textContent = v;
    const { mask, steps, trail } = fanoConverge(v);
    const isC = CONSTANTS.includes(mask);
    this.shadowRoot.querySelector('[data-fout]').textContent =
      `${v} (${v.toString(2).padStart(7, '0')})  →  ` + trail.map((m) => m.toString(2).padStart(7, '0')).join('  →  ') +
      `\nsettled in ${steps} step${steps === 1 ? '' : 's'} on ${hex(mask)} — ${isC ? CONST_NAME(mask) + ' (a constant)' : 'not a constant'}`;
    const grid = this.shadowRoot.querySelector('[data-grid]');
    grid.innerHTML = '';
    for (let i = 0; i < 16; i++) {
      const m = CONSTANTS[i];
      const cell = document.createElement('div');
      cell.className = 'cell' + (m === mask ? ' on' : '');
      cell.textContent = i;
      cell.title = hex(m) + ' — ' + CONST_NAME(m);
      grid.appendChild(cell);
    }
  }

  // ---- tetrahedron ----
  drawTetra() {
    const V = { WORD: 'WORD', GOD: 'GOD', OF: 'OF', of: 'of' };
    const names = Object.keys(V);
    const box = this.shadowRoot.querySelector('[data-edges]');
    for (let i = 0; i < names.length; i++) for (let j = i + 1; j < names.length; j++) {
      const A = V[names[i]], B = V[names[j]];
      const row = document.createElement('div'); row.className = 'edge';
      if (A.length !== B.length) {
        row.innerHTML = `<span>${names[i]} &#8596; ${names[j]}</span><span class="nc">lengths ${A.length}/${B.length} — not comparable</span>`;
      } else {
        const mask = bytesOf(A).map((x, k) => x ^ bytesOf(B)[k]);
        const pop = mask.reduce((s, m) => s + popcount(m), 0);
        row.className = 'edge lit';
        row.innerHTML = `<span>${names[i]} &#8596; ${names[j]}</span><span><b>${mask.map((x) => hex(x)).join(' ')}</b> · ${pop} bits</span>`;
      }
      box.appendChild(row);
    }
  }

  // ---- the two switches ----
  drawSwitch() {
    let s = this.swBase;
    if (this.sw.enc) s = rot13(s);
    if (this.sw.rev) s = [...s].reverse().join('');
    this.shadowRoot.querySelector('[data-pl]').textContent = s;
    this.shadowRoot.querySelector('[data-sw]').textContent =
      `direction: ${this.sw.rev ? 'reversed' : 'forward'} · encoding: ${this.sw.enc ? 'rot13' : 'plain'} — the literal underneath is unchanged`;
  }

  // ---- the Agent Commons: sovereign agents, each its own local state, one shared plane ----
  drawCommons() {
    const ctx = this.commonsCtx, cx = WIDTH / 2, cy = HEIGHT / 2;
    ctx.fillStyle = '#020607'; ctx.fillRect(0, 0, WIDTH, HEIGHT);
    // the observer's point of view: a turn (pov) keeps the colour order; the other side (flip) reverses it
    const rot = this.pov * Math.PI / 4, mir = this.flip ? -1 : 1;
    const view = (x, y) => { const dx = (x - cx) * mir, dy = y - cy; return [cx + dx * Math.cos(rot) - dy * Math.sin(rot), cy + dx * Math.sin(rot) + dy * Math.cos(rot)]; };
    // the 8 base agents — each a sun (it pulls) and a planet (it orbits): position from its local state
    const bodies = this.agents.map((ag, i) => {
      const rad = 46 + i * 15, ang = (ag.state / 65536) * Math.PI * 2 + i * (Math.PI * 2 / 8);
      return { x: cx + rad * Math.cos(ang), y: cy + rad * Math.sin(ang) };
    });
    const bx = bodies.reduce((s, b) => s + b.x, 0) / 8, by = bodies.reduce((s, b) => s + b.y, 0) / 8;  // f-mean = barycentre = global base
    // the 144 colour-name field, swayed by the eight's gravity; each sun accumulates the circular mean (f-mean) of the hues it pulls
    const g = Math.min(WIDTH, HEIGHT) - 28, ox = (WIDTH - g) / 2, oy = (HEIGHT - g) / 2, cs = g / 11;
    const acc = bodies.map(() => ({ sx: 0, sy: 0 }));
    for (let r = 0; r < 12; r++) for (let c = 0; c < 12; c++) {
      const px = ox + c * cs, py = oy + r * cs, hue = (r * 12 + c) / 144 * 360, hr = hue * Math.PI / 180;
      let dx = 0, dy = 0;
      bodies.forEach((b, j) => { const ex = b.x - px, ey = b.y - py, k = 520 / (ex * ex + ey * ey + 900); dx += ex * k; dy += ey * k; acc[j].sx += k * Math.cos(hr); acc[j].sy += k * Math.sin(hr); });
      const [vx, vy] = view(px + dx, py + dy);
      ctx.fillStyle = `hsl(${Math.round(hue)} 60% 55%)`;
      ctx.beginPath(); ctx.arc(vx, vy, 2.6, 0, Math.PI * 2); ctx.fill();
    }
    const sunHue = (j) => ((Math.atan2(acc[j].sy, acc[j].sx) * 180 / Math.PI) + 360) % 360;
    bodies.forEach((b, j) => {
      const [vx, vy] = view(b.x, b.y), h = Math.round(sunHue(j));
      ctx.fillStyle = `hsla(${h}, 90%, 60%, 0.22)`; ctx.beginPath(); ctx.arc(vx, vy, 13, 0, Math.PI * 2); ctx.fill();
      ctx.fillStyle = `hsl(${h} 90% 64%)`; ctx.beginPath(); ctx.arc(vx, vy, 5, 0, Math.PI * 2); ctx.fill();
    });
    const [vbx, vby] = view(bx, by);
    ctx.strokeStyle = '#72ffe0'; ctx.lineWidth = 1.5; ctx.beginPath(); ctx.arc(vbx, vby, 7, 0, Math.PI * 2); ctx.stroke();
    // the colour order read clockwise from this vantage
    const order = bodies.map((b, j) => { const [vx, vy] = view(b.x, b.y); return { a: Math.atan2(vy - vby, vx - vbx), h: Math.round(sunHue(j)) }; })
      .sort((p, q) => p.a - q.a).map((o) => o.h);
    ctx.fillStyle = '#9bcfc4'; ctx.font = '11px ui-monospace, monospace';
    ctx.fillText(`observer ${this.pov}${this.flip ? ' · flipped' : ''}  ·  ○ = f-mean (global base)`, 14, HEIGHT - 12);
    this.shadowRoot.querySelector('[data-cout]').textContent = `colour order from here: ${order.map((h) => h + '°').join(' ')}`;
  }
  stepCommons() { for (const ag of this.agents) ag.state = nextState(ag.state); this.drawCommons(); }
  walkCommons(btn) {
    if (this.cwalk) { clearInterval(this.cwalk); this.cwalk = null; btn.textContent = 'Walk'; }
    else { this.cwalk = setInterval(() => this.stepCommons(), 140); btn.textContent = 'Stop'; }
  }
}

customElements.define('agent-commons', AgentCommons);
