import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { apiClient } from '@/services/api';
import type * as API from '@/types/api';

export function useSimulations() {
  return useQuery({
    queryKey: ['simulations'],
    queryFn: () => apiClient.getSimulations(),
    refetchInterval: 30000,
  });
}

export function useCreateSimulation() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: (request: API.CreateSimulationRequest) => apiClient.createSimulation(request),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['simulations'] });
    },
  });
}

export function useSimulationTemplates() {
  return useQuery({
    queryKey: ['simulation-templates'],
    queryFn: () => apiClient.getSimulationTemplates(),
  });
}
