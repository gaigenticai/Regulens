/**
 * Fraud Detection Hooks
 * Real-time fraud rule management and detection
 * NO MOCKS - Real API integration
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import apiClient from '@/services/api';
import type { FraudRule, FraudAlert, FraudStats, CreateFraudRuleRequest } from '@/types/api';

/**
 * Fetch all fraud detection rules
 */
export function useFraudRules() {
  return useQuery({
    queryKey: ['fraud-rules'],
    queryFn: async () => {
      const data = await apiClient.getFraudRules();
      return data;
    },
  });
}

/**
 * Fetch single fraud rule
 */
export function useFraudRule(id: string | undefined) {
  return useQuery({
    queryKey: ['fraud-rule', id],
    queryFn: async () => {
      if (!id) throw new Error('Rule ID is required');
      const data = await apiClient.getFraudRule(id);
      return data;
    },
    enabled: !!id,
  });
}

/**
 * Create new fraud detection rule
 */
export function useCreateFraudRule() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (rule: CreateFraudRuleRequest) => {
      const data = await apiClient.createFraudRule(rule);
      return data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['fraud-rules'] });
    },
  });
}

/**
 * Update fraud detection rule
 */
export function useUpdateFraudRule() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({ id, rule }: { id: string; rule: Partial<CreateFraudRuleRequest> }) => {
      const data = await apiClient.updateFraudRule(id, rule);
      return data;
    },
    onSuccess: (updatedRule) => {
      queryClient.setQueryData(['fraud-rule', updatedRule.id], updatedRule);
      queryClient.invalidateQueries({ queryKey: ['fraud-rules'] });
    },
  });
}

/**
 * Delete fraud detection rule
 */
export function useDeleteFraudRule() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (id: string) => {
      await apiClient.deleteFraudRule(id);
      return id;
    },
    onSuccess: (deletedId) => {
      queryClient.invalidateQueries({ queryKey: ['fraud-rules'] });
      queryClient.removeQueries({ queryKey: ['fraud-rule', deletedId] });
    },
  });
}

/**
 * Enable/disable fraud rule
 */
export function useToggleFraudRule() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({ id, enabled }: { id: string; enabled: boolean }) => {
      const data = await apiClient.toggleFraudRule(id, enabled);
      return data;
    },
    onSuccess: (updatedRule) => {
      queryClient.setQueryData(['fraud-rule', updatedRule.id], updatedRule);
      queryClient.invalidateQueries({ queryKey: ['fraud-rules'] });
    },
  });
}

/**
 * Test fraud rule against historical data
 */
export function useTestFraudRule() {
  return useMutation({
    mutationFn: async ({ ruleId, timeRange }: { ruleId: string; timeRange: string }) => {
      const data = await apiClient.testFraudRule(ruleId, timeRange);
      return data;
    },
  });
}

/**
 * Fetch fraud alerts
 */
export function useFraudAlerts(params?: {
  limit?: number;
  severity?: 'low' | 'medium' | 'high' | 'critical';
  status?: 'new' | 'investigating' | 'confirmed' | 'false_positive' | 'resolved';
  startDate?: string;
  endDate?: string;
}) {
  const { limit = 50, severity, status, startDate, endDate } = params || {};

  return useQuery({
    queryKey: ['fraud-alerts', limit, severity, status, startDate, endDate],
    queryFn: async () => {
      const queryParams = new URLSearchParams();
      if (limit) queryParams.append('limit', limit.toString());
      if (severity) queryParams.append('severity', severity);
      if (status) queryParams.append('status', status);
      if (startDate) queryParams.append('start_date', startDate);
      if (endDate) queryParams.append('end_date', endDate);

      const data = await apiClient.getFraudAlerts(queryParams);
      return data;
    },
    refetchInterval: 30000, // Refresh every 30 seconds
  });
}

/**
 * Get single fraud alert
 */
export function useFraudAlert(id: string | undefined) {
  return useQuery({
    queryKey: ['fraud-alert', id],
    queryFn: async () => {
      if (!id) throw new Error('Alert ID is required');
      const data = await apiClient.getFraudAlert(id);
      return data;
    },
    enabled: !!id,
  });
}

/**
 * Update fraud alert status
 */
export function useUpdateFraudAlertStatus() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({
      id,
      status,
      notes,
    }: {
      id: string;
      status: FraudAlert['status'];
      notes?: string;
    }) => {
      const data = await apiClient.updateFraudAlertStatus(id, status, notes);
      return data;
    },
    onSuccess: (updatedAlert) => {
      queryClient.setQueryData(['fraud-alert', updatedAlert.id], updatedAlert);
      queryClient.invalidateQueries({ queryKey: ['fraud-alerts'] });
      queryClient.invalidateQueries({ queryKey: ['fraud-stats'] });
    },
  });
}

/**
 * Fetch fraud statistics
 */
export function useFraudStats(timeRange: '24h' | '7d' | '30d' = '7d') {
  return useQuery({
    queryKey: ['fraud-stats', timeRange],
    queryFn: async () => {
      const data = await apiClient.getFraudStats(timeRange);
      return data;
    },
    refetchInterval: 60000, // Refresh every minute
  });
}

/**
 * Get fraud detection models
 */
export function useFraudModels() {
  return useQuery({
    queryKey: ['fraud-models'],
    queryFn: async () => {
      const data = await apiClient.getFraudModels();
      return data;
    },
  });
}

/**
 * Train fraud detection model
 */
export function useTrainFraudModel() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (params: {
      modelType: string;
      trainingData: string;
      parameters?: Record<string, any>;
    }) => {
      const data = await apiClient.trainFraudModel(params);
      return data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['fraud-models'] });
    },
  });
}

/**
 * Get model performance metrics
 */
export function useModelPerformance(modelId: string | undefined) {
  return useQuery({
    queryKey: ['model-performance', modelId],
    queryFn: async () => {
      if (!modelId) throw new Error('Model ID is required');
      const data = await apiClient.getModelPerformance(modelId);
      return data;
    },
    enabled: !!modelId,
  });
}

/**
 * Run batch fraud scan
 */
export function useBatchFraudScan() {
  return useMutation({
    mutationFn: async (params: {
      transactionIds?: string[];
      dateRange?: { start: string; end: string };
      ruleIds?: string[];
    }) => {
      const data = await apiClient.runBatchFraudScan(params);
      return data;
    },
  });
}

/**
 * Export fraud report
 */
export function useExportFraudReport() {
  return useMutation({
    mutationFn: async (params: {
      format: 'pdf' | 'csv' | 'json';
      timeRange: string;
      includeAlerts?: boolean;
      includeRules?: boolean;
      includeStats?: boolean;
    }) => {
      const data = await apiClient.exportFraudReport(params);
      return data;
    },
  });
}
