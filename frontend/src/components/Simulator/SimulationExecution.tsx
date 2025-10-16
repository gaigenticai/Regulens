/**
 * Simulation Execution Component
 * Real-time simulation execution with monitoring and control
 * Production-grade execution engine with advanced analytics
 */

import React, { useState, useEffect, useRef } from 'react';
import {
  Play,
  Pause,
  Square,
  RefreshCw,
  CheckCircle,
  XCircle,
  AlertTriangle,
  Clock,
  Zap,
  BarChart3,
  Download,
  Upload,
  Settings,
  Eye,
  FileText,
  TrendingUp,
  AlertCircle,
  Info,
  Target,
  Layers,
  Activity,
  Cpu,
  HardDrive,
  Wifi,
  WifiOff,
  ChevronDown,
  ChevronUp,
  MoreVertical,
  Save,
  Share2,
  Copy,
  Terminal,
  Database,
  Server,
  ZapOff
} from 'lucide-react';
import { clsx } from 'clsx';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { format, formatDistanceToNow } from 'date-fns';

// Types (matching backend)
export interface SimulationExecution {
  execution_id: string;
  scenario_id: string;
  user_id: string;
  execution_status: 'pending' | 'running' | 'completed' | 'failed' | 'cancelled' | 'paused';
  execution_parameters: any;
  started_at?: string;
  completed_at?: string;
  cancelled_at?: string;
  paused_at?: string;
  resumed_at?: string;
  error_message?: string;
  progress_percentage: number;
  current_step?: string;
  estimated_completion?: string;
  created_at: string;
  metadata: any;
  performance_metrics?: {
    cpu_usage: number;
    memory_usage: number;
    network_io: number;
    execution_time: number;
  };
  logs: SimulationLog[];
}

export interface SimulationLog {
  log_id: string;
  execution_id: string;
  timestamp: string;
  level: 'info' | 'warning' | 'error' | 'debug';
  message: string;
  step?: string;
  metadata?: any;
}

export interface ExecutionRequest {
  scenario_id: string;
  custom_parameters?: any;
  test_data_override?: any;
  async_execution?: boolean;
  priority?: number;
  notification_settings?: any;
}

interface SimulationExecutionProps {
  scenarioId?: string;
  executionId?: string;
  onExecutionStart?: (executionId: string) => void;
  onExecutionComplete?: (execution: SimulationExecution) => void;
  onExecutionCancel?: (executionId: string) => void;
  onViewResults?: (executionId: string) => void;
  onExportResults?: (executionId: string, format: 'json' | 'csv' | 'pdf') => void;
  className?: string;
}

const EXECUTION_STATUSES = [
  { value: 'pending', label: 'Pending', color: 'bg-yellow-100 text-yellow-800', icon: Clock },
  { value: 'running', label: 'Running', color: 'bg-blue-100 text-blue-800', icon: RefreshCw },
  { value: 'completed', label: 'Completed', color: 'bg-green-100 text-green-800', icon: CheckCircle },
  { value: 'failed', label: 'Failed', color: 'bg-red-100 text-red-800', icon: XCircle },
  { value: 'cancelled', label: 'Cancelled', color: 'bg-gray-100 text-gray-800', icon: Square },
  { value: 'paused', label: 'Paused', color: 'bg-orange-100 text-orange-800', icon: Pause }
];

const LOG_LEVELS = {
  info: { color: 'text-blue-600', bg: 'bg-blue-50', icon: Info },
  warning: { color: 'text-yellow-600', bg: 'bg-yellow-50', icon: AlertTriangle },
  error: { color: 'text-red-600', bg: 'bg-red-50', icon: XCircle },
  debug: { color: 'text-gray-600', bg: 'bg-gray-50', icon: Settings }
};

const SimulationExecution: React.FC<SimulationExecutionProps> = ({
  scenarioId,
  executionId,
  onExecutionStart,
  onExecutionComplete,
  onExecutionCancel,
  onViewResults,
  onExportResults,
  className = ''
}) => {
  const [executions, setExecutions] = useState<SimulationExecution[]>([]);
  const [currentExecution, setCurrentExecution] = useState<SimulationExecution | null>(null);
  const [isStarting, setIsStarting] = useState(false);
  const [isCancelling, setIsCancelling] = useState(false);
  const [isPausing, setIsPausing] = useState(false);
  const [showLogs, setShowLogs] = useState(true);
  const [showMetrics, setShowMetrics] = useState(true);
  const [autoScroll, setAutoScroll] = useState(true);
  const [selectedExecution, setSelectedExecution] = useState<string | null>(executionId || null);
  const logsEndRef = useRef<HTMLDivElement>(null);

  // Mock execution data (replace with real WebSocket/API calls)
  useEffect(() => {
    const mockExecutions: SimulationExecution[] = [
      {
        execution_id: 'exec-001',
        scenario_id: scenarioId || 'scenario-001',
        user_id: 'user-001',
        execution_status: 'running',
        execution_parameters: { priority: 2, async_execution: true },
        started_at: new Date(Date.now() - 300000).toISOString(), // 5 minutes ago
        progress_percentage: 65,
        current_step: 'Impact Analysis',
        estimated_completion: new Date(Date.now() + 300000).toISOString(), // 5 minutes from now
        created_at: new Date(Date.now() - 300000).toISOString(),
        metadata: {},
        performance_metrics: {
          cpu_usage: 78,
          memory_usage: 2.4,
          network_io: 45,
          execution_time: 300
        },
        logs: [
          {
            log_id: 'log-001',
            execution_id: 'exec-001',
            timestamp: new Date(Date.now() - 280000).toISOString(),
            level: 'info',
            message: 'Simulation execution started',
            step: 'Initialization'
          },
          {
            log_id: 'log-002',
            execution_id: 'exec-001',
            timestamp: new Date(Date.now() - 250000).toISOString(),
            level: 'info',
            message: 'Loading baseline data from database',
            step: 'Data Loading'
          },
          {
            log_id: 'log-003',
            execution_id: 'exec-001',
            timestamp: new Date(Date.now() - 200000).toISOString(),
            level: 'info',
            message: 'Regulatory changes parsed successfully',
            step: 'Data Validation'
          },
          {
            log_id: 'log-004',
            execution_id: 'exec-001',
            timestamp: new Date(Date.now() - 150000).toISOString(),
            level: 'warning',
            message: 'High memory usage detected, optimizing data structures',
            step: 'Impact Analysis'
          },
          {
            log_id: 'log-005',
            execution_id: 'exec-001',
            timestamp: new Date(Date.now() - 100000).toISOString(),
            level: 'info',
            message: 'Risk assessment calculations completed',
            step: 'Impact Analysis'
          },
          {
            log_id: 'log-006',
            execution_id: 'exec-001',
            timestamp: new Date(Date.now() - 50000).toISOString(),
            level: 'info',
            message: 'Generating compliance impact report',
            step: 'Report Generation'
          }
        ]
      },
      {
        execution_id: 'exec-002',
        scenario_id: scenarioId || 'scenario-002',
        user_id: 'user-001',
        execution_status: 'completed',
        execution_parameters: { priority: 1, async_execution: true },
        started_at: new Date(Date.now() - 900000).toISOString(), // 15 minutes ago
        completed_at: new Date(Date.now() - 600000).toISOString(), // 10 minutes ago
        progress_percentage: 100,
        created_at: new Date(Date.now() - 900000).toISOString(),
        metadata: {},
        performance_metrics: {
          cpu_usage: 85,
          memory_usage: 3.2,
          network_io: 67,
          execution_time: 900
        },
        logs: []
      }
    ];

    setExecutions(mockExecutions);
    setCurrentExecution(mockExecutions[0]);
    setSelectedExecution(mockExecutions[0].execution_id);
  }, [scenarioId, executionId]);

  // Auto-scroll logs
  useEffect(() => {
    if (autoScroll && logsEndRef.current) {
      logsEndRef.current.scrollIntoView({ behavior: 'smooth' });
    }
  }, [currentExecution?.logs, autoScroll]);

  // Simulate real-time updates
  useEffect(() => {
    if (currentExecution?.execution_status === 'running') {
      const interval = setInterval(() => {
        setCurrentExecution(prev => {
          if (!prev || prev.execution_status !== 'running') return prev;

          const newProgress = Math.min(prev.progress_percentage + Math.random() * 5, 95);
          const newLogs = [...prev.logs];

          // Add random log entries
          if (Math.random() > 0.7) {
            const logLevels: Array<'info' | 'warning' | 'error'> = ['info', 'info', 'warning'];
            const messages = [
              'Processing regulatory impact calculations',
              'Updating compliance metrics',
              'Validating simulation parameters',
              'Optimizing memory usage',
              'Generating impact assessment report',
              'High computational load detected',
              'Memory optimization completed'
            ];

            newLogs.push({
              log_id: `log-${Date.now()}`,
              execution_id: prev.execution_id,
              timestamp: new Date().toISOString(),
              level: logLevels[Math.floor(Math.random() * logLevels.length)],
              message: messages[Math.floor(Math.random() * messages.length)],
              step: prev.current_step
            });
          }

          // Randomly complete the execution
          const shouldComplete = Math.random() > 0.95 && newProgress > 90;
          if (shouldComplete) {
            if (onExecutionComplete) {
              onExecutionComplete({ ...prev, execution_status: 'completed', progress_percentage: 100 });
            }
            return {
              ...prev,
              execution_status: 'completed',
              progress_percentage: 100,
              completed_at: new Date().toISOString(),
              logs: newLogs
            };
          }

          return {
            ...prev,
            progress_percentage: newProgress,
            logs: newLogs
          };
        });
      }, 2000);

      return () => clearInterval(interval);
    }
  }, [currentExecution?.execution_status, onExecutionComplete]);

  const handleStartExecution = async () => {
    if (!scenarioId) return;

    setIsStarting(true);
    try {
      // Mock execution start
      await new Promise(resolve => setTimeout(resolve, 1000));

      const newExecution: SimulationExecution = {
        execution_id: `exec-${Date.now()}`,
        scenario_id: scenarioId,
        user_id: 'current-user',
        execution_status: 'running',
        execution_parameters: { priority: 1, async_execution: true },
        started_at: new Date().toISOString(),
        progress_percentage: 0,
        current_step: 'Initialization',
        estimated_completion: new Date(Date.now() + 600000).toISOString(),
        created_at: new Date().toISOString(),
        metadata: {},
        performance_metrics: {
          cpu_usage: 0,
          memory_usage: 0,
          network_io: 0,
          execution_time: 0
        },
        logs: [{
          log_id: `log-${Date.now()}`,
          execution_id: `exec-${Date.now()}`,
          timestamp: new Date().toISOString(),
          level: 'info',
          message: 'Simulation execution started',
          step: 'Initialization'
        }]
      };

      setExecutions(prev => [newExecution, ...prev]);
      setCurrentExecution(newExecution);
      setSelectedExecution(newExecution.execution_id);

      if (onExecutionStart) {
        onExecutionStart(newExecution.execution_id);
      }
    } catch (error) {
      console.error('Failed to start execution:', error);
    } finally {
      setIsStarting(false);
    }
  };

  const handleCancelExecution = async (executionId: string) => {
    setIsCancelling(true);
    try {
      // Mock cancellation
      await new Promise(resolve => setTimeout(resolve, 500));

      setExecutions(prev => prev.map(exec =>
        exec.execution_id === executionId
          ? { ...exec, execution_status: 'cancelled', cancelled_at: new Date().toISOString() }
          : exec
      ));

      if (currentExecution?.execution_id === executionId) {
        setCurrentExecution(prev => prev ? { ...prev, execution_status: 'cancelled', cancelled_at: new Date().toISOString() } : null);
      }

      if (onExecutionCancel) {
        onExecutionCancel(executionId);
      }
    } catch (error) {
      console.error('Failed to cancel execution:', error);
    } finally {
      setIsCancelling(false);
    }
  };

  const handlePauseExecution = async (executionId: string) => {
    setIsPausing(true);
    try {
      // Mock pause
      await new Promise(resolve => setTimeout(resolve, 500));

      setExecutions(prev => prev.map(exec =>
        exec.execution_id === executionId
          ? { ...exec, execution_status: 'paused', paused_at: new Date().toISOString() }
          : exec
      ));

      if (currentExecution?.execution_id === executionId) {
        setCurrentExecution(prev => prev ? { ...prev, execution_status: 'paused', paused_at: new Date().toISOString() } : null);
      }
    } catch (error) {
      console.error('Failed to pause execution:', error);
    } finally {
      setIsPausing(false);
    }
  };

  const getExecutionStatusInfo = (status: string) => {
    return EXECUTION_STATUSES.find(s => s.value === status) || EXECUTION_STATUSES[0];
  };

  const formatDuration = (seconds: number): string => {
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;

    if (hours > 0) {
      return `${hours}h ${minutes}m ${secs}s`;
    } else if (minutes > 0) {
      return `${minutes}m ${secs}s`;
    }
    return `${secs}s`;
  };

  const getExecutionTime = (execution: SimulationExecution): number => {
    if (!execution.started_at) return 0;

    const endTime = execution.completed_at || execution.cancelled_at || new Date().toISOString();
    return Math.floor((new Date(endTime).getTime() - new Date(execution.started_at).getTime()) / 1000);
  };

  return (
    <div className={clsx('space-y-6', className)}>
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold text-gray-900 flex items-center gap-3">
            <Zap className="w-8 h-8 text-blue-600" />
            Simulation Execution
          </h1>
          <p className="text-gray-600 mt-1">
            Real-time execution monitoring and control
          </p>
        </div>

        <div className="flex items-center gap-3">
          {scenarioId && !currentExecution && (
            <button
              onClick={handleStartExecution}
              disabled={isStarting}
              className="flex items-center gap-2 px-6 py-3 bg-blue-600 hover:bg-blue-700 disabled:bg-gray-400 text-white rounded-lg transition-colors"
            >
              {isStarting ? (
                <RefreshCw className="w-5 h-5 animate-spin" />
              ) : (
                <Play className="w-5 h-5" />
              )}
              Start Execution
            </button>
          )}

          <button className="flex items-center gap-2 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors">
            <Settings className="w-4 h-4" />
            Settings
          </button>
        </div>
      </div>

      {/* Execution History */}
      <div className="bg-white rounded-lg border shadow-sm">
        <div className="p-6 border-b border-gray-200">
          <h2 className="text-xl font-semibold text-gray-900">Execution History</h2>
          <p className="text-gray-600 mt-1">Recent simulation executions</p>
        </div>

        <div className="divide-y divide-gray-200 max-h-96 overflow-y-auto">
          {executions.map((execution) => {
            const StatusIcon = getExecutionStatusInfo(execution.execution_status).icon;
            const isSelected = selectedExecution === execution.execution_id;

            return (
              <div
                key={execution.execution_id}
                className={clsx(
                  'p-4 hover:bg-gray-50 cursor-pointer transition-colors',
                  isSelected && 'bg-blue-50 border-l-4 border-blue-500'
                )}
                onClick={() => {
                  setSelectedExecution(execution.execution_id);
                  setCurrentExecution(execution);
                }}
              >
                <div className="flex items-center justify-between">
                  <div className="flex items-center gap-3 flex-1">
                    <StatusIcon
                      className={clsx(
                        'w-5 h-5',
                        execution.execution_status === 'running' && 'animate-spin text-blue-600',
                        execution.execution_status === 'completed' && 'text-green-600',
                        execution.execution_status === 'failed' && 'text-red-600',
                        execution.execution_status === 'pending' && 'text-yellow-600'
                      )}
                    />
                    <div>
                      <h3 className="font-medium text-gray-900">
                        Execution {execution.execution_id.slice(-8)}
                      </h3>
                      <p className="text-sm text-gray-600">
                        {execution.current_step || 'Initializing...'}
                      </p>
                    </div>
                  </div>

                  <div className="flex items-center gap-4 text-sm text-gray-600">
                    <span>{execution.progress_percentage}% complete</span>
                    <span>{formatDistanceToNow(new Date(execution.created_at), { addSuffix: true })}</span>
                    <span className={clsx(
                      'px-2 py-1 text-xs font-medium rounded-full',
                      getExecutionStatusInfo(execution.execution_status).color
                    )}>
                      {getExecutionStatusInfo(execution.execution_status).label}
                    </span>
                  </div>
                </div>

                {execution.execution_status === 'running' && (
                  <div className="mt-3">
                    <div className="w-full bg-gray-200 rounded-full h-2">
                      <div
                        className="bg-blue-600 h-2 rounded-full transition-all duration-300"
                        style={{ width: `${execution.progress_percentage}%` }}
                      />
                    </div>
                  </div>
                )}
              </div>
            );
          })}
        </div>
      </div>

      {/* Current Execution Details */}
      {currentExecution && (
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
          {/* Main Execution Panel */}
          <div className="lg:col-span-2 space-y-6">
            {/* Execution Status */}
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <div className="flex items-center justify-between mb-6">
                <h3 className="text-lg font-semibold text-gray-900">Execution Status</h3>
                <div className="flex items-center gap-2">
                  {currentExecution.execution_status === 'running' && (
                    <>
                      <button
                        onClick={() => handlePauseExecution(currentExecution.execution_id)}
                        disabled={isPausing}
                        className="flex items-center gap-2 px-3 py-1 text-yellow-600 hover:bg-yellow-50 rounded transition-colors"
                      >
                        <Pause className="w-4 h-4" />
                        Pause
                      </button>
                      <button
                        onClick={() => handleCancelExecution(currentExecution.execution_id)}
                        disabled={isCancelling}
                        className="flex items-center gap-2 px-3 py-1 text-red-600 hover:bg-red-50 rounded transition-colors"
                      >
                        <Square className="w-4 h-4" />
                        Cancel
                      </button>
                    </>
                  )}
                  {currentExecution.execution_status === 'completed' && (
                    <>
                      <button
                        onClick={() => onViewResults?.(currentExecution.execution_id)}
                        className="flex items-center gap-2 px-3 py-1 text-blue-600 hover:bg-blue-50 rounded transition-colors"
                      >
                        <Eye className="w-4 h-4" />
                        View Results
                      </button>
                      <button
                        onClick={() => onExportResults?.(currentExecution.execution_id, 'json')}
                        className="flex items-center gap-2 px-3 py-1 text-green-600 hover:bg-green-50 rounded transition-colors"
                      >
                        <Download className="w-4 h-4" />
                        Export
                      </button>
                    </>
                  )}
                </div>
              </div>

              <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-6">
                <div className="text-center">
                  <div className="text-2xl font-bold text-gray-900">
                    {currentExecution.progress_percentage}%
                  </div>
                  <div className="text-sm text-gray-600">Progress</div>
                </div>
                <div className="text-center">
                  <div className="text-2xl font-bold text-gray-900">
                    {formatDuration(getExecutionTime(currentExecution))}
                  </div>
                  <div className="text-sm text-gray-600">Duration</div>
                </div>
                <div className="text-center">
                  <div className="text-2xl font-bold text-gray-900">
                    {currentExecution.current_step || 'N/A'}
                  </div>
                  <div className="text-sm text-gray-600">Current Step</div>
                </div>
                <div className="text-center">
                  <div className="text-2xl font-bold text-gray-900">
                    {currentExecution.estimated_completion
                      ? formatDistanceToNow(new Date(currentExecution.estimated_completion), { addSuffix: true })
                      : 'N/A'
                    }
                  </div>
                  <div className="text-sm text-gray-600">ETA</div>
                </div>
              </div>

              {currentExecution.execution_status === 'running' && (
                <div className="space-y-2">
                  <div className="flex justify-between text-sm text-gray-600">
                    <span>Progress</span>
                    <span>{currentExecution.progress_percentage}%</span>
                  </div>
                  <div className="w-full bg-gray-200 rounded-full h-3">
                    <div
                      className="bg-blue-600 h-3 rounded-full transition-all duration-500"
                      style={{ width: `${currentExecution.progress_percentage}%` }}
                    />
                  </div>
                </div>
              )}
            </div>

            {/* Execution Logs */}
            <div className="bg-white rounded-lg border shadow-sm">
              <div className="p-4 border-b border-gray-200 flex items-center justify-between">
                <div className="flex items-center gap-2">
                  <button
                    onClick={() => setShowLogs(!showLogs)}
                    className="flex items-center gap-2 text-gray-900 hover:text-gray-700"
                  >
                    {showLogs ? <ChevronUp className="w-4 h-4" /> : <ChevronDown className="w-4 h-4" />}
                    Execution Logs ({currentExecution.logs.length})
                  </button>
                </div>
                <div className="flex items-center gap-2">
                  <label className="flex items-center gap-2 text-sm text-gray-600">
                    <input
                      type="checkbox"
                      checked={autoScroll}
                      onChange={(e) => setAutoScroll(e.target.checked)}
                      className="rounded border-gray-300"
                    />
                    Auto-scroll
                  </label>
                  <button className="p-1 text-gray-400 hover:text-gray-600">
                    <Download className="w-4 h-4" />
                  </button>
                </div>
              </div>

              {showLogs && (
                <div className="max-h-96 overflow-y-auto p-4 space-y-2 font-mono text-sm">
                  {currentExecution.logs.map((log) => {
                    const logStyle = LOG_LEVELS[log.level];

                    return (
                      <div key={log.log_id} className={clsx('p-2 rounded', logStyle.bg)}>
                        <div className="flex items-start gap-2">
                          <logStyle.icon className={clsx('w-4 h-4 mt-0.5 flex-shrink-0', logStyle.color)} />
                          <div className="flex-1 min-w-0">
                            <div className="flex items-center gap-2 text-xs text-gray-500 mb-1">
                              <span>{format(new Date(log.timestamp), 'HH:mm:ss')}</span>
                              {log.step && <span>• {log.step}</span>}
                            </div>
                            <div className={clsx('text-sm', logStyle.color)}>
                              {log.message}
                            </div>
                          </div>
                        </div>
                      </div>
                    );
                  })}
                  <div ref={logsEndRef} />
                </div>
              )}
            </div>
          </div>

          {/* Performance Metrics Sidebar */}
          <div className="space-y-6">
            {/* Performance Metrics */}
            <div className="bg-white rounded-lg border shadow-sm">
              <div className="p-4 border-b border-gray-200 flex items-center justify-between">
                <h4 className="font-medium text-gray-900">Performance Metrics</h4>
                <button
                  onClick={() => setShowMetrics(!showMetrics)}
                  className="text-gray-400 hover:text-gray-600"
                >
                  {showMetrics ? <ChevronUp className="w-4 h-4" /> : <ChevronDown className="w-4 h-4" />}
                </button>
              </div>

              {showMetrics && currentExecution.performance_metrics && (
                <div className="p-4 space-y-4">
                  <div className="flex items-center justify-between">
                    <div className="flex items-center gap-2">
                      <Cpu className="w-4 h-4 text-blue-600" />
                      <span className="text-sm text-gray-600">CPU Usage</span>
                    </div>
                    <span className="font-medium">{currentExecution.performance_metrics.cpu_usage}%</span>
                  </div>

                  <div className="flex items-center justify-between">
                    <div className="flex items-center gap-2">
                      <HardDrive className="w-4 h-4 text-green-600" />
                      <span className="text-sm text-gray-600">Memory</span>
                    </div>
                    <span className="font-medium">{currentExecution.performance_metrics.memory_usage}GB</span>
                  </div>

                  <div className="flex items-center justify-between">
                    <div className="flex items-center gap-2">
                      <Wifi className="w-4 h-4 text-purple-600" />
                      <span className="text-sm text-gray-600">Network I/O</span>
                    </div>
                    <span className="font-medium">{currentExecution.performance_metrics.network_io}MB/s</span>
                  </div>

                  <div className="flex items-center justify-between">
                    <div className="flex items-center gap-2">
                      <Clock className="w-4 h-4 text-orange-600" />
                      <span className="text-sm text-gray-600">Execution Time</span>
                    </div>
                    <span className="font-medium">{formatDuration(currentExecution.performance_metrics.execution_time)}</span>
                  </div>
                </div>
              )}
            </div>

            {/* Execution Parameters */}
            <div className="bg-white rounded-lg border shadow-sm p-4">
              <h4 className="font-medium text-gray-900 mb-3">Execution Parameters</h4>
              <div className="space-y-2 text-sm">
                <div className="flex justify-between">
                  <span className="text-gray-600">Priority:</span>
                  <span className="font-medium">{currentExecution.execution_parameters.priority || 1}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-gray-600">Async:</span>
                  <span className="font-medium">
                    {currentExecution.execution_parameters.async_execution ? 'Yes' : 'No'}
                  </span>
                </div>
                <div className="flex justify-between">
                  <span className="text-gray-600">Scenario ID:</span>
                  <span className="font-medium font-mono text-xs">
                    {currentExecution.scenario_id.slice(-8)}
                  </span>
                </div>
              </div>
            </div>

            {/* Quick Actions */}
            <div className="bg-white rounded-lg border shadow-sm p-4">
              <h4 className="font-medium text-gray-900 mb-3">Quick Actions</h4>
              <div className="space-y-2">
                <button
                  onClick={() => onViewResults?.(currentExecution.execution_id)}
                  className="w-full text-left px-3 py-2 text-sm text-gray-700 hover:bg-gray-50 rounded transition-colors"
                >
                  View Detailed Results
                </button>
                <button
                  onClick={() => onExportResults?.(currentExecution.execution_id, 'json')}
                  className="w-full text-left px-3 py-2 text-sm text-gray-700 hover:bg-gray-50 rounded transition-colors"
                >
                  Export Results (JSON)
                </button>
                <button
                  onClick={() => onExportResults?.(currentExecution.execution_id, 'csv')}
                  className="w-full text-left px-3 py-2 text-sm text-gray-700 hover:bg-gray-50 rounded transition-colors"
                >
                  Export Results (CSV)
                </button>
                <button
                  onClick={() => onExportResults?.(currentExecution.execution_id, 'pdf')}
                  className="w-full text-left px-3 py-2 text-sm text-gray-700 hover:bg-gray-50 rounded transition-colors"
                >
                  Export Report (PDF)
                </button>
              </div>
            </div>
          </div>
        </div>
      )}

      {!currentExecution && !scenarioId && (
        <div className="bg-white rounded-lg border shadow-sm p-12 text-center">
          <Zap className="w-12 h-12 mx-auto mb-4 text-gray-400" />
          <h3 className="text-lg font-medium text-gray-900 mb-2">No Active Execution</h3>
          <p className="text-sm text-gray-600 mb-4">
            Select a scenario to start a new simulation execution, or view execution history above.
          </p>
          <button className="bg-blue-600 hover:bg-blue-700 text-white px-4 py-2 rounded-lg transition-colors">
            Browse Scenarios
          </button>
        </div>
      )}
    </div>
  );
};

export default SimulationExecution;
