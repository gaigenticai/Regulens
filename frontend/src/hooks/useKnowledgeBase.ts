/**
 * Knowledge Base Hooks
 * Real vector search and semantic retrieval
 * NO MOCKS - Real API integration with embeddings
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import apiClient from '@/services/api';

/**
 * Search knowledge base with semantic search
 */
export function useKnowledgeSearch(query: string, options?: {
  limit?: number;
  threshold?: number;
  category?: string;
}) {
  const { limit = 10, threshold = 0.7, category } = options || {};

  return useQuery({
    queryKey: ['knowledge-search', query, limit, threshold, category],
    queryFn: async () => {
      if (!query || query.length < 3) return [];

      const params: Record<string, string> = {
        q: query,
        limit: limit.toString(),
        threshold: threshold.toString(),
      };
      if (category) params.category = category;

      const data = await apiClient.searchKnowledge(params);
      return data;
    },
    enabled: query.length >= 3, // Only search with 3+ characters
  });
}

/**
 * Get all knowledge entries with filtering
 */
export function useKnowledgeEntries(params?: {
  limit?: number;
  category?: string;
  tag?: string;
  sortBy?: 'relevance' | 'date' | 'usage';
}) {
  const { limit = 50, category, tag, sortBy = 'relevance' } = params || {};

  return useQuery({
    queryKey: ['knowledge-entries', limit, category, tag, sortBy],
    queryFn: async () => {
      const queryParams: Record<string, string> = {
        limit: limit.toString(),
        sort_by: sortBy,
      };
      if (category) queryParams.category = category;
      if (tag) queryParams.tag = tag;

      const data = await apiClient.getKnowledgeEntries(queryParams);
      return data;
    },
  });
}

/**
 * Get single knowledge entry
 */
export function useKnowledgeEntry(id: string | undefined) {
  return useQuery({
    queryKey: ['knowledge-entry', id],
    queryFn: async () => {
      if (!id) throw new Error('Entry ID is required');
      const data = await apiClient.getKnowledgeEntry(id);
      return data;
    },
    enabled: !!id,
  });
}

/**
 * Create new knowledge entry
 */
export function useCreateKnowledgeEntry() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (entry: {
      title: string;
      content: string;
      category: string;
      tags: string[];
      metadata?: Record<string, unknown>;
    }) => {
      const data = await apiClient.createKnowledgeEntry(entry);
      return data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['knowledge-entries'] });
    },
  });
}

/**
 * Update knowledge entry
 */
export function useUpdateKnowledgeEntry() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({
      id,
      entry,
    }: {
      id: string;
      entry: Partial<{
        title: string;
        content: string;
        category: string;
        tags: string[];
        metadata: Record<string, unknown>;
      }>;
    }) => {
      const data = await apiClient.updateKnowledgeEntry(id, entry);
      return data;
    },
    onSuccess: (updatedEntry) => {
      queryClient.setQueryData(['knowledge-entry', updatedEntry.id], updatedEntry);
      queryClient.invalidateQueries({ queryKey: ['knowledge-entries'] });
    },
  });
}

/**
 * Delete knowledge entry
 */
export function useDeleteKnowledgeEntry() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (id: string) => {
      await apiClient.deleteKnowledgeEntry(id);
      return id;
    },
    onSuccess: (deletedId) => {
      queryClient.invalidateQueries({ queryKey: ['knowledge-entries'] });
      queryClient.removeQueries({ queryKey: ['knowledge-entry', deletedId] });
    },
  });
}

/**
 * Get similar knowledge entries (using vector similarity)
 */
export function useSimilarEntries(entryId: string | undefined, limit: number = 5) {
  return useQuery({
    queryKey: ['similar-entries', entryId, limit],
    queryFn: async () => {
      if (!entryId) throw new Error('Entry ID is required');
      const data = await apiClient.getSimilarKnowledge(entryId, limit);
      return data;
    },
    enabled: !!entryId,
  });
}

/**
 * Get case examples from knowledge base
 */
export function useCaseExamples(params?: {
  category?: string;
  complexity?: 'low' | 'medium' | 'high';
  limit?: number;
}) {
  const { category, complexity, limit = 20 } = params || {};

  return useQuery({
    queryKey: ['case-examples', category, complexity, limit],
    queryFn: async () => {
      const queryParams: Record<string, string> = {
        limit: limit.toString(),
      };
      if (category) queryParams.category = category;
      if (complexity) queryParams.complexity = complexity;

      const data = await apiClient.getCaseExamples(queryParams);
      return data;
    },
  });
}

/**
 * Get case example by ID
 */
export function useCaseExample(id: string | undefined) {
  return useQuery({
    queryKey: ['case-example', id],
    queryFn: async () => {
      if (!id) throw new Error('Case ID is required');
      const data = await apiClient.getCaseExample(id);
      return data;
    },
    enabled: !!id,
  });
}

/**
 * Ask AI question using knowledge base context (RAG - Retrieval Augmented Generation)
 */
export function useAskKnowledgeBase() {
  return useMutation({
    mutationFn: async (params: {
      question: string;
      context?: string[];
      maxResults?: number;
      temperature?: number;
    }) => {
      const data = await apiClient.askKnowledgeBase(params);
      return data;
    },
  });
}

/**
 * Generate embeddings for text (for semantic search)
 */
export function useGenerateEmbedding() {
  return useMutation({
    mutationFn: async (text: string) => {
      const data = await apiClient.generateEmbedding(text);
      return data;
    },
  });
}

/**
 * Get knowledge base statistics
 */
export function useKnowledgeStats() {
  return useQuery({
    queryKey: ['knowledge-stats'],
    queryFn: async () => {
      const data = await apiClient.getKnowledgeStats();
      return data;
    },
    staleTime: 1000 * 60 * 5, // Cache for 5 minutes
  });
}

/**
 * Reindex knowledge base (rebuild vector index)
 */
export function useReindexKnowledge() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async () => {
      const data = await apiClient.reindexKnowledge();
      return data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['knowledge-entries'] });
      queryClient.invalidateQueries({ queryKey: ['knowledge-stats'] });
    },
  });
}
