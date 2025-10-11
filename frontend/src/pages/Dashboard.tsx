/**
 * Dashboard Page
 * Main landing page with real-time data from all backend services
 * NO MOCKS - All data fetched from real API endpoints
 */

import React, { useState } from 'react';
import {
  Activity,
  Bot,
  GitBranch,
  TrendingUp,
  AlertCircle,
  CheckCircle,
  RefreshCw,
  Cpu,
  HardDrive,
} from 'lucide-react';
import { useDashboardData } from '@/hooks/useDashboardData';
import { StatsCard } from '@/components/Dashboard/StatsCard';
import { RecentDecisions } from '@/components/Dashboard/RecentDecisions';
import { AgentStatus } from '@/components/Dashboard/AgentStatus';
import { RegulatoryAlerts } from '@/components/Dashboard/RegulatoryAlerts';
import { LoadingSpinner } from '@/components/LoadingSpinner';

const Dashboard: React.FC = () => {
  const { data, isLoading, isError, refetch } = useDashboardData();

  // Show loading spinner only on initial load
  if (isLoading && !data.activityStats) {
    return <LoadingSpinner fullScreen message="Loading dashboard..." />;
  }

  // Calculate derived statistics with defensive checks
  const agents = Array.isArray(data.agents) ? data.agents : [];
  const recentDecisions = Array.isArray(data.recentDecisions) ? data.recentDecisions : [];
  const recentRegChanges = Array.isArray(data.recentRegChanges) ? data.recentRegChanges : [];
  const recentTransactions = Array.isArray(data.recentTransactions) ? data.recentTransactions : [];

  const activeAgents = agents.filter((a) => a.status === 'active').length;
  const totalAgents = agents.length;

  const decisionsToday = recentDecisions.filter((d) => {
    const today = new Date().setHours(0, 0, 0, 0);
    const decisionDate = new Date(d.timestamp).setHours(0, 0, 0, 0);
    return decisionDate === today;
  }).length;

  const criticalAlerts = recentRegChanges.filter(
    (c) => c.severity === 'critical'
  ).length;

  const avgConfidence = recentDecisions.length > 0
    ? recentDecisions.reduce((sum, d) => sum + (d.confidenceScore || 0), 0) /
      recentDecisions.length
    : 0;

  // Calculate high-risk transactions
  const highRiskTransactions = recentTransactions.filter(
    (t) => t.riskLevel === 'high' || t.riskLevel === 'critical'
  ).length;

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-gray-900">Dashboard</h1>
          <p className="text-gray-600 mt-1">
            Welcome to Regulens - Real-time regulatory compliance monitoring
          </p>
        </div>

        <button
          onClick={() => refetch()}
          disabled={isLoading}
          className="flex items-center gap-2 px-4 py-2 bg-white border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors disabled:opacity-50"
        >
          <RefreshCw className={`w-5 h-5 ${isLoading ? 'animate-spin' : ''}`} />
          Refresh
        </button>
      </div>

      {/* Error Banner */}
      {isError && (
        <div className="bg-red-50 border border-red-200 rounded-lg p-4 flex items-center gap-3">
          <AlertCircle className="w-5 h-5 text-red-600 flex-shrink-0" />
          <div className="flex-1">
            <p className="text-sm font-medium text-red-900">Failed to load dashboard data</p>
            <p className="text-sm text-red-700 mt-1">
              Some dashboard components may show incomplete data. Please refresh to try again.
            </p>
          </div>
        </div>
      )}

      {/* Statistics Cards */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
        <StatsCard
          title="Active Agents"
          value={`${activeAgents}/${totalAgents}`}
          change={{
            value: totalAgents > 0 ? (activeAgents / totalAgents) * 100 : 0,
            label: 'operational',
          }}
          icon={Bot}
          iconColor="text-blue-600"
          iconBg="bg-blue-100"
          trend={activeAgents === totalAgents ? 'up' : activeAgents > 0 ? 'neutral' : 'down'}
          loading={isLoading}
        />

        <StatsCard
          title="Decisions Today"
          value={decisionsToday}
          change={
            data.activityStats
              ? {
                  value: data.activityStats.last24Hours,
                  label: 'total activities',
                }
              : undefined
          }
          icon={GitBranch}
          iconColor="text-green-600"
          iconBg="bg-green-100"
          trend="neutral"
          loading={isLoading}
        />

        <StatsCard
          title="Critical Alerts"
          value={criticalAlerts}
          change={
            data.recentRegChanges.length > 0
              ? {
                  value: Math.round((criticalAlerts / data.recentRegChanges.length) * 100),
                  label: 'of total',
                }
              : undefined
          }
          icon={AlertCircle}
          iconColor="text-red-600"
          iconBg="bg-red-100"
          trend={criticalAlerts > 0 ? 'down' : 'up'}
          loading={isLoading}
        />

        <StatsCard
          title="Avg Confidence"
          value={`${(avgConfidence * 100).toFixed(1)}%`}
          change={
            data.recentDecisions.length > 0
              ? {
                  value: data.recentDecisions.length,
                  label: 'decisions',
                }
              : undefined
          }
          icon={TrendingUp}
          iconColor="text-purple-600"
          iconBg="bg-purple-100"
          trend={avgConfidence > 0.8 ? 'up' : avgConfidence > 0.6 ? 'neutral' : 'down'}
          loading={isLoading}
        />
      </div>

      {/* System Metrics */}
      {data.systemMetrics && (
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <div className="flex items-center gap-2 mb-2">
              <Cpu className="w-4 h-4 text-gray-600" />
              <p className="text-sm font-medium text-gray-600">CPU Usage</p>
            </div>
            <p className="text-xl font-bold text-gray-900">
              {data.systemMetrics.cpuUsage.toFixed(1)}%
            </p>
            <div className="w-full bg-gray-200 rounded-full h-2 mt-2">
              <div
                className="bg-blue-600 h-2 rounded-full transition-all"
                style={{ width: `${data.systemMetrics.cpuUsage}%` }}
              />
            </div>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <div className="flex items-center gap-2 mb-2">
              <HardDrive className="w-4 h-4 text-gray-600" />
              <p className="text-sm font-medium text-gray-600">Memory Usage</p>
            </div>
            <p className="text-xl font-bold text-gray-900">
              {data.systemMetrics.memoryUsage.toFixed(1)}%
            </p>
            <div className="w-full bg-gray-200 rounded-full h-2 mt-2">
              <div
                className="bg-green-600 h-2 rounded-full transition-all"
                style={{ width: `${data.systemMetrics.memoryUsage}%` }}
              />
            </div>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <div className="flex items-center gap-2 mb-2">
              <Activity className="w-4 h-4 text-gray-600" />
              <p className="text-sm font-medium text-gray-600">Request Rate</p>
            </div>
            <p className="text-xl font-bold text-gray-900">
              {data.systemMetrics.requestRate.toFixed(0)}/s
            </p>
            <p className="text-xs text-gray-500 mt-1">
              {data.systemMetrics.avgResponseTime.toFixed(0)}ms avg response
            </p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <div className="flex items-center gap-2 mb-2">
              <CheckCircle className="w-4 h-4 text-gray-600" />
              <p className="text-sm font-medium text-gray-600">Error Rate</p>
            </div>
            <p className="text-xl font-bold text-gray-900">
              {data.systemMetrics.errorRate.toFixed(2)}%
            </p>
            <p className="text-xs text-gray-500 mt-1">
              {highRiskTransactions} high-risk transactions
            </p>
          </div>
        </div>
      )}

      {/* Widgets Grid */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <RecentDecisions decisions={recentDecisions} loading={isLoading} />
        <AgentStatus agents={agents} loading={isLoading} />
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <RegulatoryAlerts changes={recentRegChanges} loading={isLoading} />

        {/* Activity Summary */}
        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <h3 className="text-lg font-semibold text-gray-900 mb-4 flex items-center gap-2">
            <Activity className="w-5 h-5 text-blue-600" />
            Activity Summary
          </h3>

          {data.activityStats ? (
            <div className="space-y-3">
              {Object.entries(data.activityStats.byType || {}).map(([type, count]) => (
                <div key={type} className="flex items-center justify-between">
                  <span className="text-sm text-gray-600 capitalize">
                    {type.replace(/_/g, ' ')}
                  </span>
                  <span className="text-sm font-semibold text-gray-900">
                    {(count as number).toLocaleString()}
                  </span>
                </div>
              ))}

              <div className="mt-4 pt-4 border-t border-gray-200">
                <div className="flex items-center justify-between text-sm">
                  <span className="font-medium text-gray-900">Total Activities (24h)</span>
                  <span className="font-bold text-blue-600">
                    {(data.activityStats.total || 0).toLocaleString()}
                  </span>
                </div>
                <div className="flex items-center justify-between text-sm mt-2">
                  <span className="text-gray-600">Active Sessions</span>
                  <span className="font-semibold text-gray-900">
                    {data.activityStats.last24Hours || 0}
                  </span>
                </div>
              </div>
            </div>
          ) : (
            <div className="text-center py-8">
              <Activity className="w-12 h-12 text-gray-400 mx-auto mb-4" />
              <p className="text-gray-500">No activity data available</p>
              <p className="text-sm text-gray-400 mt-1">Activity metrics will appear once the system processes compliance events</p>
            </div>
          )}
        </div>
      </div>

      {/* Real-time indicator */}
      <div className="flex items-center justify-center gap-2 text-sm text-gray-500">
        <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse" />
        Data updates every 30 seconds
      </div>
    </div>
  );
};

export default Dashboard;
