import React, { useState, useEffect } from 'react';
import { RefreshCw, Activity, Layers, Hexagon, Database, AlertCircle } from 'lucide-react';

// The 15 Hopf Terms mapped to Narrative Analysis Axes (Thorne Vocabulary)
const HOPF_AXES = [
  { id: 0x0, name: 'Base point', aegean: '𐄀', metric: 'Source Origin Verification' },
  { id: 0x1, name: 'Fiber', aegean: '𐄁', metric: 'Narrative Thread Continuity' },
  { id: 0x2, name: 'Projection', aegean: '𐄂', metric: 'Target Audience Spin' },
  { id: 0x3, name: 'Local trivialization', aegean: '𐄇', metric: 'Complexity Reduction' },
  { id: 0x4, name: 'Transition', aegean: '𐄈', metric: 'Context Shifting' },
  { id: 0x5, name: 'Homotopy', aegean: '𐄉', metric: 'Emotional Invariant' },
  { id: 0x6, name: 'Principal bundle', aegean: '𐄊', metric: 'Tribal/Group Appeal' },
  { id: 0x7, name: 'OP¹', aegean: '𐄋', metric: 'Orthogonal Perspective' },
  { id: 0x8, name: 'S⁷ fiber', aegean: '𐄌', metric: 'Semantic Density' },
  { id: 0x9, name: 'S¹⁵ total', aegean: '𐄍', metric: 'Global Contextualization' },
  { id: 0xA, name: 'Stereographic', aegean: '𐄎', metric: 'Real-world Actionability' },
  { id: 0xB, name: 'Villarceau', aegean: '𐄏', metric: 'Correlated Linkages' },
  { id: 0xC, name: 'Hopf link', aegean: '𐄐', metric: 'Causal Entanglement' },
  { id: 0xD, name: 'Adams', aegean: '𐄑', metric: 'Historical Classification' },
  { id: 0xE, name: 'Exotic', aegean: '𐄒', metric: 'Anomaly / Outlier Signal' }
];

// Mock News Sources for the Aggregator
const MOCK_FEEDS = [
  { id: 1, source: 'Global Sync', title: 'Central Banks Announce Unified Digital Ledger Framework', text: 'In an unprecedented move, major central banks have agreed to a unified digital ledger framework, aiming to eliminate cross-border friction. Critics argue this reduces orthogonal monetary perspectives and centralizes the global principal bundle of finance.' },
  { id: 2, source: 'Tech Vanguard', title: 'Breakthrough in Room-Temperature Superconductors', text: 'Researchers claim to have stabilized a room-temperature superconductor using a novel ceramic matrix. If verified, this S15-level breakthrough could reorganize global energy distribution, though historical Adams classifications urge caution due to past false positives.' },
  { id: 3, source: 'Political Pulse', title: 'Senate Deadlock Over Infrastructure Bill Approaches Third Week', text: 'The ongoing Senate deadlock over the infrastructure bill has reached a fever pitch. Both sides are digging into their principal bundles, using heavy emotional invariants to rally their bases. The local trivialization of the bill into a "tax vs. spend" debate ignores the complex Villarceau linkages of the actual policies.' },
  { id: 4, source: 'Cultural Wave', title: 'The Rise of "Slow Computing" Movements in Urban Centers', text: 'A growing demographic of tech workers are abandoning high-frequency devices for "slow computing"—using E-ink displays and single-purpose hardware. This exotic anomaly signals a rejection of high semantic density in favor of local triviality.' }
];

// Radial Chart Component (The Omnitron Eye)
const HopfRadarChart = ({ data }) => {
  const size = 300;
  const center = size / 2;
  const radius = (size / 2) - 30;
  const numAxes = HOPF_AXES.length;

  // Generate polygon points based on 1-9 Aegean scale
  const points = HOPF_AXES.map((axis, i) => {
    const value = data ? data[axis.name] : 0;
    const normalizedValue = value / 9; // Scale 1-9
    const angle = (Math.PI * 2 * i) / numAxes - Math.PI / 2;
    const x = center + radius * normalizedValue * Math.cos(angle);
    const y = center + radius * normalizedValue * Math.sin(angle);
    return `${x},${y}`;
  }).join(' ');

  return (
    <div className="relative flex justify-center items-center w-full h-full p-4">
      <svg width={size} height={size} viewBox={`0 0 ${size} ${size}`} className="overflow-visible">
        {/* Draw Web Background */}
        {[0.2, 0.4, 0.6, 0.8, 1].map((scale, level) => (
          <polygon
            key={level}
            points={HOPF_AXES.map((_, i) => {
              const angle = (Math.PI * 2 * i) / numAxes - Math.PI / 2;
              return `${center + radius * scale * Math.cos(angle)},${center + radius * scale * Math.sin(angle)}`;
            }).join(' ')}
            fill="none"
            stroke="#334155"
            strokeWidth="1"
            strokeDasharray={level % 2 === 0 ? "2,2" : ""}
          />
        ))}

        {/* Draw Axes */}
        {HOPF_AXES.map((axis, i) => {
          const angle = (Math.PI * 2 * i) / numAxes - Math.PI / 2;
          const x2 = center + radius * Math.cos(angle);
          const y2 = center + radius * Math.sin(angle);
          const labelX = center + (radius + 20) * Math.cos(angle);
          const labelY = center + (radius + 20) * Math.sin(angle);
          
          return (
            <g key={i}>
              <line x1={center} y1={center} x2={x2} y2={y2} stroke="#475569" strokeWidth="1" />
              <text 
                x={labelX} 
                y={labelY} 
                fill="#94a3b8" 
                fontSize="12" 
                textAnchor="middle" 
                dominantBaseline="middle"
                className="font-mono"
              >
                {axis.aegean}
              </text>
            </g>
          );
        })}

        {/* Draw Data Polygon */}
        {data && (
          <polygon
            points={points}
            fill="rgba(16, 185, 129, 0.2)"
            stroke="#10b981"
            strokeWidth="2"
            className="transition-all duration-500 ease-in-out"
          />
        )}
      </svg>
    </div>
  );
};

export default function App() {
  const [activeArticle, setActiveArticle] = useState(null);
  const [customText, setCustomText] = useState("");
  const [analysis, setAnalysis] = useState(null);
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [error, setError] = useState(null);

  const apiKey = ""; // Provided by execution environment

  const handleAnalyze = async (textToAnalyze) => {
    if (!textToAnalyze.trim()) return;
    
    setIsAnalyzing(true);
    setError(null);
    setAnalysis(null);

    const systemPrompt = `
      You are the Omnitron Protocol Narrative Analyzer, utilizing the Thorne Calculus.
      Your task is to analyze the provided text and map it onto the 15 terms of the S15 Hopf Fibration.
      Score each metric strictly on a scale of 1 to 9 (representing the Aegean bijective base-9).
      
      The 15 Axes and their Narrative Meanings:
      1. Base point: Source Origin Verification (How verifiable/rooted is the source?)
      2. Fiber: Narrative Thread Continuity (Does the story hold together logically?)
      3. Projection: Target Audience Spin (How heavily is it spun for a specific demographic?)
      4. Local trivialization: Complexity Reduction (How much does it dumb down complex issues?)
      5. Transition: Context Shifting (Does it employ "whataboutism" or shift the goalposts?)
      6. Homotopy: Emotional Invariant (How heavily does it rely on static emotional triggers like rage/fear?)
      7. Principal bundle: Tribal/Group Appeal (How much does it appeal to in-group/out-group dynamics?)
      8. OP¹: Orthogonal Perspective (Does it offer a genuinely new/orthogonal viewpoint?)
      9. S⁷ fiber: Semantic Density (How dense/informative is the text?)
      10. S¹⁵ total: Global Contextualization (Does it connect the local event to the global whole?)
      11. Stereographic: Real-world Actionability (How practical/actionable is the information?)
      12. Villarceau: Correlated Linkages (Does it accurately link seemingly unrelated phenomena?)
      13. Hopf link: Causal Entanglement (Does it conflate correlation with causation?)
      14. Adams: Historical Classification (How well does it place the event in historical context?)
      15. Exotic: Anomaly / Outlier Signal (Does this represent a black swan or exotic anomaly?)

      Return a valid JSON object where keys are the EXACT axis names (e.g., "Base point", "Fiber") and values are integers from 1 to 9.
    `;

    const payload = {
      contents: [{ parts: [{ text: textToAnalyze }] }],
      systemInstruction: { parts: [{ text: systemPrompt }] },
      generationConfig: {
        responseMimeType: "application/json",
        responseSchema: {
          type: "OBJECT",
          properties: HOPF_AXES.reduce((acc, axis) => {
            acc[axis.name] = { type: "INTEGER" };
            return acc;
          }, {})
        }
      }
    };

    try {
      const response = await fetchWithRetry(
        `https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash-preview-09-2025:generateContent?key=${apiKey}`,
        {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        }
      );

      const data = await response.json();
      if (data.error) throw new Error(data.error.message);
      
      const rawText = data.candidates?.[0]?.content?.parts?.[0]?.text;
      if (!rawText) throw new Error("No analysis generated");
      
      const parsedAnalysis = JSON.parse(rawText);
      setAnalysis(parsedAnalysis);
    } catch (err) {
      console.error("Analysis Error:", err);
      setError("Failed to execute Hopf analysis. The manifold did not converge.");
    } finally {
      setIsAnalyzing(false);
    }
  };

  const fetchWithRetry = async (url, options, retries = 5) => {
    let delay = 1000;
    for (let i = 0; i < retries; i++) {
      try {
        const res = await fetch(url, options);
        if (res.ok) return res;
      } catch (e) {
        if (i === retries - 1) throw e;
      }
      await new Promise(resolve => setTimeout(resolve, delay));
      delay *= 2;
    }
  };

  return (
    <div className="min-h-screen bg-slate-950 text-slate-300 font-sans flex flex-col">
      {/* Header */}
      <header className="border-b border-slate-800 bg-slate-900 p-4 flex items-center justify-between">
        <div className="flex items-center space-x-3">
          <Hexagon className="text-emerald-500" size={28} />
          <div>
            <h1 className="text-xl font-bold text-slate-100 tracking-wider">OMNITRON DDC</h1>
            <p className="text-xs text-emerald-500 font-mono">MULTI-AXIS NARRATIVE ANALYZER (S¹⁵ FIBER)</p>
          </div>
        </div>
        <div className="flex items-center space-x-2 text-xs font-mono text-slate-500">
          <span>PERIOD 8</span>
          <span>•</span>
          <span>NULL != 10</span>
        </div>
      </header>

      <div className="flex flex-1 overflow-hidden">
        {/* Left Panel: News Aggregator Stream */}
        <aside className="w-1/3 border-r border-slate-800 bg-slate-900/50 flex flex-col">
          <div className="p-4 border-b border-slate-800 flex items-center space-x-2">
            <Database size={16} className="text-slate-400" />
            <h2 className="font-semibold text-slate-200">Data Stream (Aggregator)</h2>
          </div>
          <div className="flex-1 overflow-y-auto p-4 space-y-4">
            {MOCK_FEEDS.map(feed => (
              <div 
                key={feed.id} 
                className={`p-4 rounded-lg border cursor-pointer transition-all ${activeArticle?.id === feed.id ? 'bg-slate-800 border-emerald-500/50' : 'bg-slate-900 border-slate-800 hover:border-slate-600'}`}
                onClick={() => {
                  setActiveArticle(feed);
                  setCustomText(feed.text);
                  handleAnalyze(feed.text);
                }}
              >
                <div className="text-xs text-emerald-500 font-mono mb-2">{feed.source}</div>
                <h3 className="font-medium text-slate-200 mb-2 leading-tight">{feed.title}</h3>
                <p className="text-sm text-slate-400 line-clamp-2">{feed.text}</p>
              </div>
            ))}

            <div className="mt-8 pt-4 border-t border-slate-800">
              <h3 className="text-sm font-semibold text-slate-200 mb-3 font-mono">MANUAL INPUT</h3>
              <textarea
                className="w-full h-32 bg-slate-950 border border-slate-800 rounded-md p-3 text-sm focus:outline-none focus:border-emerald-500 transition-colors resize-none"
                placeholder="Paste external text here for Hopf projection..."
                value={customText}
                onChange={(e) => setCustomText(e.target.value)}
              />
              <button
                className="w-full mt-3 bg-emerald-600 hover:bg-emerald-500 text-white font-mono text-sm py-2 rounded-md transition-colors flex items-center justify-center space-x-2 disabled:opacity-50"
                onClick={() => {
                  setActiveArticle(null);
                  handleAnalyze(customText);
                }}
                disabled={isAnalyzing || !customText.trim()}
              >
                {isAnalyzing ? <RefreshCw className="animate-spin" size={16} /> : <Activity size={16} />}
                <span>{isAnalyzing ? 'COMPUTING INVARIANT...' : 'PROJECT TO S¹⁵'}</span>
              </button>
            </div>
          </div>
        </aside>

        {/* Right Panel: Multi-Axis Analysis */}
        <main className="w-2/3 bg-slate-950 flex flex-col relative overflow-hidden">
          {error && (
            <div className="absolute top-4 left-4 right-4 bg-red-900/50 border border-red-500 text-red-200 px-4 py-3 rounded-md flex items-center space-x-3 z-10">
              <AlertCircle size={18} />
              <span className="text-sm">{error}</span>
            </div>
          )}

          {!analysis && !isAnalyzing && (
            <div className="flex-1 flex flex-col items-center justify-center text-slate-600">
              <Layers size={48} strokeWidth={1} className="mb-4 opacity-50" />
              <p className="font-mono text-sm">AWAITING TEXT FOR METACIRCULAR EVALUATION</p>
            </div>
          )}

          {isAnalyzing && (
            <div className="flex-1 flex flex-col items-center justify-center text-emerald-500">
              <RefreshCw size={48} strokeWidth={1} className="animate-spin mb-4" />
              <p className="font-mono text-sm animate-pulse">EVALUATING HOPF FIBRATION TERMS...</p>
            </div>
          )}

          {analysis && !isAnalyzing && (
            <div className="flex-1 overflow-y-auto p-6 flex flex-col xl:flex-row gap-8">
              {/* Radar Chart Area */}
              <div className="flex-1 flex flex-col items-center bg-slate-900/30 rounded-xl border border-slate-800 p-6">
                <h3 className="font-mono text-emerald-400 mb-2">OMNICRON-GNOMON PROJECTION</h3>
                <p className="text-xs text-slate-500 mb-8">Score Matrix (Base-9 Aegean)</p>
                <HopfRadarChart data={analysis} />
              </div>

              {/* Data Table */}
              <div className="flex-1">
                <h3 className="font-mono text-emerald-400 mb-4 border-b border-slate-800 pb-2">FIBER BUNDLE METRICS</h3>
                <div className="space-y-3">
                  {HOPF_AXES.map((axis) => {
                    const score = analysis[axis.name] || 0;
                    return (
                      <div key={axis.id} className="flex items-center text-sm">
                        <div className="w-8 text-center font-mono text-emerald-600 text-lg">
                          {axis.aegean}
                        </div>
                        <div className="flex-1 pl-3">
                          <div className="flex justify-between mb-1">
                            <span className="text-slate-300">{axis.metric}</span>
                            <span className="font-mono text-emerald-400">{score}/9</span>
                          </div>
                          <div className="w-full bg-slate-900 h-1.5 rounded-full overflow-hidden">
                            <div 
                              className="h-full bg-emerald-500 transition-all duration-1000 ease-out"
                              style={{ width: `${(score / 9) * 100}%` }}
                            />
                          </div>
                        </div>
                      </div>
                    );
                  })}
                </div>
              </div>
            </div>
          )}
        </main>
      </div>
    </div>
  );
}