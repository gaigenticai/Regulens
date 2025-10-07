/**
 * Real-time Collaboration Hooks
 * Multi-agent collaboration with WebSocket communication
 * NO MOCKS - Real WebSocket integration for live collaboration
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { useState, useEffect, useCallback, useRef } from 'react';
import apiClient from '@/services/api';
import type {
  CollaborationSession,
  CollaborationAgent,
  CollaborationMessage,
  CollaborationTask,
  CollaborationStats,
} from '@/types/api';

/**
 * WebSocket connection for real-time collaboration
 */
export function useCollaborationWebSocket(sessionId: string | undefined) {
  const [socket, setSocket] = useState<WebSocket | null>(null);
  const [isConnected, setIsConnected] = useState(false);
  const [messages, setMessages] = useState<CollaborationMessage[]>([]);
  const [error, setError] = useState<Error | null>(null);
  const reconnectTimeoutRef = useRef<NodeJS.Timeout>();
  const reconnectAttemptsRef = useRef(0);
  const maxReconnectAttempts = 10;
  const queryClient = useQueryClient();

  const connect = useCallback(() => {
    if (!sessionId) return;

    const wsUrl = `${apiClient.wsBaseURL}/collaboration/${sessionId}`;
    const token = apiClient.getToken();
    const ws = new WebSocket(`${wsUrl}?token=${token}`);

    ws.onopen = () => {
      console.log('Collaboration WebSocket connected');
      setIsConnected(true);
      setError(null);
      reconnectAttemptsRef.current = 0;
    };

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);

        switch (data.type) {
          case 'message':
            setMessages((prev) => [...prev, data.message]);
            queryClient.invalidateQueries({ queryKey: ['collaboration-messages', sessionId] });
            break;

          case 'agent_joined':
            queryClient.invalidateQueries({ queryKey: ['collaboration-agents', sessionId] });
            queryClient.invalidateQueries({ queryKey: ['collaboration-session', sessionId] });
            break;

          case 'agent_left':
            queryClient.invalidateQueries({ queryKey: ['collaboration-agents', sessionId] });
            queryClient.invalidateQueries({ queryKey: ['collaboration-session', sessionId] });
            break;

          case 'task_created':
          case 'task_updated':
          case 'task_completed':
            queryClient.invalidateQueries({ queryKey: ['collaboration-tasks', sessionId] });
            break;

          case 'session_updated':
            queryClient.invalidateQueries({ queryKey: ['collaboration-session', sessionId] });
            break;

          case 'typing':
            // Handle typing indicators
            queryClient.setQueryData(['collaboration-typing', sessionId], data.agentId);
            break;

          default:
            console.warn('Unknown collaboration message type:', data.type);
        }
      } catch (error) {
        console.error('Failed to parse WebSocket message:', error);
      }
    };

    ws.onerror = (event) => {
      console.error('Collaboration WebSocket error:', event);
      setError(new Error('WebSocket connection error'));
    };

    ws.onclose = () => {
      console.log('Collaboration WebSocket disconnected');
      setIsConnected(false);
      setSocket(null);

      // Auto-reconnect with exponential backoff
      if (reconnectAttemptsRef.current < maxReconnectAttempts) {
        const delay = Math.min(1000 * Math.pow(2, reconnectAttemptsRef.current), 30000);
        reconnectAttemptsRef.current += 1;

        reconnectTimeoutRef.current = setTimeout(() => {
          console.log(`Reconnecting... (attempt ${reconnectAttemptsRef.current})`);
          connect();
        }, delay);
      }
    };

    setSocket(ws);
  }, [sessionId, queryClient]);

  useEffect(() => {
    connect();

    return () => {
      if (reconnectTimeoutRef.current) {
        clearTimeout(reconnectTimeoutRef.current);
      }
      socket?.close();
    };
  }, [connect, socket]);

  const sendMessage = useCallback((message: Partial<CollaborationMessage>) => {
    if (socket && socket.readyState === WebSocket.OPEN) {
      socket.send(JSON.stringify({
        type: 'message',
        message,
      }));
    } else {
      console.error('WebSocket is not connected');
    }
  }, [socket]);

  const sendTypingIndicator = useCallback((agentId: string, isTyping: boolean) => {
    if (socket && socket.readyState === WebSocket.OPEN) {
      socket.send(JSON.stringify({
        type: 'typing',
        agentId,
        isTyping,
      }));
    }
  }, [socket]);

  const disconnect = useCallback(() => {
    if (reconnectTimeoutRef.current) {
      clearTimeout(reconnectTimeoutRef.current);
    }
    socket?.close();
    setSocket(null);
    setIsConnected(false);
  }, [socket]);

  return {
    isConnected,
    messages,
    error,
    sendMessage,
    sendTypingIndicator,
    disconnect,
  };
}

/**
 * Get all collaboration sessions
 */
export function useCollaborationSessions(params?: {
  limit?: number;
  status?: 'active' | 'completed' | 'archived';
  agentId?: string;
}) {
  const { limit = 50, status, agentId } = params || {};

  return useQuery({
    queryKey: ['collaboration-sessions', limit, status, agentId],
    queryFn: async () => {
      const queryParams: Record<string, string> = {
        limit: limit.toString(),
      };
      if (status) queryParams.status = status;
      if (agentId) queryParams.agent_id = agentId;

      const data = await apiClient.getCollaborationSessions(queryParams);
      return data;
    },
  });
}

/**
 * Get single collaboration session
 */
export function useCollaborationSession(sessionId: string | undefined) {
  return useQuery({
    queryKey: ['collaboration-session', sessionId],
    queryFn: async () => {
      if (!sessionId) throw new Error('Session ID is required');
      const data = await apiClient.getCollaborationSession(sessionId);
      return data;
    },
    enabled: !!sessionId,
  });
}

/**
 * Create new collaboration session
 */
export function useCreateCollaborationSession() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (params: {
      title: string;
      description?: string;
      agentIds: string[];
      objective?: string;
      context?: Record<string, unknown>;
      settings?: {
        maxDuration?: number;
        autoArchive?: boolean;
        allowExternal?: boolean;
      };
    }) => {
      const data = await apiClient.createCollaborationSession(params);
      return data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['collaboration-sessions'] });
    },
  });
}

/**
 * Update collaboration session
 */
export function useUpdateCollaborationSession() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({
      sessionId,
      updates,
    }: {
      sessionId: string;
      updates: Partial<{
        title: string;
        description: string;
        status: 'active' | 'completed' | 'archived';
        objective: string;
        context: Record<string, unknown>;
      }>;
    }) => {
      const data = await apiClient.updateCollaborationSession(sessionId, updates);
      return data;
    },
    onSuccess: (updatedSession) => {
      queryClient.setQueryData(['collaboration-session', updatedSession.id], updatedSession);
      queryClient.invalidateQueries({ queryKey: ['collaboration-sessions'] });
    },
  });
}

/**
 * Join collaboration session
 */
export function useJoinCollaborationSession() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (params: {
      sessionId: string;
      agentId: string;
      role?: 'participant' | 'observer' | 'facilitator';
    }) => {
      const data = await apiClient.joinCollaborationSession(params);
      return data;
    },
    onSuccess: (_, variables) => {
      queryClient.invalidateQueries({ queryKey: ['collaboration-session', variables.sessionId] });
      queryClient.invalidateQueries({ queryKey: ['collaboration-agents', variables.sessionId] });
    },
  });
}

/**
 * Leave collaboration session
 */
export function useLeaveCollaborationSession() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (params: {
      sessionId: string;
      agentId: string;
    }) => {
      const data = await apiClient.leaveCollaborationSession(params);
      return data;
    },
    onSuccess: (_, variables) => {
      queryClient.invalidateQueries({ queryKey: ['collaboration-session', variables.sessionId] });
      queryClient.invalidateQueries({ queryKey: ['collaboration-agents', variables.sessionId] });
    },
  });
}

/**
 * Get agents in session
 */
export function useCollaborationAgents(sessionId: string | undefined) {
  return useQuery({
    queryKey: ['collaboration-agents', sessionId],
    queryFn: async () => {
      if (!sessionId) throw new Error('Session ID is required');
      const data = await apiClient.getCollaborationAgents(sessionId);
      return data;
    },
    enabled: !!sessionId,
    refetchInterval: 10000, // Refresh every 10 seconds
  });
}

/**
 * Get messages in session
 */
export function useCollaborationMessages(sessionId: string | undefined, params?: {
  limit?: number;
  beforeId?: string;
  afterId?: string;
}) {
  const { limit = 100, beforeId, afterId } = params || {};

  return useQuery({
    queryKey: ['collaboration-messages', sessionId, limit, beforeId, afterId],
    queryFn: async () => {
      if (!sessionId) throw new Error('Session ID is required');

      const queryParams: Record<string, string> = {
        limit: limit.toString(),
      };
      if (beforeId) queryParams.before_id = beforeId;
      if (afterId) queryParams.after_id = afterId;

      const data = await apiClient.getCollaborationMessages(sessionId, queryParams);
      return data;
    },
    enabled: !!sessionId,
  });
}

/**
 * Send message to session
 */
export function useSendCollaborationMessage() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (params: {
      sessionId: string;
      agentId: string;
      content: string;
      type?: 'text' | 'code' | 'file' | 'system';
      metadata?: Record<string, unknown>;
      replyToId?: string;
    }) => {
      const data = await apiClient.sendCollaborationMessage(params);
      return data;
    },
    onSuccess: (_, variables) => {
      queryClient.invalidateQueries({ queryKey: ['collaboration-messages', variables.sessionId] });
    },
  });
}

/**
 * Get tasks in session
 */
export function useCollaborationTasks(sessionId: string | undefined, params?: {
  status?: 'pending' | 'in_progress' | 'completed' | 'blocked';
  assignedTo?: string;
}) {
  const { status, assignedTo } = params || {};

  return useQuery({
    queryKey: ['collaboration-tasks', sessionId, status, assignedTo],
    queryFn: async () => {
      if (!sessionId) throw new Error('Session ID is required');

      const queryParams: Record<string, string> = {};
      if (status) queryParams.status = status;
      if (assignedTo) queryParams.assigned_to = assignedTo;

      const data = await apiClient.getCollaborationTasks(sessionId, queryParams);
      return data;
    },
    enabled: !!sessionId,
  });
}

/**
 * Create task in session
 */
export function useCreateCollaborationTask() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (params: {
      sessionId: string;
      title: string;
      description?: string;
      assignedTo?: string;
      priority?: 'low' | 'medium' | 'high' | 'critical';
      dueDate?: string;
      dependencies?: string[];
      metadata?: Record<string, unknown>;
    }) => {
      const data = await apiClient.createCollaborationTask(params);
      return data;
    },
    onSuccess: (_, variables) => {
      queryClient.invalidateQueries({ queryKey: ['collaboration-tasks', variables.sessionId] });
    },
  });
}

/**
 * Update task
 */
export function useUpdateCollaborationTask() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({
      sessionId,
      taskId,
      updates,
    }: {
      sessionId: string;
      taskId: string;
      updates: Partial<{
        title: string;
        description: string;
        status: 'pending' | 'in_progress' | 'completed' | 'blocked';
        assignedTo: string;
        priority: 'low' | 'medium' | 'high' | 'critical';
        dueDate: string;
        progress: number;
      }>;
    }) => {
      const data = await apiClient.updateCollaborationTask(sessionId, taskId, updates);
      return data;
    },
    onSuccess: (_, variables) => {
      queryClient.invalidateQueries({ queryKey: ['collaboration-tasks', variables.sessionId] });
    },
  });
}

/**
 * Get session analytics
 */
export function useCollaborationAnalytics(sessionId: string | undefined) {
  return useQuery({
    queryKey: ['collaboration-analytics', sessionId],
    queryFn: async () => {
      if (!sessionId) throw new Error('Session ID is required');
      const data = await apiClient.getCollaborationAnalytics(sessionId);
      return data;
    },
    enabled: !!sessionId,
    refetchInterval: 60000, // Refresh every minute
  });
}

/**
 * Get collaboration statistics
 */
export function useCollaborationStats(params?: {
  timeRange?: '24h' | '7d' | '30d' | '90d';
  agentId?: string;
}) {
  const { timeRange = '7d', agentId } = params || {};

  return useQuery({
    queryKey: ['collaboration-stats', timeRange, agentId],
    queryFn: async () => {
      const queryParams: Record<string, string> = {
        time_range: timeRange,
      };
      if (agentId) queryParams.agent_id = agentId;

      const data = await apiClient.getCollaborationStats(queryParams);
      return data;
    },
    refetchInterval: 120000, // Refresh every 2 minutes
  });
}

/**
 * Share session resource (file, code, etc.)
 */
export function useShareCollaborationResource() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (params: {
      sessionId: string;
      type: 'file' | 'code' | 'link' | 'data';
      content: string | object;
      title?: string;
      description?: string;
      metadata?: Record<string, unknown>;
    }) => {
      const data = await apiClient.shareCollaborationResource(params);
      return data;
    },
    onSuccess: (_, variables) => {
      queryClient.invalidateQueries({ queryKey: ['collaboration-session', variables.sessionId] });
    },
  });
}

/**
 * Get shared resources in session
 */
export function useCollaborationResources(sessionId: string | undefined, params?: {
  type?: 'file' | 'code' | 'link' | 'data';
  limit?: number;
}) {
  const { type, limit = 50 } = params || {};

  return useQuery({
    queryKey: ['collaboration-resources', sessionId, type, limit],
    queryFn: async () => {
      if (!sessionId) throw new Error('Session ID is required');

      const queryParams: Record<string, string> = {
        limit: limit.toString(),
      };
      if (type) queryParams.type = type;

      const data = await apiClient.getCollaborationResources(sessionId, queryParams);
      return data;
    },
    enabled: !!sessionId,
  });
}

/**
 * Vote on decision/proposal
 */
export function useCollaborationVote() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (params: {
      sessionId: string;
      proposalId: string;
      agentId: string;
      vote: 'approve' | 'reject' | 'abstain';
      comment?: string;
    }) => {
      const data = await apiClient.submitCollaborationVote(params);
      return data;
    },
    onSuccess: (_, variables) => {
      queryClient.invalidateQueries({ queryKey: ['collaboration-session', variables.sessionId] });
    },
  });
}

/**
 * Get session decisions/proposals
 */
export function useCollaborationDecisions(sessionId: string | undefined) {
  return useQuery({
    queryKey: ['collaboration-decisions', sessionId],
    queryFn: async () => {
      if (!sessionId) throw new Error('Session ID is required');
      const data = await apiClient.getCollaborationDecisions(sessionId);
      return data;
    },
    enabled: !!sessionId,
  });
}

/**
 * Export collaboration session report
 */
export function useExportCollaborationReport() {
  return useMutation({
    mutationFn: async (params: {
      sessionId: string;
      format: 'pdf' | 'md' | 'json' | 'html';
      includeMessages?: boolean;
      includeTasks?: boolean;
      includeAnalytics?: boolean;
      includeResources?: boolean;
    }) => {
      const data = await apiClient.exportCollaborationReport(params);
      return data;
    },
  });
}

/**
 * Search across collaboration sessions
 */
export function useSearchCollaboration(query: string, options?: {
  sessionId?: string;
  searchIn?: 'messages' | 'tasks' | 'resources' | 'all';
  limit?: number;
}) {
  const { sessionId, searchIn = 'all', limit = 50 } = options || {};

  return useQuery({
    queryKey: ['collaboration-search', query, sessionId, searchIn, limit],
    queryFn: async () => {
      if (!query || query.length < 3) return [];

      const queryParams: Record<string, string> = {
        q: query,
        search_in: searchIn,
        limit: limit.toString(),
      };
      if (sessionId) queryParams.session_id = sessionId;

      const data = await apiClient.searchCollaboration(queryParams);
      return data;
    },
    enabled: query.length >= 3,
  });
}
