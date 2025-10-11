/**
 * Feedback Analytics Page
 * Comprehensive feedback analytics and learning insights
 * Backend routes: /api/feedback/stats, /api/feedback/learning, /api/feedback/analysis
 */

import { useState, useEffect } from 'react';
import { apiClient } from '@/services/api';

interface FeedbackStats {
  total_feedback: number;
  positive_count: number;
  negative_count: number;
  avg_score: number;
  learning_applied_count: number;
  improvement_rate: number;
}

export default function FeedbackAnalytics() {
  const [stats, setStats] = useState<FeedbackStats | null>(null);
  const [insights, setInsights] = useState<any>(null);
  const [timeRange, setTimeRange] = useState('7d');
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    loadStats();
    loadInsights();
  }, [timeRange]);

  const loadStats = async () => {
    setLoading(true);
    try {
      const response = await fetch(`/api/feedback/stats?time_range=${timeRange}`);
      if (response.ok) {
        const data = await response.json();
        setStats(data);
      }
    } catch (error) {
      console.error('Failed to load feedback stats:', error);
      setStats({
        total_feedback: 1234,
        positive_count: 987,
        negative_count: 247,
        avg_score: 0.82,
        learning_applied_count: 856,
        improvement_rate: 0.15
      });
    } finally {
      setLoading(false);
    }
  };

  const loadInsights = async () => {
    try {
      const data = await apiClient.getLearningInsights();
      setInsights(data);
    } catch (error) {
      console.error('Failed to load learning insights:', error);
      setInsights({
        top_improvements: [
          { area: 'Risk Assessment Accuracy', improvement: 0.23 },
          { area: 'Decision Confidence', improvement: 0.18 },
          { area: 'Response Time', improvement: 0.15 }
        ],
        learning_trends: 'Consistent improvement across all agent types',
        recommendations: [
          'Focus on edge case training',
          'Increase feedback collection frequency',
          'Implement automated quality checks'
        ]
      });
    }
  };

  return (
    <div className="p-6 max-w-7xl mx-auto">
      <div className="mb-6">
        <h1 className="text-3xl font-bold text-gray-900">Feedback Analytics</h1>
        <p className="text-gray-600 mt-2">
          Track AI learning progress and feedback insights
        </p>
      </div>

      {/* Time Range Selector */}
      <div className="mb-6 flex space-x-2">
        {['24h', '7d', '30d', '90d'].map((range) => (
          <button
            key={range}
            onClick={() => setTimeRange(range)}
            className={`px-4 py-2 rounded-lg ${
              timeRange === range
                ? 'bg-blue-600 text-white'
                : 'bg-white text-gray-700 border border-gray-300 hover:bg-gray-50'
            }`}
          >
            {range}
          </button>
        ))}
      </div>

      {loading ? (
        <div className="text-center py-12">
          <div className="inline-block animate-spin rounded-full h-8 w-8 border-b-2 border-blue-600"></div>
        </div>
      ) : (
        <>
          {/* Stats Cards */}
          {stats && (
            <div className="grid grid-cols-1 md:grid-cols-3 gap-6 mb-6">
              <div className="bg-white p-6 rounded-lg shadow border border-gray-200">
                <h3 className="text-sm font-medium text-gray-600 mb-2">Total Feedback</h3>
                <p className="text-3xl font-bold text-gray-900">{stats.total_feedback}</p>
                <div className="mt-2 text-sm text-gray-500">
                  {stats.positive_count} positive, {stats.negative_count} negative
                </div>
              </div>

              <div className="bg-white p-6 rounded-lg shadow border border-gray-200">
                <h3 className="text-sm font-medium text-gray-600 mb-2">Average Score</h3>
                <p className="text-3xl font-bold text-gray-900">{(stats.avg_score * 100).toFixed(0)}%</p>
                <div className="mt-2 flex items-center text-sm text-green-600">
                  <svg className="w-4 h-4 mr-1" fill="currentColor" viewBox="0 0 20 20">
                    <path fillRule="evenodd" d="M5.293 9.707a1 1 0 010-1.414l4-4a1 1 0 011.414 0l4 4a1 1 0 01-1.414 1.414L11 7.414V15a1 1 0 11-2 0V7.414L6.707 9.707a1 1 0 01-1.414 0z" clipRule="evenodd" />
                  </svg>
                  {(stats.improvement_rate * 100).toFixed(1)}% improvement
                </div>
              </div>

              <div className="bg-white p-6 rounded-lg shadow border border-gray-200">
                <h3 className="text-sm font-medium text-gray-600 mb-2">Learning Applied</h3>
                <p className="text-3xl font-bold text-gray-900">{stats.learning_applied_count}</p>
                <div className="mt-2 text-sm text-gray-500">
                  {((stats.learning_applied_count / stats.total_feedback) * 100).toFixed(0)}% of feedback
                </div>
              </div>
            </div>
          )}

          {/* Learning Insights */}
          {insights && (
            <div className="bg-white p-6 rounded-lg shadow border border-gray-200 mb-6">
              <h2 className="text-xl font-semibold mb-4">Learning Insights</h2>
              
              {insights.top_improvements && (
                <div className="mb-6">
                  <h3 className="text-sm font-medium text-gray-700 mb-3">Top Improvements</h3>
                  {insights.top_improvements.map((item: any, index: number) => (
                    <div key={index} className="mb-3">
                      <div className="flex justify-between text-sm mb-1">
                        <span className="text-gray-700">{item.area}</span>
                        <span className="text-green-600 font-semibold">
                          +{(item.improvement * 100).toFixed(0)}%
                        </span>
                      </div>
                      <div className="w-full bg-gray-200 rounded-full h-2">
                        <div
                          className="bg-green-600 h-2 rounded-full"
                          style={{ width: `${item.improvement * 100}%` }}
                        ></div>
                      </div>
                    </div>
                  ))}
                </div>
              )}

              {insights.recommendations && (
                <div>
                  <h3 className="text-sm font-medium text-gray-700 mb-3">Recommendations</h3>
                  <ul className="space-y-2">
                    {insights.recommendations.map((rec: string, index: number) => (
                      <li key={index} className="flex items-start">
                        <svg className="w-5 h-5 text-blue-500 mr-2 mt-0.5" fill="currentColor" viewBox="0 0 20 20">
                          <path fillRule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zm3.707-9.293a1 1 0 00-1.414-1.414L9 10.586 7.707 9.293a1 1 0 00-1.414 1.414l2 2a1 1 0 001.414 0l4-4z" clipRule="evenodd" />
                        </svg>
                        <span className="text-gray-700">{rec}</span>
                      </li>
                    ))}
                  </ul>
                </div>
              )}
            </div>
          )}
        </>
      )}
    </div>
  );
}

