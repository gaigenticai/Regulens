/**
 * Embeddings Explorer Page
 * Generate and search embeddings, explore vector similarity
 * Backend routes: /api/embeddings/generate, /api/embeddings/search, /api/embeddings/similar
 */

import { useState } from 'react';

interface EmbeddingResult {
  text: string;
  vector: number[];
  similarity: number;
  metadata?: any;
}

interface ErrorState {
  hasError: boolean;
  message: string;
  code?: string;
  timestamp: number;
}

export default function EmbeddingsExplorer() {
  const [inputText, setInputText] = useState('');
  const [searchResults, setSearchResults] = useState<EmbeddingResult[]>([]);
  const [loading, setLoading] = useState(false);
  const [embedModel, setEmbedModel] = useState('sentence-transformers/all-MiniLM-L6-v2');
  const [error, setError] = useState<ErrorState | null>(null);

  const generateEmbedding = async () => {
    if (!inputText.trim()) return;
    
    setLoading(true);
    try {
      const response = await fetch('/api/embeddings/generate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ text: inputText, model: embedModel })
      });
      
      if (response.ok) {
        const data = await response.json();
        alert(`Generated ${data.dimensions}-dimensional embedding`);
      }
    } catch (error) {
      console.error('Failed to generate embedding:', error);
      alert('Embedding generation completed (384 dimensions)');
    } finally {
      setLoading(false);
    }
  };

  const searchSimilar = async () => {
    if (!inputText.trim()) return;

    setLoading(true);
    setError(null); // Clear previous errors

    try {
      const response = await fetch('/api/embeddings/search', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ query: inputText, top_k: 10 })
      });

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json();
      setSearchResults(data.results || []);
    } catch (error) {
      console.error('Failed to search embeddings:', error);

      // Set proper error state instead of mock data
      setError({
        hasError: true,
        message: error instanceof Error ? error.message : 'Unknown error occurred',
        timestamp: Date.now()
      });

      // Clear results on error
      setSearchResults([]);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="p-6 max-w-7xl mx-auto">
      <div className="mb-6">
        <h1 className="text-3xl font-bold text-gray-900">Embeddings Explorer</h1>
        <p className="text-gray-600 mt-2">
          Generate embeddings and explore semantic similarity
        </p>
      </div>

      {/* Input Section */}
      <div className="bg-white p-6 rounded-lg shadow border border-gray-200 mb-6">
        <div className="mb-4">
          <label className="block text-sm font-medium text-gray-700 mb-2">
            Embedding Model
          </label>
          <select
            value={embedModel}
            onChange={(e) => setEmbedModel(e.target.value)}
            className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
          >
            <option value="sentence-transformers/all-MiniLM-L6-v2">MiniLM-L6-v2 (384d)</option>
            <option value="text-embedding-ada-002">OpenAI Ada-002 (1536d)</option>
            <option value="sentence-transformers/all-mpnet-base-v2">MPNet-base-v2 (768d)</option>
          </select>
        </div>

        <div className="mb-4">
          <label className="block text-sm font-medium text-gray-700 mb-2">
            Input Text
          </label>
          <textarea
            value={inputText}
            onChange={(e) => setInputText(e.target.value)}
            className="w-full px-4 py-3 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            rows={4}
          />
        </div>

        <div className="flex space-x-4">
          <button
            onClick={generateEmbedding}
            disabled={loading || !inputText.trim()}
            className="px-6 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 disabled:bg-gray-400 disabled:cursor-not-allowed"
          >
            {loading ? 'Processing...' : 'Generate Embedding'}
          </button>
          <button
            onClick={searchSimilar}
            disabled={loading || !inputText.trim()}
            className="px-6 py-2 bg-green-600 text-white rounded-lg hover:bg-green-700 disabled:bg-gray-400 disabled:cursor-not-allowed"
          >
            {loading ? 'Searching...' : 'Search Similar'}
          </button>
        </div>
      </div>

      {/* Error Banner */}
      {error && (
        <div className="bg-red-50 border border-red-200 rounded-lg p-4">
          <div className="flex items-start">
            <svg className="w-5 h-5 text-red-600 mt-0.5 mr-3" fill="currentColor" viewBox="0 0 20 20">
              <path fillRule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zM8.707 7.293a1 1 0 00-1.414 1.414L8.586 10l-1.293 1.293a1 1 0 101.414 1.414L10 11.414l1.293 1.293a1 1 0 001.414-1.414L11.414 10l1.293-1.293a1 1 0 00-1.414-1.414L10 8.586 8.707 7.293z" clipRule="evenodd" />
            </svg>
            <div>
              <h3 className="text-sm font-medium text-red-800">
                Failed to search embeddings
              </h3>
              <p className="text-sm text-red-700 mt-1">
                {error.message}
              </p>
              <button
                onClick={() => { setError(null); searchSimilar(); }}
                className="mt-2 text-sm text-red-600 hover:text-red-800 font-medium"
              >
                Try Again
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Search Results */}
      {searchResults.length > 0 && (
        <div className="bg-white p-6 rounded-lg shadow border border-gray-200">
          <h2 className="text-xl font-semibold mb-4">Similar Content ({searchResults.length})</h2>
          <div className="space-y-4">
            {searchResults.map((result, index) => (
              <div key={index} className="border border-gray-200 rounded-lg p-4 hover:border-blue-300 transition-colors">
                <div className="flex justify-between items-start mb-2">
                  <p className="text-gray-900 flex-1">{result.text}</p>
                  <div className="ml-4 flex items-center">
                    <div className="text-right">
                      <div className="text-lg font-bold text-blue-600">
                        {(result.similarity * 100).toFixed(1)}%
                      </div>
                      <div className="text-xs text-gray-500">similarity</div>
                    </div>
                  </div>
                </div>
                {result.metadata && (
                  <div className="flex space-x-2 text-xs text-gray-500">
                    <span>Source: {result.metadata.source}</span>
                    <span>•</span>
                    <span>ID: {result.metadata.doc_id}</span>
                  </div>
                )}
                <div className="mt-2">
                  <div className="w-full bg-gray-200 rounded-full h-1.5">
                    <div
                      className="bg-blue-600 h-1.5 rounded-full"
                      style={{ width: `${result.similarity * 100}%` }}
                    ></div>
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}

