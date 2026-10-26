import React, { useState, useEffect, useRef, useMemo } from 'react';
import * as THREE from 'three';
import { 
  Plus, Zap, Hash, Database, Layers, 
  Terminal, Share2, Hexagon, Search,
  Activity, Clock, Radio, Cpu, Move3d,
  Network, HardDrive, Layout, Box
} from 'lucide-react';

// --- Configuration ---
const RINGS_241 = [1, 8, 12, 16, 24, 32, 40, 48, 60];
const TICKS_PER_CYCLE = 12; 
const SABBATH_REST_PHASE = 11;

// Mock SVG Import Data (Coordinates from a hypothetical 2D designer)
const SVG_SCHEMA = [
  { id: 'core-01', x: 100, y: 150, label: 'AUTH_KERNEL', type: 'KERNEL' },
  { id: 'data-02', x: 300, y: 120, label: 'LEDGER_SHARD', type: 'STORAGE' },
  { id: 'net-03', x: 200, y: 350, label: 'ROUTING_TABLE', type: 'NETWORK' },
  { id: 'sec-04', x: 50, y: 300, label: 'ENCLAVE_GATE', type: 'SECURITY' },
  { id: 'io-05', x: 350, y: 320, label: 'STREAM_BUFFER', type: 'INTERFACE' },
];

// Helper: Calculate "Semantic Weight" from HDNode bits
const calculateSemanticWeight = (vector) => {
  const flatBits = vector.flat();
  const activeCount = flatBits.reduce((a, b) => a + b, 0);
  return (activeCount / flatBits.length) * 200 - 100; // Map density to Z-depth
};

const FanoScrubber = ({ colors, angle, onRotate, size = 100 }) => {
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
    <div className="relative cursor-crosshair touch-none select-none flex justify-center py-4 bg-zinc-900/40 rounded-3xl mb-2">
      <svg 
        ref={svgRef} width={size} height={size * 0.86} viewBox="0 0 178 154"
        onMouseDown={() => setIsDragging(true)} onMouseUp={() => setIsDragging(false)}
        onMouseMove={handleMove} onTouchStart={() => setIsDragging(true)}
        onTouchEnd={() => setIsDragging(false)} onTouchMove={handleMove}
      >
        <g transform={`rotate(${angle}, 89, 77)`}>
          <path d="M 89 1 L 176 151 L 3 151 Z" fill="none" stroke="#444" strokeWidth="2" strokeDasharray="4 2" />
          {points.map((p, i) => (
            <circle key={i} cx={p.cx} cy={p.cy} r="10" fill={colors[i] || '#111'} className="stroke-zinc-800" />
          ))}
        </g>
      </svg>
    </div>
  );
};

const App = () => {
  const [nodes, setNodes] = useState([]);
  const [selectedId, setSelectedId] = useState(null);
  const [globalTick, setGlobalTick] = useState(0);
  const [viewMode, setViewMode] = useState('SEMANTIC'); // 'ARCHITECTURAL' (2D) or 'SEMANTIC' (3D)

  const mountRef = useRef(null);
  const sceneRef = useRef(null);
  const meshesRef = useRef({});
  const shaderMatRef = useRef(null);

  // --- Core Shader: Cumulative Network Density ---
  const vertexShader = `
    varying vec2 vUv;
    varying vec3 vNormal;
    varying vec3 vWorldPosition;
    void main() {
      vUv = uv;
      vNormal = normalize(normalMatrix * normal);
      vec4 worldPos = modelMatrix * vec4(position, 1.0);
      vWorldPosition = worldPos.xyz;
      gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
    }
  `;

  const fragmentShader = `
    varying vec2 vUv;
    varying vec3 vNormal;
    varying vec3 vWorldPosition;
    uniform float uTime;
    uniform float uSabbath;
    uniform vec3 uNodePositions[10];
    uniform float uNodeActivity[10];
    uniform vec3 uNodeColors[10];

    void main() {
      vec3 finalColor = vec3(0.02, 0.02, 0.05);
      float totalHeat = 0.0;

      if (uSabbath > 0.5) {
        float pulse = sin(uTime * 2.0) * 0.5 + 0.5;
        finalColor = mix(finalColor, vec3(0.15, 0.08, 0.02), pulse * 0.3);
      } else {
        for(int i = 0; i < 10; i++) {
          float d = distance(normalize(vWorldPosition), uNodePositions[i]);
          float influence = smoothstep(0.6, 0.0, d);
          float act = uNodeActivity[i];
          totalHeat += influence * act;
          finalColor += uNodeColors[i] * influence * act * 0.6;
        }
      }

      // Fresnel glow
      float fresnel = pow(1.0 - max(0.0, dot(vNormal, vec3(0,0,1))), 3.0);
      finalColor += vec3(0.2, 0.4, 1.0) * fresnel * 0.4;

      gl_FragColor = vec4(finalColor, 0.85);
    }
  `;

  useEffect(() => {
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x010103);
    sceneRef.current = scene;

    const camera = new THREE.PerspectiveCamera(45, window.innerWidth / window.innerHeight, 0.1, 2000);
    camera.position.set(0, 300, 600);
    
    const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    renderer.setSize(window.innerWidth, window.innerHeight);
    mountRef.current.appendChild(renderer.domElement);

    // Feature Closure (The Brain Core)
    const closureGeo = new THREE.IcosahedronGeometry(130, 4);
    const closureMat = new THREE.ShaderMaterial({
      uniforms: {
        uTime: { value: 0 },
        uSabbath: { value: 0 },
        uNodePositions: { value: Array(10).fill(new THREE.Vector3()) },
        uNodeActivity: { value: Array(10).fill(0) },
        uNodeColors: { value: Array(10).fill(new THREE.Color()) }
      },
      vertexShader,
      fragmentShader,
      transparent: true,
      side: THREE.DoubleSide
    });
    shaderMatRef.current = closureMat;
    const closure = new THREE.Mesh(closureGeo, closureMat);
    scene.add(closure);

    const ambient = new THREE.AmbientLight(0xffffff, 0.2);
    scene.add(ambient);

    // Initial Node Population
    const initialNodes = SVG_SCHEMA.map((s, idx) => {
      const vector = RINGS_241.map(c => Array.from({length: c}, () => Math.random() > 0.5 ? 1 : 0));
      const zOffset = calculateSemanticWeight(vector);
      
      return {
        ...s,
        ref: `inode://${s.id}`,
        vector,
        pos2D: [s.x - 200, -(s.y - 200)], // Center the SVG coordinates
        pos3D: [ (s.x / 400) * 400 - 200, -( (s.y / 400) * 400 - 200 ), zOffset],
        hue: (idx * 72) % 360,
        angle: 0,
        activity: 0
      };
    });
    setNodes(initialNodes);

    const animate = () => {
      requestAnimationFrame(animate);
      const time = Date.now() * 0.001;

      if (shaderMatRef.current) {
        shaderMatRef.current.uniforms.uTime.value = time;
        shaderMatRef.current.uniforms.uSabbath.value = globalTick === SABBATH_REST_PHASE ? 1 : 0;
      }

      nodes.forEach((n, i) => {
        let mesh = meshesRef.current[n.id];
        if (!mesh) {
          const geo = new THREE.OctahedronGeometry(10, 0);
          const mat = new THREE.MeshPhongMaterial({ color: new THREE.Color().setHSL(n.hue/360, 0.8, 0.5), emissive: new THREE.Color().setHSL(n.hue/360, 0.8, 0.2) });
          mesh = new THREE.Mesh(geo, mat);
          scene.add(mesh);
          meshesRef.current[n.id] = mesh;
        }

        // Morphing Logic: 2D Plane vs 3D Semantic Space
        const targetX = viewMode === 'ARCHITECTURAL' ? n.pos2D[0] : n.pos3D[0];
        const targetY = viewMode === 'ARCHITECTURAL' ? n.pos2D[1] : n.pos3D[1];
        const targetZ = viewMode === 'ARCHITECTURAL' ? 0 : n.pos3D[2];

        mesh.position.lerp(new THREE.Vector3(targetX, targetY, targetZ), 0.08);
        mesh.rotation.y += 0.02;

        // Sync Shader
        if (shaderMatRef.current && i < 10) {
          const normalizedPos = mesh.position.clone().normalize();
          shaderMatRef.current.uniforms.uNodePositions.value[i] = normalizedPos;
          const bit = n.vector[6][(globalTick + Math.floor(n.angle/30)) % n.vector[6].length];
          shaderMatRef.current.uniforms.uNodeActivity.value[i] = bit ? 1.0 : 0.1;
          shaderMatRef.current.uniforms.uNodeColors.value[i] = new THREE.Color().setHSL(n.hue/360, 0.8, 0.5);
        }
      });

      renderer.render(scene, camera);
    };
    animate();

    return () => {
      renderer.dispose();
      mountRef.current?.removeChild(renderer.domElement);
    };
  }, [nodes, viewMode, globalTick]);

  // Synchronous Logic
  useEffect(() => {
    const timer = setInterval(() => setGlobalTick(t => (t + 1) % TICKS_PER_CYCLE), 1000);
    return () => clearInterval(timer);
  }, []);

  const selectedNode = nodes.find(n => n.id === selectedId);

  return (
    <div className="w-full h-screen bg-[#010103] text-zinc-500 font-mono flex overflow-hidden">
      {/* Feature Map Controls */}
      <aside className="w-80 h-full border-r border-zinc-900 bg-black/40 backdrop-blur-3xl z-10 flex flex-col p-6 overflow-hidden">
        <div className="flex items-center gap-4 mb-8">
          <div className="p-3 bg-indigo-500/10 border border-indigo-500/20 rounded-2xl">
            <Cpu size={20} className="text-indigo-400" />
          </div>
          <div>
            <h1 className="text-[10px] font-black text-white tracking-[0.3em] uppercase">Brain_Layer_RT</h1>
            <div className="text-[8px] text-zinc-600 font-bold uppercase mt-1 flex items-center gap-2">
               <div className={`w-1.5 h-1.5 rounded-full animate-pulse ${globalTick === SABBATH_REST_PHASE ? 'bg-orange-500' : 'bg-green-500'}`} />
               Status: ACTIVE_EMBED
            </div>
          </div>
        </div>

        {/* View Mode Switcher */}
        <div className="grid grid-cols-2 gap-2 mb-8 p-1 bg-zinc-900/40 rounded-2xl">
           <button 
            onClick={() => setViewMode('ARCHITECTURAL')}
            className={`flex items-center justify-center gap-2 py-2 rounded-xl text-[9px] font-black transition-all ${viewMode === 'ARCHITECTURAL' ? 'bg-indigo-600 text-white shadow-lg' : 'text-zinc-500 hover:text-zinc-300'}`}
           >
             <Layout size={12} /> ARCH
           </button>
           <button 
            onClick={() => setViewMode('SEMANTIC')}
            className={`flex items-center justify-center gap-2 py-2 rounded-xl text-[9px] font-black transition-all ${viewMode === 'SEMANTIC' ? 'bg-indigo-600 text-white shadow-lg' : 'text-zinc-500 hover:text-zinc-300'}`}
           >
             <Box size={12} /> SEMANTIC
           </button>
        </div>

        {/* iNode Registry */}
        <div className="flex-1 space-y-3 overflow-y-auto custom-scrollbar pr-2">
          <div className="text-[8px] font-black text-zinc-600 uppercase tracking-widest px-1 mb-2">Network_Registry</div>
          {nodes.map(node => (
            <div 
              key={node.id}
              onClick={() => setSelectedId(node.id)}
              className={`p-4 rounded-3xl border transition-all cursor-pointer ${selectedId === node.id ? 'bg-indigo-600/10 border-indigo-500/30 ring-1 ring-indigo-500/20' : 'bg-zinc-900/20 border-zinc-900 hover:border-zinc-800'}`}
            >
              <div className="flex justify-between items-center mb-1">
                <span className="text-[10px] font-black text-white tracking-tighter uppercase">{node.label}</span>
                <span className="text-[8px] text-zinc-700 font-bold">{node.type}</span>
              </div>
              <div className="text-[7px] text-zinc-700 font-mono truncate mb-2">{node.ref}</div>
              
              {selectedId === node.id && (
                <div className="animate-in fade-in zoom-in-95 duration-300 pt-2">
                  <FanoScrubber 
                    colors={node.vector[2].slice(0, 7).map(b => b ? `hsl(${node.hue}, 80%, 45%)` : '#0a0a0a')}
                    angle={node.angle}
                    onRotate={(a) => {
                      setNodes(prev => prev.map(n => n.id === node.id ? { ...n, angle: a, hue: a } : n));
                    }}
                  />
                  <div className="flex justify-between items-center text-[8px] font-black text-zinc-600 uppercase px-1">
                    <span>Feature_Vector</span>
                    <span className="text-indigo-500">{(node.angle/3.6).toFixed(0)}% Shift</span>
                  </div>
                </div>
              )}
            </div>
          ))}
        </div>
      </aside>

      <main className="flex-1 relative overflow-hidden">
        <div ref={mountRef} className="w-full h-full" />

        {/* Global HUD Overlay */}
        <div className="absolute top-10 left-10 pointer-events-none space-y-4">
          <div className="flex items-center gap-4 text-white text-[12px] font-black tracking-[0.5em] uppercase">
            <Activity size={18} className="text-indigo-500" />
            Integrated_Closure_Runtime
          </div>
          <div className="flex gap-4">
            <div className="px-4 py-2 bg-black/60 border border-zinc-800 rounded-2xl flex items-center gap-3">
              <div className="text-[8px] text-zinc-500 uppercase font-black">Tick_Phase</div>
              <div className="flex gap-1">
                {Array.from({length: 12}).map((_, i) => (
                  <div key={i} className={`w-1 h-3 rounded-full transition-all ${i === globalTick ? 'bg-indigo-500' : 'bg-zinc-900'}`} />
                ))}
              </div>
            </div>
          </div>
        </div>

        {/* Semantic Inspector Pane */}
        {selectedNode && (
          <div className="absolute bottom-10 right-10 w-72 space-y-4 animate-in slide-in-from-right-8 duration-500">
             <div className="bg-black/80 backdrop-blur-2xl border border-zinc-900 p-6 rounded-[2.5rem] shadow-2xl">
               <div className="flex items-center gap-3 mb-6">
                 <div className="w-3 h-3 rounded-full animate-ping" style={{ backgroundColor: `hsl(${selectedNode.hue}, 80%, 50%)` }} />
                 <div className="text-[10px] font-black text-white uppercase tracking-widest">{selectedNode.label}</div>
               </div>
               
               <div className="space-y-6">
                 <div>
                   <div className="flex justify-between text-[8px] text-zinc-600 font-black uppercase mb-2">
                     <span>Semantic_Depth (Z)</span>
                     <span className="text-white">{selectedNode.pos3D[2].toFixed(1)}u</span>
                   </div>
                   <div className="h-1 w-full bg-zinc-900 rounded-full overflow-hidden">
                      <div 
                        className="h-full bg-indigo-500 transition-all duration-500" 
                        style={{ width: `${((selectedNode.pos3D[2] + 100) / 200) * 100}%` }}
                      />
                   </div>
                 </div>

                 <div>
                   <div className="text-[8px] text-zinc-600 font-black uppercase mb-2">HDNode_Pulse_Stream</div>
                   <div className="grid grid-cols-12 gap-1">
                     {selectedNode.vector[3].slice(0, 36).map((b, i) => (
                       <div key={i} className={`h-4 rounded-sm transition-all ${b ? 'bg-indigo-500/80 shadow-[0_0_8px_rgba(99,102,241,0.3)]' : 'bg-zinc-900'}`} />
                     ))}
                   </div>
                 </div>

                 <div className="pt-4 border-t border-zinc-900 flex justify-between items-center">
                    <div className="text-[8px] text-zinc-600 font-black uppercase">Ref_Link</div>
                    <div className="px-2 py-1 bg-zinc-900 rounded-lg text-[8px] text-zinc-400 font-bold">{selectedNode.ref}</div>
                 </div>
               </div>
             </div>
          </div>
        )}

        {/* Mode Toast */}
        <div className="absolute bottom-10 left-10 px-6 py-3 bg-indigo-600/10 border border-indigo-500/20 rounded-full text-[9px] font-black text-indigo-400 uppercase tracking-widest backdrop-blur-md">
          VIEW_MODE: <span className="text-white">{viewMode}</span>
        </div>
      </main>

      <style>{`
        .custom-scrollbar::-webkit-scrollbar { width: 4px; }
        .custom-scrollbar::-webkit-scrollbar-track { background: transparent; }
        .custom-scrollbar::-webkit-scrollbar-thumb { background: #222; border-radius: 10px; }
      `}</style>
    </div>
  );
};

export default App;