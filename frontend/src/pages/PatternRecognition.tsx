/**
 * Pattern Recognition Page - Production Implementation
 * Advanced AI pattern analysis and machine learning insights
 * Connects to real Pattern Recognition Engine and Learning Systems
 */

import React, { useState, useEffect } from 'react';
import {
  Network, Brain, Search, BarChart3, TrendingUp, AlertCircle,
  Play, Pause, Settings, Filter, Download, Eye, Zap,
  Clock, Target, CheckCircle, XCircle, RefreshCw
} from 'lucide-react';
import { apiClient } from '@/services/api';
import type * as API from '@/types/api';

interface PatternAnalysisJob {
  jobId: string;
  status: 'pending' | 'running' | 'completed' | 'failed';
  algorithm: 'clustering' | 'sequential' | 'association' | 'neural';
  dataSource: string;
  progress: number;
  startTime: string;
  endTime?: string;
  results?: API.Pattern[];
}

const PatternRecognition: React.FC = () => {
  const [patterns, setPatterns] = useState<API.Pattern[]>([]);
  const [patternStats, setPatternStats] = useState<API.PatternStats | null>(null);
  const [analysisJobs, setAnalysisJobs] = useState<PatternAnalysisJob[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [activeTab, setActiveTab] = useState<'discovered' | 'analysis' | 'predictions' | 'anomalies'>('discovered');
  const [selectedPattern, setSelectedPattern] = useState<API.Pattern | null>(null);
  const [filters, setFilters] = useState({
    algorithm: 'all',
    confidence: 0.7,
    timeRange: '7d'
  });

  useEffect(() => {
    loadPatternData();
  }, [filters]);

  const loadPatternData = async () => {
    try {
      setLoading(true);
      setError(null);

      const [patternsRes, statsRes, anomaliesRes] = await Promise.allSettled([
        apiClient.getPatterns({
          algorithm: filters.algorithm !== 'all' ? filters.algorithm : '',
          minConfidence: filters.confidence.toString(),
          timeRange: filters.timeRange
        }),
        apiClient.getPatternStats(),
        apiClient.getPatternAnomalies({
          severity: 'medium',
          timeRange: filters.timeRange
        })
      ]);

      if (patternsRes.status === 'fulfilled') {
        setPatterns(patternsRes.value);
      }

      if (statsRes.status === 'fulfilled') {
        setPatternStats(statsRes.value);
      }

      // Handle anomalies if needed
      if (anomaliesRes.status === 'rejected') {
        console.warn('Anomalies fetch failed:', anomaliesRes.reason);
      }

    } catch (err) {
      setError('Failed to load pattern data');
      console.error('Error loading patterns:', err);
    } finally {
      setLoading(false);
    }
  };

  const startPatternAnalysis = async (algorithm: string, dataSource: string) => {
    try {
      const result = await apiClient.detectPatterns({
        dataSource,
        algorithm: algorithm as any,
        timeRange: filters.timeRange,
        minSupport: 0.1,
        minConfidence: filters.confidence
      });

      // Add job to tracking
      const newJob: PatternAnalysisJob = {
        jobId: result.jobId,
        status: 'running',
        algorithm: algorithm as any,
        dataSource,
        progress: 0,
        startTime: new Date().toISOString()
      };

      setAnalysisJobs(prev => [...prev, newJob]);
      
      // Poll for job status updates from backend
      const pollJobStatus = async () => {
        try {
          const jobStatus = await apiClient.getPatternAnalysisJobStatus(result.jobId);
          
          setAnalysisJobs(prev => prev.map(job => 
            job.jobId === result.jobId ? {
              ...job,
              status: jobStatus.status,
              progress: jobStatus.progress || job.progress,
              endTime: jobStatus.status === 'completed' ? new Date().toISOString() : undefined
            } : job
          ));
          
          // Continue polling if job is still running
          if (jobStatus.status === 'running' || jobStatus.status === 'pending') {
            setTimeout(pollJobStatus, 2000); // Poll every 2 seconds
          } else if (jobStatus.status === 'completed') {
            // Reload patterns after completion
            loadPatternData();
          }
        } catch (pollError) {
          console.error('Error polling job status:', pollError);
          // Mark job as failed if polling fails
          setAnalysisJobs(prev => prev.map(job => 
            job.jobId === result.jobId ? { ...job, status: 'failed' } : job
          ));
        }
      };
      
      // Start polling after a short delay
      setTimeout(pollJobStatus, 1000);

    } catch (err) {
      console.error('Failed to start pattern analysis:', err);
      setError('Failed to start pattern analysis');
    }
  };

  const getPatternIcon = (type: string) => {
    switch (type) {
      case 'sequential': return <TrendingUp className="w-5 h-5 text-blue-500" />;
      case 'clustering': return <Network className="w-5 h-5 text-purple-500" />;
      case 'association': return <Target className="w-5 h-5 text-green-500" />;
      case 'neural': return <Brain className="w-5 h-5 text-orange-500" />;
      default: return <BarChart3 className="w-5 h-5 text-gray-500" />;
    }
  };

  const getConfidenceColor = (confidence: number) => {
    if (confidence >= 0.8) return 'text-green-600 bg-green-50';
    if (confidence >= 0.6) return 'text-yellow-600 bg-yellow-50';
    return 'text-red-600 bg-red-50';
  };

  const getJobStatusIcon = (status: string) => {
    switch (status) {
      case 'completed': return <CheckCircle className="w-4 h-4 text-green-500" />;
      case 'failed': return <XCircle className="w-4 h-4 text-red-500" />;
      case 'running': return <RefreshCw className="w-4 h-4 text-blue-500 animate-spin" />;
      default: return <Clock className="w-4 h-4 text-gray-500" />;
    }
  };

  if (loading) {
    return (
      <div className="p-6">
        <div className="animate-pulse space-y-6">
          <div className="h-8 bg-gray-200 rounded w-1/3"></div>
          <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
            {[...Array(3)].map((_, i) => (
              <div key={i} className="h-32 bg-gray-200 rounded"></div>
            ))}
          </div>
          <div className="h-96 bg-gray-200 rounded"></div>
        </div>
      </div>
    );
  }

  return (
    <div className="p-6 space-y-6">
      {/* Header */}
      <div className="flex justify-between items-center">
        <div>
          <h1 className="text-2xl font-bold text-gray-900 flex items-center gap-3">
            <Network className="w-8 h-8 text-purple-600" />
            Pattern Recognition & Analysis
          </h1>
          <p className="text-gray-600 mt-1">
            Advanced AI-powered pattern discovery and behavioral analytics
          </p>
        </div>
        <div className="flex items-center gap-4">
          <button
            onClick={() => startPatternAnalysis('neural', 'transactions')}
            className="flex items-center gap-2 px-4 py-2 bg-purple-600 text-white rounded-lg hover:bg-purple-700"
          >
            <Play className="w-4 h-4" />
            Run Analysis
          </button>
          <button
            onClick={loadPatternData}
            className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700"
          >
            <RefreshCw className="w-4 h-4" />
            Refresh
          </button>
        </div>
      </div>

      {/* Error Alert */}
      {error && (
        <div className="bg-red-50 border border-red-200 rounded-lg p-4">
          <div className="flex items-center gap-3">
            <AlertCircle className="w-5 h-5 text-red-600" />
            <div>
              <h3 className="font-semibold text-red-800">Pattern Analysis Error</h3>
              <p className="text-red-600">{error}</p>
            </div>
          </div>
        </div>
      )}

      {/* Statistics Cards */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
        <div className="bg-white rounded-lg shadow p-6">
          <div className="flex items-center justify-between">
            <div>
              <p className="text-sm text-gray-600">Total Patterns</p>
              <p className="text-2xl font-bold text-purple-600">
                {patternStats?.totalPatterns || patterns.length}
              </p>
            </div>
            <Network className="w-8 h-8 text-purple-500" />
          </div>
        </div>

        <div className="bg-white rounded-lg shadow p-6">
          <div className="flex items-center justify-between">
            <div>
              <p className="text-sm text-gray-600">High Confidence</p>
              <p className="text-2xl font-bold text-green-600">
                {patterns.filter(p => (p.confidence || 0) >= 0.8).length}
              </p>
            </div>
            <CheckCircle className="w-8 h-8 text-green-500" />
          </div>
        </div>

        <div className="bg-white rounded-lg shadow p-6">
          <div className="flex items-center justify-between">
            <div>
              <p className="text-sm text-gray-600">Active Jobs</p>
              <p className="text-2xl font-bold text-blue-600">
                {analysisJobs.filter(j => j.status === 'running').length}
              </p>
            </div>
            <RefreshCw className="w-8 h-8 text-blue-500" />
          </div>
        </div>

        <div className="bg-white rounded-lg shadow p-6">
          <div className="flex items-center justify-between">
            <div>
              <p className="text-sm text-gray-600">Avg Confidence</p>
              <p className="text-2xl font-bold text-orange-600">
                {patterns.length > 0 
                  ? (patterns.reduce((sum, p) => sum + (p.confidence || 0), 0) / patterns.length * 100).toFixed(1)
                  : '0'}%
              </p>
            </div>
            <Target className="w-8 h-8 text-orange-500" />
          </div>
        </div>
      </div>

      {/* Filters */}
      <div className="bg-white rounded-lg shadow p-6">
        <div className="flex items-center gap-4 flex-wrap">
          <div className="flex items-center gap-2">
            <Filter className="w-4 h-4 text-gray-500" />
            <span className="text-sm font-medium text-gray-700">Filters:</span>
          </div>
          
          <div className="flex items-center gap-2">
            <label className="text-sm text-gray-600">Algorithm:</label>
            <select
              value={filters.algorithm}
              onChange={(e) => setFilters(prev => ({ ...prev, algorithm: e.target.value }))}
              className="px-3 py-1 border rounded text-sm"
            >
              <option value="all">All</option>
              <option value="clustering">Clustering</option>
              <option value="sequential">Sequential</option>
              <option value="association">Association</option>
              <option value="neural">Neural</option>
            </select>
          </div>

          <div className="flex items-center gap-2">
            <label className="text-sm text-gray-600">Min Confidence:</label>
            <input
              type="range"
              min="0"
              max="1"
              step="0.1"
              value={filters.confidence}
              onChange={(e) => setFilters(prev => ({ ...prev, confidence: parseFloat(e.target.value) }))}
              className="w-20"
            />
            <span className="text-sm font-medium">{(filters.confidence * 100).toFixed(0)}%</span>
          </div>

          <div className="flex items-center gap-2">
            <label className="text-sm text-gray-600">Time Range:</label>
            <select
              value={filters.timeRange}
              onChange={(e) => setFilters(prev => ({ ...prev, timeRange: e.target.value }))}
              className="px-3 py-1 border rounded text-sm"
            >
              <option value="1d">Last 24 hours</option>
              <option value="7d">Last 7 days</option>
              <option value="30d">Last 30 days</option>
              <option value="90d">Last 90 days</option>
            </select>
          </div>
        </div>
      </div>

      {/* Tab Navigation */}
      <div className="border-b border-gray-200">
        <nav className="flex space-x-8">
          {[
            { id: 'discovered', label: 'Discovered Patterns', icon: Search },
            { id: 'analysis', label: 'Analysis Jobs', icon: Settings },
            { id: 'predictions', label: 'Predictions', icon: TrendingUp },
            { id: 'anomalies', label: 'Anomalies', icon: AlertCircle }
          ].map(({ id, label, icon: Icon }) => (
            <button
              key={id}
              onClick={() => setActiveTab(id as any)}
              className={`flex items-center gap-2 py-2 px-1 border-b-2 font-medium text-sm ${
                activeTab === id
                  ? 'border-purple-500 text-purple-600'
                  : 'border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300'
              }`}
            >
              <Icon className="w-4 h-4" />
              {label}
            </button>
          ))}
        </nav>
      </div>

      {/* Tab Content */}
      {activeTab === 'discovered' && (
        <div className="space-y-6">
          <div className="grid grid-cols-1 lg:grid-cols-2 xl:grid-cols-3 gap-6">
            {patterns.map((pattern) => (
              <div
                key={pattern.id}
                className="bg-white rounded-lg shadow p-6 hover:shadow-lg transition-shadow cursor-pointer"
                onClick={() => setSelectedPattern(pattern)}
              >
                <div className="flex justify-between items-start mb-4">
                  <div className="flex items-center gap-2">
                    {getPatternIcon(pattern.type || 'unknown')}
                    <h3 className="font-semibold">{pattern.name}</h3>
                  </div>
                  <span className={`px-2 py-1 rounded text-xs font-medium ${
                    getConfidenceColor(pattern.confidence || 0)
                  }`}>
                    {((pattern.confidence || 0) * 100).toFixed(1)}%
                  </span>
                </div>
                
                <p className="text-gray-600 text-sm mb-4">{pattern.description}</p>
                
                <div className="flex justify-between items-center text-sm">
                  <span className="text-gray-500">
                    Detected: {pattern.detectedAt ? new Date(pattern.detectedAt).toLocaleDateString() : 'Unknown'}
                  </span>
                  <button
                    onClick={(e) => {
                      e.stopPropagation();
                      setSelectedPattern(pattern);
                    }}
                    className="text-purple-600 hover:text-purple-700 flex items-center gap-1"
                  >
                    <Eye className="w-3 h-3" />
                    View
                  </button>
                </div>
              </div>
            ))}
          </div>

          {patterns.length === 0 && (
            <div className="text-center py-12">
              <Network className="w-16 h-16 text-gray-300 mx-auto mb-4" />
              <h3 className="text-lg font-medium text-gray-900 mb-2">No Patterns Discovered</h3>
              <p className="text-gray-500 mb-4">Run pattern analysis to discover behavioral patterns in your data.</p>
              <button
                onClick={() => startPatternAnalysis('neural', 'transactions')}
                className="inline-flex items-center gap-2 px-4 py-2 bg-purple-600 text-white rounded-lg hover:bg-purple-700"
              >
                <Play className="w-4 h-4" />
                Start Analysis
              </button>
            </div>
          )}
        </div>
      )}

      {activeTab === 'analysis' && (
        <div className="space-y-6">
          <div className="bg-white rounded-lg shadow p-6">
            <h3 className="text-lg font-semibold mb-4">Analysis Jobs</h3>
            {analysisJobs.length > 0 ? (
              <div className="space-y-4">
                {analysisJobs.map((job) => (
                  <div key={job.jobId} className="border rounded-lg p-4">
                    <div className="flex justify-between items-start mb-2">
                      <div className="flex items-center gap-2">
                        {getJobStatusIcon(job.status)}
                        <h4 className="font-medium">{job.algorithm} Analysis</h4>
                      </div>
                      <span className={`px-2 py-1 rounded text-xs font-medium ${
                        job.status === 'completed' ? 'bg-green-100 text-green-800' :
                        job.status === 'failed' ? 'bg-red-100 text-red-800' :
                        job.status === 'running' ? 'bg-blue-100 text-blue-800' :
                        'bg-gray-100 text-gray-800'
                      }`}>
                        {job.status.toUpperCase()}
                      </span>
                    </div>
                    
                    <div className="grid grid-cols-2 md:grid-cols-4 gap-4 text-sm mb-3">
                      <div>
                        <span className="text-gray-600">Data Source:</span>
                        <span className="ml-2 font-medium">{job.dataSource}</span>
                      </div>
                      <div>
                        <span className="text-gray-600">Started:</span>
                        <span className="ml-2 font-medium">
                          {new Date(job.startTime).toLocaleString()}
                        </span>
                      </div>
                      <div>
                        <span className="text-gray-600">Progress:</span>
                        <span className="ml-2 font-medium">{job.progress}%</span>
                      </div>
                      {job.endTime && (
                        <div>
                          <span className="text-gray-600">Completed:</span>
                          <span className="ml-2 font-medium">
                            {new Date(job.endTime).toLocaleString()}
                          </span>
                        </div>
                      )}
                    </div>
                    
                    {job.status === 'running' && (
                      <div className="w-full bg-gray-200 rounded-full h-2">
                        <div 
                          className="bg-blue-600 h-2 rounded-full transition-all duration-300"
                          style={{ width: `${job.progress}%` }}
                        ></div>
                      </div>
                    )}
                  </div>
                ))}
              </div>
            ) : (
              <p className="text-gray-500 text-center py-8">No analysis jobs running</p>
            )}
          </div>
        </div>
      )}

      {/* Selected Pattern Details Modal */}
      {selectedPattern && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center p-4 z-50">
          <div className="bg-white rounded-lg shadow-xl max-w-2xl w-full max-h-96 overflow-y-auto">
            <div className="p-6">
              <div className="flex justify-between items-start mb-4">
                <div className="flex items-center gap-2">
                  {getPatternIcon(selectedPattern.type || 'unknown')}
                  <h3 className="text-xl font-semibold">{selectedPattern.name}</h3>
                </div>
                <button
                  onClick={() => setSelectedPattern(null)}
                  className="text-gray-400 hover:text-gray-600"
                >
                  <XCircle className="w-6 h-6" />
                </button>
              </div>
              
              <div className="space-y-4">
                <div>
                  <label className="text-sm font-medium text-gray-600">Description</label>
                  <p className="text-gray-800">{selectedPattern.description}</p>
                </div>
                
                <div className="grid grid-cols-2 gap-4">
                  <div>
                    <label className="text-sm font-medium text-gray-600">Confidence</label>
                    <p className="text-lg font-semibold">
                      {((selectedPattern.confidence || 0) * 100).toFixed(1)}%
                    </p>
                  </div>
                  <div>
                    <label className="text-sm font-medium text-gray-600">Type</label>
                    <p className="text-lg font-semibold capitalize">{selectedPattern.type}</p>
                  </div>
                </div>
                
                {selectedPattern.metadata && (
                  <div>
                    <label className="text-sm font-medium text-gray-600">Details</label>
                    <pre className="text-xs bg-gray-50 p-3 rounded overflow-auto">
                      {JSON.stringify(selectedPattern.metadata, null, 2)}
                    </pre>
                  </div>
                )}
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default PatternRecognition;
