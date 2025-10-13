/**
 * Alert System Hooks
 * Production-grade React Query hooks for alert management
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { apiClient } from '@/services/api';
import type * as API from '@/types/api';

export function useAlertRules() {
  return useQuery({
    queryKey: ['alert-rules'],
    queryFn: () => apiClient.getAlertRules(),
    refetchInterval: 30000, // Refetch every 30 seconds
  });
}

export function useCreateAlertRule() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: (request: API.CreateAlertRuleRequest) =>
      apiClient.createAlertRule(request),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['alert-rules'] });
      queryClient.invalidateQueries({ queryKey: ['alert-stats'] });
    },
  });
}

export function useAlertDeliveryLog() {
  return useQuery({
    queryKey: ['alert-delivery-log'],
    queryFn: () => apiClient.getAlertDeliveryLog(),
    refetchInterval: 60000, // Refetch every 60 seconds
  });
}

export function useAlertStats() {
  return useQuery({
    queryKey: ['alert-stats'],
    queryFn: () => apiClient.getAlertStats(),
    refetchInterval: 30000, // Refetch every 30 seconds
  });
}

