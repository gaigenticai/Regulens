/**
 * LLM Integration Hooks
 * Real AI-powered analysis with streaming support
 * NO MOCKS - Real API integration with GPT-4, Claude, and other LLMs
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { useState, useCallback } from 'react';
import apiClient from '@/services/api';

/**
 * Get available LLM models
 */
export function useLLMModels() {
  return useQuery({
    queryKey: ['llm-models'],
    queryFn: async () => {
      const data = await apiClient.getLLMModels();
      return data;
    },
    staleTime: 1000 * 60 * 10, // Cache for 10 minutes
  });
}

/**
 * Get single LLM model details
 */
export function useLLMModel(modelId: string | undefined) {
  return useQuery({
    queryKey: ['llm-model', modelId],
    queryFn: async () => {
      if (!modelId) throw new Error('Model ID is required');
      const data = await apiClient.getLLMModel(modelId);
      return data;
    },
    enabled: !!modelId,
  });
}

/**
 * Generate LLM completion (non-streaming)
 */
export function useLLMCompletion() {
  return useMutation({
    mutationFn: async (params: {
      modelId: string;
      prompt: string;
      systemPrompt?: string;
      temperature?: number;
      maxTokens?: number;
      topP?: number;
      frequencyPenalty?: number;
      presencePenalty?: number;
      stop?: string[];
    }) => {
      const data = await apiClient.generateLLMCompletion(params);
      return data;
    },
  });
}

/**
 * Streaming LLM completion hook
 * Uses Server-Sent Events (SSE) for real-time token streaming
 */
export function useLLMStream() {
  const [isStreaming, setIsStreaming] = useState(false);
  const [streamedContent, setStreamedContent] = useState('');
  const [error, setError] = useState<Error | null>(null);
  const [tokenCount, setTokenCount] = useState(0);
  const [controller, setController] = useState<AbortController | null>(null);

  const startStream = useCallback(async (params: {
    modelId: string;
    prompt: string;
    systemPrompt?: string;
    temperature?: number;
    maxTokens?: number;
    topP?: number;
    frequencyPenalty?: number;
    presencePenalty?: number;
    stop?: string[];
    onToken?: (token: string) => void;
    onComplete?: (fullContent: string) => void;
    onError?: (error: Error) => void;
  }) => {
    const abortController = new AbortController();
    setController(abortController);
    setIsStreaming(true);
    setStreamedContent('');
    setError(null);
    setTokenCount(0);

    try {
      const response = await fetch(`${apiClient.baseURL}/llm/stream`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${apiClient.getToken()}`,
        },
        body: JSON.stringify({
          model_id: params.modelId,
          prompt: params.prompt,
          system_prompt: params.systemPrompt,
          temperature: params.temperature,
          max_tokens: params.maxTokens,
          top_p: params.topP,
          frequency_penalty: params.frequencyPenalty,
          presence_penalty: params.presencePenalty,
          stop: params.stop,
        }),
        signal: abortController.signal,
      });

      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }

      const reader = response.body?.getReader();
      const decoder = new TextDecoder();
      let fullContent = '';

      if (!reader) {
        throw new Error('Response body is null');
      }

      while (true) {
        const { done, value } = await reader.read();

        if (done) {
          break;
        }

        const chunk = decoder.decode(value, { stream: true });
        const lines = chunk.split('\n');

        for (const line of lines) {
          if (line.startsWith('data: ')) {
            const data = line.slice(6);

            if (data === '[DONE]') {
              continue;
            }

            try {
              const parsed = JSON.parse(data);

              if (parsed.token) {
                fullContent += parsed.token;
                setStreamedContent(fullContent);
                setTokenCount((prev) => prev + 1);
                params.onToken?.(parsed.token);
              }

              if (parsed.error) {
                throw new Error(parsed.error);
              }
            } catch (parseError) {
              // Ignore parsing errors for incomplete chunks
              if (data.trim() !== '') {
                console.warn('Failed to parse SSE data:', data);
              }
            }
          }
        }
      }

      setIsStreaming(false);
      params.onComplete?.(fullContent);
    } catch (err) {
      const error = err instanceof Error ? err : new Error('Unknown error occurred');

      if (error.name === 'AbortError') {
        // Stream was cancelled, not an error
        setIsStreaming(false);
        return;
      }

      setError(error);
      setIsStreaming(false);
      params.onError?.(error);
    }
  }, []);

  const stopStream = useCallback(() => {
    controller?.abort();
    setIsStreaming(false);
  }, [controller]);

  const resetStream = useCallback(() => {
    setStreamedContent('');
    setError(null);
    setTokenCount(0);
    setIsStreaming(false);
  }, []);

  return {
    isStreaming,
    streamedContent,
    error,
    tokenCount,
    startStream,
    stopStream,
    resetStream,
  };
}

/**
 * Analyze text with LLM
 */
export function useLLMAnalyze() {
  return useMutation({
    mutationFn: async (params: {
      modelId: string;
      text: string;
      analysisType: 'sentiment' | 'summary' | 'entities' | 'classification' | 'custom';
      instructions?: string;
      temperature?: number;
    }) => {
      const data = await apiClient.analyzeLLM(params);
      return data;
    },
  });
}

/**
 * Get LLM conversation history
 */
export function useLLMConversations(params?: {
  limit?: number;
  userId?: string;
  modelId?: string;
}) {
  const { limit = 50, userId, modelId } = params || {};

  return useQuery({
    queryKey: ['llm-conversations', limit, userId, modelId],
    queryFn: async () => {
      const queryParams: Record<string, string> = {
        limit: limit.toString(),
      };
      if (userId) queryParams.user_id = userId;
      if (modelId) queryParams.model_id = modelId;

      const data = await apiClient.getLLMConversations(queryParams);
      return data;
    },
  });
}

/**
 * Get single conversation with messages
 */
export function useLLMConversation(conversationId: string | undefined) {
  return useQuery({
    queryKey: ['llm-conversation', conversationId],
    queryFn: async () => {
      if (!conversationId) throw new Error('Conversation ID is required');
      const data = await apiClient.getLLMConversation(conversationId);
      return data;
    },
    enabled: !!conversationId,
  });
}

/**
 * Create new conversation
 */
export function useCreateLLMConversation() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (params: {
      title?: string;
      modelId: string;
      systemPrompt?: string;
      metadata?: Record<string, unknown>;
    }) => {
      const data = await apiClient.createLLMConversation(params);
      return data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['llm-conversations'] });
    },
  });
}

/**
 * Add message to conversation
 */
export function useAddLLMMessage() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (params: {
      conversationId: string;
      role: 'user' | 'assistant' | 'system';
      content: string;
      metadata?: Record<string, unknown>;
    }) => {
      const data = await apiClient.addLLMMessage(params);
      return data;
    },
    onSuccess: (_, variables) => {
      queryClient.invalidateQueries({ queryKey: ['llm-conversation', variables.conversationId] });
      queryClient.invalidateQueries({ queryKey: ['llm-conversations'] });
    },
  });
}

/**
 * Continue conversation with streaming
 */
export function useLLMConversationStream() {
  const [isStreaming, setIsStreaming] = useState(false);
  const [streamedContent, setStreamedContent] = useState('');
  const [error, setError] = useState<Error | null>(null);
  const [controller, setController] = useState<AbortController | null>(null);
  const queryClient = useQueryClient();

  const continueConversation = useCallback(async (params: {
    conversationId: string;
    userMessage: string;
    temperature?: number;
    maxTokens?: number;
    onToken?: (token: string) => void;
    onComplete?: (fullContent: string, messageId: string) => void;
    onError?: (error: Error) => void;
  }) => {
    const abortController = new AbortController();
    setController(abortController);
    setIsStreaming(true);
    setStreamedContent('');
    setError(null);

    try {
      const response = await fetch(`${apiClient.baseURL}/llm/conversations/${params.conversationId}/stream`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${apiClient.getToken()}`,
        },
        body: JSON.stringify({
          user_message: params.userMessage,
          temperature: params.temperature,
          max_tokens: params.maxTokens,
        }),
        signal: abortController.signal,
      });

      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }

      const reader = response.body?.getReader();
      const decoder = new TextDecoder();
      let fullContent = '';
      let messageId = '';

      if (!reader) {
        throw new Error('Response body is null');
      }

      while (true) {
        const { done, value } = await reader.read();

        if (done) {
          break;
        }

        const chunk = decoder.decode(value, { stream: true });
        const lines = chunk.split('\n');

        for (const line of lines) {
          if (line.startsWith('data: ')) {
            const data = line.slice(6);

            if (data === '[DONE]') {
              continue;
            }

            try {
              const parsed = JSON.parse(data);

              if (parsed.token) {
                fullContent += parsed.token;
                setStreamedContent(fullContent);
                params.onToken?.(parsed.token);
              }

              if (parsed.message_id) {
                messageId = parsed.message_id;
              }

              if (parsed.error) {
                throw new Error(parsed.error);
              }
            } catch (parseError) {
              if (data.trim() !== '') {
                console.warn('Failed to parse SSE data:', data);
              }
            }
          }
        }
      }

      setIsStreaming(false);
      queryClient.invalidateQueries({ queryKey: ['llm-conversation', params.conversationId] });
      params.onComplete?.(fullContent, messageId);
    } catch (err) {
      const error = err instanceof Error ? err : new Error('Unknown error occurred');

      if (error.name === 'AbortError') {
        setIsStreaming(false);
        return;
      }

      setError(error);
      setIsStreaming(false);
      params.onError?.(error);
    }
  }, [queryClient]);

  const stopConversation = useCallback(() => {
    controller?.abort();
    setIsStreaming(false);
  }, [controller]);

  const resetConversation = useCallback(() => {
    setStreamedContent('');
    setError(null);
    setIsStreaming(false);
  }, []);

  return {
    isStreaming,
    streamedContent,
    error,
    continueConversation,
    stopConversation,
    resetConversation,
  };
}

/**
 * Delete conversation
 */
export function useDeleteLLMConversation() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (conversationId: string) => {
      await apiClient.deleteLLMConversation(conversationId);
      return conversationId;
    },
    onSuccess: (deletedId) => {
      queryClient.invalidateQueries({ queryKey: ['llm-conversations'] });
      queryClient.removeQueries({ queryKey: ['llm-conversation', deletedId] });
    },
  });
}

/**
 * Get LLM usage statistics
 */
export function useLLMUsageStats(params?: {
  timeRange?: '24h' | '7d' | '30d' | '90d';
  modelId?: string;
  userId?: string;
}) {
  const { timeRange = '7d', modelId, userId } = params || {};

  return useQuery({
    queryKey: ['llm-usage-stats', timeRange, modelId, userId],
    queryFn: async () => {
      const queryParams: Record<string, string> = {
        time_range: timeRange,
      };
      if (modelId) queryParams.model_id = modelId;
      if (userId) queryParams.user_id = userId;

      const data = await apiClient.getLLMUsageStats(queryParams);
      return data;
    },
    refetchInterval: 120000, // Refresh every 2 minutes
  });
}

/**
 * Get token cost estimate
 */
export function useEstimateLLMCost() {
  return useMutation({
    mutationFn: async (params: {
      modelId: string;
      inputTokens: number;
      outputTokens: number;
    }) => {
      const data = await apiClient.estimateLLMCost(params);
      return data;
    },
  });
}

/**
 * Batch process with LLM
 */
export function useLLMBatchProcess() {
  return useMutation({
    mutationFn: async (params: {
      modelId: string;
      items: Array<{
        id: string;
        prompt: string;
      }>;
      systemPrompt?: string;
      temperature?: number;
      maxTokens?: number;
      batchSize?: number;
    }) => {
      const data = await apiClient.batchProcessLLM(params);
      return data;
    },
  });
}

/**
 * Get batch processing job status
 */
export function useLLMBatchJob(jobId: string | undefined) {
  return useQuery({
    queryKey: ['llm-batch-job', jobId],
    queryFn: async () => {
      if (!jobId) throw new Error('Job ID is required');
      const data = await apiClient.getLLMBatchJob(jobId);
      return data;
    },
    enabled: !!jobId,
    refetchInterval: (query) => {
      // Poll faster if job is running
      const job = query.state.data as any;
      if (job?.status === 'running' || job?.status === 'pending') {
        return 3000; // 3 seconds
      }
      return false; // Stop polling if completed
    },
  });
}

/**
 * Fine-tune LLM model
 */
export function useFineTuneLLM() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (params: {
      baseModelId: string;
      trainingDataset: string;
      validationDataset?: string;
      epochs?: number;
      learningRate?: number;
      batchSize?: number;
      name?: string;
      description?: string;
    }) => {
      const data = await apiClient.fineTuneLLM(params);
      return data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['llm-models'] });
    },
  });
}

/**
 * Get fine-tuning job status
 */
export function useFineTuneJob(jobId: string | undefined) {
  return useQuery({
    queryKey: ['fine-tune-job', jobId],
    queryFn: async () => {
      if (!jobId) throw new Error('Job ID is required');
      const data = await apiClient.getFineTuneJob(jobId);
      return data;
    },
    enabled: !!jobId,
    refetchInterval: (query) => {
      const job = query.state.data as any;
      if (job?.status === 'training' || job?.status === 'pending') {
        return 10000; // 10 seconds
      }
      return false;
    },
  });
}

/**
 * Compare multiple LLM models
 */
export function useCompareLLMModels() {
  return useMutation({
    mutationFn: async (params: {
      modelIds: string[];
      prompts: string[];
      systemPrompt?: string;
      temperature?: number;
    }) => {
      const data = await apiClient.compareLLMModels(params);
      return data;
    },
  });
}

/**
 * Get model performance benchmarks
 */
export function useLLMBenchmarks(modelId: string | undefined) {
  return useQuery({
    queryKey: ['llm-benchmarks', modelId],
    queryFn: async () => {
      if (!modelId) throw new Error('Model ID is required');
      const data = await apiClient.getLLMBenchmarks(modelId);
      return data;
    },
    enabled: !!modelId,
    staleTime: 1000 * 60 * 30, // Cache for 30 minutes
  });
}

/**
 * Export LLM conversation/analysis report
 */
export function useExportLLMReport() {
  return useMutation({
    mutationFn: async (params: {
      conversationId?: string;
      analysisId?: string;
      format: 'pdf' | 'json' | 'txt' | 'md';
      includeMetadata?: boolean;
      includeTokenStats?: boolean;
    }) => {
      const data = await apiClient.exportLLMReport(params);
      return data;
    },
  });
}
