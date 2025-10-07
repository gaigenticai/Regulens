/**
 * Regulatory Alerts Widget
 * Displays recent regulatory changes from backend
 */

import React from 'react';
import { Link } from 'react-router-dom';
import { FileText, ArrowRight, AlertCircle } from 'lucide-react';
import { formatDistanceToNow } from 'date-fns';
import { clsx } from 'clsx';
import type { RegulatoryChange } from '@/types/api';

interface RegulatoryAlertsProps {
  changes: RegulatoryChange[];
  loading?: boolean;
}

const severityColors = {
  low: 'text-gray-600 bg-gray-100',
  medium: 'text-blue-600 bg-blue-100',
  high: 'text-orange-600 bg-orange-100',
  critical: 'text-red-600 bg-red-100',
};

const sourceColors = {
  ECB: 'bg-blue-100 text-blue-700',
  SEC: 'bg-purple-100 text-purple-700',
  FCA: 'bg-green-100 text-green-700',
  RSS: 'bg-orange-100 text-orange-700',
  WebScraping: 'bg-gray-100 text-gray-700',
  Database: 'bg-indigo-100 text-indigo-700',
};

export const RegulatoryAlerts: React.FC<RegulatoryAlertsProps> = ({ changes, loading = false }) => {
  if (loading) {
    return (
      <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
        <h3 className="text-lg font-semibold text-gray-900 mb-4">Regulatory Alerts</h3>
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
          <FileText className="w-5 h-5 text-blue-600" />
          Regulatory Alerts
        </h3>
        <Link
          to="/regulatory"
          className="text-sm text-blue-600 hover:text-blue-700 font-medium flex items-center gap-1"
        >
          View all
          <ArrowRight className="w-4 h-4" />
        </Link>
      </div>

      {changes.length === 0 ? (
        <p className="text-gray-500 text-center py-8">No recent changes</p>
      ) : (
        <div className="space-y-3">
          {changes.map((change) => {
            const severityColor = severityColors[change.severity];
            const sourceColor = sourceColors[change.source] || sourceColors.Database;

            return (
              <div
                key={change.id}
                className="p-3 rounded-lg border border-gray-200 hover:bg-gray-50 transition-colors"
              >
                <div className="flex items-start justify-between gap-2 mb-2">
                  <h4 className="font-medium text-gray-900 text-sm line-clamp-1">
                    {change.title}
                  </h4>
                  <div className={clsx('flex items-center gap-1 px-2 py-0.5 rounded-full text-xs font-medium flex-shrink-0', severityColor)}>
                    {change.severity === 'critical' && <AlertCircle className="w-3 h-3" />}
                    {change.severity}
                  </div>
                </div>

                <p className="text-xs text-gray-600 mb-2 line-clamp-2">
                  {change.description}
                </p>

                <div className="flex items-center justify-between text-xs">
                  <span className={clsx('px-2 py-0.5 rounded-full font-medium', sourceColor)}>
                    {change.source}
                  </span>
                  <span className="text-gray-500">
                    {formatDistanceToNow(new Date(change.publishedDate), { addSuffix: true })}
                  </span>
                </div>

                {change.tags && change.tags.length > 0 && (
                  <div className="flex flex-wrap gap-1 mt-2 pt-2 border-t border-gray-100">
                    {change.tags.slice(0, 3).map((tag) => (
                      <span key={tag} className="px-2 py-0.5 bg-gray-100 text-gray-600 rounded text-xs">
                        {tag}
                      </span>
                    ))}
                  </div>
                )}
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
};

export default RegulatoryAlerts;
