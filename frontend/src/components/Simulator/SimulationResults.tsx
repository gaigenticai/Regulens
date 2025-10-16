/**
 * Simulation Results Component
 * Comprehensive results visualization and impact analysis
 * Production-grade results dashboard with advanced analytics
 */

import React, { useState, useEffect, useMemo } from 'react';
import {
  BarChart3,
  TrendingUp,
  TrendingDown,
  Download,
  Share2,
  Eye,
  FileText,
  AlertTriangle,
  CheckCircle,
  XCircle,
  Info,
  Target,
  DollarSign,
  Shield,
  AlertCircle,
  PieChart,
  LineChart,
  Activity,
  Zap,
  Clock,
  Users,
  Building,
  ChevronDown,
  ChevronUp,
  Filter,
  Search,
  RefreshCw,
  Save,
  Copy,
  ExternalLink,
  BookOpen,
  Calculator,
  Lightbulb,
  ArrowUp,
  ArrowDown,
  Minus,
  Settings
} from 'lucide-react';
import { clsx } from 'clsx';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { format, formatDistanceToNow } from 'date-fns';

// Types (matching backend)
export interface SimulationResult {
  result_id: string;
  execution_id: string;
  scenario_id: string;
  user_id: string;
  result_type: 'impact_analysis' | 'compliance_check' | 'risk_assessment';
  impact_summary: any;
  detailed_results: any;
  affected_entities: any;
  recommendations: any;
  risk_assessment: any;
  cost_impact: any;
  compliance_impact: any;
  operational_impact: any;
  created_at: string;
  metadata: any;
}

export interface ImpactMetrics {
  total_entities_affected: number;
  high_risk_entities: number;
  medium_risk_entities: number;
  low_risk_entities: number;
  compliance_score_change: number;
  risk_score_change: number;
  operational_cost_increase: number;
  estimated_implementation_time_days: number;
  critical_violations: string[];
  recommended_actions: string[];
}

interface SimulationResultsProps {
  executionId?: string;
  scenarioId?: string;
  onExport?: (format: 'pdf' | 'csv' | 'json') => void;
  onShare?: (resultId: string) => void;
  onCompare?: (resultIds: string[]) => void;
  onViewRecommendations?: (resultId: string) => void;
  className?: string;
}

const RESULT_TYPES = [
  { value: 'impact_analysis', label: 'Impact Analysis', icon: BarChart3, color: 'bg-blue-100 text-blue-800' },
  { value: 'compliance_check', label: 'Compliance Check', icon: Shield, color: 'bg-green-100 text-green-800' },
  { value: 'risk_assessment', label: 'Risk Assessment', icon: AlertTriangle, color: 'bg-red-100 text-red-800' }
];

const RISK_LEVELS = [
  { level: 'critical', label: 'Critical', color: 'bg-red-500', bgColor: 'bg-red-50', textColor: 'text-red-700' },
  { level: 'high', label: 'High', color: 'bg-orange-500', bgColor: 'bg-orange-50', textColor: 'text-orange-700' },
  { level: 'medium', label: 'Medium', color: 'bg-yellow-500', bgColor: 'bg-yellow-50', textColor: 'text-yellow-700' },
  { level: 'low', label: 'Low', color: 'bg-green-500', bgColor: 'bg-green-50', textColor: 'text-green-700' }
];

const SimulationResults: React.FC<SimulationResultsProps> = ({
  executionId,
  scenarioId,
  onExport,
  onShare,
  onCompare,
  onViewRecommendations,
  className = ''
}) => {
  const [results, setResults] = useState<SimulationResult[]>([]);
  const [selectedResult, setSelectedResult] = useState<SimulationResult | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [activeTab, setActiveTab] = useState<'overview' | 'impact' | 'compliance' | 'risk' | 'cost' | 'recommendations'>('overview');
  const [showFilters, setShowFilters] = useState(false);
  const [searchTerm, setSearchTerm] = useState('');
  const [resultTypeFilter, setResultTypeFilter] = useState<string>('all');
  const [expandedSections, setExpandedSections] = useState<Set<string>>(new Set(['summary', 'metrics']));

  // Mock results data (replace with real API calls)
  useEffect(() => {
    const loadResults = async () => {
      setIsLoading(true);
      try {
        // Simulate API calls
        await new Promise(resolve => setTimeout(resolve, 1000));

        // Mock simulation results
        const mockResults: SimulationResult[] = [
          {
            result_id: 'result-001',
            execution_id: executionId || 'exec-001',
            scenario_id: scenarioId || 'scenario-001',
            user_id: 'user-001',
            result_type: 'impact_analysis',
            impact_summary: {
              total_entities_affected: 1250,
              high_risk_entities: 45,
              medium_risk_entities: 156,
              low_risk_entities: 1049,
              compliance_score_change: -2.3,
              risk_score_change: 15.7,
              operational_cost_increase: 285000,
              estimated_implementation_time_days: 90
            },
            detailed_results: {
              regulatory_changes: [
                {
                  change_id: 'change-001',
                  description: 'Enhanced KYC verification requirements',
                  impact_score: 8.5,
                  affected_entities: 450,
                  compliance_impact: 'high',
                  cost_impact: 125000
                },
                {
                  change_id: 'change-002',
                  description: 'New transaction monitoring thresholds',
                  impact_score: 6.2,
                  affected_entities: 800,
                  compliance_impact: 'medium',
                  cost_impact: 160000
                }
              ],
              entity_breakdown: {
                customers: 650,
                accounts: 420,
                transactions: 180,
                processes: 95
              }
            },
            affected_entities: {
              customers: 650,
              accounts: 420,
              transactions: 180,
              processes: 95
            },
            recommendations: [
              {
                priority: 'high',
                category: 'implementation',
                title: 'Implement automated KYC verification',
                description: 'Deploy automated KYC verification system to reduce manual processing time by 60%',
                estimated_savings: 95000,
                implementation_time_days: 45
              },
              {
                priority: 'medium',
                category: 'training',
                title: 'Staff training on new requirements',
                description: 'Provide comprehensive training for compliance staff on new regulatory requirements',
                estimated_savings: 0,
                implementation_time_days: 14
              },
              {
                priority: 'low',
                category: 'monitoring',
                title: 'Enhanced monitoring system',
                description: 'Upgrade transaction monitoring system with new AI-based anomaly detection',
                estimated_savings: 75000,
                implementation_time_days: 60
              }
            ],
            risk_assessment: {
              overall_risk_score: 7.2,
              risk_trends: 'increasing',
              critical_risks: [
                'Non-compliance penalties up to $2.5M',
                'Reputational damage from regulatory violations',
                'Operational disruptions during implementation'
              ],
              risk_mitigation: [
                'Implement phased rollout approach',
                'Establish dedicated compliance task force',
                'Regular progress reporting to senior management'
              ]
            },
            cost_impact: {
              one_time_costs: 285000,
              recurring_costs: 125000,
              total_cost: 410000,
              cost_breakdown: {
                technology: 180000,
                personnel: 95000,
                training: 25000,
                compliance: 110000
              },
              roi_projection: {
                year_1: -15,
                year_2: 25,
                year_3: 45,
                breakeven_months: 18
              }
            },
            compliance_impact: {
              current_compliance_score: 87.3,
              projected_compliance_score: 89.6,
              compliance_gaps: [
                'Enhanced customer verification processes',
                'Transaction monitoring capabilities',
                'Reporting and documentation requirements'
              ],
              remediation_steps: [
                'Update customer onboarding workflows',
                'Enhance transaction monitoring rules',
                'Implement automated reporting systems'
              ]
            },
            operational_impact: {
              process_changes: 12,
              system_updates: 8,
              training_required: 145,
              estimated_downtime_hours: 24,
              productivity_impact: -8.5
            },
            created_at: new Date(Date.now() - 3600000).toISOString(),
            metadata: {
              execution_duration_seconds: 1250,
              data_quality_score: 94.2,
              confidence_level: 87.5
            }
          }
        ];

        setResults(mockResults);
        setSelectedResult(mockResults[0]);
      } catch (error) {
        console.error('Failed to load results:', error);
      } finally {
        setIsLoading(false);
      }
    };

    loadResults();
  }, [executionId, scenarioId]);

  const filteredResults = results.filter(result => {
    const matchesSearch = result.result_id.toLowerCase().includes(searchTerm.toLowerCase());
    const matchesType = resultTypeFilter === 'all' || result.result_type === resultTypeFilter;
    return matchesSearch && matchesType;
  });

  const toggleSection = (section: string) => {
    const newExpanded = new Set(expandedSections);
    if (newExpanded.has(section)) {
      newExpanded.delete(section);
    } else {
      newExpanded.add(section);
    }
    setExpandedSections(newExpanded);
  };

  const getResultTypeInfo = (type: string) => {
    return RESULT_TYPES.find(t => t.value === type) || RESULT_TYPES[0];
  };

  const getRiskLevelInfo = (level: string) => {
    return RISK_LEVELS.find(r => r.level === level) || RISK_LEVELS[3];
  };

  const formatCurrency = (amount: number): string => {
    return new Intl.NumberFormat('en-US', {
      style: 'currency',
      currency: 'USD',
      minimumFractionDigits: 0,
      maximumFractionDigits: 0
    }).format(amount);
  };

  const formatPercentage = (value: number): string => {
    return `${value > 0 ? '+' : ''}${value.toFixed(1)}%`;
  };

  const getPriorityColor = (priority: string): string => {
    switch (priority) {
      case 'high': return 'bg-red-100 text-red-800';
      case 'medium': return 'bg-yellow-100 text-yellow-800';
      case 'low': return 'bg-green-100 text-green-800';
      default: return 'bg-gray-100 text-gray-800';
    }
  };

  const renderMetricCard = (title: string, value: string | number, change?: number, icon: React.ComponentType<{ className?: string }>, color: string) => (
    <div className="bg-white rounded-lg border shadow-sm p-6">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-3">
          <div className={clsx('p-2 rounded-lg', color)}>
            <icon className="w-5 h-5 text-white" />
          </div>
          <div>
            <p className="text-sm font-medium text-gray-600">{title}</p>
            <p className="text-2xl font-bold text-gray-900">{value}</p>
          </div>
        </div>
        {change !== undefined && (
          <div className="flex items-center gap-1">
            {change > 0 ? (
              <ArrowUp className="w-4 h-4 text-green-500" />
            ) : change < 0 ? (
              <ArrowDown className="w-4 h-4 text-red-500" />
            ) : (
              <Minus className="w-4 h-4 text-gray-500" />
            )}
            <span className={clsx(
              'text-sm font-medium',
              change > 0 ? 'text-green-600' : change < 0 ? 'text-red-600' : 'text-gray-600'
            )}>
              {formatPercentage(change)}
            </span>
          </div>
        )}
      </div>
    </div>
  );

  if (isLoading) {
    return (
      <div className="flex items-center justify-center min-h-96">
        <LoadingSpinner />
      </div>
    );
  }

  if (!selectedResult) {
    return (
      <div className="bg-white rounded-lg border shadow-sm p-12 text-center">
        <BarChart3 className="w-12 h-12 mx-auto mb-4 text-gray-400" />
        <h3 className="text-lg font-medium text-gray-900 mb-2">No Results Available</h3>
        <p className="text-sm text-gray-600">
          Run a simulation to generate results and analysis.
        </p>
      </div>
    );
  }

  const impactSummary = selectedResult.impact_summary;

  return (
    <div className={clsx('space-y-6', className)}>
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold text-gray-900 flex items-center gap-3">
            <BarChart3 className="w-8 h-8 text-blue-600" />
            Simulation Results
          </h1>
          <p className="text-gray-600 mt-1">
            Comprehensive analysis of regulatory impact simulation
          </p>
        </div>

        <div className="flex items-center gap-3">
          <button
            onClick={() => onShare?.(selectedResult.result_id)}
            className="flex items-center gap-2 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors"
          >
            <Share2 className="w-4 h-4" />
            Share
          </button>
          <div className="flex items-center gap-2">
            <button
              onClick={() => onExport?.('pdf')}
              className="flex items-center gap-2 px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors"
            >
              <Download className="w-4 h-4" />
              Export PDF
            </button>
          </div>
        </div>
      </div>

      {/* Results Navigation */}
      <div className="bg-white rounded-lg border shadow-sm p-4">
        <div className="flex items-center gap-4 overflow-x-auto">
          {filteredResults.map((result) => {
            const isSelected = selectedResult?.result_id === result.result_id;
            const typeInfo = getResultTypeInfo(result.result_type);
            const TypeIcon = typeInfo.icon;

            return (
              <button
                key={result.result_id}
                onClick={() => setSelectedResult(result)}
                className={clsx(
                  'flex items-center gap-2 px-4 py-2 rounded-lg transition-colors whitespace-nowrap',
                  isSelected
                    ? 'bg-blue-600 text-white'
                    : 'bg-gray-100 hover:bg-gray-200 text-gray-700'
                )}
              >
                <TypeIcon className="w-4 h-4" />
                <span className="text-sm font-medium">{typeInfo.label}</span>
                <span className="text-xs opacity-75">
                  {format(new Date(result.created_at), 'MMM dd, HH:mm')}
                </span>
              </button>
            );
          })}
        </div>
      </div>

      {/* Tabs */}
      <div className="border-b border-gray-200">
        <nav className="-mb-px flex space-x-8 overflow-x-auto">
          {[
            { id: 'overview', label: 'Overview', icon: Eye },
            { id: 'impact', label: 'Impact Analysis', icon: BarChart3 },
            { id: 'compliance', label: 'Compliance', icon: Shield },
            { id: 'risk', label: 'Risk Assessment', icon: AlertTriangle },
            { id: 'cost', label: 'Cost Impact', icon: DollarSign },
            { id: 'recommendations', label: 'Recommendations', icon: Lightbulb }
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
            {/* Key Metrics */}
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
              {renderMetricCard(
                'Entities Affected',
                impactSummary.total_entities_affected.toLocaleString(),
                undefined,
                Users,
                'bg-blue-500'
              )}
              {renderMetricCard(
                'Compliance Change',
                formatPercentage(impactSummary.compliance_score_change),
                impactSummary.compliance_score_change,
                Shield,
                'bg-green-500'
              )}
              {renderMetricCard(
                'Risk Change',
                formatPercentage(impactSummary.risk_score_change),
                impactSummary.risk_score_change,
                AlertTriangle,
                'bg-red-500'
              )}
              {renderMetricCard(
                'Cost Impact',
                formatCurrency(impactSummary.operational_cost_increase),
                undefined,
                DollarSign,
                'bg-yellow-500'
              )}
            </div>

            {/* Executive Summary */}
            <div className="bg-white rounded-lg border shadow-sm">
              <div
                className="p-6 border-b border-gray-200 flex items-center justify-between cursor-pointer"
                onClick={() => toggleSection('summary')}
              >
                <h3 className="text-lg font-semibold text-gray-900">Executive Summary</h3>
                {expandedSections.has('summary') ? (
                  <ChevronUp className="w-5 h-5 text-gray-400" />
                ) : (
                  <ChevronDown className="w-5 h-5 text-gray-400" />
                )}
              </div>

              {expandedSections.has('summary') && (
                <div className="p-6 space-y-4">
                  <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                    <div>
                      <h4 className="font-medium text-gray-900 mb-2">Impact Overview</h4>
                      <div className="space-y-2 text-sm text-gray-600">
                        <p>• <strong>{impactSummary.total_entities_affected.toLocaleString()}</strong> entities will be affected by the regulatory changes</p>
                        <p>• Compliance score will <strong>{impactSummary.compliance_score_change > 0 ? 'improve' : 'decrease'}</strong> by <strong>{Math.abs(impactSummary.compliance_score_change)}%</strong></p>
                        <p>• Risk exposure will <strong>{impactSummary.risk_score_change > 0 ? 'increase' : 'decrease'}</strong> by <strong>{Math.abs(impactSummary.risk_score_change)}%</strong></p>
                        <p>• Implementation will require approximately <strong>{impactSummary.estimated_implementation_time_days} days</strong></p>
                      </div>
                    </div>

                    <div>
                      <h4 className="font-medium text-gray-900 mb-2">Risk Distribution</h4>
                      <div className="space-y-2">
                        <div className="flex justify-between items-center">
                          <span className="text-sm text-red-600">High Risk</span>
                          <span className="font-medium">{impactSummary.high_risk_entities}</span>
                        </div>
                        <div className="flex justify-between items-center">
                          <span className="text-sm text-orange-600">Medium Risk</span>
                          <span className="font-medium">{impactSummary.medium_risk_entities}</span>
                        </div>
                        <div className="flex justify-between items-center">
                          <span className="text-sm text-yellow-600">Low Risk</span>
                          <span className="font-medium">{impactSummary.low_risk_entities}</span>
                        </div>
                      </div>
                    </div>
                  </div>

                  <div className="mt-6">
                    <h4 className="font-medium text-gray-900 mb-2">Key Findings</h4>
                    <div className="bg-blue-50 border border-blue-200 rounded-lg p-4">
                      <div className="flex items-start gap-3">
                        <Info className="w-5 h-5 text-blue-600 mt-0.5" />
                        <div className="text-sm text-blue-800">
                          <p>The simulation indicates moderate impact on operational processes with opportunities for compliance improvement.
                          Priority should be given to high-risk entities and critical regulatory requirements.</p>
                        </div>
                      </div>
                    </div>
                  </div>
                </div>
              )}
            </div>

            {/* Detailed Metrics */}
            <div className="bg-white rounded-lg border shadow-sm">
              <div
                className="p-6 border-b border-gray-200 flex items-center justify-between cursor-pointer"
                onClick={() => toggleSection('metrics')}
              >
                <h3 className="text-lg font-semibold text-gray-900">Detailed Metrics</h3>
                {expandedSections.has('metrics') ? (
                  <ChevronUp className="w-5 h-5 text-gray-400" />
                ) : (
                  <ChevronDown className="w-5 h-5 text-gray-400" />
                )}
              </div>

              {expandedSections.has('metrics') && (
                <div className="p-6">
                  <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
                    <div className="space-y-4">
                      <h4 className="font-medium text-gray-900">Entity Breakdown</h4>
                      {Object.entries(selectedResult.affected_entities).map(([entity, count]) => (
                        <div key={entity} className="flex justify-between items-center">
                          <span className="text-sm text-gray-600 capitalize">{entity}</span>
                          <span className="font-medium">{count as number}</span>
                        </div>
                      ))}
                    </div>

                    <div className="space-y-4">
                      <h4 className="font-medium text-gray-900">Operational Impact</h4>
                      <div className="space-y-2">
                        <div className="flex justify-between items-center">
                          <span className="text-sm text-gray-600">Process Changes</span>
                          <span className="font-medium">{selectedResult.operational_impact.process_changes}</span>
                        </div>
                        <div className="flex justify-between items-center">
                          <span className="text-sm text-gray-600">System Updates</span>
                          <span className="font-medium">{selectedResult.operational_impact.system_updates}</span>
                        </div>
                        <div className="flex justify-between items-center">
                          <span className="text-sm text-gray-600">Training Required</span>
                          <span className="font-medium">{selectedResult.operational_impact.training_required}</span>
                        </div>
                      </div>
                    </div>

                    <div className="space-y-4">
                      <h4 className="font-medium text-gray-900">Metadata</h4>
                      <div className="space-y-2">
                        <div className="flex justify-between items-center">
                          <span className="text-sm text-gray-600">Execution Time</span>
                          <span className="font-medium">{selectedResult.metadata.execution_duration_seconds}s</span>
                        </div>
                        <div className="flex justify-between items-center">
                          <span className="text-sm text-gray-600">Data Quality</span>
                          <span className="font-medium">{selectedResult.metadata.data_quality_score}%</span>
                        </div>
                        <div className="flex justify-between items-center">
                          <span className="text-sm text-gray-600">Confidence</span>
                          <span className="font-medium">{selectedResult.metadata.confidence_level}%</span>
                        </div>
                      </div>
                    </div>
                  </div>
                </div>
              )}
            </div>
          </div>
        )}

        {activeTab === 'impact' && (
          <div className="space-y-6">
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <h3 className="text-lg font-semibold text-gray-900 mb-6">Detailed Impact Analysis</h3>

              <div className="space-y-6">
                {selectedResult.detailed_results.regulatory_changes.map((change: any, index: number) => (
                  <div key={index} className="border border-gray-200 rounded-lg p-4">
                    <div className="flex items-center justify-between mb-3">
                      <h4 className="font-medium text-gray-900">{change.description}</h4>
                      <div className="flex items-center gap-2">
                        <span className={clsx(
                          'px-2 py-1 text-xs font-medium rounded',
                          change.compliance_impact === 'high' ? 'bg-red-100 text-red-800' :
                          change.compliance_impact === 'medium' ? 'bg-yellow-100 text-yellow-800' :
                          'bg-green-100 text-green-800'
                        )}>
                          {change.compliance_impact} impact
                        </span>
                        <span className="text-sm font-medium text-gray-600">
                          Score: {change.impact_score}/10
                        </span>
                      </div>
                    </div>

                    <div className="grid grid-cols-2 md:grid-cols-4 gap-4 text-sm">
                      <div>
                        <span className="text-gray-600">Affected Entities:</span>
                        <span className="ml-2 font-medium">{change.affected_entities}</span>
                      </div>
                      <div>
                        <span className="text-gray-600">Cost Impact:</span>
                        <span className="ml-2 font-medium">{formatCurrency(change.cost_impact)}</span>
                      </div>
                      <div>
                        <span className="text-gray-600">Risk Level:</span>
                        <span className="ml-2 font-medium capitalize">{change.compliance_impact}</span>
                      </div>
                      <div>
                        <span className="text-gray-600">Priority:</span>
                        <span className="ml-2 font-medium">High</span>
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          </div>
        )}

        {activeTab === 'compliance' && (
          <div className="space-y-6">
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <h3 className="text-lg font-semibold text-gray-900 mb-6">Compliance Impact Analysis</h3>

              <div className="grid grid-cols-1 md:grid-cols-2 gap-6 mb-6">
                <div className="text-center">
                  <div className="text-3xl font-bold text-gray-900 mb-2">
                    {selectedResult.compliance_impact.current_compliance_score}%
                  </div>
                  <div className="text-sm text-gray-600">Current Score</div>
                </div>
                <div className="text-center">
                  <div className="text-3xl font-bold text-green-600 mb-2">
                    {selectedResult.compliance_impact.projected_compliance_score}%
                  </div>
                  <div className="text-sm text-gray-600">Projected Score</div>
                </div>
              </div>

              <div className="space-y-6">
                <div>
                  <h4 className="font-medium text-gray-900 mb-3">Compliance Gaps Identified</h4>
                  <div className="space-y-2">
                    {selectedResult.compliance_impact.compliance_gaps.map((gap: string, index: number) => (
                      <div key={index} className="flex items-center gap-3 p-3 bg-red-50 border border-red-200 rounded-lg">
                        <AlertCircle className="w-5 h-5 text-red-600" />
                        <span className="text-sm text-red-800">{gap}</span>
                      </div>
                    ))}
                  </div>
                </div>

                <div>
                  <h4 className="font-medium text-gray-900 mb-3">Remediation Steps</h4>
                  <div className="space-y-2">
                    {selectedResult.compliance_impact.remediation_steps.map((step: string, index: number) => (
                      <div key={index} className="flex items-center gap-3 p-3 bg-green-50 border border-green-200 rounded-lg">
                        <CheckCircle className="w-5 h-5 text-green-600" />
                        <span className="text-sm text-green-800">{step}</span>
                      </div>
                    ))}
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'risk' && (
          <div className="space-y-6">
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <h3 className="text-lg font-semibold text-gray-900 mb-6">Risk Assessment</h3>

              <div className="text-center mb-6">
                <div className="text-4xl font-bold text-gray-900 mb-2">
                  {selectedResult.risk_assessment.overall_risk_score}/10
                </div>
                <div className="text-sm text-gray-600">Overall Risk Score</div>
                <div className="text-sm font-medium text-red-600 mt-1">
                  Risk trend: {selectedResult.risk_assessment.risk_trends}
                </div>
              </div>

              <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                <div>
                  <h4 className="font-medium text-gray-900 mb-3">Critical Risks</h4>
                  <div className="space-y-2">
                    {selectedResult.risk_assessment.critical_risks.map((risk: string, index: number) => (
                      <div key={index} className="flex items-start gap-3 p-3 bg-red-50 border border-red-200 rounded-lg">
                        <AlertTriangle className="w-5 h-5 text-red-600 mt-0.5" />
                        <span className="text-sm text-red-800">{risk}</span>
                      </div>
                    ))}
                  </div>
                </div>

                <div>
                  <h4 className="font-medium text-gray-900 mb-3">Risk Mitigation Strategies</h4>
                  <div className="space-y-2">
                    {selectedResult.risk_assessment.risk_mitigation.map((strategy: string, index: number) => (
                      <div key={index} className="flex items-start gap-3 p-3 bg-blue-50 border border-blue-200 rounded-lg">
                        <Shield className="w-5 h-5 text-blue-600 mt-0.5" />
                        <span className="text-sm text-blue-800">{strategy}</span>
                      </div>
                    ))}
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'cost' && (
          <div className="space-y-6">
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <h3 className="text-lg font-semibold text-gray-900 mb-6">Cost Impact Analysis</h3>

              <div className="grid grid-cols-1 md:grid-cols-3 gap-6 mb-6">
                <div className="text-center">
                  <div className="text-2xl font-bold text-gray-900 mb-1">
                    {formatCurrency(selectedResult.cost_impact.one_time_costs)}
                  </div>
                  <div className="text-sm text-gray-600">One-time Costs</div>
                </div>
                <div className="text-center">
                  <div className="text-2xl font-bold text-gray-900 mb-1">
                    {formatCurrency(selectedResult.cost_impact.recurring_costs)}
                  </div>
                  <div className="text-sm text-gray-600">Recurring Costs</div>
                </div>
                <div className="text-center">
                  <div className="text-2xl font-bold text-red-600 mb-1">
                    {formatCurrency(selectedResult.cost_impact.total_cost)}
                  </div>
                  <div className="text-sm text-gray-600">Total Cost</div>
                </div>
              </div>

              <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                <div>
                  <h4 className="font-medium text-gray-900 mb-3">Cost Breakdown</h4>
                  <div className="space-y-2">
                    {Object.entries(selectedResult.cost_impact.cost_breakdown).map(([category, amount]) => (
                      <div key={category} className="flex justify-between items-center">
                        <span className="text-sm text-gray-600 capitalize">{category}</span>
                        <span className="font-medium">{formatCurrency(amount as number)}</span>
                      </div>
                    ))}
                  </div>
                </div>

                <div>
                  <h4 className="font-medium text-gray-900 mb-3">ROI Projection</h4>
                  <div className="space-y-2">
                    {Object.entries(selectedResult.cost_impact.roi_projection).map(([period, roi]) => (
                      <div key={period} className="flex justify-between items-center">
                        <span className="text-sm text-gray-600">
                          {period.replace('_', ' ').replace('year ', 'Year ')}
                        </span>
                        <span className={clsx(
                          'font-medium',
                          (roi as number) > 0 ? 'text-green-600' : 'text-red-600'
                        )}>
                          {formatPercentage(roi as number)}
                        </span>
                      </div>
                    ))}
                  </div>
                  <div className="mt-3 p-3 bg-blue-50 border border-blue-200 rounded-lg">
                    <div className="text-sm text-blue-800">
                      <strong>Breakeven:</strong> {selectedResult.cost_impact.roi_projection.breakeven_months} months
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'recommendations' && (
          <div className="space-y-6">
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <h3 className="text-lg font-semibold text-gray-900 mb-6">Implementation Recommendations</h3>

              <div className="space-y-4">
                {selectedResult.recommendations.map((rec: any, index: number) => (
                  <div key={index} className="border border-gray-200 rounded-lg p-4">
                    <div className="flex items-start justify-between mb-3">
                      <div className="flex-1">
                        <div className="flex items-center gap-2 mb-2">
                          <h4 className="font-medium text-gray-900">{rec.title}</h4>
                          <span className={clsx(
                            'px-2 py-1 text-xs font-medium rounded',
                            getPriorityColor(rec.priority)
                          )}>
                            {rec.priority} priority
                          </span>
                        </div>
                        <p className="text-sm text-gray-600 mb-3">{rec.description}</p>

                        <div className="grid grid-cols-2 md:grid-cols-4 gap-4 text-sm">
                          <div>
                            <span className="text-gray-600">Category:</span>
                            <span className="ml-1 font-medium capitalize">{rec.category.replace('_', ' ')}</span>
                          </div>
                          <div>
                            <span className="text-gray-600">Time:</span>
                            <span className="ml-1 font-medium">{rec.implementation_time_days} days</span>
                          </div>
                          <div>
                            <span className="text-gray-600">Savings:</span>
                            <span className="ml-1 font-medium text-green-600">
                              {formatCurrency(rec.estimated_savings)}
                            </span>
                          </div>
                          <div>
                            <span className="text-gray-600">ROI:</span>
                            <span className="ml-1 font-medium">
                              {rec.estimated_savings > 0 ? formatPercentage((rec.estimated_savings / selectedResult.cost_impact.total_cost) * 100) : 'N/A'}
                            </span>
                          </div>
                        </div>
                      </div>
                    </div>

                    <div className="flex items-center gap-2">
                      <button
                        onClick={() => onViewRecommendations?.(selectedResult.result_id)}
                        className="text-blue-600 hover:text-blue-800 text-sm font-medium"
                      >
                        View Implementation Plan →
                      </button>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default SimulationResults;
