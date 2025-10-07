/**
 * Dashboard Data Hook
 * Aggregates all dashboard data from real API endpoints
 * NO MOCKS - Fetches live data
 */

import { useQuery } from '@tanstack/react-query';
import { apiClient } from '@/services/api';
import type {
  ActivityStats,
  Decision,
  Agent,
  RegulatoryChange,
  Transaction,
  SystemMetrics,
} from '@/types/api';

export interface DashboardData {
  activityStats: ActivityStats | null;
  recentDecisions: Decision[];
  agents: Agent[];
  recentRegChanges: RegulatoryChange[];
  recentTransactions: Transaction[];
  systemMetrics: SystemMetrics | null;
}

interface UseDashboardDataReturn {
  data: DashboardData;
  isLoading: boolean;
  isError: boolean;
  error: Error | null;
  refetch: () => void;
}

export const useDashboardData = (): UseDashboardDataReturn => {
  const {
    data = {
      activityStats: null,
      recentDecisions: [],
      agents: [],
      recentRegChanges: [],
      recentTransactions: [],
      systemMetrics: null,
    },
    isLoading,
    isError,
    error,
    refetch,
  } = useQuery({
    queryKey: ['dashboard-data'],
    queryFn: async () => {
      // Fetch all dashboard data in parallel
      const [
        activityStats,
        recentDecisions,
        agents,
        recentRegChanges,
        recentTransactions,
        systemMetrics,
      ] = await Promise.allSettled([
        apiClient.getActivityStats(),
        apiClient.getRecentDecisions(5),
        apiClient.getAgents(),
        apiClient.getRegulatoryChanges({ from: new Date(Date.now() - 7 * 24 * 60 * 60 * 1000).toISOString() }),
        apiClient.getTransactions({ limit: 10 }),
        apiClient.getSystemMetrics(),
      ]);

      return {
        activityStats: activityStats.status === 'fulfilled' ? activityStats.value : null,
        recentDecisions: recentDecisions.status === 'fulfilled' ? recentDecisions.value : [],
        agents: agents.status === 'fulfilled' ? agents.value : [],
        recentRegChanges: recentRegChanges.status === 'fulfilled' ? recentRegChanges.value.slice(0, 5) : [],
        recentTransactions: recentTransactions.status === 'fulfilled' ? recentTransactions.value : [],
        systemMetrics: systemMetrics.status === 'fulfilled' ? systemMetrics.value : null,
      };
    },
    refetchInterval: 30000, // Refetch every 30 seconds
    refetchOnWindowFocus: true,
    staleTime: 10000, // 10 seconds
  });

  return {
    data,
    isLoading,
    isError,
    error: error as Error | null,
    refetch,
  };
};

export default useDashboardData;
