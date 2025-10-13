/**
 * Activity Feed Page
 * Production-grade real-time activity monitoring
 * All data from database - no mocks, no fallbacks
 */

import React, { useState, useMemo } from 'react';
import { Activity as ActivityIcon, RefreshCw, AlertCircle } from 'lucide-react';
import { useActivityFeed } from '@/hooks/useActivityFeed';
import { useActivityStats } from '@/hooks/useActivityStats';
import { ActivityItem } from '@/components/ActivityFeed/ActivityItem';
import { ActivityFilters, ActivityFiltersState } from '@/components/ActivityFeed/ActivityFilters';
import { ActivityDetailModal } from '@/components/ActivityFeed/ActivityDetailModal';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { API } from '@/services/api';

const ActivityFeed: React.FC = () => {
  const [filters, setFilters] = useState<ActivityFiltersState>({
    search: '',
    type: [],
    priority: [],
  });

  const [selectedActivity, setSelectedActivity] = useState<API.ActivityItem | null>(null);
  const [isDetailModalOpen, setIsDetailModalOpen] = useState(false);

  // Fetch real activity data with polling (WebSocket disabled until backend supports it)
  const {
    activities,
    isLoading: activitiesLoading,
    isError: activitiesError,
    refetch: refetchActivities,
  } = useActivityFeed({
    limit: 100,
    enableRealtime: true,
    pollingInterval: 10000, // Poll every 10 seconds
  });

  // Fetch real statistics
  const {
    stats,
    isError: statsError,
    error: statsErrorDetails,
  } = useActivityStats();

  // Filter activities based on search and filters
  const filteredActivities = useMemo(() => {
    let filtered = [...activities];

    // Search filter
    if (filters.search) {
      const searchLower = filters.search.toLowerCase();
      filtered = filtered.filter(
        (activity) =>
          activity.title.toLowerCase().includes(searchLower) ||
          activity.description.toLowerCase().includes(searchLower) ||
          activity.actor.toLowerCase().includes(searchLower)
      );
    }

    // Type filter
    if (filters.type.length > 0) {
      filtered = filtered.filter((activity) => filters.type.includes(activity.type));
    }

    // Priority filter
    if (filters.priority.length > 0) {
      filtered = filtered.filter((activity) => filters.priority.includes(activity.priority));
    }

    return filtered;
  }, [activities, filters]);

  // Handle loading state
  if (activitiesLoading && activities.length === 0) {
    return <LoadingSpinner fullScreen message="Loading activity feed..." />;
  }

  // Handle error state
  if (activitiesError) {
    return (
      <div className="flex items-center justify-center min-h-[60vh]">
        <div className="text-center max-w-md">
          <div className="inline-flex items-center justify-center w-16 h-16 bg-red-100 rounded-full mb-4">
            <AlertCircle className="w-8 h-8 text-red-600" />
          </div>
          <h2 className="text-2xl font-bold text-gray-900 mb-2">Failed to Load Activities</h2>
          <p className="text-gray-600 mb-6">
            Unable to fetch activity data from the server. Please try again.
          </p>
          <button
            onClick={() => refetchActivities()}
            className="inline-flex items-center gap-2 px-6 py-3 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors"
          >
            <RefreshCw className="w-5 h-5" />
            Retry
          </button>
        </div>
      </div>
    );
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-gray-900 flex items-center gap-2">
            <ActivityIcon className="w-7 h-7 text-blue-600" />
            Activity Feed
          </h1>
          <p className="text-gray-600 mt-1">
            Real-time monitoring of all system activities
          </p>
        </div>

        <button
          onClick={() => refetchActivities()}
          disabled={activitiesLoading}
          className="flex items-center gap-2 px-4 py-2 bg-white border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors disabled:opacity-50"
        >
          <RefreshCw className={`w-5 h-5 ${activitiesLoading ? 'animate-spin' : ''}`} />
          Refresh
        </button>
      </div>

      {/* Statistics Cards */}
      {stats && !statsError && (
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Total Activities</p>
            <p className="text-2xl font-bold text-gray-900 mt-1">
              {stats.total.toLocaleString()}
            </p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Last 24 Hours</p>
            <p className="text-2xl font-bold text-blue-600 mt-1">
              {stats.last24Hours.toLocaleString()}
            </p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Critical Priority</p>
            <p className="text-2xl font-bold text-red-600 mt-1">
              {(stats.byPriority.critical || 0).toLocaleString()}
            </p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Compliance Alerts</p>
            <p className="text-2xl font-bold text-orange-600 mt-1">
              {(stats.byType.compliance_alert || 0).toLocaleString()}
            </p>
          </div>
        </div>
      )}

      {/* Statistics Error */}
      {statsError && (
        <div className="bg-red-50 border border-red-200 rounded-lg p-4">
          <h3 className="text-red-800 font-semibold">Failed to Load Activity Stats</h3>
          <p className="text-red-600 text-sm">{statsErrorDetails?.message || 'Unknown error'}</p>
          <button
            onClick={() => window.location.reload()}
            className="mt-2 text-red-600 underline hover:text-red-800"
          >
            Refresh Page
          </button>
        </div>
      )}

      {/* Filters */}
      <ActivityFilters filters={filters} onFiltersChange={setFilters} />

      {/* Activity List */}
      <div className="space-y-3">
        {filteredActivities.length === 0 ? (
          <div className="text-center py-12 bg-white rounded-lg border border-gray-200">
            <ActivityIcon className="w-12 h-12 text-gray-400 mx-auto mb-3" />
            <p className="text-gray-600">
              {activities.length === 0
                ? 'No activities to display'
                : 'No activities match your filters'}
            </p>
          </div>
        ) : (
          <>
            <div className="flex items-center justify-between text-sm text-gray-600">
              <span>
                Showing {filteredActivities.length} of {activities.length} activities
              </span>
              {activitiesLoading && (
                <span className="flex items-center gap-2">
                  <RefreshCw className="w-4 h-4 animate-spin" />
                  Updating...
                </span>
              )}
            </div>

            {filteredActivities.map((activity) => (
              <ActivityItem 
                key={activity.id} 
                activity={activity}
                onClick={() => {
                  setSelectedActivity(activity);
                  setIsDetailModalOpen(true);
                }}
              />
            ))}
          </>
        )}

        {/* Activity Detail Modal */}
        {selectedActivity && (
          <ActivityDetailModal
            activity={selectedActivity}
            isOpen={isDetailModalOpen}
            onClose={() => {
              setIsDetailModalOpen(false);
              setSelectedActivity(null);
            }}
          />
        )}
      </div>

      {/* Real-time indicator */}
      <div className="flex items-center justify-center gap-2 text-sm text-gray-500">
        <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse" />
        Live updates enabled
      </div>
    </div>
  );
};

export default ActivityFeed;
