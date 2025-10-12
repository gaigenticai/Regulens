/**
 * Circuit Breaker Hooks
 * Real-time circuit breaker monitoring and management
 * NO MOCKS - Real production circuit breaker system
 */

import { useQuery } from '@tanstack/react-query';
import apiClient from '@/services/api';

export interface CircuitBreaker {
  service_name: string;
  current_state: 'CLOSED' | 'OPEN' | 'HALF_OPEN';
  failure_count: number;
  success_count: number;
  last_failure_time: string | null;
  last_success_time: string | null;
  failure_threshold: number;
  timeout_duration: number;
  half_open_max_calls: number;
}

export interface CircuitBreakerStats {
  totalServices: number;
  openCircuits: number;
}

/**
 * Get all circuit breakers
 */
export function useCircuitBreakers() {
  return useQuery({
    queryKey: ['circuit-breakers'],
    queryFn: async () => {
      const response = await fetch(`${apiClient.baseURL}/api/circuit-breakers`, {
        headers: {
          'Authorization': `Bearer ${apiClient.getToken()}`,
        },
      });
      if (!response.ok) {
        throw new Error('Failed to fetch circuit breakers');
      }
      const data = await response.json();
      return data as CircuitBreaker[];
    },
    refetchInterval: 5000, // Refresh every 5 seconds
  });
}

/**
 * Get circuit breaker statistics
 */
export function useCircuitBreakerStats() {
  return useQuery({
    queryKey: ['circuit-breaker-stats'],
    queryFn: async () => {
      const response = await fetch(`${apiClient.baseURL}/api/circuit-breakers/stats`, {
        headers: {
          'Authorization': `Bearer ${apiClient.getToken()}`,
        },
      });
      if (!response.ok) {
        throw new Error('Failed to fetch circuit breaker stats');
      }
      const data = await response.json();
      return data as CircuitBreakerStats;
    },
    refetchInterval: 5000, // Refresh every 5 seconds
  });
}
