/**
 * Knowledge Base Page
 * Semantic search with vector embeddings and RAG
 * NO MOCKS - Real vector search and AI-powered retrieval
 */

import React, { useState } from 'react';
import {
  BookOpen,
  Search,
  Plus,
  Filter,
  Sparkles,
  FileText,
  Tag,
  Database,
  Lightbulb
} from 'lucide-react';
import {
  useKnowledgeSearch,
  useKnowledgeEntries,
  useKnowledgeStats,
  useAskKnowledgeBase,
  useCreateKnowledgeEntry
} from '@/hooks/useKnowledgeBase';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { clsx } from 'clsx';

const KnowledgeBase: React.FC = () => {
  const [searchQuery, setSearchQuery] = useState('');
  const [categoryFilter, setCategoryFilter] = useState<string>('all');
  const [showCreateModal, setShowCreateModal] = useState(false);
  const [askMode, setAskMode] = useState(false);
  const [aiQuestion, setAiQuestion] = useState('');

  const { data: searchResults = [], isLoading: isSearching } = useKnowledgeSearch(
    searchQuery,
    {
      limit: 20,
      threshold: 0.7,
      category: categoryFilter === 'all' ? undefined : categoryFilter,
    }
  );

  const { data: entries = [], isLoading: isLoadingEntries } = useKnowledgeEntries({
    limit: 50,
    category: categoryFilter === 'all' ? undefined : categoryFilter,
    sortBy: 'relevance',
  });

  const { data: stats } = useKnowledgeStats();
  const askMutation = useAskKnowledgeBase();
  const createMutation = useCreateKnowledgeEntry();

  const [newEntry, setNewEntry] = useState({
    title: '',
    content: '',
    category: 'general',
    tags: [] as string[],
  });

  const [tagInput, setTagInput] = useState('');

  const handleAskAI = async () => {
    if (!aiQuestion.trim()) return;

    try {
      await askMutation.mutateAsync({
        question: aiQuestion,
        maxResults: 5,
        temperature: 0.7,
      });
    } catch (error) {
      console.error('AI query failed:', error);
    }
  };

  const handleCreateEntry = async () => {
    if (!newEntry.title || !newEntry.content) return;

    try {
      await createMutation.mutateAsync(newEntry);
      setShowCreateModal(false);
      setNewEntry({ title: '', content: '', category: 'general', tags: [] });
    } catch (error) {
      console.error('Failed to create entry:', error);
    }
  };

  const handleAddTag = () => {
    if (tagInput.trim() && !newEntry.tags.includes(tagInput.trim())) {
      setNewEntry({
        ...newEntry,
        tags: [...newEntry.tags, tagInput.trim()],
      });
      setTagInput('');
    }
  };

  const handleRemoveTag = (tag: string) => {
    setNewEntry({
      ...newEntry,
      tags: newEntry.tags.filter((t) => t !== tag),
    });
  };

  const displayEntries = searchQuery.length >= 3 ? searchResults : entries;
  const isLoading = isSearching || isLoadingEntries;

  const categories = ['all', 'compliance', 'regulations', 'best-practices', 'case-studies', 'guidelines', 'technical'];

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-gray-900 flex items-center gap-2">
            <BookOpen className="w-7 h-7 text-blue-600" />
            Knowledge Base
          </h1>
          <p className="text-gray-600 mt-1">Semantic search with vector embeddings & AI retrieval</p>
        </div>

        <div className="flex items-center gap-3">
          <button
            onClick={() => setAskMode(!askMode)}
            className={clsx(
              'flex items-center gap-2 px-4 py-2 rounded-lg transition-colors',
              askMode ? 'bg-purple-600 text-white' : 'border border-gray-300 text-gray-700 hover:bg-gray-50'
            )}
          >
            <Sparkles className="w-5 h-5" />
            Ask AI
          </button>

          <button
            onClick={() => setShowCreateModal(true)}
            className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors"
          >
            <Plus className="w-5 h-5" />
            Add Entry
          </button>
        </div>
      </div>

      {/* Statistics */}
      {stats && (
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Total Entries</p>
            <p className="text-2xl font-bold text-gray-900 mt-1">{stats.totalEntries}</p>
          </div>
          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Categories</p>
            <p className="text-2xl font-bold text-blue-600 mt-1">{stats.totalCategories}</p>
          </div>
          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Tags</p>
            <p className="text-2xl font-bold text-purple-600 mt-1">{stats.totalTags}</p>
          </div>
          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Last Updated</p>
            <p className="text-sm font-medium text-gray-900 mt-2">
              {new Date(stats.lastUpdated).toLocaleDateString()}
            </p>
          </div>
        </div>
      )}

      {/* AI Ask Mode */}
      {askMode && (
        <div className="bg-gradient-to-r from-purple-50 to-blue-50 border border-purple-200 rounded-lg p-6">
          <div className="flex items-start gap-3">
            <Sparkles className="w-6 h-6 text-purple-600 mt-1" />
            <div className="flex-1">
              <h3 className="text-lg font-semibold text-gray-900 mb-2">Ask the Knowledge Base</h3>
              <p className="text-sm text-gray-600 mb-4">
                RAG-powered AI answers based on knowledge entries
              </p>

              <div className="flex gap-3">
                <input
                  type="text"
                  value={aiQuestion}
                  onChange={(e) => setAiQuestion(e.target.value)}
                  placeholder="Ask about compliance, regulations..."
                  className="flex-1 px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-purple-500"
                  onKeyDown={(e) => e.key === 'Enter' && handleAskAI()}
                />
                <button
                  onClick={handleAskAI}
                  disabled={askMutation.isPending || !aiQuestion.trim()}
                  className="px-6 py-2 bg-purple-600 text-white rounded-lg hover:bg-purple-700 disabled:opacity-50"
                >
                  {askMutation.isPending ? 'Thinking...' : 'Ask'}
                </button>
              </div>

              {askMutation.isSuccess && askMutation.data && (
                <div className="mt-4 p-4 bg-white rounded-lg border border-purple-200">
                  <div className="flex items-center gap-2 mb-2">
                    <Lightbulb className="w-5 h-5 text-purple-600" />
                    <h4 className="font-semibold text-gray-900">AI Answer:</h4>
                  </div>
                  <p className="text-gray-700 mb-3">{askMutation.data.answer}</p>
                  <div className="text-sm text-gray-600">
                    Confidence: {(askMutation.data.confidence * 100).toFixed(1)}% • {askMutation.data.sources.length} sources
                  </div>
                </div>
              )}
            </div>
          </div>
        </div>
      )}

      {/* Search */}
      <div className="bg-white rounded-lg border border-gray-200 p-4">
        <div className="flex gap-4">
          <div className="flex-1 relative">
            <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-5 h-5 text-gray-400" />
            <input
              type="text"
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Semantic search (min 3 chars)..."
              className="w-full pl-10 pr-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
            />
          </div>

          <select
            value={categoryFilter}
            onChange={(e) => setCategoryFilter(e.target.value)}
            className="px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
          >
            {categories.map((cat) => (
              <option key={cat} value={cat}>
                {cat === 'all' ? 'All Categories' : cat.charAt(0).toUpperCase() + cat.slice(1)}
              </option>
            ))}
          </select>
        </div>
      </div>

      {/* Entries */}
      <div className="bg-white rounded-lg border border-gray-200 p-6">
        <h2 className="text-lg font-semibold text-gray-900 mb-4">
          {searchQuery.length >= 3 ? 'Search Results' : 'All Entries'}
        </h2>

        {isLoading ? (
          <LoadingSpinner message="Loading entries..." />
        ) : displayEntries.length === 0 ? (
          <div className="text-center py-12">
            <BookOpen className="w-12 h-12 text-gray-400 mx-auto mb-3" />
            <p className="text-gray-600">No entries found</p>
          </div>
        ) : (
          <div className="space-y-4">
            {displayEntries.map((entry) => (
              <div key={entry.id} className="p-4 border border-gray-200 rounded-lg hover:border-blue-300 transition-all">
                <div className="flex justify-between mb-2">
                  <h3 className="text-lg font-semibold text-gray-900">{entry.title}</h3>
                  {entry.relevanceScore && (
                    <span className="text-sm text-blue-600">{(entry.relevanceScore * 100).toFixed(0)}% match</span>
                  )}
                </div>
                <p className="text-sm text-gray-600 mb-3 line-clamp-2">{entry.content}</p>
                <div className="flex gap-4 text-xs text-gray-500">
                  <span className="flex items-center gap-1">
                    <FileText className="w-3 h-3" /> {entry.category}
                  </span>
                  <span className="flex items-center gap-1">
                    <Tag className="w-3 h-3" /> {entry.tags.length} tags
                  </span>
                  <span>Updated: {new Date(entry.updatedAt).toLocaleDateString()}</span>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Create Modal */}
      {showCreateModal && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center p-4 z-50">
          <div className="bg-white rounded-lg max-w-2xl w-full p-6">
            <h2 className="text-xl font-bold mb-4">Create Knowledge Entry</h2>
            <div className="space-y-4">
              <input
                type="text"
                value={newEntry.title}
                onChange={(e) => setNewEntry({ ...newEntry, title: e.target.value })}
                className="w-full px-3 py-2 border rounded-lg"
                placeholder="Title..."
              />
              <textarea
                value={newEntry.content}
                onChange={(e) => setNewEntry({ ...newEntry, content: e.target.value })}
                rows={8}
                className="w-full px-3 py-2 border rounded-lg"
                placeholder="Content..."
              />
              <select
                value={newEntry.category}
                onChange={(e) => setNewEntry({ ...newEntry, category: e.target.value })}
                className="w-full px-3 py-2 border rounded-lg"
              >
                {categories.filter(c => c !== 'all').map(cat => (
                  <option key={cat} value={cat}>{cat}</option>
                ))}
              </select>
            </div>
            <div className="flex justify-end gap-3 mt-6">
              <button onClick={() => setShowCreateModal(false)} className="px-4 py-2 border rounded-lg">Cancel</button>
              <button
                onClick={handleCreateEntry}
                disabled={!newEntry.title || !newEntry.content}
                className="px-4 py-2 bg-blue-600 text-white rounded-lg disabled:opacity-50"
              >
                Create
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default KnowledgeBase;
