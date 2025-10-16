import React, { useState, useMemo } from 'react';
import { Filter, Search, Download, BarChart3, TrendingUp, TrendingDown, CheckCircle, XCircle, Clock, AlertTriangle } from 'lucide-react';

interface TestResult {
  id: string;
  execution_id: string;
  suite_id: string;
  suite_name: string;
  test_id: string;
  tool_name: string;
  operation: string;
  status: 'passed' | 'failed' | 'skipped' | 'error';
  start_time: string;
  end_time?: string;
  execution_time_ms?: number;
  error_message?: string;
  result_data?: any;
  category: string;
  parameters?: Record<string, any>;
}

interface TestResultsProps {
  results?: TestResult[];
  onResultSelect?: (result: TestResult) => void;
}

export const TestResults: React.FC<TestResultsProps> = ({
  results = [],
  onResultSelect
}) => {
  const [searchTerm, setSearchTerm] = useState('');
  const [statusFilter, setStatusFilter] = useState<string>('all');
  const [categoryFilter, setCategoryFilter] = useState<string>('all');
  const [sortBy, setSortBy] = useState<'start_time' | 'execution_time' | 'tool_name'>('start_time');
  const [sortOrder, setSortOrder] = useState<'asc' | 'desc'>('desc');
  const [selectedResult, setSelectedResult] = useState<TestResult | null>(null);

  // Mock data for demonstration
  const mockResults: TestResult[] = [
    {
      id: 'result_1',
      execution_id: 'exec_1',
      suite_id: 'suite_1',
      suite_name: 'Analytics Test Suite',
      test_id: 'test_1',
      tool_name: 'data_analyzer',
      operation: 'analyze_dataset',
      status: 'passed',
      start_time: '2024-01-15T10:00:00Z',
      end_time: '2024-01-15T10:00:45Z',
      execution_time_ms: 45000,
      category: 'analytics',
      result_data: { processed_rows: 1000, accuracy: 0.95 }
    },
    {
      id: 'result_2',
      execution_id: 'exec_1',
      suite_id: 'suite_1',
      suite_name: 'Analytics Test Suite',
      test_id: 'test_2',
      tool_name: 'report_generator',
      operation: 'generate_report',
      status: 'failed',
      start_time: '2024-01-15T10:01:00Z',
      end_time: '2024-01-15T10:01:12Z',
      execution_time_ms: 12000,
      error_message: 'Report generation failed: missing template data',
      category: 'analytics'
    },
    {
      id: 'result_3',
      execution_id: 'exec_2',
      suite_id: 'suite_2',
      suite_name: 'Security Test Suite',
      test_id: 'test_3',
      tool_name: 'vulnerability_scanner',
      operation: 'scan_vulnerabilities',
      status: 'passed',
      start_time: '2024-01-15T11:00:00Z',
      end_time: '2024-01-15T11:02:30Z',
      execution_time_ms: 150000,
      category: 'security',
      result_data: { vulnerabilities_found: 0, scan_coverage: 100 }
    },
    {
      id: 'result_4',
      execution_id: 'exec_3',
      suite_id: 'suite_3',
      suite_name: 'Workflow Test Suite',
      test_id: 'test_4',
      tool_name: 'task_automator',
      operation: 'automate_task',
      status: 'error',
      start_time: '2024-01-15T12:00:00Z',
      end_time: '2024-01-15T12:00:05Z',
      execution_time_ms: 5000,
      error_message: 'Connection timeout to external service',
      category: 'workflow'
    }
  ];

  const displayResults = results.length > 0 ? results : mockResults;

  // Filter and sort results
  const filteredAndSortedResults = useMemo(() => {
    let filtered = displayResults.filter(result => {
      const matchesSearch = searchTerm === '' ||
        result.tool_name.toLowerCase().includes(searchTerm.toLowerCase()) ||
        result.operation.toLowerCase().includes(searchTerm.toLowerCase()) ||
        result.suite_name.toLowerCase().includes(searchTerm.toLowerCase());

      const matchesStatus = statusFilter === 'all' || result.status === statusFilter;
      const matchesCategory = categoryFilter === 'all' || result.category === categoryFilter;

      return matchesSearch && matchesStatus && matchesCategory;
    });

    // Sort results
    filtered.sort((a, b) => {
      let aValue: any, bValue: any;

      switch (sortBy) {
        case 'start_time':
          aValue = new Date(a.start_time).getTime();
          bValue = new Date(b.start_time).getTime();
          break;
        case 'execution_time':
          aValue = a.execution_time_ms || 0;
          bValue = b.execution_time_ms || 0;
          break;
        case 'tool_name':
          aValue = a.tool_name;
          bValue = b.tool_name;
          break;
        default:
          return 0;
      }

      if (sortOrder === 'asc') {
        return aValue > bValue ? 1 : -1;
      } else {
        return aValue < bValue ? 1 : -1;
      }
    });

    return filtered;
  }, [displayResults, searchTerm, statusFilter, categoryFilter, sortBy, sortOrder]);

  // Statistics
  const stats = useMemo(() => {
    const total = filteredAndSortedResults.length;
    const passed = filteredAndSortedResults.filter(r => r.status === 'passed').length;
    const failed = filteredAndSortedResults.filter(r => r.status === 'failed').length;
    const error = filteredAndSortedResults.filter(r => r.status === 'error').length;
    const skipped = filteredAndSortedResults.filter(r => r.status === 'skipped').length;

    const avgExecutionTime = filteredAndSortedResults.reduce((sum, r) => sum + (r.execution_time_ms || 0), 0) / total;

    return { total, passed, failed, error, skipped, avgExecutionTime };
  }, [filteredAndSortedResults]);

  const getStatusIcon = (status: string) => {
    switch (status) {
      case 'passed':
        return <CheckCircle className="w-4 h-4 text-green-500" />;
      case 'failed':
        return <XCircle className="w-4 h-4 text-red-500" />;
      case 'error':
        return <AlertTriangle className="w-4 h-4 text-orange-500" />;
      case 'skipped':
        return <Clock className="w-4 h-4 text-gray-400" />;
      default:
        return <Clock className="w-4 h-4 text-gray-400" />;
    }
  };

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'passed':
        return 'text-green-600 bg-green-100';
      case 'failed':
        return 'text-red-600 bg-red-100';
      case 'error':
        return 'text-orange-600 bg-orange-100';
      case 'skipped':
        return 'text-gray-600 bg-gray-100';
      default:
        return 'text-gray-600 bg-gray-100';
    }
  };

  const handleResultClick = (result: TestResult) => {
    setSelectedResult(result);
    if (onResultSelect) {
      onResultSelect(result);
    }
  };

  const exportResults = () => {
    const csvContent = [
      ['Suite Name', 'Tool Name', 'Operation', 'Status', 'Start Time', 'End Time', 'Execution Time (ms)', 'Error Message'].join(','),
      ...filteredAndSortedResults.map(result => [
        result.suite_name,
        result.tool_name,
        result.operation,
        result.status,
        result.start_time,
        result.end_time || '',
        result.execution_time_ms?.toString() || '',
        result.error_message || ''
      ].join(','))
    ].join('\n');

    const blob = new Blob([csvContent], { type: 'text/csv' });
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `test-results-${new Date().toISOString().split('T')[0]}.csv`;
    a.click();
    window.URL.revokeObjectURL(url);
  };

  return (
    <div className="space-y-6">
      {/* Header with Stats */}
      <div className="grid grid-cols-2 md:grid-cols-5 gap-4">
        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-4">
          <div className="flex items-center gap-2">
            <BarChart3 className="w-5 h-5 text-blue-500" />
            <span className="text-sm font-medium text-gray-700">Total</span>
          </div>
          <div className="text-2xl font-bold text-gray-900 mt-1">{stats.total}</div>
        </div>
        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-4">
          <div className="flex items-center gap-2">
            <CheckCircle className="w-5 h-5 text-green-500" />
            <span className="text-sm font-medium text-gray-700">Passed</span>
          </div>
          <div className="text-2xl font-bold text-green-600 mt-1">{stats.passed}</div>
        </div>
        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-4">
          <div className="flex items-center gap-2">
            <XCircle className="w-5 h-5 text-red-500" />
            <span className="text-sm font-medium text-gray-700">Failed</span>
          </div>
          <div className="text-2xl font-bold text-red-600 mt-1">{stats.failed}</div>
        </div>
        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-4">
          <div className="flex items-center gap-2">
            <AlertTriangle className="w-5 h-5 text-orange-500" />
            <span className="text-sm font-medium text-gray-700">Errors</span>
          </div>
          <div className="text-2xl font-bold text-orange-600 mt-1">{stats.error}</div>
        </div>
        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-4">
          <div className="flex items-center gap-2">
            <TrendingUp className="w-5 h-5 text-purple-500" />
            <span className="text-sm font-medium text-gray-700">Avg Time</span>
          </div>
          <div className="text-lg font-bold text-purple-600 mt-1">
            {stats.avgExecutionTime ? `${Math.round(stats.avgExecutionTime)}ms` : 'N/A'}
          </div>
        </div>
      </div>

      {/* Filters and Search */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
        <div className="flex flex-col md:flex-row gap-4 mb-4">
          <div className="flex-1">
            <div className="relative">
              <Search className="w-4 h-4 absolute left-3 top-1/2 transform -translate-y-1/2 text-gray-400" />
              <input
                type="text"
                placeholder="Search by tool name, operation, or suite..."
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
                className="w-full pl-10 pr-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
              />
            </div>
          </div>
          <div className="flex gap-2">
            <select
              value={statusFilter}
              onChange={(e) => setStatusFilter(e.target.value)}
              className="px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            >
              <option value="all">All Status</option>
              <option value="passed">Passed</option>
              <option value="failed">Failed</option>
              <option value="error">Error</option>
              <option value="skipped">Skipped</option>
            </select>
            <select
              value={categoryFilter}
              onChange={(e) => setCategoryFilter(e.target.value)}
              className="px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            >
              <option value="all">All Categories</option>
              <option value="analytics">Analytics</option>
              <option value="workflow">Workflow</option>
              <option value="security">Security</option>
              <option value="monitoring">Monitoring</option>
            </select>
            <select
              value={`${sortBy}_${sortOrder}`}
              onChange={(e) => {
                const [field, order] = e.target.value.split('_');
                setSortBy(field as any);
                setSortOrder(order as any);
              }}
              className="px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            >
              <option value="start_time_desc">Newest First</option>
              <option value="start_time_asc">Oldest First</option>
              <option value="execution_time_desc">Slowest First</option>
              <option value="execution_time_asc">Fastest First</option>
              <option value="tool_name_asc">Tool Name A-Z</option>
              <option value="tool_name_desc">Tool Name Z-A</option>
            </select>
            <button
              onClick={exportResults}
              className="flex items-center gap-2 bg-blue-600 text-white px-4 py-2 rounded-lg hover:bg-blue-700 transition-colors"
            >
              <Download className="w-4 h-4" />
              Export
            </button>
          </div>
        </div>
      </div>

      {/* Results Table */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200">
        <div className="overflow-x-auto">
          <table className="w-full">
            <thead className="bg-gray-50 border-b border-gray-200">
              <tr>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Status</th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Suite</th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Tool</th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Operation</th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Start Time</th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Duration</th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Actions</th>
              </tr>
            </thead>
            <tbody className="bg-white divide-y divide-gray-200">
              {filteredAndSortedResults.map((result) => (
                <tr
                  key={result.id}
                  className="hover:bg-gray-50 cursor-pointer"
                  onClick={() => handleResultClick(result)}
                >
                  <td className="px-6 py-4 whitespace-nowrap">
                    <div className={`inline-flex items-center gap-2 px-2 py-1 rounded-full text-xs font-medium ${getStatusColor(result.status)}`}>
                      {getStatusIcon(result.status)}
                      {result.status.charAt(0).toUpperCase() + result.status.slice(1)}
                    </div>
                  </td>
                  <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">
                    {result.suite_name}
                  </td>
                  <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">
                    {result.tool_name}
                  </td>
                  <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-600">
                    {result.operation}
                  </td>
                  <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-600">
                    {new Date(result.start_time).toLocaleString()}
                  </td>
                  <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-600">
                    {result.execution_time_ms ? `${Math.round(result.execution_time_ms)}ms` : 'N/A'}
                  </td>
                  <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-600">
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        handleResultClick(result);
                      }}
                      className="text-blue-600 hover:text-blue-800"
                    >
                      View Details
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>

        {filteredAndSortedResults.length === 0 && (
          <div className="text-center py-8">
            <Filter className="w-12 h-12 text-gray-400 mx-auto mb-4" />
            <h3 className="text-lg font-medium text-gray-900 mb-2">No results found</h3>
            <p className="text-gray-600">Try adjusting your filters or search terms.</p>
          </div>
        )}
      </div>

      {/* Result Details Modal */}
      {selectedResult && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
          <div className="bg-white rounded-lg shadow-xl max-w-4xl w-full mx-4 max-h-[90vh] overflow-y-auto">
            <div className="p-6 border-b border-gray-200">
              <div className="flex justify-between items-start">
                <div>
                  <h3 className="text-lg font-semibold text-gray-900">
                    Test Result Details
                  </h3>
                  <p className="text-gray-600 mt-1">
                    {selectedResult.tool_name} → {selectedResult.operation}
                  </p>
                </div>
                <button
                  onClick={() => setSelectedResult(null)}
                  className="text-gray-400 hover:text-gray-600"
                >
                  <X className="w-6 h-6" />
                </button>
              </div>
            </div>

            <div className="p-6 space-y-4">
              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label className="block text-sm font-medium text-gray-700">Suite</label>
                  <p className="text-gray-900">{selectedResult.suite_name}</p>
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700">Category</label>
                  <p className="text-gray-900 capitalize">{selectedResult.category}</p>
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700">Start Time</label>
                  <p className="text-gray-900">{new Date(selectedResult.start_time).toLocaleString()}</p>
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700">End Time</label>
                  <p className="text-gray-900">
                    {selectedResult.end_time ? new Date(selectedResult.end_time).toLocaleString() : 'N/A'}
                  </p>
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700">Execution Time</label>
                  <p className="text-gray-900">
                    {selectedResult.execution_time_ms ? `${Math.round(selectedResult.execution_time_ms)}ms` : 'N/A'}
                  </p>
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700">Status</label>
                  <div className={`inline-flex items-center gap-2 px-2 py-1 rounded-full text-sm font-medium ${getStatusColor(selectedResult.status)}`}>
                    {getStatusIcon(selectedResult.status)}
                    {selectedResult.status.charAt(0).toUpperCase() + selectedResult.status.slice(1)}
                  </div>
                </div>
              </div>

              {selectedResult.parameters && (
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">Parameters</label>
                  <pre className="bg-gray-100 p-3 rounded text-sm overflow-x-auto">
                    {JSON.stringify(selectedResult.parameters, null, 2)}
                  </pre>
                </div>
              )}

              {selectedResult.result_data && (
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">Result Data</label>
                  <pre className="bg-green-50 p-3 rounded text-sm overflow-x-auto border border-green-200">
                    {JSON.stringify(selectedResult.result_data, null, 2)}
                  </pre>
                </div>
              )}

              {selectedResult.error_message && (
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">Error Message</label>
                  <div className="bg-red-50 p-3 rounded text-sm border border-red-200 text-red-800">
                    {selectedResult.error_message}
                  </div>
                </div>
              )}
            </div>
          </div>
        </div>
      )}
    </div>
  );
};
