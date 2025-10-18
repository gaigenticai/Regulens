/**
 * Activity Filters Component
 * Real filtering controls for activity feed
 */

import React from 'react';
import { Search, Filter, X } from 'lucide-react';
import { clsx } from 'clsx';

export interface ActivityFiltersState {
  search: string;
  type: string[];
  priority: string[];
}

interface ActivityFiltersProps {
  filters: ActivityFiltersState;
  onFiltersChange: (filters: ActivityFiltersState) => void;
}

const activityTypes = [
  { value: 'regulatory_change', label: 'Regulatory Changes' },
  { value: 'decision_made', label: 'Decisions' },
  { value: 'data_ingestion', label: 'Data Ingestion' },
  { value: 'agent_action', label: 'Agent Actions' },
  { value: 'compliance_alert', label: 'Compliance Alerts' },
];

const priorities = [
  { value: 'low', label: 'Low', color: 'bg-gray-100 text-gray-700' },
  { value: 'medium', label: 'Medium', color: 'bg-blue-100 text-blue-700' },
  { value: 'high', label: 'High', color: 'bg-orange-100 text-orange-700' },
  { value: 'critical', label: 'Critical', color: 'bg-red-100 text-red-700' },
];

export const ActivityFilters: React.FC<ActivityFiltersProps> = ({
  filters,
  onFiltersChange,
}) => {
  const [showFilters, setShowFilters] = React.useState(false);

  const handleSearchChange = (value: string) => {
    onFiltersChange({ ...filters, search: value });
  };

  const handleTypeToggle = (type: string) => {
    const newTypes = filters.type.includes(type)
      ? filters.type.filter((t) => t !== type)
      : [...filters.type, type];
    onFiltersChange({ ...filters, type: newTypes });
  };

  const handlePriorityToggle = (priority: string) => {
    const newPriorities = filters.priority.includes(priority)
      ? filters.priority.filter((p) => p !== priority)
      : [...filters.priority, priority];
    onFiltersChange({ ...filters, priority: newPriorities });
  };

  const handleClearFilters = () => {
    onFiltersChange({ search: '', type: [], priority: [] });
  };

  const hasActiveFilters = filters.search || filters.type.length > 0 || filters.priority.length > 0;

  return (
    <div className="space-y-4">
      {/* Search and Filter Toggle */}
      <div className="flex items-center gap-2">
        <div className="relative flex-1">
          <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-5 h-5 text-gray-400" />
          <input
            type="text"
            value={filters.search}
            onChange={(e) => handleSearchChange(e.target.value)}
            className="w-full pl-10 pr-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
          />
        </div>

        <button
          onClick={() => setShowFilters(!showFilters)}
          className={clsx(
            'flex items-center gap-2 px-4 py-2 rounded-lg border transition-colors',
            showFilters
              ? 'bg-blue-50 border-blue-300 text-blue-700'
              : 'bg-white border-gray-300 text-gray-700 hover:bg-gray-50'
          )}
        >
          <Filter className="w-5 h-5" />
          Filters
          {hasActiveFilters && (
            <span className="w-2 h-2 bg-blue-600 rounded-full" />
          )}
        </button>

        {hasActiveFilters && (
          <button
            onClick={handleClearFilters}
            className="flex items-center gap-2 px-4 py-2 rounded-lg border border-gray-300 text-gray-700 hover:bg-gray-50 transition-colors"
          >
            <X className="w-5 h-5" />
            Clear
          </button>
        )}
      </div>

      {/* Filter Options */}
      {showFilters && (
        <div className="bg-white border border-gray-200 rounded-lg p-4 space-y-4 animate-fade-in">
          {/* Activity Types */}
          <div>
            <h4 className="text-sm font-medium text-gray-900 mb-2">Activity Type</h4>
            <div className="flex flex-wrap gap-2">
              {activityTypes.map((type) => (
                <button
                  key={type.value}
                  onClick={() => handleTypeToggle(type.value)}
                  className={clsx(
                    'px-3 py-1.5 rounded-lg text-sm font-medium transition-colors',
                    filters.type.includes(type.value)
                      ? 'bg-blue-600 text-white'
                      : 'bg-gray-100 text-gray-700 hover:bg-gray-200'
                  )}
                >
                  {type.label}
                </button>
              ))}
            </div>
          </div>

          {/* Priorities */}
          <div>
            <h4 className="text-sm font-medium text-gray-900 mb-2">Priority</h4>
            <div className="flex flex-wrap gap-2">
              {priorities.map((priority) => (
                <button
                  key={priority.value}
                  onClick={() => handlePriorityToggle(priority.value)}
                  className={clsx(
                    'px-3 py-1.5 rounded-lg text-sm font-medium transition-colors border',
                    filters.priority.includes(priority.value)
                      ? priority.color + ' border-current'
                      : 'bg-white text-gray-700 border-gray-300 hover:bg-gray-50'
                  )}
                >
                  {priority.label}
                </button>
              ))}
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default ActivityFilters;
