/**
 * Pattern Analysis Hooks
 * Real ML-based pattern detection and analysis
 * NO MOCKS - Real API integration with pattern recognition
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import apiClient from '@/services/api';

/**
 * Fetch all detected patterns
 */
export function usePatterns(params?: {
  limit?: number;
  type?: string;
  minConfidence?: number;
  startDate?: string;
  endDate?: string;
}) {
  const { limit = 50, type, minConfidence, startDate, endDate } = params || {};

  return useQuery({
    queryKey: ['patterns', limit, type, minConfidence, startDate, endDate],
    queryFn: async () => {
      const queryParams: Record<string, string> = {};
      if (limit) queryParams.limit = limit.toString();
      if (type) queryParams.type = type;
      if (minConfidence !== undefined) queryParams.min_confidence = minConfidence.toString();
      if (startDate) queryParams.start_date = startDate;
      if (endDate) queryParams.end_date = endDate;

      const data = await apiClient.getPatterns(queryParams);
      return data;
    },
    refetchInterval: 60000, // Refresh every minute
  });
}

/**
 * Fetch single pattern details
 */
export function usePattern(id: string | undefined) {
  return useQuery({
    queryKey: ['pattern', id],
    queryFn: async () => {
      if (!id) throw new Error('Pattern ID is required');
      const data = await apiClient.getPatternById(id);
      return data;
    },
    enabled: !!id,
  });
}

/**
 * Fetch pattern statistics
 */
export function usePatternStats() {
  return useQuery({
    queryKey: ['pattern-stats'],
    queryFn: async () => {
      const data = await apiClient.getPatternStats();
      return data;
    },
    refetchInterval: 120000, // Refresh every 2 minutes
  });
}

/**
 * Run pattern detection analysis
 */
export function useDetectPatterns() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (params: {
      dataSource: string;
      algorithm?: 'clustering' | 'sequential' | 'association' | 'neural';
      timeRange?: string;
      minSupport?: number;
      minConfidence?: number;
    }) => {
      const data = await apiClient.detectPatterns(params);
      return data;
    },
    onSuccess: () => {
      // Invalidate patterns to refresh with new detections
      queryClient.invalidateQueries({ queryKey: ['patterns'] });
      queryClient.invalidateQueries({ queryKey: ['pattern-stats'] });
    },
  });
}

/**
 * Get pattern predictions
 */
export function usePatternPredictions(patternId: string | undefined) {
  return useQuery({
    queryKey: ['pattern-predictions', patternId],
    queryFn: async () => {
      if (!patternId) throw new Error('Pattern ID is required');
      const data = await apiClient.getPatternPredictions(patternId);
      return data;
    },
    enabled: !!patternId,
  });
}

/**
 * Validate pattern against new data
 */
export function useValidatePattern() {
  return useMutation({
    mutationFn: async (params: {
      patternId: string;
      validationData: Record<string, unknown>;
    }) => {
      const data = await apiClient.validatePattern(params.patternId, params.validationData);
      return data;
    },
  });
}

/**
 * Get pattern correlations
 */
export function usePatternCorrelations(patternId: string | undefined) {
  return useQuery({
    queryKey: ['pattern-correlations', patternId],
    queryFn: async () => {
      if (!patternId) throw new Error('Pattern ID is required');
      const data = await apiClient.getPatternCorrelations(patternId);
      return data;
    },
    enabled: !!patternId,
  });
}

/**
 * Get pattern timeline (historical occurrences)
 */
export function usePatternTimeline(patternId: string | undefined, timeRange: '24h' | '7d' | '30d' | '90d' = '30d') {
  return useQuery({
    queryKey: ['pattern-timeline', patternId, timeRange],
    queryFn: async () => {
      if (!patternId) throw new Error('Pattern ID is required');
      const data = await apiClient.getPatternTimeline(patternId, timeRange);
      return data;
    },
    enabled: !!patternId,
  });
}

/**
 * Export pattern analysis report
 */
export function useExportPatternReport() {
  return useMutation({
    mutationFn: async (params: {
      patternIds?: string[];
      format: 'pdf' | 'csv' | 'json';
      includeVisualization?: boolean;
      includeStats?: boolean;
    }) => {
      const data = await apiClient.exportPatternReport(params);
      return data;
    },
  });
}

/**
 * Get pattern anomalies (patterns that deviate from expected behavior)
 */
export function usePatternAnomalies(params?: {
  limit?: number;
  severity?: 'low' | 'medium' | 'high';
}) {
  const { limit = 20, severity } = params || {};

  return useQuery({
    queryKey: ['pattern-anomalies', limit, severity],
    queryFn: async () => {
      const queryParams: Record<string, string> = {};
      if (limit) queryParams.limit = limit.toString();
      if (severity) queryParams.severity = severity;

      const data = await apiClient.getPatternAnomalies(queryParams);
      return data;
    },
    refetchInterval: 90000, // Refresh every 90 seconds
  });
}
