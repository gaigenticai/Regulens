/**
 * Metrics Dashboard - Phase 7C.2
 * Business, Technical, and Cost metrics monitoring
 */

import React, { useState, useEffect } from 'react';
import { monitoringAPI } from '../services/api';
import {
  LineChart, Line, BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer
} from 'recharts';

interface MetricValue {
  label: string;
  value: string | number;
  unit?: string;
  trend?: 'up' | 'down' | 'stable';
  status?: 'good' | 'warning' | 'critical';
}

export const MetricsDashboard: React.FC = () => {
  const [activeTab, setActiveTab] = useState<'business' | 'technical' | 'cost'>('business');
  const [businessMetrics, setBusinessMetrics] = useState<any>(null);
  const [technicalMetrics, setTechnicalMetrics] = useState<any>(null);
  const [costMetrics, setCostMetrics] = useState<any>(null);
  const [costRecs, setCostRecs] = useState<any[]>([]);
  const [perfRecs, setPerfRecs] = useState<any[]>([]);
  const [slaReport, setSlaReport] = useState<any>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    loadMetrics();
  }, [activeTab]);

  const loadMetrics = async () => {
    setLoading(true);
    setError(null);
    try {
      if (activeTab === 'business') {
        const biz = await monitoringAPI.getBusinessMetrics(60);
        setBusinessMetrics(biz);
      } else if (activeTab === 'technical') {
        const [tech, perf] = await Promise.all([
          monitoringAPI.getTechnicalMetrics(60),
          monitoringAPI.getPerformanceOptimizations(),
        ]);
        setTechnicalMetrics(tech);
        setPerfRecs(perf);
      } else if (activeTab === 'cost') {
        const [cost, recs] = await Promise.all([
          monitoringAPI.getCostMetrics(1),
          monitoringAPI.getCostOptimizations(),
        ]);
        setCostMetrics(cost);
        setCostRecs(recs);
      }
    } catch (err) {
      setError('Failed to load metrics');
      console.error(err);
    } finally {
      setLoading(false);
    }
  };

  const getStatusColor = (status?: string) => {
    switch (status) {
      case 'good': return 'bg-green-100 text-green-700';
      case 'warning': return 'bg-yellow-100 text-yellow-700';
      case 'critical': return 'bg-red-100 text-red-700';
      default: return 'bg-gray-100 text-gray-700';
    }
  };

  const renderMetricCard = (metric: MetricValue, index: number) => (
    <div key={index} className="bg-white p-4 rounded-lg border border-gray-200">
      <div className="text-sm text-gray-600 mb-1">{metric.label}</div>
      <div className="text-2xl font-bold text-gray-900">{metric.value}</div>
      {metric.unit && <div className="text-xs text-gray-500 mt-1">{metric.unit}</div>}
      {metric.status && (
        <div className={`inline-block mt-2 px-2 py-1 text-xs font-medium rounded ${getStatusColor(metric.status)}`}>
          {metric.status}
        </div>
      )}
    </div>
  );

  const renderBusinessMetrics = () => (
    <div className="space-y-6">
      {/* Key Metrics */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
        {businessMetrics && [
          { label: 'Total Decisions', value: businessMetrics.total_decisions, unit: 'decisions' },
          { label: 'Accuracy Rate', value: (businessMetrics.decision_accuracy * 100).toFixed(1) + '%', unit: 'overall', status: businessMetrics.decision_accuracy > 0.9 ? 'good' : 'warning' },
          { label: 'Success Rate', value: ((businessMetrics.successful_decisions / (businessMetrics.total_decisions || 1)) * 100).toFixed(1) + '%' },
          { label: 'Avg Confidence', value: (businessMetrics.avg_decision_confidence * 100).toFixed(1) + '%' },
        ].map((m, i) => renderMetricCard(m, i))}
      </div>

      {/* Detailed Analytics */}
      <div className="bg-white p-6 rounded-lg border border-gray-200">
        <h3 className="text-lg font-semibold mb-4">Decision Quality Metrics</h3>
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
          <div className="text-center">
            <div className="text-2xl font-bold text-blue-600">{businessMetrics?.rule_effectiveness.toFixed(1) || 0}%</div>
            <div className="text-sm text-gray-600 mt-1">Rule Effectiveness</div>
          </div>
          <div className="text-center">
            <div className="text-2xl font-bold text-green-600">{businessMetrics?.false_positive_rate.toFixed(2) || 0}%</div>
            <div className="text-sm text-gray-600 mt-1">False Positive Rate</div>
          </div>
          <div className="text-center">
            <div className="text-2xl font-bold text-purple-600">{businessMetrics?.ensemble_vs_single_win_rate.toFixed(1) || 0}%</div>
            <div className="text-sm text-gray-600 mt-1">Ensemble Win Rate</div>
          </div>
          <div className="text-center">
            <div className="text-2xl font-bold text-orange-600">{businessMetrics?.false_negative_rate.toFixed(2) || 0}%</div>
            <div className="text-sm text-gray-600 mt-1">False Negative Rate</div>
          </div>
        </div>
      </div>
    </div>
  );

  const renderTechnicalMetrics = () => (
    <div className="space-y-6">
      {/* Key Metrics */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
        {technicalMetrics && [
          { label: 'Avg Latency', value: technicalMetrics.avg_latency_ms.toFixed(0) + 'ms', status: technicalMetrics.avg_latency_ms < 100 ? 'good' : 'warning' },
          { label: 'P99 Latency', value: technicalMetrics.p99_latency_ms.toFixed(0) + 'ms' },
          { label: 'Throughput', value: technicalMetrics.throughput_requests_per_second + ' req/s' },
          { label: 'Error Rate', value: technicalMetrics.error_rate.toFixed(2) + '%', status: technicalMetrics.error_rate < 0.5 ? 'good' : 'critical' },
        ].map((m, i) => renderMetricCard(m, i))}
      </div>

      {/* Percentiles */}
      <div className="bg-white p-6 rounded-lg border border-gray-200">
        <h3 className="text-lg font-semibold mb-4">Latency Percentiles</h3>
        <div className="grid grid-cols-3 gap-4">
          <div className="text-center p-4 bg-blue-50 rounded">
            <div className="text-xl font-bold text-blue-600">{technicalMetrics?.p50_latency_ms.toFixed(1) || 0}ms</div>
            <div className="text-sm text-gray-600">P50</div>
          </div>
          <div className="text-center p-4 bg-yellow-50 rounded">
            <div className="text-xl font-bold text-yellow-600">{technicalMetrics?.p95_latency_ms.toFixed(1) || 0}ms</div>
            <div className="text-sm text-gray-600">P95</div>
          </div>
          <div className="text-center p-4 bg-red-50 rounded">
            <div className="text-xl font-bold text-red-600">{technicalMetrics?.p99_latency_ms.toFixed(1) || 0}ms</div>
            <div className="text-sm text-gray-600">P99</div>
          </div>
        </div>
      </div>

      {/* Performance Recommendations */}
      {perfRecs.length > 0 && (
        <div className="bg-white p-6 rounded-lg border border-gray-200">
          <h3 className="text-lg font-semibold mb-4 flex items-center">
            <span className="w-3 h-3 bg-orange-500 rounded-full mr-2"></span>
            Performance Optimization Recommendations
          </h3>
          <div className="space-y-3">
            {perfRecs.map((rec, i) => (
              <div key={i} className="p-4 bg-orange-50 rounded border border-orange-200">
                <div className="font-medium text-orange-900">{rec.title}</div>
                <div className="text-sm text-orange-700 mt-1">{rec.description}</div>
                <div className={`inline-block mt-2 px-2 py-1 text-xs font-medium rounded ${
                  rec.priority === 'CRITICAL' ? 'bg-red-200 text-red-800' : 
                  rec.priority === 'HIGH' ? 'bg-orange-200 text-orange-800' :
                  'bg-yellow-200 text-yellow-800'
                }`}>
                  {rec.priority}
                </div>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );

  const renderCostMetrics = () => (
    <div className="space-y-6">
      {/* Cost Summary */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
        {costMetrics && [
          { label: 'Monthly Cost', value: '$' + costMetrics.total_monthly_cost.toFixed(2) },
          { label: 'Compute/Hour', value: '$' + costMetrics.compute_cost_per_hour.toFixed(2) },
          { label: 'Storage/Month', value: '$' + costMetrics.storage_cost_per_month.toFixed(2) },
          { label: 'API Calls/Month', value: (costMetrics.api_calls_made / 1000).toFixed(0) + 'K' },
        ].map((m, i) => renderMetricCard(m, i))}
      </div>

      {/* Cost Breakdown */}
      <div className="bg-white p-6 rounded-lg border border-gray-200">
        <h3 className="text-lg font-semibold mb-4">Cost Breakdown</h3>
        <div className="grid grid-cols-3 gap-4">
          <div className="text-center p-4 bg-blue-50 rounded">
            <div className="text-lg font-bold text-blue-600">${costMetrics?.compute_cost_per_hour.toFixed(0) || 0}/hr</div>
            <div className="text-sm text-gray-600 mt-1">Compute</div>
            <div className="text-xs text-gray-500 mt-1">{((costMetrics?.compute_cost_per_hour / (costMetrics?.total_monthly_cost || 1)) * 100).toFixed(0)}% of total</div>
          </div>
          <div className="text-center p-4 bg-green-50 rounded">
            <div className="text-lg font-bold text-green-600">${costMetrics?.storage_cost_per_month.toFixed(0) || 0}/mo</div>
            <div className="text-sm text-gray-600 mt-1">Storage</div>
            <div className="text-xs text-gray-500 mt-1">{costMetrics?.storage_gb_used || 0} GB</div>
          </div>
          <div className="text-center p-4 bg-purple-50 rounded">
            <div className="text-lg font-bold text-purple-600">${costMetrics?.api_call_cost.toFixed(0) || 0}</div>
            <div className="text-sm text-gray-600 mt-1">API Calls</div>
            <div className="text-xs text-gray-500 mt-1">{costMetrics?.api_calls_made || 0} calls</div>
          </div>
        </div>
      </div>

      {/* Cost Optimizations */}
      {costRecs.length > 0 && (
        <div className="bg-white p-6 rounded-lg border border-gray-200">
          <h3 className="text-lg font-semibold mb-4 flex items-center">
            <span className="w-3 h-3 bg-green-500 rounded-full mr-2"></span>
            Cost Optimization Opportunities
          </h3>
          <div className="space-y-3">
            {costRecs.map((rec, i) => (
              <div key={i} className="p-4 bg-green-50 rounded border border-green-200">
                <div className="font-medium text-green-900">{rec.title}</div>
                <div className="text-sm text-green-700 mt-1">{rec.description}</div>
                <div className="text-sm font-semibold text-green-800 mt-2">
                  Potential Savings: ${rec.estimated_savings?.toFixed(0) || 0}/month
                </div>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );

  return (
    <div className="min-h-screen bg-gray-100 p-6">
      <div className="max-w-7xl mx-auto">
        {/* Header */}
        <div className="mb-8">
          <h1 className="text-3xl font-bold text-gray-900">Metrics Dashboard</h1>
          <p className="text-gray-600 mt-2">Business, Technical, and Cost metrics monitoring</p>
        </div>

        {/* Tabs */}
        <div className="bg-white rounded-lg shadow mb-6 border-b">
          <div className="flex">
            {[
              { id: 'business', label: 'Business Metrics', icon: '📊' },
              { id: 'technical', label: 'Technical Metrics', icon: '⚡' },
              { id: 'cost', label: 'Cost Metrics', icon: '💰' },
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
                <p className="text-gray-600">Loading metrics...</p>
              </div>
            </div>
          ) : error ? (
            <div className="text-red-600 p-4 bg-red-50 rounded">{error}</div>
          ) : (
            <>
              {activeTab === 'business' && renderBusinessMetrics()}
              {activeTab === 'technical' && renderTechnicalMetrics()}
              {activeTab === 'cost' && renderCostMetrics()}
            </>
          )}
        </div>

        {/* Footer */}
        <div className="mt-4 text-sm text-gray-600 text-center">
          Last updated: {new Date().toLocaleTimeString()}
        </div>
      </div>
    </div>
  );
};

export default MetricsDashboard;
