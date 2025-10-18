/**
 * Alert Dashboard - Phase 7C.1
 * Active alerts, anomalies, correlations, and alert management
 */

import React, { useState, useEffect } from 'react';
import { monitoringAPI } from '../services/api';

interface Alert {
  alert_id: string;
  title: string;
  description: string;
  severity: 'INFO' | 'WARNING' | 'ERROR' | 'CRITICAL';
  affected_metrics: string[];
  created_at: string;
  is_acknowledged: boolean;
  acknowledged_by?: string;
}

interface Anomaly {
  anomaly_id: string;
  metric_name: string;
  anomaly_score: number;
  detected_at: string;
  is_confirmed: boolean;
}

export const AlertDashboard: React.FC = () => {
  const [activeTab, setActiveTab] = useState<'active' | 'anomalies' | 'correlations'>('active');
  const [alerts, setAlerts] = useState<Alert[]>([]);
  const [anomalies, setAnomalies] = useState<Anomaly[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [userId, setUserId] = useState('system');

  useEffect(() => {
    loadAlertData();
  }, [activeTab]);

  const loadAlertData = async () => {
    setLoading(true);
    setError(null);
    try {
      const [activeAlerts, detectedAnomalies] = await Promise.all([
        monitoringAPI.getActiveAlerts(),
        monitoringAPI.getAnomalies(10),
      ]);
      setAlerts(activeAlerts || []);
      setAnomalies(detectedAnomalies || []);
    } catch (err) {
      setError('Failed to load alert data');
      console.error(err);
    } finally {
      setLoading(false);
    }
  };

  const handleAcknowledge = async (alertId: string) => {
    try {
      await monitoringAPI.acknowledgeAlert(alertId, userId);
      loadAlertData();
    } catch (err) {
      setError('Failed to acknowledge alert');
    }
  };

  const handleResolve = async (alertId: string) => {
    try {
      await monitoringAPI.resolveAlert(alertId);
      loadAlertData();
    } catch (err) {
      setError('Failed to resolve alert');
    }
  };

  const getSeverityColor = (severity: string) => {
    switch (severity) {
      case 'CRITICAL': return 'bg-red-100 text-red-900 border-red-300';
      case 'ERROR': return 'bg-orange-100 text-orange-900 border-orange-300';
      case 'WARNING': return 'bg-yellow-100 text-yellow-900 border-yellow-300';
      case 'INFO': return 'bg-blue-100 text-blue-900 border-blue-300';
      default: return 'bg-gray-100 text-gray-900 border-gray-300';
    }
  };

  const getSeverityIcon = (severity: string) => {
    switch (severity) {
      case 'CRITICAL': return '🔴';
      case 'ERROR': return '🟠';
      case 'WARNING': return '🟡';
      case 'INFO': return '🔵';
      default: return '⚪';
    }
  };

  const renderActiveAlertsTab = () => (
    <div className="space-y-6">
      <div className="flex justify-between items-center">
        <h3 className="text-lg font-semibold">
          Active Alerts ({alerts.length})
        </h3>
        <button
          onClick={loadAlertData}
          className="bg-blue-600 text-white px-4 py-2 rounded hover:bg-blue-700"
        >
          Refresh
        </button>
      </div>

      {alerts.length === 0 ? (
        <div className="text-center p-8 bg-green-50 rounded-lg border border-green-200">
          <p className="text-green-800 font-medium">✓ No active alerts</p>
        </div>
      ) : (
        <div className="space-y-3">
          {alerts.map((alert) => (
            <div
              key={alert.alert_id}
              className={`p-4 rounded-lg border-l-4 ${getSeverityColor(alert.severity)}`}
            >
              <div className="flex items-start justify-between mb-2">
                <div className="flex items-start gap-3 flex-1">
                  <span className="text-2xl">{getSeverityIcon(alert.severity)}</span>
                  <div>
                    <h4 className="font-semibold">{alert.title}</h4>
                    <p className="text-sm mt-1 opacity-90">{alert.description}</p>
                    <div className="mt-2 flex gap-2 flex-wrap">
                      {alert.affected_metrics.map((metric, i) => (
                        <span
                          key={i}
                          className="inline-block px-2 py-1 text-xs bg-black bg-opacity-10 rounded"
                        >
                          {metric}
                        </span>
                      ))}
                    </div>
                  </div>
                </div>
                <div className="flex gap-2 ml-4">
                  {!alert.is_acknowledged ? (
                    <button
                      onClick={() => handleAcknowledge(alert.alert_id)}
                      className="bg-blue-500 text-white px-3 py-1 rounded text-sm hover:bg-blue-600"
                    >
                      Acknowledge
                    </button>
                  ) : (
                    <span className="text-xs px-2 py-1 bg-black bg-opacity-10 rounded">
                      ✓ Ack by {alert.acknowledged_by}
                    </span>
                  )}
                  <button
                    onClick={() => handleResolve(alert.alert_id)}
                    className="bg-green-500 text-white px-3 py-1 rounded text-sm hover:bg-green-600"
                  >
                    Resolve
                  </button>
                </div>
              </div>
              <div className="text-xs opacity-75">
                {new Date(alert.created_at).toLocaleString()}
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );

  const renderAnomaliesTab = () => (
    <div className="space-y-6">
      <h3 className="text-lg font-semibold">Detected Anomalies ({anomalies.length})</h3>

      <div className="space-y-3">
        {anomalies.map((anomaly) => (
          <div
            key={anomaly.anomaly_id}
            className={`p-4 rounded-lg border-2 ${
              anomaly.is_confirmed
                ? 'border-red-300 bg-red-50'
                : 'border-yellow-300 bg-yellow-50'
            }`}
          >
            <div className="flex justify-between items-start mb-2">
              <div>
                <h4 className="font-semibold text-gray-900">{anomaly.metric_name}</h4>
                <p className="text-sm text-gray-600 mt-1">
                  Detected: {new Date(anomaly.detected_at).toLocaleString()}
                </p>
              </div>
              <span
                className={`px-3 py-1 rounded-full text-sm font-medium ${
                  anomaly.is_confirmed
                    ? 'bg-red-200 text-red-800'
                    : 'bg-yellow-200 text-yellow-800'
                }`}
              >
                {anomaly.is_confirmed ? 'Confirmed' : 'Investigating'}
              </span>
            </div>
            <div className="mt-3">
              <div className="flex items-center gap-2">
                <div className="flex-1 bg-gray-200 rounded-full h-2">
                  <div
                    className={`h-2 rounded-full ${
                      anomaly.anomaly_score > 0.8
                        ? 'bg-red-600'
                        : anomaly.anomaly_score > 0.5
                        ? 'bg-orange-600'
                        : 'bg-yellow-600'
                    }`}
                    style={{ width: `${anomaly.anomaly_score * 100}%` }}
                  ></div>
                </div>
                <span className="text-sm font-medium w-12">
                  {(anomaly.anomaly_score * 100).toFixed(0)}%
                </span>
              </div>
              <p className="text-xs text-gray-600 mt-2">Anomaly Score</p>
            </div>
          </div>
        ))}
      </div>
    </div>
  );

  const renderCorrelationsTab = () => (
    <div className="space-y-6">
      <h3 className="text-lg font-semibold">Alert Correlations</h3>

      <div className="bg-blue-50 p-4 rounded-lg border border-blue-200">
        <p className="text-sm text-blue-800">
          ℹ️ Alerts are correlated by affected metrics to identify root causes
        </p>
      </div>

      <div className="bg-white rounded-lg border border-gray-200 p-4">
        <h4 className="font-semibold text-gray-900 mb-4">Related Alerts by Metric</h4>
        {alerts.length === 0 ? (
          <p className="text-gray-600 text-sm">No alerts to correlate</p>
        ) : (
          <div className="space-y-4">
            {[...new Set(alerts.flatMap((a) => a.affected_metrics))].map((metric) => {
              const relatedAlerts = alerts.filter((a) =>
                a.affected_metrics.includes(metric)
              );
              if (relatedAlerts.length === 0) return null;

              return (
                <div key={metric} className="bg-gray-50 p-3 rounded">
                  <h5 className="font-medium text-gray-900 mb-2">📊 {metric}</h5>
                  <div className="ml-4 space-y-1">
                    {relatedAlerts.map((alert) => (
                      <div
                        key={alert.alert_id}
                        className="text-sm text-gray-700 flex items-center gap-2"
                      >
                        <span>{getSeverityIcon(alert.severity)}</span>
                        <span>{alert.title}</span>
                      </div>
                    ))}
                  </div>
                </div>
              );
            })}
          </div>
        )}
      </div>

      <div className="bg-white rounded-lg border border-gray-200 p-4">
        <h4 className="font-semibold text-gray-900 mb-4">Severity Distribution</h4>
        <div className="space-y-2">
          {['CRITICAL', 'ERROR', 'WARNING', 'INFO'].map((severity) => {
            const count = alerts.filter((a) => a.severity === severity).length;
            return (
              <div key={severity} className="flex items-center justify-between">
                <span className="flex items-center gap-2">
                  {getSeverityIcon(severity)} {severity}
                </span>
                <div className="flex items-center gap-2">
                  <div className="w-32 bg-gray-200 rounded h-2">
                    <div
                      className={`h-2 rounded ${
                        severity === 'CRITICAL'
                          ? 'bg-red-600'
                          : severity === 'ERROR'
                          ? 'bg-orange-600'
                          : severity === 'WARNING'
                          ? 'bg-yellow-600'
                          : 'bg-blue-600'
                      }`}
                      style={{ width: `${count > 0 ? 80 : 0}%` }}
                    ></div>
                  </div>
                  <span className="w-8 text-right font-medium">{count}</span>
                </div>
              </div>
            );
          })}
        </div>
      </div>
    </div>
  );

  return (
    <div className="min-h-screen bg-gray-100 p-6">
      <div className="max-w-6xl mx-auto">
        {/* Header */}
        <div className="mb-8">
          <h1 className="text-3xl font-bold text-gray-900">Alert Dashboard</h1>
          <p className="text-gray-600 mt-2">Active alerts, anomalies, and correlation analysis</p>
        </div>

        {/* Tabs */}
        <div className="bg-white rounded-lg shadow mb-6 border-b">
          <div className="flex">
            {[
              { id: 'active', label: 'Active Alerts', icon: '🔔', badge: alerts.length },
              { id: 'anomalies', label: 'Anomalies', icon: '⚡', badge: anomalies.length },
              { id: 'correlations', label: 'Correlations', icon: '🔗' },
            ].map((tab) => (
              <button
                key={tab.id}
                onClick={() => setActiveTab(tab.id as any)}
                className={`flex-1 px-6 py-4 font-medium text-center transition-colors relative ${
                  activeTab === tab.id
                    ? 'border-b-2 border-blue-600 text-blue-600'
                    : 'text-gray-600 hover:text-gray-900'
                }`}
              >
                <span className="mr-2">{tab.icon}</span>
                {tab.label}
                {tab.badge !== undefined && tab.badge > 0 && (
                  <span className="ml-2 inline-block bg-red-600 text-white text-xs px-2 py-0.5 rounded-full">
                    {tab.badge}
                  </span>
                )}
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
              {activeTab === 'active' && renderActiveAlertsTab()}
              {activeTab === 'anomalies' && renderAnomaliesTab()}
              {activeTab === 'correlations' && renderCorrelationsTab()}
            </>
          )}
        </div>
      </div>
    </div>
  );
};

export default AlertDashboard;
