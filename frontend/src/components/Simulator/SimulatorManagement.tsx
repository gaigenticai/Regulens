/**
 * Simulator Management Component
 * Advanced regulatory simulator management and configuration
 * Production-grade simulation lifecycle management
 */

import React, { useState, useEffect, useMemo } from 'react';
import {
  Settings,
  Play,
  Pause,
  Square,
  Calendar,
  Clock,
  Users,
  BarChart3,
  FileText,
  Edit,
  Trash2,
  Copy,
  Download,
  Upload,
  RefreshCw,
  AlertTriangle,
  CheckCircle,
  XCircle,
  Zap,
  Target,
  TrendingUp,
  Filter,
  Search,
  MoreVertical,
  Plus,
  Minus,
  ChevronDown,
  ChevronUp,
  Eye,
  Save,
  RotateCcw,
  Archive,
  Shield,
  AlertCircle,
  Info,
  Layers,
  GitBranch,
  Workflow
} from 'lucide-react';
import { clsx } from 'clsx';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { format, formatDistanceToNow, addDays } from 'date-fns';

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
  version: string;
  parent_scenario_id?: string;
  approval_status: 'draft' | 'pending_review' | 'approved' | 'rejected';
  approved_by?: string;
  approved_at?: string;
  execution_count: number;
  last_execution_at?: string;
  success_rate: number;
}

export interface SimulationSchedule {
  schedule_id: string;
  scenario_id: string;
  schedule_name: string;
  schedule_type: 'one_time' | 'recurring' | 'conditional';
  cron_expression?: string;
  next_run_at?: string;
  last_run_at?: string;
  is_active: boolean;
  execution_parameters: any;
  notification_settings: any;
  created_at: string;
  created_by: string;
}

export interface SimulationWorkflow {
  workflow_id: string;
  workflow_name: string;
  description: string;
  steps: SimulationWorkflowStep[];
  is_active: boolean;
  created_at: string;
  created_by: string;
  execution_count: number;
  success_rate: number;
}

export interface SimulationWorkflowStep {
  step_id: string;
  step_name: string;
  step_type: 'scenario_execution' | 'data_validation' | 'report_generation' | 'approval_request';
  scenario_id?: string;
  parameters: any;
  dependencies: string[]; // step_ids that must complete before this step
  timeout_seconds: number;
  retry_count: number;
}

interface SimulatorManagementProps {
  className?: string;
  onScenarioCreate?: (scenario: Partial<SimulationScenario>) => void;
  onScenarioUpdate?: (scenarioId: string, updates: Partial<SimulationScenario>) => void;
  onScenarioDelete?: (scenarioId: string) => void;
  onBulkOperation?: (operation: string, scenarioIds: string[]) => void;
  onWorkflowExecute?: (workflowId: string) => void;
  onScheduleCreate?: (schedule: Partial<SimulationSchedule>) => void;
}

const SCENARIO_TYPES = [
  { value: 'regulatory_change', label: 'Regulatory Change', color: 'bg-blue-100 text-blue-800', icon: Shield },
  { value: 'market_change', label: 'Market Change', color: 'bg-green-100 text-green-800', icon: TrendingUp },
  { value: 'operational_change', label: 'Operational Change', color: 'bg-purple-100 text-purple-800', icon: Settings }
];

const APPROVAL_STATUSES = [
  { value: 'draft', label: 'Draft', color: 'bg-gray-100 text-gray-800' },
  { value: 'pending_review', label: 'Pending Review', color: 'bg-yellow-100 text-yellow-800' },
  { value: 'approved', label: 'Approved', color: 'bg-green-100 text-green-800' },
  { value: 'rejected', label: 'Rejected', color: 'bg-red-100 text-red-800' }
];

const SimulatorManagement: React.FC<SimulatorManagementProps> = ({
  className = '',
  onScenarioCreate,
  onScenarioUpdate,
  onScenarioDelete,
  onBulkOperation,
  onWorkflowExecute,
  onScheduleCreate
}) => {
  const [activeTab, setActiveTab] = useState<'scenarios' | 'workflows' | 'schedules' | 'analytics' | 'settings'>('scenarios');
  const [scenarios, setScenarios] = useState<SimulationScenario[]>([]);
  const [workflows, setWorkflows] = useState<SimulationWorkflow[]>([]);
  const [schedules, setSchedules] = useState<SimulationSchedule[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [searchTerm, setSearchTerm] = useState('');
  const [statusFilter, setStatusFilter] = useState<string>('all');
  const [typeFilter, setTypeFilter] = useState<string>('all');
  const [selectedScenarios, setSelectedScenarios] = useState<Set<string>>(new Set());
  const [showBulkActions, setShowBulkActions] = useState(false);
  const [editingScenario, setEditingScenario] = useState<SimulationScenario | null>(null);
  const [showAdvancedFilters, setShowAdvancedFilters] = useState(false);

  // Mock data for development (replace with real API calls)
  useEffect(() => {
    const loadManagementData = async () => {
      setIsLoading(true);
      try {
        // Fetch real simulation data from backend API
        const authToken = localStorage.getItem('authToken');
        
        try {
          const [scenariosRes, workflowsRes] = await Promise.all([
            fetch('/api/simulator/scenarios', {
              headers: { 'Authorization': `Bearer ${authToken}` }
            }),
            fetch('/api/simulator/workflows', {
              headers: { 'Authorization': `Bearer ${authToken}` }
            })
          ]);
          
          if (scenariosRes.ok) {
            const data = await scenariosRes.json();
            setScenarios(Array.isArray(data) ? data : data.data || []);
          } else {
            setScenarios([]);
          }
          
          if (workflowsRes.ok) {
            const data = await workflowsRes.json();
            setWorkflows(Array.isArray(data) ? data : data.data || []);
          } else {
            setWorkflows([]);
          }
        } catch (error) {
          console.error('Failed to load simulator data:', error);
          setScenarios([]);
          setWorkflows([]);
        }
        setSchedules(mockSchedules);
      } catch (error) {
        console.error('Failed to load management data:', error);
      } finally {
        setIsLoading(false);
      }
    };

    loadManagementData();
  }, []);

  const filteredScenarios = scenarios.filter(scenario => {
    const matchesSearch =
      scenario.scenario_name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      scenario.description.toLowerCase().includes(searchTerm.toLowerCase()) ||
      scenario.tags.some(tag => tag.toLowerCase().includes(searchTerm.toLowerCase())) ||
      scenario.version.toLowerCase().includes(searchTerm.toLowerCase());

    const matchesType = typeFilter === 'all' || scenario.scenario_type === typeFilter;
    const matchesStatus = statusFilter === 'all' ||
      (statusFilter === 'active' && scenario.is_active) ||
      (statusFilter === 'inactive' && !scenario.is_active) ||
      (statusFilter === 'approved' && scenario.approval_status === 'approved') ||
      (statusFilter === 'draft' && scenario.approval_status === 'draft');

    return matchesSearch && matchesType && matchesStatus;
  });

  const getScenarioTypeInfo = (type: string) => {
    return SCENARIO_TYPES.find(t => t.value === type) || SCENARIO_TYPES[0];
  };

  const getApprovalStatusInfo = (status: string) => {
    return APPROVAL_STATUSES.find(s => s.value === status) || APPROVAL_STATUSES[0];
  };

  const handleScenarioAction = (action: string, scenario: SimulationScenario) => {
    switch (action) {
      case 'edit':
        setEditingScenario(scenario);
        break;
      case 'clone':
        const clonedScenario = {
          ...scenario,
          scenario_id: '',
          scenario_name: `${scenario.scenario_name} (Copy)`,
          version: '1.0',
          approval_status: 'draft' as const,
          execution_count: 0,
          success_rate: 0.0
        };
        if (onScenarioCreate) onScenarioCreate(clonedScenario);
        break;
      case 'archive':
        if (onScenarioUpdate) onScenarioUpdate(scenario.scenario_id, { is_active: false });
        break;
      case 'activate':
        if (onScenarioUpdate) onScenarioUpdate(scenario.scenario_id, { is_active: true });
        break;
      case 'delete':
        if (onScenarioDelete) onScenarioDelete(scenario.scenario_id);
        break;
      default:
        break;
    }
  };

  const handleBulkOperation = (operation: string) => {
    const scenarioIds = Array.from(selectedScenarios);
    if (scenarioIds.length === 0) return;

    switch (operation) {
      case 'activate':
        scenarioIds.forEach(id => {
          if (onScenarioUpdate) onScenarioUpdate(id, { is_active: true });
        });
        break;
      case 'deactivate':
        scenarioIds.forEach(id => {
          if (onScenarioUpdate) onScenarioUpdate(id, { is_active: false });
        });
        break;
      case 'delete':
        scenarioIds.forEach(id => {
          if (onScenarioDelete) onScenarioDelete(id);
        });
        break;
      default:
        if (onBulkOperation) onBulkOperation(operation, scenarioIds);
        break;
    }

    setSelectedScenarios(new Set());
    setShowBulkActions(false);
  };

  const getScenarioHealthScore = (scenario: SimulationScenario): number => {
    let score = 0;
    if (scenario.is_active) score += 25;
    if (scenario.approval_status === 'approved') score += 25;
    if (scenario.success_rate >= 80) score += 25;
    if (scenario.execution_count > 0) score += 25;
    return score;
  };

  const getHealthColor = (score: number): string => {
    if (score >= 80) return 'text-green-600';
    if (score >= 60) return 'text-yellow-600';
    return 'text-red-600';
  };

  if (isLoading) {
    return (
      <div className="flex items-center justify-center min-h-96">
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
            <Settings className="w-8 h-8 text-blue-600" />
            Simulator Management
          </h1>
          <p className="text-gray-600 mt-1">
            Advanced simulation management, workflows, and automation
          </p>
        </div>

        <div className="flex items-center gap-3">
          <button
            onClick={() => setActiveTab('scenarios')}
            className="flex items-center gap-2 px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors"
          >
            <Plus className="w-4 h-4" />
            Create Scenario
          </button>
          <button
            onClick={() => setShowBulkActions(!showBulkActions)}
            disabled={selectedScenarios.size === 0}
            className="flex items-center gap-2 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
          >
            <Layers className="w-4 h-4" />
            Bulk Actions ({selectedScenarios.size})
          </button>
        </div>
      </div>

      {/* Bulk Actions Panel */}
      {showBulkActions && selectedScenarios.size > 0 && (
        <div className="bg-blue-50 border border-blue-200 rounded-lg p-4">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-3">
              <span className="font-medium text-blue-900">
                {selectedScenarios.size} scenario{selectedScenarios.size > 1 ? 's' : ''} selected
              </span>
            </div>
            <div className="flex items-center gap-2">
              <button
                onClick={() => handleBulkOperation('activate')}
                className="flex items-center gap-2 px-3 py-2 bg-green-600 hover:bg-green-700 text-white rounded-lg text-sm transition-colors"
              >
                <CheckCircle className="w-4 h-4" />
                Activate
              </button>
              <button
                onClick={() => handleBulkOperation('deactivate')}
                className="flex items-center gap-2 px-3 py-2 bg-yellow-600 hover:bg-yellow-700 text-white rounded-lg text-sm transition-colors"
              >
                <Pause className="w-4 h-4" />
                Deactivate
              </button>
              <button
                onClick={() => handleBulkOperation('delete')}
                className="flex items-center gap-2 px-3 py-2 bg-red-600 hover:bg-red-700 text-white rounded-lg text-sm transition-colors"
              >
                <Trash2 className="w-4 h-4" />
                Delete
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Tabs */}
      <div className="border-b border-gray-200">
        <nav className="-mb-px flex space-x-8">
          {[
            { id: 'scenarios', label: 'Scenarios', icon: FileText },
            { id: 'workflows', label: 'Workflows', icon: GitBranch },
            { id: 'schedules', label: 'Schedules', icon: Calendar },
            { id: 'analytics', label: 'Analytics', icon: BarChart3 },
            { id: 'settings', label: 'Settings', icon: Settings }
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
            {/* Advanced Filters */}
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <div className="flex items-center justify-between mb-4">
                <h2 className="text-lg font-semibold text-gray-900">Filters</h2>
                <button
                  onClick={() => setShowAdvancedFilters(!showAdvancedFilters)}
                  className="flex items-center gap-2 text-sm text-gray-600 hover:text-gray-800"
                >
                  {showAdvancedFilters ? <ChevronUp className="w-4 h-4" /> : <ChevronDown className="w-4 h-4" />}
                  {showAdvancedFilters ? 'Hide' : 'Show'} Advanced Filters
                </button>
              </div>

              <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">
                    Search
                  </label>
                  <div className="relative">
                    <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 text-gray-400 w-4 h-4" />
                    <input
                      type="text"
                      value={searchTerm}
                      onChange={(e) => setSearchTerm(e.target.value)}
                      className="w-full pl-10 pr-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                    />
                  </div>
                </div>

                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">
                    Type
                  </label>
                  <select
                    value={typeFilter}
                    onChange={(e) => setTypeFilter(e.target.value)}
                    className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                  >
                    <option value="all">All Types</option>
                    {SCENARIO_TYPES.map(type => (
                      <option key={type.value} value={type.value}>{type.label}</option>
                    ))}
                  </select>
                </div>

                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">
                    Status
                  </label>
                  <select
                    value={statusFilter}
                    onChange={(e) => setStatusFilter(e.target.value)}
                    className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                  >
                    <option value="all">All Status</option>
                    <option value="active">Active</option>
                    <option value="inactive">Inactive</option>
                    <option value="approved">Approved</option>
                    <option value="draft">Draft</option>
                  </select>
                </div>

                <div className="flex items-end">
                  <button
                    onClick={() => {
                      setSearchTerm('');
                      setTypeFilter('all');
                      setStatusFilter('all');
                    }}
                    className="w-full px-4 py-2 bg-gray-600 hover:bg-gray-700 text-white rounded-lg transition-colors"
                  >
                    Clear Filters
                  </button>
                </div>
              </div>

              {showAdvancedFilters && (
                <div className="mt-4 pt-4 border-t border-gray-200">
                  <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                    <div>
                      <label className="block text-sm font-medium text-gray-700 mb-2">
                        Success Rate
                      </label>
                      <select className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500">
                        <option value="all">All Rates</option>
                        <option value="high">High (≥90%)</option>
                        <option value="medium">Medium (70-89%)</option>
                        <option value="low">Low (&lt;70%)</option>
                      </select>
                    </div>

                    <div>
                      <label className="block text-sm font-medium text-gray-700 mb-2">
                        Execution Count
                      </label>
                      <select className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500">
                        <option value="all">All Counts</option>
                        <option value="none">Never Executed</option>
                        <option value="few">1-5 Executions</option>
                        <option value="many">6+ Executions</option>
                      </select>
                    </div>

                    <div>
                      <label className="block text-sm font-medium text-gray-700 mb-2">
                        Date Range
                      </label>
                      <select className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500">
                        <option value="all">All Time</option>
                        <option value="today">Today</option>
                        <option value="week">This Week</option>
                        <option value="month">This Month</option>
                        <option value="quarter">This Quarter</option>
                      </select>
                    </div>
                  </div>
                </div>
              )}
            </div>

            {/* Scenarios Management Table */}
            <div className="bg-white rounded-lg border shadow-sm overflow-hidden">
              <div className="p-6 border-b border-gray-200">
                <div className="flex items-center justify-between">
                  <h2 className="text-xl font-semibold text-gray-900">
                    Scenario Management ({filteredScenarios.length})
                  </h2>
                  <div className="text-sm text-gray-600">
                    Showing {filteredScenarios.length} of {scenarios.length} scenarios
                  </div>
                </div>
              </div>

              <div className="overflow-x-auto">
                <table className="min-w-full divide-y divide-gray-200">
                  <thead className="bg-gray-50">
                    <tr>
                      <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                        <input
                          type="checkbox"
                          onChange={(e) => {
                            if (e.target.checked) {
                              setSelectedScenarios(new Set(filteredScenarios.map(s => s.scenario_id)));
                            } else {
                              setSelectedScenarios(new Set());
                            }
                          }}
                          checked={selectedScenarios.size === filteredScenarios.length && filteredScenarios.length > 0}
                          className="rounded border-gray-300 text-blue-600 focus:ring-blue-500"
                        />
                      </th>
                      <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                        Scenario
                      </th>
                      <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                        Type
                      </th>
                      <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                        Status
                      </th>
                      <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                        Performance
                      </th>
                      <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                        Version
                      </th>
                      <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                        Actions
                      </th>
                    </tr>
                  </thead>
                  <tbody className="bg-white divide-y divide-gray-200">
                    {filteredScenarios.map((scenario) => {
                      const TypeIcon = getScenarioTypeInfo(scenario.scenario_type).icon;
                      const healthScore = getScenarioHealthScore(scenario);

                      return (
                        <tr key={scenario.scenario_id} className="hover:bg-gray-50">
                          <td className="px-6 py-4 whitespace-nowrap">
                            <input
                              type="checkbox"
                              checked={selectedScenarios.has(scenario.scenario_id)}
                              onChange={(e) => {
                                const newSelected = new Set(selectedScenarios);
                                if (e.target.checked) {
                                  newSelected.add(scenario.scenario_id);
                                } else {
                                  newSelected.delete(scenario.scenario_id);
                                }
                                setSelectedScenarios(newSelected);
                              }}
                              className="rounded border-gray-300 text-blue-600 focus:ring-blue-500"
                            />
                          </td>
                          <td className="px-6 py-4 whitespace-nowrap">
                            <div className="flex items-center">
                              <div className="flex-shrink-0 h-10 w-10">
                                <div className="h-10 w-10 rounded-full bg-gray-200 flex items-center justify-center">
                                  <TypeIcon className="h-5 w-5 text-gray-600" />
                                </div>
                              </div>
                              <div className="ml-4">
                                <div className="text-sm font-medium text-gray-900">
                                  {scenario.scenario_name}
                                </div>
                                <div className="text-sm text-gray-500 line-clamp-1">
                                  {scenario.description}
                                </div>
                              </div>
                            </div>
                          </td>
                          <td className="px-6 py-4 whitespace-nowrap">
                            <span className={clsx(
                              'inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium',
                              getScenarioTypeInfo(scenario.scenario_type).color
                            )}>
                              {getScenarioTypeInfo(scenario.scenario_type).label}
                            </span>
                          </td>
                          <td className="px-6 py-4 whitespace-nowrap">
                            <div className="space-y-1">
                              <span className={clsx(
                                'inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium',
                                getApprovalStatusInfo(scenario.approval_status).color
                              )}>
                                {getApprovalStatusInfo(scenario.approval_status).label}
                              </span>
                              <div className="flex items-center gap-1">
                                <div className={clsx(
                                  'w-2 h-2 rounded-full',
                                  scenario.is_active ? 'bg-green-500' : 'bg-gray-400'
                                )} />
                                <span className="text-xs text-gray-500">
                                  {scenario.is_active ? 'Active' : 'Inactive'}
                                </span>
                              </div>
                            </div>
                          </td>
                          <td className="px-6 py-4 whitespace-nowrap">
                            <div className="space-y-1">
                              <div className="flex items-center gap-2">
                                <Target className="w-4 h-4 text-gray-400" />
                                <span className="text-sm font-medium">{scenario.success_rate}%</span>
                              </div>
                              <div className="text-xs text-gray-500">
                                {scenario.execution_count} executions
                              </div>
                              <div className={clsx('text-xs font-medium', getHealthColor(healthScore))}>
                                Health: {healthScore}%
                              </div>
                            </div>
                          </td>
                          <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">
                            v{scenario.version}
                            {scenario.parent_scenario_id && (
                              <div className="text-xs text-gray-500">Forked</div>
                            )}
                          </td>
                          <td className="px-6 py-4 whitespace-nowrap text-sm font-medium">
                            <div className="flex items-center gap-2">
                              <button
                                onClick={() => handleScenarioAction('edit', scenario)}
                                className="text-blue-600 hover:text-blue-900"
                              >
                                <Edit className="w-4 h-4" />
                              </button>
                              <button
                                onClick={() => handleScenarioAction('clone', scenario)}
                                className="text-green-600 hover:text-green-900"
                              >
                                <Copy className="w-4 h-4" />
                              </button>
                              <button
                                onClick={() => handleScenarioAction(scenario.is_active ? 'archive' : 'activate', scenario)}
                                className={scenario.is_active ? 'text-yellow-600 hover:text-yellow-900' : 'text-green-600 hover:text-green-900'}
                              >
                                {scenario.is_active ? <Archive className="w-4 h-4" /> : <RotateCcw className="w-4 h-4" />}
                              </button>
                              <div className="relative">
                                <button className="text-gray-400 hover:text-gray-600">
                                  <MoreVertical className="w-4 h-4" />
                                </button>
                              </div>
                            </div>
                          </td>
                        </tr>
                      );
                    })}
                  </tbody>
                </table>
              </div>

              {filteredScenarios.length === 0 && (
                <div className="p-12 text-center text-gray-500">
                  <FileText className="w-12 h-12 mx-auto mb-4 text-gray-400" />
                  <h3 className="text-lg font-medium text-gray-900 mb-2">No scenarios found</h3>
                  <p className="text-sm">
                    Try adjusting your filters or create a new scenario.
                  </p>
                </div>
              )}
            </div>
          </div>
        )}

        {activeTab === 'workflows' && (
          <div className="space-y-6">
            <div className="flex items-center justify-between">
              <h2 className="text-xl font-semibold text-gray-900">Simulation Workflows</h2>
              <button className="flex items-center gap-2 px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors">
                <Plus className="w-4 h-4" />
                Create Workflow
              </button>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
              {workflows.map((workflow) => (
                <div key={workflow.workflow_id} className="bg-white rounded-lg border shadow-sm p-6">
                  <div className="flex items-start justify-between mb-4">
                    <div className="flex-1">
                      <h3 className="font-semibold text-gray-900">{workflow.workflow_name}</h3>
                      <p className="text-sm text-gray-600 mt-1">{workflow.description}</p>
                    </div>
                    <div className="flex items-center gap-1">
                      {workflow.is_active ? (
                        <CheckCircle className="w-4 h-4 text-green-500" />
                      ) : (
                        <XCircle className="w-4 h-4 text-gray-400" />
                      )}
                    </div>
                  </div>

                  <div className="space-y-2 mb-4">
                    <div className="flex items-center gap-2 text-sm text-gray-600">
                      <Workflow className="w-4 h-4" />
                      <span>{workflow.steps.length} steps</span>
                    </div>
                    <div className="flex items-center gap-2 text-sm text-gray-600">
                      <BarChart3 className="w-4 h-4" />
                      <span>{workflow.execution_count} executions • {workflow.success_rate}% success</span>
                    </div>
                  </div>

                  <div className="flex items-center gap-2">
                    <button
                      onClick={() => onWorkflowExecute?.(workflow.workflow_id)}
                      className="flex-1 bg-blue-600 hover:bg-blue-700 text-white px-3 py-2 rounded-lg text-sm font-medium transition-colors"
                    >
                      Execute Workflow
                    </button>
                    <button className="p-2 text-gray-400 hover:text-gray-600 transition-colors">
                      <Edit className="w-4 h-4" />
                    </button>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {activeTab === 'schedules' && (
          <div className="space-y-6">
            <div className="flex items-center justify-between">
              <h2 className="text-xl font-semibold text-gray-900">Scheduled Simulations</h2>
              <button
                onClick={() => onScheduleCreate?.({})}
                className="flex items-center gap-2 px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors"
              >
                <Plus className="w-4 h-4" />
                Create Schedule
              </button>
            </div>

            <div className="bg-white rounded-lg border shadow-sm overflow-hidden">
              <div className="overflow-x-auto">
                <table className="min-w-full divide-y divide-gray-200">
                  <thead className="bg-gray-50">
                    <tr>
                      <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                        Schedule
                      </th>
                      <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                        Scenario
                      </th>
                      <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                        Type
                      </th>
                      <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                        Next Run
                      </th>
                      <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                        Status
                      </th>
                      <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                        Actions
                      </th>
                    </tr>
                  </thead>
                  <tbody className="bg-white divide-y divide-gray-200">
                    {schedules.map((schedule) => (
                      <tr key={schedule.schedule_id}>
                        <td className="px-6 py-4 whitespace-nowrap">
                          <div>
                            <div className="text-sm font-medium text-gray-900">
                              {schedule.schedule_name}
                            </div>
                            <div className="text-sm text-gray-500">
                              {schedule.cron_expression}
                            </div>
                          </div>
                        </td>
                        <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">
                          {scenarios.find(s => s.scenario_id === schedule.scenario_id)?.scenario_name || 'Unknown'}
                        </td>
                        <td className="px-6 py-4 whitespace-nowrap">
                          <span className="capitalize text-sm text-gray-900">
                            {schedule.schedule_type.replace('_', ' ')}
                          </span>
                        </td>
                        <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">
                          {schedule.next_run_at ? format(new Date(schedule.next_run_at), 'MMM dd, HH:mm') : 'N/A'}
                        </td>
                        <td className="px-6 py-4 whitespace-nowrap">
                          <span className={clsx(
                            'inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium',
                            schedule.is_active ? 'bg-green-100 text-green-800' : 'bg-gray-100 text-gray-800'
                          )}>
                            {schedule.is_active ? 'Active' : 'Inactive'}
                          </span>
                        </td>
                        <td className="px-6 py-4 whitespace-nowrap text-sm font-medium">
                          <div className="flex items-center gap-2">
                            <button className="text-blue-600 hover:text-blue-900">
                              <Edit className="w-4 h-4" />
                            </button>
                            <button className={schedule.is_active ? 'text-yellow-600 hover:text-yellow-900' : 'text-green-600 hover:text-green-900'}>
                              {schedule.is_active ? <Pause className="w-4 h-4" /> : <Play className="w-4 h-4" />}
                            </button>
                          </div>
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'analytics' && (
          <div className="space-y-6">
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <h2 className="text-xl font-semibold text-gray-900 mb-6">Management Analytics</h2>
              <div className="text-center py-12 text-gray-500">
                <BarChart3 className="w-12 h-12 mx-auto mb-4 text-gray-400" />
                <h3 className="text-lg font-medium text-gray-900 mb-2">Advanced Analytics</h3>
                <p className="text-sm">
                  Comprehensive analytics for scenario performance, workflow efficiency, and scheduling metrics.<br />
                  <em className="text-blue-600">Production-grade management analytics with predictive insights</em>
                </p>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'settings' && (
          <div className="space-y-6">
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <h2 className="text-xl font-semibold text-gray-900 mb-6">Management Settings</h2>

              <div className="space-y-6">
                <div>
                  <h3 className="text-sm font-medium text-gray-900 mb-3">Approval Workflow</h3>
                  <div className="space-y-3">
                    <div className="flex items-center justify-between">
                      <span className="text-sm text-gray-700">Require approval for new scenarios</span>
                      <input type="checkbox" defaultChecked className="rounded border-gray-300" />
                    </div>
                    <div className="flex items-center justify-between">
                      <span className="text-sm text-gray-700">Auto-approve scenarios from templates</span>
                      <input type="checkbox" className="rounded border-gray-300" />
                    </div>
                    <div className="flex items-center justify-between">
                      <span className="text-sm text-gray-700">Require review for scenario updates</span>
                      <input type="checkbox" defaultChecked className="rounded border-gray-300" />
                    </div>
                  </div>
                </div>

                <div>
                  <h3 className="text-sm font-medium text-gray-900 mb-3">Automation Settings</h3>
                  <div className="space-y-3">
                    <div className="flex items-center justify-between">
                      <span className="text-sm text-gray-700">Auto-archive inactive scenarios</span>
                      <input type="checkbox" className="rounded border-gray-300" />
                    </div>
                    <div className="flex items-center justify-between">
                      <span className="text-sm text-gray-700">Send notifications for failed simulations</span>
                      <input type="checkbox" defaultChecked className="rounded border-gray-300" />
                    </div>
                    <div className="flex items-center justify-between">
                      <span className="text-sm text-gray-700">Enable predictive scenario suggestions</span>
                      <input type="checkbox" className="rounded border-gray-300" />
                    </div>
                  </div>
                </div>

                <div>
                  <h3 className="text-sm font-medium text-gray-900 mb-3">Performance Monitoring</h3>
                  <div className="space-y-3">
                    <div className="flex items-center justify-between">
                      <span className="text-sm text-gray-700">Monitor scenario health scores</span>
                      <input type="checkbox" defaultChecked className="rounded border-gray-300" />
                    </div>
                    <div className="flex items-center justify-between">
                      <span className="text-sm text-gray-700">Alert on low success rates</span>
                      <input type="checkbox" defaultChecked className="rounded border-gray-300" />
                    </div>
                    <div className="flex items-center justify-between">
                      <span className="text-sm text-gray-700">Track execution performance metrics</span>
                      <input type="checkbox" defaultChecked className="rounded border-gray-300" />
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default SimulatorManagement;
