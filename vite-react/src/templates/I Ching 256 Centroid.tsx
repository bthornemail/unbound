import React, { useState, useMemo, useEffect, useRef } from 'react';

const App = () => {
  const GRID_SIZE = 16;
  const P_SIZE = GRID_SIZE * GRID_SIZE; // 256 Address Space
  
  const [params, setParams] = useState({
    level: 6,       
    innerRadius: 100, 
    rotation: -Math.PI / 2,
    lineSpacing: 12, // Increased for individual focus
    morph: 0,      
    hue: 190,      
    hexIndex: 0    // Which specific I Ching symbol to render
  });

  const [activeLED, setActiveLED] = useState({ x: 0, y: 0, id: 0 });

  // 1. GENERATE THE 256 LED ADDRESS SPACE
  const ledPoints = useMemo(() => {
    const points = [];
    const spacing = 22;
    const offset = (GRID_SIZE - 1) * spacing / 2;
    
    for (let y = 0; y < GRID_SIZE; y++) {
      for (let x = 0; x < GRID_SIZE; x++) {
        const id = y * GRID_SIZE + x;
        const gx = 200 + (x * spacing - offset);
        const gy = 200 + (y * spacing - offset);
        const angle = (id / P_SIZE) * Math.PI * 2 + params.rotation;
        const cx = 200 + Math.cos(angle) * 175;
        const cy = 200 + Math.sin(angle) * 175;
        points.push({ gx, gy, cx, cy, id });
      }
    }
    return points;
  }, [params.rotation]);

  // 2. GENERATE THE SINGULAR ACTIVE HEXAGRAM
  const activeHexData = useMemo(() => {
    const bits = params.hexIndex.toString(2).padStart(6, '0').split('').reverse().map(Number);
    return {
      id: params.hexIndex,
      bits: bits
    };
  }, [params.hexIndex]);

  // 3. MAP SELECTOR TO GRID (Projection)
  useEffect(() => {
    const targetId = Math.floor((params.hexIndex / 64) * P_SIZE);
    setActiveLED(ledPoints.find(p => p.id === targetId) || ledPoints[0]);
  }, [params.hexIndex, ledPoints]);

  const canvasRef = useRef(null);
  
  const drawHexLine = (ctx, x, y, isYang, length, offset, hue) => {
    ctx.save();
    ctx.translate(x, y + offset);
    const w = length;
    const h = 4; // Thicker lines for singular focus
    
    ctx.fillStyle = isYang ? `hsla(${hue}, 100%, 50%, 1)` : `hsla(${(hue + 160) % 360}, 100%, 80%, 0.8)`;
    ctx.shadowBlur = 10;
    ctx.shadowColor = ctx.fillStyle;
    
    if (isYang) {
      ctx.fillRect(-w / 2, -h / 2, w, h);
    } else {
      const gap = w * 0.2;
      const segment = (w - gap) / 2;
      ctx.fillRect(-w / 2, -h / 2, segment, h);
      ctx.fillRect(w / 2 - segment, -h / 2, segment, h);
    }
    ctx.restore();
  };

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    let animId;

    const render = () => {
      ctx.clearRect(0, 0, 400, 400);

      // Draw Morphing LEDs
      ledPoints.forEach((led) => {
        const isActive = led.id === activeLED.id;
        const px = led.gx + (led.cx - led.gx) * params.morph;
        const py = led.gy + (led.cy - led.gy) * params.morph;
        
        const distFromCenter = Math.sqrt((px - 200)**2 + (py - 200)**2);
        
        if (distFromCenter < 115) {
            ctx.globalAlpha = 0.05;
            ctx.fillStyle = '#222';
        } else {
            ctx.globalAlpha = isActive ? 1 : 0.15;
            ctx.fillStyle = isActive ? `hsla(${params.hue}, 100%, 50%, 1)` : '#444';
        }

        ctx.beginPath();
        ctx.arc(px, py, isActive ? 3.5 : 1.2, 0, Math.PI * 2);
        ctx.fill();
        
        if (isActive) {
          ctx.shadowBlur = 20;
          ctx.shadowColor = `hsla(${params.hue}, 100%, 50%, 1)`;
          ctx.strokeRect(px - 5, py - 5, 10, 10);
          ctx.shadowBlur = 0;
        }
      });

      // Draw Singular Central I Ching Shape
      activeHexData.bits.forEach((bit, i) => {
        // Render from bottom to top
        const offset = (2.5 - i) * params.lineSpacing;
        drawHexLine(ctx, 200, 200, bit === 1, 60, offset, params.hue);
      });

      // Pointer connection
      const targetPX = activeLED.gx + (activeLED.cx - activeLED.gx) * params.morph;
      const targetPY = activeLED.gy + (activeLED.cy - activeLED.gy) * params.morph;
      ctx.beginPath();
      ctx.setLineDash([4, 6]);
      ctx.strokeStyle = `hsla(${params.hue}, 100%, 50%, 0.3)`;
      ctx.moveTo(200, 200);
      ctx.lineTo(targetPX, targetPY);
      ctx.stroke();
      ctx.setLineDash([]);

      animId = requestAnimationFrame(render);
    };

    render();
    return () => cancelAnimationFrame(animId);
  }, [activeHexData, ledPoints, activeLED, params]);

  return (
    <div className="flex items-center justify-center min-h-screen bg-[#020205] text-white p-4 font-mono">
      <div className="relative w-full max-w-4xl aspect-square bg-[#08080c] rounded-[3rem] border border-white/5 shadow-2xl overflow-hidden flex items-center justify-center">
        
        {/* Top Controls Overlay */}
        <div className="absolute top-10 right-10 z-20 w-56 space-y-6 bg-black/60 backdrop-blur-xl p-5 rounded-3xl border border-white/10 shadow-2xl">
          <div className="space-y-3">
            <div className="flex justify-between text-[10px] uppercase font-black text-cyan-400">
              <span>Shape Index</span>
              <span>{params.hexIndex} / 63</span>
            </div>
            <input type="range" min="0" max="63" step="1" value={params.hexIndex} 
              onChange={e => setParams({...params, hexIndex: parseInt(e.target.value)})}
              className="w-full accent-cyan-500 h-1 bg-white/10 rounded-lg appearance-none cursor-pointer" 
            />
          </div>

          <div className="space-y-3">
            <div className="flex justify-between text-[10px] uppercase font-black text-white/40">
              <span>Morph</span>
              <span>{params.morph > 0.5 ? 'POLAR' : 'GRID'}</span>
            </div>
            <input type="range" min="0" max="1" step="0.01" value={params.morph} 
              onChange={e => setParams({...params, morph: parseFloat(e.target.value)})}
              className="w-full accent-white h-1 bg-white/10 rounded-lg appearance-none cursor-pointer" 
            />
          </div>

          <div className="space-y-3">
            <div className="flex justify-between text-[10px] uppercase font-black text-white/40">
              <span>Theme</span>
              <span style={{color: `hsl(${params.hue}, 100%, 50%)`}}>●</span>
            </div>
            <input type="range" min="0" max="360" value={params.hue} 
              onChange={e => setParams({...params, hue: parseInt(e.target.value)})}
              className="w-full h-1 rounded-lg appearance-none cursor-pointer" 
              style={{ background: 'linear-gradient(to right, #ff0000, #ffff00, #00ff00, #00ffff, #0000ff, #ff00ff, #ff0000)' }}
            />
          </div>
        </div>

        {/* Central I Ching Card Effect */}
        <div className="absolute w-[220px] h-[220px] rounded-[3rem] bg-[#0d0d15]/95 backdrop-blur-3xl border border-white/10 shadow-[0_0_80px_rgba(0,0,0,1)] z-0 rotate-45" />
        
        <canvas 
          ref={canvasRef} 
          width="400" 
          height="400" 
          className="relative z-10 w-full h-full"
        />

        {/* Binary / Hex Metadata Footer */}
        <div className="absolute bottom-10 left-1/2 -translate-x-1/2 flex items-center gap-8 bg-black/80 px-8 py-4 rounded-full border border-white/10 backdrop-blur-md shadow-2xl">
           <div className="flex flex-col items-center">
             <span className="text-[8px] text-white/30 uppercase mb-1 tracking-tighter">Hexagram Binary</span>
             <span className="text-sm font-black text-fuchsia-500 tracking-widest">{activeHexData.bits.join('')}</span>
           </div>
           <div className="w-[1px] h-8 bg-white/10" />
           <div className="flex flex-col items-center">
             <span className="text-[8px] text-white/30 uppercase mb-1 tracking-tighter">LED Pointer</span>
             <span className="text-sm font-black text-cyan-400 tracking-widest">0x{activeLED.id.toString(16).toUpperCase().padStart(2, '0')}</span>
           </div>
        </div>

        {/* Subtle Identity Label */}
        <div className="absolute top-10 left-10">
           <h2 className="text-xl font-black italic text-white/10 uppercase tracking-tighter">Shao Yong<br/>Engine</h2>
        </div>
      </div>
    </div>
  );
};

export default App;