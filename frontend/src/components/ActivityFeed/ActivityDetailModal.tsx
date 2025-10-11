/**
 * Activity Detail Modal Component
 * Production-grade modal for displaying full activity details
 * Shows: Who did what, when, detailed metadata
 */

import React from 'react';
import { X, User, Calendar, Tag, Info } from 'lucide-react';
import { API } from '@/services/api';

interface ActivityDetailModalProps {
  activity: API.ActivityItem;
  isOpen: boolean;
  onClose: () => void;
}

export const ActivityDetailModal: React.FC<ActivityDetailModalProps> = ({
  activity,
  isOpen,
  onClose,
}) => {
  if (!isOpen) return null;

  const getPriorityColor = (priority: string) => {
    switch (priority) {
      case 'critical': return 'text-red-600 bg-red-100';
      case 'high': return 'text-orange-600 bg-orange-100';
      case 'medium': return 'text-yellow-600 bg-yellow-100';
      case 'low': return 'text-blue-600 bg-blue-100';
      default: return 'text-gray-600 bg-gray-100';
    }
  };

  const getTypeColor = (type: string) => {
    switch (type) {
      case 'agent_control': return 'text-purple-600 bg-purple-100';
      case 'agent_action': return 'text-blue-600 bg-blue-100';
      case 'regulatory_change': return 'text-green-600 bg-green-100';
      case 'decision_made': return 'text-indigo-600 bg-indigo-100';
      case 'data_ingestion': return 'text-cyan-600 bg-cyan-100';
      case 'compliance_alert': return 'text-red-600 bg-red-100';
      default: return 'text-gray-600 bg-gray-100';
    }
  };

  const formatDate = (dateString: string) => {
    const date = new Date(dateString);
    return date.toLocaleString('en-US', {
      year: 'numeric',
      month: 'long',
      day: 'numeric',
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
    });
  };

  // Extract user info from metadata
  const username = activity.metadata?.username || activity.actor || 'System';
  const userId = activity.metadata?.user_id || 'N/A';
  const agentId = activity.metadata?.agent_id || 'N/A';
  const action = activity.metadata?.action || 'N/A';
  const status = activity.metadata?.status || 'N/A';

  return (
    <div className="fixed inset-0 z-50 overflow-y-auto">
      <div className="flex items-center justify-center min-h-screen px-4 pt-4 pb-20 text-center sm:block sm:p-0">
        {/* Background overlay */}
        <div
          className="fixed inset-0 transition-opacity bg-gray-500 bg-opacity-75"
          onClick={onClose}
        />

        {/* Modal panel */}
        <div className="inline-block align-bottom bg-white rounded-lg text-left overflow-hidden shadow-xl transform transition-all sm:my-8 sm:align-middle sm:max-w-3xl sm:w-full">
          {/* Header */}
          <div className="bg-gradient-to-r from-blue-600 to-indigo-600 px-6 py-4">
            <div className="flex items-center justify-between">
              <h3 className="text-xl font-semibold text-white">Activity Details</h3>
              <button
                onClick={onClose}
                className="text-white hover:text-gray-200 transition-colors"
              >
                <X className="w-6 h-6" />
              </button>
            </div>
          </div>

          {/* Body */}
          <div className="bg-white px-6 py-6 space-y-6">
            {/* Title & Description */}
            <div>
              <h4 className="text-lg font-semibold text-gray-900 mb-2">{activity.title}</h4>
              <p className="text-gray-600">{activity.description}</p>
            </div>

            {/* Tags */}
            <div className="flex flex-wrap gap-3">
              <span className={`inline-flex items-center px-3 py-1 rounded-full text-sm font-medium ${getTypeColor(activity.type)}`}>
                <Tag className="w-4 h-4 mr-1" />
                {activity.type.replace(/_/g, ' ')}
              </span>
              <span className={`inline-flex items-center px-3 py-1 rounded-full text-sm font-medium ${getPriorityColor(activity.priority)}`}>
                <Info className="w-4 h-4 mr-1" />
                {activity.priority} priority
              </span>
            </div>

            {/* User Attribution */}
            <div className="border-t border-gray-200 pt-4">
              <h5 className="text-sm font-semibold text-gray-700 mb-3">User Attribution</h5>
              <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                <div className="flex items-start space-x-3">
                  <User className="w-5 h-5 text-gray-400 mt-0.5" />
                  <div>
                    <p className="text-sm font-medium text-gray-700">Performed By</p>
                    <p className="text-sm text-gray-900">{username}</p>
                    {userId && userId !== 'N/A' && (
                      <p className="text-xs text-gray-500 mt-1">User ID: {userId}</p>
                    )}
                  </div>
                </div>
                <div className="flex items-start space-x-3">
                  <Calendar className="w-5 h-5 text-gray-400 mt-0.5" />
                  <div>
                    <p className="text-sm font-medium text-gray-700">When</p>
                    <p className="text-sm text-gray-900">{formatDate(activity.timestamp)}</p>
                  </div>
                </div>
              </div>
            </div>

            {/* Action Details */}
            <div className="border-t border-gray-200 pt-4">
              <h5 className="text-sm font-semibold text-gray-700 mb-3">Action Details</h5>
              <div className="bg-gray-50 rounded-lg p-4 space-y-2">
                <div className="flex justify-between">
                  <span className="text-sm font-medium text-gray-600">Action:</span>
                  <span className="text-sm text-gray-900">{action}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-sm font-medium text-gray-600">Status:</span>
                  <span className="text-sm text-gray-900">{status}</span>
                </div>
                {agentId && agentId !== 'N/A' && (
                  <div className="flex justify-between">
                    <span className="text-sm font-medium text-gray-600">Agent ID:</span>
                    <span className="text-sm text-gray-900 font-mono">{agentId.substring(0, 13)}...</span>
                  </div>
                )}
              </div>
            </div>

            {/* Raw Metadata */}
            {activity.metadata && Object.keys(activity.metadata).length > 0 && (
              <div className="border-t border-gray-200 pt-4">
                <h5 className="text-sm font-semibold text-gray-700 mb-3">Technical Metadata</h5>
                <div className="bg-gray-900 rounded-lg p-4 overflow-x-auto">
                  <pre className="text-xs text-green-400 font-mono">
                    {JSON.stringify(activity.metadata, null, 2)}
                  </pre>
                </div>
              </div>
            )}
          </div>

          {/* Footer */}
          <div className="bg-gray-50 px-6 py-4 flex justify-end">
            <button
              onClick={onClose}
              className="px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors font-medium"
            >
              Close
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};

