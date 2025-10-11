/**
 * Function Calling Debugger Page
 * Debug LLM function calls, audit execution, and metrics
 * Backend routes: /api/llm/function-calls, /api/llm/function-audit, /api/llm/function-metrics
 */

import { useState, useEffect } from 'react';

interface FunctionCall {
  call_id: string;
  function_name: string;
  arguments: any;
  result: any;
  success: boolean;
  execution_time_ms: number;
  timestamp: string;
  model: string;
}

interface FunctionMetrics {
  total_calls: number;
  success_rate: number;
  avg_execution_time: number;
  error_count: number;
}

export default function FunctionCallingDebugger() {
  const [functionCalls, setFunctionCalls] = useState<FunctionCall[]>([]);
  const [metrics, setMetrics] = useState<FunctionMetrics | null>(null);
  const [selectedCall, setSelectedCall] = useState<FunctionCall | null>(null);
  const [filter, setFilter] = useState<'all' | 'success' | 'error'>('all');

  useEffect(() => {
    loadFunctionCalls();
    loadMetrics();
  }, [filter]);

  const loadFunctionCalls = async () => {
    try {
      const response = await fetch(`/api/llm/function-calls?filter=${filter}&limit=50`);
      if (response.ok) {
        const data = await response.json();
        setFunctionCalls(data.calls || []);
      }
    } catch (error) {
      console.error('Failed to load function calls:', error);
      // Sample data for development
      setFunctionCalls([
        {
          call_id: 'fc_001',
          function_name: 'assess_compliance_risk',
          arguments: { transaction_id: 'tx_12345', jurisdiction: 'EU' },
          result: { risk_score: 0.23, compliant: true },
          success: true,
          execution_time_ms: 145,
          timestamp: new Date().toISOString(),
          model: 'gpt-4'
        },
        {
          call_id: 'fc_002',
          function_name: 'query_knowledge_base',
          arguments: { query: 'GDPR requirements', max_results: 5 },
          result: { error: 'Timeout connecting to database' },
          success: false,
          execution_time_ms: 3000,
          timestamp: new Date(Date.now() - 60000).toISOString(),
          model: 'gpt-3.5-turbo'
        }
      ]);
    }
  };

  const loadMetrics = async () => {
    try {
      const response = await fetch('/api/llm/function-metrics');
      if (response.ok) {
        const data = await response.json();
        setMetrics(data.metrics);
      }
    } catch (error) {
      console.error('Failed to load metrics:', error);
      setMetrics({
        total_calls: 8456,
        success_rate: 0.94,
        avg_execution_time: 287,
        error_count: 507
      });
    }
  };

  return (
    <div className="p-6 max-w-7xl mx-auto">
      <div className="mb-6">
        <h1 className="text-3xl font-bold text-gray-900">Function Calling Debugger</h1>
        <p className="text-gray-600 mt-2">
          Debug and monitor LLM function call execution
        </p>
      </div>

      {/* Metrics */}
      {metrics && (
        <div className="grid grid-cols-1 md:grid-cols-4 gap-6 mb-6">
          <div className="bg-white p-6 rounded-lg shadow border border-gray-200">
            <h3 className="text-sm font-medium text-gray-600 mb-2">Total Calls</h3>
            <p className="text-3xl font-bold text-gray-900">{metrics.total_calls.toLocaleString()}</p>
          </div>
          <div className="bg-white p-6 rounded-lg shadow border border-gray-200">
            <h3 className="text-sm font-medium text-gray-600 mb-2">Success Rate</h3>
            <p className="text-3xl font-bold text-green-600">{(metrics.success_rate * 100).toFixed(1)}%</p>
          </div>
          <div className="bg-white p-6 rounded-lg shadow border border-gray-200">
            <h3 className="text-sm font-medium text-gray-600 mb-2">Avg Execution</h3>
            <p className="text-3xl font-bold text-blue-600">{metrics.avg_execution_time}ms</p>
          </div>
          <div className="bg-white p-6 rounded-lg shadow border border-gray-200">
            <h3 className="text-sm font-medium text-gray-600 mb-2">Errors</h3>
            <p className="text-3xl font-bold text-red-600">{metrics.error_count}</p>
          </div>
        </div>
      )}

      {/* Filter */}
      <div className="mb-6 flex space-x-2">
        {(['all', 'success', 'error'] as const).map((f) => (
          <button
            key={f}
            onClick={() => setFilter(f)}
            className={`px-4 py-2 rounded-lg capitalize ${
              filter === f
                ? 'bg-blue-600 text-white'
                : 'bg-white text-gray-700 border border-gray-300 hover:bg-gray-50'
            }`}
          >
            {f}
          </button>
        ))}
      </div>

      {/* Function Calls List */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <div className="space-y-3">
          <h2 className="text-xl font-semibold mb-4">Function Calls ({functionCalls.length})</h2>
          {functionCalls.map((call) => (
            <div
              key={call.call_id}
              onClick={() => setSelectedCall(call)}
              className={`bg-white p-4 rounded-lg shadow border cursor-pointer transition-colors ${
                selectedCall?.call_id === call.call_id
                  ? 'border-blue-500 bg-blue-50'
                  : 'border-gray-200 hover:border-blue-300'
              }`}
            >
              <div className="flex justify-between items-start mb-2">
                <span className="font-mono text-sm font-semibold text-gray-900">
                  {call.function_name}
                </span>
                <span className={`px-2 py-1 text-xs rounded ${
                  call.success ? 'bg-green-100 text-green-800' : 'bg-red-100 text-red-800'
                }`}>
                  {call.success ? 'SUCCESS' : 'ERROR'}
                </span>
              </div>
              <div className="flex justify-between text-sm text-gray-600">
                <span>{call.model}</span>
                <span>{call.execution_time_ms}ms</span>
              </div>
              <div className="text-xs text-gray-500 mt-1">
                {new Date(call.timestamp).toLocaleString()}
              </div>
            </div>
          ))}
          {functionCalls.length === 0 && (
            <div className="text-center py-12 bg-gray-50 rounded-lg">
              <p className="text-gray-500">No function calls found</p>
            </div>
          )}
        </div>

        {/* Details Panel */}
        <div className="bg-white p-6 rounded-lg shadow border border-gray-200">
          <h2 className="text-xl font-semibold mb-4">Call Details</h2>
          {selectedCall ? (
            <div className="space-y-4">
              <div>
                <h3 className="text-sm font-medium text-gray-700 mb-2">Function Name</h3>
                <code className="block bg-gray-100 p-2 rounded text-sm">
                  {selectedCall.function_name}
                </code>
              </div>
              
              <div>
                <h3 className="text-sm font-medium text-gray-700 mb-2">Arguments</h3>
                <pre className="bg-gray-100 p-3 rounded text-xs overflow-auto max-h-40">
                  {JSON.stringify(selectedCall.arguments, null, 2)}
                </pre>
              </div>

              <div>
                <h3 className="text-sm font-medium text-gray-700 mb-2">Result</h3>
                <pre className="bg-gray-100 p-3 rounded text-xs overflow-auto max-h-40">
                  {JSON.stringify(selectedCall.result, null, 2)}
                </pre>
              </div>

              <div className="grid grid-cols-2 gap-4 pt-4 border-t border-gray-200">
                <div>
                  <span className="text-sm text-gray-600">Model:</span>
                  <p className="font-semibold text-gray-900">{selectedCall.model}</p>
                </div>
                <div>
                  <span className="text-sm text-gray-600">Execution Time:</span>
                  <p className="font-semibold text-gray-900">{selectedCall.execution_time_ms}ms</p>
                </div>
                <div>
                  <span className="text-sm text-gray-600">Status:</span>
                  <p className={`font-semibold ${selectedCall.success ? 'text-green-600' : 'text-red-600'}`}>
                    {selectedCall.success ? 'SUCCESS' : 'ERROR'}
                  </p>
                </div>
                <div>
                  <span className="text-sm text-gray-600">Timestamp:</span>
                  <p className="font-semibold text-gray-900 text-xs">
                    {new Date(selectedCall.timestamp).toLocaleString()}
                  </p>
                </div>
              </div>
            </div>
          ) : (
            <div className="text-center py-12 text-gray-500">
              Select a function call to view details
            </div>
          )}
        </div>
      </div>
    </div>
  );
}

