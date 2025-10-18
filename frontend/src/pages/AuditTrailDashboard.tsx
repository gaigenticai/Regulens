/**
 * Audit Trail Dashboard - Phase 7B.2
 * Change history, entity versioning, rollback capability
 */

import React, { useState, useEffect } from 'react';
import { securityAPI } from '../services/api';

interface AuditRecord {
  change_id: string;
  entity_type: string;
  entity_id: string;
  operation: string;
  changed_by: string;
  changed_at: string;
  impact_level: 'LOW' | 'MEDIUM' | 'HIGH' | 'CRITICAL';
  description: string;
}

export const AuditTrailDashboard: React.FC = () => {
  const [activeTab, setActiveTab] = useState<'history' | 'rollback' | 'reports'>('history');
  const [changes, setChanges] = useState<AuditRecord[]>([]);
  const [highImpactChanges, setHighImpactChanges] = useState<AuditRecord[]>([]);
  const [selectedEntity, setSelectedEntity] = useState('');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [daysFilter, setDaysFilter] = useState(7);
  const [selectedChangeId, setSelectedChangeId] = useState('');
  const [rollbackReason, setRollbackReason] = useState('');

  useEffect(() => {
    loadAuditData();
  }, [activeTab, daysFilter]);

  const loadAuditData = async () => {
    setLoading(true);
    setError(null);
    try {
      const [auditReport, impactfulChanges] = await Promise.all([
        securityAPI.generateAuditReport(daysFilter),
        securityAPI.getHighImpactChanges(daysFilter),
      ]);
      setChanges(auditReport?.changes || []);
      setHighImpactChanges(impactfulChanges || []);
    } catch (err) {
      setError('Failed to load audit data');
      console.error(err);
    } finally {
      setLoading(false);
    }
  };

  const handleRollback = async () => {
    if (!selectedChangeId || !rollbackReason) {
      setError('Please select a change and provide a reason');
      return;
    }

    try {
      const rollback = await securityAPI.submitRollback(selectedChangeId, rollbackReason);
      if (rollback?.rollback_id) {
        await securityAPI.executeRollback(rollback.rollback_id);
        setSelectedChangeId('');
        setRollbackReason('');
        setError(null);
        loadAuditData();
      }
    } catch (err) {
      setError('Failed to execute rollback');
      console.error(err);
    }
  };

  const getImpactColor = (level: string) => {
    switch (level) {
      case 'CRITICAL': return 'bg-red-100 text-red-800';
      case 'HIGH': return 'bg-orange-100 text-orange-800';
      case 'MEDIUM': return 'bg-yellow-100 text-yellow-800';
      case 'LOW': return 'bg-green-100 text-green-800';
      default: return 'bg-gray-100 text-gray-800';
    }
  };

  const getOperationIcon = (op: string) => {
    switch (op) {
      case 'CREATE': return '➕';
      case 'UPDATE': return '✏️';
      case 'DELETE': return '🗑️';
      case 'APPROVE': return '✅';
      case 'REJECT': return '❌';
      default: return '🔄';
    }
  };

  const renderHistoryTab = () => (
    <div className="space-y-6">
      <div className="flex justify-between items-center">
        <h3 className="text-lg font-semibold">Change History</h3>
        <select
          value={daysFilter}
          onChange={(e) => setDaysFilter(parseInt(e.target.value))}
          className="px-3 py-2 border rounded"
        >
          <option value={1}>Last 24 hours</option>
          <option value={7}>Last 7 days</option>
          <option value={30}>Last 30 days</option>
          <option value={90}>Last 90 days</option>
        </select>
      </div>

      <div className="space-y-3">
        {changes.map((change) => (
          <div key={change.change_id} className="bg-white p-4 rounded-lg border border-gray-200 hover:shadow-md transition">
            <div className="flex items-start gap-4">
              <div className="text-2xl">{getOperationIcon(change.operation)}</div>
              <div className="flex-1">
                <div className="flex justify-between items-start mb-2">
                  <div>
                    <h4 className="font-semibold text-gray-900">
                      {change.operation} {change.entity_type}
                    </h4>
                    <p className="text-sm text-gray-600 mt-1">{change.description}</p>
                  </div>
                  <span className={`px-2 py-1 text-xs font-medium rounded ${getImpactColor(change.impact_level)}`}>
                    {change.impact_level}
                  </span>
                </div>
                <div className="flex gap-4 text-sm text-gray-500">
                  <span>ID: {change.entity_id}</span>
                  <span>By: {change.changed_by}</span>
                  <span>{new Date(change.changed_at).toLocaleString()}</span>
                </div>
              </div>
            </div>
          </div>
        ))}
      </div>
    </div>
  );

  const renderRollbackTab = () => (
    <div className="space-y-6">
      <h3 className="text-lg font-semibold">Rollback Management</h3>

      <div className="bg-yellow-50 p-4 rounded-lg border border-yellow-200">
        <p className="text-sm text-yellow-800">
          ⚠️ Rollback will revert selected change and all dependent changes. This action is audited.
        </p>
      </div>

      <div className="bg-white p-4 rounded-lg border border-gray-200 space-y-4">
        <div>
          <label className="block text-sm font-medium text-gray-700 mb-2">
            Select Change to Rollback
          </label>
          <select
            value={selectedChangeId}
            onChange={(e) => setSelectedChangeId(e.target.value)}
            className="w-full px-3 py-2 border rounded"
          >
            <option value="">-- Select a change --</option>
            {highImpactChanges.map((change) => (
              <option key={change.change_id} value={change.change_id}>
                {change.operation} {change.entity_type} - {change.description} ({new Date(change.changed_at).toLocaleString()})
              </option>
            ))}
          </select>
        </div>

        <div>
          <label className="block text-sm font-medium text-gray-700 mb-2">
            Rollback Reason
          </label>
          <textarea
            value={rollbackReason}
            onChange={(e) => setRollbackReason(e.target.value)}
            placeholder="Provide reason for rollback..."
            className="w-full px-3 py-2 border rounded"
            rows={4}
          />
        </div>

        {selectedChangeId && (
          <div className="bg-blue-50 p-4 rounded border border-blue-200">
            <h4 className="font-medium text-blue-900 mb-2">Change Details</h4>
            <div className="text-sm text-blue-800 space-y-1">
              {highImpactChanges
                .filter((c) => c.change_id === selectedChangeId)
                .map((c) => (
                  <div key={c.change_id}>
                    <p>Entity: {c.entity_type} ({c.entity_id})</p>
                    <p>Changed by: {c.changed_by}</p>
                    <p>Date: {new Date(c.changed_at).toLocaleString()}</p>
                  </div>
                ))}
            </div>
          </div>
        )}

        <button
          onClick={handleRollback}
          disabled={!selectedChangeId || !rollbackReason}
          className="w-full bg-red-600 text-white px-4 py-2 rounded hover:bg-red-700 disabled:bg-gray-400"
        >
          Execute Rollback
        </button>
      </div>
    </div>
  );

  const renderReportsTab = () => (
    <div className="space-y-6">
      <h3 className="text-lg font-semibold">Compliance Reports</h3>

      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        <div className="bg-white p-6 rounded-lg border border-gray-200">
          <h4 className="font-semibold text-gray-900 mb-4">SOC2 Audit Report</h4>
          <p className="text-sm text-gray-600 mb-4">
            Complete audit trail for SOC2 Type II compliance certification
          </p>
          <button
            onClick={async () => {
              try {
                const report = await securityAPI.generateSOC2Report(365);
                console.log(report);
                alert('SOC2 report generated - check console');
              } catch (err) {
                setError('Failed to generate SOC2 report');
              }
            }}
            className="w-full bg-blue-600 text-white px-4 py-2 rounded hover:bg-blue-700"
          >
            Generate SOC2 Report
          </button>
        </div>

        <div className="bg-white p-6 rounded-lg border border-gray-200">
          <h4 className="font-semibold text-gray-900 mb-4">GDPR Compliance Report</h4>
          <p className="text-sm text-gray-600 mb-4">
            Data processing and consent audit for GDPR compliance
          </p>
          <button
            onClick={async () => {
              try {
                const report = await securityAPI.generateGDPRReport(90);
                console.log(report);
                alert('GDPR report generated - check console');
              } catch (err) {
                setError('Failed to generate GDPR report');
              }
            }}
            className="w-full bg-green-600 text-white px-4 py-2 rounded hover:bg-green-700"
          >
            Generate GDPR Report
          </button>
        </div>
      </div>

      <div className="bg-white p-6 rounded-lg border border-gray-200">
        <h4 className="font-semibold text-gray-900 mb-4">High Impact Changes (Last 7 days)</h4>
        <div className="space-y-2">
          {highImpactChanges.slice(0, 10).map((change) => (
            <div key={change.change_id} className="flex justify-between items-center p-3 bg-gray-50 rounded">
              <span className="text-sm">{change.description}</span>
              <span className={`px-2 py-1 text-xs font-medium rounded ${getImpactColor(change.impact_level)}`}>
                {change.impact_level}
              </span>
            </div>
          ))}
        </div>
      </div>
    </div>
  );

  return (
    <div className="min-h-screen bg-gray-100 p-6">
      <div className="max-w-6xl mx-auto">
        {/* Header */}
        <div className="mb-8">
          <h1 className="text-3xl font-bold text-gray-900">Audit Trail Dashboard</h1>
          <p className="text-gray-600 mt-2">Change history, versioning, rollback, and compliance</p>
        </div>

        {/* Tabs */}
        <div className="bg-white rounded-lg shadow mb-6 border-b">
          <div className="flex">
            {[
              { id: 'history', label: 'Change History', icon: '📜' },
              { id: 'rollback', label: 'Rollback', icon: '⏮️' },
              { id: 'reports', label: 'Reports', icon: '📋' },
            ].map((tab) => (
              <button
                key={tab.id}
                onClick={() => setActiveTab(tab.id as any)}
                className={`flex-1 px-6 py-4 font-medium text-center transition-colors ${
                  activeTab === tab.id
                    ? 'border-b-2 border-blue-600 text-blue-600'
                    : 'text-gray-600 hover:text-gray-900'
                }`}
              >
                <span className="mr-2">{tab.icon}</span>
                {tab.label}
              </button>
            ))}
          </div>
        </div>

        {/* Content */}
        <div className="bg-white rounded-lg shadow p-6">
          {loading ? (
            <div className="flex justify-center items-center h-96">
              <div className="text-center">
                <div className="w-12 h-12 bg-blue-200 rounded-full animate-spin mx-auto mb-4"></div>
                <p className="text-gray-600">Loading...</p>
              </div>
            </div>
          ) : error ? (
            <div className="text-red-600 p-4 bg-red-50 rounded">{error}</div>
          ) : (
            <>
              {activeTab === 'history' && renderHistoryTab()}
              {activeTab === 'rollback' && renderRollbackTab()}
              {activeTab === 'reports' && renderReportsTab()}
            </>
          )}
        </div>
      </div>
    </div>
  );
};

export default AuditTrailDashboard;
