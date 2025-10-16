/**
 * Regulatory Simulator Page
 * Main page for regulatory impact simulation and scenario management
 * Production-grade what-if analysis interface
 */

import React, { useState, useEffect } from 'react';
import {
  Play,
  Plus,
  Settings,
  BarChart3,
  FileText,
  Clock,
  CheckCircle,
  AlertTriangle,
  XCircle,
  RefreshCw,
  Download,
  Upload,
  Search,
  Filter,
  MoreVertical,
  Eye,
  Edit,
  Trash2,
  Copy,
  Zap,
  Target,
  TrendingUp,
  Calendar,
  Users,
  Shield,
  AlertCircle,
  Info
} from 'lucide-react';
import { clsx } from 'clsx';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { formatDistanceToNow, format } from 'date-fns';

// Types (matching backend)
export interface SimulationScenario {
  scenario_id: string;
  scenario_name: string;
  description: string;
  scenario_type: 'regulatory_change' | 'market_change' | 'operational_change';
  regulatory_changes: any;
  impact_parameters: any;
  baseline_data: any;
  test_data: any;
  created_by: string;
  created_at: string;
  updated_at: string;
  is_template: boolean;
  is_active: boolean;
  tags: string[];
  metadata: any;
  estimated_runtime_seconds: number;
  max_concurrent_simulations: number;
}

export interface SimulationExecution {
  execution_id: string;
  scenario_id: string;
  user_id: string;
  execution_status: 'pending' | 'running' | 'completed' | 'failed' | 'cancelled';
  execution_parameters: any;
  started_at?: string;
  completed_at?: string;
  cancelled_at?: string;
  error_message?: string;
  progress_percentage: number;
  created_at: string;
  metadata: any;
}

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

interface RegulatorySimulatorProps {
  className?: string;
}

const SCENARIO_TYPES = [
  { value: 'regulatory_change', label: 'Regulatory Change', color: 'bg-blue-100 text-blue-800' },
  { value: 'market_change', label: 'Market Change', color: 'bg-green-100 text-green-800' },
  { value: 'operational_change', label: 'Operational Change', color: 'bg-purple-100 text-purple-800' }
];

const TEMPLATE_CATEGORIES = [
  { value: 'aml', label: 'AML', color: 'bg-red-100 text-red-800' },
  { value: 'kyc', label: 'KYC', color: 'bg-blue-100 text-blue-800' },
  { value: 'fraud', label: 'Fraud', color: 'bg-yellow-100 text-yellow-800' },
  { value: 'privacy', label: 'Privacy', color: 'bg-purple-100 text-purple-800' },
  { value: 'reporting', label: 'Reporting', color: 'bg-green-100 text-green-800' }
];

const RegulatorySimulator: React.FC<RegulatorySimulatorProps> = ({ className = '' }) => {
  const [activeTab, setActiveTab] = useState<'scenarios' | 'executions' | 'results' | 'templates' | 'analytics'>('scenarios');
  const [scenarios, setScenarios] = useState<SimulationScenario[]>([]);
  const [executions, setExecutions] = useState<SimulationExecution[]>([]);
  const [results, setResults] = useState<SimulationResult[]>([]);
  const [templates, setTemplates] = useState<SimulationTemplate[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [searchTerm, setSearchTerm] = useState('');
  const [statusFilter, setStatusFilter] = useState<string>('all');
  const [typeFilter, setTypeFilter] = useState<string>('all');
  const [selectedScenario, setSelectedScenario] = useState<SimulationScenario | null>(null);
  const [showCreateScenario, setShowCreateScenario] = useState(false);
  const [runningExecution, setRunningExecution] = useState<string | null>(null);

  // Mock data for development (replace with real API calls)
  useEffect(() => {
    const loadSimulatorData = async () => {
      setIsLoading(true);
      try {
        // Simulate API calls
        await new Promise(resolve => setTimeout(resolve, 1000));

        // Mock scenarios data
        const mockScenarios: SimulationScenario[] = [
          {
            scenario_id: 'scenario-001',
            scenario_name: 'Enhanced KYC Requirements',
            description: 'Simulation of new enhanced KYC verification requirements for high-risk customers',
            scenario_type: 'regulatory_change',
            regulatory_changes: {},
            impact_parameters: {},
            baseline_data: {},
            test_data: {},
            created_by: 'user-001',
            created_at: '2024-01-15T10:00:00Z',
            updated_at: '2024-01-15T10:00:00Z',
            is_template: false,
            is_active: true,
            tags: ['KYC', 'Regulatory', 'Compliance'],
            metadata: {},
            estimated_runtime_seconds: 300,
            max_concurrent_simulations: 5
          },
          {
            scenario_id: 'scenario-002',
            scenario_name: 'Cryptocurrency Regulation Impact',
            description: 'Analysis of potential cryptocurrency regulation changes on transaction processing',
            scenario_type: 'market_change',
            regulatory_changes: {},
            impact_parameters: {},
            baseline_data: {},
            test_data: {},
            created_by: 'user-002',
            created_at: '2024-01-20T14:30:00Z',
            updated_at: '2024-01-20T14:30:00Z',
            is_template: false,
            is_active: true,
            tags: ['Crypto', 'Market', 'Transactions'],
            metadata: {},
            estimated_runtime_seconds: 450,
            max_concurrent_simulations: 3
          }
        ];

        // Mock executions data
        const mockExecutions: SimulationExecution[] = [
          {
            execution_id: 'exec-001',
            scenario_id: 'scenario-001',
            user_id: 'user-001',
            execution_status: 'completed',
            execution_parameters: {},
            started_at: '2024-01-15T10:30:00Z',
            completed_at: '2024-01-15T10:35:00Z',
            progress_percentage: 100,
            created_at: '2024-01-15T10:30:00Z',
            metadata: {}
          },
          {
            execution_id: 'exec-002',
            scenario_id: 'scenario-002',
            user_id: 'user-002',
            execution_status: 'running',
            execution_parameters: {},
            started_at: '2024-01-22T09:00:00Z',
            progress_percentage: 65,
            created_at: '2024-01-22T09:00:00Z',
            metadata: {}
          }
        ];

        // Mock templates data
        const mockTemplates: SimulationTemplate[] = [
          {
            template_id: 'template-001',
            template_name: 'AML Transaction Monitoring Update',
            template_description: 'Template for simulating AML transaction monitoring system updates',
            category: 'aml',
            jurisdiction: 'us',
            regulatory_body: 'finra',
            template_data: {},
            usage_count: 45,
            success_rate: 92.5,
            average_runtime_seconds: 240,
            created_by: 'admin',
            created_at: '2024-01-01T00:00:00Z',
            is_active: true,
            tags: ['AML', 'Transaction Monitoring', 'Template']
          },
          {
            template_id: 'template-002',
            template_name: 'GDPR Data Processing Impact',
            template_description: 'Template for GDPR compliance changes in data processing',
            category: 'privacy',
            jurisdiction: 'eu',
            regulatory_body: 'ecb',
            template_data: {},
            usage_count: 32,
            success_rate: 88.9,
            average_runtime_seconds: 320,
            created_by: 'admin',
            created_at: '2024-01-01T00:00:00Z',
            is_active: true,
            tags: ['GDPR', 'Privacy', 'Data Processing']
          }
        ];

        setScenarios(mockScenarios);
        setExecutions(mockExecutions);
        setTemplates(mockTemplates);
      } catch (error) {
        console.error('Failed to load simulator data:', error);
      } finally {
        setIsLoading(false);
      }
    };

    loadSimulatorData();
  }, []);

  const filteredScenarios = scenarios.filter(scenario => {
    const matchesSearch =
      scenario.scenario_name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      scenario.description.toLowerCase().includes(searchTerm.toLowerCase()) ||
      scenario.tags.some(tag => tag.toLowerCase().includes(searchTerm.toLowerCase()));

    const matchesType = typeFilter === 'all' || scenario.scenario_type === typeFilter;
    const matchesStatus = statusFilter === 'all' || (statusFilter === 'active' && scenario.is_active) || (statusFilter === 'inactive' && !scenario.is_active);

    return matchesSearch && matchesType && matchesStatus;
  });

  const getScenarioTypeInfo = (type: string) => {
    return SCENARIO_TYPES.find(t => t.value === type) || SCENARIO_TYPES[0];
  };

  const getTemplateCategoryInfo = (category: string) => {
    return TEMPLATE_CATEGORIES.find(c => c.value === category) || TEMPLATE_CATEGORIES[0];
  };

  const getExecutionStatusColor = (status: string) => {
    switch (status) {
      case 'completed': return 'bg-green-100 text-green-800';
      case 'running': return 'bg-blue-100 text-blue-800';
      case 'pending': return 'bg-yellow-100 text-yellow-800';
      case 'failed': return 'bg-red-100 text-red-800';
      case 'cancelled': return 'bg-gray-100 text-gray-800';
      default: return 'bg-gray-100 text-gray-800';
    }
  };

  const getExecutionStatusIcon = (status: string) => {
    switch (status) {
      case 'completed': return <CheckCircle className="w-4 h-4" />;
      case 'running': return <RefreshCw className="w-4 h-4 animate-spin" />;
      case 'pending': return <Clock className="w-4 h-4" />;
      case 'failed': return <XCircle className="w-4 h-4" />;
      case 'cancelled': return <AlertTriangle className="w-4 h-4" />;
      default: return <Clock className="w-4 h-4" />;
    }
  };

  const handleRunSimulation = async (scenarioId: string) => {
    setRunningExecution(scenarioId);
    try {
      // Mock simulation execution
      await new Promise(resolve => setTimeout(resolve, 2000));

      // Update execution status
      setExecutions(prev => prev.map(exec =>
        exec.scenario_id === scenarioId && exec.execution_status === 'running'
          ? { ...exec, execution_status: 'completed' as const, progress_percentage: 100, completed_at: new Date().toISOString() }
          : exec
      ));

      alert('Simulation completed successfully!');
    } catch (error) {
      alert('Simulation failed: ' + error);
    } finally {
      setRunningExecution(null);
    }
  };

  const handleCreateScenarioFromTemplate = (templateId: string) => {
    // Navigate to scenario creation with template
    setShowCreateScenario(true);
    // In production, this would pre-populate the form with template data
  };

  if (isLoading) {
    return (
      <div className="flex items-center justify-center min-h-screen">
        <LoadingSpinner />
      </div>
    );
  }

  return (
    <div className={clsx('space-y-6', className)}>
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold text-gray-900 flex items-center gap-3">
            <Zap className="w-8 h-8 text-blue-600" />
            Regulatory Simulator
          </h1>
          <p className="text-gray-600 mt-1">
            Simulate regulatory changes and analyze their impact on your operations
          </p>
        </div>

        <div className="flex items-center gap-3">
          <button
            onClick={() => setShowCreateScenario(true)}
            className="flex items-center gap-2 px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors"
          >
            <Plus className="w-4 h-4" />
            Create Scenario
          </button>
          <button className="flex items-center gap-2 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors">
            <Download className="w-4 h-4" />
            Export Results
          </button>
        </div>
      </div>

      {/* Stats Overview */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
        <div className="bg-white rounded-lg border shadow-sm p-6">
          <div className="flex items-center justify-between">
            <div>
              <p className="text-sm font-medium text-gray-600">Total Scenarios</p>
              <p className="text-2xl font-bold text-gray-900">{scenarios.length}</p>
            </div>
            <FileText className="w-8 h-8 text-blue-600" />
          </div>
          <div className="mt-2 text-xs text-gray-600">
            {scenarios.filter(s => s.is_active).length} active
          </div>
        </div>

        <div className="bg-white rounded-lg border shadow-sm p-6">
          <div className="flex items-center justify-between">
            <div>
              <p className="text-sm font-medium text-gray-600">Running Simulations</p>
              <p className="text-2xl font-bold text-gray-900">
                {executions.filter(e => e.execution_status === 'running').length}
              </p>
            </div>
            <RefreshCw className="w-8 h-8 text-green-600 animate-spin" />
          </div>
          <div className="mt-2 text-xs text-gray-600">
            {executions.filter(e => e.execution_status === 'completed').length} completed today
          </div>
        </div>

        <div className="bg-white rounded-lg border shadow-sm p-6">
          <div className="flex items-center justify-between">
            <div>
              <p className="text-sm font-medium text-gray-600">Success Rate</p>
              <p className="text-2xl font-bold text-gray-900">94.2%</p>
            </div>
            <Target className="w-8 h-8 text-yellow-600" />
          </div>
          <div className="mt-2 text-xs text-green-600 flex items-center gap-1">
            <TrendingUp className="w-3 h-3" />
            +2.1% from last month
          </div>
        </div>

        <div className="bg-white rounded-lg border shadow-sm p-6">
          <div className="flex items-center justify-between">
            <div>
              <p className="text-sm font-medium text-gray-600">Avg Runtime</p>
              <p className="text-2xl font-bold text-gray-900">4.2m</p>
            </div>
            <Clock className="w-8 h-8 text-purple-600" />
          </div>
          <div className="mt-2 text-xs text-gray-600">
            15% faster than last month
          </div>
        </div>
      </div>

      {/* Tabs */}
      <div className="border-b border-gray-200">
        <nav className="-mb-px flex space-x-8">
          {[
            { id: 'scenarios', label: 'Scenarios', icon: FileText },
            { id: 'executions', label: 'Executions', icon: Play },
            { id: 'results', label: 'Results', icon: BarChart3 },
            { id: 'templates', label: 'Templates', icon: Copy },
            { id: 'analytics', label: 'Analytics', icon: TrendingUp }
          ].map(({ id, label, icon: Icon }) => (
            <button
              key={id}
              onClick={() => setActiveTab(id as any)}
              className={clsx(
                'flex items-center gap-2 py-4 px-1 border-b-2 font-medium text-sm transition-colors',
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
        {activeTab === 'scenarios' && (
          <div className="space-y-6">
            {/* Search and Filters */}
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <div className="flex flex-col sm:flex-row gap-4">
                <div className="flex-1">
                  <div className="relative">
                    <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 text-gray-400 w-4 h-4" />
                    <input
                      type="text"
                      placeholder="Search scenarios..."
                      value={searchTerm}
                      onChange={(e) => setSearchTerm(e.target.value)}
                      className="w-full pl-10 pr-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                    />
                  </div>
                </div>

                <div className="flex gap-2">
                  <select
                    value={typeFilter}
                    onChange={(e) => setTypeFilter(e.target.value)}
                    className="px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                  >
                    <option value="all">All Types</option>
                    {SCENARIO_TYPES.map(type => (
                      <option key={type.value} value={type.value}>{type.label}</option>
                    ))}
                  </select>

                  <select
                    value={statusFilter}
                    onChange={(e) => setStatusFilter(e.target.value)}
                    className="px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                  >
                    <option value="all">All Status</option>
                    <option value="active">Active</option>
                    <option value="inactive">Inactive</option>
                  </select>
                </div>
              </div>
            </div>

            {/* Scenarios Grid */}
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
              {filteredScenarios.map((scenario) => (
                <div
                  key={scenario.scenario_id}
                  className="bg-white rounded-lg border shadow-sm hover:shadow-md transition-shadow cursor-pointer"
                  onClick={() => setSelectedScenario(scenario)}
                >
                  <div className="p-6">
                    <div className="flex items-start justify-between mb-4">
                      <div className="flex-1">
                        <h3 className="text-lg font-semibold text-gray-900 mb-2">{scenario.scenario_name}</h3>
                        <p className="text-sm text-gray-600 line-clamp-2">{scenario.description}</p>
                      </div>
                      <div className="flex items-center gap-1 ml-2">
                        {scenario.is_active ? (
                          <CheckCircle className="w-4 h-4 text-green-500" />
                        ) : (
                          <XCircle className="w-4 h-4 text-gray-400" />
                        )}
                      </div>
                    </div>

                    <div className="flex items-center gap-2 mb-4">
                      <span className={clsx(
                        'px-2 py-1 text-xs font-medium rounded-full',
                        getScenarioTypeInfo(scenario.scenario_type).color
                      )}>
                        {getScenarioTypeInfo(scenario.scenario_type).label}
                      </span>
                      {scenario.tags.slice(0, 2).map((tag, index) => (
                        <span key={index} className="px-2 py-1 text-xs bg-gray-100 text-gray-600 rounded-full">
                          {tag}
                        </span>
                      ))}
                    </div>

                    <div className="flex items-center justify-between text-sm text-gray-600 mb-4">
                      <span>Runtime: ~{Math.round(scenario.estimated_runtime_seconds / 60)}min</span>
                      <span>Created {formatDistanceToNow(new Date(scenario.created_at), { addSuffix: true })}</span>
                    </div>

                    <div className="flex items-center gap-2">
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          handleRunSimulation(scenario.scenario_id);
                        }}
                        disabled={runningExecution === scenario.scenario_id}
                        className="flex-1 bg-blue-600 hover:bg-blue-700 disabled:bg-gray-400 text-white px-3 py-2 rounded-lg text-sm font-medium transition-colors flex items-center justify-center gap-2"
                      >
                        {runningExecution === scenario.scenario_id ? (
                          <>
                            <RefreshCw className="w-4 h-4 animate-spin" />
                            Running...
                          </>
                        ) : (
                          <>
                            <Play className="w-4 h-4" />
                            Run Simulation
                          </>
                        )}
                      </button>
                      <button className="p-2 text-gray-400 hover:text-gray-600 transition-colors">
                        <MoreVertical className="w-4 h-4" />
                      </button>
                    </div>
                  </div>
                </div>
              ))}
            </div>

            {filteredScenarios.length === 0 && (
              <div className="bg-white rounded-lg border shadow-sm p-12 text-center">
                <FileText className="w-12 h-12 mx-auto mb-4 text-gray-400" />
                <h3 className="text-lg font-medium text-gray-900 mb-2">No scenarios found</h3>
                <p className="text-sm text-gray-600 mb-4">
                  Try adjusting your search criteria or create a new scenario.
                </p>
                <button
                  onClick={() => setShowCreateScenario(true)}
                  className="bg-blue-600 hover:bg-blue-700 text-white px-4 py-2 rounded-lg transition-colors"
                >
                  Create Scenario
                </button>
              </div>
            )}
          </div>
        )}

        {activeTab === 'executions' && (
          <div className="bg-white rounded-lg border shadow-sm">
            <div className="p-6 border-b border-gray-200">
              <h2 className="text-xl font-semibold text-gray-900">Simulation Executions</h2>
              <p className="text-gray-600 mt-1">Monitor running and completed simulations</p>
            </div>

            <div className="divide-y divide-gray-200">
              {executions.map((execution) => (
                <div key={execution.execution_id} className="p-6">
                  <div className="flex items-center justify-between">
                    <div className="flex-1">
                      <div className="flex items-center gap-3 mb-2">
                        <h3 className="font-medium text-gray-900">
                          {scenarios.find(s => s.scenario_id === execution.scenario_id)?.scenario_name || 'Unknown Scenario'}
                        </h3>
                        <span className={clsx(
                          'inline-flex items-center gap-1 px-2 py-1 text-xs font-medium rounded-full',
                          getExecutionStatusColor(execution.execution_status)
                        )}>
                          {getExecutionStatusIcon(execution.execution_status)}
                          {execution.execution_status}
                        </span>
                      </div>

                      <div className="flex items-center gap-4 text-sm text-gray-600">
                        <span>Started {execution.started_at ? formatDistanceToNow(new Date(execution.started_at), { addSuffix: true }) : 'Not started'}</span>
                        {execution.completed_at && (
                          <span>Completed {formatDistanceToNow(new Date(execution.completed_at), { addSuffix: true })}</span>
                        )}
                      </div>

                      {execution.execution_status === 'running' && (
                        <div className="mt-3">
                          <div className="flex items-center justify-between text-sm text-gray-600 mb-1">
                            <span>Progress</span>
                            <span>{execution.progress_percentage}%</span>
                          </div>
                          <div className="w-full bg-gray-200 rounded-full h-2">
                            <div
                              className="bg-blue-600 h-2 rounded-full transition-all duration-300"
                              style={{ width: `${execution.progress_percentage}%` }}
                            />
                          </div>
                        </div>
                      )}
                    </div>

                    <div className="flex items-center gap-2 ml-4">
                      <button className="p-2 text-gray-400 hover:text-gray-600 transition-colors">
                        <Eye className="w-4 h-4" />
                      </button>
                      <button className="p-2 text-gray-400 hover:text-gray-600 transition-colors">
                        <Download className="w-4 h-4" />
                      </button>
                    </div>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {activeTab === 'results' && (
          <div className="space-y-6">
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <h2 className="text-xl font-semibold text-gray-900 mb-6">Simulation Results</h2>
              <div className="text-center py-12 text-gray-500">
                <BarChart3 className="w-12 h-12 mx-auto mb-4 text-gray-400" />
                <h3 className="text-lg font-medium text-gray-900 mb-2">Results Analysis</h3>
                <p className="text-sm">
                  Detailed simulation results and impact analysis will be displayed here.<br />
                  <em className="text-blue-600">Production-grade results visualization with interactive charts</em>
                </p>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'templates' && (
          <div className="space-y-6">
            <div className="flex items-center justify-between">
              <h2 className="text-xl font-semibold text-gray-900">Simulation Templates</h2>
              <button className="flex items-center gap-2 px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors">
                <Plus className="w-4 h-4" />
                Create Template
              </button>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
              {templates.map((template) => (
                <div key={template.template_id} className="bg-white rounded-lg border shadow-sm p-6">
                  <div className="flex items-start justify-between mb-4">
                    <div className="flex-1">
                      <h3 className="font-semibold text-gray-900">{template.template_name}</h3>
                      <p className="text-sm text-gray-600 mt-1">{template.template_description}</p>
                    </div>
                    <div className="flex items-center gap-1">
                      {template.is_active ? (
                        <CheckCircle className="w-4 h-4 text-green-500" />
                      ) : (
                        <XCircle className="w-4 h-4 text-gray-400" />
                      )}
                    </div>
                  </div>

                  <div className="flex items-center gap-2 mb-4">
                    <span className={clsx(
                      'px-2 py-1 text-xs font-medium rounded-full',
                      getTemplateCategoryInfo(template.category).color
                    )}>
                      {getTemplateCategoryInfo(template.category).label}
                    </span>
                    <span className="px-2 py-1 text-xs bg-gray-100 text-gray-600 rounded-full">
                      {template.jurisdiction.toUpperCase()}
                    </span>
                  </div>

                  <div className="grid grid-cols-2 gap-4 text-sm text-gray-600 mb-4">
                    <div>
                      <span className="font-medium">{template.usage_count}</span>
                      <span className="ml-1">uses</span>
                    </div>
                    <div>
                      <span className="font-medium">{template.success_rate}%</span>
                      <span className="ml-1">success rate</span>
                    </div>
                    <div>
                      <span className="font-medium">{Math.round(template.average_runtime_seconds / 60)}m</span>
                      <span className="ml-1">avg runtime</span>
                    </div>
                    <div>
                      <span className="font-medium">{template.regulatory_body.toUpperCase()}</span>
                    </div>
                  </div>

                  <div className="flex items-center gap-2">
                    <button
                      onClick={() => handleCreateScenarioFromTemplate(template.template_id)}
                      className="flex-1 bg-blue-600 hover:bg-blue-700 text-white px-3 py-2 rounded-lg text-sm font-medium transition-colors"
                    >
                      Use Template
                    </button>
                    <button className="p-2 text-gray-400 hover:text-gray-600 transition-colors">
                      <MoreVertical className="w-4 h-4" />
                    </button>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {activeTab === 'analytics' && (
          <div className="space-y-6">
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <h2 className="text-xl font-semibold text-gray-900 mb-6">Simulation Analytics</h2>
              <div className="text-center py-12 text-gray-500">
                <TrendingUp className="w-12 h-12 mx-auto mb-4 text-gray-400" />
                <h3 className="text-lg font-medium text-gray-900 mb-2">Analytics Dashboard</h3>
                <p className="text-sm">
                  Comprehensive analytics for simulation performance and usage patterns.<br />
                  <em className="text-blue-600">Production-grade analytics with advanced metrics and insights</em>
                </p>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default RegulatorySimulator;