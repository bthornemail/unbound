import React, { useState, useMemo, useEffect } from 'react';
import { Box, Code, GitMerge, Cpu, Network, Database, ChevronRight, Activity } from 'lucide-react';

/**
 * OMICRON EVALUATOR: POLYFORM TRUTH SURFACE
 * Grand Unified Theorem: Polyform-Truth-Polynomial Isomorphism
 * 16 Polyforms = 16 Boolean Truth Functions
 * 15-bit MaxiCode = Polynomial Signature
 */

// --- The 16 Boolean Functions / Polyform Bases ---
// Array order maps to TT, TF, FT, FF
const BOOLEAN_FUNCTIONS = [
  { id: 0,  name: "Zero (⊥)",         bits: [0,0,0,0], polyform: "Void (0)",       meaning: "Always False" },
  { id: 1,  name: "NOR",              bits: [0,0,0,1], polyform: "Hexomino (6)",   meaning: "p ↓ q" },
  { id: 2,  name: "Converse NonImp",  bits: [0,0,1,0], polyform: "Undecimino",     meaning: "p ↚ q" },
  { id: 3,  name: "NOT p",            bits: [0,0,1,1], polyform: "Nonomino (9)",   meaning: "¬p" },
  { id: 4,  name: "NonImplication",   bits: [0,1,0,0], polyform: "Dodecomino",     meaning: "p ↛ q" },
  { id: 5,  name: "NOT q",            bits: [0,1,0,1], polyform: "Decomino (10)",  meaning: "¬q" },
  { id: 6,  name: "XOR",              bits: [0,1,1,0], polyform: "Tetromino (4)",  meaning: "p ↮ q" },
  { id: 7,  name: "NAND",             bits: [0,1,1,1], polyform: "Pentomino (5)",  meaning: "p ↑ q" },
  { id: 8,  name: "AND",              bits: [1,0,0,0], polyform: "Domino (2)",     meaning: "p ∧ q" },
  { id: 9,  name: "XNOR",             bits: [1,0,0,1], polyform: "Octomino (8)",   meaning: "p ↔ q" },
  { id: 10, name: "q",                bits: [1,0,1,0], polyform: "Tridecomino",    meaning: "q" },
  { id: 11, name: "Implication",      bits: [1,0,1,1], polyform: "Heptomino (7)",  meaning: "p → q" },
  { id: 12, name: "p",                bits: [1,1,0,0], polyform: "Monomino (1)",   meaning: "p" },
  { id: 13, name: "Converse Imp",     bits: [1,1,0,1], polyform: "Tetradecomino",  meaning: "p ← q" },
  { id: 14, name: "OR",               bits: [1,1,1,0], polyform: "Tromino (3)",    meaning: "p ∨ q" },
  { id: 15, name: "One (⊤) / Closure",bits: [1,1,1,1], polyform: "Omicron (16)",   meaning: "Always True" }
];

// --- 15-Bit Signature Enums ---
const ARITY = [
  { val: 0, bits: "00", name: "Unary", rows: 2 },
  { val: 1, bits: "01", name: "Binary", rows: 4 },
  { val: 2, bits: "10", name: "Ternary", rows: 8 },
  { val: 3, bits: "11", name: "Quaternary", rows: 16 }
];

const ORDERING = [
  { val: 0, bits: "00", name: "Lexicographic" },
  { val: 1, bits: "01", name: "GradedLex" },
  { val: 2, bits: "10", name: "GradedRevLex" },
  { val: 3, bits: "11", name: "Elimination" }
];

const GROUPING = [
  { val: 0, bits: "00", name: "Monomial" },
  { val: 1, bits: "01", name: "Binomial" },
  { val: 2, bits: "10", name: "Trinomial" },
  { val: 3, bits: "11", name: "Quadrinomial" }
];

const SEQUENCING = [
  { val: 0, bits: "00", name: "Sequential" },
  { val: 1, bits: "01", name: "Parallel" },
  { val: 2, bits: "10", name: "Recursive" },
  { val: 3, bits: "11", name: "Iterative" }
];

const CLOSURE = [
  { val: 0, bits: "0", name: "Open" },
  { val: 1, bits: "1", name: "Closed (Omicron)" }
];

const App = () => {
  // Signature State
  const [selectedFn, setSelectedFn] = useState(7); // Default: Pentomino (NAND)
  const [arity, setArity] = useState(1);
  const [ordering, setOrdering] = useState(0);
  const [grouping, setGrouping] = useState(0);
  const [sequencing, setSequencing] = useState(0);
  const [closure, setClosure] = useState(0);

  // Live Stream State (P and Q streams)
  const [pStream, setPStream] = useState([1, 0, 1, 0, 1, 1, 0, 0]);
  const [qStream, setQStream] = useState([1, 1, 0, 0, 1, 0, 1, 0]);
  const [streamIndex, setStreamIndex] = useState(0);
  const [isRunning, setIsRunning] = useState(false);

  useEffect(() => {
    let timer;
    if (isRunning) {
      timer = setInterval(() => {
        setStreamIndex(idx => (idx + 1) % 8);
      }, 500);
    }
    return () => clearInterval(timer);
  }, [isRunning]);

  const activeFn = BOOLEAN_FUNCTIONS[selectedFn];
  const pVal = pStream[streamIndex];
  const qVal = qStream[streamIndex];
  
  // Truth Table Evaluator
  const getOutput = (p, q) => {
    if (p===1 && q===1) return activeFn.bits[0];
    if (p===1 && q===0) return activeFn.bits[1];
    if (p===0 && q===1) return activeFn.bits[2];
    if (p===0 && q===0) return activeFn.bits[3];
    return 0;
  };

  const outputVal = getOutput(pVal, qVal);

  // Derived 15-bit MaxiCode Signature
  const fnBits = activeFn.bits.join("");
  const signatureBits = `${fnBits} ${ARITY[arity].bits} ${ORDERING[ordering].bits} ${GROUPING[grouping].bits} ${SEQUENCING[sequencing].bits} ${CLOSURE[closure].bits} 00`;

  return (
    <div className="min-h-screen bg-slate-950 text-slate-300 p-4 md:p-8 font-mono overflow-hidden flex flex-col">
      
      {/* Header */}
      <div className="border-b border-white/10 pb-6 mb-8 flex justify-between items-end">
        <div>
           <h1 className="text-sm font-black tracking-[0.4em] text-white/50 flex items-center gap-3">
             <Network size={16} className="text-emerald-500" />
             OMICRON_EVALUATOR // POLYFORM_TRUTH_SURFACE
           </h1>
           <p className="text-[10px] text-white/30 uppercase mt-2">
             Theorem: Polynomial-Truth-Polyform Isomorphism is complete.
           </p>
        </div>
        <div className="text-right flex flex-col items-end">
           <div className="text-[10px] font-bold text-emerald-500 tracking-widest uppercase mb-1">
             MaxiCode 15-Bit Signature
           </div>
           <div className="text-xl font-bold tracking-[0.2em] bg-white/5 px-4 py-1 border border-white/10 text-white">
             {signatureBits}
           </div>
        </div>
      </div>

      <div className="grid grid-cols-1 xl:grid-cols-12 gap-8 flex-1">
        
        {/* LEFT PANEL: The Polyform Basis Selector */}
        <div className="xl:col-span-3 space-y-4">
          <h2 className="text-[10px] font-black tracking-widest text-white/40 border-b border-white/10 pb-2 flex items-center gap-2">
            <Box size={14} /> POLYFORM BASIS (TRUTH FUNCTION)
          </h2>
          <div className="flex flex-col gap-1 max-h-[60vh] overflow-y-auto pr-2 custom-scrollbar">
            {BOOLEAN_FUNCTIONS.map((fn, i) => (
              <button 
                key={i}
                onClick={() => setSelectedFn(i)}
                className={`text-left px-3 py-2 text-xs flex justify-between items-center transition-all ${
                  selectedFn === i 
                    ? 'bg-emerald-500/20 border-l-2 border-emerald-500 text-white' 
                    : 'hover:bg-white/5 text-white/50 border-l-2 border-transparent'
                }`}
              >
                <div>
                  <div className="font-bold">{fn.polyform}</div>
                  <div className="text-[10px] opacity-60 mt-0.5">{fn.name} : {fn.meaning}</div>
                </div>
                <div className="font-mono text-[10px] tracking-widest px-2 py-1 bg-black/40 rounded">
                  {fn.bits.join("")}
                </div>
              </button>
            ))}
          </div>
        </div>

        {/* CENTER PANEL: Cell Evaluation & Stream */}
        <div className="xl:col-span-6 flex flex-col gap-8">
           
           {/* Interactive Truth Table Cell */}
           <div className="bg-white/[0.02] border border-white/10 rounded-2xl p-8 flex flex-col items-center relative overflow-hidden">
             
             <div className="absolute top-4 left-4 text-[10px] font-bold tracking-widest text-emerald-500 uppercase">
               Active Cell: {activeFn.polyform}
             </div>
             
             <div className="flex items-center gap-12 mt-8 z-10">
               {/* Inputs */}
               <div className="space-y-6">
                 <div className={`w-16 h-16 flex items-center justify-center text-2xl font-bold border-2 transition-all ${pVal ? 'bg-blue-500/20 border-blue-500 text-blue-400 shadow-[0_0_20px_rgba(59,130,246,0.3)]' : 'border-white/10 text-white/20'}`}>
                   P={pVal}
                 </div>
                 <div className={`w-16 h-16 flex items-center justify-center text-2xl font-bold border-2 transition-all ${qVal ? 'bg-amber-500/20 border-amber-500 text-amber-400 shadow-[0_0_20px_rgba(245,158,11,0.3)]' : 'border-white/10 text-white/20'}`}>
                   Q={qVal}
                 </div>
               </div>

               {/* Gate Connection */}
               <div className="flex flex-col items-center">
                 <div className="w-24 h-px bg-white/20 mb-1" />
                 <Network className="text-white/30" />
                 <div className="w-24 h-px bg-white/20 mt-1" />
               </div>

               {/* The Polyform Gate Evaluator */}
               <div className={`w-32 h-32 rounded-xl flex flex-col items-center justify-center border-4 transition-all ${
                 outputVal 
                   ? 'bg-emerald-500/20 border-emerald-500 shadow-[0_0_40px_rgba(16,185,129,0.4)]' 
                   : 'bg-white/5 border-white/10'
               }`}>
                  <span className="text-sm font-bold tracking-widest uppercase text-white/40 mb-2">OUTPUT</span>
                  <span className={`text-5xl font-black ${outputVal ? 'text-emerald-400' : 'text-white/20'}`}>
                    {outputVal}
                  </span>
               </div>
             </div>

             {/* Functional Completeness Note */}
             <div className="mt-12 text-center">
               <div className="text-[10px] uppercase tracking-widest text-white/30 mb-2">Wittgenstein Table Row Mapping</div>
               <div className="flex gap-4">
                 {[[1,1],[1,0],[0,1],[0,0]].map((row, idx) => (
                   <div key={idx} className={`px-3 py-1 text-[10px] border ${pVal===row[0] && qVal===row[1] ? 'bg-white text-black font-bold border-white' : 'border-white/10 text-white/40'}`}>
                     ({row[0]},{row[1]}) ➔ {activeFn.bits[idx]}
                   </div>
                 ))}
               </div>
             </div>
           </div>

           {/* Tape Stream Feed */}
           <div className="bg-white/[0.02] border border-white/10 rounded-2xl p-6">
             <div className="flex justify-between items-center mb-6">
                <h2 className="text-[10px] font-black tracking-widest text-white/40 flex items-center gap-2">
                  <Database size={14} /> STREAM EVALUATION
                </h2>
                <button 
                  onClick={() => setIsRunning(!isRunning)}
                  className="px-4 py-1.5 border border-emerald-500/50 bg-emerald-500/10 text-emerald-400 text-[10px] font-bold uppercase hover:bg-emerald-500 hover:text-black transition-colors"
                >
                  {isRunning ? "HALT CLOCK" : "START CLOCK"}
                </button>
             </div>

             <div className="space-y-2">
               {/* P Stream */}
               <div className="flex items-center gap-4">
                 <span className="w-4 font-bold text-blue-400">P</span>
                 <div className="flex-1 flex gap-1">
                   {pStream.map((bit, i) => (
                     <div key={i} className={`flex-1 h-6 flex items-center justify-center text-xs font-bold ${i === streamIndex ? 'bg-blue-500 text-white' : (bit ? 'bg-white/20' : 'bg-white/5')}`}>{bit}</div>
                   ))}
                 </div>
               </div>
               {/* Q Stream */}
               <div className="flex items-center gap-4">
                 <span className="w-4 font-bold text-amber-400">Q</span>
                 <div className="flex-1 flex gap-1">
                   {qStream.map((bit, i) => (
                     <div key={i} className={`flex-1 h-6 flex items-center justify-center text-xs font-bold ${i === streamIndex ? 'bg-amber-500 text-white' : (bit ? 'bg-white/20' : 'bg-white/5')}`}>{bit}</div>
                   ))}
                 </div>
               </div>
               {/* Output Stream */}
               <div className="flex items-center gap-4 mt-4 pt-4 border-t border-white/10">
                 <span className="w-4 font-bold text-emerald-400">R</span>
                 <div className="flex-1 flex gap-1">
                   {pStream.map((p, i) => {
                     const q = qStream[i];
                     const out = getOutput(p, q);
                     return (
                       <div key={i} className={`flex-1 h-8 flex items-center justify-center text-sm font-black border ${i === streamIndex ? 'bg-emerald-500 border-emerald-400 text-black shadow-[0_0_10px_#10b981]' : (out ? 'bg-white/20 border-white/10' : 'bg-white/5 border-transparent')}`}>{out}</div>
                     )
                   })}
                 </div>
               </div>
             </div>
           </div>
        </div>

        {/* RIGHT PANEL: Identity Signature Builder */}
        <div className="xl:col-span-3 space-y-4">
          <h2 className="text-[10px] font-black tracking-widest text-white/40 border-b border-white/10 pb-2 flex items-center gap-2">
            <Code size={14} /> SIGNATURE METADATA
          </h2>
          
          <div className="space-y-4">
            {/* Arity */}
            <div>
              <div className="text-[10px] text-white/30 uppercase tracking-widest mb-2 flex justify-between">
                <span>Arity ({ARITY[arity].rows} Rows)</span>
                <span className="text-emerald-500">{ARITY[arity].bits}</span>
              </div>
              <div className="flex gap-1">
                {ARITY.map((a, i) => (
                  <button key={i} onClick={() => setArity(i)} className={`flex-1 text-[10px] py-1 ${arity === i ? 'bg-white text-black' : 'bg-white/10 hover:bg-white/20'}`}>{a.name}</button>
                ))}
              </div>
            </div>

            {/* Ordering */}
            <div>
              <div className="text-[10px] text-white/30 uppercase tracking-widest mb-2 flex justify-between">
                <span>Ordering</span>
                <span className="text-emerald-500">{ORDERING[ordering].bits}</span>
              </div>
              <div className="grid grid-cols-2 gap-1">
                {ORDERING.map((o, i) => (
                  <button key={i} onClick={() => setOrdering(i)} className={`text-[10px] py-1 ${ordering === i ? 'bg-white text-black' : 'bg-white/10 hover:bg-white/20'}`}>{o.name}</button>
                ))}
              </div>
            </div>

            {/* Grouping */}
            <div>
              <div className="text-[10px] text-white/30 uppercase tracking-widest mb-2 flex justify-between">
                <span>Grouping</span>
                <span className="text-emerald-500">{GROUPING[grouping].bits}</span>
              </div>
              <div className="grid grid-cols-2 gap-1">
                {GROUPING.map((g, i) => (
                  <button key={i} onClick={() => setGrouping(i)} className={`text-[10px] py-1 ${grouping === i ? 'bg-white text-black' : 'bg-white/10 hover:bg-white/20'}`}>{g.name}</button>
                ))}
              </div>
            </div>

            {/* Sequencing */}
            <div>
              <div className="text-[10px] text-white/30 uppercase tracking-widest mb-2 flex justify-between">
                <span>Evaluation Sequencing</span>
                <span className="text-emerald-500">{SEQUENCING[sequencing].bits}</span>
              </div>
              <div className="grid grid-cols-2 gap-1">
                {SEQUENCING.map((s, i) => (
                  <button key={i} onClick={() => setSequencing(i)} className={`text-[10px] py-1 ${sequencing === i ? 'bg-white text-black' : 'bg-white/10 hover:bg-white/20'}`}>{s.name}</button>
                ))}
              </div>
            </div>

            {/* Closure */}
            <div>
              <div className="text-[10px] text-white/30 uppercase tracking-widest mb-2 flex justify-between">
                <span>Omicron Closure</span>
                <span className="text-emerald-500">{CLOSURE[closure].bits}</span>
              </div>
              <div className="flex gap-1">
                {CLOSURE.map((c, i) => (
                  <button key={i} onClick={() => setClosure(i)} className={`flex-1 text-[10px] py-1 ${closure === i ? 'bg-emerald-500 text-black' : 'bg-white/10 hover:bg-white/20'}`}>{c.name}</button>
                ))}
              </div>
            </div>
          </div>
          
          {/* Hardware Compile Note */}
          <div className="mt-8 p-4 bg-emerald-500/10 border border-emerald-500/30 rounded text-[10px] font-mono leading-relaxed text-emerald-400">
             {'>'} The truth table acts as the hardware gate.<br/>
             {'>'} Stream states flow through the cell in sequence.<br/>
             {'>'} 15-Bit MaxiCode Signature validates execution context prior to Omicron evaluation.
          </div>

        </div>
      </div>
      
      <style dangerouslySetInnerHTML={{ __html: `
        .custom-scrollbar::-webkit-scrollbar { width: 4px; }
        .custom-scrollbar::-webkit-scrollbar-track { background: rgba(255,255,255,0.05); }
        .custom-scrollbar::-webkit-scrollbar-thumb { background: rgba(255,255,255,0.2); border-radius: 4px; }
      ` }} />
    </div>
  );
};

export default App;