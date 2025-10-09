/**
 * System Metrics Page - Production Implementation  
 * Real-time system monitoring with live metrics
 * NO MOCKS - Real API integration with C++ backend
 */

import React, { useState, useEffect } from 'react';
import {
  BarChart3,
  Cpu,
  Memory,
  HardDrive,
  Network,
  Activity,
  RefreshCw,
  TrendingUp,
  TrendingDown,
  AlertCircle,
  CheckCircle,
  Clock
} from 'lucide-react';
import { apiClient } from '@/services/api';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import type { SystemMetrics } from '@/types/api';

const SystemMetricsPage: React.FC = () => {
  const [metrics, setMetrics] = useState<SystemMetrics | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [isRefreshing, setIsRefreshing] = useState(false);
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());

  useEffect(() => {
    fetchMetrics();
    
    // Auto-refresh every 15 seconds for real-time monitoring
    const interval = setInterval(fetchMetrics, 15000);
    return () => clearInterval(interval);
  }, []);

  const fetchMetrics = async () => {
    try {
      setError(null);
      const metricsData = await apiClient.getSystemMetrics();
      setMetrics(metricsData);
      setLastUpdate(new Date());
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load system metrics');
    } finally {
      setIsLoading(false);
      setIsRefreshing(false);
    }
  };

  const handleRefresh = async () => {
    setIsRefreshing(true);
    await fetchMetrics();
  };

  const getStatusColor = (value: number, thresholds: { warning: number; critical: number }) => {
    if (value >= thresholds.critical) return 'text-red-600 bg-red-100';
    if (value >= thresholds.warning) return 'text-yellow-600 bg-yellow-100';
    return 'text-green-600 bg-green-100';
  };

  const getStatusIcon = (value: number, thresholds: { warning: number; critical: number }) => {
    if (value >= thresholds.critical) return <AlertCircle className="w-5 h-5 text-red-500" />;
    if (value >= thresholds.warning) return <Clock className="w-5 h-5 text-yellow-500" />;
    return <CheckCircle className="w-5 h-5 text-green-500" />;
  };

  const formatBytes = (bytes: number) => {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
  };

  const formatUptime = (seconds: number) => {
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    
    if (days > 0) return `${days}d ${hours}h ${minutes}m`;
    if (hours > 0) return `${hours}h ${minutes}m`;
    return `${minutes}m`;
  };

  if (isLoading) {
    return <LoadingSpinner fullScreen message="Loading system metrics..." />;
  }

  if (error || !metrics) {
    return (
      <div className="min-h-screen bg-gray-50 flex items-center justify-center">
        <div className="bg-white p-8 rounded-lg shadow-sm max-w-md w-full text-center">
          <AlertCircle className="w-16 h-16 text-red-500 mx-auto mb-4" />
          <h1 className="text-2xl font-bold text-gray-900 mb-2">Unable to Load Metrics</h1>
          <p className="text-gray-600 mb-6">{error || 'System metrics are currently unavailable.'}</p>
          <button
            onClick={handleRefresh}
            className="inline-flex items-center px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700"
          >
            <RefreshCw className="w-4 h-4 mr-2" />
            Retry
          </button>
        </div>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-gray-50">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
        {/* Header */}
        <div className="mb-8">
          <div className="flex items-center justify-between">
            <div>
              <h1 className="text-3xl font-bold text-gray-900">System Metrics</h1>
              <p className="mt-2 text-gray-600">
                Real-time monitoring of system performance and resources
              </p>
            </div>
            <div className="flex items-center space-x-4">
              <div className="text-sm text-gray-500">
                Last updated: {lastUpdate.toLocaleTimeString()}
              </div>
              <button
                onClick={handleRefresh}
                disabled={isRefreshing}
                className="inline-flex items-center px-4 py-2 bg-white border border-gray-300 rounded-lg text-gray-700 hover:bg-gray-50 disabled:opacity-50"
              >
                <RefreshCw className={`w-4 h-4 mr-2 ${isRefreshing ? 'animate-spin' : ''}`} />
                Refresh
              </button>
            </div>
          </div>
        </div>

        {/* Key Metrics Cards */}
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6 mb-8">
          {/* CPU Usage */}
          <div className="bg-white rounded-lg shadow-sm p-6">
            <div className="flex items-center justify-between mb-4">
              <div className="flex items-center space-x-2">
                <Cpu className="w-5 h-5 text-blue-500" />
                <h3 className="text-sm font-medium text-gray-700">CPU Usage</h3>
              </div>
              {getStatusIcon(metrics.cpuUsage, { warning: 70, critical: 90 })}
            </div>
            <div className="space-y-2">
              <div className="flex items-end space-x-2">
                <span className="text-2xl font-bold text-gray-900">{metrics.cpuUsage.toFixed(1)}</span>
                <span className="text-sm text-gray-500 mb-1">%</span>
              </div>
              <div className="w-full bg-gray-200 rounded-full h-2">
                <div 
                  className={`h-2 rounded-full transition-all duration-300 ${
                    metrics.cpuUsage >= 90 ? 'bg-red-500' :
                    metrics.cpuUsage >= 70 ? 'bg-yellow-500' : 'bg-green-500'
                  }`}
                  style={{ width: `${Math.min(metrics.cpuUsage, 100)}%` }}
                ></div>
              </div>
            </div>
          </div>

          {/* Memory Usage */}
          <div className="bg-white rounded-lg shadow-sm p-6">
            <div className="flex items-center justify-between mb-4">
              <div className="flex items-center space-x-2">
                <Memory className="w-5 h-5 text-green-500" />
                <h3 className="text-sm font-medium text-gray-700">Memory Usage</h3>
              </div>
              {getStatusIcon(metrics.memoryUsage, { warning: 80, critical: 95 })}
            </div>
            <div className="space-y-2">
              <div className="flex items-end space-x-2">
                <span className="text-2xl font-bold text-gray-900">{metrics.memoryUsage.toFixed(1)}</span>
                <span className="text-sm text-gray-500 mb-1">%</span>
              </div>
              <div className="w-full bg-gray-200 rounded-full h-2">
                <div 
                  className={`h-2 rounded-full transition-all duration-300 ${
                    metrics.memoryUsage >= 95 ? 'bg-red-500' :
                    metrics.memoryUsage >= 80 ? 'bg-yellow-500' : 'bg-green-500'
                  }`}
                  style={{ width: `${Math.min(metrics.memoryUsage, 100)}%` }}
                ></div>
              </div>
              <div className="text-xs text-gray-500">
                {formatBytes(metrics.memoryUsed || 0)} / {formatBytes(metrics.memoryTotal || 0)}
              </div>
            </div>
          </div>

          {/* Disk Usage */}
          <div className="bg-white rounded-lg shadow-sm p-6">
            <div className="flex items-center justify-between mb-4">
              <div className="flex items-center space-x-2">
                <HardDrive className="w-5 h-5 text-purple-500" />
                <h3 className="text-sm font-medium text-gray-700">Disk Usage</h3>
              </div>
              {getStatusIcon(metrics.diskUsage, { warning: 85, critical: 95 })}
            </div>
            <div className="space-y-2">
              <div className="flex items-end space-x-2">
                <span className="text-2xl font-bold text-gray-900">{metrics.diskUsage.toFixed(1)}</span>
                <span className="text-sm text-gray-500 mb-1">%</span>
              </div>
              <div className="w-full bg-gray-200 rounded-full h-2">
                <div 
                  className={`h-2 rounded-full transition-all duration-300 ${
                    metrics.diskUsage >= 95 ? 'bg-red-500' :
                    metrics.diskUsage >= 85 ? 'bg-yellow-500' : 'bg-green-500'
                  }`}
                  style={{ width: `${Math.min(metrics.diskUsage, 100)}%` }}
                ></div>
              </div>
              <div className="text-xs text-gray-500">
                {formatBytes(metrics.diskUsed || 0)} / {formatBytes(metrics.diskTotal || 0)}
              </div>
            </div>
          </div>

          {/* Network */}
          <div className="bg-white rounded-lg shadow-sm p-6">
            <div className="flex items-center justify-between mb-4">
              <div className="flex items-center space-x-2">
                <Network className="w-5 h-5 text-indigo-500" />
                <h3 className="text-sm font-medium text-gray-700">Network</h3>
              </div>
              <CheckCircle className="w-5 h-5 text-green-500" />
            </div>
            <div className="space-y-2">
              <div className="flex items-end space-x-2">
                <span className="text-2xl font-bold text-gray-900">{metrics.networkConnections || 0}</span>
                <span className="text-sm text-gray-500 mb-1">connections</span>
              </div>
              <div className="text-xs text-gray-500 space-y-1">
                <div>↑ {formatBytes(metrics.networkTxBytes || 0)}</div>
                <div>↓ {formatBytes(metrics.networkRxBytes || 0)}</div>
              </div>
            </div>
          </div>
        </div>

        {/* Detailed Metrics */}
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-8">
          {/* System Information */}
          <div className="bg-white rounded-lg shadow-sm p-6">
            <h2 className="text-xl font-semibold text-gray-900 mb-6">System Information</h2>
            <div className="space-y-4">
              <div className="flex justify-between items-center py-2 border-b border-gray-100">
                <span className="text-sm font-medium text-gray-700">Uptime</span>
                <span className="text-sm text-gray-900">{formatUptime(metrics.uptimeSeconds || 0)}</span>
              </div>
              <div className="flex justify-between items-center py-2 border-b border-gray-100">
                <span className="text-sm font-medium text-gray-700">Load Average</span>
                <span className="text-sm text-gray-900">
                  {metrics.loadAverage ? metrics.loadAverage.join(', ') : 'N/A'}
                </span>
              </div>
              <div className="flex justify-between items-center py-2 border-b border-gray-100">
                <span className="text-sm font-medium text-gray-700">CPU Cores</span>
                <span className="text-sm text-gray-900">{metrics.cpuCores || 'N/A'}</span>
              </div>
              <div className="flex justify-between items-center py-2 border-b border-gray-100">
                <span className="text-sm font-medium text-gray-700">Architecture</span>
                <span className="text-sm text-gray-900">{metrics.architecture || 'N/A'}</span>
              </div>
              <div className="flex justify-between items-center py-2">
                <span className="text-sm font-medium text-gray-700">OS Version</span>
                <span className="text-sm text-gray-900">{metrics.osVersion || 'N/A'}</span>
              </div>
            </div>
          </div>

          {/* Performance Stats */}
          <div className="bg-white rounded-lg shadow-sm p-6">
            <h2 className="text-xl font-semibold text-gray-900 mb-6">Performance Statistics</h2>
            <div className="space-y-4">
              <div className="flex justify-between items-center py-2 border-b border-gray-100">
                <span className="text-sm font-medium text-gray-700">Requests/sec</span>
                <div className="flex items-center space-x-2">
                  <span className="text-sm text-gray-900">{metrics.requestsPerSecond || 0}</span>
                  <TrendingUp className="w-4 h-4 text-green-500" />
                </div>
              </div>
              <div className="flex justify-between items-center py-2 border-b border-gray-100">
                <span className="text-sm font-medium text-gray-700">Avg Response Time</span>
                <div className="flex items-center space-x-2">
                  <span className="text-sm text-gray-900">{metrics.avgResponseTime || 0}ms</span>
                  <TrendingDown className="w-4 h-4 text-green-500" />
                </div>
              </div>
              <div className="flex justify-between items-center py-2 border-b border-gray-100">
                <span className="text-sm font-medium text-gray-700">Error Rate</span>
                <div className="flex items-center space-x-2">
                  <span className="text-sm text-gray-900">{metrics.errorRate || 0}%</span>
                  {(metrics.errorRate || 0) < 1 ? 
                    <TrendingDown className="w-4 h-4 text-green-500" /> :
                    <TrendingUp className="w-4 h-4 text-red-500" />
                  }
                </div>
              </div>
              <div className="flex justify-between items-center py-2 border-b border-gray-100">
                <span className="text-sm font-medium text-gray-700">Active Connections</span>
                <span className="text-sm text-gray-900">{metrics.activeConnections || 0}</span>
              </div>
              <div className="flex justify-between items-center py-2">
                <span className="text-sm font-medium text-gray-700">Queue Length</span>
                <span className="text-sm text-gray-900">{metrics.queueLength || 0}</span>
              </div>
            </div>
          </div>
        </div>

        {/* Status Banner */}
        <div className="mt-8">
          <div className={`rounded-lg p-4 ${
            (metrics.cpuUsage < 70 && metrics.memoryUsage < 80 && metrics.diskUsage < 85) ?
            'bg-green-50 border border-green-200' :
            (metrics.cpuUsage < 90 && metrics.memoryUsage < 95 && metrics.diskUsage < 95) ?
            'bg-yellow-50 border border-yellow-200' :
            'bg-red-50 border border-red-200'
          }`}>
            <div className="flex items-center">
              {(metrics.cpuUsage < 70 && metrics.memoryUsage < 80 && metrics.diskUsage < 85) ?
                <CheckCircle className="w-5 h-5 text-green-500 mr-3" /> :
                (metrics.cpuUsage < 90 && metrics.memoryUsage < 95 && metrics.diskUsage < 95) ?
                <Clock className="w-5 h-5 text-yellow-500 mr-3" /> :
                <AlertCircle className="w-5 h-5 text-red-500 mr-3" />
              }
              <div>
                <h3 className={`font-medium ${
                  (metrics.cpuUsage < 70 && metrics.memoryUsage < 80 && metrics.diskUsage < 85) ?
                  'text-green-800' :
                  (metrics.cpuUsage < 90 && metrics.memoryUsage < 95 && metrics.diskUsage < 95) ?
                  'text-yellow-800' :
                  'text-red-800'
                }`}>
                  System Status: {(metrics.cpuUsage < 70 && metrics.memoryUsage < 80 && metrics.diskUsage < 85) ?
                    'Healthy' :
                    (metrics.cpuUsage < 90 && metrics.memoryUsage < 95 && metrics.diskUsage < 95) ?
                    'Warning' :
                    'Critical'
                  }
                </h3>
                <p className={`text-sm ${
                  (metrics.cpuUsage < 70 && metrics.memoryUsage < 80 && metrics.diskUsage < 85) ?
                  'text-green-600' :
                  (metrics.cpuUsage < 90 && metrics.memoryUsage < 95 && metrics.diskUsage < 95) ?
                  'text-yellow-600' :
                  'text-red-600'
                }`}>
                  {(metrics.cpuUsage < 70 && metrics.memoryUsage < 80 && metrics.diskUsage < 85) ?
                    'All system resources are operating within normal parameters.' :
                    (metrics.cpuUsage < 90 && metrics.memoryUsage < 95 && metrics.diskUsage < 95) ?
                    'Some system resources are approaching capacity thresholds.' :
                    'Critical resource usage detected. Immediate attention required.'
                  }
                </p>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default SystemMetricsPage;
