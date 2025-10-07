/**
 * Statistics Card Component
 * Displays real-time statistics from backend
 */

import React from 'react';
import { LucideIcon } from 'lucide-react';
import { clsx } from 'clsx';

interface StatsCardProps {
  title: string;
  value: string | number;
  change?: {
    value: number;
    label: string;
  };
  icon: LucideIcon;
  iconColor: string;
  iconBg: string;
  trend?: 'up' | 'down' | 'neutral';
  loading?: boolean;
}

export const StatsCard: React.FC<StatsCardProps> = ({
  title,
  value,
  change,
  icon: Icon,
  iconColor,
  iconBg,
  trend,
  loading = false,
}) => {
  return (
    <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
      <div className="flex items-center justify-between">
        <div className="flex-1">
          <p className="text-sm font-medium text-gray-600 mb-1">{title}</p>
          {loading ? (
            <div className="h-8 w-20 bg-gray-200 animate-pulse rounded" />
          ) : (
            <p className="text-2xl font-bold text-gray-900">{value}</p>
          )}
          {change && !loading && (
            <p className="text-xs text-gray-500 mt-1">
              <span
                className={clsx(
                  'font-medium',
                  trend === 'up' && 'text-green-600',
                  trend === 'down' && 'text-red-600',
                  trend === 'neutral' && 'text-gray-600'
                )}
              >
                {trend === 'up' && '↑ '}
                {trend === 'down' && '↓ '}
                {change.value}%
              </span>{' '}
              {change.label}
            </p>
          )}
        </div>
        <div className={clsx('w-12 h-12 rounded-lg flex items-center justify-center', iconBg)}>
          <Icon className={clsx('w-6 h-6', iconColor)} />
        </div>
      </div>
    </div>
  );
};

export default StatsCard;
