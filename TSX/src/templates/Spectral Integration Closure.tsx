import React, { useState, useEffect, useRef, useMemo } from 'react';
import * as THREE from 'three';
import { 
  Plus, Zap, Hash, Database, Layers, 
  Terminal, Share2, Hexagon, Search,
  BookOpen, Globe, Activity, Clock, Waves,
  Pause, Play, ShieldAlert, Radio
} from 'lucide-react';

// --- Constants ---
const RINGS_241 = [1, 8, 12, 16, 24, 32, 40, 48, 60];
const TICKS_PER_CYCLE = 12; 
const SABBATH_REST_PHASE = 11; 

// --- Custom WordNet Dictionary (Deterministic Seeds) ---
const SUBJECTS = ["QUASAR", "PHANTOM", "ISOTOPE", "MONOLITH", "CYPHER", "RELICT", "ZENITH", "AETHER"];
const PREDICATES = ["PULSE", "RECURSE", "OSCILLATE", "DIVERGE", "TRANSMUTE", "RESONATE", "VOID", "COLLAPSE"];

// --- Fano Scrubber (Phase/Hue Controller) ---
const FanoScrubber = ({ colors, angle, onRotate, size = 120 }) => {
  const [isDragging, setIsDragging] = useState(false);
  const svgRef = useRef(null);
  const points = [{ cx: 89, cy: 101 }, { cx: 46, cy: 76 }, { cx: 132, cy: 75 }, { cx: 3, cy: 151 }, { cx: 89, cy: 151 }, { cx: 175, cy: 151 }, { cx: 90, cy: 3 }];

  const handleMove = (e) => {
    if (!isDragging || !svgRef.current) return;
    const rect = svgRef.current.getBoundingClientRect();
    const cx = rect.left + rect.width / 2;
    const cy = rect.top + rect.height / 2;
    const clientX = e.touches ? e.touches[0].clientX : e.clientX;
    const clientY = e.touches ? e.touches[0].clientY : e.clientY;
    const rad = Math.atan2(clientY - cy, clientX - cx);
    onRotate(((rad * 180 / Math.PI) + 450) % 360);
  };

  return (
    <div className="relative cursor-crosshair touch-none select-none">
      <svg 
        ref={svgRef} width={size} height={size * 0.86} viewBox="0 0 178 154"
        onMouseDown={() => setIsDragging(true)} onMouseUp={() => setIsDragging(false)}
        onMouseMove={handleMove} onTouchStart={() => setIsDragging(true)}
        onTouchEnd={() => setIsDragging(false)} onTouchMove={handleMove}
      >
        <g transform={`rotate(${angle}, 89, 77)`}>
          <path d="M 89 1 L 176 151 L 3 151 Z" fill="none" stroke="#222" strokeWidth="1" />
          {points.map((p, i) => (
            <circle key={i} cx={p.cx} cy={p.cy} r="10" fill={colors[i] || '#050505'} className="stroke-zinc-800 stroke-1" />
          ))}
        </g>
      </svg>
    </div>
  );
};

const App = () => {
  const [nodes, setNodes] = useState([]);
  const [selectedId, setSelectedId] = useState(null);
  const [mnemonicInput, setMnemonicInput] = useState("");
  const [globalTick, setGlobalTick] = useState(0);
  const [isPlaying, setIsPlaying] = useState(true);
  
  const mountRef = useRef(null);
  const sceneRef = useRef(null);
  const meshesRef = useRef({});
  const closureRef = useRef(null);

  // --- Initialize Simulation ---
  useEffect(() => {
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x010103);
    sceneRef.current = scene;

    const camera = new THREE.PerspectiveCamera(60, window.innerWidth / window.innerHeight, 1, 2000);
    camera.position.set(300, 200, 500);
    
    const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    renderer.setSize(window.innerWidth, window.innerHeight);
    renderer.setPixelRatio(window.devicePixelRatio);
    mountRef.current.appendChild(renderer.domElement);

    // --- The Spectral Closure Mesh ---
    const closureGeo = new THREE.IcosahedronGeometry(80, 4);
    const closureMat = new THREE.MeshPhongMaterial({ 
      wireframe: true, 
      color: 0x3b82f6, 
      emissive: 0x1d4ed8, 
      emissiveIntensity: 0.5,
      transparent: true,
      opacity: 0.1
    });
    const closure = new THREE.Mesh(closureGeo, closureMat);
    scene.add(closure);
    closureRef.current = closure;

    const ambient = new THREE.AmbientLight(0xffffff, 0.1);
    scene.add(ambient);
    const point = new THREE.PointLight(0x4f46e5, 2, 1000);
    point.position.set(0, 200, 0);
    scene.add(point);

    const animate = () => {
      requestAnimationFrame(animate);
      
      // Update Closure Appearance based on Active Node Density
      if (closureRef.current) {
        const time = Date.now() * 0.001;
        closureRef.current.rotation.y += 0.002;
        closureRef.current.rotation.z += 0.001;
        
        // Dynamic "Breathing" for the closure
        const isSabbath = globalTick === SABBATH_REST_PHASE;
        const pulseBase = isSabbath ? 0.8 : 1.1;
        const pulse = pulseBase + Math.sin(time * 2) * 0.05;
        closureRef.current.scale.set(pulse, pulse, pulse);
        
        // Summing harmonic inputs for closure emissivity
        const totalDensity = nodes.reduce((sum, n) => {
            const bit = n.rings[6][globalTick % n.rings[6].length];
            return sum + (bit ? 1 : 0);
        }, 0);
        
        closureRef.current.material.emissiveIntensity = isSabbath ? 0.05 : (totalDensity / (nodes.length || 1)) + 0.2;
      }

      // Orbital Physics
      Object.entries(meshesRef.current).forEach(([id, mesh]) => {
        const node = nodes.find(n => n.id === id);
        if (node) {
          const orbitSpeed = 0.0002 * (parseInt(id.slice(0,2), 16) || 1);
          const time = Date.now() * orbitSpeed;
          const radius = 220 + (node.hue * 0.2);
          mesh.position.x = Math.cos(time + node.angle) * radius;
          mesh.position.z = Math.sin(time + node.angle) * radius;
          mesh.rotation.x += 0.01;
          mesh.rotation.y += 0.01;
        }
      });

      renderer.render(scene, camera);
    };
    animate();

    return () => {
      renderer.dispose();
      mountRef.current?.removeChild(renderer.domElement);
    };
  }, [nodes, globalTick]);

  // --- Mnemonic Generator ---
  const spawnMnemonic = async () => {
    if (!mnemonicInput) return;
    const words = mnemonicInput.trim().toUpperCase().split(" ");
    const seedText = words.join("_");
    const hashBuffer = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(seedText));
    const hash = new Uint8Array(hashBuffer);

    const id = hash.slice(0, 4).reduce((acc, v) => acc + v.toString(16), "");
    const hue = hash[0] % 360;

    const rings = RINGS_241.map((count, rIdx) => {
      const bits = [];
      for (let i = 0; i < count; i++) {
        bits.push((hash[(rIdx + i) % hash.length] >> (i % 8)) & 1);
      }
      return bits;
    });

    const newNode = {
      id,
      mnemonic: seedText,
      hue,
      angle: (hash[1] / 255) * Math.PI * 2,
      rings,
      position: [0, (hash[2] - 128), 0] // Orbit radius set in animate
    };

    setNodes(prev => [...prev, newNode]);
    setSelectedId(id);
    setMnemonicInput("");
    initMesh(newNode);
  };

  const initMesh = (node) => {
    if (!sceneRef.current) return;
    const geo = new THREE.OctahedronGeometry(15, 0);
    const mat = new THREE.MeshPhongMaterial({ 
        vertexColors: true, 
        flatShading: true,
        transparent: true,
        opacity: 0.9
    });
    const mesh = new THREE.Mesh(geo, mat);
    sceneRef.current.add(mesh);
    meshesRef.current[node.id] = mesh;
    updateMeshBits(node);
  };

  const updateMeshBits = (node) => {
    const mesh = meshesRef.current[node.id];
    if (!mesh) return;
    const colors = [];
    const color = new THREE.Color();
    const pos = mesh.geometry.attributes.position;
    
    for (let i = 0; i < pos.count; i++) {
      const bit = node.rings[i % 9][i % node.rings[i % 9].length];
      const isSabbath = globalTick === SABBATH_REST_PHASE;
      color.setHSL(node.hue / 360, 0.8, isSabbath ? 0.05 : (bit ? 0.7 : 0.1));
      colors.push(color.r, color.g, color.b);
    }
    mesh.geometry.setAttribute('color', new THREE.Float32BufferAttribute(colors, 3));
  };

  // Sync Cycle
  useEffect(() => {
    if (!isPlaying) return;
    const timer = setInterval(() => setGlobalTick(t => (t + 1) % TICKS_PER_CYCLE), 1000);
    return () => clearInterval(timer);
  }, [isPlaying]);

  useEffect(() => {
    nodes.forEach(updateMeshBits);
  }, [globalTick]);

  const selectedNode = useMemo(() => nodes.find(n => n.id === selectedId), [nodes, selectedId]);

  return (
    <div className="w-full h-screen bg-[#010103] text-zinc-500 font-mono flex overflow-hidden">
      {/* SIDEBAR: HARMONIC CONTROLS */}
      <aside className="w-80 h-full border-r border-zinc-900 bg-black/60 backdrop-blur-3xl z-10 flex flex-col p-6 overflow-y-auto">
        <div className="flex items-center gap-4 mb-10">
          <div className="p-3 bg-indigo-600/10 border border-indigo-500/20 rounded-2xl shadow-[0_0_20px_rgba(79,70,229,0.1)]">
            <Radio size={20} className="text-indigo-400" />
          </div>
          <div>
            <h1 className="text-[10px] font-black text-white tracking-[0.4em] uppercase">Spectral_Modem</h1>
            <div className="text-[8px] text-zinc-600 font-bold uppercase tracking-widest mt-0.5">Integration: DETERMINISTIC</div>
          </div>
        </div>

        {/* Global Cycle Meter */}
        <div className="mb-10 p-5 bg-zinc-900/30 border border-zinc-800 rounded-3xl space-y-4">
          <div className="flex justify-between items-center text-[9px] font-black uppercase tracking-widest">
            <span className="text-zinc-600">Sabbath_Phase</span>
            <span className={globalTick === SABBATH_REST_PHASE ? "text-orange-500 animate-pulse" : "text-indigo-400"}>
              {globalTick === SABBATH_REST_PHASE ? "REST_SEQUENCE" : `CH_${globalTick + 1}`}
            </span>
          </div>
          <div className="flex gap-1.5 h-1">
            {Array.from({ length: TICKS_PER_CYCLE }).map((_, i) => (
              <div 
                key={i} 
                className={`flex-1 rounded-full transition-all duration-500 ${
                  i === globalTick ? 'bg-indigo-500 scale-y-[3] shadow-[0_0_12px_rgba(99,102,241,0.6)]' : 'bg-zinc-800'
                }`}
              />
            ))}
          </div>
        </div>

        {/* Mnemonic Entrance */}
        <div className="space-y-4 mb-10">
          <div className="flex items-center gap-2 text-[9px] font-black uppercase text-zinc-600">
            <Hash size={12} /> Inject_Mnemonic
          </div>
          <div className="flex gap-2">
            <input 
              value={mnemonicInput}
              onChange={(e) => setMnemonicInput(e.target.value)}
              placeholder="QUASAR PULSE..."
              className="flex-1 bg-black border border-zinc-800 rounded-2xl px-5 py-3.5 text-[10px] text-white focus:border-indigo-500 outline-none placeholder:text-zinc-800 transition-all"
              onKeyDown={(e) => e.key === 'Enter' && spawnMnemonic()}
            />
            <button 
                onClick={spawnMnemonic} 
                className="p-3 bg-indigo-600 text-white rounded-2xl hover:scale-105 active:scale-95 transition-all shadow-lg"
            >
              <Plus size={20} />
            </button>
          </div>
        </div>

        {/* Node Library */}
        <div className="flex-1 space-y-3">
          <span className="text-[9px] font-black uppercase text-zinc-600 tracking-widest block mb-4">Spectral_Inventory</span>
          {nodes.map(node => (
            <div 
              key={node.id}
              onClick={() => setSelectedId(node.id)}
              className={`p-5 rounded-3xl border transition-all cursor-pointer group ${selectedId === node.id ? 'bg-indigo-600/10 border-indigo-500/40 shadow-inner' : 'bg-zinc-900/20 border-zinc-900 hover:border-zinc-700'}`}
            >
              <div className="flex justify-between items-center mb-4">
                <span className="text-[10px] font-black text-white uppercase group-hover:text-indigo-400 transition-colors tracking-tighter">{node.mnemonic}</span>
                <div className="w-1.5 h-1.5 rounded-full" style={{ backgroundColor: `hsl(${node.hue}, 80%, 60%)` }} />
              </div>
              
              {selectedId === node.id && (
                <div className="pt-2 animate-in fade-in slide-in-from-top-2 flex flex-col items-center">
                   <FanoScrubber 
                      colors={node.rings[2].slice(0, 7).map(b => b ? `hsl(${node.hue}, 80%, 50%)` : '#080808')}
                      angle={node.angle * (180/Math.PI)}
                      onRotate={(a) => {
                        const updated = { ...node, angle: (a * Math.PI / 180), hue: a };
                        setNodes(prev => prev.map(n => n.id === node.id ? updated : n));
                      }}
                   />
                </div>
              )}
            </div>
          ))}
        </div>
      </aside>

      {/* VIEWPORT: THE CLOSURE */}
      <main className="flex-1 relative overflow-hidden">
        <div ref={mountRef} className="w-full h-full" />
        
        {/* HUD Data Plane */}
        <div className="absolute top-10 left-10 pointer-events-none">
          <div className="flex items-center gap-4 text-[12px] font-black text-white tracking-[0.5em] uppercase mb-4">
            <Activity size={18} className="text-indigo-500 animate-pulse" />
            Integration_Field_v1.0
          </div>
          <div className="h-[2px] w-64 bg-gradient-to-r from-indigo-500 via-indigo-500/20 to-transparent" />
        </div>

        {/* Spectral Density Visualization */}
        <div className="absolute bottom-10 right-10 p-6 bg-black/60 backdrop-blur-xl border border-zinc-900 rounded-[2.5rem] pointer-events-none">
            <div className="text-[9px] font-black text-zinc-600 uppercase tracking-widest mb-4">Closure_Harmonic_Density</div>
            <div className="flex items-end gap-1.5 h-16 w-48">
              {Array.from({ length: 16 }).map((_, i) => {
                const h = 20 + Math.random() * 60 + (globalTick === SABBATH_REST_PHASE ? -40 : 0);
                return <div key={i} className="flex-1 bg-indigo-500/20 rounded-t-lg transition-all duration-500" style={{ height: `${Math.max(10, h)}%` }} />;
              })}
            </div>
        </div>

        {selectedNode && (
          <div className="absolute top-1/2 right-10 -translate-y-1/2 flex flex-col gap-6 pointer-events-none">
             <div className="bg-black/80 border border-zinc-900 p-6 rounded-3xl animate-in slide-in-from-right-8">
                <div className="text-[8px] font-black text-zinc-600 uppercase mb-3">Ring_States [6]</div>
                <div className="grid grid-cols-10 gap-1">
                   {selectedNode.rings[6].slice(0, 40).map((b, i) => (
                      <div key={i} className={`w-2 h-2 rounded-sm ${b ? 'bg-indigo-500 shadow-[0_0_8px_indigo]' : 'bg-zinc-800'}`} />
                   ))}
                </div>
                <div className="mt-6 flex justify-between items-end">
                   <div>
                      <div className="text-[8px] font-black text-zinc-600 uppercase">Phase_Angle</div>
                      <div className="text-lg font-black text-white italic">{Math.floor(selectedNode.angle * (180/Math.PI))}°</div>
                   </div>
                   <div className="text-right">
                      <div className="text-[8px] font-black text-zinc-600 uppercase">Synset</div>
                      <div className="text-[10px] text-indigo-400 font-black tracking-widest">{selectedNode.mnemonic}</div>
                   </div>
                </div>
             </div>
          </div>
        )}
      </main>
    </div>
  );
};

export default App;