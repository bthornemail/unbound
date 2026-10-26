import { lazy, type ComponentType, type LazyExoticComponent } from 'react';

export interface Example {
  slug: string;
  title: string;
  group: string;
  Component: LazyExoticComponent<ComponentType>;
}

const entry = (
  title: string,
  group: string,
  load: () => Promise<{ default: ComponentType }>,
): Example => ({
  slug: title.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, ''),
  title,
  group,
  Component: lazy(load),
});

export const examples: Example[] = [
  entry('Atomic ALU Surface', 'Atomic', () => import('../templates/Atomic ALU Surface.tsx')),
  entry('Atomic Incidence Surface', 'Atomic', () => import('../templates/Atomic Incidence Surface.tsx')),
  entry('Atomic Incidence Monitor', 'Atomic', () => import('../templates/Atomic Incidence Monitor.tsx')),
  entry('Atomic Wallis Surface', 'Atomic', () => import('../templates/Atomic Wallis Surface.tsx')),
  entry('Atomic Tape Reader', 'Atomic', () => import('../templates/Atomic Tape Reader.tsx')),

  // entry('Feature Map Runtime', 'Runtime', () => import('../templates/Feature Map Runtime.tsx')),
  // entry('Feature Map Runtime v2', 'Runtime', () => import('../templates/Feature Map Runtime.v2.tsx')),
  entry('Feature Map Runtime v3', 'Runtime', () => import('../templates/Feature Map Runtime.v3.tsx')),
  entry('GENESIS Protocol Explorer', 'Runtime', () => import('../templates/GENESIS Protocol Explorer.tsx')),
  entry('SCGNN VM', 'Runtime', () => import('../templates/SCGNN VM - Stateless Sacred Geometry Neural Network Virtual Mach.tsx')),
  entry('UBHP Core Functions Demo', 'Runtime', () => import('../templates/UBHP Core Functions Demo.tsx')),

  entry('Geometric Hypergraph AI System', 'Geometry', () => import('../templates/Geometric Hypergraph AI System.tsx')),
  // entry('Basic Incidents Graph', 'Geometry', () => import('../templates/Basic Incidents Graph.tsx')),
  entry('Incidence Hypergraph Visualization', 'Geometry', () => import('../templates/Incidence Hypergraph Visualization.tsx')),
  entry('Polyhedral Spin Explorer', 'Geometry', () => import('../templates/Polyhedral Spin Explorer.tsx')),
  entry('Tetrahedral Graph Visualization', 'Geometry', () => import('../templates/Tetrahedral Graph Visualization.tsx')),
  entry('I Ching 256 Centroid', 'Geometry', () => import('../templates/I Ching 256 Centroid.tsx')),
  entry('Harmonic Closure Editor', 'Geometry', () => import('../templates/Harmonic Closure Editor.tsx')),
  entry('Spectral Integration Closure', 'Geometry', () => import('../templates/Spectral Integration Closure.tsx')),
  // entry('Unified Projective Workbench', 'Geometry', () => import('../templates/Unified Projective Workbench.tsx')),
  entry('Unified Projective Workbench v2', 'Geometry', () => import('../templates/Unified Projective Workbench.v2.tsx')),

  // entry('Semantic Merkle Tree Visualization', 'Semantic', () => import('../templates/Semantic Merkle Tree Visualization.tsx')),
  entry('Semantic Merkle Tree Visualization v2', 'Semantic', () => import('../templates/Semantic Merkle Tree Visualization.v2.tsx')),
  entry('Semantic Mnemonic Editor', 'Semantic', () => import('../templates/Semantic Mnemonic Editor.tsx')),
  entry('Semantic Raycasting Through Platonic Space', 'Semantic', () => import('../templates/Semantic Raycasting Through Platonic Space.tsx')),

  entry('Omicron Evaluator: Polyform Truth Surface', 'Omi', () => import('../templates/Omicron Evaluator_ Polyform Truth Surface.tsx')),
  entry('Omicron Evaluator: Polyform Truth Surface v2', 'Omi', () => import('../templates/Omicron Evaluator_ Polyform Truth Surface.v2.tsx')),
  entry('Omi-World Builder', 'Omi', () => import('../templates/Omi-World Builder.tsx')),
  entry('Omnitron Multi-Axis Analyzer', 'Omi', () => import('../templates/Omnitron Multi-Axis Analyzer.tsx')),

  entry('NFT Upload Demo', 'Apps', () => import('../templates/NFT Upload Demo.tsx')),
  entry('Ollama Chat Interface', 'Apps', () => import('../templates/Ollama Chat Interface Component.tsx')),
  entry('P2P Marketplace Web App', 'Apps', () => import('../templates/P2P Marketplace Web App.tsx')),
  entry('WebAuthn Wallet QR Code App', 'Apps', () => import('../templates/WebAuthn Wallet QR Code App.tsx')),
];

export const groups = [...new Set(examples.map((e) => e.group))];

export const findExample = (slug: string | undefined) => examples.findIndex((e) => e.slug === slug);
