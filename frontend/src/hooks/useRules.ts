/**
 * Rule Engine API Hooks
 * Production-grade hooks for rule engine operations
 * NO MOCKS - Real API integration following RESTful conventions
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { api } from '@/services/api';

// Types matching backend
export interface RuleCondition {
  field: string;
  operator: 'equals' | 'not_equals' | 'greater_than' | 'less_than' | 'contains' | 'starts_with' | 'ends_with' | 'regex' | 'in' | 'between';
  value: string | number | string[];
  logicalOperator?: 'AND' | 'OR';
}

export interface RuleAction {
  type: 'flag_transaction' | 'block_transaction' | 'notify_compliance' | 'escalate_to_team' | 'log_event' | 'custom_action';
  parameters?: Record<string, any>;
}

export interface Rule {
  id: string;
  name: string;
  description: string;
  ruleType: 'fraud_detection' | 'compliance' | 'risk_scoring' | 'policy_enforcement';
  priority: 'low' | 'medium' | 'high' | 'critical';
  executionMode: 'synchronous' | 'asynchronous' | 'batch' | 'streaming';
  conditions: RuleCondition[];
  actions: RuleAction[];
  enabled: boolean;
  tags: string[];
  createdAt: string;
  updatedAt: string;
  lastExecuted?: string;
  executionCount: number;
  successRate: number;
  averageExecutionTime: number;
  createdBy: string;
  version: number;
}

export interface RuleExecutionResult {
  executionId: string;
  ruleId: string;
  result: 'pass' | 'fail' | 'error' | 'timeout' | 'skipped';
  riskLevel?: 'low' | 'medium' | 'high' | 'critical';
  executionTime: number;
  timestamp: string;
  details?: string;
  alertsTriggered: number;
  errorMessage?: string;
}

export interface RuleAnalytics {
  ruleId: string;
  totalExecutions: number;
  successRate: number;
  averageExecutionTime: number;
  errorRate: number;
  alertsTriggered: number;
  falsePositives: number;
  falseNegatives: number;
  trend: 'improving' | 'declining' | 'stable';
  performanceScore: number;
  lastExecuted?: string;
  executionHistory: RuleExecutionResult[];
}

export interface CreateRuleRequest {
  name: string;
  description: string;
  ruleType: 'fraud_detection' | 'compliance' | 'risk_scoring' | 'policy_enforcement';
  priority: 'low' | 'medium' | 'high' | 'critical';
  executionMode: 'synchronous' | 'asynchronous' | 'batch' | 'streaming';
  conditions: RuleCondition[];
  actions: RuleAction[];
  enabled?: boolean;
  tags?: string[];
}

export interface UpdateRuleRequest extends Partial<CreateRuleRequest> {
  id: string;
}

export interface RuleFilters {
  ruleType?: string;
  priority?: string;
  enabled?: boolean;
  tags?: string[];
  search?: string;
}

export interface RuleExecutionRequest {
  ruleId: string;
  data: Record<string, any>;
  context?: Record<string, any>;
}

export interface BatchExecutionRequest {
  rules: string[];
  data: Record<string, any>;
  context?: Record<string, any>;
}

export interface TestRuleRequest {
  ruleId: string;
  testData: Record<string, any>;
  expectedResult?: 'pass' | 'fail';
}

// Query Keys
export const ruleKeys = {
  all: ['rules'] as const,
  lists: () => [...ruleKeys.all, 'list'] as const,
  list: (filters: RuleFilters) => [...ruleKeys.lists(), filters] as const,
  details: () => [...ruleKeys.all, 'detail'] as const,
  detail: (id: string) => [...ruleKeys.details(), id] as const,
  analytics: () => [...ruleKeys.all, 'analytics'] as const,
  analytic: (id: string) => [...ruleKeys.analytics(), id] as const,
  executions: () => [...ruleKeys.all, 'executions'] as const,
  execution: (id: string) => [...ruleKeys.executions(), id] as const,
};

// Get all rules with filtering
export const useRules = (filters?: RuleFilters) => {
  return useQuery({
    queryKey: ruleKeys.list(filters || {}),
    queryFn: async () => {
      const params = new URLSearchParams();

      if (filters?.ruleType) params.append('ruleType', filters.ruleType);
      if (filters?.priority) params.append('priority', filters.priority);
      if (filters?.enabled !== undefined) params.append('enabled', filters.enabled.toString());
      if (filters?.tags?.length) params.append('tags', filters.tags.join(','));
      if (filters?.search) params.append('search', filters.search);

      const response = await api.get<Rule[]>(`/rules?${params.toString()}`);
      return response.data;
    },
    staleTime: 5 * 60 * 1000, // 5 minutes
  });
};

// Get single rule by ID
export const useRule = (id: string) => {
  return useQuery({
    queryKey: ruleKeys.detail(id),
    queryFn: async () => {
      const response = await api.get<Rule>(`/rules/${id}`);
      return response.data;
    },
    enabled: !!id,
    staleTime: 2 * 60 * 1000, // 2 minutes
  });
};

// Create new rule
export const useCreateRule = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (rule: CreateRuleRequest) => {
      const response = await api.post<Rule>('/rules', rule);
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ruleKeys.lists() });
    },
  });
};

// Update existing rule
export const useUpdateRule = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({ id, rule }: { id: string; rule: UpdateRuleRequest }) => {
      const response = await api.put<Rule>(`/rules/${id}`, rule);
      return response.data;
    },
    onSuccess: (data) => {
      queryClient.invalidateQueries({ queryKey: ruleKeys.lists() });
      queryClient.invalidateQueries({ queryKey: ruleKeys.detail(data.id) });
    },
  });
};

// Delete rule
export const useDeleteRule = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (id: string) => {
      await api.delete(`/rules/${id}`);
      return id;
    },
    onSuccess: (id) => {
      queryClient.invalidateQueries({ queryKey: ruleKeys.lists() });
      queryClient.removeQueries({ queryKey: ruleKeys.detail(id) });
    },
  });
};

// Execute single rule
export const useExecuteRule = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (request: RuleExecutionRequest) => {
      const response = await api.post<RuleExecutionResult>(`/rules/${request.ruleId}/execute`, {
        data: request.data,
        context: request.context,
      });
      return response.data;
    },
    onSuccess: (result) => {
      // Invalidate rule details and analytics to reflect new execution
      queryClient.invalidateQueries({ queryKey: ruleKeys.detail(result.ruleId) });
      queryClient.invalidateQueries({ queryKey: ruleKeys.analytic(result.ruleId) });
    },
  });
};

// Batch execute rules
export const useBatchExecuteRules = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (request: BatchExecutionRequest) => {
      const response = await api.post<RuleExecutionResult[]>('/rules/batch-execute', request);
      return response.data;
    },
    onSuccess: () => {
      // Invalidate all rule queries since multiple rules were executed
      queryClient.invalidateQueries({ queryKey: ruleKeys.all });
    },
  });
};

// Test rule execution
export const useTestRule = () => {
  return useMutation({
    mutationFn: async (request: TestRuleRequest) => {
      const response = await api.post<{
        result: RuleExecutionResult;
        passed: boolean;
        expectedResult?: string;
        actualResult: string;
      }>(`/rules/${request.ruleId}/test`, {
        testData: request.testData,
        expectedResult: request.expectedResult,
      });
      return response.data;
    },
  });
};

// Get rule execution history
export const useRuleExecutionHistory = (ruleId: string, limit = 50) => {
  return useQuery({
    queryKey: [...ruleKeys.execution(ruleId), limit],
    queryFn: async () => {
      const response = await api.get<RuleExecutionResult[]>(`/rules/${ruleId}/history?limit=${limit}`);
      return response.data;
    },
    enabled: !!ruleId,
    staleTime: 30 * 1000, // 30 seconds
  });
};

// Get rule analytics
export const useRuleAnalytics = (ruleId?: string) => {
  return useQuery({
    queryKey: ruleId ? ruleKeys.analytic(ruleId) : ruleKeys.analytics(),
    queryFn: async () => {
      const endpoint = ruleId ? `/rules/${ruleId}/analytics` : '/rules/analytics';
      const response = await api.get<RuleAnalytics | RuleAnalytics[]>(endpoint);
      return response.data;
    },
    enabled: ruleId ? !!ruleId : true,
    staleTime: 2 * 60 * 1000, // 2 minutes
  });
};

// Toggle rule enabled/disabled status
export const useToggleRule = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (id: string) => {
      const response = await api.patch<Rule>(`/rules/${id}/toggle`);
      return response.data;
    },
    onSuccess: (data) => {
      queryClient.invalidateQueries({ queryKey: ruleKeys.lists() });
      queryClient.invalidateQueries({ queryKey: ruleKeys.detail(data.id) });
    },
  });
};

// Clone existing rule
export const useCloneRule = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (id: string) => {
      const response = await api.post<Rule>(`/rules/${id}/clone`);
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ruleKeys.lists() });
    },
  });
};

// Export rule configuration
export const useExportRule = () => {
  return useMutation({
    mutationFn: async ({ id, format = 'json' }: { id: string; format?: 'json' | 'yaml' }) => {
      const response = await api.get(`/rules/${id}/export?format=${format}`, {
        responseType: 'blob',
      });
      return { data: response.data, filename: `rule-${id}.${format}` };
    },
  });
};

// Import rule configuration
export const useImportRule = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (file: File) => {
      const formData = new FormData();
      formData.append('file', file);

      const response = await api.post<Rule>('/rules/import', formData, {
        headers: {
          'Content-Type': 'multipart/form-data',
        },
      });
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ruleKeys.lists() });
    },
  });
};

// Validate rule configuration
export const useValidateRule = () => {
  return useMutation({
    mutationFn: async (rule: CreateRuleRequest | UpdateRuleRequest) => {
      const response = await api.post<{
        valid: boolean;
        errors: string[];
        warnings: string[];
        suggestions: string[];
      }>('/rules/validate', rule);
      return response.data;
    },
  });
};

// Get rule templates
export const useRuleTemplates = () => {
  return useQuery({
    queryKey: [...ruleKeys.all, 'templates'],
    queryFn: async () => {
      const response = await api.get<Array<{
        id: string;
        name: string;
        description: string;
        category: string;
        ruleType: string;
        template: Record<string, any>;
        variables: Array<{
          name: string;
          type: string;
          description: string;
          required: boolean;
          default?: any;
        }>;
      }>>('/rules/templates');
      return response.data;
    },
    staleTime: 10 * 60 * 1000, // 10 minutes
  });
};

// Create rule from template
export const useCreateRuleFromTemplate = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({
      templateId,
      variables
    }: {
      templateId: string;
      variables: Record<string, any>
    }) => {
      const response = await api.post<Rule>('/rules/from-template', {
        templateId,
        variables,
      });
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ruleKeys.lists() });
    },
  });
};
