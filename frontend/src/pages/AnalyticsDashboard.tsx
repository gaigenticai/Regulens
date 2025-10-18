/**
 * Analytics Dashboard - Phase 7A
 * Real-time decision, rule, and learning analytics
 */

import React, { useState, useEffect } from 'react';
import { analyticsAPI } from '../services/api';
import {
  LineChart, Line, BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer,
  PieChart, Pie, Cell
} from 'recharts';

interface AnalyticsStat {
  label: string;
  value: number | string;
  color: string;
}

interface AlgorithmPerformance {
  algorithm: string;
  accuracy_rate: number;
  total_decisions: number;
}

export const AnalyticsDashboard: React.FC = () => {
  const [activeTab, setActiveTab] = useState<'decision' | 'rule' | 'learning'>('decision');
  const [decisionStats, setDecisionStats] = useState<any>(null);
  const [ruleStats, setRuleStats] = useState<any>(null);
  const [learningStats, setLearningStats] = useState<any>(null);
  const [algorithmComparison, setAlgorithmComparison] = useState<AlgorithmPerformance[]>([]);
  const [ensembleAnalysis, setEnsembleAnalysis] = useState<any>(null);
  const [highFPRules, setHighFPRules] = useState<any[]>([]);
  const [featureImportance, setFeatureImportance] = useState<any[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    loadAnalytics();
  }, [activeTab]);

  const loadAnalytics = async () => {
    setLoading(true);
    setError(null);
    try {
      if (activeTab === 'decision') {
        const [stats, ensemble, algComparison] = await Promise.all([
          analyticsAPI.getDecisionStats(30),
          analyticsAPI.getEnsembleComparison(30),
          analyticsAPI.getAlgorithmComparison(['AHP', 'TOPSIS', 'PROMETHEE', 'ELECTRE'], 30),
        ]);
        setDecisionStats(stats);
        setEnsembleAnalysis(ensemble);
        setAlgorithmComparison(algComparison);
      } else if (activeTab === 'rule') {
        const [stats, fpRules] = await Promise.all([
          analyticsAPI.getRuleStats(30),
          analyticsAPI.getHighFalsePositiveRules(10),
        ]);
        setRuleStats(stats);
        setHighFPRules(fpRules);
      } else if (activeTab === 'learning') {
        const [stats, features, recs] = await Promise.all([
          analyticsAPI.getLearningStats(30),
          analyticsAPI.getFeatureImportance(20),
          analyticsAPI.getLearningRecommendations(),
        ]);
        setLearningStats(stats);
        setFeatureImportance(features);
      }
    } catch (err) {
      setError('Failed to load analytics data');
      console.error(err);
    } finally {
      setLoading(false);
    }
  };

  const renderDecisionAnalytics = () => (
    <div className="space-y-6">
      {/* Key Metrics */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
        {decisionStats && [
          { label: 'Total Decisions', value: decisionStats.total_decisions, color: 'bg-blue-500' },
          { label: 'Overall Accuracy', value: `${(decisionStats.overall_accuracy * 100).toFixed(1)}%`, color: 'bg-green-500' },
          { label: 'Avg Confidence', value: (decisionStats.avg_confidence * 100).toFixed(1) + '%', color: 'bg-purple-500' },
          { label: 'Best Algorithm', value: decisionStats.best_algorithm, color: 'bg-orange-500' },
        ].map((stat, i) => (
          <div key={i} className={`${stat.color} text-white p-4 rounded-lg`}>
            <div className="text-sm font-medium opacity-90">{stat.label}</div>
            <div className="text-2xl font-bold mt-2">{stat.value}</div>
          </div>
        ))}
      </div>

      {/* Algorithm Comparison Chart */}
      {algorithmComparison.length > 0 && (
        <div className="bg-white p-6 rounded-lg shadow">
          <h3 className="text-lg font-semibold mb-4">Algorithm Performance Comparison</h3>
          <ResponsiveContainer width="100%" height={300}>
            <BarChart data={algorithmComparison}>
              <CartesianGrid strokeDasharray="3 3" />
              <XAxis dataKey="algorithm" />
              <YAxis label={{ value: 'Accuracy Rate', angle: -90, position: 'insideLeft' }} />
              <Tooltip />
              <Bar dataKey="accuracy_rate" fill="#3b82f6" name="Accuracy Rate" />
            </BarChart>
          </ResponsiveContainer>
        </div>
      )}

      {/* Ensemble vs Individual */}
      {ensembleAnalysis && (
        <div className="bg-white p-6 rounded-lg shadow">
          <h3 className="text-lg font-semibold mb-4">Ensemble vs Individual Performance</h3>
          <div className="grid grid-cols-2 gap-4">
            <div className="text-center p-4 bg-blue-50 rounded">
              <div className="text-3xl font-bold text-blue-600">{ensembleAnalysis.ensemble_correct_count}</div>
              <div className="text-sm text-gray-600 mt-2">Ensemble Correct</div>
              <div className="text-xs text-gray-500 mt-1">Win Rate: {(ensembleAnalysis.ensemble_win_rate * 100).toFixed(1)}%</div>
            </div>
            <div className="text-center p-4 bg-green-50 rounded">
              <div className="text-3xl font-bold text-green-600">{ensembleAnalysis.individual_best_correct_count}</div>
              <div className="text-sm text-gray-600 mt-2">Individual Best Correct</div>
              <div className="text-xs text-gray-500 mt-1">Total: {ensembleAnalysis.total_comparisons}</div>
            </div>
          </div>
        </div>
      )}
    </div>
  );

  const renderRuleAnalytics = () => (
    <div className="space-y-6">
      {/* Key Metrics */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
        {ruleStats && [
          { label: 'Total Rules', value: ruleStats.total_rules, color: 'bg-blue-500' },
          { label: 'Avg Precision', value: (ruleStats.avg_precision * 100).toFixed(1) + '%', color: 'bg-green-500' },
          { label: 'Avg F1-Score', value: (ruleStats.avg_f1_score * 100).toFixed(1) + '%', color: 'bg-purple-500' },
          { label: 'Redundant Pairs', value: ruleStats.redundant_rule_pairs, color: 'bg-orange-500' },
        ].map((stat, i) => (
          <div key={i} className={`${stat.color} text-white p-4 rounded-lg`}>
            <div className="text-sm font-medium opacity-90">{stat.label}</div>
            <div className="text-2xl font-bold mt-2">{stat.value}</div>
          </div>
        ))}
      </div>

      {/* High False Positive Rules */}
      {highFPRules.length > 0 && (
        <div className="bg-white p-6 rounded-lg shadow">
          <h3 className="text-lg font-semibold mb-4 flex items-center">
            <span className="w-3 h-3 bg-red-500 rounded-full mr-2"></span>
            Rules with High False Positive Rate
          </h3>
          <div className="overflow-x-auto">
            <table className="w-full">
              <thead className="bg-gray-50">
                <tr>
                  <th className="px-4 py-2 text-left text-sm font-semibold">Rule</th>
                  <th className="px-4 py-2 text-left text-sm font-semibold">FP Rate</th>
                  <th className="px-4 py-2 text-left text-sm font-semibold">Precision</th>
                </tr>
              </thead>
              <tbody>
                {highFPRules.map((rule, i) => (
                  <tr key={i} className="border-t hover:bg-gray-50">
                    <td className="px-4 py-2">{rule.rule_name}</td>
                    <td className="px-4 py-2 text-red-600">{(rule.false_positive_rate * 100).toFixed(1)}%</td>
                    <td className="px-4 py-2">{(rule.precision * 100).toFixed(1)}%</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}
    </div>
  );

  const renderLearningAnalytics = () => (
    <div className="space-y-6">
      {/* Key Metrics */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
        {learningStats && [
          { label: 'Feedback Items', value: learningStats.total_feedback_items, color: 'bg-blue-500' },
          { label: 'Avg Effectiveness', value: (learningStats.avg_feedback_effectiveness).toFixed(1), color: 'bg-green-500' },
          { label: 'Total Reward', value: learningStats.total_cumulative_reward.toFixed(1), color: 'bg-purple-500' },
          { label: 'Converged', value: learningStats.learning_converged ? 'Yes' : 'No', color: learningStats.learning_converged ? 'bg-green-500' : 'bg-yellow-500' },
        ].map((stat, i) => (
          <div key={i} className={`${stat.color} text-white p-4 rounded-lg`}>
            <div className="text-sm font-medium opacity-90">{stat.label}</div>
            <div className="text-2xl font-bold mt-2">{stat.value}</div>
          </div>
        ))}
      </div>

      {/* Feature Importance */}
      {featureImportance.length > 0 && (
        <div className="bg-white p-6 rounded-lg shadow">
          <h3 className="text-lg font-semibold mb-4">Top 20 Feature Importance</h3>
          <ResponsiveContainer width="100%" height={400}>
            <BarChart data={featureImportance}>
              <CartesianGrid strokeDasharray="3 3" />
              <XAxis dataKey="feature_name" angle={-45} textAnchor="end" height={100} />
              <YAxis label={{ value: 'Importance Score', angle: -90, position: 'insideLeft' }} />
              <Tooltip />
              <Bar dataKey="importance_score" fill="#8b5cf6" name="Importance" />
            </BarChart>
          </ResponsiveContainer>
        </div>
      )}
    </div>
  );

  return (
    <div className="min-h-screen bg-gray-100 p-6">
      <div className="max-w-7xl mx-auto">
        {/* Header */}
        <div className="mb-8">
          <h1 className="text-3xl font-bold text-gray-900">Analytics Dashboard</h1>
          <p className="text-gray-600 mt-2">Real-time insights into decision, rule, and learning analytics</p>
        </div>

        {/* Tabs */}
        <div className="bg-white rounded-lg shadow mb-6 border-b">
          <div className="flex">
            {[
              { id: 'decision', label: 'Decision Analytics', icon: '📊' },
              { id: 'rule', label: 'Rule Performance', icon: '⚙️' },
              { id: 'learning', label: 'Learning Insights', icon: '🧠' },
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
                <p className="text-gray-600">Loading analytics...</p>
              </div>
            </div>
          ) : error ? (
            <div className="text-red-600 p-4 bg-red-50 rounded">{error}</div>
          ) : (
            <>
              {activeTab === 'decision' && renderDecisionAnalytics()}
              {activeTab === 'rule' && renderRuleAnalytics()}
              {activeTab === 'learning' && renderLearningAnalytics()}
            </>
          )}
        </div>

        {/* Auto-refresh Info */}
        <div className="mt-4 text-sm text-gray-600 text-center">
          Auto-refreshes every 30 seconds • Last updated: {new Date().toLocaleTimeString()}
        </div>
      </div>
    </div>
  );
};

export default AnalyticsDashboard;

