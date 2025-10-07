/**
 * Decisions Hook
 * Fetches and manages MCDA decisions from backend
 * NO MOCKS - Real API integration
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { apiClient } from '@/services/api';
import type { Decision, MCDAAlgorithm, Criterion, Alternative } from '@/types/api';

interface UseDecisionsOptions {
  limit?: number;
}

interface UseDecisionsReturn {
  decisions: Decision[];
  isLoading: boolean;
  isError: boolean;
  error: Error | null;
  refetch: () => void;
}

export const useDecisions = (options: UseDecisionsOptions = {}): UseDecisionsReturn => {
  const { limit = 20 } = options;

  const {
    data: decisions = [],
    isLoading,
    isError,
    error,
    refetch,
  } = useQuery({
    queryKey: ['decisions', limit],
    queryFn: async () => {
      const data = await apiClient.getRecentDecisions(limit);
      return data;
    },
    refetchOnWindowFocus: true,
    staleTime: 30000, // 30 seconds
  });

  return {
    decisions,
    isLoading,
    isError,
    error: error as Error | null,
    refetch,
  };
};

interface UseDecisionDetailReturn {
  decision: Decision | null;
  isLoading: boolean;
  isError: boolean;
  error: Error | null;
  refetch: () => void;
}

export const useDecisionDetail = (id: string): UseDecisionDetailReturn => {
  const {
    data: decision = null,
    isLoading,
    isError,
    error,
    refetch,
  } = useQuery({
    queryKey: ['decision', id],
    queryFn: async () => {
      const data = await apiClient.getDecisionById(id);
      return data;
    },
    enabled: !!id,
    refetchOnWindowFocus: true,
    staleTime: 60000, // 1 minute
  });

  return {
    decision,
    isLoading,
    isError,
    error: error as Error | null,
    refetch,
  };
};

interface CreateDecisionData {
  title: string;
  description: string;
  algorithm: MCDAAlgorithm;
  criteria: Criterion[];
  alternatives: Alternative[];
}

interface UseCreateDecisionReturn {
  createDecision: (data: CreateDecisionData) => Promise<Decision>;
  isLoading: boolean;
  isError: boolean;
  error: Error | null;
}

export const useCreateDecision = (): UseCreateDecisionReturn => {
  const queryClient = useQueryClient();

  const mutation = useMutation({
    mutationFn: async (data: CreateDecisionData) => {
      const decision = await apiClient.createDecision(data);
      return decision;
    },
    onSuccess: () => {
      // Invalidate decisions list to refetch
      queryClient.invalidateQueries({ queryKey: ['decisions'] });
    },
  });

  return {
    createDecision: mutation.mutateAsync,
    isLoading: mutation.isPending,
    isError: mutation.isError,
    error: mutation.error as Error | null,
  };
};

export default useDecisions;
