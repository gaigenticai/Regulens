import { useQuery } from '@tanstack/react-query';
import { apiClient } from '@/services/api';

export function useIntegrationConnectors() {
  return useQuery({
    queryKey: ['integration-connectors'],
    queryFn: () => apiClient.getIntegrationConnectors(),
  });
}

export function useIntegrationInstances() {
  return useQuery({
    queryKey: ['integration-instances'],
    queryFn: () => apiClient.getIntegrationInstances(),
    refetchInterval: 30000,
  });
}
