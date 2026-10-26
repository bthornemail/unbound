import React, { useState, useCallback } from 'react';
import { Card, CardContent, CardHeader, CardTitle } from '../components/ui/card';

// UBHP Core Implementation
class UBHPCore {
  // SExprType Enumeration
  static SExprType = {
    NULL: 0x00,
    BOOL: 0x01,
    INT32: 0x02,
    STRING: 0x06,
    SYMBOL: 0x07,
    LIST: 0x08,
    LAMBDA: 0x09,
    REFERENCE: 0x0A,
    MODEL_WEIGHTS: 0x0B,
    SEED_TRANSFORM: 0x0C
  };

  // Variable-length integer encoding
  static encodeVarInt(value) {
    const result = [];
    while (value >= 0x80) {
      result.push((value & 0x7F) | 0x80);
      value >>>= 7;
    }
    result.push(value & 0x7F);
    return new Uint8Array(result);
  }

  static decodeVarInt(buffer, offset = 0) {
    let value = 0;
    let shift = 0;
    let pos = offset;
    while (pos < buffer.length) {
      const byte = buffer[pos++];
      value |= (byte & 0x7F) << shift;
      if ((byte & 0x80) === 0) break;
      shift += 7;
    }
    return [value, pos - offset];
  }

  // Harmonize function - generates harmonic signature from ArrayBuffer
  static harmonize(inputSExpr, originBuffer = null) {
    const view = new Uint8Array(inputSExpr);
    const rawValues = Array.from(view);

    // XOR with origin if provided (for shared context consensus)
    const values = originBuffer
      ? rawValues.map((v, i) => v ^ new Uint8Array(originBuffer)[i % originBuffer.byteLength])
      : rawValues;

    const h = Math.hypot(...values); // Euclidean norm
    const sin = Math.sin(h / Math.PI);
    const cos = Math.cos(h / 1.61803398875); // Golden ratio constant
    const tan = Math.tan(Math.PI / (h || 1e-10));

    // Content-based ID using harmonic properties
    const id = `UBHP_${h.toFixed(8)}_${sin.toFixed(8)}_${cos.toFixed(8)}_${view.length}`;

    return {
      id,
      length: values.length,
      sin,
      cos,
      tan,
      h,
      buffer: inputSExpr
    };
  }

  // Convert ArrayBuffer to normalized vector ray
  static typedArrayToRay(inputSExprBuffer) {
    const input = new Uint8Array(inputSExprBuffer);
    const norm = Math.hypot(...input);
    return norm === 0 ? Array.from(input) : Array.from(input).map((v) => v / norm);
  }

  // Calculate cosine similarity between two vectors
  static cosineSimilarity(a, b) {
    let dot = 0;
    let normA = 0;
    let normB = 0;
    const len = Math.min(a.length, b.length);

    for (let i = 0; i < len; i++) {
      dot += a[i] * b[i];
      normA += a[i] * a[i];
      normB += b[i] * b[i];
    }
    const magnitude = Math.sqrt(normA) * Math.sqrt(normB);
    return magnitude === 0 ? 0 : dot / magnitude;
  }

  // Calculate centroid of multiple vectors
  static calculateCentroid(wordVectors) {
    if (wordVectors.length === 0) return [];
    const dimensions = wordVectors[0].length;
    const centroid = new Array(dimensions).fill(0);
    
    for (const vec of wordVectors) {
      if (vec.length !== dimensions) throw new Error("All vectors must have the same dimension.");
      for (let i = 0; i < dimensions; i++) centroid[i] += vec[i];
    }
    
    for (let i = 0; i < dimensions; i++) centroid[i] /= wordVectors.length;
    return centroid;
  }

  // Simple S-expression encoder for strings
  static encodeString(value) {
    const buffer = [];
    buffer.push(this.SExprType.STRING);
    const utf8Bytes = new TextEncoder().encode(value);
    const lengthBytes = this.encodeVarInt(utf8Bytes.length);
    buffer.push(...lengthBytes, ...utf8Bytes);
    return new Uint8Array(buffer).buffer;
  }
}

export default function UBHPDemo() {
  const [inputText, setInputText] = useState("Hello, UBHP!");
  const [originText, setOriginText] = useState("");
  const [results, setResults] = useState(null);
  const [comparisonData, setComparisonData] = useState([]);

  const processInput = useCallback(() => {
    try {
      // Encode input as S-expression
      const inputBuffer = UBHPCore.encodeString(inputText);
      const originBuffer = originText ? UBHPCore.encodeString(originText) : null;
      
      // Generate harmonic signature
      const harmonic = UBHPCore.harmonize(inputBuffer, originBuffer);
      
      // Generate ray vector
      const ray = UBHPCore.typedArrayToRay(inputBuffer);
      
      // Show first few ray components (truncated for display)
      const rayDisplay = ray.slice(0, 8);
      
      setResults({
        harmonic,
        ray: rayDisplay,
        rayLength: ray.length,
        bufferSize: inputBuffer.byteLength
      });
    } catch (error) {
      setResults({ error: error.message });
    }
  }, [inputText, originText]);

  const addToComparison = useCallback(() => {
    if (results && !results.error) {
      const newEntry = {
        text: inputText,
        harmonic: results.harmonic,
        ray: UBHPCore.typedArrayToRay(UBHPCore.encodeString(inputText)),
        id: Date.now()
      };
      setComparisonData(prev => [...prev, newEntry]);
    }
  }, [results, inputText]);

  const calculateSimilarities = useCallback(() => {
    if (comparisonData.length < 2) return [];
    
    const similarities = [];
    for (let i = 0; i < comparisonData.length; i++) {
      for (let j = i + 1; j < comparisonData.length; j++) {
        const similarity = UBHPCore.cosineSimilarity(
          comparisonData[i].ray,
          comparisonData[j].ray
        );
        similarities.push({
          text1: comparisonData[i].text,
          text2: comparisonData[j].text,
          similarity: similarity.toFixed(6)
        });
      }
    }
    return similarities;
  }, [comparisonData]);

  const clearComparison = useCallback(() => {
    setComparisonData([]);
  }, []);

  return (
    <div className="max-w-6xl mx-auto p-6 space-y-6">
      <Card>
        <CardHeader>
          <CardTitle>Universal Binary Hypergraph Protocol (UBHP) Demo</CardTitle>
        </CardHeader>
        <CardContent>
          <p className="text-sm text-gray-600 mb-4">
            This demo implements the core UBHP functions: harmonize(), typedArrayToRay(), and cosineSimilarity().
            Enter text to see how it's converted to harmonic signatures and vector rays.
          </p>
        </CardContent>
      </Card>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <Card>
          <CardHeader>
            <CardTitle>Input & Processing</CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div>
              <label className="block text-sm font-medium mb-2">Input Text:</label>
              <input
                type="text"
                value={inputText}
                onChange={(e) => setInputText(e.target.value)}
                className="w-full p-2 border rounded"
                placeholder="Enter text to harmonize..."
              />
            </div>
            
            <div>
              <label className="block text-sm font-medium mb-2">Origin Buffer (optional):</label>
              <input
                type="text"
                value={originText}
                onChange={(e) => setOriginText(e.target.value)}
                className="w-full p-2 border rounded"
                placeholder="Optional origin for XOR operation..."
              />
            </div>

            <div className="flex gap-2">
              <button
                onClick={processInput}
                className="px-4 py-2 bg-blue-500 text-white rounded hover:bg-blue-600"
              >
                Process
              </button>
              <button
                onClick={addToComparison}
                disabled={!results || results.error}
                className="px-4 py-2 bg-green-500 text-white rounded hover:bg-green-600 disabled:opacity-50"
              >
                Add to Comparison
              </button>
            </div>
          </CardContent>
        </Card>

        <Card>
          <CardHeader>
            <CardTitle>Harmonic Signature Results</CardTitle>
          </CardHeader>
          <CardContent>
            {results && (
              <div className="space-y-3">
                {results.error ? (
                  <div className="text-red-600">Error: {results.error}</div>
                ) : (
                  <>
                    <div className="text-sm">
                      <strong>ID:</strong> <span className="font-mono text-xs">{results.harmonic.id}</span>
                    </div>
                    <div className="grid grid-cols-2 gap-2 text-sm">
                      <div><strong>H:</strong> {results.harmonic.h.toFixed(6)}</div>
                      <div><strong>Length:</strong> {results.harmonic.length}</div>
                      <div><strong>Sin:</strong> {results.harmonic.sin.toFixed(6)}</div>
                      <div><strong>Cos:</strong> {results.harmonic.cos.toFixed(6)}</div>
                      <div><strong>Tan:</strong> {results.harmonic.tan.toFixed(6)}</div>
                      <div><strong>Buffer Size:</strong> {results.bufferSize} bytes</div>
                    </div>
                    <div className="text-sm">
                      <strong>Ray Vector (first 8 components):</strong>
                      <div className="font-mono text-xs mt-1 bg-gray-100 p-2 rounded max-h-20 overflow-y-auto">
                        [{results.ray.map(v => v.toFixed(4)).join(', ')}]
                        {results.rayLength > 8 && <span className="text-gray-500"> ... ({results.rayLength} total)</span>}
                      </div>
                    </div>
                  </>
                )}
              </div>
            )}
          </CardContent>
        </Card>
      </div>

      <Card>
        <CardHeader>
          <CardTitle>Similarity Comparison</CardTitle>
        </CardHeader>
        <CardContent>
          <div className="flex gap-2 mb-4">
            <span className="text-sm text-gray-600">
              Entries: {comparisonData.length}
            </span>
            <button
              onClick={clearComparison}
              className="px-3 py-1 text-xs bg-gray-500 text-white rounded hover:bg-gray-600"
            >
              Clear
            </button>
          </div>

          {comparisonData.length > 0 && (
            <div className="space-y-3">
              <div>
                <strong>Added Entries:</strong>
                <div className="mt-2 space-y-1">
                  {comparisonData.map((entry, idx) => (
                    <div key={entry.id} className="text-sm bg-gray-50 p-2 rounded">
                      {idx + 1}. "{entry.text}" (h: {entry.harmonic.h.toFixed(4)})
                    </div>
                  ))}
                </div>
              </div>

              {comparisonData.length >= 2 && (
                <div>
                  <strong>Cosine Similarities:</strong>
                  <div className="mt-2 space-y-1">
                    {calculateSimilarities().map((sim, idx) => (
                      <div key={idx} className="text-sm bg-blue-50 p-2 rounded">
                        "{sim.text1}" ↔ "{sim.text2}": {sim.similarity}
                      </div>
                    ))}
                  </div>
                </div>
              )}
            </div>
          )}
        </CardContent>
      </Card>

      <Card>
        <CardHeader>
          <CardTitle>About UBHP</CardTitle>
        </CardHeader>
        <CardContent>
          <div className="text-sm text-gray-600 space-y-2">
            <p>
              <strong>Harmonic Signatures:</strong> Generate unique content-based addresses using trigonometric functions
              applied to the Euclidean norm of binary data.
            </p>
            <p>
              <strong>Vector Rays:</strong> Normalized vectors that represent the "direction" of content in high-dimensional space,
              enabling geometric analysis and similarity detection.
            </p>
            <p>
              <strong>Cosine Similarity:</strong> Measures the angular similarity between vectors, with values closer to 1
              indicating higher similarity in content structure.
            </p>
          </div>
        </CardContent>
      </Card>
    </div>
  );
}