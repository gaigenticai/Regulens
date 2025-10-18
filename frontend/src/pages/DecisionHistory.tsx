/**
 * Decision History Page
 * Lists all decisions with filtering and sorting
 * NO MOCKS - Real data from backend
 */

import React, { useState, useMemo } from 'react';
import { Link } from 'react-router-dom';
import { GitBranch, Search, Plus } from 'lucide-react';
import { formatDistanceToNow } from 'date-fns';
import { useDecisions } from '@/hooks/useDecisions';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { clsx } from 'clsx';
import type { Decision, MCDAAlgorithm } from '@/types/api';

const DecisionHistory: React.FC = () => {
  const { decisions, isLoading } = useDecisions({ limit: 100 });

  const [searchQuery, setSearchQuery] = useState('');
  const [filterAlgorithm, setFilterAlgorithm] = useState<MCDAAlgorithm | 'all'>('all');
  const [filterStatus, setFilterStatus] = useState<Decision['status'] | 'all'>('all');

  const filteredDecisions = useMemo(() => {
    return decisions.filter((decision) => {
      const matchesSearch =
        searchQuery === '' ||
        decision.title.toLowerCase().includes(searchQuery.toLowerCase()) ||
        decision.description.toLowerCase().includes(searchQuery.toLowerCase());

      const matchesAlgorithm = filterAlgorithm === 'all' || decision.algorithm === filterAlgorithm;

      const matchesStatus = filterStatus === 'all' || decision.status === filterStatus;

      return matchesSearch && matchesAlgorithm && matchesStatus;
    });
  }, [decisions, searchQuery, filterAlgorithm, filterStatus]);

  const statusColors = {
    draft: 'bg-gray-100 text-gray-700',
    pending: 'bg-yellow-100 text-yellow-700',
    approved: 'bg-green-100 text-green-700',
    rejected: 'bg-red-100 text-red-700',
    implemented: 'bg-blue-100 text-blue-700',
  };

  if (isLoading) {
    return <LoadingSpinner fullScreen message="Loading decisions..." />;
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-gray-900 flex items-center gap-2">
            <GitBranch className="w-7 h-7 text-blue-600" />
            Decision History
          </h1>
          <p className="text-gray-600 mt-1">View and manage all MCDA decisions</p>
        </div>

        <Link
          to="/decisions"
          className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors"
        >
          <Plus className="w-5 h-5" />
          New Decision
        </Link>
      </div>

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
              className="w-full pl-10 pr-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
            />
          </div>

          {/* Algorithm Filter */}
          <select
            value={filterAlgorithm}
            onChange={(e) => setFilterAlgorithm(e.target.value as any)}
            className="px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
          >
            <option value="all">All Algorithms</option>
            <option value="TOPSIS">TOPSIS</option>
            <option value="ELECTRE_III">ELECTRE III</option>
            <option value="PROMETHEE_II">PROMETHEE II</option>
            <option value="AHP">AHP</option>
            <option value="VIKOR">VIKOR</option>
          </select>

          {/* Status Filter */}
          <select
            value={filterStatus}
            onChange={(e) => setFilterStatus(e.target.value as any)}
            className="px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
          >
            <option value="all">All Statuses</option>
            <option value="draft">Draft</option>
            <option value="pending">Pending</option>
            <option value="approved">Approved</option>
            <option value="rejected">Rejected</option>
            <option value="implemented">Implemented</option>
          </select>
        </div>

        <div className="mt-3 text-sm text-gray-600">
          Showing {filteredDecisions.length} of {decisions.length} decisions
        </div>
      </div>

      {/* Decisions List */}
      {filteredDecisions.length === 0 ? (
        <div className="text-center py-12 bg-white rounded-lg border border-gray-200">
          <GitBranch className="w-12 h-12 text-gray-400 mx-auto mb-3" />
          <p className="text-gray-600">
            {decisions.length === 0 ? 'No decisions found' : 'No decisions match your filters'}
          </p>
        </div>
      ) : (
        <div className="grid grid-cols-1 gap-4">
          {filteredDecisions.map((decision) => (
            <Link
              key={decision.id}
              to={`/decisions/${decision.id}`}
              className="block bg-white rounded-lg border border-gray-200 p-6 hover:shadow-md transition-shadow"
            >
              <div className="flex items-start justify-between mb-3">
                <div className="flex-1">
                  <h3 className="text-lg font-semibold text-gray-900 mb-1">{decision.title}</h3>
                  <p className="text-sm text-gray-600 line-clamp-2">{decision.description}</p>
                </div>

                <div className={clsx('px-3 py-1 rounded-full text-xs font-medium capitalize ml-4', statusColors[decision.status])}>
                  {decision.status}
                </div>
              </div>

              <div className="grid grid-cols-2 md:grid-cols-5 gap-4 mt-4">
                <div>
                  <p className="text-xs text-gray-600 mb-1">Algorithm</p>
                  <p className="text-sm font-medium text-gray-900">{decision.algorithm}</p>
                </div>

                <div>
                  <p className="text-xs text-gray-600 mb-1">Confidence</p>
                  <p className="text-sm font-medium text-green-600">
                    {(decision.confidenceScore * 100).toFixed(1)}%
                  </p>
                </div>

                <div>
                  <p className="text-xs text-gray-600 mb-1">Criteria</p>
                  <p className="text-sm font-medium text-gray-900">{decision.criteria.length}</p>
                </div>

                <div>
                  <p className="text-xs text-gray-600 mb-1">Alternatives</p>
                  <p className="text-sm font-medium text-gray-900">{decision.alternatives.length}</p>
                </div>

                <div>
                  <p className="text-xs text-gray-600 mb-1">Created</p>
                  <p className="text-sm font-medium text-gray-900">
                    {formatDistanceToNow(new Date(decision.timestamp), { addSuffix: true })}
                  </p>
                </div>
              </div>

              <div className="mt-3 pt-3 border-t border-gray-100 text-sm text-gray-600">
                By {decision.decisionMaker}
                {decision.approvedBy && ` • Approved by ${decision.approvedBy}`}
              </div>
            </Link>
          ))}
        </div>
      )}
    </div>
  );
};

export default DecisionHistory;
