import { useState } from 'react';

// This is a simplified demo version of the component
// Note: The actual hook functionality won't work in this preview

const NFTUploadDemo = () => {
  // States
  const [isLoading, setIsLoading] = useState(false);
  const [result, setResult] = useState(null);
  const [isPinned, setIsPinned] = useState(false);
  const [error, setError] = useState(null);
  
  // Sample NFT data
  const nftData = {
    name: "My Awesome NFT",
    description: "This is a unique digital collectible minted on my private Ethereum network.",
    image: "ipfs://QmImageHash123456789abcdefghijklmnopqrstuvwxyz",
    attributes: [
      { trait_type: "Background", value: "Blue" },
      { trait_type: "Eyes", value: "Starry" },
      { trait_type: "Mouth", value: "Smile" },
      { trait_type: "Level", value: 5, max_value: 10 },
      { trait_type: "Stamina", value: 1.4, display_type: "boost_percentage" },
      { trait_type: "Creation Date", value: 1685567400, display_type: "date" }
    ]
  };
  
  // Mock upload function
  const handleUpload = () => {
    setIsLoading(true);
    setError(null);
    
    // Simulate API call
    setTimeout(() => {
      setIsLoading(false);
      const mockResult = {
        cid: "QmZMj74RHxSKvz6Mdf5rTZE5mRSLcmRvznwMVL7hYLWUQn",
        ipfsUrl: "ipfs://QmZMj74RHxSKvz6Mdf5rTZE5mRSLcmRvznwMVL7hYLWUQn",
        gatewayUrl: "http://localhost:8080/ipfs/QmZMj74RHxSKvz6Mdf5rTZE5mRSLcmRvznwMVL7hYLWUQn"
      };
      setResult(mockResult);
    }, 1500);
  };
  
  // Mock pin function
  const handlePin = () => {
    setIsLoading(true);
    setTimeout(() => {
      setIsLoading(false);
      setIsPinned(true);
    }, 800);
  };
  
  // Mock error function
  const simulateError = () => {
    setError(new Error("Connection to IPFS node failed. Please check your configuration."));
  };

  return (
    <div className="p-6 max-w-2xl mx-auto bg-white rounded-xl shadow-md">
      <h2 className="text-2xl font-bold mb-6">Upload NFT to IPFS</h2>
      
      <div className="space-y-4 mb-6">
        <div>
          <label className="block text-sm font-medium text-gray-700 mb-1">Name:</label>
          <input
            type="text"
            value={nftData.name}
            className="w-full p-2 border border-gray-300 rounded"
            readOnly
          />
        </div>
        
        <div>
          <label className="block text-sm font-medium text-gray-700 mb-1">Description:</label>
          <textarea
            value={nftData.description}
            className="w-full p-2 border border-gray-300 rounded"
            rows={3}
            readOnly
          />
        </div>
        
        <div>
          <label className="block text-sm font-medium text-gray-700 mb-1">Image URI:</label>
          <input
            type="text"
            value={nftData.image}
            className="w-full p-2 border border-gray-300 rounded"
            readOnly
          />
        </div>
        
        <div className="bg-gray-50 p-3 rounded">
          <p className="font-medium">Attributes: {nftData.attributes.length} configured</p>
          <ul className="mt-2 text-sm text-gray-600">
            {nftData.attributes.map((attr, index) => (
              <li key={index} className="flex justify-between">
                <span>{attr.trait_type}:</span>
                <span className="font-medium">{attr.value.toString()}</span>
              </li>
            ))}
          </ul>
        </div>
      </div>
      
      <div className="flex space-x-3">
        <button 
          onClick={handleUpload} 
          disabled={isLoading || result}
          className="bg-blue-500 text-white px-4 py-2 rounded disabled:bg-blue-300"
        >
          {isLoading && !result ? 'Uploading...' : 'Upload to IPFS'}
        </button>
        
        <button
          onClick={simulateError}
          className="bg-red-500 text-white px-4 py-2 rounded"
        >
          Simulate Error
        </button>
      </div>
      
      {error && (
        <div className="mt-4 p-3 bg-red-50 border border-red-200 text-red-700 rounded">
          <p>Error: {error.message}</p>
        </div>
      )}
      
      {result && (
        <div className="mt-6 p-4 bg-green-50 border border-green-200 rounded">
          <h3 className="text-lg font-semibold text-green-800">Upload Successful!</h3>
          <div className="mt-2 space-y-2">
            <p><span className="font-medium">CID:</span> {result.cid}</p>
            <p><span className="font-medium">IPFS URL:</span> {result.ipfsUrl}</p>
            <p><span className="font-medium">Gateway URL:</span> {result.gatewayUrl}</p>
          </div>
          
          <button 
            onClick={handlePin} 
            disabled={isLoading || isPinned}
            className="mt-3 bg-green-600 text-white px-4 py-2 rounded disabled:bg-green-300"
          >
            {isPinned ? '✓ Pinned!' : isLoading ? 'Pinning...' : 'Pin to IPFS'}
          </button>
        </div>
      )}
    </div>
  );
};

export default NFTUploadDemo;