/**
 * Regulatory Changes Page
 * Real-time monitoring of regulatory changes with impact assessment
 * NO MOCKS - Real WebSocket updates and API data
 */

import React, { useState, useMemo } from 'react';
import { Link } from 'react-router-dom';
import { Scale, Search, AlertCircle, CheckCircle, Clock, XCircle, Wifi, WifiOff } from 'lucide-react';
import { formatDistanceToNow, format } from 'date-fns';
import { useRegulatoryChanges, useRegulatorySources, useRegulatoryStats } from '@/hooks/useRegulatoryChanges';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { clsx } from 'clsx';
import type { RegulatoryChange, RegulatorySource } from '@/types/api';

const RegulatoryChanges: React.FC = () => {
  const [searchQuery, setSearchQuery] = useState('');
  const [filterSource, setFilterSource] = useState<string>('all');
  const [filterSeverity, setFilterSeverity] = useState<RegulatoryChange['severity'] | 'all'>('all');
  const [filterStatus, setFilterStatus] = useState<RegulatoryChange['status'] | 'all'>('all');

  const { changes, isLoading, isConnected } = useRegulatoryChanges({
    limit: 100,
    sourceId: filterSource === 'all' ? undefined : filterSource,
    severity: filterSeverity === 'all' ? undefined : filterSeverity,
    status: filterStatus === 'all' ? undefined : filterStatus,
  });

  const { data: sources = [] } = useRegulatorySources();
  const { data: stats } = useRegulatoryStats();

  const filteredChanges = useMemo(() => {
    return (Array.isArray(changes) ? changes : []).filter((change: RegulatoryChange) => {
      const matchesSearch =
        searchQuery === '' ||
        (change.title && change.title.toLowerCase().includes(searchQuery.toLowerCase())) ||
        (change.description && change.description.toLowerCase().includes(searchQuery.toLowerCase())) ||
        (change.regulatoryBody && change.regulatoryBody.toLowerCase().includes(searchQuery.toLowerCase()));

      return matchesSearch;
    });
  }, [changes, searchQuery]);

  const severityColors = {
    low: 'bg-blue-100 text-blue-700 border-blue-300',
    medium: 'bg-yellow-100 text-yellow-700 border-yellow-300',
    high: 'bg-orange-100 text-orange-700 border-orange-300',
    critical: 'bg-red-100 text-red-700 border-red-300',
  };

  const statusIcons = {
    pending: Clock,
    analyzed: CheckCircle,
    implemented: CheckCircle,
    dismissed: XCircle,
  };

  const statusColors = {
    pending: 'text-yellow-600',
    analyzed: 'text-blue-600',
    implemented: 'text-green-600',
    dismissed: 'text-gray-600',
  };

  if (isLoading) {
    return <LoadingSpinner fullScreen message="Loading regulatory changes..." />;
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-gray-900 flex items-center gap-2">
            <Scale className="w-7 h-7 text-blue-600" />
            Regulatory Changes Monitor
          </h1>
          <p className="text-gray-600 mt-1">Real-time tracking of regulatory updates and compliance changes</p>
        </div>

        <div className="flex items-center gap-2">
          {isConnected ? (
            <div className="flex items-center gap-2 px-3 py-1 bg-green-50 text-green-700 rounded-lg border border-green-200">
              <Wifi className="w-4 h-4" />
              <span className="text-sm font-medium">Live</span>
            </div>
          ) : (
            <div className="flex items-center gap-2 px-3 py-1 bg-gray-50 text-gray-700 rounded-lg border border-gray-200">
              <WifiOff className="w-4 h-4" />
              <span className="text-sm font-medium">Polling</span>
            </div>
          )}
        </div>
      </div>

      {/* Statistics Cards */}
      {stats && (
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Total Changes</p>
            <p className="text-2xl font-bold text-gray-900 mt-1">{stats.totalChanges}</p>
            <p className="text-xs text-gray-500 mt-1">Last 30 days</p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Pending Review</p>
            <p className="text-2xl font-bold text-yellow-600 mt-1">{stats.pendingChanges}</p>
            <p className="text-xs text-gray-500 mt-1">Requires action</p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Critical Severity</p>
            <p className="text-2xl font-bold text-red-600 mt-1">{stats.criticalChanges}</p>
            <p className="text-xs text-gray-500 mt-1">Immediate attention</p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Active Sources</p>
            <p className="text-2xl font-bold text-blue-600 mt-1">{stats.activeSources}</p>
            <p className="text-xs text-gray-500 mt-1">Monitoring</p>
          </div>
        </div>
      )}

      {/* Filters */}
      <div className="bg-white rounded-lg border border-gray-200 p-4">
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
          {/* Search */}
          <div className="relative">
            <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-5 h-5 text-gray-400" />
            <input
              type="text"
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search changes..."
              className="w-full pl-10 pr-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
            />
          </div>

          {/* Source Filter */}
          <select
            value={filterSource}
            onChange={(e) => setFilterSource(e.target.value)}
            className="px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
          >
            <option value="all">All Sources</option>
            {(Array.isArray(sources) ? sources : []).map((source: RegulatorySource) => (
              <option key={source.id} value={source.id}>
                {source.name}
              </option>
            ))}
          </select>

          {/* Severity Filter */}
          <select
            value={filterSeverity}
            onChange={(e) => setFilterSeverity(e.target.value as any)}
            className="px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
          >
            <option value="all">All Severities</option>
            <option value="low">Low</option>
            <option value="medium">Medium</option>
            <option value="high">High</option>
            <option value="critical">Critical</option>
          </select>

          {/* Status Filter */}
          <select
            value={filterStatus}
            onChange={(e) => setFilterStatus(e.target.value as any)}
            className="px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
          >
            <option value="all">All Statuses</option>
            <option value="pending">Pending</option>
            <option value="analyzed">Analyzed</option>
            <option value="implemented">Implemented</option>
            <option value="dismissed">Dismissed</option>
          </select>
        </div>

        <div className="mt-3 text-sm text-gray-600">
          Showing {filteredChanges.length} of {changes.length} changes
        </div>
      </div>

      {/* Changes List */}
      {filteredChanges.length === 0 ? (
        <div className="text-center py-12 bg-white rounded-lg border border-gray-200">
          <Scale className="w-12 h-12 text-gray-400 mx-auto mb-3" />
          <p className="text-gray-600">
            {changes.length === 0 ? 'No regulatory changes found' : 'No changes match your filters'}
          </p>
        </div>
      ) : (
        <div className="space-y-4">
          {filteredChanges.map((change: RegulatoryChange) => {
            const StatusIcon = statusIcons[change.status as keyof typeof statusIcons];

            return (
              <Link
                key={change.id}
                to={`/regulatory/${change.id}`}
                className="block bg-white rounded-lg border border-gray-200 p-6 hover:shadow-md transition-shadow"
              >
                <div className="flex items-start justify-between mb-3">
                  <div className="flex-1">
                    <div className="flex items-center gap-3 mb-2">
                      <h3 className="text-lg font-semibold text-gray-900">{change.title}</h3>
                      <span
                        className={clsx(
                          'px-3 py-1 rounded-full text-xs font-medium border uppercase',
                          severityColors[change.severity as keyof typeof severityColors]
                        )}
                      >
                        {change.severity}
                      </span>
                    </div>
                    <p className="text-sm text-gray-600 line-clamp-2">{change.description}</p>
                  </div>

                  <StatusIcon className={clsx('w-6 h-6 ml-4', statusColors[change.status as keyof typeof statusColors])} />
                </div>

                <div className="grid grid-cols-2 md:grid-cols-5 gap-4 mt-4">
                  <div>
                    <p className="text-xs text-gray-600 mb-1">Regulatory Body</p>
                    <p className="text-sm font-medium text-gray-900">{change.regulatoryBody}</p>
                  </div>

                  <div>
                    <p className="text-xs text-gray-600 mb-1">Effective Date</p>
                    <p className="text-sm font-medium text-gray-900">
                      {format(new Date(change.effectiveDate), 'MMM dd, yyyy')}
                    </p>
                  </div>

                  <div>
                    <p className="text-xs text-gray-600 mb-1">Impact Score</p>
                    <p className="text-sm font-medium text-orange-600">
                      {change.impactScore ? change.impactScore.toFixed(1) : 'N/A'}
                    </p>
                  </div>

                  <div>
                    <p className="text-xs text-gray-600 mb-1">Affected Systems</p>
                    <p className="text-sm font-medium text-gray-900">
                      {change.affectedSystems?.length || 0}
                    </p>
                  </div>

                  <div>
                    <p className="text-xs text-gray-600 mb-1">Detected</p>
                    <p className="text-sm font-medium text-gray-900">
                      {formatDistanceToNow(new Date(change.detectedAt), { addSuffix: true })}
                    </p>
                  </div>
                </div>

                {change.requiresAction && (
                  <div className="mt-3 p-2 bg-orange-50 border border-orange-200 rounded-lg flex items-center gap-2">
                    <AlertCircle className="w-4 h-4 text-orange-600" />
                    <p className="text-sm text-orange-800 font-medium">Action Required</p>
                  </div>
                )}
              </Link>
            );
          })}
        </div>
      )}
    </div>
  );
};

export default RegulatoryChanges;
