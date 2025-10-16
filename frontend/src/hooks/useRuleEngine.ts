/**
 * Advanced Rule Engine API Hooks
 * Production-grade hooks for fraud detection rule management
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import apiClient from '../services/api';

// Types based on the backend API
export interface RuleDefinition {
  rule_id: string;
  name: string;
  description: string;
  priority: 'LOW' | 'MEDIUM' | 'HIGH' | 'CRITICAL';
  rule_type: 'VALIDATION' | 'SCORING' | 'PATTERN' | 'MACHINE_LEARNING';
  rule_logic: any;
  parameters: any;
  input_fields: string[];
  output_fields: string[];
  is_active: boolean;
  valid_from?: string;
  valid_until?: string;
  created_by: string;
  created_at: string;
  updated_at: string;
}

export interface RuleExecutionResultDetail {
  rule_id: string;
  rule_name: string;
  result: 'PASS' | 'FAIL' | 'ERROR';
  confidence_score: number;
  risk_level: 'LOW' | 'MEDIUM' | 'HIGH' | 'CRITICAL';
  execution_time_ms: number;
  triggered_conditions?: string[];
  error_message?: string;
}

export interface FraudDetectionResult {
  transaction_id: string;
  is_fraudulent: boolean;
  overall_risk: 'LOW' | 'MEDIUM' | 'HIGH' | 'CRITICAL';
  fraud_score: number;
  rule_results: RuleExecutionResultDetail[];
  aggregated_findings: any;
  detection_time: string;
  processing_duration: string;
  recommendation: string;
}

export interface RulePerformanceMetrics {
  rule_id: string;
  total_executions: number;
  successful_executions: number;
  failed_executions: number;
  fraud_detections: number;
  false_positives: number;
  average_execution_time_ms: number;
  average_confidence_score: number;
  last_execution: string;
  error_counts: Record<string, number>;
}

export interface RuleExecutionContext {
  transaction_id: string;
  user_id: string;
  session_id?: string;
  transaction_data: any;
  user_profile?: any;
  historical_data?: any;
  execution_time: string;
  source_system?: string;
  metadata?: Record<string, string>;
}

// Rule Management Hooks
export const useRules = (params?: {
  type?: string;
  priority?: string;
  active_only?: boolean;
  search?: string;
}) => {
  return useQuery({
    queryKey: ['rules', params],
    queryFn: async () => {
      const response = await apiClient.get('/rules', { params });
      return response.data;
    },
  });
};

export const useRule = (ruleId: string) => {
  return useQuery({
    queryKey: ['rules', ruleId],
    queryFn: async () => {
      const response = await apiClient.get(`/rules/${ruleId}`);
      return response.data as RuleDefinition;
    },
    enabled: !!ruleId,
  });
};

export const useCreateRule = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (ruleData: Partial<RuleDefinition>) => {
      const response = await apiClient.post('/rules', ruleData);
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['rules'] });
    },
  });
};

export const useUpdateRule = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({ ruleId, updates }: { ruleId: string; updates: Partial<RuleDefinition> }) => {
      const response = await apiClient.put(`/rules/${ruleId}`, updates);
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['rules'] });
    },
  });
};

export const useDeleteRule = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (ruleId: string) => {
      await apiClient.delete(`/rules/${ruleId}`);
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['rules'] });
    },
  });
};

export const useDeactivateRule = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (ruleId: string) => {
      const response = await apiClient.post(`/rules/${ruleId}/deactivate`);
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['rules'] });
    },
  });
};

// Transaction Evaluation Hooks
export const useEvaluateTransaction = () => {
  return useMutation({
    mutationFn: async (context: RuleExecutionContext) => {
      const response = await apiClient.post('/rules/evaluate-transaction', context);
      return response.data as FraudDetectionResult;
    },
  });
};

export const useBatchEvaluateTransactions = () => {
  return useMutation({
    mutationFn: async (contexts: RuleExecutionContext[]) => {
      const response = await apiClient.post('/rules/batch-evaluate', { transactions: contexts });
      return response.data;
    },
  });
};

export const useBatchResults = (batchId: string) => {
  return useQuery({
    queryKey: ['batch-results', batchId],
    queryFn: async () => {
      const response = await apiClient.get(`/rules/batch/${batchId}/results`);
      return response.data as FraudDetectionResult[];
    },
    enabled: !!batchId,
  });
};

export const useBatchProgress = (batchId: string) => {
  return useQuery({
    queryKey: ['batch-progress', batchId],
    queryFn: async () => {
      const response = await apiClient.get(`/rules/batch/${batchId}/progress`);
      return response.data;
    },
    enabled: !!batchId,
    refetchInterval: 2000, // Poll every 2 seconds
  });
};

// Rule Testing and Validation Hooks
export const useExecuteRule = () => {
  return useMutation({
    mutationFn: async ({ ruleId, context }: { ruleId: string; context: RuleExecutionContext }) => {
      const response = await apiClient.post(`/rules/${ruleId}/execute`, context);
      return response.data as RuleExecutionResultDetail;
    },
  });
};

export const useTestRule = () => {
  return useMutation({
    mutationFn: async ({ ruleId, testContext }: { ruleId: string; testContext: RuleExecutionContext }) => {
      const response = await apiClient.post(`/rules/${ruleId}/test`, testContext);
      return response.data;
    },
  });
};

export const useValidateRuleLogic = () => {
  return useMutation({
    mutationFn: async (ruleLogic: any) => {
      const response = await apiClient.post('/rules/validate-logic', { rule_logic: ruleLogic });
      return response.data;
    },
  });
};

// Performance and Analytics Hooks
export const useRuleMetrics = (ruleId?: string) => {
  return useQuery({
    queryKey: ['rule-metrics', ruleId],
    queryFn: async () => {
      if (ruleId) {
        const response = await apiClient.get(`/rules/${ruleId}/metrics`);
        return response.data as RulePerformanceMetrics;
      } else {
        const response = await apiClient.get('/rules/metrics');
        return response.data as RulePerformanceMetrics[];
      }
    },
  });
};

export const useResetRuleMetrics = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (ruleId?: string) => {
      const response = await apiClient.post('/rules/reset-metrics', { rule_id: ruleId });
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['rule-metrics'] });
    },
  });
};

export const useFraudDetectionStats = (params?: {
  start_date?: string;
  end_date?: string;
  group_by?: string;
}) => {
  return useQuery({
    queryKey: ['fraud-stats', params],
    queryFn: async () => {
      const response = await apiClient.get('/rules/fraud-stats', { params });
      return response.data;
    },
  });
};

// Configuration and Management Hooks
export const useReloadRules = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async () => {
      const response = await apiClient.post('/rules/reload');
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['rules'] });
    },
  });
};

export const useEngineStatus = () => {
  return useQuery({
    queryKey: ['engine-status'],
    queryFn: async () => {
      const response = await apiClient.get('/rules/engine/status');
      return response.data;
    },
    refetchInterval: 30000, // Refresh every 30 seconds
  });
};

export const useUpdateEngineConfig = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (config: any) => {
      const response = await apiClient.put('/rules/engine/config', config);
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['engine-status'] });
    },
  });
};

// Utility hooks for common operations
export const useActiveRules = () => {
  return useQuery({
    queryKey: ['rules', 'active'],
    queryFn: async () => {
      const response = await apiClient.get('/rules/active');
      return response.data as RuleDefinition[];
    },
  });
};

export const useRulesByType = (ruleType: string) => {
  return useQuery({
    queryKey: ['rules', 'by-type', ruleType],
    queryFn: async () => {
      const response = await apiClient.get(`/rules/type/${ruleType}`);
      return response.data as RuleDefinition[];
    },
    enabled: !!ruleType,
  });
};

export const useTopFraudRules = (limit: number = 10) => {
  return useQuery({
    queryKey: ['rules', 'top-fraud', limit],
    queryFn: async () => {
      const response = await apiClient.get('/rules/analytics/top-fraud', { params: { limit } });
      return response.data;
    },
  });
};
