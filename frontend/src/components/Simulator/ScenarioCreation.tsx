/**
 * Scenario Creation Component
 * Comprehensive form for creating regulatory simulation scenarios
 * Production-grade scenario creation with validation and templates
 */

import React, { useState, useEffect, useMemo } from 'react';
import {
  Save,
  Play,
  Eye,
  X,
  Plus,
  Minus,
  AlertTriangle,
  CheckCircle,
  Info,
  Settings,
  FileText,
  Code,
  Upload,
  Download,
  RefreshCw,
  ChevronDown,
  ChevronUp,
  Zap,
  Target,
  BarChart3,
  Clock,
  Shield,
  AlertCircle,
  Layers,
  GitBranch,
  Copy,
  Trash2
} from 'lucide-react';
import { clsx } from 'clsx';
import { LoadingSpinner } from '@/components/LoadingSpinner';

// Types (matching backend)
export interface SimulationScenario {
  scenario_id?: string;
  scenario_name: string;
  description: string;
  scenario_type: 'regulatory_change' | 'market_change' | 'operational_change';
  regulatory_changes: any;
  impact_parameters: any;
  baseline_data: any;
  test_data: any;
  created_by?: string;
  created_at?: string;
  updated_at?: string;
  is_template?: boolean;
  is_active?: boolean;
  tags: string[];
  metadata: any;
  estimated_runtime_seconds: number;
  max_concurrent_simulations: number;
  version?: string;
  parent_scenario_id?: string;
  approval_status?: 'draft' | 'pending_review' | 'approved' | 'rejected';
}

export interface SimulationTemplate {
  template_id: string;
  template_name: string;
  template_description: string;
  category: 'aml' | 'kyc' | 'fraud' | 'privacy' | 'reporting';
  jurisdiction: string;
  regulatory_body: string;
  template_data: any;
  usage_count: number;
  success_rate: number;
  average_runtime_seconds: number;
  created_by: string;
  created_at: string;
  is_active: boolean;
  tags: string[];
}

interface ScenarioCreationProps {
  initialScenario?: Partial<SimulationScenario>;
  template?: SimulationTemplate;
  onSave?: (scenario: SimulationScenario) => Promise<void>;
  onTest?: (scenario: SimulationScenario) => Promise<any>;
  onPreview?: (scenario: SimulationScenario) => void;
  onCancel?: () => void;
  className?: string;
}

const SCENARIO_TYPES = [
  {
    value: 'regulatory_change' as const,
    label: 'Regulatory Change',
    description: 'Simulate the impact of new or modified regulations',
    icon: Shield,
    color: 'bg-blue-100 text-blue-800'
  },
  {
    value: 'market_change' as const,
    label: 'Market Change',
    description: 'Analyze market condition changes and their effects',
    icon: BarChart3,
    color: 'bg-green-100 text-green-800'
  },
  {
    value: 'operational_change' as const,
    label: 'Operational Change',
    description: 'Test operational process modifications',
    icon: Settings,
    color: 'bg-purple-100 text-purple-800'
  }
];

const REGULATORY_BODIES = [
  'SEC', 'FINRA', 'ECB', 'FCA', 'MAS', 'ASIC', 'OSFI', 'BoE', 'Fed', 'OCC'
];

const JURISDICTIONS = [
  'United States', 'European Union', 'United Kingdom', 'Singapore', 'Australia', 'Canada', 'Japan', 'Switzerland'
];

const ScenarioCreation: React.FC<ScenarioCreationProps> = ({
  initialScenario,
  template,
  onSave,
  onTest,
  onPreview,
  onCancel,
  className = ''
}) => {
  const [activeStep, setActiveStep] = useState(1);
  const [isLoading, setIsLoading] = useState(false);
  const [isTesting, setIsTesting] = useState(false);
  const [testResults, setTestResults] = useState<any>(null);
  const [validationErrors, setValidationErrors] = useState<Record<string, string>>({});
  const [showAdvanced, setShowAdvanced] = useState(false);
  const [jsonEditorMode, setJsonEditorMode] = useState<'form' | 'json'>('form');

  const [scenario, setScenario] = useState<SimulationScenario>({
    scenario_name: initialScenario?.scenario_name || template?.template_name || '',
    description: initialScenario?.description || template?.template_description || '',
    scenario_type: initialScenario?.scenario_type || template ? 'regulatory_change' : 'regulatory_change',
    regulatory_changes: initialScenario?.regulatory_changes || template?.template_data?.regulatory_changes || {
      jurisdiction: '',
      regulatory_body: '',
      regulation_type: '',
      effective_date: '',
      changes: []
    },
    impact_parameters: initialScenario?.impact_parameters || template?.template_data?.impact_parameters || {
      risk_threshold: 0.7,
      compliance_weight: 0.8,
      operational_weight: 0.6,
      cost_weight: 0.4,
      time_horizon_days: 365
    },
    baseline_data: initialScenario?.baseline_data || template?.template_data?.baseline_data || {
      current_risk_score: 0.3,
      current_compliance_score: 0.85,
      current_operational_efficiency: 0.75,
      current_cost_baseline: 1000000
    },
    test_data: initialScenario?.test_data || template?.template_data?.test_data || {
      test_transactions: [],
      test_entities: [],
      test_scenarios: []
    },
    tags: initialScenario?.tags || template?.tags || [],
    metadata: initialScenario?.metadata || template?.template_data?.metadata || {},
    estimated_runtime_seconds: initialScenario?.estimated_runtime_seconds || template?.average_runtime_seconds || 300,
    max_concurrent_simulations: initialScenario?.max_concurrent_simulations || 5,
    version: initialScenario?.version || '1.0',
    is_template: initialScenario?.is_template || false,
    is_active: initialScenario?.is_active ?? true,
    approval_status: initialScenario?.approval_status || 'draft'
  });

  const [newTag, setNewTag] = useState('');

  const steps = [
    { id: 1, title: 'Basic Information', description: 'Scenario name and type' },
    { id: 2, title: 'Regulatory Changes', description: 'Define regulatory modifications' },
    { id: 3, title: 'Impact Parameters', description: 'Configure simulation parameters' },
    { id: 4, title: 'Data Configuration', description: 'Baseline and test data' },
    { id: 5, title: 'Review & Test', description: 'Validate and test scenario' }
  ];

  const validateStep = (step: number): boolean => {
    const errors: Record<string, string> = {};

    switch (step) {
      case 1:
        if (!scenario.scenario_name.trim()) {
          errors.scenario_name = 'Scenario name is required';
        }
        if (!scenario.description.trim()) {
          errors.description = 'Description is required';
        }
        break;
      case 2:
        if (!scenario.regulatory_changes.jurisdiction) {
          errors.jurisdiction = 'Jurisdiction is required';
        }
        if (!scenario.regulatory_changes.regulatory_body) {
          errors.regulatory_body = 'Regulatory body is required';
        }
        break;
      case 3:
        if (scenario.impact_parameters.risk_threshold < 0 || scenario.impact_parameters.risk_threshold > 1) {
          errors.risk_threshold = 'Risk threshold must be between 0 and 1';
        }
        if (scenario.estimated_runtime_seconds < 60) {
          errors.runtime = 'Runtime must be at least 60 seconds';
        }
        break;
      case 4:
        // Basic validation for data configuration
        if (!scenario.baseline_data || Object.keys(scenario.baseline_data).length === 0) {
          errors.baseline_data = 'Baseline data is required';
        }
        break;
    }

    setValidationErrors(errors);
    return Object.keys(errors).length === 0;
  };

  const handleNext = () => {
    if (validateStep(activeStep)) {
      setActiveStep(prev => Math.min(prev + 1, steps.length));
    }
  };

  const handlePrevious = () => {
    setActiveStep(prev => Math.max(prev - 1, 1));
  };

  const handleSave = async () => {
    if (!validateStep(activeStep)) return;

    setIsLoading(true);
    try {
      if (onSave) {
        await onSave(scenario);
      }
    } catch (error) {
      console.error('Failed to save scenario:', error);
    } finally {
      setIsLoading(false);
    }
  };

  const handleTest = async () => {
    setIsTesting(true);
    setTestResults(null);
    try {
      if (onTest) {
        const results = await onTest(scenario);
        setTestResults(results);
      }
    } catch (error) {
      setTestResults({ error: error.message });
    } finally {
      setIsTesting(false);
    }
  };

  const addTag = () => {
    if (newTag.trim() && !scenario.tags.includes(newTag.trim())) {
      setScenario(prev => ({
        ...prev,
        tags: [...prev.tags, newTag.trim()]
      }));
      setNewTag('');
    }
  };

  const removeTag = (tagToRemove: string) => {
    setScenario(prev => ({
      ...prev,
      tags: prev.tags.filter(tag => tag !== tagToRemove)
    }));
  };

  const updateRegulatoryChange = (field: string, value: any) => {
    setScenario(prev => ({
      ...prev,
      regulatory_changes: {
        ...prev.regulatory_changes,
        [field]: value
      }
    }));
  };

  const updateImpactParameter = (field: string, value: any) => {
    setScenario(prev => ({
      ...prev,
      impact_parameters: {
        ...prev.impact_parameters,
        [field]: value
      }
    }));
  };

  const updateBaselineData = (field: string, value: any) => {
    setScenario(prev => ({
      ...prev,
      baseline_data: {
        ...prev.baseline_data,
        [field]: value
      }
    }));
  };

  const isStepValid = (step: number): boolean => {
    // Basic validation logic
    switch (step) {
      case 1:
        return !!(scenario.scenario_name.trim() && scenario.description.trim());
      case 2:
        return !!(scenario.regulatory_changes.jurisdiction && scenario.regulatory_changes.regulatory_body);
      case 3:
        return scenario.impact_parameters.risk_threshold >= 0 && scenario.impact_parameters.risk_threshold <= 1;
      case 4:
        return !!(scenario.baseline_data && Object.keys(scenario.baseline_data).length > 0);
      case 5:
        return true; // Review step is always valid
      default:
        return false;
    }
  };

  const renderStepContent = () => {
    switch (activeStep) {
      case 1:
        return (
          <div className="space-y-6">
            <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
              <div className="md:col-span-2">
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Scenario Name *
                </label>
                <input
                  type="text"
                  value={scenario.scenario_name}
                  onChange={(e) => setScenario(prev => ({ ...prev, scenario_name: e.target.value }))}
                  className={clsx(
                    'w-full px-3 py-2 border rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500',
                    validationErrors.scenario_name ? 'border-red-300' : 'border-gray-300'
                  )}
                  placeholder="Enter scenario name"
                />
                {validationErrors.scenario_name && (
                  <p className="mt-1 text-sm text-red-600">{validationErrors.scenario_name}</p>
                )}
              </div>

              <div className="md:col-span-2">
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Description *
                </label>
                <textarea
                  value={scenario.description}
                  onChange={(e) => setScenario(prev => ({ ...prev, description: e.target.value }))}
                  rows={4}
                  className={clsx(
                    'w-full px-3 py-2 border rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500',
                    validationErrors.description ? 'border-red-300' : 'border-gray-300'
                  )}
                  placeholder="Describe the scenario and its objectives"
                />
                {validationErrors.description && (
                  <p className="mt-1 text-sm text-red-600">{validationErrors.description}</p>
                )}
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-3">
                  Scenario Type *
                </label>
                <div className="space-y-3">
                  {SCENARIO_TYPES.map((type) => {
                    const Icon = type.icon;
                    return (
                      <div
                        key={type.value}
                        onClick={() => setScenario(prev => ({ ...prev, scenario_type: type.value }))}
                        className={clsx(
                          'p-4 border rounded-lg cursor-pointer transition-colors',
                          scenario.scenario_type === type.value
                            ? 'border-blue-500 bg-blue-50'
                            : 'border-gray-200 hover:border-gray-300'
                        )}
                      >
                        <div className="flex items-center gap-3">
                          <Icon className="w-5 h-5 text-gray-600" />
                          <div>
                            <h4 className="font-medium text-gray-900">{type.label}</h4>
                            <p className="text-sm text-gray-600">{type.description}</p>
                          </div>
                        </div>
                      </div>
                    );
                  })}
                </div>
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Tags
                </label>
                <div className="flex gap-2 mb-2">
                  <input
                    type="text"
                    value={newTag}
                    onChange={(e) => setNewTag(e.target.value)}
                    onKeyPress={(e) => e.key === 'Enter' && addTag()}
                    className="flex-1 px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                    placeholder="Add tag"
                  />
                  <button
                    onClick={addTag}
                    className="px-3 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors"
                  >
                    <Plus className="w-4 h-4" />
                  </button>
                </div>
                <div className="flex flex-wrap gap-2">
                  {scenario.tags.map((tag, index) => (
                    <span
                      key={index}
                      className="inline-flex items-center gap-1 px-2 py-1 bg-blue-100 text-blue-800 text-sm rounded-full"
                    >
                      {tag}
                      <button
                        onClick={() => removeTag(tag)}
                        className="hover:text-blue-600"
                      >
                        <X className="w-3 h-3" />
                      </button>
                    </span>
                  ))}
                </div>
              </div>
            </div>

            <div className="flex items-center gap-2">
              <input
                type="checkbox"
                id="is_template"
                checked={scenario.is_template}
                onChange={(e) => setScenario(prev => ({ ...prev, is_template: e.target.checked }))}
                className="rounded border-gray-300 text-blue-600 focus:ring-blue-500"
              />
              <label htmlFor="is_template" className="text-sm text-gray-700">
                Save as reusable template
              </label>
            </div>
          </div>
        );

      case 2:
        return (
          <div className="space-y-6">
            <div className="flex items-center justify-between">
              <h3 className="text-lg font-medium text-gray-900">Regulatory Changes Configuration</h3>
              <div className="flex items-center gap-2">
                <button
                  onClick={() => setJsonEditorMode(jsonEditorMode === 'form' ? 'json' : 'form')}
                  className="flex items-center gap-2 px-3 py-1 text-sm border border-gray-300 rounded hover:bg-gray-50"
                >
                  {jsonEditorMode === 'form' ? <Code className="w-4 h-4" /> : <FileText className="w-4 h-4" />}
                  {jsonEditorMode === 'form' ? 'JSON Editor' : 'Form Editor'}
                </button>
              </div>
            </div>

            {jsonEditorMode === 'form' ? (
              <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">
                    Jurisdiction *
                  </label>
                  <select
                    value={scenario.regulatory_changes.jurisdiction}
                    onChange={(e) => updateRegulatoryChange('jurisdiction', e.target.value)}
                    className={clsx(
                      'w-full px-3 py-2 border rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500',
                      validationErrors.jurisdiction ? 'border-red-300' : 'border-gray-300'
                    )}
                  >
                    <option value="">Select jurisdiction</option>
                    {JURISDICTIONS.map(jurisdiction => (
                      <option key={jurisdiction} value={jurisdiction}>{jurisdiction}</option>
                    ))}
                  </select>
                  {validationErrors.jurisdiction && (
                    <p className="mt-1 text-sm text-red-600">{validationErrors.jurisdiction}</p>
                  )}
                </div>

                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">
                    Regulatory Body *
                  </label>
                  <select
                    value={scenario.regulatory_changes.regulatory_body}
                    onChange={(e) => updateRegulatoryChange('regulatory_body', e.target.value)}
                    className={clsx(
                      'w-full px-3 py-2 border rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500',
                      validationErrors.regulatory_body ? 'border-red-300' : 'border-gray-300'
                    )}
                  >
                    <option value="">Select regulatory body</option>
                    {REGULATORY_BODIES.map(body => (
                      <option key={body} value={body}>{body}</option>
                    ))}
                  </select>
                  {validationErrors.regulatory_body && (
                    <p className="mt-1 text-sm text-red-600">{validationErrors.regulatory_body}</p>
                  )}
                </div>

                <div className="md:col-span-2">
                  <label className="block text-sm font-medium text-gray-700 mb-2">
                    Regulation Type
                  </label>
                  <select
                    value={scenario.regulatory_changes.regulation_type}
                    onChange={(e) => updateRegulatoryChange('regulation_type', e.target.value)}
                    className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                  >
                    <option value="">Select regulation type</option>
                    <option value="kyc">Know Your Customer (KYC)</option>
                    <option value="aml">Anti-Money Laundering (AML)</option>
                    <option value="privacy">Data Privacy</option>
                    <option value="reporting">Regulatory Reporting</option>
                    <option value="capital">Capital Requirements</option>
                    <option value="liquidity">Liquidity Requirements</option>
                  </select>
                </div>

                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">
                    Effective Date
                  </label>
                  <input
                    type="date"
                    value={scenario.regulatory_changes.effective_date}
                    onChange={(e) => updateRegulatoryChange('effective_date', e.target.value)}
                    className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                  />
                </div>

                <div className="md:col-span-2">
                  <label className="block text-sm font-medium text-gray-700 mb-2">
                    Change Summary
                  </label>
                  <textarea
                    value={scenario.regulatory_changes.summary || ''}
                    onChange={(e) => updateRegulatoryChange('summary', e.target.value)}
                    rows={3}
                    className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                    placeholder="Brief summary of the regulatory changes"
                  />
                </div>
              </div>
            ) : (
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Regulatory Changes (JSON)
                </label>
                <textarea
                  value={JSON.stringify(scenario.regulatory_changes, null, 2)}
                  onChange={(e) => {
                    try {
                      const parsed = JSON.parse(e.target.value);
                      setScenario(prev => ({ ...prev, regulatory_changes: parsed }));
                    } catch (error) {
                      // Invalid JSON, keep current value
                    }
                  }}
                  rows={20}
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg font-mono text-sm focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                  placeholder="Enter regulatory changes as JSON"
                />
              </div>
            )}
          </div>
        );

      case 3:
        return (
          <div className="space-y-6">
            <h3 className="text-lg font-medium text-gray-900">Impact Parameters</h3>

            <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Risk Threshold (0-1) *
                </label>
                <input
                  type="number"
                  min="0"
                  max="1"
                  step="0.1"
                  value={scenario.impact_parameters.risk_threshold}
                  onChange={(e) => updateImpactParameter('risk_threshold', parseFloat(e.target.value))}
                  className={clsx(
                    'w-full px-3 py-2 border rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500',
                    validationErrors.risk_threshold ? 'border-red-300' : 'border-gray-300'
                  )}
                />
                {validationErrors.risk_threshold && (
                  <p className="mt-1 text-sm text-red-600">{validationErrors.risk_threshold}</p>
                )}
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Time Horizon (Days)
                </label>
                <input
                  type="number"
                  min="1"
                  max="3650"
                  value={scenario.impact_parameters.time_horizon_days}
                  onChange={(e) => updateImpactParameter('time_horizon_days', parseInt(e.target.value))}
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                />
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Compliance Weight (0-1)
                </label>
                <input
                  type="number"
                  min="0"
                  max="1"
                  step="0.1"
                  value={scenario.impact_parameters.compliance_weight}
                  onChange={(e) => updateImpactParameter('compliance_weight', parseFloat(e.target.value))}
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                />
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Operational Weight (0-1)
                </label>
                <input
                  type="number"
                  min="0"
                  max="1"
                  step="0.1"
                  value={scenario.impact_parameters.operational_weight}
                  onChange={(e) => updateImpactParameter('operational_weight', parseFloat(e.target.value))}
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                />
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Cost Weight (0-1)
                </label>
                <input
                  type="number"
                  min="0"
                  max="1"
                  step="0.1"
                  value={scenario.impact_parameters.cost_weight}
                  onChange={(e) => updateImpactParameter('cost_weight', parseFloat(e.target.value))}
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                />
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Estimated Runtime (seconds) *
                </label>
                <input
                  type="number"
                  min="60"
                  value={scenario.estimated_runtime_seconds}
                  onChange={(e) => setScenario(prev => ({ ...prev, estimated_runtime_seconds: parseInt(e.target.value) }))}
                  className={clsx(
                    'w-full px-3 py-2 border rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500',
                    validationErrors.runtime ? 'border-red-300' : 'border-gray-300'
                  )}
                />
                {validationErrors.runtime && (
                  <p className="mt-1 text-sm text-red-600">{validationErrors.runtime}</p>
                )}
              </div>
            </div>

            <div className="flex items-center gap-2">
              <button
                onClick={() => setShowAdvanced(!showAdvanced)}
                className="flex items-center gap-2 text-sm text-gray-600 hover:text-gray-800"
              >
                {showAdvanced ? <ChevronUp className="w-4 h-4" /> : <ChevronDown className="w-4 h-4" />}
                {showAdvanced ? 'Hide' : 'Show'} Advanced Settings
              </button>
            </div>

            {showAdvanced && (
              <div className="border border-gray-200 rounded-lg p-4">
                <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                  <div>
                    <label className="block text-sm font-medium text-gray-700 mb-2">
                      Max Concurrent Simulations
                    </label>
                    <input
                      type="number"
                      min="1"
                      max="50"
                      value={scenario.max_concurrent_simulations}
                      onChange={(e) => setScenario(prev => ({ ...prev, max_concurrent_simulations: parseInt(e.target.value) }))}
                      className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                    />
                  </div>

                  <div>
                    <label className="block text-sm font-medium text-gray-700 mb-2">
                      Version
                    </label>
                    <input
                      type="text"
                      value={scenario.version}
                      onChange={(e) => setScenario(prev => ({ ...prev, version: e.target.value }))}
                      className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                      placeholder="1.0"
                    />
                  </div>
                </div>
              </div>
            )}
          </div>
        );

      case 4:
        return (
          <div className="space-y-6">
            <h3 className="text-lg font-medium text-gray-900">Data Configuration</h3>

            <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
              <div>
                <h4 className="text-md font-medium text-gray-900 mb-4">Baseline Data *</h4>
                <div className="space-y-4">
                  <div>
                    <label className="block text-sm font-medium text-gray-700 mb-1">
                      Current Risk Score (0-1)
                    </label>
                    <input
                      type="number"
                      min="0"
                      max="1"
                      step="0.01"
                      value={scenario.baseline_data.current_risk_score}
                      onChange={(e) => updateBaselineData('current_risk_score', parseFloat(e.target.value))}
                      className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                    />
                  </div>

                  <div>
                    <label className="block text-sm font-medium text-gray-700 mb-1">
                      Current Compliance Score (0-1)
                    </label>
                    <input
                      type="number"
                      min="0"
                      max="1"
                      step="0.01"
                      value={scenario.baseline_data.current_compliance_score}
                      onChange={(e) => updateBaselineData('current_compliance_score', parseFloat(e.target.value))}
                      className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                    />
                  </div>

                  <div>
                    <label className="block text-sm font-medium text-gray-700 mb-1">
                      Current Operational Efficiency (0-1)
                    </label>
                    <input
                      type="number"
                      min="0"
                      max="1"
                      step="0.01"
                      value={scenario.baseline_data.current_operational_efficiency}
                      onChange={(e) => updateBaselineData('current_operational_efficiency', parseFloat(e.target.value))}
                      className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                    />
                  </div>

                  <div>
                    <label className="block text-sm font-medium text-gray-700 mb-1">
                      Current Cost Baseline ($)
                    </label>
                    <input
                      type="number"
                      min="0"
                      step="1000"
                      value={scenario.baseline_data.current_cost_baseline}
                      onChange={(e) => updateBaselineData('current_cost_baseline', parseFloat(e.target.value))}
                      className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                    />
                  </div>
                </div>
              </div>

              <div>
                <h4 className="text-md font-medium text-gray-900 mb-4">Test Data Configuration</h4>
                <div className="space-y-4">
                  <div className="p-4 border border-gray-200 rounded-lg">
                    <div className="flex items-center justify-between mb-2">
                      <span className="text-sm font-medium text-gray-700">Test Transactions</span>
                      <span className="text-sm text-gray-500">
                        {scenario.test_data.test_transactions?.length || 0} configured
                      </span>
                    </div>
                    <p className="text-xs text-gray-600 mb-3">
                      Sample transactions to test regulatory impact
                    </p>
                    <button className="text-sm text-blue-600 hover:text-blue-800">
                      Configure Test Transactions →
                    </button>
                  </div>

                  <div className="p-4 border border-gray-200 rounded-lg">
                    <div className="flex items-center justify-between mb-2">
                      <span className="text-sm font-medium text-gray-700">Test Entities</span>
                      <span className="text-sm text-gray-500">
                        {scenario.test_data.test_entities?.length || 0} configured
                      </span>
                    </div>
                    <p className="text-xs text-gray-600 mb-3">
                      Sample entities (customers, accounts) for testing
                    </p>
                    <button className="text-sm text-blue-600 hover:text-blue-800">
                      Configure Test Entities →
                    </button>
                  </div>

                  <div className="p-4 border border-gray-200 rounded-lg">
                    <div className="flex items-center justify-between mb-2">
                      <span className="text-sm font-medium text-gray-700">Test Scenarios</span>
                      <span className="text-sm text-gray-500">
                        {scenario.test_data.test_scenarios?.length || 0} configured
                      </span>
                    </div>
                    <p className="text-xs text-gray-600 mb-3">
                      Specific test cases and edge conditions
                    </p>
                    <button className="text-sm text-blue-600 hover:text-blue-800">
                      Configure Test Scenarios →
                    </button>
                  </div>
                </div>
              </div>
            </div>
          </div>
        );

      case 5:
        return (
          <div className="space-y-6">
            <h3 className="text-lg font-medium text-gray-900">Review & Test</h3>

            <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
              <div>
                <h4 className="text-md font-medium text-gray-900 mb-4">Scenario Summary</h4>
                <div className="bg-gray-50 rounded-lg p-4 space-y-3">
                  <div>
                    <span className="text-sm font-medium text-gray-700">Name:</span>
                    <span className="ml-2 text-sm text-gray-900">{scenario.scenario_name}</span>
                  </div>
                  <div>
                    <span className="text-sm font-medium text-gray-700">Type:</span>
                    <span className="ml-2 text-sm text-gray-900">
                      {SCENARIO_TYPES.find(t => t.value === scenario.scenario_type)?.label}
                    </span>
                  </div>
                  <div>
                    <span className="text-sm font-medium text-gray-700">Jurisdiction:</span>
                    <span className="ml-2 text-sm text-gray-900">
                      {scenario.regulatory_changes.jurisdiction}
                    </span>
                  </div>
                  <div>
                    <span className="text-sm font-medium text-gray-700">Regulatory Body:</span>
                    <span className="ml-2 text-sm text-gray-900">
                      {scenario.regulatory_changes.regulatory_body}
                    </span>
                  </div>
                  <div>
                    <span className="text-sm font-medium text-gray-700">Estimated Runtime:</span>
                    <span className="ml-2 text-sm text-gray-900">
                      {Math.round(scenario.estimated_runtime_seconds / 60)} minutes
                    </span>
                  </div>
                  <div>
                    <span className="text-sm font-medium text-gray-700">Tags:</span>
                    <div className="ml-2 mt-1 flex flex-wrap gap-1">
                      {scenario.tags.map((tag, index) => (
                        <span key={index} className="px-2 py-1 bg-blue-100 text-blue-800 text-xs rounded-full">
                          {tag}
                        </span>
                      ))}
                    </div>
                  </div>
                </div>
              </div>

              <div>
                <h4 className="text-md font-medium text-gray-900 mb-4">Actions</h4>
                <div className="space-y-3">
                  <button
                    onClick={handleTest}
                    disabled={isTesting}
                    className="w-full flex items-center justify-center gap-2 px-4 py-3 bg-green-600 hover:bg-green-700 disabled:bg-gray-400 text-white rounded-lg transition-colors"
                  >
                    {isTesting ? (
                      <>
                        <RefreshCw className="w-4 h-4 animate-spin" />
                        Testing...
                      </>
                    ) : (
                      <>
                        <Play className="w-4 h-4" />
                        Test Scenario
                      </>
                    )}
                  </button>

                  <button
                    onClick={() => onPreview?.(scenario)}
                    className="w-full flex items-center justify-center gap-2 px-4 py-3 border border-gray-300 hover:bg-gray-50 text-gray-700 rounded-lg transition-colors"
                  >
                    <Eye className="w-4 h-4" />
                    Preview Results
                  </button>

                  <button
                    onClick={() => {/* Export configuration */}}
                    className="w-full flex items-center justify-center gap-2 px-4 py-3 border border-gray-300 hover:bg-gray-50 text-gray-700 rounded-lg transition-colors"
                  >
                    <Download className="w-4 h-4" />
                    Export Configuration
                  </button>
                </div>
              </div>
            </div>

            {testResults && (
              <div className="border border-gray-200 rounded-lg p-4">
                <h4 className="text-md font-medium text-gray-900 mb-3">Test Results</h4>
                {testResults.error ? (
                  <div className="flex items-center gap-2 text-red-600">
                    <X className="w-4 h-4" />
                    <span className="text-sm">{testResults.error}</span>
                  </div>
                ) : (
                  <div className="space-y-2">
                    <div className="flex items-center gap-2 text-green-600">
                      <CheckCircle className="w-4 h-4" />
                      <span className="text-sm">Test completed successfully</span>
                    </div>
                    <div className="text-sm text-gray-600">
                      <pre className="bg-gray-50 p-2 rounded text-xs overflow-x-auto">
                        {JSON.stringify(testResults, null, 2)}
                      </pre>
                    </div>
                  </div>
                )}
              </div>
            )}
          </div>
        );

      default:
        return null;
    }
  };

  return (
    <div className={clsx('max-w-6xl mx-auto', className)}>
      {/* Progress Steps */}
      <div className="mb-8">
        <div className="flex items-center justify-between mb-4">
          {steps.map((step, index) => (
            <div key={step.id} className="flex items-center">
              <div className={clsx(
                'flex items-center justify-center w-8 h-8 rounded-full text-sm font-medium',
                activeStep > step.id
                  ? 'bg-green-600 text-white'
                  : activeStep === step.id
                  ? 'bg-blue-600 text-white'
                  : 'bg-gray-300 text-gray-600'
              )}>
                {activeStep > step.id ? <CheckCircle className="w-4 h-4" /> : step.id}
              </div>
              <div className="ml-3">
                <div className={clsx(
                  'text-sm font-medium',
                  activeStep >= step.id ? 'text-gray-900' : 'text-gray-500'
                )}>
                  {step.title}
                </div>
                <div className="text-xs text-gray-500">{step.description}</div>
              </div>
              {index < steps.length - 1 && (
                <div className={clsx(
                  'w-12 h-0.5 mx-4',
                  activeStep > step.id ? 'bg-green-600' : 'bg-gray-300'
                )} />
              )}
            </div>
          ))}
        </div>
      </div>

      {/* Step Content */}
      <div className="bg-white rounded-lg border shadow-sm p-6 mb-6">
        {renderStepContent()}
      </div>

      {/* Navigation */}
      <div className="flex items-center justify-between">
        <button
          onClick={handlePrevious}
          disabled={activeStep === 1}
          className="flex items-center gap-2 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50 disabled:opacity-50 disabled:cursor-not-allowed transition-colors"
        >
          Previous
        </button>

        <div className="flex items-center gap-3">
          {activeStep === steps.length ? (
            <>
              <button
                onClick={onCancel}
                className="px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors"
              >
                Cancel
              </button>
              <button
                onClick={handleSave}
                disabled={isLoading}
                className="flex items-center gap-2 px-6 py-2 bg-blue-600 hover:bg-blue-700 disabled:bg-gray-400 text-white rounded-lg transition-colors"
              >
                {isLoading ? (
                  <>
                    <RefreshCw className="w-4 h-4 animate-spin" />
                    Saving...
                  </>
                ) : (
                  <>
                    <Save className="w-4 h-4" />
                    Save Scenario
                  </>
                )}
              </button>
            </>
          ) : (
            <button
              onClick={handleNext}
              disabled={!isStepValid(activeStep)}
              className="flex items-center gap-2 px-6 py-2 bg-blue-600 hover:bg-blue-700 disabled:bg-gray-400 text-white rounded-lg transition-colors"
            >
              Next
            </button>
          )}
        </div>
      </div>
    </div>
  );
};

export default ScenarioCreation;
