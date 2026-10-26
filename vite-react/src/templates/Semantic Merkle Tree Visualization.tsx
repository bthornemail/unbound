import React, { useState, useEffect, useMemo } from 'react';
import { 
  Network, 
  GitBranch, 
  Binary, 
  ShieldCheck, 
  Cpu, 
  RefreshCw, 
  Hash,
  Layers,
  Activity
} from 'lucide-react';

/**
 * ARCHITECTURAL MAP:
 * ROOT (Identity): Fano Plane Point (1 of 7)
 * BRANCH (Logic): Register State (1 of 8)
 * SUB-BRANCH (Symbol): Hexagram State (1 of 64)
 * LEAVES (Verification): 240-node Shell (Residuals)
 */

const App = () => {
  const [activeRoot, setActiveRoot] = useState(0); // 0-6
  const [activeBranch, setActiveBranch] = useState(0); // 0-7
  const [hexagram, setHexagram] = useState([1, 1, 0, 1, 0, 0]); // 6-bit state
  const [isVerifying, setIsVerifying] = useState(false);
  const [history, setHistory] = useState([]);

  // Generate a mock hash for the current tree state
  const currentHash = useMemo(() => {
    const data = `${activeRoot}-${activeBranch}-${hexagram.join('')}`;
    // Simple mock hash function for UI
    let hash = 0;
    for (let i = 0; i < data.length; i++) {
      hash = ((hash << 5) - hash) + data.charCodeAt(i);
      hash |= 0;
    }
    return Math.abs(hash).toString(16).padStart(8, '0').toUpperCase();
  }, [activeRoot, activeBranch, hexagram]);

  const verifyState = () => {
    setIsVerifying(true);
    setTimeout(() => {
      const newEntry = {
        id: history.length,
        hash: currentHash,
        timestamp: new Date().toLocaleTimeString(),
        status: 'VALID'
      };
      setHistory(prev => [newEntry, ...prev].slice(0, 5));
      setIsVerifying(false);
    }, 800);
  };

  return (
    <div className="min-h-screen bg-slate-950 text-slate-300 font-mono p-4 md:p-8 selection:bg-cyan-500/30">
      {/* Header */}
      <div className="max-w-7xl mx-auto mb-8 flex flex-col md:flex-row md:items-end justify-between gap-4 border-b border-white/10 pb-6">
        <div>
          <h1 className="text-2xl font-bold text-white flex items-center gap-3">
            <Network className="text-cyan-400" />
            Semantic Merkle Lattice
          </h1>
          <p className="text-sm text-slate-500 mt-1">Decentralized Attribution Architecture: 7 → 8 → 64 → 240</p>
        </div>
        <div className="flex items-center gap-4 bg-black/40 px-4 py-2 rounded-lg border border-white/5">
          <div className="text-right">
            <p className="text-[10px] text-slate-500 uppercase">Current Root Hash</p>
            <p className="text-cyan-400 font-bold">{currentHash}</p>
          </div>
          <button 
            onClick={verifyState}
            disabled={isVerifying}
            className={`p-2 rounded-md transition-all ${isVerifying ? 'bg-slate-800' : 'bg-cyan-600 hover:bg-cyan-500 text-black'}`}
          >
            <RefreshCw className={`w-5 h-5 ${isVerifying ? 'animate-spin' : ''}`} />
          </button>
        </div>
      </div>

      <div className="max-w-7xl mx-auto grid grid-cols-1 lg:grid-cols-12 gap-8">
        
        {/* Left: Tree Navigation */}
        <div className="lg:col-span-4 space-y-6">
          
          {/* Level 1: Fano Identity Root */}
          <section className="p-5 rounded-xl bg-slate-900/50 border border-white/10">
            <h2 className="text-xs font-bold text-slate-500 uppercase mb-4 flex items-center gap-2">
              <Layers className="w-4 h-4 text-cyan-500" />
              Level 1: Fano Identity Root
            </h2>
            <div className="flex justify-between items-center bg-black/30 p-4 rounded-lg">
              {[...Array(7)].map((_, i) => (
                <button
                  key={i}
                  onClick={() => setActiveRoot(i)}
                  className={`w-8 h-8 rounded-full border text-[10px] transition-all flex items-center justify-center ${
                    activeRoot === i ? 'bg-cyan-500 text-black border-cyan-400 shadow-[0_0_10px_rgba(6,182,212,0.5)]' : 'border-white/10 hover:border-white/30 text-slate-500'
                  }`}
                >
                  {i}
                </button>
              ))}
            </div>
            <p className="text-[10px] text-slate-600 mt-3 leading-tight italic">
              Identity anchor point. Each point represents a deterministic trust origin.
            </p>
          </section>

          {/* Level 2: Register Logic Branches */}
          <section className="p-5 rounded-xl bg-slate-900/50 border border-white/10">
            <h2 className="text-xs font-bold text-slate-500 uppercase mb-4 flex items-center gap-2">
              <GitBranch className="w-4 h-4 text-amber-500" />
              Level 2: 8-State Branching
            </h2>
            <div className="grid grid-cols-4 gap-2">
              {[...Array(8)].map((_, i) => (
                <button
                  key={i}
                  onClick={() => setActiveBranch(i)}
                  className={`py-3 rounded border text-[10px] transition-all ${
                    activeBranch === i ? 'bg-amber-500/10 border-amber-500 text-amber-400' : 'border-white/5 bg-black/20 text-slate-600'
                  }`}
                >
                  {i.toString(2).padStart(3, '0')}
                </button>
              ))}
            </div>
            <p className="text-[10px] text-slate-600 mt-3 italic leading-tight">
              Logical branching state (Read/Write/Verify instructions).
            </p>
          </section>

          {/* Verification Ledger */}
          <section className="p-5 rounded-xl bg-slate-900/50 border border-white/10">
            <h2 className="text-xs font-bold text-slate-500 uppercase mb-4 flex items-center gap-2">
              <ShieldCheck className="w-4 h-4 text-emerald-500" />
              Attribution Ledger
            </h2>
            <div className="space-y-2">
              {history.map(item => (
                <div key={item.id} className="flex items-center justify-between bg-black/40 p-2 rounded border-l-2 border-emerald-500 text-[10px]">
                  <span className="text-slate-500">{item.timestamp}</span>
                  <span className="text-emerald-400 font-bold">{item.hash}</span>
                  <span className="bg-emerald-500/10 text-emerald-500 px-1 rounded">OK</span>
                </div>
              ))}
              {history.length === 0 && <p className="text-[10px] text-slate-700 italic">No hashes verified yet...</p>}
            </div>
          </section>
        </div>

        {/* Center/Right: Tree Visualization */}
        <div className="lg:col-span-8 flex flex-col gap-6">
          
          <div className="flex-1 bg-black/30 rounded-2xl border border-white/5 relative overflow-hidden flex flex-col items-center justify-center p-8 min-h-[500px]">
            {/* Background Grid */}
            <div className="absolute inset-0 opacity-[0.03]" style={{ backgroundImage: 'linear-gradient(#fff 1px, transparent 1px), linear-gradient(90deg, #fff 1px, transparent 1px)', backgroundSize: '40px 40px' }} />

            {/* Tree Diagram SVG */}
            <svg viewBox="0 0 600 400" className="w-full h-full max-w-2xl overflow-visible drop-shadow-2xl">
              {/* Lines Connecting Levels */}
              <line x1="300" y1="50" x2="300" y2="120" stroke="white" strokeOpacity="0.1" strokeDasharray="4" />
              
              {/* Level 1: Root */}
              <circle cx="300" cy="50" r="20" className="fill-slate-900 stroke-cyan-500 stroke-2" />
              <text x="300" y="55" textAnchor="middle" className="fill-cyan-400 text-[10px] font-bold">F{activeRoot}</text>
              <text x="300" y="25" textAnchor="middle" className="fill-slate-500 text-[8px] uppercase">Identity Root</text>

              {/* Level 2: Branches */}
              {[...Array(8)].map((_, i) => {
                const x = 100 + (i * 57);
                const isActive = activeBranch === i;
                return (
                  <g key={i}>
                    <line x1="300" y1="70" x2={x} y2="130" stroke={isActive ? "#f59e0b" : "#334155"} strokeOpacity={isActive ? "0.6" : "0.2"} />
                    <rect x={x - 15} y="130" width="30" height="20" rx="4" className={`${isActive ? 'fill-amber-500/20 stroke-amber-500' : 'fill-slate-900 stroke-white/10'} stroke-1`} />
                    <text x={x} y="143" textAnchor="middle" className={`text-[8px] ${isActive ? 'fill-amber-400' : 'fill-slate-600'}`}>B{i}</text>
                  </g>
                );
              })}

              {/* Level 3: Symbol (6-bit Hexagram) */}
              <g transform="translate(300, 220)">
                <rect x="-60" y="-40" width="120" height="80" rx="8" className="fill-slate-900/80 stroke-white/10" />
                <text x="0" y="-55" textAnchor="middle" className="fill-slate-500 text-[8px] uppercase font-bold tracking-widest">Hexagram Symbol (64-state)</text>
                {hexagram.map((bit, i) => (
                  <rect 
                    key={i} 
                    x={bit === 1 ? -40 : -40} 
                    y={-30 + (i * 10)} 
                    width={80} 
                    height={4} 
                    className={`${bit === 1 ? 'fill-cyan-500/80 shadow-lg' : 'fill-slate-800'}`}
                    style={{ opacity: bit === 0 ? 0.3 : 1 }}
                  />
                ))}
              </g>

              {/* Level 4: Leaf Layer (240-node Shell) */}
              <g transform="translate(300, 360)">
                <text x="0" y="-20" textAnchor="middle" className="fill-slate-600 text-[8px] uppercase font-bold">Verifiable Leaf Layer (240 Residuals)</text>
                {[...Array(60)].map((_, i) => {
                  const angle = (i / 60) * Math.PI - Math.PI;
                  const x = Math.cos(angle) * 200;
                  const y = Math.sin(angle) * 40;
                  const isHot = i % 4 === 0;
                  return (
                    <circle 
                      key={i} 
                      cx={x} 
                      cy={y} 
                      r={isHot ? 1.5 : 1} 
                      className={isHot ? 'fill-cyan-400 shadow-[0_0_5px_cyan]' : 'fill-slate-800'} 
                    />
                  );
                })}
              </g>
            </svg>
          </div>

          {/* Detailed State Panel */}
          <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
            <div className="bg-slate-900/50 p-4 rounded-xl border border-white/10">
              <div className="flex items-center gap-2 mb-2">
                <Binary className="w-4 h-4 text-cyan-400" />
                <span className="text-[10px] font-bold text-slate-500 uppercase">Binary Derivative</span>
              </div>
              <div className="bg-black/40 p-2 rounded text-xs font-mono text-cyan-500/70 truncate">
                0x{activeRoot.toString(16)}{activeBranch.toString(16)}{parseInt(hexagram.join(''), 2).toString(16)}
              </div>
            </div>

            <div className="bg-slate-900/50 p-4 rounded-xl border border-white/10">
              <div className="flex items-center gap-2 mb-2">
                <Cpu className="w-4 h-4 text-amber-400" />
                <span className="text-[10px] font-bold text-slate-500 uppercase">Trinary Logic</span>
              </div>
              <div className="flex gap-1">
                {hexagram.map((bit, i) => (
                  <div key={i} className={`h-1 flex-1 rounded-full ${bit === 1 ? 'bg-amber-500' : 'bg-slate-800'}`} />
                ))}
              </div>
            </div>

            <div className="bg-slate-900/50 p-4 rounded-xl border border-white/10">
              <div className="flex items-center gap-2 mb-2">
                <Activity className="w-4 h-4 text-emerald-400" />
                <span className="text-[10px] font-bold text-slate-500 uppercase">Lattice Density</span>
              </div>
              <div className="text-xs text-slate-400">
                240 Shell / 6 Sectors
              </div>
            </div>
          </div>

        </div>
      </div>

      {/* Manual Hexagram Interaction Overlay */}
      <div className="fixed bottom-6 left-1/2 -translate-x-1/2 flex items-center gap-2 bg-slate-900/90 border border-white/10 p-3 rounded-full backdrop-blur-xl shadow-2xl z-50">
        <span className="text-[9px] text-slate-500 mr-2 font-bold px-3">BIT-FLIP SYMBOL:</span>
        {hexagram.map((bit, i) => (
          <button
            key={i}
            onClick={() => {
              const next = [...hexagram];
              next[i] = next[i] === 1 ? 0 : 1;
              setHexagram(next);
            }}
            className={`w-10 h-3 rounded-sm transition-all ${bit === 1 ? 'bg-cyan-500' : 'bg-slate-700 hover:bg-slate-600'}`}
          />
        ))}
      </div>
    </div>
  );
};

export default App;