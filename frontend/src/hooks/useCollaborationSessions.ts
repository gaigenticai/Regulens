/**
 * Collaboration Sessions Hook
 * Production-grade React Query hook for collaboration session management
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { apiClient } from '@/services/api';
import type * as API from '@/types/api';

export function useCollaborationSessions(status?: string, limit?: number) {
  return useQuery({
    queryKey: ['collaboration-sessions', status, limit],
    queryFn: () => apiClient.getCollaborationSessions(status, limit),
    refetchInterval: 30000, // Refetch every 30 seconds
  });
}

export function useCollaborationSession(sessionId: string) {
  return useQuery({
    queryKey: ['collaboration-session', sessionId],
    queryFn: () => apiClient.getCollaborationSession(sessionId),
    enabled: !!sessionId,
    refetchInterval: 10000, // Refetch every 10 seconds for active session
  });
}

export function useCreateCollaborationSession() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: (request: API.CreateSessionRequest) =>
      apiClient.createCollaborationSession(request),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['collaboration-sessions'] });
    },
  });
}

export function useSessionReasoningSteps(sessionId: string) {
  return useQuery({
    queryKey: ['reasoning-steps', sessionId],
    queryFn: () => apiClient.getSessionReasoningSteps(sessionId),
    enabled: !!sessionId,
    refetchInterval: 5000, // Refetch every 5 seconds for real-time updates
  });
}

export function useRecordHumanOverride() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: (request: API.RecordOverrideRequest) =>
      apiClient.recordHumanOverride(request),
    onSuccess: (_, variables) => {
      if (variables.session_id) {
        queryClient.invalidateQueries({ queryKey: ['collaboration-session', variables.session_id] });
        queryClient.invalidateQueries({ queryKey: ['reasoning-steps', variables.session_id] });
      }
    },
  });
}

export function useCollaborationDashboardStats() {
  return useQuery({
    queryKey: ['collaboration-dashboard-stats'],
    queryFn: () => apiClient.getCollaborationDashboardStats(),
    refetchInterval: 15000, // Refetch every 15 seconds
  });
}

