/**
 * Tool Categories Testing Page
 * UI component for testing ANALYTICS, WORKFLOW, SECURITY, and MONITORING tool categories
 * Backend routes: /api/tools/categories/{category}/execute
 */

import React, { useState, useEffect } from 'react';
import { Play, Activity, BarChart3, Workflow, Shield, Monitor, CheckCircle, XCircle, Loader } from 'lucide-react';
import { LoadingSpinner } from '@/components/LoadingSpinner';

interface ToolTestResult {
  tool_name: string;
  operation: string;
  success: boolean;
  execution_time_ms: number;
  result_data?: any;
  error_message?: string;
}

interface ToolCategory {
  id: string;
  name: string;
  description: string;
  icon: React.ReactNode;
  tools: Tool[];
}

interface Tool {
  name: string;
  description: string;
  operations: string[];
}

const ToolCategoriesTesting: React.FC = () => {
  const [activeCategory, setActiveCategory] = useState<string>('analytics');
  const [selectedTool, setSelectedTool] = useState<string>('');
  const [selectedOperation, setSelectedOperation] = useState<string>('');
  const [testResults, setTestResults] = useState<ToolTestResult[]>([]);
  const [isRunning, setIsRunning] = useState(false);
  const [loading, setLoading] = useState(false);

  const categories: ToolCategory[] = [
    {
      id: 'analytics',
      name: 'Analytics Tools',
      description: 'Business intelligence and reporting tools',
      icon: <BarChart3 className="w-6 h-6" />,
      tools: [
        {
          name: 'data_analyzer',
          description: 'Analyze datasets for insights',
          operations: ['analyze_dataset']
        },
        {
          name: 'report_generator',
          description: 'Generate compliance and performance reports',
          operations: ['generate_report']
        },
        {
          name: 'dashboard_builder',
          description: 'Create interactive dashboards',
          operations: ['build_dashboard']
        },
        {
          name: 'predictive_model',
          description: 'Run predictive risk models',
          operations: ['run_prediction']
        }
      ]
    },
    {
      id: 'workflow',
      name: 'Workflow Tools',
      description: 'Process automation and task management',
      icon: <Workflow className="w-6 h-6" />,
      tools: [
        {
          name: 'task_automator',
          description: 'Automate complex multi-step workflows',
          operations: ['automate_task']
        },
        {
          name: 'process_optimizer',
          description: 'Optimize business processes',
          operations: ['optimize_process']
        },
        {
          name: 'approval_workflow',
          description: 'Manage approval workflows',
          operations: ['manage_approval']
        }
      ]
    },
    {
      id: 'security',
      name: 'Security Tools',
      description: 'Vulnerability scanning and compliance checking',
      icon: <Shield className="w-6 h-6" />,
      tools: [
        {
          name: 'vulnerability_scanner',
          description: 'Scan for security vulnerabilities',
          operations: ['scan_vulnerabilities']
        },
        {
          name: 'compliance_checker',
          description: 'Check regulatory compliance',
          operations: ['check_compliance']
        },
        {
          name: 'access_analyzer',
          description: 'Analyze user access patterns',
          operations: ['analyze_access']
        },
        {
          name: 'audit_logger',
          description: 'Log security audit events',
          operations: ['log_audit_event']
        }
      ]
    },
    {
      id: 'monitoring',
      name: 'Monitoring Tools',
      description: 'System monitoring and health checking',
      icon: <Monitor className="w-6 h-6" />,
      tools: [
        {
          name: 'system_monitor',
          description: 'Monitor system performance metrics',
          operations: ['monitor_system']
        },
        {
          name: 'performance_tracker',
          description: 'Track application performance',
          operations: ['track_performance']
        },
        {
          name: 'alert_manager',
          description: 'Manage system alerts',
          operations: ['manage_alerts']
        },
        {
          name: 'health_checker',
          description: 'Check system health status',
          operations: ['check_health']
        }
      ]
    }
  ];

  const runToolTest = async () => {
    if (!selectedTool || !selectedOperation) return;

    setIsRunning(true);
    try {
      const response = await fetch(`/api/tools/categories/${activeCategory}/execute`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${localStorage.getItem('token')}`
        },
        body: JSON.stringify({
          tool_name: selectedTool,
          operation: selectedOperation,
          parameters: {
            test_mode: true,
            timestamp: new Date().toISOString()
          }
        })
      });

      const result = await response.json();

      const testResult: ToolTestResult = {
        tool_name: selectedTool,
        operation: selectedOperation,
        success: response.ok && result.success,
        execution_time_ms: result.execution_time_ms || 0,
        result_data: result.data,
        error_message: result.error_message
      };

      setTestResults(prev => [testResult, ...prev.slice(0, 9)]); // Keep last 10 results

    } catch (error) {
      const testResult: ToolTestResult = {
        tool_name: selectedTool,
        operation: selectedOperation,
        success: false,
        execution_time_ms: 0,
        error_message: error instanceof Error ? error.message : 'Unknown error'
      };

      setTestResults(prev => [testResult, ...prev.slice(0, 9)]);
    } finally {
      setIsRunning(false);
    }
  };

  const currentCategory = categories.find(cat => cat.id === activeCategory);
  const currentTool = currentCategory?.tools.find(tool => tool.name === selectedTool);

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold text-gray-900">Tool Categories Testing</h1>
        <p className="text-gray-600 mt-1">Test ANALYTICS, WORKFLOW, SECURITY, and MONITORING tool categories</p>
      </div>

      {/* Category Selection */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200">
        <div className="p-6 border-b border-gray-200">
          <h2 className="text-lg font-semibold text-gray-900">Select Tool Category</h2>
        </div>
        <div className="p-6">
          <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
            {categories.map((category) => (
              <button
                key={category.id}
                onClick={() => {
                  setActiveCategory(category.id);
                  setSelectedTool('');
                  setSelectedOperation('');
                }}
                className={`p-4 border rounded-lg text-left hover:border-blue-500 transition-colors ${
                  activeCategory === category.id ? 'border-blue-500 bg-blue-50' : 'border-gray-200'
                }`}
              >
                <div className="flex items-center gap-3 mb-2">
                  <div className={`p-2 rounded-lg ${
                    activeCategory === category.id ? 'bg-blue-100 text-blue-600' : 'bg-gray-100 text-gray-600'
                  }`}>
                    {category.icon}
                  </div>
                  <h3 className="font-semibold text-gray-900">{category.name}</h3>
                </div>
                <p className="text-sm text-gray-600">{category.description}</p>
              </button>
            ))}
          </div>
        </div>
      </div>

      {/* Tool Testing */}
      {currentCategory && (
        <div className="bg-white rounded-lg shadow-sm border border-gray-200">
          <div className="p-6 border-b border-gray-200">
            <h2 className="text-lg font-semibold text-gray-900">Test {currentCategory.name}</h2>
          </div>
          <div className="p-6 space-y-4">
            {/* Tool Selection */}
            <div>
              <label className="block text-sm font-medium text-gray-700 mb-2">Select Tool</label>
              <select
                value={selectedTool}
                onChange={(e) => {
                  setSelectedTool(e.target.value);
                  setSelectedOperation('');
                }}
                className="w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
              >
                <option value="">Choose a tool...</option>
                {currentCategory.tools.map((tool) => (
                  <option key={tool.name} value={tool.name}>
                    {tool.name} - {tool.description}
                  </option>
                ))}
              </select>
            </div>

            {/* Operation Selection */}
            {selectedTool && currentTool && (
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">Select Operation</label>
                <select
                  value={selectedOperation}
                  onChange={(e) => setSelectedOperation(e.target.value)}
                  className="w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
                >
                  <option value="">Choose an operation...</option>
                  {currentTool.operations.map((operation) => (
                    <option key={operation} value={operation}>
                      {operation}
                    </option>
                  ))}
                </select>
              </div>
            )}

            {/* Run Test Button */}
            <button
              onClick={runToolTest}
              disabled={!selectedTool || !selectedOperation || isRunning}
              className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 disabled:bg-gray-300 disabled:cursor-not-allowed"
            >
              {isRunning ? (
                <Loader className="w-4 h-4 animate-spin" />
              ) : (
                <Play className="w-4 h-4" />
              )}
              {isRunning ? 'Running Test...' : 'Run Test'}
            </button>
          </div>
        </div>
      )}

      {/* Test Results */}
      {testResults.length > 0 && (
        <div className="bg-white rounded-lg shadow-sm border border-gray-200">
          <div className="p-6 border-b border-gray-200">
            <h2 className="text-lg font-semibold text-gray-900">Test Results</h2>
          </div>
          <div className="divide-y divide-gray-200">
            {testResults.map((result, index) => (
              <div key={index} className="p-6">
                <div className="flex items-center justify-between mb-3">
                  <div className="flex items-center gap-3">
                    {result.success ? (
                      <CheckCircle className="w-5 h-5 text-green-600" />
                    ) : (
                      <XCircle className="w-5 h-5 text-red-600" />
                    )}
                    <div>
                      <h3 className="font-semibold text-gray-900">
                        {result.tool_name}.{result.operation}
                      </h3>
                      <p className="text-sm text-gray-600">
                        {result.execution_time_ms}ms execution time
                      </p>
                    </div>
                  </div>
                  <span className={`px-3 py-1 text-sm rounded-full ${
                    result.success ? 'bg-green-100 text-green-800' : 'bg-red-100 text-red-800'
                  }`}>
                    {result.success ? 'Success' : 'Failed'}
                  </span>
                </div>

                {!result.success && result.error_message && (
                  <div className="mt-3 p-3 bg-red-50 border border-red-200 rounded-md">
                    <p className="text-sm text-red-800">{result.error_message}</p>
                  </div>
                )}

                {result.success && result.result_data && (
                  <div className="mt-3 p-3 bg-green-50 border border-green-200 rounded-md">
                    <p className="text-sm text-green-800">
                      Result: {JSON.stringify(result.result_data, null, 2).substring(0, 200)}...
                    </p>
                  </div>
                )}
              </div>
            ))}
          </div>
        </div>
      )}

      {loading && <LoadingSpinner fullScreen message="Loading tool categories..." />}
    </div>
  );
};

export default ToolCategoriesTesting;
