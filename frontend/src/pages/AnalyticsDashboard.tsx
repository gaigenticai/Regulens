import React from 'react';
import { BarChart3, Lightbulb, TrendingUp, Eye } from 'lucide-react';
import { useBIDashboards, useAnalyticsMetrics, useDataInsights, useAnalyticsStats } from '@/hooks/useAnalytics';
import { LoadingSpinner } from '@/components/LoadingSpinner';

const AnalyticsDashboard: React.FC = () => {
  const { data: dashboards = [], isLoading: dashLoading } = useBIDashboards();
  const { data: metrics = [] } = useAnalyticsMetrics();
  const { data: insights = [] } = useDataInsights();
  const { data: stats, isLoading: statsLoading } = useAnalyticsStats();

  if (dashLoading || statsLoading) {
    return <LoadingSpinner fullScreen message="Loading analytics..." />;
  }

  const getPriorityColor = (priority: string) => {
    switch (priority) {
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
        <h1 className="text-2xl font-bold text-gray-900">Advanced Analytics & BI Dashboard</h1>
        <p className="text-gray-600 mt-1">Executive insights and data storytelling</p>
      </div>

      {/* Statistics */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-3 mb-2">
            <div className="p-2 bg-blue-100 rounded-lg">
              <BarChart3 className="w-5 h-5 text-blue-600" />
            </div>
            <h3 className="text-sm font-medium text-gray-600">BI Dashboards</h3>
          </div>
          <p className="text-2xl font-bold text-gray-900">{stats?.total_dashboards || 0}</p>
          <p className="text-xs text-gray-500 mt-1">Total available</p>
        </div>

        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-3 mb-2">
            <div className="p-2 bg-purple-100 rounded-lg">
              <TrendingUp className="w-5 h-5 text-purple-600" />
            </div>
            <h3 className="text-sm font-medium text-gray-600">Recent Metrics</h3>
          </div>
          <p className="text-2xl font-bold text-gray-900">{stats?.recent_metrics || 0}</p>
          <p className="text-xs text-gray-500 mt-1">Last 24 hours</p>
        </div>

        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-3 mb-2">
            <div className="p-2 bg-green-100 rounded-lg">
              <Lightbulb className="w-5 h-5 text-green-600" />
            </div>
            <h3 className="text-sm font-medium text-gray-600">Active Insights</h3>
          </div>
          <p className="text-2xl font-bold text-gray-900">{stats?.active_insights || 0}</p>
          <p className="text-xs text-gray-500 mt-1">Automated discoveries</p>
        </div>
      </div>

      {/* Data Insights */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200">
        <div className="p-6 border-b border-gray-200">
          <h2 className="text-lg font-semibold text-gray-900">Automated Data Insights</h2>
          <p className="text-sm text-gray-600 mt-1">AI-powered discoveries and recommendations</p>
        </div>
        <div className="divide-y divide-gray-200">
          {insights.length === 0 ? (
            <div className="p-12 text-center">
              <Lightbulb className="w-12 h-12 text-gray-400 mx-auto mb-4" />
              <h3 className="text-lg font-medium text-gray-900 mb-2">No insights yet</h3>
              <p className="text-gray-600">Automated insights will appear as data is analyzed</p>
            </div>
          ) : (
            insights.map((insight) => (
              <div key={insight.insight_id} className="p-6 hover:bg-gray-50">
                <div className="flex items-start justify-between">
                  <div className="flex-1">
                    <div className="flex items-center gap-3 mb-2">
                      <h3 className="text-lg font-semibold text-gray-900">{insight.title}</h3>
                      <span className={`px-2 py-1 text-xs font-medium rounded-full ${getPriorityColor(insight.priority)}`}>
                        {insight.priority}
                      </span>
                      <span className="px-2 py-1 text-xs font-medium rounded-full bg-blue-100 text-blue-800">
                        {insight.insight_type}
                      </span>
                    </div>
                    <p className="text-gray-600 text-sm mb-2">{insight.description}</p>
                    <div className="flex items-center gap-4 text-xs text-gray-500">
                      <span>Confidence: {(insight.confidence_score * 100).toFixed(1)}%</span>
                      <span>Discovered: {new Date(insight.discovered_at).toLocaleString()}</span>
                    </div>
                  </div>
                </div>
              </div>
            ))
          )}
        </div>
      </div>

      {/* BI Dashboards */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200">
        <div className="p-6 border-b border-gray-200">
          <h2 className="text-lg font-semibold text-gray-900">Business Intelligence Dashboards</h2>
          <p className="text-sm text-gray-600 mt-1">Customizable analytics dashboards</p>
        </div>
        <div className="grid grid-cols-1 md:grid-cols-2 gap-6 p-6">
          {dashboards.length === 0 ? (
            <div className="col-span-2 p-12 text-center">
              <BarChart3 className="w-12 h-12 text-gray-400 mx-auto mb-4" />
              <h3 className="text-lg font-medium text-gray-900 mb-2">No dashboards configured</h3>
              <p className="text-gray-600">Create your first BI dashboard to visualize data</p>
            </div>
          ) : (
            dashboards.map((dashboard) => (
              <div key={dashboard.dashboard_id} className="p-6 border border-gray-200 rounded-lg hover:border-blue-500 cursor-pointer">
                <div className="flex items-start justify-between mb-3">
                  <div>
                    <h3 className="text-lg font-semibold text-gray-900">{dashboard.dashboard_name}</h3>
                    <p className="text-sm text-gray-600 mt-1">{dashboard.description}</p>
                  </div>
                  <span className="px-2 py-1 text-xs font-medium rounded-full bg-purple-100 text-purple-800">
                    {dashboard.dashboard_type}
                  </span>
                </div>
                <div className="flex items-center gap-4 text-xs text-gray-500">
                  <div className="flex items-center gap-1">
                    <Eye className="w-4 h-4" />
                    <span>{dashboard.view_count} views</span>
                  </div>
                  <span>Created: {new Date(dashboard.created_at).toLocaleDateString()}</span>
                </div>
              </div>
            ))
          )}
        </div>
      </div>

      {/* Recent Metrics */}
      {metrics.length > 0 && (
        <div className="bg-white rounded-lg shadow-sm border border-gray-200">
          <div className="p-6 border-b border-gray-200">
            <h2 className="text-lg font-semibold text-gray-900">Recent Metrics (24h)</h2>
          </div>
          <div className="overflow-x-auto">
            <table className="w-full">
              <thead className="bg-gray-50">
                <tr>
                  <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Metric</th>
                  <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Category</th>
                  <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Value</th>
                  <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Period</th>
                  <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Time</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-gray-200">
                {metrics.slice(0, 10).map((metric, idx) => (
                  <tr key={idx} className="hover:bg-gray-50">
                    <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">
                      {metric.metric_name}
                    </td>
                    <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-600">
                      {metric.metric_category}
                    </td>
                    <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">
                      {metric.metric_value} {metric.metric_unit}
                    </td>
                    <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-600">
                      {metric.aggregation_period}
                    </td>
                    <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                      {new Date(metric.calculated_at).toLocaleTimeString()}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}
    </div>
  );
};

export default AnalyticsDashboard;

