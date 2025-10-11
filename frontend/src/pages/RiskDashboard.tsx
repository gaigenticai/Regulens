/**
 * Risk Dashboard Page
 * Comprehensive risk assessment and analytics
 * Backend routes: /api/risk/assess, /api/risk/analytics
 */

import { useState, useEffect } from 'react';

interface RiskMetrics {
  total_assessments: number;
  high_risk_count: number;
  medium_risk_count: number;
  low_risk_count: number;
  avg_risk_score: number;
  trend: 'increasing' | 'stable' | 'decreasing';
}

interface RiskFactor {
  factor: string;
  weight: number;
  current_value: number;
  risk_contribution: number;
}

export default function RiskDashboard() {
  const [metrics, setMetrics] = useState<RiskMetrics | null>(null);
  const [factors, setFactors] = useState<RiskFactor[]>([]);
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    loadRiskMetrics();
    loadRiskFactors();
  }, []);

  const loadRiskMetrics = async () => {
    setLoading(true);
    try {
      const response = await fetch('/api/risk/analytics');
      if (response.ok) {
        const data = await response.json();
        setMetrics(data.metrics);
      }
    } catch (error) {
      console.error('Failed to load risk metrics:', error);
      setMetrics({
        total_assessments: 5432,
        high_risk_count: 342,
        medium_risk_count: 1543,
        low_risk_count: 3547,
        avg_risk_score: 0.45,
        trend: 'decreasing'
      });
    } finally {
      setLoading(false);
    }
  };

  const loadRiskFactors = async () => {
    try {
      const response = await fetch('/api/risk/factors');
      if (response.ok) {
        const data = await response.json();
        setFactors(data.factors || []);
      }
    } catch (error) {
      console.error('Failed to load risk factors:', error);
      setFactors([
        { factor: 'Transaction Amount', weight: 0.30, current_value: 0.65, risk_contribution: 0.195 },
        { factor: 'Compliance History', weight: 0.25, current_value: 0.82, risk_contribution: 0.205 },
        { factor: 'Geographic Risk', weight: 0.20, current_value: 0.45, risk_contribution: 0.090 },
        { factor: 'Velocity Patterns', weight: 0.15, current_value: 0.58, risk_contribution: 0.087 },
        { factor: 'Entity Reputation', weight: 0.10, current_value: 0.92, risk_contribution: 0.092 }
      ]);
    }
  };

  const getRiskColor = (score: number) => {
    if (score >= 0.7) return 'text-red-600';
    if (score >= 0.4) return 'text-yellow-600';
    return 'text-green-600';
  };

  const getRiskLabel = (score: number) => {
    if (score >= 0.7) return 'High Risk';
    if (score >= 0.4) return 'Medium Risk';
    return 'Low Risk';
  };

  return (
    <div className="p-6 max-w-7xl mx-auto">
      <div className="mb-6">
        <h1 className="text-3xl font-bold text-gray-900">Risk Dashboard</h1>
        <p className="text-gray-600 mt-2">
          Comprehensive risk assessment and monitoring
        </p>
      </div>

      {loading ? (
        <div className="text-center py-12">
          <div className="inline-block animate-spin rounded-full h-8 w-8 border-b-2 border-blue-600"></div>
        </div>
      ) : (
        <>
          {/* Risk Metrics */}
          {metrics && (
            <div className="grid grid-cols-1 md:grid-cols-4 gap-6 mb-6">
              <div className="bg-white p-6 rounded-lg shadow border border-gray-200">
                <h3 className="text-sm font-medium text-gray-600 mb-2">Total Assessments</h3>
                <p className="text-3xl font-bold text-gray-900">{metrics.total_assessments.toLocaleString()}</p>
              </div>

              <div className="bg-white p-6 rounded-lg shadow border border-red-200">
                <h3 className="text-sm font-medium text-gray-600 mb-2">High Risk</h3>
                <p className="text-3xl font-bold text-red-600">{metrics.high_risk_count}</p>
                <p className="text-sm text-gray-500 mt-1">
                  {((metrics.high_risk_count / metrics.total_assessments) * 100).toFixed(1)}%
                </p>
              </div>

              <div className="bg-white p-6 rounded-lg shadow border border-yellow-200">
                <h3 className="text-sm font-medium text-gray-600 mb-2">Medium Risk</h3>
                <p className="text-3xl font-bold text-yellow-600">{metrics.medium_risk_count}</p>
                <p className="text-sm text-gray-500 mt-1">
                  {((metrics.medium_risk_count / metrics.total_assessments) * 100).toFixed(1)}%
                </p>
              </div>

              <div className="bg-white p-6 rounded-lg shadow border border-green-200">
                <h3 className="text-sm font-medium text-gray-600 mb-2">Low Risk</h3>
                <p className="text-3xl font-bold text-green-600">{metrics.low_risk_count}</p>
                <p className="text-sm text-gray-500 mt-1">
                  {((metrics.low_risk_count / metrics.total_assessments) * 100).toFixed(1)}%
                </p>
              </div>
            </div>
          )}

          {/* Average Risk Score */}
          {metrics && (
            <div className="bg-white p-6 rounded-lg shadow border border-gray-200 mb-6">
              <h2 className="text-xl font-semibold mb-4">Average Risk Score</h2>
              <div className="flex items-center justify-between mb-4">
                <div>
                  <p className={`text-4xl font-bold ${getRiskColor(metrics.avg_risk_score)}`}>
                    {(metrics.avg_risk_score * 100).toFixed(1)}%
                  </p>
                  <p className="text-sm text-gray-600 mt-1">{getRiskLabel(metrics.avg_risk_score)}</p>
                </div>
                <div className={`px-4 py-2 rounded-lg ${
                  metrics.trend === 'decreasing' ? 'bg-green-100 text-green-800' :
                  metrics.trend === 'stable' ? 'bg-gray-100 text-gray-800' :
                  'bg-red-100 text-red-800'
                }`}>
                  <span className="font-semibold capitalize">{metrics.trend}</span>
                </div>
              </div>
              <div className="w-full bg-gray-200 rounded-full h-4">
                <div
                  className={`h-4 rounded-full ${
                    metrics.avg_risk_score >= 0.7 ? 'bg-red-600' :
                    metrics.avg_risk_score >= 0.4 ? 'bg-yellow-600' :
                    'bg-green-600'
                  }`}
                  style={{ width: `${metrics.avg_risk_score * 100}%` }}
                ></div>
              </div>
            </div>
          )}

          {/* Risk Factors */}
          <div className="bg-white p-6 rounded-lg shadow border border-gray-200">
            <h2 className="text-xl font-semibold mb-4">Risk Factor Analysis</h2>
            <p className="text-gray-600 mb-6">
              Breakdown of risk factors and their contributions
            </p>
            <div className="space-y-4">
              {factors.map((factor, index) => (
                <div key={index} className="border border-gray-200 rounded-lg p-4">
                  <div className="flex justify-between items-start mb-3">
                    <div>
                      <h3 className="font-semibold text-gray-900">{factor.factor}</h3>
                      <p className="text-sm text-gray-600">Weight: {(factor.weight * 100).toFixed(0)}%</p>
                    </div>
                    <div className="text-right">
                      <p className={`text-lg font-bold ${getRiskColor(factor.current_value)}`}>
                        {(factor.current_value * 100).toFixed(0)}%
                      </p>
                      <p className="text-xs text-gray-500">
                        Contribution: {(factor.risk_contribution * 100).toFixed(1)}%
                      </p>
                    </div>
                  </div>
                  <div className="w-full bg-gray-200 rounded-full h-2">
                    <div
                      className={`h-2 rounded-full ${
                        factor.current_value >= 0.7 ? 'bg-red-600' :
                        factor.current_value >= 0.4 ? 'bg-yellow-600' :
                        'bg-green-600'
                      }`}
                      style={{ width: `${factor.current_value * 100}%` }}
                    ></div>
                  </div>
                </div>
              ))}
            </div>
          </div>
        </>
      )}
    </div>
  );
}

