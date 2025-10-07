/**
 * Transaction Guardian Page
 * Real-time transaction monitoring with fraud analysis
 * NO MOCKS - Real WebSocket updates and fraud detection
 */

import React, { useState, useMemo } from 'react';
import { Link } from 'react-router-dom';
import { Shield, Search, DollarSign, AlertTriangle, CheckCircle, XCircle, Clock, Wifi, WifiOff } from 'lucide-react';
import { formatDistanceToNow } from 'date-fns';
import { useTransactions, useTransactionStats, useAnalyzeTransaction } from '@/hooks/useTransactions';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { clsx } from 'clsx';
import type { Transaction } from '@/types/api';

const Transactions: React.FC = () => {
  const [searchQuery, setSearchQuery] = useState('');
  const [filterStatus, setFilterStatus] = useState<Transaction['status'] | 'all'>('all');
  const [filterRiskLevel, setFilterRiskLevel] = useState<Transaction['riskLevel'] | 'all'>('all');
  const [timeRange, setTimeRange] = useState<'1h' | '24h' | '7d' | '30d'>('24h');

  const { transactions, isLoading, isConnected } = useTransactions({
    limit: 100,
    status: filterStatus === 'all' ? undefined : filterStatus,
    riskLevel: filterRiskLevel === 'all' ? undefined : filterRiskLevel,
  });

  const { data: stats } = useTransactionStats(timeRange);
  const { mutate: analyzeTransaction, isPending: isAnalyzing } = useAnalyzeTransaction();

  const filteredTransactions = useMemo(() => {
    return transactions.filter((tx) => {
      const matchesSearch =
        searchQuery === '' ||
        tx.id.toLowerCase().includes(searchQuery.toLowerCase()) ||
        tx.fromAccount.toLowerCase().includes(searchQuery.toLowerCase()) ||
        tx.toAccount.toLowerCase().includes(searchQuery.toLowerCase()) ||
        tx.description?.toLowerCase().includes(searchQuery.toLowerCase());

      return matchesSearch;
    });
  }, [transactions, searchQuery]);

  const riskColors = {
    low: 'bg-green-100 text-green-700 border-green-300',
    medium: 'bg-yellow-100 text-yellow-700 border-yellow-300',
    high: 'bg-orange-100 text-orange-700 border-orange-300',
    critical: 'bg-red-100 text-red-700 border-red-300',
  };

  const statusIcons = {
    pending: Clock,
    approved: CheckCircle,
    rejected: XCircle,
    flagged: AlertTriangle,
    completed: CheckCircle,
  };

  const statusColors = {
    pending: 'text-yellow-600',
    approved: 'text-green-600',
    rejected: 'text-red-600',
    flagged: 'text-orange-600',
    completed: 'text-blue-600',
  };

  if (isLoading) {
    return <LoadingSpinner fullScreen message="Loading transactions..." />;
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-gray-900 flex items-center gap-2">
            <Shield className="w-7 h-7 text-blue-600" />
            Transaction Guardian
          </h1>
          <p className="text-gray-600 mt-1">Real-time transaction monitoring and fraud detection</p>
        </div>

        <div className="flex items-center gap-2">
          {isConnected ? (
            <div className="flex items-center gap-2 px-3 py-1 bg-green-50 text-green-700 rounded-lg border border-green-200">
              <Wifi className="w-4 h-4" />
              <span className="text-sm font-medium">Live</span>
            </div>
          ) : (
            <div className="flex items-center gap-2 px-3 py-1 bg-gray-50 text-gray-700 rounded-lg border border-gray-200">
              <WifiOff className="w-4 h-4" />
              <span className="text-sm font-medium">Polling</span>
            </div>
          )}
        </div>
      </div>

      {/* Statistics Cards */}
      {stats && (
        <div className="grid grid-cols-1 md:grid-cols-5 gap-4">
          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Total Volume</p>
            <p className="text-2xl font-bold text-gray-900 mt-1">
              ${stats.totalVolume.toLocaleString()}
            </p>
            <p className="text-xs text-gray-500 mt-1">Last {timeRange === '1h' ? 'hour' : timeRange === '24h' ? '24 hours' : timeRange === '7d' ? '7 days' : '30 days'}</p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Transactions</p>
            <p className="text-2xl font-bold text-blue-600 mt-1">{stats.totalTransactions}</p>
            <p className="text-xs text-gray-500 mt-1">Processed</p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Flagged</p>
            <p className="text-2xl font-bold text-orange-600 mt-1">{stats.flaggedTransactions}</p>
            <p className="text-xs text-gray-500 mt-1">For review</p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Fraud Rate</p>
            <p className="text-2xl font-bold text-red-600 mt-1">{(stats.fraudRate * 100).toFixed(2)}%</p>
            <p className="text-xs text-gray-500 mt-1">Detection rate</p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Avg. Risk Score</p>
            <p className="text-2xl font-bold text-purple-600 mt-1">{stats.averageRiskScore.toFixed(2)}</p>
            <p className="text-xs text-gray-500 mt-1">Out of 100</p>
          </div>
        </div>
      )}

      {/* Filters */}
      <div className="bg-white rounded-lg border border-gray-200 p-4">
        <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
          {/* Search */}
          <div className="relative">
            <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-5 h-5 text-gray-400" />
            <input
              type="text"
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search by ID, account, or description..."
              className="w-full pl-10 pr-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
            />
          </div>

          {/* Status Filter */}
          <select
            value={filterStatus}
            onChange={(e) => setFilterStatus(e.target.value as any)}
            className="px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
          >
            <option value="all">All Statuses</option>
            <option value="pending">Pending</option>
            <option value="approved">Approved</option>
            <option value="rejected">Rejected</option>
            <option value="flagged">Flagged</option>
            <option value="completed">Completed</option>
          </select>

          {/* Risk Level Filter */}
          <select
            value={filterRiskLevel}
            onChange={(e) => setFilterRiskLevel(e.target.value as any)}
            className="px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
          >
            <option value="all">All Risk Levels</option>
            <option value="low">Low</option>
            <option value="medium">Medium</option>
            <option value="high">High</option>
            <option value="critical">Critical</option>
          </select>
        </div>

        {/* Time Range Selector */}
        <div className="mt-4 flex items-center gap-2">
          <span className="text-sm text-gray-600">Time Range:</span>
          <div className="flex gap-2">
            {(['1h', '24h', '7d', '30d'] as const).map((range) => (
              <button
                key={range}
                onClick={() => setTimeRange(range)}
                className={clsx(
                  'px-3 py-1 rounded-lg text-sm font-medium transition-colors',
                  timeRange === range
                    ? 'bg-blue-600 text-white'
                    : 'bg-gray-100 text-gray-700 hover:bg-gray-200'
                )}
              >
                {range === '1h' ? '1 Hour' : range === '24h' ? '24 Hours' : range === '7d' ? '7 Days' : '30 Days'}
              </button>
            ))}
          </div>
        </div>

        <div className="mt-3 text-sm text-gray-600">
          Showing {filteredTransactions.length} of {transactions.length} transactions
        </div>
      </div>

      {/* Transactions List */}
      {filteredTransactions.length === 0 ? (
        <div className="text-center py-12 bg-white rounded-lg border border-gray-200">
          <Shield className="w-12 h-12 text-gray-400 mx-auto mb-3" />
          <p className="text-gray-600">
            {transactions.length === 0 ? 'No transactions found' : 'No transactions match your filters'}
          </p>
        </div>
      ) : (
        <div className="space-y-3">
          {filteredTransactions.map((tx: Transaction) => {
            const StatusIcon = statusIcons[tx.status as keyof typeof statusIcons];

            return (
              <div
                key={tx.id}
                className={clsx(
                  'bg-white rounded-lg border-2 p-5 transition-all',
                  tx.riskLevel === 'critical' ? 'border-red-300 bg-red-50' :
                  tx.riskLevel === 'high' ? 'border-orange-300' :
                  'border-gray-200 hover:shadow-md'
                )}
              >
                <div className="flex items-start justify-between mb-3">
                  <div className="flex-1">
                    <div className="flex items-center gap-3 mb-2">
                      <Link
                        to={`/transactions/${tx.id}`}
                        className="text-lg font-semibold text-gray-900 hover:text-blue-600"
                      >
                        {tx.id}
                      </Link>
                      <span
                        className={clsx(
                          'px-3 py-1 rounded-full text-xs font-medium border uppercase',
                          riskColors[tx.riskLevel as keyof typeof riskColors]
                        )}
                      >
                        {tx.riskLevel} RISK
                      </span>
                      <StatusIcon className={clsx('w-5 h-5', statusColors[tx.status as keyof typeof statusColors])} />
                    </div>
                    {tx.description && (
                      <p className="text-sm text-gray-600">{tx.description}</p>
                    )}
                  </div>

                  <div className="text-right ml-4">
                    <div className="flex items-center gap-1 text-2xl font-bold text-gray-900">
                      <DollarSign className="w-6 h-6" />
                      {tx.amount.toLocaleString()}
                    </div>
                    <p className="text-xs text-gray-500 mt-1">{tx.currency}</p>
                  </div>
                </div>

                <div className="grid grid-cols-2 md:grid-cols-5 gap-4 mt-4 pt-4 border-t border-gray-200">
                  <div>
                    <p className="text-xs text-gray-600 mb-1">From Account</p>
                    <p className="text-sm font-mono font-medium text-gray-900">{tx.fromAccount}</p>
                  </div>

                  <div>
                    <p className="text-xs text-gray-600 mb-1">To Account</p>
                    <p className="text-sm font-mono font-medium text-gray-900">{tx.toAccount}</p>
                  </div>

                  <div>
                    <p className="text-xs text-gray-600 mb-1">Risk Score</p>
                    <p className="text-sm font-medium text-orange-600">{tx.riskScore.toFixed(1)}/100</p>
                  </div>

                  <div>
                    <p className="text-xs text-gray-600 mb-1">Status</p>
                    <p className="text-sm font-medium text-gray-900 capitalize">{tx.status}</p>
                  </div>

                  <div>
                    <p className="text-xs text-gray-600 mb-1">Timestamp</p>
                    <p className="text-sm font-medium text-gray-900">
                      {formatDistanceToNow(new Date(tx.timestamp), { addSuffix: true })}
                    </p>
                  </div>
                </div>

                {tx.fraudIndicators && tx.fraudIndicators.length > 0 && (
                  <div className="mt-3 p-3 bg-orange-50 border border-orange-200 rounded-lg">
                    <div className="flex items-center gap-2 mb-2">
                      <AlertTriangle className="w-4 h-4 text-orange-600" />
                      <p className="text-sm font-medium text-orange-800">Fraud Indicators Detected</p>
                    </div>
                    <div className="flex flex-wrap gap-2">
                      {tx.fraudIndicators.map((indicator: string, index: number) => (
                        <span
                          key={index}
                          className="px-2 py-1 bg-orange-100 text-orange-700 text-xs rounded-full"
                        >
                          {indicator}
                        </span>
                      ))}
                    </div>
                  </div>
                )}

                <div className="mt-3 flex gap-2">
                  <Link
                    to={`/transactions/${tx.id}`}
                    className="px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors text-sm font-medium"
                  >
                    View Details
                  </Link>

                  <button
                    onClick={() => analyzeTransaction(tx.id)}
                    disabled={isAnalyzing}
                    className="px-4 py-2 bg-white border border-gray-300 text-gray-700 rounded-lg hover:bg-gray-50 transition-colors text-sm font-medium disabled:opacity-50"
                  >
                    {isAnalyzing ? 'Analyzing...' : 'Re-analyze'}
                  </button>
                </div>
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
};

export default Transactions;
