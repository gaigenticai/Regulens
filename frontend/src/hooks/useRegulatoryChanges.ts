/**
 * Regulatory Changes Hooks
 * Real-time monitoring of regulatory changes with WebSocket support
 * NO MOCKS - Real API integration
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { useEffect, useRef } from 'react';
import apiClient from '@/services/api';
import type { RegulatoryChange } from '@/types/api';

interface UseRegulatoryChangesOptions {
  limit?: number;
  sourceId?: string;
  severity?: 'low' | 'medium' | 'high' | 'critical';
  status?: 'pending' | 'analyzed' | 'implemented' | 'dismissed';
  startDate?: string;
  endDate?: string;
}

/**
 * Fetch regulatory changes with filtering
 * Uses WebSocket for real-time updates with polling fallback
 */
export function useRegulatoryChanges(options: UseRegulatoryChangesOptions = {}) {
  const { limit = 50, sourceId, severity, status, startDate, endDate } = options;
  const queryClient = useQueryClient();
  const wsRef = useRef<WebSocket | null>(null);

  const { data: changes = [], isLoading, isError, error } = useQuery({
    queryKey: ['regulatory-changes', limit, sourceId, severity, status, startDate, endDate],
    queryFn: async () => {
      const params: { from?: string; to?: string; source?: string; severity?: string; limit?: number } = {};
      if (limit) params.limit = limit;
      if (sourceId) params.source = sourceId;
      if (severity) params.severity = severity;
      if (startDate) params.from = startDate;
      if (endDate) params.to = endDate;

      const data = await apiClient.getRegulatoryChanges(params);
      return data;
    },
    refetchInterval: 30000, // Polling fallback every 30 seconds
  });

  // WebSocket connection for real-time updates
  useEffect(() => {
    const wsUrl = `${apiClient.wsBaseURL}/regulatory`;

    const connectWebSocket = () => {
      try {
        const ws = new WebSocket(wsUrl);
        wsRef.current = ws;

        ws.onopen = () => {
          console.log('[WebSocket] Connected to regulatory changes feed');
        };

        ws.onmessage = (event) => {
          try {
            const message = JSON.parse(event.data);

            if (message.type === 'new_change') {
              const newChange = message.data as RegulatoryChange;

              // Apply filters before adding to cache
              if (sourceId && newChange.sourceId !== sourceId) return;
              if (severity && newChange.severity !== severity) return;
              if (status && newChange.status !== status) return;

              queryClient.setQueryData<RegulatoryChange[]>(
                ['regulatory-changes', limit, sourceId, severity, status, startDate, endDate],
                (old = []) => {
                  const exists = old.some(c => c.id === newChange.id);
                  if (exists) return old;
                  return [newChange, ...old.slice(0, limit - 1)];
                }
              );
            } else if (message.type === 'update_change') {
              const updatedChange = message.data as RegulatoryChange;

              queryClient.setQueryData<RegulatoryChange[]>(
                ['regulatory-changes', limit, sourceId, severity, status, startDate, endDate],
                (old = []) => old.map(c => c.id === updatedChange.id ? updatedChange : c)
              );
            }
          } catch (parseError) {
            console.error('[WebSocket] Failed to parse message:', parseError);
          }
        };

        ws.onerror = (error) => {
          console.error('[WebSocket] Connection error:', error);
        };

        ws.onclose = () => {
          console.log('[WebSocket] Connection closed, reconnecting in 5s...');
          setTimeout(connectWebSocket, 5000);
        };
      } catch (error) {
        console.error('[WebSocket] Failed to connect:', error);
        setTimeout(connectWebSocket, 5000);
      }
    };

    connectWebSocket();

    return () => {
      if (wsRef.current) {
        wsRef.current.close();
        wsRef.current = null;
      }
    };
  }, [limit, sourceId, severity, status, startDate, endDate, queryClient]);

  return {
    changes,
    isLoading,
    isError,
    error,
    isConnected: wsRef.current?.readyState === WebSocket.OPEN,
  };
}

/**
 * Fetch single regulatory change details
 */
export function useRegulatoryChange(id: string | undefined) {
  return useQuery({
    queryKey: ['regulatory-change', id],
    queryFn: async () => {
      if (!id) throw new Error('Change ID is required');
      const data = await apiClient.getRegulatoryChange(id);
      return data;
    },
    enabled: !!id,
  });
}

/**
 * Fetch all regulatory sources
 */
export function useRegulatorySources() {
  return useQuery({
    queryKey: ['regulatory-sources'],
    queryFn: async () => {
      const data = await apiClient.getRegulatorySources();
      return data;
    },
    staleTime: 1000 * 60 * 15, // Sources don't change frequently
  });
}

/**
 * Update regulatory change status
 */
export function useUpdateRegulatoryStatus() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({ id, status }: { id: string; status: NonNullable<RegulatoryChange['status']> }) => {
      const data = await apiClient.updateRegulatoryStatus(id, status);
      return data;
    },
    onSuccess: (updatedChange) => {
      // Invalidate all regulatory change queries
      queryClient.invalidateQueries({ queryKey: ['regulatory-changes'] });

      // Update specific change in cache
      queryClient.setQueryData(['regulatory-change', updatedChange.id], updatedChange);
    },
  });
}

/**
 * Get impact assessment for a regulatory change
 */
export function useImpactAssessment(changeId: string | undefined) {
  return useQuery({
    queryKey: ['impact-assessment', changeId],
    queryFn: async () => {
      if (!changeId) throw new Error('Change ID is required');
      const data = await apiClient.getImpactAssessment(changeId);
      return data;
    },
    enabled: !!changeId,
  });
}

/**
 * Generate impact assessment
 */
export function useGenerateImpactAssessment() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (changeId: string) => {
      const data = await apiClient.generateImpactAssessment(changeId);
      return data;
    },
    onSuccess: (assessment, changeId) => {
      // Update assessment cache
      queryClient.setQueryData(['impact-assessment', changeId], assessment);

      // Invalidate change to refresh impact status
      queryClient.invalidateQueries({ queryKey: ['regulatory-change', changeId] });
      queryClient.invalidateQueries({ queryKey: ['regulatory-changes'] });
    },
  });
}

/**
 * Fetch regulatory statistics
 */
export function useRegulatoryStats() {
  return useQuery({
    queryKey: ['regulatory-stats'],
    queryFn: async () => {
      const data = await apiClient.getRegulatoryStats();
      return data;
    },
    refetchInterval: 60000, // Refresh every minute
  });
}
