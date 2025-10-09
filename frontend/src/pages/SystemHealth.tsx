/**
 * System Health Page - Production Implementation  
 * Real-time system health monitoring and metrics dashboard
 * Connects to actual backend health endpoints, metrics collection, and circuit breakers
 */

import React, { useState, useEffect, useCallback } from 'react';
import {
  Heart, Activity, AlertTriangle, CheckCircle, XCircle, 
  Server, Database, Cpu, HardDrive, Network, Clock,
  RefreshCw, TrendingUp, TrendingDown, Zap, Shield,
  BarChart3, Gauge, AlertCircle, Info
} from 'lucide-react';
import { apiClient } from '@/services/api';
import type * as API from '@/types/api';

interface HealthMetrics {
  systemHealth: API.HealthCheck | null;
  systemMetrics: API.SystemMetrics | null;
  circuitBreakers: API.CircuitBreakerStatus[];
  lastUpdated: Date;
}

const SystemHealth: React.FC = () => {
  const [healthData, setHealthData] = useState<HealthMetrics>({
    systemHealth: null,
    systemMetrics: null,
    circuitBreakers: [],
    lastUpdated: new Date()
  });
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(30); // seconds
  const [activeTab, setActiveTab] = useState<'overview' | 'metrics' | 'circuits' | 'services'>('overview');
  const [alertThresholds, setAlertThresholds] = useState({
    cpu: 80,
    memory: 85,
    disk: 90,
    errorRate: 5
  });

  const loadHealthData = useCallback(async () => {
    try {
      setError(null);
      
      // Load health data from multiple endpoints
      const [healthRes, metricsRes, circuitRes] = await Promise.allSettled([
        apiClient.getHealth(),
        apiClient.getSystemMetrics(),
        apiClient.getCircuitBreakerStatus()
      ]);

      const newHealthData: HealthMetrics = {
        systemHealth: healthRes.status === 'fulfilled' ? healthRes.value : null,
        systemMetrics: metricsRes.status === 'fulfilled' ? metricsRes.value : null,
        circuitBreakers: circuitRes.status === 'fulfilled' ? circuitRes.value : [],
        lastUpdated: new Date()
      };

      setHealthData(newHealthData);
      
      // Check for any failed requests and log warnings
      if (healthRes.status === 'rejected') {
        console.warn('Health check failed:', healthRes.reason);
      }
      if (metricsRes.status === 'rejected') {
        console.warn('Metrics fetch failed:', metricsRes.reason);
      }
      if (circuitRes.status === 'rejected') {
        console.warn('Circuit breaker status failed:', circuitRes.reason);
      }

    } catch (err) {
      setError('Failed to load system health data');
      console.error('Error loading health data:', err);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    loadHealthData();
  }, [loadHealthData]);

  useEffect(() => {
    if (!autoRefresh) return;

    const interval = setInterval(() => {
      loadHealthData();
    }, refreshInterval * 1000);

    return () => clearInterval(interval);
  }, [autoRefresh, refreshInterval, loadHealthData]);

  const getHealthIcon = (status: string) => {
    switch (status) {
      case 'healthy': return <CheckCircle className="w-5 h-5 text-green-500" />;
      case 'degraded': return <AlertTriangle className="w-5 h-5 text-yellow-500" />;
      case 'unhealthy': return <XCircle className="w-5 h-5 text-red-500" />;
      default: return <AlertCircle className="w-5 h-5 text-gray-500" />;
    }
  };

  const getHealthColor = (status: string) => {
    switch (status) {
      case 'healthy': return 'bg-green-50 text-green-800 border-green-200';
      case 'degraded': return 'bg-yellow-50 text-yellow-800 border-yellow-200';
      case 'unhealthy': return 'bg-red-50 text-red-800 border-red-200';
      default: return 'bg-gray-50 text-gray-800 border-gray-200';
    }
  };

  const getMetricColor = (value: number, threshold: number) => {
    if (value >= threshold) return 'text-red-600';
    if (value >= threshold * 0.8) return 'text-yellow-600';
    return 'text-green-600';
  };

  const getCircuitBreakerIcon = (state: string) => {
    switch (state) {
      case 'closed': return <CheckCircle className="w-4 h-4 text-green-500" />;
      case 'half_open': return <AlertTriangle className="w-4 h-4 text-yellow-500" />;
      case 'open': return <XCircle className="w-4 h-4 text-red-500" />;
      default: return <AlertCircle className="w-4 h-4 text-gray-500" />;
    }
  };

  const formatUptime = (seconds: number) => {
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    
    if (days > 0) return `${days}d ${hours}h ${minutes}m`;
    if (hours > 0) return `${hours}h ${minutes}m`;
    return `${minutes}m`;
  };

  const formatBytes = (bytes: number) => {
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    if (bytes === 0) return '0 B';
    const i = Math.floor(Math.log(bytes) / Math.log(1024));
    return `${(bytes / Math.pow(1024, i)).toFixed(1)} ${sizes[i]}`;
  };

  if (loading) {
    return (
      <div className="p-6">
        <div className="animate-pulse space-y-6">
          <div className="h-8 bg-gray-200 rounded w-1/3"></div>
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
            {[...Array(4)].map((_, i) => (
              <div key={i} className="h-24 bg-gray-200 rounded"></div>
            ))}
          </div>
          <div className="h-64 bg-gray-200 rounded"></div>
        </div>
      </div>
    );
  }

  return (
    <div className="p-6 space-y-6">
      {/* Header */}
      <div className="flex justify-between items-center">
        <div>
          <h1 className="text-2xl font-bold text-gray-900 flex items-center gap-3">
            <Heart className="w-8 h-8 text-red-500" />
            System Health Dashboard
          </h1>
          <p className="text-gray-600 mt-1">
            Real-time monitoring of system health, performance metrics, and service status
          </p>
        </div>
        <div className="flex items-center gap-4">
          <div className="flex items-center gap-2">
            <label className="text-sm text-gray-600">Auto-refresh:</label>
            <button
              onClick={() => setAutoRefresh(!autoRefresh)}
              className={`px-3 py-1 rounded text-xs font-medium ${
                autoRefresh ? 'bg-green-100 text-green-800' : 'bg-gray-100 text-gray-800'
              }`}
            >
              {autoRefresh ? 'ON' : 'OFF'}
            </button>
          </div>
          <button
            onClick={loadHealthData}
            disabled={loading}
            className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 disabled:opacity-50"
          >
            <RefreshCw className={`w-4 h-4 ${loading ? 'animate-spin' : ''}`} />
            Refresh
          </button>
        </div>
      </div>

      {/* Error Alert */}
      {error && (
        <div className="bg-red-50 border border-red-200 rounded-lg p-4">
          <div className="flex items-center gap-3">
            <AlertCircle className="w-5 h-5 text-red-600" />
            <div>
              <h3 className="font-semibold text-red-800">Health Monitoring Error</h3>
              <p className="text-red-600">{error}</p>
            </div>
          </div>
        </div>
      )}

      {/* System Status Overview */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
        {/* Overall Health */}
        <div className="bg-white rounded-lg shadow p-6">
          <div className="flex items-center justify-between">
            <div>
              <p className="text-sm text-gray-600">Overall Health</p>
              <div className="flex items-center gap-2 mt-1">
                {getHealthIcon(healthData.systemHealth?.status || 'unknown')}
                <span className="font-semibold capitalize">
                  {healthData.systemHealth?.status || 'Unknown'}
                </span>
              </div>
            </div>
            <Activity className="w-8 h-8 text-blue-500" />
          </div>
          {healthData.systemHealth?.uptimeSeconds && (
            <p className="text-xs text-gray-500 mt-2">
              Uptime: {formatUptime(healthData.systemHealth.uptimeSeconds)}
            </p>
          )}
        </div>

        {/* CPU Usage */}
        <div className="bg-white rounded-lg shadow p-6">
          <div className="flex items-center justify-between">
            <div>
              <p className="text-sm text-gray-600">CPU Usage</p>
              <div className="flex items-center gap-2 mt-1">
                <span className={`text-2xl font-bold ${
                  getMetricColor(healthData.systemMetrics?.cpuUsage || 0, alertThresholds.cpu)
                }`}>
                  {healthData.systemMetrics?.cpuUsage?.toFixed(1) || '0'}%
                </span>
              </div>
            </div>
            <Cpu className="w-8 h-8 text-orange-500" />
          </div>
          <div className="mt-3">
            <div className="w-full bg-gray-200 rounded-full h-2">
              <div 
                className={`h-2 rounded-full transition-all ${
                  healthData.systemMetrics?.cpuUsage >= alertThresholds.cpu ? 'bg-red-500' :
                  healthData.systemMetrics?.cpuUsage >= alertThresholds.cpu * 0.8 ? 'bg-yellow-500' :
                  'bg-green-500'
                }`}
                style={{ width: `${Math.min(healthData.systemMetrics?.cpuUsage || 0, 100)}%` }}
              ></div>
            </div>
          </div>
        </div>

        {/* Memory Usage */}
        <div className="bg-white rounded-lg shadow p-6">
          <div className="flex items-center justify-between">
            <div>
              <p className="text-sm text-gray-600">Memory Usage</p>
              <div className="flex items-center gap-2 mt-1">
                <span className={`text-2xl font-bold ${
                  getMetricColor(healthData.systemMetrics?.memoryUsage || 0, alertThresholds.memory)
                }`}>
                  {healthData.systemMetrics?.memoryUsage?.toFixed(1) || '0'}%
                </span>
              </div>
            </div>
            <Server className="w-8 h-8 text-purple-500" />
          </div>
          <div className="mt-3">
            <div className="w-full bg-gray-200 rounded-full h-2">
              <div 
                className={`h-2 rounded-full transition-all ${
                  healthData.systemMetrics?.memoryUsage >= alertThresholds.memory ? 'bg-red-500' :
                  healthData.systemMetrics?.memoryUsage >= alertThresholds.memory * 0.8 ? 'bg-yellow-500' :
                  'bg-green-500'
                }`}
                style={{ width: `${Math.min(healthData.systemMetrics?.memoryUsage || 0, 100)}%` }}
              ></div>
            </div>
          </div>
        </div>

        {/* Disk Usage */}
        <div className="bg-white rounded-lg shadow p-6">
          <div className="flex items-center justify-between">
            <div>
              <p className="text-sm text-gray-600">Disk Usage</p>
              <div className="flex items-center gap-2 mt-1">
                <span className={`text-2xl font-bold ${
                  getMetricColor(healthData.systemMetrics?.diskUsage || 0, alertThresholds.disk)
                }`}>
                  {healthData.systemMetrics?.diskUsage?.toFixed(1) || '0'}%
                </span>
              </div>
            </div>
            <HardDrive className="w-8 h-8 text-green-500" />
          </div>
          <div className="mt-3">
            <div className="w-full bg-gray-200 rounded-full h-2">
              <div 
                className={`h-2 rounded-full transition-all ${
                  healthData.systemMetrics?.diskUsage >= alertThresholds.disk ? 'bg-red-500' :
                  healthData.systemMetrics?.diskUsage >= alertThresholds.disk * 0.8 ? 'bg-yellow-500' :
                  'bg-green-500'
                }`}
                style={{ width: `${Math.min(healthData.systemMetrics?.diskUsage || 0, 100)}%` }}
              ></div>
            </div>
          </div>
        </div>
      </div>

      {/* Tab Navigation */}
      <div className="border-b border-gray-200">
        <nav className="flex space-x-8">
          {[
            { id: 'overview', label: 'System Overview', icon: Gauge },
            { id: 'metrics', label: 'Performance Metrics', icon: BarChart3 },
            { id: 'circuits', label: 'Circuit Breakers', icon: Zap },
            { id: 'services', label: 'Service Health', icon: Shield }
          ].map(({ id, label, icon: Icon }) => (
            <button
              key={id}
              onClick={() => setActiveTab(id as any)}
              className={`flex items-center gap-2 py-2 px-1 border-b-2 font-medium text-sm ${
                activeTab === id
                  ? 'border-blue-500 text-blue-600'
                  : 'border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300'
              }`}
            >
              <Icon className="w-4 h-4" />
              {label}
            </button>
          ))}
        </nav>
      </div>

      {/* Tab Content */}
      {activeTab === 'overview' && (
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          {/* Network Metrics */}
          <div className="bg-white rounded-lg shadow p-6">
            <h3 className="text-lg font-semibold mb-4 flex items-center gap-2">
              <Network className="w-5 h-5 text-blue-600" />
              Network Traffic
            </h3>
            <div className="space-y-4">
              <div className="flex justify-between items-center">
                <span className="text-gray-600">Incoming</span>
                <span className="font-semibold">
                  {formatBytes(healthData.systemMetrics?.networkIn || 0)}/s
                </span>
              </div>
              <div className="flex justify-between items-center">
                <span className="text-gray-600">Outgoing</span>
                <span className="font-semibold">
                  {formatBytes(healthData.systemMetrics?.networkOut || 0)}/s
                </span>
              </div>
            </div>
          </div>

          {/* Request Metrics */}
          <div className="bg-white rounded-lg shadow p-6">
            <h3 className="text-lg font-semibold mb-4 flex items-center gap-2">
              <Activity className="w-5 h-5 text-green-600" />
              Request Metrics
            </h3>
            <div className="space-y-4">
              <div className="flex justify-between items-center">
                <span className="text-gray-600">Request Rate</span>
                <span className="font-semibold">
                  {healthData.systemMetrics?.requestRate?.toFixed(1) || '0'} req/s
                </span>
              </div>
              <div className="flex justify-between items-center">
                <span className="text-gray-600">Error Rate</span>
                <span className={`font-semibold ${
                  getMetricColor(healthData.systemMetrics?.errorRate || 0, alertThresholds.errorRate)
                }`}>
                  {healthData.systemMetrics?.errorRate?.toFixed(2) || '0'}%
                </span>
              </div>
              <div className="flex justify-between items-center">
                <span className="text-gray-600">Avg Response Time</span>
                <span className="font-semibold">
                  {healthData.systemMetrics?.avgResponseTime?.toFixed(0) || '0'}ms
                </span>
              </div>
            </div>
          </div>

          {/* System Information */}
          <div className="bg-white rounded-lg shadow p-6 lg:col-span-2">
            <h3 className="text-lg font-semibold mb-4 flex items-center gap-2">
              <Info className="w-5 h-5 text-blue-600" />
              System Information
            </h3>
            <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
              <div className="space-y-3">
                <div>
                  <label className="text-sm text-gray-600">Service</label>
                  <p className="font-medium">{healthData.systemHealth?.service || 'Unknown'}</p>
                </div>
                <div>
                  <label className="text-sm text-gray-600">Version</label>
                  <p className="font-medium">{healthData.systemHealth?.version || 'Unknown'}</p>
                </div>
                <div>
                  <label className="text-sm text-gray-600">Total Requests</label>
                  <p className="font-medium">{healthData.systemHealth?.totalRequests?.toLocaleString() || '0'}</p>
                </div>
              </div>
              <div className="space-y-3">
                <div>
                  <label className="text-sm text-gray-600">Last Updated</label>
                  <p className="font-medium">{healthData.lastUpdated.toLocaleString()}</p>
                </div>
                <div>
                  <label className="text-sm text-gray-600">Status</label>
                  <div className={`inline-block px-2 py-1 rounded text-sm font-medium border ${
                    getHealthColor(healthData.systemHealth?.status || 'unknown')
                  }`}>
                    {healthData.systemHealth?.status || 'Unknown'}
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>
      )}

      {activeTab === 'circuits' && (
        <div className="space-y-6">
          <div className="bg-white rounded-lg shadow p-6">
            <h3 className="text-lg font-semibold mb-4 flex items-center gap-2">
              <Zap className="w-5 h-5 text-yellow-600" />
              Circuit Breakers Status
            </h3>
            {healthData.circuitBreakers.length > 0 ? (
              <div className="space-y-4">
                {healthData.circuitBreakers.map((breaker) => (
                  <div key={breaker.name} className="border rounded-lg p-4">
                    <div className="flex justify-between items-start mb-2">
                      <div className="flex items-center gap-2">
                        {getCircuitBreakerIcon(breaker.state)}
                        <h4 className="font-medium">{breaker.name}</h4>
                      </div>
                      <span className={`px-2 py-1 rounded text-xs font-medium ${
                        breaker.state === 'closed' ? 'bg-green-100 text-green-800' :
                        breaker.state === 'half_open' ? 'bg-yellow-100 text-yellow-800' :
                        'bg-red-100 text-red-800'
                      }`}>
                        {breaker.state.toUpperCase()}
                      </span>
                    </div>
                    <div className="grid grid-cols-2 md:grid-cols-4 gap-4 text-sm">
                      <div>
                        <span className="text-gray-600">Failures:</span>
                        <span className="ml-2 font-medium">{breaker.failureCount}</span>
                      </div>
                      <div>
                        <span className="text-gray-600">Success:</span>
                        <span className="ml-2 font-medium">{breaker.successCount}</span>
                      </div>
                      {breaker.lastFailure && (
                        <div>
                          <span className="text-gray-600">Last Failure:</span>
                          <span className="ml-2 font-medium">
                            {new Date(breaker.lastFailure).toLocaleString()}
                          </span>
                        </div>
                      )}
                      {breaker.nextRetry && (
                        <div>
                          <span className="text-gray-600">Next Retry:</span>
                          <span className="ml-2 font-medium">
                            {new Date(breaker.nextRetry).toLocaleString()}
                          </span>
                        </div>
                      )}
                    </div>
                  </div>
                ))}
              </div>
            ) : (
              <p className="text-gray-500 text-center py-8">No circuit breakers configured</p>
            )}
          </div>
        </div>
      )}
    </div>
  );
};

export default SystemHealth;
