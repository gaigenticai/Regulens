/**
 * Recent Decisions Widget
 * Displays recent MCDA decisions from backend
 * NO MOCKS - Real API data
 */

import React from 'react';
import { Link } from 'react-router-dom';
import { GitBranch, ArrowRight, CheckCircle, Clock, XCircle } from 'lucide-react';
import { formatDistanceToNow } from 'date-fns';
import { clsx } from 'clsx';
import type { Decision } from '@/types/api';

interface RecentDecisionsProps {
  decisions: Decision[];
  loading?: boolean;
}

const statusIcons = {
  draft: Clock,
  pending: Clock,
  approved: CheckCircle,
  rejected: XCircle,
  implemented: CheckCircle,
};

const statusColors = {
  draft: 'text-gray-600 bg-gray-100',
  pending: 'text-yellow-600 bg-yellow-100',
  approved: 'text-green-600 bg-green-100',
  rejected: 'text-red-600 bg-red-100',
  implemented: 'text-blue-600 bg-blue-100',
};

export const RecentDecisions: React.FC<RecentDecisionsProps> = ({ decisions, loading = false }) => {
  if (loading) {
    return (
      <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
        <h3 className="text-lg font-semibold text-gray-900 mb-4">Recent Decisions</h3>
        <div className="space-y-3">
          {[1, 2, 3].map((i) => (
            <div key={i} className="animate-pulse">
              <div className="h-4 bg-gray-200 rounded w-3/4 mb-2" />
              <div className="h-3 bg-gray-200 rounded w-1/2" />
            </div>
          ))}
        </div>
      </div>
    );
  }

  return (
    <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
      <div className="flex items-center justify-between mb-4">
        <h3 className="text-lg font-semibold text-gray-900 flex items-center gap-2">
          <GitBranch className="w-5 h-5 text-blue-600" />
          Recent Decisions
        </h3>
        <Link
          to="/decisions"
          className="text-sm text-blue-600 hover:text-blue-700 font-medium flex items-center gap-1"
        >
          View all
          <ArrowRight className="w-4 h-4" />
        </Link>
      </div>

      {decisions.length === 0 ? (
        <p className="text-gray-500 text-center py-8">No recent decisions</p>
      ) : (
        <div className="space-y-3">
          {decisions.map((decision) => {
            const StatusIcon = statusIcons[decision.status];
            const statusColor = statusColors[decision.status];

            return (
              <Link
                key={decision.id}
                to={`/decisions/${decision.id}`}
                className="block p-3 rounded-lg border border-gray-200 hover:bg-gray-50 transition-colors"
              >
                <div className="flex items-start justify-between gap-2 mb-2">
                  <h4 className="font-medium text-gray-900 text-sm line-clamp-1">
                    {decision.title}
                  </h4>
                  <div className={clsx('flex items-center gap-1 px-2 py-0.5 rounded-full text-xs font-medium', statusColor)}>
                    <StatusIcon className="w-3 h-3" />
                    {decision.status}
                  </div>
                </div>

                <p className="text-xs text-gray-600 mb-2 line-clamp-2">
                  {decision.description}
                </p>

                <div className="flex items-center justify-between text-xs text-gray-500">
                  <span className="font-medium">{decision.algorithm}</span>
                  <span>{formatDistanceToNow(new Date(decision.timestamp), { addSuffix: true })}</span>
                </div>

                {decision.confidenceScore !== undefined && (
                  <div className="mt-2 pt-2 border-t border-gray-100">
                    <div className="flex items-center justify-between text-xs mb-1">
                      <span className="text-gray-600">Confidence</span>
                      <span className="font-medium text-gray-900">
                        {(decision.confidenceScore * 100).toFixed(1)}%
                      </span>
                    </div>
                    <div className="w-full bg-gray-200 rounded-full h-1.5">
                      <div
                        className="bg-blue-600 h-1.5 rounded-full"
                        style={{ width: `${decision.confidenceScore * 100}%` }}
                      />
                    </div>
                  </div>
                )}
              </Link>
            );
          })}
        </div>
      )}
    </div>
  );
};

export default RecentDecisions;
