/**
 * Activity Statistics Hook
 * Fetches real activity statistics from backend
 * NO MOCKS - Real API integration
 */

import { useQuery } from '@tanstack/react-query';
import { apiClient } from '@/services/api';
import type { ActivityStats } from '@/types/api';

interface UseActivityStatsReturn {
  stats: ActivityStats | null;
  isLoading: boolean;
  isError: boolean;
  error: Error | null;
  refetch: () => void;
}

export const useActivityStats = (): UseActivityStatsReturn => {
  const {
    data: stats = null,
    isLoading,
    isError,
    error,
    refetch,
  } = useQuery({
    queryKey: ['activity-stats'],
    queryFn: async () => {
      const data = await apiClient.getActivityStats();
      console.log('[ActivityStats] Successfully fetched stats:', data);
      return data;
    },
    refetchInterval: 30000, // Refetch every 30 seconds
    refetchOnWindowFocus: true,
    staleTime: 10000, // 10 seconds
    retry: 2, // Retry up to 2 times
    retryDelay: (attemptIndex) => Math.min(1000 * 2 ** attemptIndex, 5000),
  });

  return {
    stats,
    isLoading,
    isError,
    error: error as Error | null,
    refetch,
  };
};

export default useActivityStats;
