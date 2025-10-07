/**
 * Audit Trail Page
 * Real-time audit log monitoring with timeline view
 * NO MOCKS - Real WebSocket updates and API data
 */

import React, { useState, useMemo } from 'react';
import { FileText, Search, User, Calendar, AlertTriangle, Info, XCircle, Wifi, WifiOff } from 'lucide-react';
import { formatDistanceToNow, format } from 'date-fns';
import { useAuditTrail, useAuditStats } from '@/hooks/useAuditTrail';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { clsx } from 'clsx';
import type { AuditEntry } from '@/types/api';

const AuditTrail: React.FC = () => {
  const [searchQuery, setSearchQuery] = useState('');
  const [filterAction, setFilterAction] = useState<string>('all');
  const [filterEntityType, setFilterEntityType] = useState<string>('all');
  const [filterSeverity, setFilterSeverity] = useState<AuditEntry['severity'] | 'all'>('all');
  const [timeRange, setTimeRange] = useState<'24h' | '7d' | '30d'>('7d');

  const { entries, isLoading, isConnected } = useAuditTrail({
    limit: 200,
    action: filterAction === 'all' ? undefined : filterAction,
    entityType: filterEntityType === 'all' ? undefined : filterEntityType,
    severity: filterSeverity === 'all' ? undefined : filterSeverity,
  });

  const { data: stats } = useAuditStats(timeRange);

  // Extract unique actions and entity types for filters
  const uniqueActions = useMemo(() => {
    const actions = new Set(entries.map((e: AuditEntry) => e.action));
    return Array.from(actions).sort();
  }, [entries]);

  const uniqueEntityTypes = useMemo(() => {
    const types = new Set(entries.map((e: AuditEntry) => e.entityType));
    return Array.from(types).sort();
  }, [entries]);

  const filteredEntries = useMemo(() => {
    return entries.filter((entry: AuditEntry) => {
      const matchesSearch =
        searchQuery === '' ||
        entry.action.toLowerCase().includes(searchQuery.toLowerCase()) ||
        entry.userId.toLowerCase().includes(searchQuery.toLowerCase()) ||
        entry.entityType.toLowerCase().includes(searchQuery.toLowerCase()) ||
        entry.entityId.toLowerCase().includes(searchQuery.toLowerCase()) ||
        (entry.details && JSON.stringify(entry.details).toLowerCase().includes(searchQuery.toLowerCase()));

      return matchesSearch;
    });
  }, [entries, searchQuery]);

  const severityIcons = {
    info: Info,
    warning: AlertTriangle,
    error: XCircle,
    critical: AlertTriangle,
  };

  const severityColors = {
    info: 'text-blue-600 bg-blue-50',
    warning: 'text-yellow-600 bg-yellow-50',
    error: 'text-red-600 bg-red-50',
    critical: 'text-red-700 bg-red-100',
  };

  if (isLoading) {
    return <LoadingSpinner fullScreen message="Loading audit trail..." />;
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-gray-900 flex items-center gap-2">
            <FileText className="w-7 h-7 text-blue-600" />
            Audit Trail
          </h1>
          <p className="text-gray-600 mt-1">Complete history of system activities and user actions</p>
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
        <div className="grid grid-cols-1 md:grid-cols-5 gap-4">
          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Total Events</p>
            <p className="text-2xl font-bold text-gray-900 mt-1">{stats.totalEvents}</p>
            <p className="text-xs text-gray-500 mt-1">Last {timeRange === '24h' ? '24 hours' : timeRange === '7d' ? '7 days' : '30 days'}</p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Unique Users</p>
            <p className="text-2xl font-bold text-blue-600 mt-1">{stats.uniqueUsers}</p>
            <p className="text-xs text-gray-500 mt-1">Active</p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Critical Events</p>
            <p className="text-2xl font-bold text-red-600 mt-1">{stats.criticalEvents}</p>
            <p className="text-xs text-gray-500 mt-1">Requiring review</p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Failed Actions</p>
            <p className="text-2xl font-bold text-orange-600 mt-1">{stats.failedActions}</p>
            <p className="text-xs text-gray-500 mt-1">Errors occurred</p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Entity Types</p>
            <p className="text-2xl font-bold text-purple-600 mt-1">{stats.entityTypes}</p>
            <p className="text-xs text-gray-500 mt-1">Tracked</p>
          </div>
        </div>
      )}

      {/* Filters */}
      <div className="bg-white rounded-lg border border-gray-200 p-4">
        <div className="grid grid-cols-1 md:grid-cols-5 gap-4">
          {/* Search */}
          <div className="relative md:col-span-2">
            <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-5 h-5 text-gray-400" />
            <input
              type="text"
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search audit logs..."
              className="w-full pl-10 pr-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
            />
          </div>

          {/* Action Filter */}
          <select
            value={filterAction}
            onChange={(e) => setFilterAction(e.target.value)}
            className="px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
          >
            <option value="all">All Actions</option>
            {uniqueActions.map((action: string) => (
              <option key={action} value={action}>
                {action}
              </option>
            ))}
          </select>

          {/* Entity Type Filter */}
          <select
            value={filterEntityType}
            onChange={(e) => setFilterEntityType(e.target.value)}
            className="px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
          >
            <option value="all">All Entity Types</option>
            {uniqueEntityTypes.map((type: string) => (
              <option key={type} value={type}>
                {type}
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
            <option value="info">Info</option>
            <option value="warning">Warning</option>
            <option value="error">Error</option>
            <option value="critical">Critical</option>
          </select>
        </div>

        {/* Time Range Selector */}
        <div className="mt-4 flex items-center gap-2">
          <span className="text-sm text-gray-600">Time Range:</span>
          <div className="flex gap-2">
            {(['24h', '7d', '30d'] as const).map((range) => (
              <button
                key={range}
                onClick={() => setTimeRange(range)}
                className={clsx(
                  'px-3 py-1 rounded-lg text-sm font-medium transition-colors',
                  timeRange === range
                    ? 'bg-blue-600 text-white'
                    : 'bg-gray-100 text-gray-700 hover:bg-gray-200'
                )}
              >
                {range === '24h' ? '24 Hours' : range === '7d' ? '7 Days' : '30 Days'}
              </button>
            ))}
          </div>
        </div>

        <div className="mt-3 text-sm text-gray-600">
          Showing {filteredEntries.length} of {entries.length} audit entries
        </div>
      </div>

      {/* Audit Timeline */}
      {filteredEntries.length === 0 ? (
        <div className="text-center py-12 bg-white rounded-lg border border-gray-200">
          <FileText className="w-12 h-12 text-gray-400 mx-auto mb-3" />
          <p className="text-gray-600">
            {entries.length === 0 ? 'No audit entries found' : 'No entries match your filters'}
          </p>
        </div>
      ) : (
        <div className="bg-white rounded-lg border border-gray-200 p-6">
          <div className="space-y-4">
            {filteredEntries.map((entry: AuditEntry, index: number) => {
              const SeverityIcon = severityIcons[entry.severity as keyof typeof severityIcons];
              const isFirstOfDay =
                index === 0 ||
                format(new Date(entry.timestamp), 'yyyy-MM-dd') !==
                  format(new Date(filteredEntries[index - 1].timestamp), 'yyyy-MM-dd');

              return (
                <div key={entry.id}>
                  {isFirstOfDay && (
                    <div className="flex items-center gap-2 mb-3 mt-6 first:mt-0">
                      <Calendar className="w-4 h-4 text-gray-500" />
                      <span className="text-sm font-semibold text-gray-700">
                        {format(new Date(entry.timestamp), 'MMMM dd, yyyy')}
                      </span>
                      <div className="flex-1 h-px bg-gray-200" />
                    </div>
                  )}

                  <div className="flex gap-4 group">
                    {/* Timeline Line */}
                    <div className="flex flex-col items-center">
                      <div
                        className={clsx(
                          'w-8 h-8 rounded-full flex items-center justify-center',
                          severityColors[entry.severity as keyof typeof severityColors]
                        )}
                      >
                        <SeverityIcon className="w-4 h-4" />
                      </div>
                      {index < filteredEntries.length - 1 && (
                        <div className="w-0.5 flex-1 bg-gray-200 mt-2" style={{ minHeight: '20px' }} />
                      )}
                    </div>

                    {/* Entry Content */}
                    <div className="flex-1 pb-6">
                      <div className="flex items-start justify-between">
                        <div className="flex-1">
                          <div className="flex items-center gap-2 mb-1">
                            <span className="font-semibold text-gray-900">{entry.action}</span>
                            <span className="text-sm text-gray-500">by</span>
                            <div className="flex items-center gap-1">
                              <User className="w-3 h-3 text-gray-500" />
                              <span className="text-sm font-medium text-gray-700">{entry.userId}</span>
                            </div>
                          </div>

                          <div className="flex items-center gap-4 text-sm text-gray-600 mb-2">
                            <span>
                              <span className="font-medium">Entity:</span> {entry.entityType} ({entry.entityId})
                            </span>
                            {entry.ipAddress && (
                              <span>
                                <span className="font-medium">IP:</span> {entry.ipAddress}
                              </span>
                            )}
                          </div>

                          {entry.details && (
                            <div className="mt-2 p-3 bg-gray-50 rounded-lg">
                              <pre className="text-xs text-gray-700 whitespace-pre-wrap font-mono">
                                {JSON.stringify(entry.details, null, 2)}
                              </pre>
                            </div>
                          )}
                        </div>

                        <span className="text-sm text-gray-500 ml-4">
                          {formatDistanceToNow(new Date(entry.timestamp), { addSuffix: true })}
                        </span>
                      </div>

                      {entry.success === false && (
                        <div className="mt-2 p-2 bg-red-50 border border-red-200 rounded-lg">
                          <p className="text-sm text-red-800">
                            <span className="font-medium">Action Failed:</span>{' '}
                            {entry.errorMessage || 'Unknown error'}
                          </p>
                        </div>
                      )}
                    </div>
                  </div>
                </div>
              );
            })}
          </div>
        </div>
      )}
    </div>
  );
};

export default AuditTrail;
