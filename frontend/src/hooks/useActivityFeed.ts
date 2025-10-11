/**
 * Activity Feed Hook
 * Real-time activity monitoring with WebSocket and polling
 * NO MOCKS - Fetches real data from backend
 */

import { useQuery, useQueryClient } from '@tanstack/react-query';
import { useEffect } from 'react';
import { apiClient } from '@/services/api';
import type { ActivityItem } from '@/types/api';

interface UseActivityFeedOptions {
  limit?: number;
  enableRealtime?: boolean;
  pollingInterval?: number;
}

interface UseActivityFeedReturn {
  activities: ActivityItem[];
  isLoading: boolean;
  isError: boolean;
  error: Error | null;
  refetch: () => void;
}

export const useActivityFeed = (
  options: UseActivityFeedOptions = {}
): UseActivityFeedReturn => {
  const { limit = 50, enableRealtime = true, pollingInterval = 10000 } = options;
  const queryClient = useQueryClient();

  const {
    data: activities = [],
    isLoading,
    isError,
    error,
    refetch,
  } = useQuery({
    queryKey: ['activity-feed', limit],
    queryFn: async () => {
      try {
        const data = await apiClient.getRecentActivity(limit);
        console.log('[ActivityFeed] Successfully fetched', data.length, 'activities');
        return data;
      } catch (error) {
        console.error('[ActivityFeed] Failed to fetch activities:', error);
        throw error;
      }
    },
    refetchInterval: enableRealtime ? pollingInterval : false,
    refetchOnWindowFocus: true,
    staleTime: 5000, // 5 seconds
    retry: (failureCount, error: any) => {
      // Don't retry indefinitely for network errors
      if (failureCount >= 3) return false;
      
      // Don't retry for 4xx errors (client errors)
      if (error?.response?.status >= 400 && error?.response?.status < 500) {
        return false;
      }
      
      return true;
    },
    retryDelay: (attemptIndex) => Math.min(1000 * 2 ** attemptIndex, 10000),
  });

  // WebSocket connection for real-time updates
  useEffect(() => {
    if (!enableRealtime) return;

    // Connect to WebSocket for real-time activity updates
    const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsHost = window.location.hostname;
    const wsPort = import.meta.env.VITE_WS_PORT || '8080';
    const wsUrl = `${wsProtocol}//${wsHost}:${wsPort}/ws/activity`;

    let ws: WebSocket | null = null;
    let reconnectTimeout: NodeJS.Timeout;
    let isIntentionallyClosed = false;

    const connect = () => {
      try {
        ws = new WebSocket(wsUrl);

        ws.onopen = () => {
          console.log('[ActivityFeed] WebSocket connected');
        };

        ws.onmessage = (event) => {
          try {
            const message = JSON.parse(event.data);

            // Handle different message types
            if (message.type === 'activity_update') {
              // Invalidate and refetch the activity feed
              queryClient.invalidateQueries({ queryKey: ['activity-feed'] });
            } else if (message.type === 'new_activity') {
              // Optimistically add new activity to the feed
              const newActivity: ActivityItem = message.data;
              queryClient.setQueryData<ActivityItem[]>(
                ['activity-feed', limit],
                (old = []) => [newActivity, ...old.slice(0, limit - 1)]
              );
            }
          } catch (err) {
            console.error('[ActivityFeed] Error parsing WebSocket message:', err);
          }
        };

        ws.onerror = (error) => {
          console.error('[ActivityFeed] WebSocket error:', error);
        };

        ws.onclose = () => {
          console.log('[ActivityFeed] WebSocket disconnected');

          // Attempt to reconnect after 5 seconds if not intentionally closed
          if (!isIntentionallyClosed) {
            reconnectTimeout = setTimeout(() => {
              console.log('[ActivityFeed] Attempting to reconnect...');
              connect();
            }, 5000);
          }
        };
      } catch (err) {
        console.error('[ActivityFeed] WebSocket connection error:', err);
        // Fall back to polling if WebSocket fails
      }
    };

    // Initial connection
    connect();

    // Cleanup
    return () => {
      isIntentionallyClosed = true;
      if (reconnectTimeout) {
        clearTimeout(reconnectTimeout);
      }
      if (ws) {
        ws.close();
      }
    };
  }, [enableRealtime, limit, queryClient]);

  return {
    activities,
    isLoading,
    isError,
    error: error as Error | null,
    refetch,
  };
};

export default useActivityFeed;
