/**
 * Audit Trail Hooks
 * Real-time audit log monitoring with filtering and search
 * NO MOCKS - Real API integration with WebSocket
 */

import { useQuery, useQueryClient } from '@tanstack/react-query';
import { useEffect, useRef } from 'react';
import apiClient from '@/services/api';
import type { AuditEntry } from '@/types/api';

interface UseAuditTrailOptions {
  limit?: number;
  userId?: string;
  action?: string;
  entityType?: string;
  startDate?: string;
  endDate?: string;
  severity?: 'info' | 'warning' | 'error' | 'critical';
}

/**
 * Fetch audit trail entries with filtering
 * Uses WebSocket for real-time updates
 */
export function useAuditTrail(options: UseAuditTrailOptions = {}) {
  const { limit = 100, userId, action, entityType, startDate, endDate, severity } = options;
  const queryClient = useQueryClient();
  const wsRef = useRef<WebSocket | null>(null);

  const { data: entries = [], isLoading, isError, error } = useQuery({
    queryKey: ['audit-trail', limit, userId, action, entityType, startDate, endDate, severity],
    queryFn: async () => {
      const params: { from?: string; to?: string; eventType?: string; actor?: string; severity?: string; limit?: number } = {};
      if (limit) params.limit = limit;
      if (userId) params.actor = userId;
      if (action) params.eventType = action;
      if (startDate) params.from = startDate;
      if (endDate) params.to = endDate;
      if (severity) params.severity = severity;

      const data = await apiClient.getAuditTrail(params);
      return data;
    },
    refetchInterval: 30000, // Polling fallback
  });

  // WebSocket for real-time audit events
  useEffect(() => {
    const wsUrl = `${apiClient.wsBaseURL}/audit`;

    const connectWebSocket = () => {
      try {
        const ws = new WebSocket(wsUrl);
        wsRef.current = ws;

        ws.onopen = () => {
          console.log('[WebSocket] Connected to audit trail feed');
        };

        ws.onmessage = (event) => {
          try {
            const message = JSON.parse(event.data);

            if (message.type === 'new_audit_entry') {
              const newEntry = message.data as AuditEntry;

              // Apply filters
              if (userId && newEntry.userId !== userId) return;
              if (action && newEntry.action !== action) return;
              if (entityType && newEntry.entityType !== entityType) return;
              if (severity && newEntry.severity !== severity) return;

              queryClient.setQueryData<AuditEntry[]>(
                ['audit-trail', limit, userId, action, entityType, startDate, endDate, severity],
                (old = []) => {
                  const exists = old.some(e => e.id === newEntry.id);
                  if (exists) return old;
                  return [newEntry, ...old.slice(0, limit - 1)];
                }
              );

              // Invalidate stats
              queryClient.invalidateQueries({ queryKey: ['audit-stats'] });
            }
          } catch (parseError) {
            console.error('[WebSocket] Failed to parse audit message:', parseError);
          }
        };

        ws.onerror = (error) => {
          console.error('[WebSocket] Audit connection error:', error);
        };

        ws.onclose = () => {
          console.log('[WebSocket] Audit connection closed, reconnecting in 5s...');
          setTimeout(connectWebSocket, 5000);
        };
      } catch (error) {
        console.error('[WebSocket] Failed to connect to audit feed:', error);
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
  }, [limit, userId, action, entityType, startDate, endDate, severity, queryClient]);

  return {
    entries,
    isLoading,
    isError,
    error,
    isConnected: wsRef.current?.readyState === WebSocket.OPEN,
  };
}

/**
 * Fetch single audit entry details
 */
export function useAuditEntry(id: string | undefined) {
  return useQuery({
    queryKey: ['audit-entry', id],
    queryFn: async () => {
      if (!id) throw new Error('Entry ID is required');
      const data = await apiClient.getAuditEntry(id);
      return data;
    },
    enabled: !!id,
  });
}

/**
 * Fetch audit statistics
 */
export function useAuditStats(timeRange: '24h' | '7d' | '30d' = '7d') {
  return useQuery({
    queryKey: ['audit-stats', timeRange],
    queryFn: async () => {
      const data = await apiClient.getAuditStats(timeRange);
      return data;
    },
    refetchInterval: 60000, // Refresh every minute
  });
}

/**
 * Search audit trail with advanced filtering
 */
export function useAuditSearch(searchTerm: string, filters: UseAuditTrailOptions = {}) {
  return useQuery({
    queryKey: ['audit-search', searchTerm, filters],
    queryFn: async () => {
      const params: Record<string, string> = {};
      if (filters.limit) params.limit = filters.limit.toString();
      if (filters.userId) params.user_id = filters.userId;
      if (filters.action) params.action = filters.action;
      if (filters.entityType) params.entity_type = filters.entityType;
      if (filters.startDate) params.start_date = filters.startDate;
      if (filters.endDate) params.end_date = filters.endDate;
      if (filters.severity) params.severity = filters.severity;

      const data = await apiClient.searchAuditTrail(searchTerm, params);
      return data;
    },
    enabled: searchTerm.length >= 2, // Only search with 2+ characters
  });
}

/**
 * Get audit trail for specific entity
 */
export function useEntityAuditTrail(entityType: string, entityId: string) {
  return useQuery({
    queryKey: ['entity-audit', entityType, entityId],
    queryFn: async () => {
      const data = await apiClient.getEntityAuditTrail(entityType, entityId);
      return data;
    },
    enabled: !!entityType && !!entityId,
  });
}

/**
 * Get user activity summary
 */
export function useUserActivity(userId: string | undefined, timeRange: '24h' | '7d' | '30d' = '7d') {
  return useQuery({
    queryKey: ['user-activity', userId, timeRange],
    queryFn: async () => {
      if (!userId) throw new Error('User ID is required');
      const data = await apiClient.getUserActivity(userId, timeRange);
      return data;
    },
    enabled: !!userId,
  });
}

/**
 * Get system audit logs
 * Production-grade: Connects to /audit/system-logs endpoint
 */
export function useSystemLogs(options: { limit?: number; severity?: string } = {}) {
  const { limit = 100, severity } = options;
  
  return useQuery({
    queryKey: ['system-logs', limit, severity],
    queryFn: async () => {
      const params: Record<string, string> = {
        limit: limit.toString(),
      };
      if (severity) params.severity = severity;
      
      const data = await apiClient.getSystemLogs(params);
      return data;
    },
    refetchInterval: 30000, // Refresh every 30 seconds
  });
}

/**
 * Get security events
 * Production-grade: Connects to /audit/security-events endpoint
 */
export function useSecurityEvents(options: { limit?: number } = {}) {
  const { limit = 100 } = options;
  
  return useQuery({
    queryKey: ['security-events', limit],
    queryFn: async () => {
      const params: Record<string, string> = {
        limit: limit.toString(),
      };
      
      const data = await apiClient.getSecurityEvents(params);
      return data;
    },
    refetchInterval: 15000, // Refresh every 15 seconds (security is critical)
  });
}

/**
 * Get login history
 * Production-grade: Connects to /audit/login-history endpoint
 */
export function useLoginHistory(options: { limit?: number; username?: string } = {}) {
  const { limit = 100, username } = options;
  
  return useQuery({
    queryKey: ['login-history', limit, username],
    queryFn: async () => {
      const params: Record<string, string> = {
        limit: limit.toString(),
      };
      if (username) params.username = username;
      
      const data = await apiClient.getLoginHistory(params);
      return data;
    },
    refetchInterval: 30000, // Refresh every 30 seconds
  });
}
