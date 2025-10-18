/**
 * Async Job Monitoring Page - Week 1 Integration
 * Real-time tracking of async jobs and queue management
 * Production-grade job monitoring with live metrics
 */

import React, { useState, useEffect } from 'react';
import {
  Clock,
  Activity,
  CheckCircle,
  XCircle,
  AlertTriangle,
  RefreshCw,
  Pause,
  Play,
  Trash2,
  BarChart3,
  Zap,
  TrendingUp,
} from 'lucide-react';
import { apiClient } from '@/services/api';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { formatDistanceToNow } from 'date-fns';

interface AsyncJob {
  id: string;
  type: string;
  status: 'PENDING' | 'RUNNING' | 'COMPLETED' | 'FAILED' | 'CANCELLED';
  priority: 'LOW' | 'MEDIUM' | 'HIGH' | 'CRITICAL';
  created_at: number;
  started_at?: number;
  completed_at?: number;
  progress?: number;
  error_message?: string;
}

interface QueueStats {
  pending_count: number;
  running_count: number;
  completed_count: number;
  failed_count: number;
  avg_processing_time: number;
}

const AsyncJobMonitoring: React.FC = () => {
  const [jobs, setJobs] = useState<AsyncJob[]>([]);
  const [stats, setStats] = useState<QueueStats | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(5); // seconds
  const [activeFilter, setActiveFilter] = useState<'all' | 'pending' | 'running' | 'completed' | 'failed'>('all');
  const [actionInProgress, setActionInProgress] = useState<string | null>(null);

  // Load jobs and stats
  const loadData = async () => {
    try {
      setError(null);
      const [jobsRes, statsRes] = await Promise.allSettled([
        apiClient.listJobs({ limit: 100, status: activeFilter === 'all' ? undefined : activeFilter }),
        apiClient.getQueueStats(),
      ]);

      if (jobsRes.status === 'fulfilled') {
        setJobs(jobsRes.value.jobs || []);
      }
      if (statsRes.status === 'fulfilled') {
        setStats(statsRes.value);
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load jobs');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadData();
  }, [activeFilter]);

  useEffect(() => {
    if (!autoRefresh) return;
    const interval = setInterval(loadData, refreshInterval * 1000);
    return () => clearInterval(interval);
  }, [autoRefresh, refreshInterval, activeFilter]);

  const handleCancelJob = async (jobId: string) => {
    setActionInProgress(jobId);
    try {
      await apiClient.cancelJob(jobId);
      await loadData();
    } catch (err) {
      console.error('Failed to cancel job:', err);
      setError('Failed to cancel job');
    } finally {
      setActionInProgress(null);
    }
  };

  const handleRetryJob = async (jobId: string) => {
    setActionInProgress(jobId);
    try {
      await apiClient.retryJob(jobId);
      await loadData();
    } catch (err) {
      console.error('Failed to retry job:', err);
      setError('Failed to retry job');
    } finally {
      setActionInProgress(null);
    }
  };

  const getStatusColor = (status: AsyncJob['status']) => {
    switch (status) {
      case 'COMPLETED':
        return 'bg-green-100 text-green-800';
      case 'RUNNING':
        return 'bg-blue-100 text-blue-800';
      case 'PENDING':
        return 'bg-yellow-100 text-yellow-800';
      case 'FAILED':
        return 'bg-red-100 text-red-800';
      case 'CANCELLED':
        return 'bg-gray-100 text-gray-800';
      default:
        return 'bg-gray-100 text-gray-800';
    }
  };

  const getStatusIcon = (status: AsyncJob['status']) => {
    switch (status) {
      case 'COMPLETED':
        return <CheckCircle className="w-5 h-5" />;
      case 'RUNNING':
        return <Activity className="w-5 h-5 animate-spin" />;
      case 'PENDING':
        return <Clock className="w-5 h-5" />;
      case 'FAILED':
        return <XCircle className="w-5 h-5" />;
      case 'CANCELLED':
        return <AlertTriangle className="w-5 h-5" />;
      default:
        return <Activity className="w-5 h-5" />;
    }
  };

  const getPriorityColor = (priority: AsyncJob['priority']) => {
    switch (priority) {
      case 'CRITICAL':
        return 'text-red-700 bg-red-50';
      case 'HIGH':
        return 'text-orange-700 bg-orange-50';
      case 'MEDIUM':
        return 'text-blue-700 bg-blue-50';
      case 'LOW':
        return 'text-gray-700 bg-gray-50';
      default:
        return 'text-gray-700 bg-gray-50';
    }
  };

  if (loading) {
    return <LoadingSpinner fullScreen message="Loading async jobs..." />;
  }

  return (
    <div className="space-y-6 p-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold text-gray-900">Async Job Monitoring</h1>
          <p className="text-gray-600 mt-1">Real-time tracking of asynchronous job queue and execution</p>
        </div>
        <div className="flex gap-3">
          <button
            onClick={loadData}
            disabled={actionInProgress !== null}
            className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 disabled:bg-gray-400"
          >
            <RefreshCw className="w-4 h-4" />
            Refresh
          </button>
          <label className="flex items-center gap-2 px-4 py-2 bg-gray-100 text-gray-700 rounded-lg cursor-pointer hover:bg-gray-200">
            <input
              type="checkbox"
              checked={autoRefresh}
              onChange={(e) => setAutoRefresh(e.target.checked)}
              className="w-4 h-4"
            />
            <span className="text-sm">Auto-refresh</span>
          </label>
          {autoRefresh && (
            <select
              value={refreshInterval}
              onChange={(e) => setRefreshInterval(Number(e.target.value))}
              className="px-3 py-2 border border-gray-300 rounded-lg bg-white"
            >
              <option value={2}>2s</option>
              <option value={5}>5s</option>
              <option value={10}>10s</option>
              <option value={30}>30s</option>
            </select>
          )}
        </div>
      </div>

      {/* Error Alert */}
      {error && (
        <div className="p-4 bg-red-50 border border-red-200 rounded-lg text-red-700 flex items-center gap-2">
          <AlertTriangle className="w-5 h-5" />
          {error}
        </div>
      )}

      {/* Queue Statistics */}
      {stats && (
        <div className="grid grid-cols-5 gap-4">
          <div className="bg-white p-4 rounded-lg shadow border-l-4 border-yellow-500">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-gray-600 text-sm">Pending</p>
                <p className="text-2xl font-bold text-yellow-600">{stats.pending_count}</p>
              </div>
              <Clock className="w-8 h-8 text-yellow-500 opacity-50" />
            </div>
          </div>
          <div className="bg-white p-4 rounded-lg shadow border-l-4 border-blue-500">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-gray-600 text-sm">Running</p>
                <p className="text-2xl font-bold text-blue-600">{stats.running_count}</p>
              </div>
              <Activity className="w-8 h-8 text-blue-500 opacity-50 animate-spin" />
            </div>
          </div>
          <div className="bg-white p-4 rounded-lg shadow border-l-4 border-green-500">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-gray-600 text-sm">Completed</p>
                <p className="text-2xl font-bold text-green-600">{stats.completed_count}</p>
              </div>
              <CheckCircle className="w-8 h-8 text-green-500 opacity-50" />
            </div>
          </div>
          <div className="bg-white p-4 rounded-lg shadow border-l-4 border-red-500">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-gray-600 text-sm">Failed</p>
                <p className="text-2xl font-bold text-red-600">{stats.failed_count}</p>
              </div>
              <XCircle className="w-8 h-8 text-red-500 opacity-50" />
            </div>
          </div>
          <div className="bg-white p-4 rounded-lg shadow border-l-4 border-purple-500">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-gray-600 text-sm">Avg Time</p>
                <p className="text-2xl font-bold text-purple-600">{stats.avg_processing_time}ms</p>
              </div>
              <BarChart3 className="w-8 h-8 text-purple-500 opacity-50" />
            </div>
          </div>
        </div>
      )}

      {/* Filter Tabs */}
      <div className="flex gap-2 border-b border-gray-200">
        {(['all', 'pending', 'running', 'completed', 'failed'] as const).map((filter) => (
          <button
            key={filter}
            onClick={() => setActiveFilter(filter)}
            className={`px-4 py-2 font-medium border-b-2 transition-colors ${
              activeFilter === filter
                ? 'border-blue-600 text-blue-600'
                : 'border-transparent text-gray-600 hover:text-gray-900'
            }`}
          >
            {filter.charAt(0).toUpperCase() + filter.slice(1)}
          </button>
        ))}
      </div>

      {/* Jobs List */}
      <div className="bg-white rounded-lg shadow overflow-hidden">
        {jobs.length === 0 ? (
          <div className="p-8 text-center text-gray-500">
            <Clock className="w-12 h-12 mx-auto mb-3 opacity-50" />
            <p>No jobs found</p>
          </div>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full">
              <thead className="bg-gray-50 border-b border-gray-200">
                <tr>
                  <th className="px-6 py-3 text-left text-sm font-semibold text-gray-900">Job ID</th>
                  <th className="px-6 py-3 text-left text-sm font-semibold text-gray-900">Type</th>
                  <th className="px-6 py-3 text-left text-sm font-semibold text-gray-900">Status</th>
                  <th className="px-6 py-3 text-left text-sm font-semibold text-gray-900">Priority</th>
                  <th className="px-6 py-3 text-left text-sm font-semibold text-gray-900">Progress</th>
                  <th className="px-6 py-3 text-left text-sm font-semibold text-gray-900">Created</th>
                  <th className="px-6 py-3 text-left text-sm font-semibold text-gray-900">Actions</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-gray-200">
                {jobs.map((job) => (
                  <tr key={job.id} className="hover:bg-gray-50 transition-colors">
                    <td className="px-6 py-4 text-sm font-mono text-gray-600 truncate">{job.id.slice(0, 12)}...</td>
                    <td className="px-6 py-4 text-sm text-gray-900">{job.type}</td>
                    <td className="px-6 py-4 text-sm">
                      <div className={`flex items-center gap-2 w-fit px-3 py-1 rounded-full ${getStatusColor(job.status)}`}>
                        {getStatusIcon(job.status)}
                        <span className="font-medium">{job.status}</span>
                      </div>
                    </td>
                    <td className="px-6 py-4 text-sm">
                      <span className={`px-3 py-1 rounded-full text-xs font-medium ${getPriorityColor(job.priority)}`}>
                        {job.priority}
                      </span>
                    </td>
                    <td className="px-6 py-4 text-sm">
                      {job.progress !== undefined ? (
                        <div className="w-full bg-gray-200 rounded-full h-2">
                          <div
                            className="bg-blue-600 h-2 rounded-full transition-all"
                            style={{ width: `${job.progress}%` }}
                          />
                        </div>
                      ) : (
                        <span className="text-gray-500">—</span>
                      )}
                    </td>
                    <td className="px-6 py-4 text-sm text-gray-600">
                      {formatDistanceToNow(new Date(job.created_at * 1000), { addSuffix: true })}
                    </td>
                    <td className="px-6 py-4 text-sm">
                      <div className="flex gap-2">
                        {(job.status === 'PENDING' || job.status === 'RUNNING') && (
                          <button
                            onClick={() => handleCancelJob(job.id)}
                            disabled={actionInProgress === job.id}
                            className="p-1 text-red-600 hover:bg-red-50 rounded disabled:opacity-50"
                            title="Cancel job"
                          >
                            <Trash2 className="w-4 h-4" />
                          </button>
                        )}
                        {job.status === 'FAILED' && (
                          <button
                            onClick={() => handleRetryJob(job.id)}
                            disabled={actionInProgress === job.id}
                            className="p-1 text-yellow-600 hover:bg-yellow-50 rounded disabled:opacity-50"
                            title="Retry job"
                          >
                            <RefreshCw className="w-4 h-4" />
                          </button>
                        )}
                        {job.error_message && (
                          <div className="text-xs text-red-600 max-w-xs truncate" title={job.error_message}>
                            {job.error_message}
                          </div>
                        )}
                      </div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>
    </div>
  );
};

export default AsyncJobMonitoring;
