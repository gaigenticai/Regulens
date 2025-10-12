import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { apiClient } from '@/services/api';
import type * as API from '@/types/api';

export function useExportRequests() {
  return useQuery({
    queryKey: ['export-requests'],
    queryFn: () => apiClient.getExportRequests(),
    refetchInterval: 10000,
  });
}

export function useCreateExportRequest() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: (request: API.CreateExportRequest) => apiClient.createExportRequest(request),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['export-requests'] });
    },
  });
}

export function useExportTemplates() {
  return useQuery({
    queryKey: ['export-templates'],
    queryFn: () => apiClient.getExportTemplates(),
  });
}

