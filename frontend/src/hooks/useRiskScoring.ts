import { useQuery } from '@tanstack/react-query';
import { apiClient } from '@/services/api';

export function useRiskPredictions() {
  return useQuery({
    queryKey: ['risk-predictions'],
    queryFn: () => apiClient.getRiskPredictions(),
    refetchInterval: 30000,
  });
}

export function useMLModels() {
  return useQuery({
    queryKey: ['ml-models'],
    queryFn: () => apiClient.getMLModels(),
  });
}

export function useRiskDashboardStats() {
  return useQuery({
    queryKey: ['risk-dashboard-stats'],
    queryFn: () => apiClient.getRiskDashboardStats(),
    refetchInterval: 60000,
  });
}

