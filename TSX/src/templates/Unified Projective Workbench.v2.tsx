import React, { useState, useEffect, useRef } from 'react';
import { 
  Settings, Play, Shield, Layers, Plus, Trash2, 
  Activity, Terminal, Cpu, Save, RotateCcw, Upload,
  Key, FileText, ChevronDown, ChevronUp
} from 'lucide-react';

/**
 * UNIFIED PROJECTIVE WORKBENCH V4
 * -------------------------------------
 * Features:
 * - File Upload / Stream Input
 * - Mnemonic State Lock (Simulation)
 * - Dotfile Geometry Geometry (.schema, .genesis, etc.)
 * - Church-Encoded Functional Library (λ-transforms)
 * - Epistemological Quadrant View
 */

const App = () => {
  // --- Core State ---
  const [setupOpen, setSetupOpen] = useState(true);
  const [rawInput, setRawInput] = useState("");
  const [mnemonic, setMnemonic] = useState("");
  const [activeFunctionIds, setActiveFunctionIds] = useState([]);
  const fileInputRef = useRef(null);

  const [functionLibrary, setFunctionLibrary] = useState(() => {
    const saved = localStorage.getItem('proj_library_v4');
    return saved ? JSON.parse(saved) : [
      { id: 'f1', name: 'Church: User-to-Member', logic: 'role:user -> role:member' },
      { id: 'f2', name: 'Identity: Anonymize', logic: 'name: -> id:hidden_' }
    ];
  });

  const [dotfiles, setDotfiles] = useState(() => {
    const saved = localStorage.getItem('proj_dotfiles_v4');
    return saved ? JSON.parse(saved) : {
      env: "MODE=STRICT",
      include: "status:active",
      ignore: "VOID",
      schema: "id:",
      sequence: "id,name,role",
      genesis: "role:admin -> access:overseer",
      atom: "status:active -> state:alive",
      manifest: "FORMAT=UPPERCASE"
    };
  });

  const [quadrants, setQuadrants] = useState({ kk: [], ku: [], uk: [], uu: [] });

  // --- Persistence ---
  useEffect(() => {
    localStorage.setItem('proj_dotfiles_v4', JSON.stringify(dotfiles));
    localStorage.setItem('proj_library_v4', JSON.stringify(functionLibrary));
  }, [dotfiles, functionLibrary]);

  // --- File Handling ---
  const handleFileUpload = (e) => {
    const file = e.target.files[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = (event) => {
      setRawInput(event.target.result);
    };
    reader.readAsText(file);
  };

  // --- Church Encoding Functions ---
  const addFunction = () => {
    const name = prompt("Function Name (e.g. Folder Map):");
    const logic = prompt("Encoding Pair (find -> replace):");
    if (name && logic) {
      setFunctionLibrary(prev => [...prev, {
        id: Math.random().toString(36).substr(2, 9),
        name,
        logic
      }]);
    }
  };

  const removeFunction = (id) => {
    setFunctionLibrary(prev => prev.filter(f => f.id !== id));
    setActiveFunctionIds(prev => prev.filter(fid => fid !== id));
  };

  const toggleFunction = (id) => {
    setActiveFunctionIds(prev => 
      prev.includes(id) ? prev.filter(fid => fid !== id) : [...prev, id]
    );
  };

  // --- The Projective Resolver ---
  const resolve = () => {
    if (!rawInput) return;
    const lines = rawInput.split('\n').filter(l => l.trim());
    const results = { kk: [], ku: [], uk: [], uu: [] };

    lines.forEach(line => {
      let current = line;
      let stateTrace = { valid: true, transformed: false, filtered: false };

      // 1. .schema check
      if (dotfiles.schema && !current.includes(dotfiles.schema)) {
        results.uu.push(current);
        return;
      }

      // 2. .include / .ignore
      if (dotfiles.include && !current.includes(dotfiles.include)) stateTrace.filtered = true;
      if (dotfiles.ignore && current.includes(dotfiles.ignore)) stateTrace.filtered = true;

      if (stateTrace.filtered) {
        results.uk.push(current);
        return;
      }

      // 3. Church Functions (λ)
      activeFunctionIds.forEach(fid => {
        const func = functionLibrary.find(f => f.id === fid);
        if (func && func.logic.includes('->')) {
          const [find, replace] = func.logic.split('->').map(s => s.trim());
          if (current.includes(find)) {
            current = current.split(find).join(replace);
            stateTrace.transformed = true;
          }
        }
      });

      // 4. .genesis / .atom
      [dotfiles.genesis, dotfiles.atom].forEach(transform => {
        if (transform && transform.includes('->')) {
          const [f, r] = transform.split('->').map(s => s.trim());
          if (current.includes(f)) {
            current = current.split(f).join(r);
            stateTrace.transformed = true;
          }
        }
      });

      // 5. .manifest
      if (dotfiles.manifest.includes("UPPERCASE")) current = current.toUpperCase();

      if (stateTrace.transformed) results.ku.push(current);
      else results.kk.push(current);
    });

    setQuadrants(results);
  };

  return (
    <div className="min-h-screen bg-[#050505] text-zinc-400 font-mono text-xs p-4 md:p-6">
      <div className="max-w-7xl mx-auto space-y-6">
        
        {/* Header */}
        <header className="flex justify-between items-center border-b border-zinc-900 pb-6">
          <div className="flex items-center gap-4">
            <div className="p-2 bg-blue-600/10 rounded-lg border border-blue-500/20">
              <Cpu className="text-blue-500" size={20} />
            </div>
            <div>
              <h1 className="text-lg font-bold text-white tracking-widest uppercase">Projective Resolver</h1>
              <p className="text-[9px] text-zinc-600 tracking-[0.2em] font-bold">Functional Invariant Environment</p>
            </div>
          </div>
          
          <div className="flex gap-2">
            <button 
              onClick={() => setSetupOpen(!setupOpen)}
              className={`p-2 rounded border transition-all ${setupOpen ? 'bg-zinc-800 border-zinc-600 text-white' : 'border-zinc-900 text-zinc-600 hover:text-zinc-400'}`}
            >
              <Settings size={18} />
            </button>
            <button 
              onClick={resolve}
              className="px-6 py-2 bg-blue-600 hover:bg-blue-500 text-white font-bold rounded flex items-center gap-2 transition-all shadow-lg shadow-blue-600/20 uppercase tracking-tighter"
            >
              <Play size={14} fill="currentColor" /> Resolve
            </button>
          </div>
        </header>

        {/* Configuration Overlay */}
        {setupOpen && (
          <div className="grid grid-cols-1 lg:grid-cols-12 gap-6 p-6 bg-zinc-900/20 border border-zinc-900 rounded-2xl animate-in fade-in zoom-in duration-300">
            <div className="lg:col-span-8 space-y-6">
              <div className="flex justify-between items-center border-b border-zinc-800 pb-2">
                <h2 className="text-[10px] font-bold uppercase text-zinc-500 flex items-center gap-2"><Shield size={12} /> Geometry Configuration</h2>
                <RotateCcw size={12} className="cursor-pointer hover:text-white" onClick={() => { if(confirm('Reset?')) { localStorage.clear(); window.location.reload(); }}} />
              </div>
              <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
                {Object.keys(dotfiles).map(f => (
                  <div key={f} className="space-y-1">
                    <label className="text-[8px] text-zinc-600 uppercase">.{f}</label>
                    <input 
                      value={dotfiles[f]} 
                      onChange={(e) => setDotfiles({...dotfiles, [f]: e.target.value})}
                      className="w-full bg-black border border-zinc-800 rounded px-2 py-1.5 text-[10px] text-blue-400 outline-none focus:border-zinc-700"
                    />
                  </div>
                ))}
              </div>
              <div className="pt-2">
                <label className="text-[8px] text-zinc-600 uppercase flex items-center gap-1"><Key size={10} /> Mnemonic State Lock</label>
                <input 
                  type="password" 
                  value={mnemonic}
                  onChange={(e) => setMnemonic(e.target.value)}
                  placeholder="Enter mnemonic to secure projection..."
                  className="w-full bg-black border border-zinc-800 rounded px-2 py-2 mt-1 text-xs outline-none focus:border-zinc-700" 
                />
              </div>
            </div>

            <div className="lg:col-span-4 space-y-4">
              <h2 className="text-[10px] font-bold uppercase text-zinc-500 flex items-center gap-2 border-b border-zinc-800 pb-2"><Layers size={12} /> Lambda Library</h2>
              <div className="space-y-2 max-h-40 overflow-y-auto">
                {functionLibrary.map(fn => (
                  <div key={fn.id} className="flex gap-1">
                    <button 
                      onClick={() => toggleFunction(fn.id)}
                      className={`flex-1 text-left px-3 py-2 rounded border text-[10px] truncate transition ${activeFunctionIds.includes(fn.id) ? 'bg-blue-600/10 border-blue-600 text-blue-400' : 'bg-black border-zinc-800 text-zinc-600'}`}
                    >
                      {fn.name}
                    </button>
                    <button onClick={() => removeFunction(fn.id)} className="p-2 text-zinc-800 hover:text-red-500"><Trash2 size={12}/></button>
                  </div>
                ))}
              </div>
              <button onClick={addFunction} className="w-full py-2 border border-dashed border-zinc-800 rounded text-[9px] uppercase hover:bg-zinc-900 transition flex items-center justify-center gap-2"><Plus size={12}/> New Encoding</button>
            </div>
          </div>
        )}

        {/* Input/Output Workspace */}
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-8">
          <div className="space-y-3">
            <div className="flex justify-between items-center">
              <h3 className="text-[10px] font-bold uppercase text-zinc-600 flex items-center gap-2"><Terminal size={12}/> Input Atoms</h3>
              <div className="flex gap-2">
                <input type="file" ref={fileInputRef} onChange={handleFileUpload} className="hidden" />
                <button 
                  onClick={() => fileInputRef.current.click()}
                  className="flex items-center gap-2 px-3 py-1 bg-zinc-900 border border-zinc-800 rounded hover:border-zinc-600 transition"
                >
                  <Upload size={12}/> <span className="text-[10px] uppercase">Upload File</span>
                </button>
                <button onClick={() => setRawInput("")} className="text-[10px] uppercase text-zinc-700 hover:text-zinc-500">Clear</button>
              </div>
            </div>
            <textarea 
              value={rawInput}
              onChange={(e) => setRawInput(e.target.value)}
              className="w-full h-[450px] bg-zinc-900/10 border border-zinc-900 rounded-2xl p-4 text-xs font-mono text-zinc-300 focus:border-zinc-800 outline-none resize-none shadow-inner"
              placeholder="Paste data atoms or upload a file..."
            />
          </div>

          <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
            <Quadrant title="Known Knowns" color="emerald" data={quadrants.kk} status="Invariant" />
            <Quadrant title="Known Unknowns" color="blue" data={quadrants.ku} status="Transformed" />
            <Quadrant title="Unknown Knowns" color="amber" data={quadrants.uk} status="Filtered" strike />
            <Quadrant title="Unknown Unknowns" color="rose" data={quadrants.uu} status="Anomalies" italic />
          </div>
        </div>

        <footer className="pt-4 border-t border-zinc-900 flex justify-between items-center opacity-30 text-[8px] uppercase tracking-[0.4em]">
          <span>Engine: Projective_V4_Stable</span>
          <span>Entropy: {Math.random().toString(16).substr(2, 8)}</span>
          <span>Environment: Ready</span>
        </footer>
      </div>
    </div>
  );
};

const Quadrant = ({ title, color, data, status, strike, italic }) => {
  const styles = {
    emerald: 'border-emerald-900/30 bg-emerald-950/5 text-emerald-500',
    blue: 'border-blue-900/30 bg-blue-950/5 text-blue-500',
    amber: 'border-amber-900/30 bg-amber-950/5 text-amber-500',
    rose: 'border-rose-900/30 bg-rose-950/5 text-rose-500'
  };

  return (
    <div className={`border rounded-2xl p-4 flex flex-col h-[218px] transition ${styles[color]}`}>
      <div className="flex justify-between items-start mb-3">
        <div>
          <h4 className="text-[9px] font-bold uppercase tracking-wider">{title}</h4>
          <p className="text-[7px] opacity-60 uppercase">{status}</p>
        </div>
        <FileText size={12} className="opacity-20" />
      </div>
      <div className="flex-1 overflow-y-auto space-y-1.5 custom-scrollbar pr-1">
        {data.length === 0 && <div className="h-full flex items-center justify-center opacity-10 italic text-[8px] tracking-widest">Null_Set</div>}
        {data.map((l, i) => (
          <div key={i} className={`text-[9px] font-mono truncate py-0.5 border-b border-white/5 last:border-0 ${strike ? 'line-through opacity-30' : 'text-zinc-200'} ${italic ? 'italic opacity-50' : ''}`}>
            {l}
          </div>
        ))}
      </div>
    </div>
  );
};

export default App;