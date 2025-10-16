/**
 * Impact Analysis Component
 * Advanced regulatory impact analysis with forecasting and optimization
 * Production-grade analytical tools for regulatory impact assessment
 */

import React, { useState, useEffect, useMemo } from 'react';
import {
  TrendingUp,
  TrendingDown,
  BarChart3,
  LineChart,
  PieChart,
  ScatterChart,
  Activity,
  Target,
  Zap,
  Calculator,
  Lightbulb,
  AlertTriangle,
  CheckCircle,
  XCircle,
  RefreshCw,
  Download,
  Share2,
  Settings,
  Filter,
  Search,
  Eye,
  Layers,
  GitBranch,
  Clock,
  DollarSign,
  Users,
  Building,
  Shield,
  AlertCircle,
  Info,
  ChevronDown,
  ChevronUp,
  Plus,
  Minus,
  ArrowUp,
  ArrowDown,
  ArrowRight,
  Play,
  Pause,
  RotateCcw,
  Save,
  FileText,
  Calendar,
  Percent,
  Hash,
  Globe,
  MapPin
} from 'lucide-react';
import { clsx } from 'clsx';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { format, addMonths, subMonths } from 'date-fns';

// Types (matching backend and extending for advanced analysis)
export interface ImpactAnalysisData {
  analysis_id: string;
  scenario_id: string;
  analysis_type: 'baseline' | 'comparative' | 'sensitivity' | 'forecasting' | 'optimization';
  baseline_scenario?: string;
  comparison_scenarios?: string[];
  time_horizon_months: number;
  analysis_parameters: any;
  results: ImpactAnalysisResults;
  confidence_intervals: any;
  sensitivity_analysis?: SensitivityAnalysis;
  optimization_recommendations?: OptimizationRecommendation[];
  created_at: string;
  metadata: any;
}

export interface ImpactAnalysisResults {
  summary_metrics: {
    total_impact_score: number;
    confidence_level: number;
    risk_adjusted_impact: number;
    time_to_full_impact: number;
  };
  temporal_projection: {
    monthly_impact: Array<{
      month: string;
      compliance_impact: number;
      operational_impact: number;
      financial_impact: number;
      risk_impact: number;
    }>;
    cumulative_impact: Array<{
      month: string;
      total_compliance_change: number;
      total_operational_change: number;
      total_financial_change: number;
      total_risk_change: number;
    }>;
  };
  dimensional_breakdown: {
    by_regulatory_area: Array<{
      area: string;
      impact_score: number;
      affected_entities: number;
      compliance_gap: number;
    }>;
    by_entity_type: Array<{
      entity_type: string;
      impact_score: number;
      count: number;
      percentage: number;
    }>;
    by_geographic_region: Array<{
      region: string;
      impact_score: number;
      entities_affected: number;
      local_factors: any;
    }>;
  };
  correlation_analysis: {
    impact_correlations: Array<{
      factor1: string;
      factor2: string;
      correlation_coefficient: number;
      significance_level: number;
    }>;
    key_drivers: Array<{
      factor: string;
      impact_weight: number;
      confidence_interval: [number, number];
    }>;
  };
  risk_assessment: {
    overall_risk_score: number;
    risk_distribution: {
      low_risk: number;
      medium_risk: number;
      high_risk: number;
      critical_risk: number;
    };
    risk_trends: Array<{
      period: string;
      risk_score: number;
      trend_direction: 'increasing' | 'decreasing' | 'stable';
    }>;
    risk_mitigation_effectiveness: Array<{
      mitigation_strategy: string;
      effectiveness_score: number;
      implementation_cost: number;
      risk_reduction: number;
    }>;
  };
}

export interface SensitivityAnalysis {
  variables: Array<{
    variable_name: string;
    baseline_value: number;
    range: [number, number];
    impact_sensitivity: number;
  }>;
  scenarios: Array<{
    scenario_name: string;
    parameter_changes: Record<string, number>;
    impact_change: number;
    probability: number;
  }>;
  tornado_diagram: Array<{
    variable: string;
    low_impact: number;
    high_impact: number;
    sensitivity: number;
  }>;
}

export interface OptimizationRecommendation {
  recommendation_id: string;
  title: string;
  description: string;
  impact_improvement: number;
  implementation_cost: number;
  implementation_time_days: number;
  risk_level: 'low' | 'medium' | 'high';
  priority_score: number;
  dependencies: string[];
  success_probability: number;
}

interface ImpactAnalysisProps {
  scenarioId?: string;
  analysisType?: 'baseline' | 'comparative' | 'sensitivity' | 'forecasting' | 'optimization';
  onAnalysisComplete?: (analysis: ImpactAnalysisData) => void;
  onExport?: (format: 'pdf' | 'csv' | 'json') => void;
  onSaveAnalysis?: (analysis: ImpactAnalysisData) => void;
  className?: string;
}

const ANALYSIS_TYPES = [
  { value: 'baseline', label: 'Baseline Analysis', description: 'Analyze impact against current state', icon: Target },
  { value: 'comparative', label: 'Comparative Analysis', description: 'Compare multiple scenarios', icon: BarChart3 },
  { value: 'sensitivity', label: 'Sensitivity Analysis', description: 'Test impact sensitivity to variables', icon: Activity },
  { value: 'forecasting', label: 'Impact Forecasting', description: 'Project future impact trends', icon: TrendingUp },
  { value: 'optimization', label: 'Impact Optimization', description: 'Find optimal implementation strategies', icon: Zap }
];

const TIME_HORIZONS = [
  { value: 3, label: '3 Months' },
  { value: 6, label: '6 Months' },
  { value: 12, label: '1 Year' },
  { value: 24, label: '2 Years' },
  { value: 36, label: '3 Years' }
];

const ImpactAnalysis: React.FC<ImpactAnalysisProps> = ({
  scenarioId,
  analysisType = 'baseline',
  onAnalysisComplete,
  onExport,
  onSaveAnalysis,
  className = ''
}) => {
  const [currentAnalysis, setCurrentAnalysis] = useState<ImpactAnalysisData | null>(null);
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [selectedAnalysisType, setSelectedAnalysisType] = useState(analysisType);
  const [timeHorizon, setTimeHorizon] = useState(12);
  const [comparisonScenarios, setComparisonScenarios] = useState<string[]>([]);
  const [analysisParameters, setAnalysisParameters] = useState<any>({
    include_confidence_intervals: true,
    sensitivity_variables: ['compliance_threshold', 'operational_capacity', 'budget_allocation'],
    forecasting_method: 'linear_regression',
    optimization_criteria: ['maximize_compliance', 'minimize_cost', 'minimize_risk']
  });
  const [activeTab, setActiveTab] = useState<'overview' | 'temporal' | 'dimensional' | 'correlation' | 'sensitivity' | 'optimization'>('overview');
  const [expandedSections, setExpandedSections] = useState<Set<string>>(new Set(['summary', 'metrics']));

  // Mock analysis data (replace with real API calls)
  useEffect(() => {
    if (scenarioId) {
      const mockAnalysis: ImpactAnalysisData = {
        analysis_id: `analysis-${Date.now()}`,
        scenario_id: scenarioId,
        analysis_type: selectedAnalysisType,
        baseline_scenario: 'scenario-baseline',
        comparison_scenarios: comparisonScenarios,
        time_horizon_months: timeHorizon,
        analysis_parameters: analysisParameters,
        results: {
          summary_metrics: {
            total_impact_score: 7.8,
            confidence_level: 89.5,
            risk_adjusted_impact: 6.9,
            time_to_full_impact: 8.5
          },
          temporal_projection: {
            monthly_impact: Array.from({ length: timeHorizon }, (_, i) => ({
              month: addMonths(new Date(), i).toISOString(),
              compliance_impact: Math.random() * 2 - 1,
              operational_impact: Math.random() * 3 - 1.5,
              financial_impact: Math.random() * 50000 - 25000,
              risk_impact: Math.random() * 1.5 - 0.5
            })),
            cumulative_impact: Array.from({ length: timeHorizon }, (_, i) => {
              const months = i + 1;
              return {
                month: addMonths(new Date(), i).toISOString(),
                total_compliance_change: (Math.random() - 0.5) * months * 0.5,
                total_operational_change: (Math.random() - 0.3) * months,
                total_financial_change: (Math.random() - 0.5) * months * 10000,
                total_risk_change: (Math.random() - 0.4) * months * 0.3
              };
            })
          },
          dimensional_breakdown: {
            by_regulatory_area: [
              { area: 'KYC/AML', impact_score: 8.5, affected_entities: 450, compliance_gap: 15.2 },
              { area: 'Data Privacy', impact_score: 7.2, affected_entities: 320, compliance_gap: 8.7 },
              { area: 'Reporting', impact_score: 6.8, affected_entities: 280, compliance_gap: 12.1 },
              { area: 'Capital Requirements', impact_score: 9.1, affected_entities: 180, compliance_gap: 22.4 }
            ],
            by_entity_type: [
              { entity_type: 'Customers', impact_score: 7.8, count: 1250, percentage: 35.2 },
              { entity_type: 'Accounts', impact_score: 8.2, count: 890, percentage: 25.1 },
              { entity_type: 'Transactions', impact_score: 6.9, count: 2150, percentage: 60.6 },
              { entity_type: 'Processes', impact_score: 7.5, count: 145, percentage: 4.1 }
            ],
            by_geographic_region: [
              { region: 'North America', impact_score: 8.1, entities_affected: 1200, local_factors: { regulatory_complexity: 0.8 } },
              { region: 'Europe', impact_score: 7.9, entities_affected: 980, local_factors: { regulatory_complexity: 0.9 } },
              { region: 'Asia Pacific', impact_score: 6.7, entities_affected: 650, local_factors: { regulatory_complexity: 0.6 } },
              { region: 'Other', impact_score: 7.2, entities_affected: 340, local_factors: { regulatory_complexity: 0.7 } }
            ]
          },
          correlation_analysis: {
            impact_correlations: [
              { factor1: 'compliance_score', factor2: 'operational_efficiency', correlation_coefficient: 0.78, significance_level: 0.001 },
              { factor1: 'budget_allocation', factor2: 'implementation_speed', correlation_coefficient: -0.65, significance_level: 0.01 },
              { factor1: 'regulatory_complexity', factor2: 'risk_score', correlation_coefficient: 0.82, significance_level: 0.001 }
            ],
            key_drivers: [
              { factor: 'compliance_threshold', impact_weight: 0.35, confidence_interval: [0.32, 0.38] },
              { factor: 'operational_capacity', impact_weight: 0.28, confidence_interval: [0.25, 0.31] },
              { factor: 'budget_allocation', impact_weight: 0.22, confidence_interval: [0.19, 0.25] },
              { factor: 'training_completion', impact_weight: 0.15, confidence_interval: [0.12, 0.18] }
            ]
          },
          risk_assessment: {
            overall_risk_score: 7.2,
            risk_distribution: {
              low_risk: 25,
              medium_risk: 35,
              high_risk: 30,
              critical_risk: 10
            },
            risk_trends: Array.from({ length: timeHorizon }, (_, i) => ({
              period: addMonths(new Date(), i).toISOString(),
              risk_score: 7.2 + (Math.random() - 0.5) * 2,
              trend_direction: Math.random() > 0.5 ? 'increasing' : 'decreasing' as 'increasing' | 'decreasing' | 'stable'
            })),
            risk_mitigation_effectiveness: [
              { mitigation_strategy: 'Phased Implementation', effectiveness_score: 0.85, implementation_cost: 50000, risk_reduction: 0.6 },
              { mitigation_strategy: 'Enhanced Training', effectiveness_score: 0.72, implementation_cost: 75000, risk_reduction: 0.45 },
              { mitigation_strategy: 'Technology Upgrade', effectiveness_score: 0.91, implementation_cost: 200000, risk_reduction: 0.75 },
              { mitigation_strategy: 'External Consulting', effectiveness_score: 0.68, implementation_cost: 150000, risk_reduction: 0.35 }
            ]
          }
        },
        confidence_intervals: {
          impact_score: [7.2, 8.4],
          compliance_change: [-3.2, -1.8],
          cost_impact: [380000, 420000]
        },
        sensitivity_analysis: selectedAnalysisType === 'sensitivity' ? {
          variables: [
            { variable_name: 'compliance_threshold', baseline_value: 0.8, range: [0.7, 0.9], impact_sensitivity: 0.75 },
            { variable_name: 'operational_capacity', baseline_value: 100, range: [80, 120], impact_sensitivity: 0.62 },
            { variable_name: 'budget_allocation', baseline_value: 500000, range: [300000, 700000], impact_sensitivity: 0.58 }
          ],
          scenarios: [
            { scenario_name: 'Optimistic', parameter_changes: { compliance_threshold: 0.85, operational_capacity: 110 }, impact_change: 15.2, probability: 0.3 },
            { scenario_name: 'Pessimistic', parameter_changes: { compliance_threshold: 0.75, operational_capacity: 90 }, impact_change: -8.7, probability: 0.2 },
            { scenario_name: 'Most Likely', parameter_changes: { compliance_threshold: 0.8, operational_capacity: 100 }, impact_change: 2.1, probability: 0.5 }
          ],
          tornado_diagram: [
            { variable: 'compliance_threshold', low_impact: -12.5, high_impact: 8.3, sensitivity: 0.85 },
            { variable: 'operational_capacity', low_impact: -9.2, high_impact: 11.7, sensitivity: 0.78 },
            { variable: 'budget_allocation', low_impact: -7.8, high_impact: 9.4, sensitivity: 0.72 }
          ]
        } : undefined,
        optimization_recommendations: selectedAnalysisType === 'optimization' ? [
          {
            recommendation_id: 'rec-001',
            title: 'Implement Automated Compliance Monitoring',
            description: 'Deploy AI-powered compliance monitoring system to reduce manual oversight by 60%',
            impact_improvement: 25.5,
            implementation_cost: 180000,
            implementation_time_days: 90,
            risk_level: 'low',
            priority_score: 9.2,
            dependencies: [],
            success_probability: 0.85
          },
          {
            recommendation_id: 'rec-002',
            title: 'Phased Regulatory Implementation',
            description: 'Implement regulatory changes in phases to minimize operational disruption',
            impact_improvement: 18.3,
            implementation_cost: 75000,
            implementation_time_days: 120,
            risk_level: 'medium',
            priority_score: 8.7,
            dependencies: ['rec-001'],
            success_probability: 0.78
          },
          {
            recommendation_id: 'rec-003',
            title: 'Enhanced Staff Training Program',
            description: 'Comprehensive training program for all affected staff members',
            impact_improvement: 15.8,
            implementation_cost: 95000,
            implementation_time_days: 60,
            risk_level: 'low',
            priority_score: 8.1,
            dependencies: [],
            success_probability: 0.92
          }
        ] : undefined,
        created_at: new Date().toISOString(),
        metadata: {
          analysis_version: '2.1',
          data_quality_score: 94.2,
          computational_time_seconds: 45.8
        }
      };

      setCurrentAnalysis(mockAnalysis);
    }
  }, [scenarioId, selectedAnalysisType, timeHorizon, comparisonScenarios, analysisParameters]);

  const handleRunAnalysis = async () => {
    setIsAnalyzing(true);
    try {
      // Mock analysis execution
      await new Promise(resolve => setTimeout(resolve, 3000));

      // Analysis is already set in useEffect
      if (onAnalysisComplete && currentAnalysis) {
        onAnalysisComplete(currentAnalysis);
      }
    } catch (error) {
      console.error('Analysis failed:', error);
    } finally {
      setIsAnalyzing(false);
    }
  };

  const toggleSection = (section: string) => {
    const newExpanded = new Set(expandedSections);
    if (newExpanded.has(section)) {
      newExpanded.delete(section);
    } else {
      newExpanded.add(section);
    }
    setExpandedSections(newExpanded);
  };

  const getAnalysisTypeInfo = (type: string) => {
    return ANALYSIS_TYPES.find(t => t.value === type) || ANALYSIS_TYPES[0];
  };

  const formatPercentage = (value: number): string => {
    return `${value > 0 ? '+' : ''}${value.toFixed(1)}%`;
  };

  const formatCurrency = (amount: number): string => {
    return new Intl.NumberFormat('en-US', {
      style: 'currency',
      currency: 'USD',
      minimumFractionDigits: 0,
      maximumFractionDigits: 0
    }).format(amount);
  };

  const getRiskColor = (score: number): string => {
    if (score >= 8) return 'text-red-600';
    if (score >= 6) return 'text-orange-600';
    if (score >= 4) return 'text-yellow-600';
    return 'text-green-600';
  };

  const getImpactColor = (impact: number): string => {
    if (impact > 0) return 'text-green-600';
    if (impact > -5) return 'text-yellow-600';
    return 'text-red-600';
  };

  if (!scenarioId) {
    return (
      <div className="bg-white rounded-lg border shadow-sm p-12 text-center">
        <BarChart3 className="w-12 h-12 mx-auto mb-4 text-gray-400" />
        <h3 className="text-lg font-medium text-gray-900 mb-2">No Scenario Selected</h3>
        <p className="text-sm text-gray-600">
          Select a scenario to perform advanced impact analysis.
        </p>
      </div>
    );
  }

  return (
    <div className={clsx('space-y-6', className)}>
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold text-gray-900 flex items-center gap-3">
            <Calculator className="w-8 h-8 text-blue-600" />
            Advanced Impact Analysis
          </h1>
          <p className="text-gray-600 mt-1">
            Deep-dive analysis with forecasting, optimization, and comparative insights
          </p>
        </div>

        <div className="flex items-center gap-3">
          <button
            onClick={handleRunAnalysis}
            disabled={isAnalyzing}
            className="flex items-center gap-2 px-6 py-3 bg-blue-600 hover:bg-blue-700 disabled:bg-gray-400 text-white rounded-lg transition-colors"
          >
            {isAnalyzing ? (
              <RefreshCw className="w-5 h-5 animate-spin" />
            ) : (
              <Play className="w-5 h-5" />
            )}
            {isAnalyzing ? 'Analyzing...' : 'Run Analysis'}
          </button>

          {currentAnalysis && (
            <>
              <button
                onClick={() => onSaveAnalysis?.(currentAnalysis)}
                className="flex items-center gap-2 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors"
              >
                <Save className="w-4 h-4" />
                Save Analysis
              </button>
              <button
                onClick={() => onExport?.('pdf')}
                className="flex items-center gap-2 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors"
              >
                <Download className="w-4 h-4" />
                Export Report
              </button>
            </>
          )}
        </div>
      </div>

      {/* Analysis Configuration */}
      <div className="bg-white rounded-lg border shadow-sm p-6">
        <h3 className="text-lg font-semibold text-gray-900 mb-4">Analysis Configuration</h3>

        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-2">
              Analysis Type
            </label>
            <select
              value={selectedAnalysisType}
              onChange={(e) => setSelectedAnalysisType(e.target.value as any)}
              className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
            >
              {ANALYSIS_TYPES.map(type => (
                <option key={type.value} value={type.value}>{type.label}</option>
              ))}
            </select>
          </div>

          <div>
            <label className="block text-sm font-medium text-gray-700 mb-2">
              Time Horizon
            </label>
            <select
              value={timeHorizon}
              onChange={(e) => setTimeHorizon(parseInt(e.target.value))}
              className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
            >
              {TIME_HORIZONS.map(horizon => (
                <option key={horizon.value} value={horizon.value}>{horizon.label}</option>
              ))}
            </select>
          </div>

          {selectedAnalysisType === 'comparative' && (
            <div className="md:col-span-2">
              <label className="block text-sm font-medium text-gray-700 mb-2">
                Comparison Scenarios
              </label>
              <div className="flex gap-2">
                <input
                  type="text"
                  placeholder="Add scenario ID..."
                  className="flex-1 px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                  onKeyPress={(e) => {
                    if (e.key === 'Enter') {
                      const value = (e.target as HTMLInputElement).value.trim();
                      if (value && !comparisonScenarios.includes(value)) {
                        setComparisonScenarios([...comparisonScenarios, value]);
                        (e.target as HTMLInputElement).value = '';
                      }
                    }
                  }}
                />
              </div>
              <div className="flex flex-wrap gap-2 mt-2">
                {comparisonScenarios.map((scenario, index) => (
                  <span key={index} className="inline-flex items-center gap-1 px-2 py-1 bg-blue-100 text-blue-800 text-xs rounded-full">
                    {scenario}
                    <button
                      onClick={() => setComparisonScenarios(comparisonScenarios.filter(s => s !== scenario))}
                      className="hover:text-blue-600"
                    >
                      <X className="w-3 h-3" />
                    </button>
                  </span>
                ))}
              </div>
            </div>
          )}
        </div>
      </div>

      {/* Analysis Results */}
      {currentAnalysis && (
        <>
          {/* Summary Metrics */}
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm font-medium text-gray-600">Total Impact Score</p>
                  <p className="text-2xl font-bold text-gray-900">{currentAnalysis.results.summary_metrics.total_impact_score.toFixed(1)}</p>
                </div>
                <Target className="w-8 h-8 text-blue-600" />
              </div>
              <div className="mt-2 text-xs text-gray-600">
                Confidence: {currentAnalysis.results.summary_metrics.confidence_level}%
              </div>
            </div>

            <div className="bg-white rounded-lg border shadow-sm p-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm font-medium text-gray-600">Risk-Adjusted Impact</p>
                  <p className="text-2xl font-bold text-gray-900">{currentAnalysis.results.summary_metrics.risk_adjusted_impact.toFixed(1)}</p>
                </div>
                <Shield className="w-8 h-8 text-green-600" />
              </div>
              <div className="mt-2 text-xs text-gray-600">
                Time to full impact: {currentAnalysis.results.summary_metrics.time_to_full_impact.toFixed(1)} months
              </div>
            </div>

            <div className="bg-white rounded-lg border shadow-sm p-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm font-medium text-gray-600">Overall Risk Score</p>
                  <p className={clsx('text-2xl font-bold', getRiskColor(currentAnalysis.results.risk_assessment.overall_risk_score))}>
                    {currentAnalysis.results.risk_assessment.overall_risk_score.toFixed(1)}
                  </p>
                </div>
                <AlertTriangle className="w-8 h-8 text-red-600" />
              </div>
              <div className="mt-2 text-xs text-gray-600">
                Low: {currentAnalysis.results.risk_assessment.risk_distribution.low_risk}% •
                Medium: {currentAnalysis.results.risk_assessment.risk_distribution.medium_risk}% •
                High: {currentAnalysis.results.risk_assessment.risk_distribution.high_risk}%
              </div>
            </div>

            <div className="bg-white rounded-lg border shadow-sm p-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm font-medium text-gray-600">Analysis Quality</p>
                  <p className="text-2xl font-bold text-gray-900">{currentAnalysis.metadata.data_quality_score}%</p>
                </div>
                <CheckCircle className="w-8 h-8 text-green-600" />
              </div>
              <div className="mt-2 text-xs text-gray-600">
                Computational time: {currentAnalysis.metadata.computational_time_seconds.toFixed(1)}s
              </div>
            </div>
          </div>

          {/* Analysis Tabs */}
          <div className="border-b border-gray-200">
            <nav className="-mb-px flex space-x-8 overflow-x-auto">
              {[
                { id: 'overview', label: 'Overview', icon: Eye },
                { id: 'temporal', label: 'Temporal Analysis', icon: Calendar },
                { id: 'dimensional', label: 'Dimensional Breakdown', icon: Layers },
                { id: 'correlation', label: 'Correlation Analysis', icon: ScatterChart },
                ...(selectedAnalysisType === 'sensitivity' ? [{ id: 'sensitivity', label: 'Sensitivity Analysis', icon: Activity }] : []),
                ...(selectedAnalysisType === 'optimization' ? [{ id: 'optimization', label: 'Optimization', icon: Lightbulb }] : [])
              ].map(({ id, label, icon: Icon }) => (
                <button
                  key={id}
                  onClick={() => setActiveTab(id as any)}
                  className={clsx(
                    'flex items-center gap-2 py-4 px-1 border-b-2 font-medium text-sm transition-colors whitespace-nowrap',
                    activeTab === id
                      ? 'border-blue-500 text-blue-600'
                      : 'border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300'
                  )}
                >
                  <Icon className="w-4 h-4" />
                  {label}
                </button>
              ))}
            </nav>
          </div>

          {/* Tab Content */}
          <div className="space-y-6">
            {activeTab === 'overview' && (
              <div className="space-y-6">
                {/* Executive Summary */}
                <div className="bg-white rounded-lg border shadow-sm">
                  <div
                    className="p-6 border-b border-gray-200 flex items-center justify-between cursor-pointer"
                    onClick={() => toggleSection('executive-summary')}
                  >
                    <h3 className="text-lg font-semibold text-gray-900">Executive Summary</h3>
                    {expandedSections.has('executive-summary') ? (
                      <ChevronUp className="w-5 h-5 text-gray-400" />
                    ) : (
                      <ChevronDown className="w-5 h-5 text-gray-400" />
                    )}
                  </div>

                  {expandedSections.has('executive-summary') && (
                    <div className="p-6">
                      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                        <div>
                          <h4 className="font-medium text-gray-900 mb-3">Key Findings</h4>
                          <ul className="space-y-2 text-sm text-gray-600">
                            <li>• Overall impact score of {currentAnalysis.results.summary_metrics.total_impact_score.toFixed(1)} with {currentAnalysis.results.summary_metrics.confidence_level}% confidence</li>
                            <li>• Risk-adjusted impact: {currentAnalysis.results.summary_metrics.risk_adjusted_impact.toFixed(1)}</li>
                            <li>• {currentAnalysis.results.risk_assessment.overall_risk_score >= 7 ? 'High' : currentAnalysis.results.risk_assessment.overall_risk_score >= 5 ? 'Medium' : 'Low'} risk profile requiring attention</li>
                            <li>• Full impact realization in {currentAnalysis.results.summary_metrics.time_to_full_impact.toFixed(1)} months</li>
                          </ul>
                        </div>

                        <div>
                          <h4 className="font-medium text-gray-900 mb-3">Critical Insights</h4>
                          <div className="space-y-3">
                            <div className="flex items-start gap-3 p-3 bg-blue-50 border border-blue-200 rounded-lg">
                              <Info className="w-5 h-5 text-blue-600 mt-0.5" />
                              <div className="text-sm text-blue-800">
                                <p><strong>Primary Impact Driver:</strong> Regulatory compliance requirements affecting {currentAnalysis.results.dimensional_breakdown.by_entity_type[0].count} entities</p>
                              </div>
                            </div>

                            <div className="flex items-start gap-3 p-3 bg-yellow-50 border border-yellow-200 rounded-lg">
                              <AlertTriangle className="w-5 h-5 text-yellow-600 mt-0.5" />
                              <div className="text-sm text-yellow-800">
                                <p><strong>Risk Alert:</strong> {currentAnalysis.results.risk_assessment.risk_distribution.critical_risk}% of entities in critical risk category</p>
                              </div>
                            </div>
                          </div>
                        </div>
                      </div>
                    </div>
                  )}
                </div>

                {/* Risk Distribution */}
                <div className="bg-white rounded-lg border shadow-sm p-6">
                  <h3 className="text-lg font-semibold text-gray-900 mb-4">Risk Distribution</h3>
                  <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
                    {Object.entries(currentAnalysis.results.risk_assessment.risk_distribution).map(([level, percentage]) => (
                      <div key={level} className="text-center">
                        <div className={clsx(
                          'text-2xl font-bold mb-1',
                          level === 'critical' ? 'text-red-600' :
                          level === 'high' ? 'text-orange-600' :
                          level === 'medium' ? 'text-yellow-600' : 'text-green-600'
                        )}>
                          {percentage}%
                        </div>
                        <div className="text-sm text-gray-600 capitalize">{level.replace('_', ' ')} Risk</div>
                      </div>
                    ))}
                  </div>
                </div>
              </div>
            )}

            {activeTab === 'temporal' && (
              <div className="space-y-6">
                <div className="bg-white rounded-lg border shadow-sm p-6">
                  <h3 className="text-lg font-semibold text-gray-900 mb-4">Temporal Impact Projection</h3>

                  <div className="space-y-4">
                    <div className="text-sm text-gray-600 mb-4">
                      Projected impact over {timeHorizon} months
                    </div>

                    <div className="overflow-x-auto">
                      <table className="min-w-full divide-y divide-gray-200">
                        <thead className="bg-gray-50">
                          <tr>
                            <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                              Month
                            </th>
                            <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                              Compliance Impact
                            </th>
                            <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                              Operational Impact
                            </th>
                            <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                              Financial Impact
                            </th>
                            <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                              Risk Impact
                            </th>
                          </tr>
                        </thead>
                        <tbody className="bg-white divide-y divide-gray-200">
                          {currentAnalysis.results.temporal_projection.monthly_impact.slice(0, 6).map((month, index) => (
                            <tr key={index}>
                              <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">
                                {format(new Date(month.month), 'MMM yyyy')}
                              </td>
                              <td className="px-6 py-4 whitespace-nowrap text-sm">
                                <span className={getImpactColor(month.compliance_impact)}>
                                  {formatPercentage(month.compliance_impact)}
                                </span>
                              </td>
                              <td className="px-6 py-4 whitespace-nowrap text-sm">
                                <span className={getImpactColor(month.operational_impact)}>
                                  {formatPercentage(month.operational_impact)}
                                </span>
                              </td>
                              <td className="px-6 py-4 whitespace-nowrap text-sm">
                                <span className={month.financial_impact > 0 ? 'text-green-600' : 'text-red-600'}>
                                  {formatCurrency(month.financial_impact)}
                                </span>
                              </td>
                              <td className="px-6 py-4 whitespace-nowrap text-sm">
                                <span className={getImpactColor(month.risk_impact)}>
                                  {formatPercentage(month.risk_impact)}
                                </span>
                              </td>
                            </tr>
                          ))}
                        </tbody>
                      </table>
                    </div>
                  </div>
                </div>
              </div>
            )}

            {activeTab === 'dimensional' && (
              <div className="space-y-6">
                {/* By Regulatory Area */}
                <div className="bg-white rounded-lg border shadow-sm p-6">
                  <h3 className="text-lg font-semibold text-gray-900 mb-4">Impact by Regulatory Area</h3>
                  <div className="space-y-4">
                    {currentAnalysis.results.dimensional_breakdown.by_regulatory_area.map((area, index) => (
                      <div key={index} className="flex items-center justify-between p-4 border border-gray-200 rounded-lg">
                        <div className="flex-1">
                          <h4 className="font-medium text-gray-900">{area.area}</h4>
                          <div className="grid grid-cols-2 gap-4 mt-2 text-sm text-gray-600">
                            <div>Impact Score: <span className="font-medium">{area.impact_score.toFixed(1)}</span></div>
                            <div>Affected Entities: <span className="font-medium">{area.affected_entities}</span></div>
                            <div>Compliance Gap: <span className="font-medium">{area.compliance_gap}%</span></div>
                          </div>
                        </div>
                        <div className="ml-4">
                          <div className="w-24 bg-gray-200 rounded-full h-2">
                            <div
                              className="bg-blue-600 h-2 rounded-full"
                              style={{ width: `${(area.impact_score / 10) * 100}%` }}
                            />
                          </div>
                        </div>
                      </div>
                    ))}
                  </div>
                </div>

                {/* By Entity Type */}
                <div className="bg-white rounded-lg border shadow-sm p-6">
                  <h3 className="text-lg font-semibold text-gray-900 mb-4">Impact by Entity Type</h3>
                  <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                    {currentAnalysis.results.dimensional_breakdown.by_entity_type.map((entity, index) => (
                      <div key={index} className="p-4 border border-gray-200 rounded-lg">
                        <div className="flex items-center justify-between mb-2">
                          <h4 className="font-medium text-gray-900">{entity.entity_type}</h4>
                          <span className="text-sm text-gray-600">{entity.percentage}%</span>
                        </div>
                        <div className="text-sm text-gray-600">
                          Impact Score: <span className="font-medium">{entity.impact_score.toFixed(1)}</span>
                        </div>
                        <div className="text-sm text-gray-600">
                          Count: <span className="font-medium">{entity.count}</span>
                        </div>
                      </div>
                    ))}
                  </div>
                </div>
              </div>
            )}

            {activeTab === 'correlation' && (
              <div className="space-y-6">
                <div className="bg-white rounded-lg border shadow-sm p-6">
                  <h3 className="text-lg font-semibold text-gray-900 mb-4">Impact Correlations</h3>
                  <div className="space-y-4">
                    {currentAnalysis.results.correlation_analysis.impact_correlations.map((correlation, index) => (
                      <div key={index} className="p-4 border border-gray-200 rounded-lg">
                        <div className="flex items-center justify-between mb-2">
                          <h4 className="font-medium text-gray-900">
                            {correlation.factor1} ↔ {correlation.factor2}
                          </h4>
                          <span className={clsx(
                            'px-2 py-1 text-xs font-medium rounded',
                            Math.abs(correlation.correlation_coefficient) > 0.7 ? 'bg-red-100 text-red-800' :
                            Math.abs(correlation.correlation_coefficient) > 0.5 ? 'bg-yellow-100 text-yellow-800' :
                            'bg-green-100 text-green-800'
                          )}>
                            {correlation.correlation_coefficient > 0 ? 'Positive' : 'Negative'} Correlation
                          </span>
                        </div>
                        <div className="grid grid-cols-2 gap-4 text-sm text-gray-600">
                          <div>Coefficient: <span className="font-medium">{correlation.correlation_coefficient.toFixed(3)}</span></div>
                          <div>Significance: <span className="font-medium">{correlation.significance_level}</span></div>
                        </div>
                      </div>
                    ))}
                  </div>
                </div>

                <div className="bg-white rounded-lg border shadow-sm p-6">
                  <h3 className="text-lg font-semibold text-gray-900 mb-4">Key Impact Drivers</h3>
                  <div className="space-y-4">
                    {currentAnalysis.results.correlation_analysis.key_drivers.map((driver, index) => (
                      <div key={index} className="flex items-center justify-between p-4 border border-gray-200 rounded-lg">
                        <div>
                          <h4 className="font-medium text-gray-900 capitalize">{driver.factor.replace('_', ' ')}</h4>
                          <div className="text-sm text-gray-600 mt-1">
                            Impact Weight: {formatPercentage(driver.impact_weight * 100)}
                          </div>
                        </div>
                        <div className="text-right">
                          <div className="text-sm font-medium text-gray-900">
                            {driver.confidence_interval[0].toFixed(2)} - {driver.confidence_interval[1].toFixed(2)}
                          </div>
                          <div className="text-xs text-gray-500">Confidence Interval</div>
                        </div>
                      </div>
                    ))}
                  </div>
                </div>
              </div>
            )}

            {activeTab === 'sensitivity' && currentAnalysis.sensitivity_analysis && (
              <div className="space-y-6">
                <div className="bg-white rounded-lg border shadow-sm p-6">
                  <h3 className="text-lg font-semibold text-gray-900 mb-4">Sensitivity Analysis</h3>

                  {/* Tornado Diagram */}
                  <div className="mb-6">
                    <h4 className="font-medium text-gray-900 mb-4">Tornado Diagram</h4>
                    <div className="space-y-3">
                      {currentAnalysis.sensitivity_analysis.tornado_diagram.map((variable, index) => (
                        <div key={index} className="flex items-center gap-4">
                          <div className="w-32 text-sm text-gray-600 truncate">
                            {variable.variable.replace('_', ' ')}
                          </div>
                          <div className="flex-1 flex items-center">
                            <div className="flex-1 bg-gray-200 rounded h-4 relative">
                              <div
                                className="absolute top-0 left-1/2 transform -translate-x-1/2 w-0.5 h-4 bg-gray-400"
                              />
                              <div
                                className="bg-red-500 h-4 rounded-l"
                                style={{
                                  width: `${Math.abs(variable.low_impact) / (Math.abs(variable.low_impact) + variable.high_impact) * 50}%`,
                                  marginLeft: `${50 - (Math.abs(variable.low_impact) / (Math.abs(variable.low_impact) + variable.high_impact) * 50)}%`
                                }}
                              />
                              <div
                                className="bg-green-500 h-4 rounded-r ml-auto"
                                style={{
                                  width: `${variable.high_impact / (Math.abs(variable.low_impact) + variable.high_impact) * 50}%`
                                }}
                              />
                            </div>
                          </div>
                          <div className="w-16 text-xs text-center">
                            {variable.sensitivity.toFixed(2)}
                          </div>
                        </div>
                      ))}
                    </div>
                  </div>

                  {/* Scenario Analysis */}
                  <div>
                    <h4 className="font-medium text-gray-900 mb-4">Scenario Analysis</h4>
                    <div className="space-y-4">
                      {currentAnalysis.sensitivity_analysis.scenarios.map((scenario, index) => (
                        <div key={index} className="p-4 border border-gray-200 rounded-lg">
                          <div className="flex items-center justify-between mb-3">
                            <h5 className="font-medium text-gray-900">{scenario.scenario_name}</h5>
                            <div className="flex items-center gap-2">
                              <span className={clsx(
                                'px-2 py-1 text-xs font-medium rounded',
                                scenario.impact_change > 0 ? 'bg-green-100 text-green-800' : 'bg-red-100 text-red-800'
                              )}>
                                {formatPercentage(scenario.impact_change)}
                              </span>
                              <span className="text-xs text-gray-600">
                                {formatPercentage(scenario.probability * 100)} probability
                              </span>
                            </div>
                          </div>

                          <div className="text-sm text-gray-600">
                            <strong>Parameter Changes:</strong>
                            {Object.entries(scenario.parameter_changes).map(([param, value]) => (
                              <span key={param} className="ml-2">
                                {param.replace('_', ' ')}: {value}
                              </span>
                            ))}
                          </div>
                        </div>
                      ))}
                    </div>
                  </div>
                </div>
              </div>
            )}

            {activeTab === 'optimization' && currentAnalysis.optimization_recommendations && (
              <div className="space-y-6">
                <div className="bg-white rounded-lg border shadow-sm p-6">
                  <h3 className="text-lg font-semibold text-gray-900 mb-4">Optimization Recommendations</h3>

                  <div className="space-y-4">
                    {currentAnalysis.optimization_recommendations
                      .sort((a, b) => b.priority_score - a.priority_score)
                      .map((rec, index) => (
                      <div key={index} className="p-4 border border-gray-200 rounded-lg">
                        <div className="flex items-start justify-between mb-3">
                          <div className="flex-1">
                            <div className="flex items-center gap-2 mb-2">
                              <h4 className="font-medium text-gray-900">{rec.title}</h4>
                              <span className={clsx(
                                'px-2 py-1 text-xs font-medium rounded',
                                rec.risk_level === 'high' ? 'bg-red-100 text-red-800' :
                                rec.risk_level === 'medium' ? 'bg-yellow-100 text-yellow-800' :
                                'bg-green-100 text-green-800'
                              )}>
                                {rec.risk_level} risk
                              </span>
                            </div>
                            <p className="text-sm text-gray-600 mb-3">{rec.description}</p>

                            <div className="grid grid-cols-2 md:grid-cols-4 gap-4 text-sm">
                              <div>
                                <span className="text-gray-600">Impact Improvement:</span>
                                <span className="ml-1 font-medium text-green-600">
                                  {formatPercentage(rec.impact_improvement)}
                                </span>
                              </div>
                              <div>
                                <span className="text-gray-600">Cost:</span>
                                <span className="ml-1 font-medium">{formatCurrency(rec.implementation_cost)}</span>
                              </div>
                              <div>
                                <span className="text-gray-600">Time:</span>
                                <span className="ml-1 font-medium">{rec.implementation_time_days} days</span>
                              </div>
                              <div>
                                <span className="text-gray-600">Success Rate:</span>
                                <span className="ml-1 font-medium">{formatPercentage(rec.success_probability * 100)}</span>
                              </div>
                            </div>
                          </div>

                          <div className="ml-4 text-right">
                            <div className="text-lg font-bold text-gray-900 mb-1">
                              {rec.priority_score.toFixed(1)}
                            </div>
                            <div className="text-xs text-gray-600">Priority Score</div>
                          </div>
                        </div>

                        {rec.dependencies.length > 0 && (
                          <div className="mt-3 pt-3 border-t border-gray-200">
                            <div className="text-sm text-gray-600">
                              <strong>Dependencies:</strong> {rec.dependencies.join(', ')}
                            </div>
                          </div>
                        )}
                      </div>
                    ))}
                  </div>
                </div>
              </div>
            )}
          </div>
        </>
      )}

      {!currentAnalysis && !isAnalyzing && (
        <div className="bg-white rounded-lg border shadow-sm p-12 text-center">
          <Calculator className="w-12 h-12 mx-auto mb-4 text-gray-400" />
          <h3 className="text-lg font-medium text-gray-900 mb-2">Ready for Analysis</h3>
          <p className="text-sm text-gray-600 mb-4">
            Configure your analysis parameters and click "Run Analysis" to generate advanced impact insights.
          </p>
          <button
            onClick={handleRunAnalysis}
            className="bg-blue-600 hover:bg-blue-700 text-white px-4 py-2 rounded-lg transition-colors"
          >
            Start Analysis
          </button>
        </div>
      )}
    </div>
  );
};

export default ImpactAnalysis;
