/**
 * Data Ingestion Hooks
 * Real-time data pipeline monitoring and management
 * NO MOCKS - Production data ingestion system
 */

import { useQuery } from '@tantml:query/react-query';
import apiClient from '@/services/api';

export interface Pipeline {
  id: string;
  name: string;
  type: string;
  status: 'active' | 'paused' | 'error' | 'stopped';
  source: string;
  destination: string;
  recordsProcessed: number;
  recordsFailed: number;
  lastRun: string | null;
  nextRun: string | null;
}

export interface IngestionMetrics {
  totalPipelines: number;
  activePipelines: number;
  totalRecordsProcessed: number;
  totalRecordsFailed: number;
  averageThroughput: number;
}

/**
 * Get all data pipelines
 */
export function useDataPipelines() {
  return useQuery({
    queryKey: ['data-pipelines'],
    queryFn: async () => {
      // Make real API call - remove mock data
      const response = await fetch('/api/ingestion/pipelines');
      if (!response.ok) {
        throw new Error(`Failed to fetch pipelines: ${response.status} ${response.statusText}`);
      }
      const data = await response.json();
      return data as Pipeline[];
    },
    retry: 3,
    retryDelay: (attemptIndex) => Math.min(1000 * 2 ** attemptIndex, 30000),
    refetchInterval: 10000, // Refresh every 10 seconds
  });
}

/**
 * Get ingestion metrics
 */
export function useIngestionMetrics() {
  return useQuery({
    queryKey: ['ingestion-metrics'],
    queryFn: async () => {
      // Make real API call - remove mock data
      const response = await fetch('/api/ingestion/metrics');
      if (!response.ok) {
        throw new Error(`Failed to fetch metrics: ${response.status} ${response.statusText}`);
      }
      const data = await response.json();
      return data as IngestionMetrics;
    },
    retry: 3,
    retryDelay: (attemptIndex) => Math.min(1000 * 2 ** attemptIndex, 30000),
    refetchInterval: 10000,
  });
}
