/**
 * Memory Management Page
 * Manages conversation memory, case-based reasoning, and learning feedback
 * Backend routes: 10 memory-related endpoints
 */

import { useState, useEffect } from 'react';
import { apiClient } from '@/services/api';

interface ConversationMemory {
  memory_id: string;
  conversation_id: string;
  agent_id: string;
  memory_type: string;
  importance_level: number;
  content: string;
  created_at: string;
}

interface CaseExample {
  case_id: string;
  domain: string;
  case_type: string;
  problem_description: string;
  solution_description: string;
  confidence_score: number;
  usage_count: number;
}

interface LearningFeedback {
  feedback_id: string;
  agent_type: string;
  feedback_type: string;
  feedback_score: number;
  feedback_text: string;
  created_at: string;
}

interface ErrorState {
  hasError: boolean;
  message: string;
  code?: string;
  timestamp: number;
}

export default function MemoryManagement() {
  const [activeTab, setActiveTab] = useState<'conversations' | 'cases' | 'feedback' | 'consolidation'>('conversations');
  const [memories, setMemories] = useState<ConversationMemory[]>([]);
  const [cases, setCases] = useState<CaseExample[]>([]);
  const [feedback, setFeedback] = useState<LearningFeedback[]>([]);
  const [loading, setLoading] = useState(false);
  const [consolidationStatus, setConsolidationStatus] = useState<any>(null);
  const [error, setError] = useState<ErrorState | null>(null);

  useEffect(() => {
    if (activeTab === 'conversations') {
      loadMemories();
    } else if (activeTab === 'cases') {
      loadCases();
    } else if (activeTab === 'feedback') {
      loadFeedback();
    } else if (activeTab === 'consolidation') {
      loadConsolidationStatus();
    }
  }, [activeTab]);

  const loadMemories = async () => {
    setLoading(true);
    setError(null); // Clear previous errors

    try {
      // Use the API client to get memory graph data
      const data = await apiClient.getMemoryGraph({ limit: '50' });
      
      // Transform the data to match our interface
      if (data && data.nodes) {
        const transformedMemories = data.nodes.map((node: any) => ({
          memory_id: node.id,
          conversation_id: node.conversation_id || node.id,
          agent_id: node.agent_id || 'system',
          memory_type: node.node_type || 'conversation',
          importance_level: node.importance || 5,
          content: node.content || node.description || 'No content available',
          created_at: node.created_at || new Date().toISOString()
        }));
        setMemories(transformedMemories);
      } else {
        setMemories([]);
      }
    } catch (error) {
      console.error('Failed to load memories:', error);

      // Set proper error state instead of mock data
      setError({
        hasError: true,
        message: error instanceof Error ? error.message : 'Unknown error occurred',
        timestamp: Date.now()
      });

      // Clear memories on error
      setMemories([]);
    } finally {
      setLoading(false);
    }
  };

  const loadCases = async () => {
    setLoading(true);
    try {
      // Use the API client to search for case-related memory nodes
      const data = await apiClient.searchMemory({
        query: 'case',
        filters: { node_type: 'case' }
      });
      
      // Transform the data to match our interface
      if (data && data.results) {
        const transformedCases = data.results.map((item: any) => ({
          case_id: item.id,
          domain: item.domain || 'compliance',
          case_type: item.case_type || 'regulatory_check',
          problem_description: item.problem_description || item.content || 'No problem description',
          solution_description: item.solution_description || item.solution || 'No solution description',
          confidence_score: item.confidence_score || item.confidence || 0.5,
          usage_count: item.usage_count || 0
        }));
        setCases(transformedCases);
      } else {
        setCases([]);
      }
    } catch (error) {
      console.error('Failed to load cases:', error);
      setError({
        hasError: true,
        message: error instanceof Error ? error.message : 'Failed to load cases',
        timestamp: Date.now()
      });
      setCases([]);
    } finally {
      setLoading(false);
    }
  };

  const loadFeedback = async () => {
    setLoading(true);
    try {
      // Use the API client to search for feedback-related memory nodes
      const data = await apiClient.searchMemory({
        query: 'feedback',
        filters: { node_type: 'feedback' }
      });
      
      // Transform the data to match our interface
      if (data && data.results) {
        const transformedFeedback = data.results.map((item: any) => ({
          feedback_id: item.id,
          agent_type: item.agent_type || 'system',
          feedback_type: item.feedback_type || 'POSITIVE',
          feedback_score: item.feedback_score || item.score || 0.5,
          feedback_text: item.feedback_text || item.content || 'No feedback text',
          created_at: item.created_at || new Date().toISOString()
        }));
        setFeedback(transformedFeedback);
      } else {
        setFeedback([]);
      }
    } catch (error) {
      console.error('Failed to load feedback:', error);
      setError({
        hasError: true,
        message: error instanceof Error ? error.message : 'Failed to load feedback',
        timestamp: Date.now()
      });
      setFeedback([]);
    } finally {
      setLoading(false);
    }
  };

  const loadConsolidationStatus = async () => {
    setLoading(true);
    try {
      // Use the API client to get memory stats
      const data = await apiClient.getMemoryStats();
      
      // Transform the data to match our interface
      if (data) {
        setConsolidationStatus({
          is_running: data.consolidation_running || false,
          last_consolidation: data.last_consolidation || new Date().toISOString(),
          memories_consolidated: data.memories_consolidated || 0,
          space_freed_mb: data.space_freed_mb || 0,
          next_scheduled_run: data.next_scheduled_run || 'auto'
        });
      } else {
        setConsolidationStatus({
          is_running: false,
          last_consolidation: new Date().toISOString(),
          memories_consolidated: 0,
          space_freed_mb: 0,
          next_scheduled_run: 'auto'
        });
      }
    } catch (error) {
      console.error('Failed to load consolidation status:', error);
      setError({
        hasError: true,
        message: error instanceof Error ? error.message : 'Failed to load consolidation status',
        timestamp: Date.now()
      });
      setConsolidationStatus({
        is_running: false,
        last_consolidation: new Date().toISOString(),
        memories_consolidated: 0,
        space_freed_mb: 0,
        next_scheduled_run: 'auto'
      });
    } finally {
      setLoading(false);
    }
  };

  const runConsolidation = async () => {
    try {
      // Use the API client to create a consolidation task
      await apiClient.createMemoryNode({
        node_type: 'consolidation_task',
        content: 'Memory consolidation task',
        metadata: {
          max_age_days: 90,
          importance_threshold: 0.3
        }
      });
      
      alert('Memory consolidation started successfully');
      loadConsolidationStatus();
    } catch (error) {
      console.error('Failed to run consolidation:', error);
      setError({
        hasError: true,
        message: error instanceof Error ? error.message : 'Failed to start consolidation',
        timestamp: Date.now()
      });
    }
  };

  return (
    <div className="p-6 max-w-7xl mx-auto">
      <div className="mb-6">
        <h1 className="text-3xl font-bold text-gray-900">Memory Management</h1>
        <p className="text-gray-600 mt-2">
          Manage AI conversation memory, case-based reasoning, and learning feedback
        </p>
      </div>

      {/* Tab Navigation */}
      <div className="border-b border-gray-200 mb-6">
        <nav className="-mb-px flex space-x-8">
          {['conversations', 'cases', 'feedback', 'consolidation'].map((tab) => (
            <button
              key={tab}
              onClick={() => setActiveTab(tab as any)}
              className={`
                py-4 px-1 border-b-2 font-medium text-sm capitalize
                ${activeTab === tab
                  ? 'border-blue-500 text-blue-600'
                  : 'border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300'
                }
              `}
            >
              {tab}
            </button>
          ))}
        </nav>
      </div>

      {/* Error Banner */}
      {error && (
        <div className="bg-red-50 border border-red-200 rounded-lg p-4 mb-6">
          <div className="flex items-start">
            <svg className="w-5 h-5 text-red-600 mt-0.5 mr-3" fill="currentColor" viewBox="0 0 20 20">
              <path fillRule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zM8.707 7.293a1 1 0 00-1.414 1.414L8.586 10l-1.293 1.293a1 1 0 101.414 1.414L10 11.414l1.293 1.293a1 1 0 001.414-1.414L11.414 10l1.293-1.293a1 1 0 00-1.414-1.414L10 8.586 8.707 7.293z" clipRule="evenodd" />
            </svg>
            <div>
              <h3 className="text-sm font-medium text-red-800">
                Failed to load data
              </h3>
              <p className="text-sm text-red-700 mt-1">
                {error.message}
              </p>
              <button
                onClick={() => {
                  setError(null);
                  if (activeTab === 'conversations') loadMemories();
                  else if (activeTab === 'cases') loadCases();
                  else if (activeTab === 'feedback') loadFeedback();
                  else if (activeTab === 'consolidation') loadConsolidationStatus();
                }}
                className="mt-2 text-sm text-red-600 hover:text-red-800 font-medium"
              >
                Try Again
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Content */}
      {loading ? (
        <div className="text-center py-12">
          <div className="inline-block animate-spin rounded-full h-8 w-8 border-b-2 border-blue-600"></div>
          <p className="mt-4 text-gray-600">Loading...</p>
        </div>
      ) : (
        <>
          {activeTab === 'conversations' && (
            <div className="space-y-4">
              <div className="flex justify-between items-center mb-4">
                <h2 className="text-xl font-semibold">Conversation Memories ({memories.length})</h2>
                <button
                  onClick={loadMemories}
                  className="px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700"
                >
                  Refresh
                </button>
              </div>
              {memories.map((memory) => (
                <div key={memory.memory_id} className="bg-white p-4 rounded-lg shadow border border-gray-200">
                  <div className="flex justify-between items-start">
                    <div className="flex-1">
                      <div className="flex items-center space-x-2 mb-2">
                        <span className="text-xs font-mono text-gray-500">{memory.memory_id}</span>
                        <span className="px-2 py-1 bg-blue-100 text-blue-800 text-xs rounded">
                          {memory.memory_type}
                        </span>
                        <span className="px-2 py-1 bg-purple-100 text-purple-800 text-xs rounded">
                          Importance: {memory.importance_level}/10
                        </span>
                      </div>
                      <p className="text-gray-900 mb-2">{memory.content}</p>
                      <div className="text-sm text-gray-500">
                        Agent: {memory.agent_id} • Created: {new Date(memory.created_at).toLocaleString()}
                      </div>
                    </div>
                  </div>
                </div>
              ))}
              {memories.length === 0 && (
                <div className="text-center py-12 bg-gray-50 rounded-lg">
                  <p className="text-gray-500">No conversation memories found</p>
                </div>
              )}
            </div>
          )}

          {activeTab === 'cases' && (
            <div className="space-y-4">
              <div className="flex justify-between items-center mb-4">
                <h2 className="text-xl font-semibold">Case-Based Reasoning ({cases.length})</h2>
                <button
                  onClick={loadCases}
                  className="px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700"
                >
                  Refresh
                </button>
              </div>
              {cases.map((case_item) => (
                <div key={case_item.case_id} className="bg-white p-4 rounded-lg shadow border border-gray-200">
                  <div className="flex justify-between items-start mb-3">
                    <div>
                      <span className="text-xs font-mono text-gray-500">{case_item.case_id}</span>
                      <span className="ml-2 px-2 py-1 bg-green-100 text-green-800 text-xs rounded">
                        {case_item.domain}
                      </span>
                      <span className="ml-2 px-2 py-1 bg-yellow-100 text-yellow-800 text-xs rounded">
                        {case_item.case_type}
                      </span>
                    </div>
                    <div className="text-right text-sm">
                      <div className="text-green-600 font-semibold">
                        Confidence: {(case_item.confidence_score * 100).toFixed(0)}%
                      </div>
                      <div className="text-gray-500">Used: {case_item.usage_count} times</div>
                    </div>
                  </div>
                  <div className="mb-2">
                    <span className="text-sm font-semibold text-gray-700">Problem:</span>
                    <p className="text-gray-900">{case_item.problem_description}</p>
                  </div>
                  <div>
                    <span className="text-sm font-semibold text-gray-700">Solution:</span>
                    <p className="text-gray-900">{case_item.solution_description}</p>
                  </div>
                </div>
              ))}
              {cases.length === 0 && (
                <div className="text-center py-12 bg-gray-50 rounded-lg">
                  <p className="text-gray-500">No cases found</p>
                </div>
              )}
            </div>
          )}

          {activeTab === 'feedback' && (
            <div className="space-y-4">
              <div className="flex justify-between items-center mb-4">
                <h2 className="text-xl font-semibold">Learning Feedback ({feedback.length})</h2>
                <button
                  onClick={loadFeedback}
                  className="px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700"
                >
                  Refresh
                </button>
              </div>
              {feedback.map((item) => (
                <div key={item.feedback_id} className="bg-white p-4 rounded-lg shadow border border-gray-200">
                  <div className="flex justify-between items-start">
                    <div className="flex-1">
                      <div className="flex items-center space-x-2 mb-2">
                        <span className="text-xs font-mono text-gray-500">{item.feedback_id}</span>
                        <span className={`px-2 py-1 text-xs rounded ${
                          item.feedback_type === 'POSITIVE' 
                            ? 'bg-green-100 text-green-800' 
                            : 'bg-red-100 text-red-800'
                        }`}>
                          {item.feedback_type}
                        </span>
                        <span className="text-sm text-gray-600">
                          Score: {(item.feedback_score * 100).toFixed(0)}%
                        </span>
                      </div>
                      <p className="text-gray-900 mb-2">{item.feedback_text}</p>
                      <div className="text-sm text-gray-500">
                        Agent: {item.agent_type} • {new Date(item.created_at).toLocaleString()}
                      </div>
                    </div>
                  </div>
                </div>
              ))}
              {feedback.length === 0 && (
                <div className="text-center py-12 bg-gray-50 rounded-lg">
                  <p className="text-gray-500">No feedback found</p>
                </div>
              )}
            </div>
          )}

          {activeTab === 'consolidation' && consolidationStatus && (
            <div className="space-y-6">
              <div className="bg-white p-6 rounded-lg shadow border border-gray-200">
                <h2 className="text-xl font-semibold mb-4">Memory Consolidation Status</h2>
                <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-6">
                  <div className="bg-gray-50 p-4 rounded-lg">
                    <div className="text-sm text-gray-600">Status</div>
                    <div className="text-2xl font-bold text-gray-900">
                      {consolidationStatus.is_running ? 'Running' : 'Idle'}
                    </div>
                  </div>
                  <div className="bg-gray-50 p-4 rounded-lg">
                    <div className="text-sm text-gray-600">Memories Consolidated</div>
                    <div className="text-2xl font-bold text-gray-900">
                      {consolidationStatus.memories_consolidated}
                    </div>
                  </div>
                  <div className="bg-gray-50 p-4 rounded-lg">
                    <div className="text-sm text-gray-600">Space Freed</div>
                    <div className="text-2xl font-bold text-gray-900">
                      {consolidationStatus.space_freed_mb || 0} MB
                    </div>
                  </div>
                  <div className="bg-gray-50 p-4 rounded-lg">
                    <div className="text-sm text-gray-600">Next Run</div>
                    <div className="text-lg font-bold text-gray-900">
                      {consolidationStatus.next_scheduled_run}
                    </div>
                  </div>
                </div>
                <div className="mb-4">
                  <span className="text-sm text-gray-600">Last Consolidation:</span>
                  <span className="ml-2 text-gray-900">
                    {consolidationStatus.last_consolidation === 'never' 
                      ? 'Never' 
                      : new Date(consolidationStatus.last_consolidation).toLocaleString()
                    }
                  </span>
                </div>
                <button
                  onClick={runConsolidation}
                  disabled={consolidationStatus.is_running}
                  className="px-6 py-3 bg-blue-600 text-white rounded-lg hover:bg-blue-700 disabled:bg-gray-400 disabled:cursor-not-allowed"
                >
                  {consolidationStatus.is_running ? 'Consolidation Running...' : 'Run Consolidation Now'}
                </button>
              </div>
            </div>
          )}
        </>
      )}
    </div>
  );
}

