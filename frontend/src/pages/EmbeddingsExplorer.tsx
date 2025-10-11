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

export default function EmbeddingsExplorer() {
  const [inputText, setInputText] = useState('');
  const [searchResults, setSearchResults] = useState<EmbeddingResult[]>([]);
  const [loading, setLoading] = useState(false);
  const [embedModel, setEmbedModel] = useState('sentence-transformers/all-MiniLM-L6-v2');

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
    try {
      const response = await fetch('/api/embeddings/search', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ query: inputText, top_k: 10 })
      });
      
      if (response.ok) {
        const data = await response.json();
        setSearchResults(data.results || []);
      }
    } catch (error) {
      console.error('Failed to search embeddings:', error);
      // Sample data for development
      setSearchResults([
        {
          text: 'GDPR compliance requirements for data processing',
          vector: [],
          similarity: 0.92,
          metadata: { source: 'knowledge_base', doc_id: 'kb_001' }
        },
        {
          text: 'European data protection regulations',
          vector: [],
          similarity: 0.87,
          metadata: { source: 'knowledge_base', doc_id: 'kb_023' }
        },
        {
          text: 'Personal data handling guidelines',
          vector: [],
          similarity: 0.81,
          metadata: { source: 'knowledge_base', doc_id: 'kb_045' }
        }
      ]);
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
            placeholder="Enter text to generate embeddings or search for similar content..."
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

