/**
 * Transaction Hooks
 * Real-time transaction monitoring and fraud analysis
 * NO MOCKS - Real API integration with WebSocket
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { useEffect, useRef } from 'react';
import apiClient from '@/services/api';
import type {
  Transaction,
  TransactionStats,
  FraudAnalysis,
  TransactionPattern,
  AnomalyDetectionResult
} from '@/types/api';

interface UseTransactionsOptions {
  limit?: number;
  status?: 'pending' | 'approved' | 'rejected' | 'flagged' | 'completed';
  riskLevel?: 'low' | 'medium' | 'high' | 'critical';
  startDate?: string;
  endDate?: string;
  minAmount?: number;
  maxAmount?: number;
}

/**
 * Fetch transactions with filtering and real-time updates
 */
export function useTransactions(options: UseTransactionsOptions = {}) {
  const { limit = 100, status, riskLevel, startDate, endDate, minAmount, maxAmount } = options;
  const queryClient = useQueryClient();
  const wsRef = useRef<WebSocket | null>(null);

  const { data: transactions = [], isLoading, isError, error } = useQuery({
    queryKey: ['transactions', limit, status, riskLevel, startDate, endDate, minAmount, maxAmount],
    queryFn: async () => {
      const params = new URLSearchParams();
      if (limit) params.append('limit', limit.toString());
      if (status) params.append('status', status);
      if (riskLevel) params.append('risk_level', riskLevel);
      if (startDate) params.append('start_date', startDate);
      if (endDate) params.append('end_date', endDate);
      if (minAmount !== undefined) params.append('min_amount', minAmount.toString());
      if (maxAmount !== undefined) params.append('max_amount', maxAmount.toString());

      const data = await apiClient.getTransactions(Object.fromEntries(params));
      return data;
    },
    refetchInterval: 15000, // Polling fallback every 15 seconds
  });

  // WebSocket for real-time transaction updates
  useEffect(() => {
    const wsUrl = `${apiClient.wsBaseURL}/transactions`;

    const connectWebSocket = () => {
      try {
        const ws = new WebSocket(wsUrl);
        wsRef.current = ws;

        ws.onopen = () => {
          console.log('[WebSocket] Connected to transaction feed');
        };

        ws.onmessage = (event) => {
          try {
            const message = JSON.parse(event.data);

            if (message.type === 'new_transaction') {
              const newTx = message.data as Transaction;

              // Apply filters
              if (status && newTx.status !== status) return;
              if (riskLevel && newTx.riskLevel !== riskLevel) return;
              if (minAmount !== undefined && newTx.amount < minAmount) return;
              if (maxAmount !== undefined && newTx.amount > maxAmount) return;

              queryClient.setQueryData<Transaction[]>(
                ['transactions', limit, status, riskLevel, startDate, endDate, minAmount, maxAmount],
                (old = []) => {
                  const exists = old.some(t => t.id === newTx.id);
                  if (exists) return old;
                  return [newTx, ...old.slice(0, limit - 1)];
                }
              );

              // Invalidate stats
              queryClient.invalidateQueries({ queryKey: ['transaction-stats'] });
            } else if (message.type === 'update_transaction') {
              const updatedTx = message.data as Transaction;

              queryClient.setQueryData<Transaction[]>(
                ['transactions', limit, status, riskLevel, startDate, endDate, minAmount, maxAmount],
                (old = []) => old.map(t => t.id === updatedTx.id ? updatedTx : t)
              );
            } else if (message.type === 'fraud_alert') {
              // Refresh fraud analysis and stats
              queryClient.invalidateQueries({ queryKey: ['fraud-analysis'] });
              queryClient.invalidateQueries({ queryKey: ['transaction-stats'] });
            }
          } catch (parseError) {
            console.error('[WebSocket] Failed to parse transaction message:', parseError);
          }
        };

        ws.onerror = (error) => {
          console.error('[WebSocket] Transaction connection error:', error);
        };

        ws.onclose = () => {
          console.log('[WebSocket] Transaction connection closed, reconnecting in 5s...');
          setTimeout(connectWebSocket, 5000);
        };
      } catch (error) {
        console.error('[WebSocket] Failed to connect to transaction feed:', error);
        setTimeout(connectWebSocket, 5000);
      }
    };

    connectWebSocket();

    return () => {
      if (wsRef.current) {
        wsRef.current.close();
        wsRef.current = null;
      }
    };
  }, [limit, status, riskLevel, startDate, endDate, minAmount, maxAmount, queryClient]);

  return {
    transactions,
    isLoading,
    isError,
    error,
    isConnected: wsRef.current?.readyState === WebSocket.OPEN,
  };
}

/**
 * Fetch single transaction details
 */
export function useTransaction(id: string | undefined) {
  return useQuery({
    queryKey: ['transaction', id],
    queryFn: async () => {
      if (!id) throw new Error('Transaction ID is required');
      const data = await apiClient.getTransactionById(id);
      return data;
    },
    enabled: !!id,
  });
}

/**
 * Fetch transaction statistics
 */
export function useTransactionStats(timeRange: '1h' | '24h' | '7d' | '30d' = '24h') {
  return useQuery({
    queryKey: ['transaction-stats', timeRange],
    queryFn: async () => {
      const data = await apiClient.getTransactionStats(timeRange);
      return data;
    },
    refetchInterval: 30000, // Refresh every 30 seconds
  });
}

/**
 * Analyze transaction for fraud
 */
export function useAnalyzeTransaction() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (transactionId: string) => {
      const data = await apiClient.analyzeTransaction(transactionId);
      return data;
    },
    onSuccess: (analysis, transactionId) => {
      // Update transaction cache with analysis results
      queryClient.setQueryData(['fraud-analysis', transactionId], analysis);

      // Refresh transaction to show updated risk level
      queryClient.invalidateQueries({ queryKey: ['transaction', transactionId] });
      queryClient.invalidateQueries({ queryKey: ['transactions'] });
    },
  });
}

/**
 * Get fraud analysis for a transaction
 */
export function useFraudAnalysis(transactionId: string | undefined) {
  return useQuery({
    queryKey: ['fraud-analysis', transactionId],
    queryFn: async () => {
      if (!transactionId) throw new Error('Transaction ID is required');
      const data = await apiClient.getFraudAnalysis(transactionId);
      return data;
    },
    enabled: !!transactionId,
  });
}

/**
 * Approve transaction
 */
export function useApproveTransaction() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (transactionId: string) => {
      const data = await apiClient.approveTransaction(transactionId);
      return data;
    },
    onSuccess: (updatedTx, transactionId) => {
      queryClient.setQueryData(['transaction', transactionId], updatedTx);
      queryClient.invalidateQueries({ queryKey: ['transactions'] });
      queryClient.invalidateQueries({ queryKey: ['transaction-stats'] });
    },
  });
}

/**
 * Reject transaction
 */
export function useRejectTransaction() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({ transactionId, reason }: { transactionId: string; reason: string }) => {
      const data = await apiClient.rejectTransaction(transactionId, reason);
      return data;
    },
    onSuccess: (updatedTx, { transactionId }) => {
      queryClient.setQueryData(['transaction', transactionId], updatedTx);
      queryClient.invalidateQueries({ queryKey: ['transactions'] });
      queryClient.invalidateQueries({ queryKey: ['transaction-stats'] });
    },
  });
}

/**
 * Detect transaction patterns
 */
export function useTransactionPatterns(timeRange: '24h' | '7d' | '30d' = '7d') {
  return useQuery({
    queryKey: ['transaction-patterns', timeRange],
    queryFn: async () => {
      const data = await apiClient.getTransactionPatterns();
      return data;
    },
    refetchInterval: 60000, // Refresh every minute
  });
}

/**
 * Run anomaly detection on transactions
 */
export function useAnomalyDetection() {
  return useMutation({
    mutationFn: async (params: { timeRange: string; algorithm?: string }) => {
      const data = await apiClient.detectAnomalies(params);
      return data;
    },
  });
}

/**
 * Get real-time transaction metrics
 */
export function useTransactionMetrics(timeRange: string = '24h') {
  return useQuery({
    queryKey: ['transaction-metrics', timeRange],
    queryFn: async () => {
      const data = await apiClient.getTransactionMetrics(timeRange);
      return data;
    },
    refetchInterval: 5000, // Refresh every 5 seconds for real-time metrics
  });
}
