/**
 * Advanced Rule Engine - Main Page
 * Comprehensive fraud detection and rule management interface
 * Week 2: Async evaluation, batch processing, learning feedback
 */

import React, { useState, useEffect } from 'react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '../components/ui/card';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '../components/ui/tabs';
import { Badge } from '../components/ui/badge';
import { Button } from '../components/ui/button';
import {
  Shield,
  Rule,
  TestTube,
  BarChart3,
  Settings,
  Activity,
  AlertTriangle,
  TrendingUp,
  Zap,
  Brain,
  CheckCircle,
  AlertCircle
} from 'lucide-react';
import { apiClient } from '@/services/api';

// Import sub-components
import RuleEngineManagement from '../components/RuleEngine/RuleEngineManagement';
import RuleCreationForm from '../components/RuleEngine/RuleCreationForm';
import RuleTesting from '../components/RuleEngine/RuleTesting';
import RuleAnalytics from '../components/RuleEngine/RuleAnalytics';

const AdvancedRuleEngine: React.FC = () => {
  const [activeTab, setActiveTab] = useState('management');
  const [isLoading, setIsLoading] = useState(false);
  
  // Week 2: State management
  const [evaluationJobId, setEvaluationJobId] = useState<string | null>(null);
  const [jobStatus, setJobStatus] = useState<string | null>(null);
  const [evaluationMode, setEvaluationMode] = useState<'SYNCHRONOUS' | 'ASYNCHRONOUS' | 'BATCH' | 'STREAMING'>('SYNCHRONOUS');
  const [batchRuleIds, setBatchRuleIds] = useState<string[]>([]);
  const [performanceMetrics, setPerformanceMetrics] = useState<any>(null);
  const [learningStats, setLearningStats] = useState<any>(null);
  const [apiError, setApiError] = useState<string | null>(null);
  const [successMessage, setSuccessMessage] = useState<string | null>(null);
  const [showAdvancedOptions, setShowAdvancedOptions] = useState(false);

  // Mock data for dashboard stats
  const stats = {
    totalRules: 47,
    activeRules: 42,
    fraudDetections: 1234,
    averageExecutionTime: 45, // ms
    riskLevels: {
      low: 65,
      medium: 23,
      high: 8,
      critical: 4
    }
  };

  // Load performance metrics and learning stats
  useEffect(() => {
    const loadMetrics = async () => {
      try {
        // Try to load actual metrics from backend
        // This would be apiClient.getRulePerformanceMetrics() for a specific rule
        // For now, we'll show the pattern
      } catch (err) {
        console.warn('Failed to load metrics:', err);
      }
    };
    
    loadMetrics();
  }, []);

  // Handle async rule evaluation
  const handleEvaluateRuleAsync = async (ruleId: string, context: Record<string, any>) => {
    setApiError(null);
    setSuccessMessage(null);

    try {
      const res = await apiClient.evaluateRuleAsync({
        rule_id: ruleId,
        context,
        execution_mode: evaluationMode,
      });

      setEvaluationJobId(res.job_id);
      setJobStatus('PENDING');
      setSuccessMessage(`Rule evaluation job ${res.job_id} submitted with mode: ${evaluationMode}`);

      // Poll for result
      const pollInterval = setInterval(async () => {
        try {
          const result = await apiClient.getEvaluationResult(res.job_id);
          if (result.status === 'COMPLETED' || result.status === 'FAILED') {
            clearInterval(pollInterval);
            setJobStatus(result.status);
            if (result.status === 'COMPLETED') {
              setSuccessMessage(`Rule evaluation completed successfully!`);
            } else {
              setApiError(`Rule evaluation failed`);
            }
          } else {
            setJobStatus(result.status);
          }
        } catch (err) {
          console.error('Poll error:', err);
        }
      }, 2000);
    } catch (err) {
      setApiError(err instanceof Error ? err.message : 'Failed to evaluate rule');
    }
  };

  // Handle batch rule evaluation
  const handleEvaluateRulesBatch = async (ruleIds: string[], context: Record<string, any>) => {
    setApiError(null);
    setSuccessMessage(null);

    try {
      const res = await apiClient.evaluateRulesBatch({
        rule_ids: ruleIds,
        context,
      });

      setBatchRuleIds(ruleIds);
      setSuccessMessage(`Batch evaluation job created with ${ruleIds.length} rules`);
    } catch (err) {
      setApiError(err instanceof Error ? err.message : 'Failed to submit batch evaluation');
    }
  };

  // Submit learning feedback
  const handleSubmitFeedback = async (evaluationId: string, actualOutcome: string, confidence: number) => {
    setApiError(null);
    setSuccessMessage(null);

    try {
      await apiClient.submitRuleEvaluationFeedback({
        evaluation_id: evaluationId,
        actual_outcome: actualOutcome,
        confidence,
        notes: 'Feedback submitted from UI',
      });

      setSuccessMessage('Learning feedback submitted successfully');
    } catch (err) {
      setApiError(err instanceof Error ? err.message : 'Failed to submit feedback');
    }
  };

  return (
    <div className="container mx-auto p-6 space-y-6">
      {/* Header Section */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold flex items-center gap-3">
            <Shield className="h-8 w-8 text-blue-600" />
            Advanced Rule Engine
          </h1>
          <p className="text-gray-600 mt-2">
            Enterprise-grade fraud detection with async evaluation & machine learning
          </p>
        </div>
        <div className="flex gap-2">
          <Button variant="outline" size="sm">
            <Settings className="h-4 w-4 mr-2" />
            Engine Config
          </Button>
          <Button variant="outline" size="sm">
            <Activity className="h-4 w-4 mr-2" />
            Engine Status
          </Button>
          <Button 
            variant={showAdvancedOptions ? "default" : "outline"} 
            size="sm"
            onClick={() => setShowAdvancedOptions(!showAdvancedOptions)}
          >
            <Zap className="h-4 w-4 mr-2" />
            Week 2 Options
          </Button>
        </div>
      </div>

      {/* Week 2 Advanced Options Panel */}
      {showAdvancedOptions && (
        <div className="bg-gradient-to-r from-purple-50 to-blue-50 rounded-lg p-6 border border-purple-200">
          <h3 className="text-lg font-semibold mb-4 flex items-center gap-2">
            <Zap className="w-5 h-5 text-purple-600" />
            Week 2: Async & Learning Features
          </h3>

          <div className="grid grid-cols-1 md:grid-cols-3 gap-4 mb-4">
            <div>
              <label className="block text-sm font-medium text-gray-900 mb-2">Evaluation Mode</label>
              <select
                value={evaluationMode}
                onChange={(e) => setEvaluationMode(e.target.value as any)}
                className="w-full px-3 py-2 border border-gray-300 rounded-lg bg-white"
              >
                <option value="SYNCHRONOUS">Synchronous (Immediate)</option>
                <option value="ASYNCHRONOUS">Asynchronous (Job-based)</option>
                <option value="BATCH">Batch Processing</option>
                <option value="STREAMING">Streaming Mode</option>
              </select>
              <p className="text-xs text-gray-600 mt-1">Choose execution mode for rule evaluation</p>
            </div>

            <div>
              <label className="block text-sm font-medium text-gray-900 mb-2">Current Job Status</label>
              <div className="px-3 py-2 bg-white border border-gray-300 rounded-lg">
                {jobStatus ? (
                  <div className="flex items-center gap-2">
                    {jobStatus === 'COMPLETED' && <CheckCircle className="w-4 h-4 text-green-600" />}
                    {jobStatus === 'RUNNING' && <Activity className="w-4 h-4 text-blue-600 animate-spin" />}
                    {jobStatus === 'FAILED' && <AlertCircle className="w-4 h-4 text-red-600" />}
                    <span className="text-sm font-medium">{jobStatus}</span>
                  </div>
                ) : (
                  <span className="text-sm text-gray-500">No active job</span>
                )}
              </div>
              {evaluationJobId && (
                <p className="text-xs text-gray-600 mt-1">ID: {evaluationJobId.slice(0, 8)}...</p>
              )}
            </div>

            <div>
              <label className="block text-sm font-medium text-gray-900 mb-2">Batch Rules</label>
              <div className="px-3 py-2 bg-white border border-gray-300 rounded-lg">
                <span className="text-sm font-medium">{batchRuleIds.length} selected</span>
              </div>
              <p className="text-xs text-gray-600 mt-1">For batch evaluation mode</p>
            </div>
          </div>

          {/* Status Messages */}
          {successMessage && (
            <div className="p-3 bg-green-50 border border-green-200 rounded-lg text-green-700 text-sm flex items-center gap-2 mb-4">
              <CheckCircle className="w-4 h-4" />
              {successMessage}
            </div>
          )}

          {apiError && (
            <div className="p-3 bg-red-50 border border-red-200 rounded-lg text-red-700 text-sm flex items-center gap-2 mb-4">
              <AlertCircle className="w-4 h-4" />
              {apiError}
            </div>
          )}
        </div>
      )}

      {/* Dashboard Stats */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
        <Card>
          <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
            <CardTitle className="text-sm font-medium">Total Rules</CardTitle>
            <Rule className="h-4 w-4 text-muted-foreground" />
          </CardHeader>
          <CardContent>
            <div className="text-2xl font-bold">{stats.totalRules}</div>
            <p className="text-xs text-muted-foreground">
              {stats.activeRules} active
            </p>
          </CardContent>
        </Card>

        <Card>
          <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
            <CardTitle className="text-sm font-medium">Fraud Detections</CardTitle>
            <AlertTriangle className="h-4 w-4 text-muted-foreground" />
          </CardHeader>
          <CardContent>
            <div className="text-2xl font-bold">{stats.fraudDetections}</div>
            <p className="text-xs text-muted-foreground">
              +12% from last month
            </p>
          </CardContent>
        </Card>

        <Card>
          <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
            <CardTitle className="text-sm font-medium">Avg Execution Time</CardTitle>
            <TrendingUp className="h-4 w-4 text-muted-foreground" />
          </CardHeader>
          <CardContent>
            <div className="text-2xl font-bold">{stats.averageExecutionTime}ms</div>
            <p className="text-xs text-muted-foreground">
              Within target range
            </p>
          </CardContent>
        </Card>

        <Card>
          <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
            <CardTitle className="text-sm font-medium">Risk Distribution</CardTitle>
            <BarChart3 className="h-4 w-4 text-muted-foreground" />
          </CardHeader>
          <CardContent>
            <div className="flex gap-1 mb-2">
              <Badge variant="secondary" className="text-xs">L:{stats.riskLevels.low}</Badge>
              <Badge variant="outline" className="text-xs">M:{stats.riskLevels.medium}</Badge>
              <Badge variant="destructive" className="text-xs">H:{stats.riskLevels.high}</Badge>
              <Badge variant="destructive" className="text-xs border-red-800">C:{stats.riskLevels.critical}</Badge>
            </div>
            <p className="text-xs text-muted-foreground">
              Recent detections
            </p>
          </CardContent>
        </Card>
      </div>

      {/* Main Content Tabs */}
      <Tabs value={activeTab} onValueChange={setActiveTab} className="space-y-4">
        <TabsList className="grid w-full grid-cols-5">
          <TabsTrigger value="management" className="flex items-center gap-2">
            <Rule className="h-4 w-4" />
            Rule Management
          </TabsTrigger>
          <TabsTrigger value="creation" className="flex items-center gap-2">
            <Settings className="h-4 w-4" />
            Rule Creation
          </TabsTrigger>
          <TabsTrigger value="testing" className="flex items-center gap-2">
            <TestTube className="h-4 w-4" />
            Rule Testing
          </TabsTrigger>
          <TabsTrigger value="analytics" className="flex items-center gap-2">
            <BarChart3 className="h-4 w-4" />
            Analytics
          </TabsTrigger>
          <TabsTrigger value="learning" className="flex items-center gap-2">
            <Brain className="h-4 w-4" />
            Learning Feedback
          </TabsTrigger>
        </TabsList>

        <TabsContent value="management" className="space-y-4">
          <RuleEngineManagement />
        </TabsContent>

        <TabsContent value="creation" className="space-y-4">
          <RuleCreationForm />
        </TabsContent>

        <TabsContent value="testing" className="space-y-4">
          <RuleTesting />
        </TabsContent>

        <TabsContent value="analytics" className="space-y-4">
          <RuleAnalytics />
        </TabsContent>

        <TabsContent value="learning" className="space-y-4">
          <Card>
            <CardHeader>
              <CardTitle className="flex items-center gap-2">
                <Brain className="w-5 h-5 text-purple-600" />
                Learning Feedback Management
              </CardTitle>
              <CardDescription>
                Submit feedback on rule evaluations to improve accuracy through machine learning
              </CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="bg-blue-50 border border-blue-200 rounded-lg p-4">
                <h4 className="font-semibold text-blue-900 mb-2">How Learning Works</h4>
                <ul className="text-sm text-blue-800 space-y-1">
                  <li>✓ Rule evaluations are captured with outcomes</li>
                  <li>✓ You provide actual outcomes and confidence levels</li>
                  <li>✓ System learns from feedback to improve rule effectiveness</li>
                  <li>✓ Weights and thresholds are automatically adjusted</li>
                </ul>
              </div>

              <div className="grid grid-cols-3 gap-4">
                <div className="p-4 bg-green-50 rounded-lg border border-green-200">
                  <p className="text-sm font-medium text-green-900">Feedback Submitted</p>
                  <p className="text-2xl font-bold text-green-700">2,847</p>
                  <p className="text-xs text-green-600">This month</p>
                </div>
                <div className="p-4 bg-blue-50 rounded-lg border border-blue-200">
                  <p className="text-sm font-medium text-blue-900">Learning Updates</p>
                  <p className="text-2xl font-bold text-blue-700">156</p>
                  <p className="text-xs text-blue-600">Active improvements</p>
                </div>
                <div className="p-4 bg-purple-50 rounded-lg border border-purple-200">
                  <p className="text-sm font-medium text-purple-900">Accuracy Gain</p>
                  <p className="text-2xl font-bold text-purple-700">+8.3%</p>
                  <p className="text-xs text-purple-600">From baseline</p>
                </div>
              </div>

              <div className="p-4 bg-gray-50 rounded-lg border border-gray-200">
                <p className="text-sm text-gray-600">
                  Learning feedback is collected from rule testing and evaluation operations. The system continuously improves rule effectiveness based on actual outcomes provided by domain experts.
                </p>
              </div>
            </CardContent>
          </Card>
        </TabsContent>
      </Tabs>
    </div>
  );
};

export default AdvancedRuleEngine;