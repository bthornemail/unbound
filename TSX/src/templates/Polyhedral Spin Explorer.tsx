import React, { useState, useEffect } from 'react';
import { Card, CardContent, CardHeader, CardTitle } from '../components/ui/card';

type PolyStructure = {
  vertices: number;
  edges: number;
  faces: number;
  p: number;
  q: number;
  betti: [number, number, number];
  name: string;
};

function universalSpin(struct: PolyStructure): number {
  const { vertices: V, edges: E, faces: F, p, q, betti } = struct;
  
  const eulerChi = V - E + F;
  const bettiChi = betti[0] - betti[1] + betti[2];
  
  const symmetry = (p * q) / (V + F);
  const growthHarmonic = (E * (E + 1) * (E + 2)) / 6;
  const delta = eulerChi - bettiChi;
  
  return Math.tan(Math.PI * symmetry) * Math.sign(delta) * Math.log(growthHarmonic);
}

const platonicSolids: PolyStructure[] = [
  { vertices: 4, edges: 6, faces: 4, p: 3, q: 3, betti: [1, 0, 1], name: "Tetrahedron" },
  { vertices: 8, edges: 12, faces: 6, p: 4, q: 3, betti: [1, 0, 1], name: "Cube" },
  { vertices: 6, edges: 12, faces: 8, p: 3, q: 4, betti: [1, 0, 1], name: "Octahedron" },
  { vertices: 20, edges: 30, faces: 12, p: 5, q: 3, betti: [1, 0, 1], name: "Dodecahedron" },
  { vertices: 12, edges: 30, faces: 20, p: 3, q: 5, betti: [1, 0, 1], name: "Icosahedron" }
];

const archimedeanSolids: PolyStructure[] = [
  { vertices: 24, edges: 36, faces: 14, p: 4, q: 3, betti: [1, 0, 1], name: "Truncated Tetrahedron" },
  { vertices: 12, edges: 18, faces: 8, p: 3, q: 3, betti: [1, 0, 1], name: "Cuboctahedron" },
  { vertices: 24, edges: 36, faces: 14, p: 6, q: 3, betti: [1, 0, 1], name: "Truncated Cube" },
];

function PolyCard({ poly }: { poly: PolyStructure }) {
  const spin = universalSpin(poly);
  const euler = poly.vertices - poly.edges + poly.faces;
  const bettiSum = poly.betti[0] - poly.betti[1] + poly.betti[2];
  
  return (
    <Card className="mb-4 border-2 hover:border-blue-400 transition-colors">
      <CardHeader>
        <CardTitle className="text-lg font-bold text-blue-800">{poly.name}</CardTitle>
      </CardHeader>
      <CardContent>
        <div className="grid grid-cols-2 gap-4 text-sm">
          <div>
            <div className="font-semibold text-gray-600">Structure</div>
            <div>V: {poly.vertices}</div>
            <div>E: {poly.edges}</div>
            <div>F: {poly.faces}</div>
          </div>
          <div>
            <div className="font-semibold text-gray-600">Parameters</div>
            <div>p: {poly.p}</div>
            <div>q: {poly.q}</div>
            <div>Betti: [{poly.betti.join(', ')}]</div>
          </div>
        </div>
        <div className="mt-4 pt-4 border-t">
          <div className="grid grid-cols-2 gap-4 text-sm">
            <div>
              <div className="font-semibold text-gray-600">Invariants</div>
              <div>Euler χ: {euler}</div>
              <div>Betti χ: {bettiSum}</div>
              <div>Δ: {euler - bettiSum}</div>
            </div>
            <div>
              <div className="font-semibold text-gray-600">Universal Spin</div>
              <div className={`font-bold text-lg ${
                isNaN(spin) ? 'text-red-600' : 
                Math.abs(spin) > 10 ? 'text-purple-600' :
                spin > 0 ? 'text-green-600' : 'text-blue-600'
              }`}>
                {isNaN(spin) ? 'Singular' : 
                 !isFinite(spin) ? '∞' :
                 spin.toFixed(3)}
              </div>
            </div>
          </div>
        </div>
      </CardContent>
    </Card>
  );
}

function CustomPolyInput({ onAdd }: { onAdd: (poly: PolyStructure) => void }) {
  const [custom, setCustom] = useState<PolyStructure>({
    vertices: 8,
    edges: 12,
    faces: 6,
    p: 4,
    q: 3,
    betti: [1, 0, 1],
    name: "Custom Shape"
  });

  return (
    <Card className="mb-6 bg-gray-50">
      <CardHeader>
        <CardTitle className="text-lg">Custom Polyhedron</CardTitle>
      </CardHeader>
      <CardContent>
        <div className="grid grid-cols-3 gap-4 mb-4">
          <div>
            <label className="block text-sm font-medium mb-1">Vertices</label>
            <input 
              type="number" 
              className="w-full p-2 border rounded"
              value={custom.vertices}
              onChange={(e) => setCustom({...custom, vertices: parseInt(e.target.value) || 0})}
            />
          </div>
          <div>
            <label className="block text-sm font-medium mb-1">Edges</label>
            <input 
              type="number" 
              className="w-full p-2 border rounded"
              value={custom.edges}
              onChange={(e) => setCustom({...custom, edges: parseInt(e.target.value) || 0})}
            />
          </div>
          <div>
            <label className="block text-sm font-medium mb-1">Faces</label>
            <input 
              type="number" 
              className="w-full p-2 border rounded"
              value={custom.faces}
              onChange={(e) => setCustom({...custom, faces: parseInt(e.target.value) || 0})}
            />
          </div>
        </div>
        <div className="grid grid-cols-2 gap-4 mb-4">
          <div>
            <label className="block text-sm font-medium mb-1">p (edges per face)</label>
            <input 
              type="number" 
              className="w-full p-2 border rounded"
              value={custom.p}
              onChange={(e) => setCustom({...custom, p: parseInt(e.target.value) || 0})}
            />
          </div>
          <div>
            <label className="block text-sm font-medium mb-1">q (faces per vertex)</label>
            <input 
              type="number" 
              className="w-full p-2 border rounded"
              value={custom.q}
              onChange={(e) => setCustom({...custom, q: parseInt(e.target.value) || 0})}
            />
          </div>
        </div>
        <div className="grid grid-cols-3 gap-4 mb-4">
          <div>
            <label className="block text-sm font-medium mb-1">β₀</label>
            <input 
              type="number" 
              className="w-full p-2 border rounded"
              value={custom.betti[0]}
              onChange={(e) => setCustom({...custom, betti: [parseInt(e.target.value) || 0, custom.betti[1], custom.betti[2]]})}
            />
          </div>
          <div>
            <label className="block text-sm font-medium mb-1">β₁</label>
            <input 
              type="number" 
              className="w-full p-2 border rounded"
              value={custom.betti[1]}
              onChange={(e) => setCustom({...custom, betti: [custom.betti[0], parseInt(e.target.value) || 0, custom.betti[2]]})}
            />
          </div>
          <div>
            <label className="block text-sm font-medium mb-1">β₂</label>
            <input 
              type="number" 
              className="w-full p-2 border rounded"
              value={custom.betti[2]}
              onChange={(e) => setCustom({...custom, betti: [custom.betti[0], custom.betti[1], parseInt(e.target.value) || 0]})}
            />
          </div>
        </div>
        <div className="mb-4">
          <label className="block text-sm font-medium mb-1">Name</label>
          <input 
            type="text" 
            className="w-full p-2 border rounded"
            value={custom.name}
            onChange={(e) => setCustom({...custom, name: e.target.value})}
          />
        </div>
        <button 
          onClick={() => onAdd(custom)}
          className="bg-blue-600 text-white px-4 py-2 rounded hover:bg-blue-700 transition-colors"
        >
          Calculate Spin
        </button>
      </CardContent>
    </Card>
  );
}

export default function PolySpinExplorer() {
  const [customPolys, setCustomPolys] = useState<PolyStructure[]>([]);

  const addCustomPoly = (poly: PolyStructure) => {
    setCustomPolys([...customPolys, { ...poly, name: `${poly.name} #${customPolys.length + 1}` }]);
  };

  return (
    <div className="max-w-6xl mx-auto p-6 bg-gradient-to-br from-blue-50 to-purple-50 min-h-screen">
      <div className="text-center mb-8">
        <h1 className="text-4xl font-bold text-gray-800 mb-2">Universal Spin Explorer</h1>
        <p className="text-lg text-gray-600">Exploring the mathematical relationships in polyhedral structures</p>
      </div>

      <CustomPolyInput onAdd={addCustomPoly} />

      <div className="grid lg:grid-cols-2 gap-6">
        <div>
          <h2 className="text-2xl font-bold text-gray-800 mb-4">Platonic Solids</h2>
          {platonicSolids.map((poly, idx) => (
            <PolyCard key={idx} poly={poly} />
          ))}
        </div>
        <div>
          <h2 className="text-2xl font-bold text-gray-800 mb-4">Archimedean Solids</h2>
          {archimedeanSolids.map((poly, idx) => (
            <PolyCard key={idx} poly={poly} />
          ))}
          
          {customPolys.length > 0 && (
            <>
              <h2 className="text-2xl font-bold text-gray-800 mb-4 mt-8">Custom Shapes</h2>
              {customPolys.map((poly, idx) => (
                <PolyCard key={idx} poly={poly} />
              ))}
            </>
          )}
        </div>
      </div>

      <div className="mt-8 p-6 bg-white rounded-lg shadow-md">
        <h3 className="text-xl font-bold mb-4">Understanding the Universal Spin Function</h3>
        <div className="space-y-2 text-sm text-gray-700">
          <p><strong>Euler χ:</strong> V - E + F (topological invariant)</p>
          <p><strong>Betti χ:</strong> β₀ - β₁ + β₂ (homological invariant)</p>
          <p><strong>Symmetry:</strong> (p × q) / (V + F)</p>
          <p><strong>Growth Harmonic:</strong> E(E+1)(E+2)/6 (tetrahedral numbers)</p>
          <p><strong>Universal Spin:</strong> tan(π × symmetry) × sign(Δ) × ln(growth harmonic)</p>
        </div>
      </div>
    </div>
  );
}