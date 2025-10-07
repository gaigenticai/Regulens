/**
 * Fraud Detection Rules Page
 * Manage fraud detection rules and view fraud alerts
 * NO MOCKS - Real rule management and fraud analytics
 */

import React, { useState } from 'react';
import { Shield, Plus, Edit2, Trash2, Power, AlertTriangle, TrendingUp } from 'lucide-react';
import { useFraudRules, useCreateFraudRule, useUpdateFraudRule, useDeleteFraudRule, useToggleFraudRule, useFraudAlerts, useFraudStats } from '@/hooks/useFraudDetection';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { clsx } from 'clsx';
import { formatDistanceToNow } from 'date-fns';
import type { FraudRule, FraudAlert, CreateFraudRuleRequest } from '@/types/api';

const FraudRules: React.FC = () => {
  const [showCreateModal, setShowCreateModal] = useState(false);
  const [editingRule, setEditingRule] = useState<FraudRule | null>(null);
  const [timeRange] = useState<'24h' | '7d' | '30d'>('7d');

  const { data: rules = [], isLoading } = useFraudRules();
  const { data: alerts = [] } = useFraudAlerts({ limit: 10, status: 'new' });
  const { data: stats } = useFraudStats(timeRange);
  const { mutate: createRule, isPending: isCreating } = useCreateFraudRule();
  const { mutate: updateRule, isPending: isUpdating } = useUpdateFraudRule();
  const { mutate: deleteRule, isPending: isDeleting } = useDeleteFraudRule();
  const { mutate: toggleRule } = useToggleFraudRule();

  const [formData, setFormData] = useState<CreateFraudRuleRequest>({
    name: '',
    description: '',
    ruleType: 'threshold',
    condition: '',
    threshold: 0,
    action: 'flag',
    severity: 'medium',
    enabled: true,
  });

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();

    if (editingRule) {
      updateRule(
        { id: editingRule.id, rule: formData },
        {
          onSuccess: () => {
            setEditingRule(null);
            setShowCreateModal(false);
            resetForm();
          },
        }
      );
    } else {
      createRule(formData, {
        onSuccess: () => {
          setShowCreateModal(false);
          resetForm();
        },
      });
    }
  };

  const resetForm = () => {
    setFormData({
      name: '',
      description: '',
      ruleType: 'threshold',
      condition: '',
      threshold: 0,
      action: 'flag',
      severity: 'medium',
      enabled: true,
    });
  };

  const handleEdit = (rule: FraudRule) => {
    setEditingRule(rule);
    setFormData({
      name: rule.name,
      description: rule.description,
      ruleType: rule.ruleType || 'threshold',
      condition: rule.condition || '',
      threshold: rule.threshold || 0,
      action: rule.action || 'flag',
      severity: rule.severity || 'medium',
      enabled: rule.enabled,
    });
    setShowCreateModal(true);
  };

  const handleDelete = (id: string) => {
    if (window.confirm('Are you sure you want to delete this rule?')) {
      deleteRule(id);
    }
  };

  const severityColors = {
    low: 'bg-blue-100 text-blue-700',
    medium: 'bg-yellow-100 text-yellow-700',
    high: 'bg-orange-100 text-orange-700',
    critical: 'bg-red-100 text-red-700',
  };

  if (isLoading) {
    return <LoadingSpinner fullScreen message="Loading fraud rules..." />;
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-gray-900 flex items-center gap-2">
            <Shield className="w-7 h-7 text-blue-600" />
            Fraud Detection Rules
          </h1>
          <p className="text-gray-600 mt-1">Configure and manage fraud detection rules and alerts</p>
        </div>

        <button
          onClick={() => {
            resetForm();
            setEditingRule(null);
            setShowCreateModal(true);
          }}
          className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors"
        >
          <Plus className="w-5 h-5" />
          New Rule
        </button>
      </div>

      {/* Statistics */}
      {stats && (
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Total Alerts</p>
            <p className="text-2xl font-bold text-gray-900 mt-1">{stats.totalAlerts}</p>
            <p className="text-xs text-gray-500 mt-1">Last {timeRange === '24h' ? '24 hours' : timeRange === '7d' ? '7 days' : '30 days'}</p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Active Rules</p>
            <p className="text-2xl font-bold text-blue-600 mt-1">{stats.activeRules}</p>
            <p className="text-xs text-gray-500 mt-1">Enabled</p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Detection Rate</p>
            <p className="text-2xl font-bold text-green-600 mt-1">{(stats.detectionRate * 100).toFixed(1)}%</p>
            <p className="text-xs text-gray-500 mt-1">Accuracy</p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">False Positives</p>
            <p className="text-2xl font-bold text-orange-600 mt-1">{stats.falsePositives}</p>
            <p className="text-xs text-gray-500 mt-1">To review</p>
          </div>
        </div>
      )}

      {/* Recent Alerts */}
      {alerts.length > 0 && (
        <div className="bg-white rounded-lg border border-gray-200 p-6">
          <h3 className="text-lg font-semibold text-gray-900 mb-4 flex items-center gap-2">
            <AlertTriangle className="w-5 h-5 text-orange-600" />
            Recent Fraud Alerts
          </h3>
          <div className="space-y-3">
            {alerts.map((alert: FraudAlert) => (
              <div key={alert.id} className="p-4 bg-orange-50 border border-orange-200 rounded-lg">
                <div className="flex items-start justify-between">
                  <div className="flex-1">
                    <div className="flex items-center gap-2 mb-1">
                      <span className="font-semibold text-gray-900">{alert.ruleId}</span>
                      <span className={clsx('px-2 py-0.5 rounded-full text-xs font-medium', severityColors[alert.severity as keyof typeof severityColors])}>
                        {alert.severity}
                      </span>
                    </div>
                    <p className="text-sm text-gray-600">Transaction: {alert.transactionId}</p>
                  </div>
                  <span className="text-sm text-gray-500">
                    {formatDistanceToNow(new Date(alert.timestamp), { addSuffix: true })}
                  </span>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Rules List */}
      <div className="bg-white rounded-lg border border-gray-200">
        <div className="p-6 border-b border-gray-200">
          <h3 className="text-lg font-semibold text-gray-900">Active Rules ({rules.length})</h3>
        </div>

        {rules.length === 0 ? (
          <div className="text-center py-12">
            <Shield className="w-12 h-12 text-gray-400 mx-auto mb-3" />
            <p className="text-gray-600">No fraud rules configured</p>
            <button
              onClick={() => setShowCreateModal(true)}
              className="mt-4 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700"
            >
              Create First Rule
            </button>
          </div>
        ) : (
          <div className="divide-y divide-gray-200">
            {rules.map((rule: FraudRule) => (
              <div key={rule.id} className="p-6 hover:bg-gray-50 transition-colors">
                <div className="flex items-start justify-between">
                  <div className="flex-1">
                    <div className="flex items-center gap-3 mb-2">
                      <h4 className="text-lg font-semibold text-gray-900">{rule.name}</h4>
                      <span className={clsx('px-2 py-1 rounded-full text-xs font-medium', severityColors[rule.severity as keyof typeof severityColors])}>
                        {rule.severity}
                      </span>
                      <span className={clsx(
                        'px-2 py-1 rounded-full text-xs font-medium',
                        rule.enabled ? 'bg-green-100 text-green-700' : 'bg-gray-100 text-gray-700'
                      )}>
                        {rule.enabled ? 'Enabled' : 'Disabled'}
                      </span>
                    </div>
                    <p className="text-sm text-gray-600 mb-3">{rule.description}</p>

                    <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
                      <div>
                        <p className="text-xs text-gray-500">Type</p>
                        <p className="text-sm font-medium text-gray-900 capitalize">{rule.ruleType}</p>
                      </div>

                      <div>
                        <p className="text-xs text-gray-500">Condition</p>
                        <p className="text-sm font-mono font-medium text-gray-900">{rule.condition}</p>
                      </div>

                      {rule.threshold !== undefined && (
                        <div>
                          <p className="text-xs text-gray-500">Threshold</p>
                          <p className="text-sm font-medium text-gray-900">{rule.threshold}</p>
                        </div>
                      )}

                      <div>
                        <p className="text-xs text-gray-500">Action</p>
                        <p className="text-sm font-medium text-gray-900 capitalize">{rule.action}</p>
                      </div>
                    </div>

                    {rule.triggerCount !== undefined && (
                      <div className="mt-3 flex items-center gap-4 text-sm">
                        <div className="flex items-center gap-1">
                          <TrendingUp className="w-4 h-4 text-blue-600" />
                          <span className="text-gray-600">Triggered:</span>
                          <span className="font-medium text-gray-900">{rule.triggerCount} times</span>
                        </div>
                        {rule.lastTriggered && (
                          <span className="text-gray-500">
                            Last: {formatDistanceToNow(new Date(rule.lastTriggered), { addSuffix: true })}
                          </span>
                        )}
                      </div>
                    )}
                  </div>

                  <div className="flex items-center gap-2 ml-4">
                    <button
                      onClick={() => toggleRule({ id: rule.id, enabled: !rule.enabled })}
                      className={clsx(
                        'p-2 rounded-lg transition-colors',
                        rule.enabled ? 'bg-green-100 text-green-700 hover:bg-green-200' : 'bg-gray-100 text-gray-700 hover:bg-gray-200'
                      )}
                      title={rule.enabled ? 'Disable rule' : 'Enable rule'}
                    >
                      <Power className="w-5 h-5" />
                    </button>

                    <button
                      onClick={() => handleEdit(rule)}
                      className="p-2 bg-blue-100 text-blue-700 rounded-lg hover:bg-blue-200 transition-colors"
                      title="Edit rule"
                    >
                      <Edit2 className="w-5 h-5" />
                    </button>

                    <button
                      onClick={() => handleDelete(rule.id)}
                      disabled={isDeleting}
                      className="p-2 bg-red-100 text-red-700 rounded-lg hover:bg-red-200 transition-colors disabled:opacity-50"
                      title="Delete rule"
                    >
                      <Trash2 className="w-5 h-5" />
                    </button>
                  </div>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Create/Edit Modal */}
      {showCreateModal && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
          <div className="bg-white rounded-lg p-6 max-w-2xl w-full mx-4 max-h-[90vh] overflow-y-auto">
            <h2 className="text-2xl font-bold text-gray-900 mb-6">
              {editingRule ? 'Edit Fraud Rule' : 'Create Fraud Rule'}
            </h2>

            <form onSubmit={handleSubmit} className="space-y-4">
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">Rule Name *</label>
                <input
                  type="text"
                  value={formData.name}
                  onChange={(e) => setFormData({ ...formData, name: e.target.value })}
                  className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
                  required
                />
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">Description *</label>
                <textarea
                  value={formData.description}
                  onChange={(e) => setFormData({ ...formData, description: e.target.value })}
                  rows={3}
                  className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
                  required
                />
              </div>

              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">Rule Type *</label>
                  <select
                    value={formData.ruleType}
                    onChange={(e) => setFormData({ ...formData, ruleType: e.target.value as any })}
                    className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
                  >
                    <option value="threshold">Threshold</option>
                    <option value="pattern">Pattern</option>
                    <option value="anomaly">Anomaly</option>
                    <option value="velocity">Velocity</option>
                    <option value="blacklist">Blacklist</option>
                  </select>
                </div>

                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">Severity *</label>
                  <select
                    value={formData.severity}
                    onChange={(e) => setFormData({ ...formData, severity: e.target.value as any })}
                    className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
                  >
                    <option value="low">Low</option>
                    <option value="medium">Medium</option>
                    <option value="high">High</option>
                    <option value="critical">Critical</option>
                  </select>
                </div>
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">Condition *</label>
                <input
                  type="text"
                  value={formData.condition}
                  onChange={(e) => setFormData({ ...formData, condition: e.target.value })}
                  placeholder="e.g., amount > threshold"
                  className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
                  required
                />
              </div>

              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">Threshold</label>
                  <input
                    type="number"
                    value={formData.threshold}
                    onChange={(e) => setFormData({ ...formData, threshold: parseFloat(e.target.value) })}
                    className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
                  />
                </div>

                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">Action *</label>
                  <select
                    value={formData.action}
                    onChange={(e) => setFormData({ ...formData, action: e.target.value as any })}
                    className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
                  >
                    <option value="flag">Flag</option>
                    <option value="block">Block</option>
                    <option value="review">Review</option>
                    <option value="alert">Alert</option>
                  </select>
                </div>
              </div>

              <div className="flex items-center gap-2">
                <input
                  type="checkbox"
                  id="enabled"
                  checked={formData.enabled}
                  onChange={(e) => setFormData({ ...formData, enabled: e.target.checked })}
                  className="w-4 h-4 text-blue-600 border-gray-300 rounded focus:ring-blue-500"
                />
                <label htmlFor="enabled" className="text-sm font-medium text-gray-700">
                  Enable rule immediately
                </label>
              </div>

              <div className="flex gap-3 pt-4">
                <button
                  type="button"
                  onClick={() => {
                    setShowCreateModal(false);
                    setEditingRule(null);
                    resetForm();
                  }}
                  className="flex-1 px-4 py-2 bg-gray-200 text-gray-700 rounded-lg hover:bg-gray-300 transition-colors"
                >
                  Cancel
                </button>

                <button
                  type="submit"
                  disabled={isCreating || isUpdating}
                  className="flex-1 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors disabled:opacity-50"
                >
                  {isCreating || isUpdating ? 'Saving...' : editingRule ? 'Update Rule' : 'Create Rule'}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  );
};

export default FraudRules;
