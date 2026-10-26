import React, { useState, useEffect, useMemo } from 'react';
import { Cpu, Compass, Zap, ArrowLeftRight, GitBranch } from 'lucide-react';

/**
 * ATOMIC ALU SURFACE (Fast Wallis)
 * - 4-Channel Sonar (τ₁₅ x 4 = 60 slots)
 * - BOM Stream Orientation: 
 * U+FEFF = Forward / Two's Complement
 * U+FFFE = Inverted / One's Complement
 * - Arithmetic: Barrett Reduction (Reciprocal Multiply + Shift)
 */

const MASTER_PERIOD = 5040;
const GOAL_CYCLE = 360;

// --- FAST HARDWARE DIVISION (Barrett Reduction) ---
// Using precomputed reciprocals to replace N / D with (N * M) >> shift
// This mirrors ASIC/FPGA integer logic perfectly.

const barrettDivMod = (n_val, m_val, shift_val, divisor_val) => {
  const n = BigInt(n_val);
  const m = BigInt(m_val);
  const shift = BigInt(shift_val);
  const divisor = BigInt(divisor_val);
  
  // Q = (N * M) >> shift
  const q = (n * m) >> shift;
  // R = N - (Q * D)
  const r = n - (q * divisor);
  
  return { q: Number(q), r: Number(r) };
};

// Precomputed Constants for Reciprocal Division:
// For D=60: M = ceil(2^36 / 60) = 1145324613, Shift = 36
const divMod60 = (n) => barrettDivMod(n, 1145324613, 36, 60);

// For D=15: M = ceil(2^32 / 15) = 286331154, Shift = 32
const divMod15 = (n) => barrettDivMod(n, 286331154, 32, 15);

// For D=7: M = ceil(2^32 / 7) = 613566757, Shift = 32
const divMod7  = (n) => barrettDivMod(n, 613566757, 32, 7);

// For D=3600 (Double Primes): M = ceil(2^44 / 3600) = 4887363600, Shift = 44
const divMod3600 = (n) => barrettDivMod(n, 4887363600, 44, 3600);

// --- Symbolic Generators ---
const FANO_LINES = [
  [0, 1, 3], [0, 2, 5], [0, 4, 6],
  [1, 2, 4], [1, 5, 6],
  [2, 3, 6],
  [3, 4, 5]
];

const getAegean = (n) => {
  const units = [0x10107, 0x10108, 0x10109, 0x1010A, 0x1010B, 0x1010C, 0x1010D, 0x1010E, 0x1010F];
  const index = Math.min(8, Math.max(0, (n % 10) - 1));
  return String.fromCodePoint(units[index] || 0x10107);
};

const getBraille8 = (n) => String.fromCharCode(0x2800 + (n % 256));
const getHexagram = (n) => String.fromCodePoint(0x4DC0 + (n % 64));

const App = () => {
  const [absoluteTick, setAbsoluteTick] = useState(0);
  const [isRunning, setIsRunning] = useState(true);
  const [speed, setSpeed] = useState(100);
  
  // Hardware Orientation Mode
  const [bomLE, setBomLE] = useState(false); // false = U+FEFF, true = U+FFFE

  useEffect(() => {
    let timer;
    if (isRunning) {
      timer = setInterval(() => {
        setAbsoluteTick(t => t + 1);
      }, speed);
    }
    return () => clearInterval(timer);
  }, [isRunning, speed]);

  // Apply 1's Complement vs 2's Complement based on BOM
  const interpTick = useMemo(() => {
    const periodMod = absoluteTick % MASTER_PERIOD;
    if (bomLE) {
      // U+FFFE: One's Complement Interpretation (Inverted Stream within the 7! space)
      // Bitwise NOT equivalent within a bounded state space
      return (MASTER_PERIOD - 1) - periodMod;
    }
    // U+FEFF: Two's Complement / Forward Stream
    return periodMod;
  }, [absoluteTick, bomLE]);

  // Fast Hardware ALU Ticks
  const aluState = useMemo(() => {
    // Wallis Base 60
    const w60 = divMod60(interpTick);
    const w3600 = divMod3600(interpTick);
    
    // Sonar Base 15
    const s15 = divMod15(w60.r); // lane is remainder of phase / 15
    const sonarPhase = w60.r;
    const sonarChannel = s15.q;
    const sonarLane = s15.r;

    // Fano Base 7
    const f7 = divMod7(interpTick);
    const fanoPhase = f7.r;

    return {
      wallis: { u: w60.r, p: w60.q % 60, dp: w3600.q },
      sonar: { phase: sonarPhase, channel: sonarChannel, lane: sonarLane },
      fano: { phase: fanoPhase, triplet: FANO_LINES[fanoPhase] }
    };
  }, [interpTick]);

  const { wallis, sonar, fano } = aluState;

  // Projection Logic
  const projection = useMemo(() => {
    const seed = sonar.lane + (fano.phase * 2);
    switch (sonar.channel) {
      case 0: return { char: (seed % 16).toString(16).toUpperCase(), sub: `HEX_${seed}` };
      case 1: return { char: String.fromCharCode(0x2800 + (seed % 64)), sub: `B6_${seed}` };
      case 2: return { char: getBraille8(seed), sub: `B8_${seed}` };
      case 3: return { char: `${getBraille8(seed)}${getHexagram(seed)}`, sub: "JOIN" };
      default: return { char: "?", sub: "" };
    }
  }, [sonar, fano]);

  const alignment = 1.0 - ((Math.abs((sonar.phase * 7) - (fano.phase * 60)) % 420) / 420);

  return (
    <div className="min-h-screen bg-slate-950 text-slate-200 p-4 md:p-8 flex flex-col font-mono overflow-hidden">
      
      {/* BOM Header & Hardware Config */}
      <div className="flex justify-between items-center pb-6 border-b border-slate-800">
        <div className="flex flex-col gap-2">
           <h1 className="text-sm font-black tracking-[0.4em] text-slate-500 flex items-center gap-2">
             <Cpu size={16} className={bomLE ? "text-rose-500" : "text-emerald-500"} />
             ATOMIC_ALU_SURFACE
           </h1>
           <div className="flex items-center gap-4 text-[10px]">
             <span className="bg-slate-900 px-2 py-1 text-slate-400 border border-slate-800">
               BARRETT_REDUCTION_ACTIVE
             </span>
             <span className="text-slate-500">Fast Reciprocal Division = (N * M) {">>"} Shift</span>
           </div>
        </div>

        {/* The Hardware Toggle */}
        <button 
          onClick={() => setBomLE(!bomLE)}
          className={`px-6 py-3 border flex items-center gap-3 transition-all ${
            bomLE 
              ? 'bg-rose-500/10 border-rose-500/50 text-rose-400' 
              : 'bg-emerald-500/10 border-emerald-500/50 text-emerald-400'
          }`}
        >
          <GitBranch size={16} />
          <div className="text-left">
            <div className="text-xs font-bold">{bomLE ? 'U+FFFE (Little Endian)' : 'U+FEFF (Big Endian)'}</div>
            <div className="text-[10px] uppercase tracking-widest opacity-70">
              {bomLE ? "1's Complement Interpolation" : "2's Complement Interpolation"}
            </div>
          </div>
        </button>
      </div>

      {/* Aegean Wallis Metrology Frame */}
      <div className="flex justify-between items-center py-8">
        <div className="flex items-center gap-4">
          <span className="text-5xl text-blue-500 opacity-90 font-serif drop-shadow-[0_0_15px_blue]">{getAegean(wallis.dp || 1)}</span>
          <div className="flex flex-col">
            <span className="text-[10px] text-slate-500 font-black tracking-widest uppercase">Mathesis Universalis</span>
            <span className="text-2xl font-bold text-slate-300">{wallis.dp}‵‵ {wallis.p}‵</span>
          </div>
        </div>

        <div className="flex flex-col items-center gap-2">
          <div className="text-xs tracking-[1em] text-slate-600 font-black uppercase ml-2">Ticks Interpolated</div>
          <div className="text-3xl font-bold font-sans tracking-tighter text-white">
            {interpTick.toString().padStart(4, '0')} <span className="text-slate-600 text-lg">/ {MASTER_PERIOD}</span>
          </div>
        </div>

        <div className="flex items-center gap-4 text-right">
          <div className="flex flex-col">
            <span className="text-[10px] text-slate-500 font-black tracking-widest uppercase">Sexagesimal Unit</span>
            <span className="text-2xl font-bold text-slate-300">{wallis.u}° 00′ 00″</span>
          </div>
          <span className="text-5xl text-emerald-500 opacity-90 font-serif drop-shadow-[0_0_15px_#10b981]">{getAegean(wallis.u || 1)}</span>
        </div>
      </div>

      {/* Main ALU Animation Frame */}
      <div className="flex-1 flex flex-col items-center justify-center relative">
        
        {/* Alignment Glow */}
        <div className="absolute inset-0 flex items-center justify-center pointer-events-none">
          <div className="w-[400px] h-[400px] bg-blue-500 rounded-full mix-blend-screen blur-[100px] transition-opacity duration-150"
               style={{ opacity: alignment > 0.9 ? 0.3 : 0 }} />
        </div>

        <div className="relative z-10 text-center space-y-6">
          {/* Big Projection */}
          <div className={`text-[35vh] leading-none transition-all duration-75 transform font-serif text-white ${bomLE ? 'text-rose-100' : 'text-emerald-100'}`}
               style={{ textShadow: `0 0 ${alignment * 40}px rgba(255,255,255,0.4)` }}>
            {projection.char}
          </div>

          <div className="inline-flex flex-col items-center">
            <div className={`px-6 py-1 text-2xl font-black tracking-tighter shadow-lg ${bomLE ? 'bg-rose-500 text-black' : 'bg-white text-black'}`}>
              {projection.sub}
            </div>
            <div className="text-[10px] text-slate-500 tracking-[0.4em] uppercase mt-4 flex items-center gap-2">
              Ch_0{sonar.channel} <ArrowLeftRight size={10} /> Ln_{sonar.lane.toString().padStart(2,'0')}
            </div>
          </div>
        </div>

        {/* Structural Overlays */}
        <div className="absolute bottom-10 left-10 space-y-3">
          <p className="text-[10px] font-black text-slate-500 uppercase tracking-widest">Fano Matrix</p>
          <div className="flex gap-2">
            {[0,1,2,3,4,5,6].map(p => (
              <div key={p} className={`w-2 h-6 transition-all duration-150 ${fano.triplet.includes(p) ? 'bg-blue-500 h-10' : 'bg-slate-800'}`} />
            ))}
          </div>
        </div>

        <div className="absolute bottom-10 right-10 space-y-3 text-right">
           <p className="text-[10px] font-black text-slate-500 uppercase tracking-widest">Phase Coherence</p>
           <div className={`text-4xl font-bold tracking-tighter ${alignment > 0.8 ? 'text-emerald-400' : 'text-slate-600'}`}>
             {(alignment * 100).toFixed(0)}%
           </div>
        </div>
      </div>

      {/* Footer Controls */}
      <div className="pt-6 border-t border-slate-800 flex justify-between items-center">
        <div className="flex items-center gap-4">
          <button onClick={() => setIsRunning(!isRunning)} className="p-3 bg-slate-900 border border-slate-700 rounded hover:bg-slate-800 transition">
            <Zap size={16} className={isRunning ? "text-amber-400" : "text-slate-500"} />
          </button>
          <input type="range" min="20" max="300" value={speed} onChange={e => setSpeed(Number(e.target.value))} className="w-32 accent-slate-400" />
        </div>
        
        <div className="flex items-center gap-6">
          <div className="text-right">
             <div className="text-[9px] text-slate-500 font-bold uppercase tracking-widest mb-1">Raw Absolute Tick</div>
             <div className="font-mono text-slate-400">{absoluteTick}</div>
          </div>
          <div className="h-8 w-px bg-slate-800" />
          <div className="text-right">
             <div className="text-[9px] text-slate-500 font-bold uppercase tracking-widest mb-1">Goal Cycle Alignment</div>
             <div className="font-mono text-amber-500 flex items-center gap-2 justify-end">
               <Compass size={12} /> {interpTick % GOAL_CYCLE}°
             </div>
          </div>
        </div>
      </div>

    </div>
  );
};

export default App;