import React, { useState, useEffect, useMemo } from 'react';
import { Cpu, Zap, Activity, Hash, Layers } from 'lucide-react';

/**
 * ATOMIC INCIDENCE SURFACE V3.0
 * 5-Channel Sonar Mapping (τ₁₅ x 5 = 75 slots)
 * Master Period: 5040 (7!)
 */

const MASTER_PERIOD = 5040;
const GOAL_CYCLE = 360;
const CHANNEL_WIDTH = 15;
const SONAR_PERIOD = 75; // 5 channels * 15 lanes
const FANO_PERIOD = 7;

const FANO_LINES = [
  [0, 1, 3], [0, 2, 5], [0, 4, 6],
  [1, 2, 4], [1, 5, 6],
  [2, 3, 6],
  [3, 4, 5]
];

const CHANNELS = [
  { id: 0, name: "1-BIT HEX", desc: "Binary Character Codes" },
  { id: 1, name: "6-DOT BRAILLE", desc: "Standard Tactile" },
  { id: 2, name: "8-DOT BRAILLE", desc: "Extended Register" },
  { id: 3, name: "BRAILLE/HEX PAIR", desc: "Dialectical Join" },
  { id: 4, name: "AEGEAN NUMBERS", desc: "Tally Witness" }
];

// --- Symbolic Generators ---

const getAegean = (n) => String.fromCodePoint(0x10107 + (n % 9));
const getBraille6 = (n) => String.fromCharCode(0x2800 + (n % 64));
const getBraille8 = (n) => String.fromCharCode(0x2800 + (n % 256));
const getHexagram = (n) => String.fromCodePoint(0x4DC0 + (n % 64));

const App = () => {
  const [tick, setTick] = useState(0);
  const [isRunning, setIsRunning] = useState(true);
  const [speed, setSpeed] = useState(100);

  useEffect(() => {
    let timer;
    if (isRunning) {
      timer = setInterval(() => {
        setTick(t => (t + 1) % MASTER_PERIOD);
      }, speed);
    }
    return () => clearInterval(timer);
  }, [isRunning, speed]);

  // Timing Calcs
  const sonar = useMemo(() => {
    const phase = tick % SONAR_PERIOD;
    const channel = Math.floor(phase / CHANNEL_WIDTH);
    const lane = phase % CHANNEL_WIDTH;
    return { phase, channel, lane };
  }, [tick]);

  const fano = useMemo(() => {
    const phase = tick % FANO_PERIOD;
    return { phase, triplet: FANO_LINES[phase] };
  }, [tick]);

  // Primary Projection Logic
  const projection = useMemo(() => {
    const { channel, lane } = sonar;
    const jitter = fano.phase;
    const seed = lane + (jitter * 2);

    switch (channel) {
      case 0: // 1-bit Hex Codes
        return { char: (seed % 16).toString(16).toUpperCase(), sub: `0x${seed.toString(16)}` };
      case 1: // 6-dot
        return { char: getBraille6(seed), sub: `B6-${seed}` };
      case 2: // 8-dot
        return { char: getBraille8(seed), sub: `B8-${seed}` };
      case 3: // Pairs
        return { 
          char: `${getBraille8(seed)}${getHexagram(seed)}`, 
          sub: "JOIN" 
        };
      case 4: // Aegean
        return { char: getAegean(seed), sub: `AEG-${seed}` };
      default:
        return { char: "?", sub: "" };
    }
  }, [sonar, fano]);

  const alignment = 1.0 - ((Math.abs((sonar.phase * 7) - (fano.phase * 75)) % 525) / 525);

  return (
    <div className="min-h-screen bg-[#050505] text-white p-4 md:p-10 font-mono flex flex-col">
      {/* HUD Header */}
      <div className="flex justify-between items-end border-b border-white/10 pb-6 mb-8">
        <div className="space-y-1">
          <h1 className="text-sm font-black tracking-[0.4em] text-white/30 flex items-center gap-3">
            <Cpu size={16} className="text-blue-500" /> 
            INCIDENCE_SURFACE_V3 [5_CHANNEL_SONAR]
          </h1>
          <div className="flex gap-6 text-[10px]">
            <span className="text-blue-400">TICK_{tick.toString().padStart(4, '0')}</span>
            <span className="text-white/40">MASTER_7!_COMPLIANT</span>
            <span className={tick % 120 === 0 ? "text-emerald-400" : "text-white/20"}>SYNC_120</span>
          </div>
        </div>
        <div className="flex items-center gap-6">
          <div className="text-right">
            <p className="text-[10px] text-white/20 uppercase font-black">Chiral Alignment</p>
            <p className={`text-xl font-bold ${alignment > 0.8 ? 'text-blue-400' : 'text-white/40'}`}>
              {(alignment * 100).toFixed(0)}%
            </p>
          </div>
          <button 
            onClick={() => setIsRunning(!isRunning)}
            className="w-12 h-12 rounded-full border border-white/10 flex items-center justify-center hover:bg-white hover:text-black transition-colors"
          >
            {isRunning ? <Zap size={20} fill="currentColor" /> : <Activity size={20} />}
          </button>
        </div>
      </div>

      {/* Main Projection Frame */}
      <div className="flex-1 flex flex-col items-center justify-center relative border border-white/5 rounded-3xl bg-gradient-to-b from-transparent to-white/[0.02]">
        
        {/* Background Channel Label */}
        <div className="absolute top-12 text-[10px] tracking-[1em] text-white/10 font-black uppercase text-center w-full">
          {CHANNELS[sonar.channel].name} // {CHANNELS[sonar.channel].desc}
        </div>

        {/* Big Unicode Display */}
        <div className="relative">
          <div className="absolute inset-0 blur-3xl bg-blue-500/10 rounded-full scale-150 animate-pulse" />
          <div className={`relative z-10 transition-all duration-100 ease-out flex items-center gap-4 ${alignment > 0.9 ? 'scale-110' : 'scale-100'}`}>
            <span className="text-[35vh] leading-none font-serif drop-shadow-[0_0_30px_rgba(255,255,255,0.1)]">
              {projection.char}
            </span>
          </div>
        </div>

        {/* Lane HUD */}
        <div className="mt-12 flex flex-col items-center gap-2">
           <div className="text-4xl font-black bg-white text-black px-6 py-2">
             LANE_{sonar.lane.toString().padStart(2, '0')}
           </div>
           <div className="text-[10px] tracking-widest text-white/40 uppercase">
             REGIME_{projection.sub} // SONAR_POS_{sonar.phase}
           </div>
        </div>

        {/* Fano Winner Points Overlay */}
        <div className="absolute left-10 bottom-10 space-y-3">
          <p className="text-[10px] font-black text-white/20 uppercase tracking-widest flex items-center gap-2">
            <Layers size={12} /> Fano Triplet
          </p>
          <div className="flex gap-2">
            {[0,1,2,3,4,5,6].map(p => (
              <div 
                key={p} 
                className={`w-2 h-8 rounded-full transition-all duration-300 ${
                  fano.triplet.includes(p) ? 'bg-blue-500 shadow-[0_0_15px_rgba(59,130,246,0.5)]' : 'bg-white/5'
                }`} 
              />
            ))}
          </div>
        </div>

        {/* Sonar Progress Track */}
        <div className="absolute bottom-0 left-0 w-full h-1.5 flex gap-1 px-10 mb-4">
          {CHANNELS.map((ch) => (
            <div key={ch.id} className="flex-1 h-full bg-white/5 rounded-full overflow-hidden">
               {sonar.channel === ch.id && (
                 <div 
                   className="h-full bg-blue-600 transition-all duration-75"
                   style={{ width: `${(sonar.lane / 14) * 100}%` }}
                 />
               )}
            </div>
          ))}
        </div>
      </div>

      {/* Footer Metrics */}
      <div className="grid grid-cols-2 md:grid-cols-5 gap-4 mt-8">
        {[
          { label: "Absolute Tick", val: tick, color: "text-white" },
          { label: "Goal Phase", val: `${(tick % GOAL_CYCLE)}°`, color: "text-amber-500" },
          { label: "Sonar Phase", val: `${sonar.phase}/75`, color: "text-emerald-500" },
          { label: "Fano Phase", val: `${fano.phase}/7`, color: "text-blue-500" },
          { label: "Master Reset", val: "7!", color: "text-white/40" }
        ].map((m, i) => (
          <div key={i} className="bg-white/5 border border-white/10 p-4 rounded-xl">
            <p className="text-[9px] font-black text-white/20 uppercase tracking-tighter mb-1">{m.label}</p>
            <p className={`text-lg font-bold font-mono ${m.color}`}>{m.val}</p>
          </div>
        ))}
      </div>
    </div>
  );
};

export default App;