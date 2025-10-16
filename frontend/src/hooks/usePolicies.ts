/**
 * Policy Generation API Hooks
 * Production-grade hooks for policy generation operations
 * NO MOCKS - Real API integration following RESTful conventions
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { api } from '@/services/api';

// Types matching backend policy generation system
export interface PolicyCondition {
  id?: string;
  field: string;
  operator: 'equals' | 'not_equals' | 'greater_than' | 'less_than' | 'contains' | 'starts_with' | 'ends_with' | 'regex' | 'in' | 'between';
  value: string | number | string[] | boolean;
  logicalOperator?: 'AND' | 'OR';
  metadata?: Record<string, any>;
}

export interface PolicyAction {
  id?: string;
  type: 'flag_transaction' | 'block_transaction' | 'notify_compliance' | 'escalate_to_team' | 'log_event' | 'custom_action' | 'approve_transaction' | 'reject_transaction';
  parameters?: Record<string, any>;
  priority?: 'low' | 'medium' | 'high' | 'critical';
  metadata?: Record<string, any>;
}

export interface PolicyRule {
  id?: string;
  name: string;
  description: string;
  conditions: PolicyCondition[];
  actions: PolicyAction[];
  enabled: boolean;
  priority: 'low' | 'medium' | 'high' | 'critical';
  metadata?: Record<string, any>;
}

export interface GeneratedPolicy {
  id: string;
  name: string;
  description: string;
  policyType: 'validation_rule' | 'business_rule' | 'compliance_rule' | 'risk_rule' | 'audit_rule' | 'workflow_rule';
  naturalLanguageDescription: string;
  generatedRules: PolicyRule[];
  status: 'draft' | 'validated' | 'deployed' | 'archived';
  version: string;
  tags: string[];
  createdAt: string;
  updatedAt: string;
  createdBy: string;
  lastValidatedAt?: string;
  validationStatus?: 'pending' | 'passed' | 'failed' | 'warnings';
  deploymentStatus?: 'not_deployed' | 'deploying' | 'deployed' | 'failed';
  deployedEnvironments?: string[];
  metadata?: Record<string, any>;
}

export interface PolicyTemplate {
  id: string;
  name: string;
  description: string;
  category: string;
  policyType: 'validation_rule' | 'business_rule' | 'compliance_rule' | 'risk_rule' | 'audit_rule' | 'workflow_rule';
  template: {
    conditions: PolicyCondition[];
    actions: PolicyAction[];
    metadata?: Record<string, any>;
  };
  variables: Array<{
    name: string;
    type: 'string' | 'number' | 'boolean' | 'array' | 'object';
    description: string;
    required: boolean;
    default?: any;
    validation?: Record<string, any>;
  }>;
  tags: string[];
  isPublic: boolean;
  usageCount: number;
  createdAt: string;
  updatedAt: string;
  createdBy: string;
  metadata?: Record<string, any>;
}

export interface ValidationResult {
  policyId: string;
  overallStatus: 'valid' | 'invalid' | 'needs_review';
  validationResults: Array<{
    id: string;
    type: 'syntax' | 'logic' | 'security' | 'compliance' | 'performance' | 'test_execution';
    status: 'pending' | 'running' | 'passed' | 'failed' | 'warning';
    title: string;
    description: string;
    details?: string;
    suggestions?: string[];
    executionTime?: number;
    severity: 'low' | 'medium' | 'high' | 'critical';
    category: string;
    lineNumber?: number;
    columnNumber?: number;
  }>;
  testResults: Array<{
    id: string;
    name: string;
    description: string;
    input: Record<string, any>;
    expectedOutput: Record<string, any>;
    status: 'pending' | 'running' | 'passed' | 'failed';
    executionTime?: number;
    errorMessage?: string;
  }>;
  summary: {
    totalValidations: number;
    passedValidations: number;
    failedValidations: number;
    warningValidations: number;
    totalTests: number;
    passedTests: number;
    failedTests: number;
    executionTime: number;
    confidenceScore: number;
  };
  recommendations: string[];
  generatedAt: string;
}

export interface DeploymentTarget {
  id: string;
  name: string;
  environment: 'development' | 'staging' | 'production';
  region: string;
  status: 'healthy' | 'degraded' | 'unhealthy' | 'maintenance';
  version: string;
  lastDeployment?: string;
  requiresApproval: boolean;
  approvers: string[];
}

export interface DeploymentStatus {
  deploymentId: string;
  overallStatus: 'pending' | 'running' | 'completed' | 'failed' | 'rolled_back';
  currentStep?: string;
  progress: number;
  steps: Array<{
    id: string;
    name: string;
    description: string;
    status: 'pending' | 'running' | 'completed' | 'failed' | 'skipped';
    startTime?: string;
    endTime?: string;
    duration?: number;
    logs: string[];
    errorMessage?: string;
  }>;
  startTime: string;
  estimatedCompletion?: string;
  deployedVersion?: string;
  rollbackAvailable: boolean;
}

export interface GeneratePolicyRequest {
  naturalLanguageDescription: string;
  policyType: 'validation_rule' | 'business_rule' | 'compliance_rule' | 'risk_rule' | 'audit_rule' | 'workflow_rule';
  context?: Record<string, any>;
  requirements?: string[];
  constraints?: string[];
  examples?: Array<{
    input: Record<string, any>;
    expectedOutput: Record<string, any>;
    description: string;
  }>;
  metadata?: Record<string, any>;
}

export interface CreatePolicyRequest {
  name: string;
  description: string;
  policyType: 'validation_rule' | 'business_rule' | 'compliance_rule' | 'risk_rule' | 'audit_rule' | 'workflow_rule';
  naturalLanguageDescription: string;
  rules: PolicyRule[];
  tags?: string[];
  metadata?: Record<string, any>;
}

export interface UpdatePolicyRequest extends Partial<CreatePolicyRequest> {
  id: string;
}

export interface PolicyFilters {
  policyType?: string;
  status?: string;
  tags?: string[];
  search?: string;
  createdBy?: string;
  dateRange?: {
    start: string;
    end: string;
  };
}

export interface CreateTemplateRequest {
  name: string;
  description: string;
  category: string;
  policyType: 'validation_rule' | 'business_rule' | 'compliance_rule' | 'risk_rule' | 'audit_rule' | 'workflow_rule';
  template: {
    conditions: PolicyCondition[];
    actions: PolicyAction[];
    metadata?: Record<string, any>;
  };
  variables: Array<{
    name: string;
    type: 'string' | 'number' | 'boolean' | 'array' | 'object';
    description: string;
    required: boolean;
    default?: any;
    validation?: Record<string, any>;
  }>;
  tags?: string[];
  isPublic?: boolean;
  metadata?: Record<string, any>;
}

export interface UpdateTemplateRequest extends Partial<CreateTemplateRequest> {
  id: string;
}

export interface DeployPolicyRequest {
  policyId: string;
  targetEnvironment: string;
  deploymentNotes?: string;
  force?: boolean;
  rollbackOnFailure?: boolean;
}

export interface ValidatePolicyRequest {
  policyCode: string;
  policyLanguage: 'json' | 'yaml' | 'javascript' | 'python';
  policyType: 'validation_rule' | 'business_rule' | 'compliance_rule' | 'risk_rule' | 'audit_rule' | 'workflow_rule';
  testData?: Record<string, any>[];
}

// Query Keys
export const policyKeys = {
  all: ['policies'] as const,
  lists: () => [...policyKeys.all, 'list'] as const,
  list: (filters: PolicyFilters) => [...policyKeys.lists(), filters] as const,
  details: () => [...policyKeys.all, 'detail'] as const,
  detail: (id: string) => [...policyKeys.details(), id] as const,
  generation: () => [...policyKeys.all, 'generation'] as const,
  templates: () => [...policyKeys.all, 'templates'] as const,
  template: (id: string) => [...policyKeys.templates(), id] as const,
  validation: () => [...policyKeys.all, 'validation'] as const,
  validationResult: (id: string) => [...policyKeys.validation(), id] as const,
  deployment: () => [...policyKeys.all, 'deployment'] as const,
  deploymentTargets: () => [...policyKeys.deployment(), 'targets'] as const,
  deploymentStatus: (id: string) => [...policyKeys.deployment(), 'status', id] as const,
};

// Generate policy from natural language description
export const useGeneratePolicy = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (request: GeneratePolicyRequest) => {
      const response = await api.post<GeneratedPolicy>('/policies/generate', request);
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: policyKeys.lists() });
    },
  });
};

// Get all policies with filtering
export const usePolicies = (filters?: PolicyFilters) => {
  return useQuery({
    queryKey: policyKeys.list(filters || {}),
    queryFn: async () => {
      const params = new URLSearchParams();

      if (filters?.policyType) params.append('policyType', filters.policyType);
      if (filters?.status) params.append('status', filters.status);
      if (filters?.tags?.length) params.append('tags', filters.tags.join(','));
      if (filters?.search) params.append('search', filters.search);
      if (filters?.createdBy) params.append('createdBy', filters.createdBy);
      if (filters?.dateRange) {
        params.append('startDate', filters.dateRange.start);
        params.append('endDate', filters.dateRange.end);
      }

      const response = await api.get<GeneratedPolicy[]>(`/policies?${params.toString()}`);
      return response.data;
    },
    staleTime: 5 * 60 * 1000, // 5 minutes
  });
};

// Get single policy by ID
export const usePolicy = (id: string) => {
  return useQuery({
    queryKey: policyKeys.detail(id),
    queryFn: async () => {
      const response = await api.get<GeneratedPolicy>(`/policies/${id}`);
      return response.data;
    },
    enabled: !!id,
    staleTime: 2 * 60 * 1000, // 2 minutes
  });
};

// Create new policy
export const useCreatePolicy = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (policy: CreatePolicyRequest) => {
      const response = await api.post<GeneratedPolicy>('/policies', policy);
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: policyKeys.lists() });
    },
  });
};

// Update existing policy
export const useUpdatePolicy = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({ id, policy }: { id: string; policy: UpdatePolicyRequest }) => {
      const response = await api.put<GeneratedPolicy>(`/policies/${id}`, policy);
      return response.data;
    },
    onSuccess: (data) => {
      queryClient.invalidateQueries({ queryKey: policyKeys.lists() });
      queryClient.invalidateQueries({ queryKey: policyKeys.detail(data.id) });
    },
  });
};

// Delete policy
export const useDeletePolicy = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (id: string) => {
      await api.delete(`/policies/${id}`);
      return id;
    },
    onSuccess: (id) => {
      queryClient.invalidateQueries({ queryKey: policyKeys.lists() });
      queryClient.removeQueries({ queryKey: policyKeys.detail(id) });
    },
  });
};

// Clone existing policy
export const useClonePolicy = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (id: string) => {
      const response = await api.post<GeneratedPolicy>(`/policies/${id}/clone`);
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: policyKeys.lists() });
    },
  });
};

// Export policy
export const useExportPolicy = () => {
  return useMutation({
    mutationFn: async ({ id, format = 'json' }: { id: string; format?: 'json' | 'yaml' }) => {
      const response = await api.get(`/policies/${id}/export?format=${format}`, {
        responseType: 'blob',
      });
      return { data: response.data, filename: `policy-${id}.${format}` };
    },
  });
};

// Import policy
export const useImportPolicy = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (file: File) => {
      const formData = new FormData();
      formData.append('file', file);

      const response = await api.post<GeneratedPolicy>('/policies/import', formData, {
        headers: {
          'Content-Type': 'multipart/form-data',
        },
      });
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: policyKeys.lists() });
    },
  });
};

// Validate policy
export const useValidatePolicy = () => {
  return useMutation({
    mutationFn: async (request: ValidatePolicyRequest) => {
      const response = await api.post<ValidationResult>('/policies/validate', request);
      return response.data;
    },
  });
};

// Run policy validation checks
export const useRunPolicyValidation = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (id: string) => {
      const response = await api.post<ValidationResult>(`/policies/${id}/validate`);
      return response.data;
    },
    onSuccess: (result, id) => {
      queryClient.invalidateQueries({ queryKey: policyKeys.detail(id) });
      queryClient.invalidateQueries({ queryKey: policyKeys.validationResult(id) });
    },
  });
};

// Get policy validation results
export const usePolicyValidationResult = (id: string) => {
  return useQuery({
    queryKey: policyKeys.validationResult(id),
    queryFn: async () => {
      const response = await api.get<ValidationResult>(`/policies/${id}/validation`);
      return response.data;
    },
    enabled: !!id,
    staleTime: 60 * 1000, // 1 minute
  });
};

// Get all policy templates
export const usePolicyTemplates = () => {
  return useQuery({
    queryKey: policyKeys.templates(),
    queryFn: async () => {
      const response = await api.get<PolicyTemplate[]>('/policies/templates');
      return response.data;
    },
    staleTime: 10 * 60 * 1000, // 10 minutes
  });
};

// Get single template by ID
export const usePolicyTemplate = (id: string) => {
  return useQuery({
    queryKey: policyKeys.template(id),
    queryFn: async () => {
      const response = await api.get<PolicyTemplate>(`/policies/templates/${id}`);
      return response.data;
    },
    enabled: !!id,
    staleTime: 5 * 60 * 1000, // 5 minutes
  });
};

// Create policy template
export const useCreatePolicyTemplate = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (template: CreateTemplateRequest) => {
      const response = await api.post<PolicyTemplate>('/policies/templates', template);
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: policyKeys.templates() });
    },
  });
};

// Update policy template
export const useUpdatePolicyTemplate = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({ id, template }: { id: string; template: UpdateTemplateRequest }) => {
      const response = await api.put<PolicyTemplate>(`/policies/templates/${id}`, template);
      return response.data;
    },
    onSuccess: (data) => {
      queryClient.invalidateQueries({ queryKey: policyKeys.templates() });
      queryClient.invalidateQueries({ queryKey: policyKeys.template(data.id) });
    },
  });
};

// Delete policy template
export const useDeletePolicyTemplate = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (id: string) => {
      await api.delete(`/policies/templates/${id}`);
      return id;
    },
    onSuccess: (id) => {
      queryClient.invalidateQueries({ queryKey: policyKeys.templates() });
      queryClient.removeQueries({ queryKey: policyKeys.template(id) });
    },
  });
};

// Create policy from template
export const useCreatePolicyFromTemplate = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({
      templateId,
      variables,
      name,
      description
    }: {
      templateId: string;
      variables: Record<string, any>;
      name: string;
      description: string;
    }) => {
      const response = await api.post<GeneratedPolicy>('/policies/from-template', {
        templateId,
        variables,
        name,
        description,
      });
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: policyKeys.lists() });
    },
  });
};

// Get deployment targets
export const useDeploymentTargets = () => {
  return useQuery({
    queryKey: policyKeys.deploymentTargets(),
    queryFn: async () => {
      const response = await api.get<DeploymentTarget[]>('/policies/deployment/targets');
      return response.data;
    },
    staleTime: 30 * 1000, // 30 seconds
  });
};

// Deploy policy
export const useDeployPolicy = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (request: DeployPolicyRequest) => {
      const response = await api.post<DeploymentStatus>('/policies/deploy', request);
      return response.data;
    },
    onSuccess: (result, variables) => {
      queryClient.invalidateQueries({ queryKey: policyKeys.detail(variables.policyId) });
      queryClient.invalidateQueries({ queryKey: policyKeys.deploymentStatus(result.deploymentId) });
    },
  });
};

// Get deployment status
export const useDeploymentStatus = (deploymentId: string) => {
  return useQuery({
    queryKey: policyKeys.deploymentStatus(deploymentId),
    queryFn: async () => {
      const response = await api.get<DeploymentStatus>(`/policies/deployment/${deploymentId}/status`);
      return response.data;
    },
    enabled: !!deploymentId,
    refetchInterval: (data) => {
      // Stop polling when deployment is complete or failed
      return data?.overallStatus === 'completed' || data?.overallStatus === 'failed' || data?.overallStatus === 'rolled_back'
        ? false
        : 2000; // Poll every 2 seconds during deployment
    },
    staleTime: 1000, // 1 second
  });
};

// Rollback deployment
export const useRollbackDeployment = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (deploymentId: string) => {
      const response = await api.post<DeploymentStatus>(`/policies/deployment/${deploymentId}/rollback`);
      return response.data;
    },
    onSuccess: (result) => {
      queryClient.invalidateQueries({ queryKey: policyKeys.deploymentStatus(result.deploymentId) });
    },
  });
};

// Get deployment history for a policy
export const usePolicyDeploymentHistory = (policyId: string) => {
  return useQuery({
    queryKey: [...policyKeys.deployment(), 'history', policyId],
    queryFn: async () => {
      const response = await api.get<Array<{
        deploymentId: string;
        policyId: string;
        targetEnvironment: string;
        status: 'pending_approval' | 'approved' | 'rejected' | 'deploying' | 'deployed' | 'failed' | 'rolled_back';
        requestedBy: string;
        requestedAt: string;
        deployedAt?: string;
        deploymentNotes?: string;
        rollbackReason?: string;
      }>>(`/policies/${policyId}/deployment-history`);
      return response.data;
    },
    enabled: !!policyId,
    staleTime: 60 * 1000, // 1 minute
  });
};

// Request deployment approval
export const useRequestDeploymentApproval = () => {
  return useMutation({
    mutationFn: async ({
      policyId,
      targetEnvironment,
      deploymentNotes
    }: {
      policyId: string;
      targetEnvironment: string;
      deploymentNotes?: string;
    }) => {
      const response = await api.post<{
        requestId: string;
        status: 'pending_approval';
        approvers: string[];
      }>('/policies/deployment/request-approval', {
        policyId,
        targetEnvironment,
        deploymentNotes,
      });
      return response.data;
    },
  });
};

// Approve deployment request
export const useApproveDeployment = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (requestId: string) => {
      const response = await api.post<{ status: 'approved'; approvedBy: string }>(`/policies/deployment/${requestId}/approve`);
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: policyKeys.deployment() });
    },
  });
};

// Reject deployment request
export const useRejectDeployment = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({ requestId, reason }: { requestId: string; reason: string }) => {
      const response = await api.post<{ status: 'rejected'; reason: string }>(`/policies/deployment/${requestId}/reject`, {
        reason,
      });
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: policyKeys.deployment() });
    },
  });
};

// Execute policy (test execution)
export const useExecutePolicy = () => {
  return useMutation({
    mutationFn: async ({
      policyId,
      testData,
      context
    }: {
      policyId: string;
      testData: Record<string, any>;
      context?: Record<string, any>;
    }) => {
      const response = await api.post<{
        executionId: string;
        result: 'pass' | 'fail' | 'error';
        executionTime: number;
        details: Record<string, any>;
        triggeredActions: string[];
      }>(`/policies/${policyId}/execute`, {
        testData,
        context,
      });
      return response.data;
    },
  });
};

// Get policy execution history
export const usePolicyExecutionHistory = (policyId: string, limit = 50) => {
  return useQuery({
    queryKey: [...policyKeys.all, 'execution-history', policyId, limit],
    queryFn: async () => {
      const response = await api.get<Array<{
        executionId: string;
        policyId: string;
        executedAt: string;
        result: 'pass' | 'fail' | 'error';
        executionTime: number;
        testData: Record<string, any>;
        triggeredActions: string[];
        errorMessage?: string;
      }>>(`/policies/${policyId}/execution-history?limit=${limit}`);
      return response.data;
    },
    enabled: !!policyId,
    staleTime: 30 * 1000, // 30 seconds
  });
};

// Get policy analytics
export const usePolicyAnalytics = (policyId?: string) => {
  return useQuery({
    queryKey: policyId ? [...policyKeys.all, 'analytics', policyId] : [...policyKeys.all, 'analytics'],
    queryFn: async () => {
      const endpoint = policyId ? `/policies/${policyId}/analytics` : '/policies/analytics';
      const response = await api.get<{
        totalPolicies: number;
        activePolicies: number;
        deployedPolicies: number;
        averageValidationScore: number;
        totalExecutions: number;
        successRate: number;
        topPerformingPolicies: Array<{
          policyId: string;
          name: string;
          successRate: number;
          executionCount: number;
        }>;
        deploymentStats: {
          totalDeployments: number;
          successfulDeployments: number;
          failedDeployments: number;
        };
        policyTypeDistribution: Record<string, number>;
      }>(endpoint);
      return response.data;
    },
    enabled: policyId ? !!policyId : true,
    staleTime: 5 * 60 * 1000, // 5 minutes
  });
};

// Toggle policy enabled/disabled status
export const useTogglePolicy = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (id: string) => {
      const response = await api.patch<GeneratedPolicy>(`/policies/${id}/toggle`);
      return response.data;
    },
    onSuccess: (data) => {
      queryClient.invalidateQueries({ queryKey: policyKeys.lists() });
      queryClient.invalidateQueries({ queryKey: policyKeys.detail(data.id) });
    },
  });
};

// Archive policy
export const useArchivePolicy = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (id: string) => {
      const response = await api.patch<GeneratedPolicy>(`/policies/${id}/archive`);
      return response.data;
    },
    onSuccess: (data) => {
      queryClient.invalidateQueries({ queryKey: policyKeys.lists() });
      queryClient.invalidateQueries({ queryKey: policyKeys.detail(data.id) });
    },
  });
};

// Get policy dependencies (what this policy depends on)
export const usePolicyDependencies = (id: string) => {
  return useQuery({
    queryKey: [...policyKeys.detail(id), 'dependencies'],
    queryFn: async () => {
      const response = await api.get<Array<{
        dependencyId: string;
        dependencyType: 'policy' | 'rule' | 'template' | 'data_source';
        name: string;
        required: boolean;
        status: 'available' | 'missing' | 'outdated';
      }>>(`/policies/${id}/dependencies`);
      return response.data;
    },
    enabled: !!id,
    staleTime: 2 * 60 * 1000, // 2 minutes
  });
};

// Get policies that depend on this one
export const usePolicyDependents = (id: string) => {
  return useQuery({
    queryKey: [...policyKeys.detail(id), 'dependents'],
    queryFn: async () => {
      const response = await api.get<Array<{
        dependentId: string;
        dependentType: 'policy' | 'rule' | 'template';
        name: string;
        impact: 'low' | 'medium' | 'high';
      }>>(`/policies/${id}/dependents`);
      return response.data;
    },
    enabled: !!id,
    staleTime: 2 * 60 * 1000, // 2 minutes
  });
};
