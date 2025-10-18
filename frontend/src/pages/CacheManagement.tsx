/**
 * Cache Management Page - Week 1 Integration
 * Redis cache monitoring and management interface
 * Production-grade cache performance monitoring and control
 */

import React, { useState, useEffect } from 'react';
import {
  Database,
  BarChart3,
  TrendingUp,
  AlertCircle,
  RefreshCw,
  Trash2,
  Zap,
  HardDrive,
  Activity,
  CheckCircle,
} from 'lucide-react';
import { apiClient } from '@/services/api';
import { LoadingSpinner } from '@/components/LoadingSpinner';

interface CacheStats {
  total_keys: number;
  memory_usage: number;
  hit_rate: number;
  eviction_count: number;
  by_type: Record<string, any>;
}

interface InvalidationRule {
  id: string;
  pattern: string;
  ttl: number;
  priority: 'LOW' | 'MEDIUM' | 'HIGH';
  active: boolean;
}

const CacheManagement: React.FC = () => {
  const [stats, setStats] = useState<CacheStats | null>(null);
  const [rules, setRules] = useState<InvalidationRule[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(10); // seconds
  const [invalidationKey, setInvalidationKey] = useState('');
  const [invalidationPattern, setInvalidationPattern] = useState('');
  const [actionInProgress, setActionInProgress] = useState<string | null>(null);
  const [successMessage, setSuccessMessage] = useState<string | null>(null);

  // Load cache data
  const loadData = async () => {
    try {
      setError(null);
      const [statsRes, rulesRes] = await Promise.allSettled([
        apiClient.getCacheStats(),
        apiClient.getCacheInvalidationRules(),
      ]);

      if (statsRes.status === 'fulfilled') {
        setStats(statsRes.value);
      }
      if (rulesRes.status === 'fulfilled') {
        setRules(rulesRes.value || []);
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load cache data');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadData();
  }, []);

  useEffect(() => {
    if (!autoRefresh) return;
    const interval = setInterval(loadData, refreshInterval * 1000);
    return () => clearInterval(interval);
  }, [autoRefresh, refreshInterval]);

  // Clear success message after 3 seconds
  useEffect(() => {
    if (!successMessage) return;
    const timer = setTimeout(() => setSuccessMessage(null), 3000);
    return () => clearTimeout(timer);
  }, [successMessage]);

  const handleInvalidateKey = async () => {
    if (!invalidationKey.trim()) {
      setError('Please enter a cache key');
      return;
    }

    setActionInProgress('invalidate-key');
    try {
      await apiClient.invalidateCacheKey(invalidationKey);
      setSuccessMessage(`Successfully invalidated key: ${invalidationKey}`);
      setInvalidationKey('');
      await loadData();
    } catch (err) {
      setError(`Failed to invalidate key: ${err instanceof Error ? err.message : 'Unknown error'}`);
    } finally {
      setActionInProgress(null);
    }
  };

  const handleInvalidatePattern = async () => {
    if (!invalidationPattern.trim()) {
      setError('Please enter a cache pattern');
      return;
    }

    setActionInProgress('invalidate-pattern');
    try {
      const res = await apiClient.invalidateCachePattern(invalidationPattern);
      setSuccessMessage(`Invalidated ${res.keys_invalidated} keys matching pattern: ${invalidationPattern}`);
      setInvalidationPattern('');
      await loadData();
    } catch (err) {
      setError(`Failed to invalidate pattern: ${err instanceof Error ? err.message : 'Unknown error'}`);
    } finally {
      setActionInProgress(null);
    }
  };

  const handleWarmCache = async () => {
    setActionInProgress('warm-cache');
    try {
      // Warm cache with common keys
      const commonKeys = [
        'rules:all',
        'decisions:templates',
        'alerts:config',
        'patterns:recent',
      ];
      const res = await apiClient.warmCache(commonKeys);
      setSuccessMessage(`Successfully warmed ${res.keys_warmed} cache keys`);
      await loadData();
    } catch (err) {
      setError(`Failed to warm cache: ${err instanceof Error ? err.message : 'Unknown error'}`);
    } finally {
      setActionInProgress(null);
    }
  };

  if (loading) {
    return <LoadingSpinner fullScreen message="Loading cache management..." />;
  }

  const formatBytes = (bytes: number) => {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return Math.round((bytes / Math.pow(k, i)) * 100) / 100 + ' ' + sizes[i];
  };

  return (
    <div className="space-y-6 p-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold text-gray-900">Cache Management</h1>
          <p className="text-gray-600 mt-1">Redis cache performance monitoring and control</p>
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
          <button
            onClick={handleWarmCache}
            disabled={actionInProgress !== null}
            className="flex items-center gap-2 px-4 py-2 bg-green-600 text-white rounded-lg hover:bg-green-700 disabled:bg-gray-400"
          >
            <Zap className="w-4 h-4" />
            Warm Cache
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
              <option value={5}>5s</option>
              <option value={10}>10s</option>
              <option value={30}>30s</option>
              <option value={60}>60s</option>
            </select>
          )}
        </div>
      </div>

      {/* Success Alert */}
      {successMessage && (
        <div className="p-4 bg-green-50 border border-green-200 rounded-lg text-green-700 flex items-center gap-2">
          <CheckCircle className="w-5 h-5" />
          {successMessage}
        </div>
      )}

      {/* Error Alert */}
      {error && (
        <div className="p-4 bg-red-50 border border-red-200 rounded-lg text-red-700 flex items-center gap-2">
          <AlertCircle className="w-5 h-5" />
          {error}
        </div>
      )}

      {/* Cache Statistics */}
      {stats && (
        <div className="grid grid-cols-4 gap-4">
          <div className="bg-white p-4 rounded-lg shadow border-l-4 border-blue-500">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-gray-600 text-sm">Total Keys</p>
                <p className="text-2xl font-bold text-blue-600">{stats.total_keys.toLocaleString()}</p>
              </div>
              <Database className="w-8 h-8 text-blue-500 opacity-50" />
            </div>
          </div>
          <div className="bg-white p-4 rounded-lg shadow border-l-4 border-purple-500">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-gray-600 text-sm">Memory Usage</p>
                <p className="text-2xl font-bold text-purple-600">{formatBytes(stats.memory_usage)}</p>
              </div>
              <HardDrive className="w-8 h-8 text-purple-500 opacity-50" />
            </div>
          </div>
          <div className="bg-white p-4 rounded-lg shadow border-l-4 border-green-500">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-gray-600 text-sm">Hit Rate</p>
                <p className="text-2xl font-bold text-green-600">{(stats.hit_rate * 100).toFixed(1)}%</p>
              </div>
              <TrendingUp className="w-8 h-8 text-green-500 opacity-50" />
            </div>
          </div>
          <div className="bg-white p-4 rounded-lg shadow border-l-4 border-red-500">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-gray-600 text-sm">Evictions</p>
                <p className="text-2xl font-bold text-red-600">{stats.eviction_count.toLocaleString()}</p>
              </div>
              <AlertCircle className="w-8 h-8 text-red-500 opacity-50" />
            </div>
          </div>
        </div>
      )}

      {/* Cache by Type */}
      {stats && Object.keys(stats.by_type).length > 0 && (
        <div className="bg-white rounded-lg shadow p-6">
          <h2 className="text-lg font-semibold text-gray-900 mb-4">Cache by Type</h2>
          <div className="grid grid-cols-2 md:grid-cols-3 gap-4">
            {Object.entries(stats.by_type).map(([type, data]: [string, any]) => (
              <div key={type} className="p-4 bg-gray-50 rounded-lg border border-gray-200">
                <p className="text-sm font-medium text-gray-600">{type}</p>
                <div className="mt-2 space-y-1">
                  <div className="flex justify-between">
                    <span className="text-xs text-gray-500">Keys:</span>
                    <span className="text-sm font-semibold text-gray-900">{data.keys || 0}</span>
                  </div>
                  <div className="flex justify-between">
                    <span className="text-xs text-gray-500">Memory:</span>
                    <span className="text-sm font-semibold text-gray-900">{formatBytes(data.memory || 0)}</span>
                  </div>
                  {data.ttl_avg && (
                    <div className="flex justify-between">
                      <span className="text-xs text-gray-500">Avg TTL:</span>
                      <span className="text-sm font-semibold text-gray-900">{data.ttl_avg}s</span>
                    </div>
                  )}
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Cache Control Operations */}
      <div className="grid grid-cols-2 gap-6">
        {/* Invalidate Key */}
        <div className="bg-white rounded-lg shadow p-6">
          <h2 className="text-lg font-semibold text-gray-900 mb-4">Invalidate Key</h2>
          <div className="space-y-4">
            <input
              type="text"
              placeholder="Enter cache key (e.g., rules:all:12345)"
              value={invalidationKey}
              onChange={(e) => setInvalidationKey(e.target.value)}
              disabled={actionInProgress !== null}
              className="w-full px-4 py-2 border border-gray-300 rounded-lg disabled:bg-gray-50"
            />
            <button
              onClick={handleInvalidateKey}
              disabled={actionInProgress !== null || !invalidationKey.trim()}
              className="w-full px-4 py-2 bg-red-600 text-white rounded-lg hover:bg-red-700 disabled:bg-gray-400 font-medium"
            >
              <div className="flex items-center justify-center gap-2">
                <Trash2 className="w-4 h-4" />
                Invalidate Key
              </div>
            </button>
            <p className="text-xs text-gray-600">
              Remove a specific cache key from Redis. Use this for individual cache entries.
            </p>
          </div>
        </div>

        {/* Invalidate Pattern */}
        <div className="bg-white rounded-lg shadow p-6">
          <h2 className="text-lg font-semibold text-gray-900 mb-4">Invalidate Pattern</h2>
          <div className="space-y-4">
            <input
              type="text"
              placeholder="Enter pattern (e.g., rules:*)"
              value={invalidationPattern}
              onChange={(e) => setInvalidationPattern(e.target.value)}
              disabled={actionInProgress !== null}
              className="w-full px-4 py-2 border border-gray-300 rounded-lg disabled:bg-gray-50"
            />
            <button
              onClick={handleInvalidatePattern}
              disabled={actionInProgress !== null || !invalidationPattern.trim()}
              className="w-full px-4 py-2 bg-orange-600 text-white rounded-lg hover:bg-orange-700 disabled:bg-gray-400 font-medium"
            >
              <div className="flex items-center justify-center gap-2">
                <AlertCircle className="w-4 h-4" />
                Invalidate Pattern
              </div>
            </button>
            <p className="text-xs text-gray-600">
              Invalidate all cache keys matching a pattern. Use wildcards like rules:* or decisions:*:*
            </p>
          </div>
        </div>
      </div>

      {/* Invalidation Rules */}
      {rules.length > 0 && (
        <div className="bg-white rounded-lg shadow overflow-hidden">
          <div className="p-6 border-b border-gray-200">
            <h2 className="text-lg font-semibold text-gray-900">Invalidation Rules</h2>
          </div>
          <div className="overflow-x-auto">
            <table className="w-full">
              <thead className="bg-gray-50 border-b border-gray-200">
                <tr>
                  <th className="px-6 py-3 text-left text-sm font-semibold text-gray-900">Pattern</th>
                  <th className="px-6 py-3 text-left text-sm font-semibold text-gray-900">TTL (seconds)</th>
                  <th className="px-6 py-3 text-left text-sm font-semibold text-gray-900">Priority</th>
                  <th className="px-6 py-3 text-left text-sm font-semibold text-gray-900">Status</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-gray-200">
                {rules.map((rule) => (
                  <tr key={rule.id} className="hover:bg-gray-50">
                    <td className="px-6 py-4 text-sm font-mono text-gray-900">{rule.pattern}</td>
                    <td className="px-6 py-4 text-sm text-gray-600">{rule.ttl}</td>
                    <td className="px-6 py-4 text-sm">
                      <span
                        className={`px-3 py-1 rounded-full text-xs font-medium ${
                          rule.priority === 'CRITICAL'
                            ? 'bg-red-100 text-red-800'
                            : rule.priority === 'HIGH'
                            ? 'bg-orange-100 text-orange-800'
                            : 'bg-blue-100 text-blue-800'
                        }`}
                      >
                        {rule.priority}
                      </span>
                    </td>
                    <td className="px-6 py-4 text-sm">
                      <div className="flex items-center gap-2">
                        {rule.active ? (
                          <>
                            <CheckCircle className="w-4 h-4 text-green-600" />
                            <span className="text-green-700">Active</span>
                          </>
                        ) : (
                          <>
                            <AlertCircle className="w-4 h-4 text-gray-400" />
                            <span className="text-gray-700">Inactive</span>
                          </>
                        )}
                      </div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}
    </div>
  );
};

export default CacheManagement;
