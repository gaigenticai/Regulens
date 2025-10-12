/**
 * Audit Trail Page
 * Real-time audit log monitoring with timeline view
 * NO MOCKS - Real WebSocket updates and API data
 * Enhanced with System Logs, Security Events, and Login History tabs
 */

import React, { useState, useMemo } from 'react';
import { 
  FileText, Search, User, Calendar, AlertTriangle, Info, XCircle, Wifi, WifiOff, 
  Shield, LogIn, Activity, CheckCircle 
} from 'lucide-react';
import { formatDistanceToNow, format } from 'date-fns';
import { 
  useAuditTrail, useAuditStats, useSystemLogs, useSecurityEvents, useLoginHistory 
} from '@/hooks/useAuditTrail';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { clsx } from 'clsx';
import type { AuditEntry } from '@/types/api';

type TabType = 'all' | 'system' | 'security' | 'login';

const AuditTrail: React.FC = () => {
  const [activeTab, setActiveTab] = useState<TabType>('all');
  const [searchQuery, setSearchQuery] = useState('');
  const [filterAction, setFilterAction] = useState<string>('all');
  const [filterEntityType, setFilterEntityType] = useState<string>('all');
  const [filterSeverity, setFilterSeverity] = useState<AuditEntry['severity'] | 'all'>('all');
  const [timeRange, setTimeRange] = useState<'24h' | '7d' | '30d'>('7d');
  const [usernameFilter, setUsernameFilter] = useState('');

  // Fetch data based on active tab
  const { entries, isLoading, isConnected } = useAuditTrail({
    limit: 200,
    action: filterAction === 'all' ? undefined : filterAction,
    entityType: filterEntityType === 'all' ? undefined : filterEntityType,
    severity: filterSeverity === 'all' ? undefined : filterSeverity,
  });

  const { data: systemLogs = [], isLoading: isLoadingSystemLogs } = useSystemLogs({
    limit: 200,
    severity: filterSeverity === 'all' ? undefined : filterSeverity,
  });

  const { data: securityEvents = [], isLoading: isLoadingSecurityEvents } = useSecurityEvents({
    limit: 200,
  });

  const { data: loginHistory = [], isLoading: isLoadingLoginHistory } = useLoginHistory({
    limit: 200,
    username: usernameFilter || undefined,
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

  // Get current data based on active tab
  const currentData = useMemo(() => {
    switch (activeTab) {
      case 'system':
        return systemLogs;
      case 'security':
        return securityEvents;
      case 'login':
        return loginHistory;
      default:
        return entries;
    }
  }, [activeTab, entries, systemLogs, securityEvents, loginHistory]);

  const filteredEntries = useMemo(() => {
    return currentData.filter((entry: any) => {
      if (searchQuery === '') return true;
      
      const searchLower = searchQuery.toLowerCase();
      
      // For login history
      if (activeTab === 'login') {
        return (
          entry.username?.toLowerCase().includes(searchLower) ||
          entry.ipAddress?.toLowerCase().includes(searchLower) ||
          entry.userAgent?.toLowerCase().includes(searchLower)
        );
      }
      
      // For system logs and security events
      if (activeTab === 'system' || activeTab === 'security') {
        return (
          entry.eventType?.toLowerCase().includes(searchLower) ||
          entry.description?.toLowerCase().includes(searchLower) ||
          entry.agentType?.toLowerCase().includes(searchLower)
        );
      }
      
      // For all events (audit trail)
      return (
        entry.action?.toLowerCase().includes(searchLower) ||
        entry.userId?.toLowerCase().includes(searchLower) ||
        entry.entityType?.toLowerCase().includes(searchLower) ||
        entry.entityId?.toLowerCase().includes(searchLower) ||
        (entry.details && JSON.stringify(entry.details).toLowerCase().includes(searchLower))
      );
    });
  }, [currentData, searchQuery, activeTab]);

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

  const currentLoading = 
    activeTab === 'all' ? isLoading :
    activeTab === 'system' ? isLoadingSystemLogs :
    activeTab === 'security' ? isLoadingSecurityEvents :
    isLoadingLoginHistory;

  if (currentLoading && (
    (activeTab === 'all' && entries.length === 0) ||
    (activeTab === 'system' && systemLogs.length === 0) ||
    (activeTab === 'security' && securityEvents.length === 0) ||
    (activeTab === 'login' && loginHistory.length === 0)
  )) {
    return <LoadingSpinner fullScreen message="Loading audit trail..." />;
  }

  const tabs = [
    { id: 'all' as TabType, label: 'All Events', icon: FileText, count: entries.length },
    { id: 'system' as TabType, label: 'System Logs', icon: Activity, count: systemLogs.length },
    { id: 'security' as TabType, label: 'Security', icon: Shield, count: securityEvents.length },
    { id: 'login' as TabType, label: 'Login History', icon: LogIn, count: loginHistory.length },
  ];

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

      {/* Tabs */}
      <div className="bg-white rounded-lg border border-gray-200 p-2">
        <div className="flex gap-2">
          {tabs.map((tab) => {
            const Icon = tab.icon;
            return (
              <button
                key={tab.id}
                onClick={() => setActiveTab(tab.id)}
                className={clsx(
                  'flex items-center gap-2 px-4 py-2 rounded-lg font-medium transition-all',
                  activeTab === tab.id
                    ? 'bg-blue-600 text-white shadow-sm'
                    : 'text-gray-700 hover:bg-gray-100'
                )}
              >
                <Icon className="w-4 h-4" />
                <span>{tab.label}</span>
                <span className={clsx(
                  'px-2 py-0.5 rounded-full text-xs font-semibold',
                  activeTab === tab.id
                    ? 'bg-blue-500 text-white'
                    : 'bg-gray-200 text-gray-700'
                )}>
                  {tab.count}
                </span>
              </button>
            );
          })}
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
          Showing {filteredEntries.length} of {currentData.length} {activeTab === 'login' ? 'login' : activeTab === 'security' ? 'security' : activeTab === 'system' ? 'system' : 'audit'} entries
        </div>
      </div>

      {/* Content Display */}
      {filteredEntries.length === 0 ? (
        <div className="text-center py-12 bg-white rounded-lg border border-gray-200">
          <FileText className="w-12 h-12 text-gray-400 mx-auto mb-3" />
          <p className="text-gray-600">
            {currentData.length === 0 ? 'No entries found' : 'No entries match your filters'}
          </p>
        </div>
      ) : activeTab === 'login' ? (
        /* Login History View */
        <div className="bg-white rounded-lg border border-gray-200 overflow-hidden">
          <table className="min-w-full divide-y divide-gray-200">
            <thead className="bg-gray-50">
              <tr>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">User</th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">IP Address</th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Status</th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">User Agent</th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Time</th>
              </tr>
            </thead>
            <tbody className="bg-white divide-y divide-gray-200">
              {filteredEntries.map((entry: any) => (
                <tr key={entry.id} className="hover:bg-gray-50">
                  <td className="px-6 py-4 whitespace-nowrap">
                    <div className="flex items-center gap-2">
                      <User className="w-4 h-4 text-gray-500" />
                      <span className="text-sm font-medium text-gray-900">{entry.username}</span>
                    </div>
                  </td>
                  <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-700">{entry.ipAddress}</td>
                  <td className="px-6 py-4 whitespace-nowrap">
                    {entry.success ? (
                      <span className="flex items-center gap-1 text-green-700">
                        <CheckCircle className="w-4 h-4" />
                        Success
                      </span>
                    ) : (
                      <span className="flex items-center gap-1 text-red-700">
                        <XCircle className="w-4 h-4" />
                        Failed{entry.failureReason && `: ${entry.failureReason}`}
                      </span>
                    )}
                  </td>
                  <td className="px-6 py-4 text-sm text-gray-600 max-w-xs truncate">{entry.userAgent}</td>
                  <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                    {formatDistanceToNow(new Date(entry.timestamp), { addSuffix: true })}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      ) : (activeTab === 'system' || activeTab === 'security') ? (
        /* System Logs & Security Events Timeline */
        <div className="bg-white rounded-lg border border-gray-200 p-6">
          <div className="space-y-4">
            {filteredEntries.map((entry: any, index: number) => {
              const severity = entry.severity?.toLowerCase() || 'info';
              const SeverityIcon = severityIcons[severity as keyof typeof severityIcons] || Info;
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
                    <div className="flex flex-col items-center">
                      <div
                        className={clsx(
                          'w-8 h-8 rounded-full flex items-center justify-center',
                          severityColors[severity as keyof typeof severityColors] || 'text-gray-600 bg-gray-50'
                        )}
                      >
                        <SeverityIcon className="w-4 h-4" />
                      </div>
                      {index < filteredEntries.length - 1 && (
                        <div className="w-0.5 flex-1 bg-gray-200 mt-2" style={{ minHeight: '20px' }} />
                      )}
                    </div>

                    <div className="flex-1 pb-6">
                      <div className="flex items-start justify-between">
                        <div className="flex-1">
                          <div className="flex items-center gap-2 mb-1">
                            <span className="font-semibold text-gray-900">{entry.eventType}</span>
                            {entry.agentType && (
                              <>
                                <span className="text-sm text-gray-500">•</span>
                                <span className="text-sm text-gray-600">{entry.agentType}</span>
                              </>
                            )}
                          </div>

                          <p className="text-sm text-gray-700 mb-2">{entry.description}</p>

                          {entry.metadata && Object.keys(entry.metadata).length > 0 && (
                            <div className="mt-2 p-3 bg-gray-50 rounded-lg">
                              <pre className="text-xs text-gray-700 whitespace-pre-wrap font-mono">
                                {JSON.stringify(entry.metadata, null, 2)}
                              </pre>
                            </div>
                          )}
                        </div>

                        <span className="text-sm text-gray-500 ml-4">
                          {formatDistanceToNow(new Date(entry.timestamp), { addSuffix: true })}
                        </span>
                      </div>
                    </div>
                  </div>
                </div>
              );
            })}
          </div>
        </div>
      ) : (
        /* Regular Audit Trail Timeline */
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
