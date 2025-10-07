/**
 * Activity Item Component
 * Displays individual activity with real data
 */

import React from 'react';
import { formatDistanceToNow } from 'date-fns';
import {
  FileText,
  GitBranch,
  Database,
  Bot,
  AlertCircle,
  CheckCircle,
  Info,
  AlertTriangle,
} from 'lucide-react';
import { clsx } from 'clsx';
import type { ActivityItem as ActivityItemType } from '@/types/api';

interface ActivityItemProps {
  activity: ActivityItemType;
  onClick?: () => void;
}

const activityIcons = {
  regulatory_change: FileText,
  decision_made: GitBranch,
  data_ingestion: Database,
  agent_action: Bot,
  compliance_alert: AlertCircle,
};

const priorityColors = {
  low: 'text-gray-600 bg-gray-100',
  medium: 'text-blue-600 bg-blue-100',
  high: 'text-orange-600 bg-orange-100',
  critical: 'text-red-600 bg-red-100',
};

const priorityIcons = {
  low: Info,
  medium: CheckCircle,
  high: AlertTriangle,
  critical: AlertCircle,
};

export const ActivityItem: React.FC<ActivityItemProps> = ({ activity, onClick }) => {
  const Icon = activityIcons[activity.type] || FileText;
  const PriorityIcon = priorityIcons[activity.priority];
  const priorityColor = priorityColors[activity.priority];

  const timeAgo = formatDistanceToNow(new Date(activity.timestamp), { addSuffix: true });

  return (
    <div
      className={clsx(
        'flex items-start gap-4 p-4 rounded-lg border border-gray-200 bg-white transition-all',
        onClick && 'hover:bg-gray-50 cursor-pointer hover:shadow-sm'
      )}
      onClick={onClick}
    >
      {/* Icon */}
      <div className={clsx('flex-shrink-0 w-10 h-10 rounded-lg flex items-center justify-center', priorityColor)}>
        <Icon className="w-5 h-5" />
      </div>

      {/* Content */}
      <div className="flex-1 min-w-0">
        <div className="flex items-start justify-between gap-2 mb-1">
          <h4 className="font-semibold text-gray-900 text-sm">{activity.title}</h4>
          <div className="flex items-center gap-1 flex-shrink-0">
            <PriorityIcon className={clsx('w-4 h-4', priorityColor.split(' ')[0])} />
          </div>
        </div>

        <p className="text-sm text-gray-600 mb-2 line-clamp-2">{activity.description}</p>

        <div className="flex items-center justify-between gap-2 text-xs">
          <span className="text-gray-500">{timeAgo}</span>
          <div className="flex items-center gap-2">
            <span className="text-gray-500">by {activity.actor}</span>
            <span className={clsx('px-2 py-0.5 rounded-full font-medium capitalize', priorityColor)}>
              {activity.priority}
            </span>
          </div>
        </div>

        {/* Metadata */}
        {activity.metadata && Object.keys(activity.metadata).length > 0 && (
          <div className="mt-2 pt-2 border-t border-gray-100">
            <div className="flex flex-wrap gap-2">
              {Object.entries(activity.metadata).slice(0, 3).map(([key, value]) => (
                <span key={key} className="text-xs text-gray-500">
                  <span className="font-medium">{key}:</span> {String(value)}
                </span>
              ))}
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default ActivityItem;
