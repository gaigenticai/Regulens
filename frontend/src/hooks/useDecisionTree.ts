/**
 * Decision Tree Hook
 * Fetches decision tree visualization data from backend
 * NO MOCKS - Real D3 data from API
 */

import { useQuery } from '@tanstack/react-query';
import { apiClient } from '@/services/api';
import type { DecisionTree, MCDAAlgorithm, Criterion, Alternative } from '@/types/api';

interface UseDecisionTreeOptions {
  decisionId?: string;
  visualizeData?: {
    algorithm: MCDAAlgorithm;
    criteria: Criterion[];
    alternatives: Alternative[];
  };
}

interface UseDecisionTreeReturn {
  tree: DecisionTree | null;
  isLoading: boolean;
  isError: boolean;
  error: Error | null;
  refetch: () => void;
}

export const useDecisionTree = (options: UseDecisionTreeOptions): UseDecisionTreeReturn => {
  const { decisionId, visualizeData } = options;

  const {
    data: tree = null,
    isLoading,
    isError,
    error,
    refetch,
  } = useQuery({
    queryKey: decisionId
      ? ['decision-tree', decisionId]
      : ['decision-tree-preview', visualizeData],
    queryFn: async () => {
      if (decisionId) {
        // Fetch existing decision tree
        const data = await apiClient.getDecisionTree(decisionId);
        return data;
      } else if (visualizeData) {
        // Visualize new decision without saving
        const data = await apiClient.visualizeDecision(visualizeData);
        return data;
      }
      return null;
    },
    enabled: !!(decisionId || visualizeData),
    refetchOnWindowFocus: false,
    staleTime: 300000, // 5 minutes - decision trees don't change often
  });

  return {
    tree,
    isLoading,
    isError,
    error: error as Error | null,
    refetch,
  };
};

export default useDecisionTree;
