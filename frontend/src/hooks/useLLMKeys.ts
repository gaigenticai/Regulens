import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { apiClient } from '@/services/api';
import type * as API from '@/types/api';

export function useLLMApiKeys() {
  return useQuery({
    queryKey: ['llm-api-keys'],
    queryFn: () => apiClient.getLLMApiKeys(),
    refetchInterval: 30000,
  });
}

export function useCreateLLMApiKey() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: (request: API.CreateLLMKeyRequest) => apiClient.createLLMApiKey(request),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['llm-api-keys'] });
    },
  });
}

export function useDeleteLLMApiKey() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: (keyId: string) => apiClient.deleteLLMApiKey(keyId),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['llm-api-keys'] });
    },
  });
}

export function useLLMKeyUsageStats() {
  return useQuery({
    queryKey: ['llm-key-usage-stats'],
    queryFn: () => apiClient.getLLMKeyUsageStats(),
    refetchInterval: 60000,
  });
}

