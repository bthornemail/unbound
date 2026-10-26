import React, { useState, useEffect, useMemo, useRef } from 'react';
import { Cpu, ArrowRight, GitBranch, Layers, Activity, Database, FastForward } from 'lucide-react';

/**
 * ATOMIC TAPE READER (The BOM Parser)
 * - Time does not "tick"; it is consumed from the BOM stream.
 * - 5040 Incident Trace Memory.
 * - 10 Discretions (2 Orientations x 5 Channels)
 * - Hardware Arithmetic: Barrett Reduction Shifts
 */

const MASTER_PERIOD = 5040;
const FANO_PERIOD = 7;

// Precomputed Constants for Reciprocal Division (Barrett Reduction)
const barrettDivMod = (n_val, m_val, shift_val, divisor_val) => {
  const n = BigInt(n_val);
  const m = BigInt(m_val);
  const shift = BigInt(shift_val);
  const divisor = BigInt(divisor_val);
  const q = (n * m) >> shift;
  const r = n - (q * divisor);
  return { q: Number(q), r: Number(r) };
};

// Divisors for our Discretions
const divMod5 = (n) => barrettDivMod(n, 858993460, 32, 5); // ceil(2^32 / 5)
const divMod7 = (n) => barrettDivMod(n, 613566757, 32, 7); // ceil(2^32 / 7)

// Symbolic Generators
const FANO_LINES = [
  [0, 1, 3], [0, 2, 5], [0, 4, 6],
  [1, 2, 4], [1, 5, 6],
  [2, 3, 6],
  [3, 4, 5]
];

const CHANNELS = [
  { id: 0, name: "1-BIT HEX", tag: "HEX" },
  { id: 1, name: "6-DOT BRAILLE", tag: "B6" },
  { id: 2, name: "8-DOT BRAILLE", tag: "B8" },
  { id: 3, name: "DIALECTIC JOIN", tag: "JOIN" },
  { id: 4, name: "AEGEAN TALLY", tag: "AEG" }
];

const getAegean = (n) => String.fromCodePoint(0x10107 + (n % 9));
const getBraille6 = (n) => String.fromCharCode(0x2800 + (n % 64));
const getBraille8 = (n) => String.fromCharCode(0x2800 + (n % 256));
const getHexagram = (n) => String.fromCodePoint(0x4DC0 + (n % 64));

// Generate a random "Tape" of BOMs (mostly FEFF, some FFFE)
const generateTapeChunk = (length) => {
  return Array.from({ length }, () => Math.random() > 0.15 ? 'FEFF' : 'FFFE');
};

const App = () => {
  // State is driven purely by the tape
  const [pulseCount, setPulseCount] = useState(0);
  const [tape, setTape] = useState(generateTapeChunk(200));
  const [traceMemory, setTraceMemory] = useState([]);
  const [autoFeed, setAutoFeed] = useState(false);
  const tapeRef = useRef(null);

  // Consume 1 BOM from the tape
  const consumePulse = () => {
    if (tape.length === 0) {
      setTape(generateTapeChunk(200));
      return;
    }
    
    const bom = tape[0];
    const newTape = tape.slice(1);
    
    // Replenish tape if getting low
    if (newTape.length < 50) {
      setTape([...newTape, ...generateTapeChunk(150)]);
    } else {
      setTape(newTape);
    }

    setPulseCount(p => p + 1);
    
    // Record into trace memory (bounded to 5040)
    setTraceMemory(prev => {
      const next = [{ id: pulseCount, bom }, ...prev];
      return next.slice(0, 10); // Keep last 10 visible for UI
    });
  };

  // Auto-feeder (simulating a fast file read)
  useEffect(() => {
    let timer;
    if (autoFeed) {
      timer = setInterval(consumePulse, 50);
    }
    return () => clearInterval(timer);
  }, [autoFeed, tape]);

  // --- ALU PROCESSING ---
  const currentBOM = traceMemory.length > 0 ? traceMemory[0].bom : 'FEFF';
  const isComplement = currentBOM === 'FFFE';

  // 1. Interpolate Stream Position within 5040 space
  const periodMod = pulseCount % MASTER_PERIOD;
  const interpPulse = isComplement ? ((MASTER_PERIOD - 1) - periodMod) : periodMod;

  // 2. Discretions of "2 of 5's"
  // Total discretions = 10. (2 orientations x 5 channels)
  const f5 = divMod5(interpPulse);
  const channelIndex = f5.r; 
  
  // 3. Fano Jitter (Base 7)
  const f7 = divMod7(interpPulse);
  const fanoPhase = f7.r;
  const triplet = FANO_LINES[fanoPhase];

  // 4. Projection Array
  const seed = pulseCount + fanoPhase;
  const activeChannel = CHANNELS[channelIndex];
  
  const getSymbol = () => {
    switch (channelIndex) {
      case 0: return (seed % 16).toString(16).toUpperCase();
      case 1: return getBraille6(seed);
      case 2: return getBraille8(seed);
      case 3: return `${getBraille8(seed)}${getHexagram(seed)}`;
      case 4: return getAegean(seed);
      default: return "?";
    }
  };

  const symbol = getSymbol();

  return (
    <div className="min-h-screen bg-[#020202] text-slate-300 p-4 md:p-8 font-mono flex flex-col overflow-hidden">
      
      {/* HUD Header */}
      <div className="flex flex-col md:flex-row justify-between items-start md:items-center gap-6 border-b border-white/10 pb-6 mb-8">
        <div>
          <h1 className="text-sm font-black tracking-[0.5em] text-white/50 flex items-center gap-3">
            <Cpu size={16} className="text-emerald-500" />
            ATOMIC_TAPE_READER [V4.0]
          </h1>
          <p className="text-[10px] text-white/30 uppercase mt-2">
            Time is parsed, not generated. | Discretions of 2 (BOM) of 5 (Channels).
          </p>
        </div>
        
        {/* Hardware Controls */}
        <div className="flex gap-4">
          <button 
            onClick={consumePulse}
            className="px-6 py-3 bg-white/10 hover:bg-white/20 border border-white/20 flex items-center gap-2 font-bold transition-all active:scale-95"
          >
            <ArrowRight size={16} /> READ 1 BYTE
          </button>
          <button 
            onClick={() => setAutoFeed(!autoFeed)}
            className={`px-6 py-3 border flex items-center gap-2 font-bold transition-all ${
              autoFeed ? 'bg-amber-500/20 border-amber-500/50 text-amber-500' : 'bg-white/5 border-white/10 hover:bg-white/10'
            }`}
          >
            <FastForward size={16} /> {autoFeed ? 'HALT STREAM' : 'AUTO READ'}
          </button>
        </div>
      </div>

      {/* Main Layout Grid */}
      <div className="grid grid-cols-1 lg:grid-cols-4 gap-8 flex-1">
        
        {/* LEFT PANEL: The Tape Stream */}
        <div className="lg:col-span-1 bg-white/[0.02] border border-white/5 p-4 flex flex-col">
          <h2 className="text-[10px] font-black tracking-widest text-white/40 mb-4 flex items-center gap-2">
            <Database size={12} /> INCOMING TAPE
          </h2>
          
          {/* Tape visualization */}
          <div className="flex-1 overflow-hidden relative" ref={tapeRef}>
            <div className="absolute top-1/2 -translate-y-1/2 left-0 right-0 h-10 border-y border-emerald-500/30 bg-emerald-500/5 flex items-center justify-between px-2 z-10">
              <span className="text-[10px] text-emerald-500 font-bold tracking-widest">READ HEAD</span>
              <ArrowRight size={12} className="text-emerald-500" />
            </div>

            <div className="flex flex-col gap-1 items-center transform translate-y-1/2 transition-transform duration-75">
              {tape.slice(0, 15).map((bom, i) => (
                <div key={i} className={`px-4 py-2 text-sm font-bold w-32 text-center rounded ${
                  i === 0 ? (bom === 'FEFF' ? 'bg-emerald-600 text-white shadow-[0_0_15px_#059669]' : 'bg-rose-600 text-white shadow-[0_0_15px_#e11d48]') 
                  : (bom === 'FEFF' ? 'bg-white/10 text-white/50' : 'bg-rose-500/10 text-rose-500/50')
                }`}>
                  {bom}
                </div>
              ))}
            </div>
          </div>
        </div>

        {/* CENTER PANEL: The ALU Projection */}
        <div className="lg:col-span-2 flex flex-col items-center justify-center relative bg-white/[0.01] border border-white/5 rounded-3xl p-8">
           
           <div className="absolute top-6 left-6 right-6 flex justify-between items-center text-[10px] tracking-widest font-black uppercase text-white/30">
             <div className="flex items-center gap-2">
               <GitBranch size={12} className={isComplement ? 'text-rose-500' : 'text-emerald-500'} />
               {isComplement ? "1'S COMPLEMENT (U+FFFE)" : "2'S COMPLEMENT (U+FEFF)"}
             </div>
             <div>PULSE_{pulseCount.toString().padStart(6, '0')}</div>
           </div>

           <div className={`text-[25vh] leading-none transition-transform duration-75 ${
             isComplement ? 'text-rose-100 scale-95' : 'text-emerald-100 scale-105'
           }`} style={{ textShadow: `0 0 50px rgba(255,255,255,0.1)` }}>
             {symbol}
           </div>

           <div className={`mt-8 px-6 py-2 text-2xl font-black tracking-widest ${
             isComplement ? 'bg-rose-500 text-black' : 'bg-white text-black'
           }`}>
             {activeChannel.tag}_{seed}
           </div>

           {/* Fano Jitter Indicator */}
           <div className="absolute bottom-8 flex gap-2">
             {[0,1,2,3,4,5,6].map(p => (
               <div key={p} className={`w-2 h-2 rounded-full transition-all duration-75 ${
                 triplet.includes(p) ? (isComplement ? 'bg-rose-500 scale-150' : 'bg-emerald-500 scale-150') : 'bg-white/10'
               }`} />
             ))}
           </div>
        </div>

        {/* RIGHT PANEL: Discretion Trace Memory */}
        <div className="lg:col-span-1 bg-white/[0.02] border border-white/5 p-4 flex flex-col">
          <h2 className="text-[10px] font-black tracking-widest text-white/40 mb-4 flex items-center gap-2">
            <Layers size={12} /> 5040 INCIDENT TRACE
          </h2>
          
          <div className="space-y-4 flex-1">
             <div className="grid grid-cols-2 gap-4 text-center">
               <div className="bg-white/5 p-3 rounded border border-white/10">
                 <div className="text-[9px] text-white/30 mb-1">INTERP_ADDRESS</div>
                 <div className="text-xl text-blue-400 font-bold">{interpPulse}</div>
               </div>
               <div className="bg-white/5 p-3 rounded border border-white/10">
                 <div className="text-[9px] text-white/30 mb-1">DISCRETION (2x5)</div>
                 <div className="text-xl text-white font-bold">{isComplement ? 1 : 0} : {channelIndex}</div>
               </div>
             </div>

             <div className="pt-4 space-y-1">
               <div className="text-[9px] text-white/30 tracking-widest mb-2 border-b border-white/10 pb-2">RECENT TRACE KERNELS</div>
               {traceMemory.map((mem, i) => (
                 <div key={mem.id} className={`flex justify-between items-center text-xs p-2 rounded ${
                   i === 0 ? 'bg-white/10 font-bold' : 'text-white/40'
                 }`}>
                   <span>Idx: {mem.id}</span>
                   <span className={mem.bom === 'FEFF' ? 'text-emerald-500' : 'text-rose-500'}>
                     {mem.bom}
                   </span>
                 </div>
               ))}
               {traceMemory.length === 0 && <div className="text-xs text-white/20 italic p-2">Awaiting stream...</div>}
             </div>
          </div>

          <div className="mt-auto border-t border-white/10 pt-4">
             <div className="text-[10px] font-black text-white/20 tracking-widest leading-relaxed">
               BARRETT_REDUCTION: ONLINE<br/>
               DIV_FREE_SHIFT_ALU: ACTIVE<br/>
               LCM(7, 8, 60, 240, 360) = 5040
             </div>
          </div>
        </div>

      </div>
    </div>
  );
};

export default App;