// import { useState } from 'react'
// import heroImg from './assets/hero.png'
// import reactLogo from './assets/react.svg'
// import viteLogo from './assets/vite.svg'
import './App.css'
import AtomicALUSurface from './templates/Atomic ALU Surface.tsx'
import AtomicIncidenceSurface from './templates/Atomic Incidence Surface.tsx';
import AtomicIncidenceMonitor from './templates/Atomic Incidence Monitor.tsx';
import AtomicTapeReader from './templates/Atomic Tape Reader.tsx';
import AtomicWallisSurface from './templates/Atomic Wallis Surface.tsx';
// import BasicIncidentsGraph from './templates/Basic Incidents Graph.tsx';
import FeatureMapRuntime from './templates/Feature Map Runtime.tsx';
import FeatureMapRuntimev2 from './templates/Feature Map Runtime.v2.tsx';
import FeatureMapRuntimev3 from './templates/Feature Map Runtime.v3.tsx';
import GENESISProtocolExplorer from './templates/GENESIS Protocol Explorer.tsx';
import GeometricHypergraphAISystem from './templates/Geometric Hypergraph AI System.tsx';
import HarmonicClosureEditor from './templates/Harmonic Closure Editor.tsx';
import IChing256Centroid from './templates/I Ching 256 Centroid.tsx';
import IncidenceHypergraphVisualization from './templates/Incidence Hypergraph Visualization.tsx';
import NFTUploadDemo from './templates/NFT Upload Demo.tsx';
import OllamaChatInterfaceComponent from './templates/Ollama Chat Interface Component.tsx';
import OmicronEvaluator_PolyformTruthSurface from './templates/Omicron Evaluator_ Polyform Truth Surface.tsx';
import OmicronEvaluator_PolyformTruthSurfacev2 from './templates/Omicron Evaluator_ Polyform Truth Surface.v2.tsx';
import OmiWorldBuilder from './templates/Omi-World Builder.tsx';
import OmnitronMultiAxisAnalyzer from './templates/Omnitron Multi-Axis Analyzer.tsx';
import P2PMarketplaceWebApp from './templates/P2P Marketplace Web App.tsx';
import PolyhedralSpinExplorer from './templates/Polyhedral Spin Explorer.tsx';
import ReactSwipeContainerTypeScript from './templates/React Swipe Container (TypeScript).tsx';
import SCGNNVMStatelessSacredGeometryNeuralNetworkVirtualMach from './templates/SCGNN VM - Stateless Sacred Geometry Neural Network Virtual Mach.tsx';
import SemanticMerkleTreeVisualization from './templates/Semantic Merkle Tree Visualization.tsx';
import SemanticMerkleTreeVisualizationv2 from './templates/Semantic Merkle Tree Visualization.v2.tsx';
import SemanticMnemonicEditor from './templates/Semantic Mnemonic Editor.tsx';
import SemanticRaycastingThroughPlatonicSpace from './templates/Semantic Raycasting Through Platonic Space.tsx';
import SpectralIntegrationClosure from './templates/Spectral Integration Closure.tsx';
import TetrahedralGraphVisualization from './templates/Tetrahedral Graph Visualization.tsx';
import UBHPCoreFunctionsDemo from './templates/UBHP Core Functions Demo.tsx';
import UnifiedProjectiveWorkbench from './templates/Unified Projective Workbench.tsx';
import UnifiedProjectiveWorkbenchv2 from './templates/Unified Projective Workbench.v2.tsx';
import WebAuthnWalletQRCodeApp from './templates/WebAuthn Wallet QR Code App.tsx';


function App() {
    const cards = [
    { id: 1, color: 'bg-red-400', title: 'Card 1',element:  <AtomicALUSurface />},
    { id: 2, color: 'bg-blue-400', title: 'Card 2',element:  <AtomicIncidenceSurface /> },
    { id: 3, color: 'bg-green-400', title: 'Card 3',element:  <AtomicIncidenceMonitor /> },
    { id: 4, color: 'bg-yellow-400', title: 'Card 4',element:  <AtomicWallisSurface /> },
    { id: 5, color: 'bg-yellow-400', title: 'Card 4',element:  <AtomicTapeReader /> },


    { id: 6, color: 'bg-red-400', title: 'Card 6',element:  <FeatureMapRuntimev3 />},
    { id: 7, color: 'bg-blue-400', title: 'Card 7',element:  <GENESISProtocolExplorer /> },
    { id: 8, color: 'bg-green-400', title: 'Card 8',element:  <GeometricHypergraphAISystem /> },
    { id: 9, color: 'bg-yellow-400', title: 'Card 9',element:  <HarmonicClosureEditor /> },
    { id: 10, color: 'bg-purple-400', title: 'Card 10',element:  <IChing256Centroid />},
    
    { id: 11, color: 'bg-red-400', title: 'Card 11',element:  <IncidenceHypergraphVisualization />},
    { id: 12, color: 'bg-blue-400', title: 'Card 12',element:  <NFTUploadDemo /> },
    { id: 13, color: 'bg-green-400', title: 'Card 13',element:  <OllamaChatInterfaceComponent /> },
    { id: 14, color: 'bg-yellow-400', title: 'Card 14',element:  <OmicronEvaluator_PolyformTruthSurface /> },
    { id: 15, color: 'bg-purple-400', title: 'Card 15',element:  <OmicronEvaluator_PolyformTruthSurfacev2 /> },
    
    { id: 16, color: 'bg-red-400', title: 'Card 16',element:  <OmiWorldBuilder /> },
    { id: 17, color: 'bg-blue-400', title: 'Card 17',element:  <OmnitronMultiAxisAnalyzer />},
    { id: 18, color: 'bg-green-400', title: 'Card 18',element:  <P2PMarketplaceWebApp />},
    { id: 19, color: 'bg-yellow-400', title: 'Card 19',element:  <PolyhedralSpinExplorer /> },
    { id: 20, color: 'bg-purple-400', title: 'Card 20',element:  <SCGNNVMStatelessSacredGeometryNeuralNetworkVirtualMach /> },
    
    // { id: 21, color: 'bg-red-400', title: 'Card 21',element:  <SemanticMerkleTreeVisualization /> },
    { id: 22, color: 'bg-blue-400', title: 'Card 22',element:  <SemanticMerkleTreeVisualizationv2 />},
    { id: 23, color: 'bg-green-400', title: 'Card 23',element:  <SemanticMnemonicEditor /> },
    { id: 24, color: 'bg-yellow-400', title: 'Card 24',element:  <SemanticRaycastingThroughPlatonicSpace /> },
    { id: 25, color: 'bg-purple-400', title: 'Card 25',element:  <SpectralIntegrationClosure /> },
    
    { id: 26, color: 'bg-red-400', title: 'Card 26',element:  <TetrahedralGraphVisualization />},
    { id: 27, color: 'bg-blue-400', title: 'Card 27',element:  <UBHPCoreFunctionsDemo />},
    // { id: 28, color: 'bg-green-400', title: 'Card 28',element:  <UnifiedProjectiveWorkbench />},
    { id: 29, color: 'bg-yellow-400', title: 'Card 29',element:  <UnifiedProjectiveWorkbenchv2 />},
    { id: 30, color: 'bg-purple-400', title: 'Card 30',element:  <WebAuthnWalletQRCodeApp /> },
    
  ];
  return (
    <>
      <ReactSwipeContainerTypeScript cards={cards} />
    </>
  )
}

export default App
