import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { apiClient } from '@/services/api';
import type * as API from '@/types/api';

export function useNLPolicies() {
  return useQuery({
    queryKey: ['nl-policies'],
    queryFn: () => apiClient.getNLPolicies(),
  });
}

export function useCreateNLPolicy() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: (request: API.CreateNLPolicyRequest) => apiClient.createNLPolicy(request),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['nl-policies'] });
    },
  });
}
