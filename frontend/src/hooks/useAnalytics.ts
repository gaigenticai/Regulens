import { useQuery } from '@tanstack/react-query';
import { apiClient } from '@/services/api';

export function useBIDashboards() {
  return useQuery({
    queryKey: ['bi-dashboards'],
    queryFn: () => apiClient.getBIDashboards(),
    refetchInterval: 60000,
  });
}

export function useAnalyticsMetrics() {
  return useQuery({
    queryKey: ['analytics-metrics'],
    queryFn: () => apiClient.getAnalyticsMetrics(),
    refetchInterval: 30000,
  });
}

export function useDataInsights() {
  return useQuery({
    queryKey: ['data-insights'],
    queryFn: () => apiClient.getDataInsights(),
    refetchInterval: 60000,
  });
}

export function useAnalyticsStats() {
  return useQuery({
    queryKey: ['analytics-stats'],
    queryFn: () => apiClient.getAnalyticsStats(),
    refetchInterval: 30000,
  });
}
