/**
 * Alert Management Page
 * Feature 2: Regulatory Change Alerts with Email
 * Production-grade UI - NO MOCKS
 */

import React, { useState } from 'react';
import { Bell, Plus, RefreshCw, Mail, CheckCircle, XCircle, Clock } from 'lucide-react';
import { useAlertRules, useAlertDeliveryLog, useAlertStats, useCreateAlertRule } from '@/hooks/useAlerts';
import { LoadingSpinner } from '@/components/LoadingSpinner';

const AlertManagement: React.FC = () => {
  const [showCreateModal, setShowCreateModal] = useState(false);
  
  const { data: rules = [], isLoading: rulesLoading, refetch: refetchRules } = useAlertRules();
  const { data: deliveryLog = [], isLoading: logLoading } = useAlertDeliveryLog();
  const { data: stats, isLoading: statsLoading } = useAlertStats();
  const createRule = useCreateAlertRule();

  const handleCreateRule = async (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    const formData = new FormData(e.currentTarget);
    
    const recipients = (formData.get('recipients') as string).split(',').map(r => r.trim());
    
    await createRule.mutateAsync({
      name: formData.get('name') as string,
      description: formData.get('description') as string,
      recipients,
      enabled: true,
      severity_filter: ['critical', 'high'],
      notification_channels: ['email'],
      throttle_minutes: 60,
    });
    
    setShowCreateModal(false);
    e.currentTarget.reset();
  };

  if (rulesLoading || statsLoading) {
    return <LoadingSpinner fullScreen message="Loading alert management..." />;
  }

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'sent': return 'text-green-600 bg-green-100';
      case 'pending': return 'text-yellow-600 bg-yellow-100';
      case 'failed': return 'text-red-600 bg-red-100';
      case 'throttled': return 'text-orange-600 bg-orange-100';
      default: return 'text-gray-600 bg-gray-100';
    }
  };

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-gray-900">Alert Management</h1>
          <p className="text-gray-600 mt-1">
            Regulatory change alerts and email notifications
          </p>
        </div>

        <div className="flex gap-3">
          <button
            onClick={() => refetchRules()}
            className="flex items-center gap-2 px-4 py-2 bg-white border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors"
          >
            <RefreshCw className={`w-5 h-5 ${rulesLoading ? 'animate-spin' : ''}`} />
            Refresh
          </button>
          
          <button
            onClick={() => setShowCreateModal(true)}
            className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors"
          >
            <Plus className="w-5 h-5" />
            New Alert Rule
          </button>
        </div>
      </div>

      {/* Statistics */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-6">
        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-3 mb-2">
            <div className="p-2 bg-blue-100 rounded-lg">
              <Bell className="w-5 h-5 text-blue-600" />
            </div>
            <h3 className="text-sm font-medium text-gray-600">Total Rules</h3>
          </div>
          <p className="text-2xl font-bold text-gray-900">{stats?.total_rules || 0}</p>
          <p className="text-xs text-gray-500 mt-1">{stats?.active_rules || 0} active</p>
        </div>

        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-3 mb-2">
            <div className="p-2 bg-green-100 rounded-lg">
              <CheckCircle className="w-5 h-5 text-green-600" />
            </div>
            <h3 className="text-sm font-medium text-gray-600">Active Rules</h3>
          </div>
          <p className="text-2xl font-bold text-gray-900">{stats?.active_rules || 0}</p>
          <p className="text-xs text-gray-500 mt-1">Currently enabled</p>
        </div>

        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-3 mb-2">
            <div className="p-2 bg-purple-100 rounded-lg">
              <Mail className="w-5 h-5 text-purple-600" />
            </div>
            <h3 className="text-sm font-medium text-gray-600">Total Deliveries</h3>
          </div>
          <p className="text-2xl font-bold text-gray-900">{stats?.total_deliveries || 0}</p>
          <p className="text-xs text-gray-500 mt-1">All time</p>
        </div>

        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-3 mb-2">
            <div className="p-2 bg-green-100 rounded-lg">
              <CheckCircle className="w-5 h-5 text-green-600" />
            </div>
            <h3 className="text-sm font-medium text-gray-600">Successful</h3>
          </div>
          <p className="text-2xl font-bold text-gray-900">{stats?.successful_deliveries || 0}</p>
          <p className="text-xs text-gray-500 mt-1">
            {stats?.total_deliveries ? `${((stats.successful_deliveries / stats.total_deliveries) * 100).toFixed(1)}%` : 'N/A'}
          </p>
        </div>
      </div>

      {/* Alert Rules */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200">
        <div className="p-6 border-b border-gray-200">
          <h2 className="text-lg font-semibold text-gray-900">Alert Rules</h2>
          <p className="text-sm text-gray-600 mt-1">Configure notification rules for regulatory changes</p>
        </div>

        <div className="divide-y divide-gray-200">
          {rules.length === 0 ? (
            <div className="p-12 text-center">
              <Bell className="w-12 h-12 text-gray-400 mx-auto mb-4" />
              <h3 className="text-lg font-medium text-gray-900 mb-2">No alert rules configured</h3>
              <p className="text-gray-600 mb-4">Create your first alert rule to start receiving notifications</p>
              <button
                onClick={() => setShowCreateModal(true)}
                className="inline-flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors"
              >
                <Plus className="w-5 h-5" />
                Create Alert Rule
              </button>
            </div>
          ) : (
            rules.map((rule) => (
              <div key={rule.rule_id} className="p-6 hover:bg-gray-50 transition-colors">
                <div className="flex items-start justify-between">
                  <div className="flex-1">
                    <div className="flex items-center gap-3 mb-2">
                      <h3 className="text-lg font-semibold text-gray-900">{rule.name}</h3>
                      <span
                        className={`px-2 py-1 text-xs font-medium rounded-full ${
                          rule.enabled
                            ? 'bg-green-100 text-green-800'
                            : 'bg-gray-100 text-gray-800'
                        }`}
                      >
                        {rule.enabled ? 'Enabled' : 'Disabled'}
                      </span>
                    </div>
                    <p className="text-gray-600 text-sm mb-3">{rule.description}</p>
                    <div className="flex items-center gap-6 text-sm text-gray-500">
                      <div className="flex items-center gap-1">
                        <Mail className="w-4 h-4" />
                        <span>{rule.recipients.length} recipient(s)</span>
                      </div>
                      <div className="flex items-center gap-1">
                        <Clock className="w-4 h-4" />
                        <span>{rule.throttle_minutes}min throttle</span>
                      </div>
                      <div className="flex items-center gap-1">
                        <Bell className="w-4 h-4" />
                        <span>{rule.trigger_count} triggers</span>
                      </div>
                    </div>
                  </div>
                </div>
              </div>
            ))
          )}
        </div>
      </div>

      {/* Delivery Log */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200">
        <div className="p-6 border-b border-gray-200">
          <h2 className="text-lg font-semibold text-gray-900">Recent Deliveries</h2>
          <p className="text-sm text-gray-600 mt-1">Email delivery status and history</p>
        </div>

        <div className="overflow-x-auto">
          <table className="w-full">
            <thead className="bg-gray-50">
              <tr>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                  Recipient
                </th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                  Channel
                </th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                  Status
                </th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                  Sent At
                </th>
              </tr>
            </thead>
            <tbody className="bg-white divide-y divide-gray-200">
              {deliveryLog.length === 0 ? (
                <tr>
                  <td colSpan={4} className="px-6 py-12 text-center text-gray-500">
                    No delivery history yet
                  </td>
                </tr>
              ) : (
                deliveryLog.map((log) => (
                  <tr key={log.delivery_id} className="hover:bg-gray-50">
                    <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">
                      {log.recipient}
                    </td>
                    <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">
                      {log.channel}
                    </td>
                    <td className="px-6 py-4 whitespace-nowrap">
                      <span className={`px-2 py-1 text-xs font-medium rounded-full ${getStatusColor(log.status)}`}>
                        {log.status}
                      </span>
                    </td>
                    <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                      {new Date(log.sent_at).toLocaleString()}
                    </td>
                  </tr>
                ))
              )}
            </tbody>
          </table>
        </div>
      </div>

      {/* Create Rule Modal */}
      {showCreateModal && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
          <div className="bg-white rounded-lg shadow-xl max-w-md w-full mx-4">
            <div className="p-6 border-b border-gray-200">
              <h2 className="text-xl font-semibold text-gray-900">Create Alert Rule</h2>
            </div>
            
            <form onSubmit={handleCreateRule} className="p-6 space-y-4">
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">
                  Rule Name *
                </label>
                <input
                  type="text"
                  name="name"
                  required
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                />
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">
                  Description
                </label>
                <textarea
                  name="description"
                  rows={2}
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                />
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">
                  Recipients (comma-separated) *
                </label>
                <input
                  type="text"
                  name="recipients"
                  required
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                />
              </div>

              <div className="flex gap-3 pt-4">
                <button
                  type="button"
                  onClick={() => setShowCreateModal(false)}
                  className="flex-1 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors"
                >
                  Cancel
                </button>
                <button
                  type="submit"
                  disabled={createRule.isPending}
                  className="flex-1 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors disabled:opacity-50"
                >
                  {createRule.isPending ? 'Creating...' : 'Create Rule'}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  );
};

export default AlertManagement;

