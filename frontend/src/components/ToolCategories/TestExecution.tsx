import React, { useState, useEffect } from 'react';
import { Play, Pause, Square, RotateCcw, CheckCircle, XCircle, Clock, AlertTriangle } from 'lucide-react';

interface TestExecution {
  id: string;
  suite_id: string;
  suite_name: string;
  status: 'pending' | 'running' | 'completed' | 'failed' | 'cancelled';
  start_time: string;
  end_time?: string;
  progress: number;
  total_tests: number;
  completed_tests: number;
  failed_tests: number;
  results: TestResult[];
}

interface TestResult {
  test_id: string;
  tool_name: string;
  operation: string;
  status: 'pending' | 'running' | 'passed' | 'failed';
  start_time: string;
  end_time?: string;
  execution_time_ms?: number;
  error_message?: string;
  result_data?: any;
}

interface TestExecutionProps {
  suiteId?: string;
  suiteName?: string;
  onExecutionComplete?: (execution: TestExecution) => void;
}

export const TestExecution: React.FC<TestExecutionProps> = ({
  suiteId,
  suiteName,
  onExecutionComplete
}) => {
  const [executions, setExecutions] = useState<TestExecution[]>([]);
  const [currentExecution, setCurrentExecution] = useState<TestExecution | null>(null);
  const [isRunning, setIsRunning] = useState(false);

  // Mock data for demonstration
  useEffect(() => {
    if (suiteId && suiteName) {
      // Initialize with a pending execution
      const newExecution: TestExecution = {
        id: `exec_${Date.now()}`,
        suite_id: suiteId,
        suite_name: suiteName,
        status: 'pending',
        start_time: new Date().toISOString(),
        progress: 0,
        total_tests: 5,
        completed_tests: 0,
        failed_tests: 0,
        results: [
          {
            test_id: 'test_1',
            tool_name: 'data_analyzer',
            operation: 'analyze_dataset',
            status: 'pending',
            start_time: new Date().toISOString()
          },
          {
            test_id: 'test_2',
            tool_name: 'report_generator',
            operation: 'generate_report',
            status: 'pending',
            start_time: new Date().toISOString()
          },
          {
            test_id: 'test_3',
            tool_name: 'dashboard_builder',
            operation: 'build_dashboard',
            status: 'pending',
            start_time: new Date().toISOString()
          },
          {
            test_id: 'test_4',
            tool_name: 'predictor',
            operation: 'run_prediction',
            status: 'pending',
            start_time: new Date().toISOString()
          },
          {
            test_id: 'test_5',
            tool_name: 'validator',
            operation: 'validate_data',
            status: 'pending',
            start_time: new Date().toISOString()
          }
        ]
      };
      setExecutions([newExecution]);
      setCurrentExecution(newExecution);
    }
  }, [suiteId, suiteName]);

  const startExecution = () => {
    if (!currentExecution) return;

    setIsRunning(true);
    const updatedExecution = {
      ...currentExecution,
      status: 'running' as const,
      start_time: new Date().toISOString()
    };

    setCurrentExecution(updatedExecution);
    setExecutions(prev => prev.map(exec =>
      exec.id === updatedExecution.id ? updatedExecution : exec
    ));

    // Simulate test execution
    runTests(updatedExecution);
  };

  const runTests = async (execution: TestExecution) => {
    const results = [...execution.results];

    for (let i = 0; i < results.length; i++) {
      if (!isRunning) break;

      // Update test to running
      results[i] = {
        ...results[i],
        status: 'running' as const
      };

      const updatedExecution = {
        ...execution,
        results,
        progress: (i / results.length) * 100
      };

      setCurrentExecution(updatedExecution);
      setExecutions(prev => prev.map(exec =>
        exec.id === updatedExecution.id ? updatedExecution : exec
      ));

      // Simulate test execution time
      await new Promise(resolve => setTimeout(resolve, 1000 + Math.random() * 2000));

      // Randomly pass or fail the test
      const passed = Math.random() > 0.2; // 80% success rate
      const executionTime = 500 + Math.random() * 1500;

      results[i] = {
        ...results[i],
        status: passed ? 'passed' : 'failed',
        end_time: new Date().toISOString(),
        execution_time_ms: executionTime,
        error_message: passed ? undefined : 'Test failed due to validation error',
        result_data: passed ? { success: true, data: 'Test completed successfully' } : undefined
      };

      const finalExecution = {
        ...execution,
        results,
        progress: ((i + 1) / results.length) * 100,
        completed_tests: results.filter(r => r.status === 'passed' || r.status === 'failed').length,
        failed_tests: results.filter(r => r.status === 'failed').length,
        status: i === results.length - 1 ? 'completed' : 'running',
        end_time: i === results.length - 1 ? new Date().toISOString() : undefined
      };

      setCurrentExecution(finalExecution);
      setExecutions(prev => prev.map(exec =>
        exec.id === finalExecution.id ? finalExecution : exec
      ));

      if (i === results.length - 1) {
        setIsRunning(false);
        if (onExecutionComplete) {
          onExecutionComplete(finalExecution);
        }
      }
    }
  };

  const pauseExecution = () => {
    setIsRunning(false);
    if (currentExecution) {
      const updatedExecution = {
        ...currentExecution,
        status: 'cancelled' as const,
        end_time: new Date().toISOString()
      };
      setCurrentExecution(updatedExecution);
      setExecutions(prev => prev.map(exec =>
        exec.id === updatedExecution.id ? updatedExecution : exec
      ));
    }
  };

  const stopExecution = () => {
    setIsRunning(false);
    if (currentExecution) {
      const updatedExecution = {
        ...currentExecution,
        status: 'cancelled' as const,
        end_time: new Date().toISOString()
      };
      setCurrentExecution(updatedExecution);
      setExecutions(prev => prev.map(exec =>
        exec.id === updatedExecution.id ? updatedExecution : exec
      ));
    }
  };

  const restartExecution = () => {
    if (currentExecution) {
      const resetExecution = {
        ...currentExecution,
        status: 'pending' as const,
        start_time: new Date().toISOString(),
        end_time: undefined,
        progress: 0,
        completed_tests: 0,
        failed_tests: 0,
        results: currentExecution.results.map(result => ({
          ...result,
          status: 'pending' as const,
          start_time: new Date().toISOString(),
          end_time: undefined,
          execution_time_ms: undefined,
          error_message: undefined,
          result_data: undefined
        }))
      };
      setCurrentExecution(resetExecution);
      setExecutions(prev => prev.map(exec =>
        exec.id === resetExecution.id ? resetExecution : exec
      ));
    }
  };

  const getStatusIcon = (status: string) => {
    switch (status) {
      case 'pending':
        return <Clock className="w-4 h-4 text-gray-400" />;
      case 'running':
        return <Play className="w-4 h-4 text-blue-500 animate-pulse" />;
      case 'passed':
        return <CheckCircle className="w-4 h-4 text-green-500" />;
      case 'failed':
        return <XCircle className="w-4 h-4 text-red-500" />;
      case 'cancelled':
        return <AlertTriangle className="w-4 h-4 text-yellow-500" />;
      default:
        return <Clock className="w-4 h-4 text-gray-400" />;
    }
  };

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'pending':
        return 'text-gray-600 bg-gray-100';
      case 'running':
        return 'text-blue-600 bg-blue-100';
      case 'passed':
        return 'text-green-600 bg-green-100';
      case 'failed':
        return 'text-red-600 bg-red-100';
      case 'cancelled':
        return 'text-yellow-600 bg-yellow-100';
      default:
        return 'text-gray-600 bg-gray-100';
    }
  };

  if (!currentExecution) {
    return (
      <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-8 text-center">
        <div className="text-gray-400 mb-4">
          <Play className="w-12 h-12 mx-auto" />
        </div>
        <h4 className="text-lg font-medium text-gray-900 mb-2">No Test Execution</h4>
        <p className="text-gray-600">
          Select a test suite to start execution.
        </p>
      </div>
    );
  }

  return (
    <div className="space-y-6">
      {/* Execution Header */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
        <div className="flex justify-between items-start mb-4">
          <div>
            <h3 className="text-lg font-semibold text-gray-900">
              Test Execution: {currentExecution.suite_name}
            </h3>
            <p className="text-gray-600 mt-1">
              Started: {new Date(currentExecution.start_time).toLocaleString()}
              {currentExecution.end_time && (
                <> • Completed: {new Date(currentExecution.end_time).toLocaleString()}</>
              )}
            </p>
          </div>
          <div className="flex gap-2">
            {currentExecution.status === 'pending' && (
              <button
                onClick={startExecution}
                className="flex items-center gap-2 bg-green-600 text-white px-4 py-2 rounded-lg hover:bg-green-700 transition-colors"
              >
                <Play className="w-4 h-4" />
                Start
              </button>
            )}
            {currentExecution.status === 'running' && (
              <>
                <button
                  onClick={pauseExecution}
                  className="flex items-center gap-2 bg-yellow-600 text-white px-4 py-2 rounded-lg hover:bg-yellow-700 transition-colors"
                >
                  <Pause className="w-4 h-4" />
                  Pause
                </button>
                <button
                  onClick={stopExecution}
                  className="flex items-center gap-2 bg-red-600 text-white px-4 py-2 rounded-lg hover:bg-red-700 transition-colors"
                >
                  <Square className="w-4 h-4" />
                  Stop
                </button>
              </>
            )}
            {(currentExecution.status === 'completed' || currentExecution.status === 'failed' || currentExecution.status === 'cancelled') && (
              <button
                onClick={restartExecution}
                className="flex items-center gap-2 bg-blue-600 text-white px-4 py-2 rounded-lg hover:bg-blue-700 transition-colors"
              >
                <RotateCcw className="w-4 h-4" />
                Restart
              </button>
            )}
          </div>
        </div>

        {/* Progress Bar */}
        <div className="mb-4">
          <div className="flex justify-between items-center mb-2">
            <span className="text-sm font-medium text-gray-700">Progress</span>
            <span className="text-sm text-gray-600">
              {currentExecution.completed_tests}/{currentExecution.total_tests} tests
            </span>
          </div>
          <div className="w-full bg-gray-200 rounded-full h-2">
            <div
              className="bg-blue-600 h-2 rounded-full transition-all duration-300"
              style={{ width: `${currentExecution.progress}%` }}
            />
          </div>
        </div>

        {/* Status and Stats */}
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
          <div className="text-center">
            <div className={`inline-flex items-center gap-2 px-3 py-1 rounded-full text-sm font-medium ${getStatusColor(currentExecution.status)}`}>
              {getStatusIcon(currentExecution.status)}
              {currentExecution.status.charAt(0).toUpperCase() + currentExecution.status.slice(1)}
            </div>
          </div>
          <div className="text-center">
            <div className="text-2xl font-bold text-green-600">{currentExecution.completed_tests - currentExecution.failed_tests}</div>
            <div className="text-sm text-gray-600">Passed</div>
          </div>
          <div className="text-center">
            <div className="text-2xl font-bold text-red-600">{currentExecution.failed_tests}</div>
            <div className="text-sm text-gray-600">Failed</div>
          </div>
          <div className="text-center">
            <div className="text-2xl font-bold text-gray-600">{currentExecution.total_tests - currentExecution.completed_tests}</div>
            <div className="text-sm text-gray-600">Remaining</div>
          </div>
        </div>
      </div>

      {/* Test Results */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200">
        <div className="p-6 border-b border-gray-200">
          <h4 className="text-lg font-semibold text-gray-900">Test Results</h4>
        </div>
        <div className="divide-y divide-gray-200">
          {currentExecution.results.map((result) => (
            <div key={result.test_id} className="p-4 hover:bg-gray-50">
              <div className="flex justify-between items-start">
                <div className="flex-1">
                  <div className="flex items-center gap-3 mb-2">
                    {getStatusIcon(result.status)}
                    <span className="font-medium text-gray-900">
                      {result.tool_name} → {result.operation}
                    </span>
                    <span className={`inline-flex items-center px-2 py-1 rounded-full text-xs font-medium ${getStatusColor(result.status)}`}>
                      {result.status.charAt(0).toUpperCase() + result.status.slice(1)}
                    </span>
                  </div>
                  <div className="text-sm text-gray-600 space-y-1">
                    <div>Started: {new Date(result.start_time).toLocaleString()}</div>
                    {result.end_time && (
                      <div>Completed: {new Date(result.end_time).toLocaleString()}</div>
                    )}
                    {result.execution_time_ms && (
                      <div>Duration: {Math.round(result.execution_time_ms)}ms</div>
                    )}
                    {result.error_message && (
                      <div className="text-red-600">Error: {result.error_message}</div>
                    )}
                    {result.result_data && (
                      <div className="text-green-600">Result: {JSON.stringify(result.result_data)}</div>
                    )}
                  </div>
                </div>
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
};
