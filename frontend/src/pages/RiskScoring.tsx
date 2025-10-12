import React from 'react';
import { AlertTriangle, TrendingUp, Brain } from 'lucide-react';
import { useRiskPredictions, useMLModels, useRiskDashboardStats } from '@/hooks/useRiskScoring';
import { LoadingSpinner } from '@/components/LoadingSpinner';

const RiskScoring: React.FC = () => {
  const { data: predictions = [], isLoading: predLoading } = useRiskPredictions();
  const { data: models = [] } = useMLModels();
  const { data: stats, isLoading: statsLoading } = useRiskDashboardStats();

  if (predLoading || statsLoading) {
    return <LoadingSpinner fullScreen message="Loading risk predictions..." />;
  }

  const getRiskLevelColor = (level: string) => {
    switch (level) {
      case 'critical': return 'text-red-600 bg-red-100';
      case 'high': return 'text-orange-600 bg-orange-100';
      case 'medium': return 'text-yellow-600 bg-yellow-100';
      case 'low': return 'text-green-600 bg-green-100';
      default: return 'text-gray-600 bg-gray-100';
    }
  };

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold text-gray-900">Predictive Compliance Risk Scoring</h1>
        <p className="text-gray-600 mt-1">ML-powered risk predictions and analysis</p>
      </div>

      {/* Statistics */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-6">
        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-3 mb-2">
            <div className="p-2 bg-blue-100 rounded-lg">
              <Brain className="w-5 h-5 text-blue-600" />
            </div>
            <h3 className="text-sm font-medium text-gray-600">Total Predictions</h3>
          </div>
          <p className="text-2xl font-bold text-gray-900">{stats?.total_predictions || 0}</p>
        </div>

        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-3 mb-2">
            <div className="p-2 bg-red-100 rounded-lg">
              <AlertTriangle className="w-5 h-5 text-red-600" />
            </div>
            <h3 className="text-sm font-medium text-gray-600">Critical Risks</h3>
          </div>
          <p className="text-2xl font-bold text-gray-900">{stats?.critical_risks || 0}</p>
        </div>

        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-3 mb-2">
            <div className="p-2 bg-orange-100 rounded-lg">
              <AlertTriangle className="w-5 h-5 text-orange-600" />
            </div>
            <h3 className="text-sm font-medium text-gray-600">High Risks</h3>
          </div>
          <p className="text-2xl font-bold text-gray-900">{stats?.high_risks || 0}</p>
        </div>

        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-3 mb-2">
            <div className="p-2 bg-purple-100 rounded-lg">
              <TrendingUp className="w-5 h-5 text-purple-600" />
            </div>
            <h3 className="text-sm font-medium text-gray-600">Avg Risk Score</h3>
          </div>
          <p className="text-2xl font-bold text-gray-900">
            {stats?.avg_risk_score ? `${(stats.avg_risk_score * 100).toFixed(1)}%` : 'N/A'}
          </p>
        </div>
      </div>

      {/* ML Models */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
        <h2 className="text-lg font-semibold text-gray-900 mb-4">Active ML Models</h2>
        <div className="space-y-3">
          {models.map((model) => (
            <div key={model.model_id} className="flex items-center justify-between p-3 bg-gray-50 rounded-lg">
              <div>
                <p className="font-medium text-gray-900">{model.model_name}</p>
                <p className="text-sm text-gray-600">{model.model_type} - v{model.model_version}</p>
              </div>
              <div className="text-right">
                <p className="text-sm font-medium text-gray-900">
                  Accuracy: {model.accuracy_score ? `${(model.accuracy_score * 100).toFixed(1)}%` : 'N/A'}
                </p>
                <span className={`text-xs px-2 py-1 rounded-full ${model.is_active ? 'bg-green-100 text-green-800' : 'bg-gray-100 text-gray-800'}`}>
                  {model.is_active ? 'Active' : 'Inactive'}
                </span>
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Recent Predictions */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200">
        <div className="p-6 border-b border-gray-200">
          <h2 className="text-lg font-semibold text-gray-900">Recent Risk Predictions</h2>
        </div>
        <div className="overflow-x-auto">
          <table className="w-full">
            <thead className="bg-gray-50">
              <tr>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Entity</th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Risk Score</th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Risk Level</th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Confidence</th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Predicted At</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-gray-200">
              {predictions.length === 0 ? (
                <tr>
                  <td colSpan={5} className="px-6 py-12 text-center text-gray-500">
                    No predictions available yet
                  </td>
                </tr>
              ) : (
                predictions.map((pred) => (
                  <tr key={pred.prediction_id} className="hover:bg-gray-50">
                    <td className="px-6 py-4 whitespace-nowrap">
                      <div className="text-sm font-medium text-gray-900">{pred.entity_type}</div>
                      <div className="text-sm text-gray-500">{pred.entity_id}</div>
                    </td>
                    <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">
                      {(pred.risk_score * 100).toFixed(1)}%
                    </td>
                    <td className="px-6 py-4 whitespace-nowrap">
                      <span className={`px-2 py-1 text-xs font-medium rounded-full ${getRiskLevelColor(pred.risk_level)}`}>
                        {pred.risk_level}
                      </span>
                    </td>
                    <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">
                      {(pred.confidence_score * 100).toFixed(1)}%
                    </td>
                    <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                      {new Date(pred.predicted_at).toLocaleString()}
                    </td>
                  </tr>
                ))
              )}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );
};

export default RiskScoring;

