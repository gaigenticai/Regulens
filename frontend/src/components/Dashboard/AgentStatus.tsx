/**
 * Agent Status Widget
 * Displays real-time agent statuses from backend
 */

import React from 'react';
import { Link } from 'react-router-dom';
import { Bot, ArrowRight, Circle } from 'lucide-react';
import { clsx } from 'clsx';
import type { Agent } from '@/types/api';

interface AgentStatusProps {
  agents: Agent[];
  loading?: boolean;
}

const statusColors = {
  active: 'text-green-600 bg-green-100',
  idle: 'text-gray-600 bg-gray-100',
  error: 'text-red-600 bg-red-100',
  disabled: 'text-gray-400 bg-gray-50',
};

const statusDots = {
  active: 'bg-green-500',
  idle: 'bg-gray-400',
  error: 'bg-red-500',
  disabled: 'bg-gray-300',
};

export const AgentStatus: React.FC<AgentStatusProps> = ({ agents, loading = false }) => {
  if (loading) {
    return (
      <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
        <h3 className="text-lg font-semibold text-gray-900 mb-4">Agent Status</h3>
        <div className="space-y-3">
          {[1, 2, 3].map((i) => (
            <div key={i} className="animate-pulse flex items-center gap-3">
              <div className="w-10 h-10 bg-gray-200 rounded-lg" />
              <div className="flex-1">
                <div className="h-4 bg-gray-200 rounded w-2/3 mb-2" />
                <div className="h-3 bg-gray-200 rounded w-1/2" />
              </div>
            </div>
          ))}
        </div>
      </div>
    );
  }

  const activeAgents = agents.filter((a) => a.status === 'active').length;
  const totalAgents = agents.length;

  return (
    <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
      <div className="flex items-center justify-between mb-4">
        <div>
          <h3 className="text-lg font-semibold text-gray-900 flex items-center gap-2">
            <Bot className="w-5 h-5 text-blue-600" />
            Agent Status
          </h3>
          <p className="text-sm text-gray-600 mt-1">
            {activeAgents} of {totalAgents} active
          </p>
        </div>
        <Link
          to="/agents"
          className="text-sm text-blue-600 hover:text-blue-700 font-medium flex items-center gap-1"
        >
          View all
          <ArrowRight className="w-4 h-4" />
        </Link>
      </div>

      {agents.length === 0 ? (
        <p className="text-gray-500 text-center py-8">No agents configured</p>
      ) : (
        <div className="space-y-3">
          {agents.slice(0, 5).map((agent) => {
            const statusColor = statusColors[agent.status];
            const statusDot = statusDots[agent.status];

            return (
              <Link
                key={agent.id}
                to={`/agents/${agent.id}`}
                className="flex items-center gap-3 p-3 rounded-lg border border-gray-200 hover:bg-gray-50 transition-colors"
              >
                <div className={clsx('w-10 h-10 rounded-lg flex items-center justify-center', statusColor)}>
                  <Bot className="w-5 h-5" />
                </div>

                <div className="flex-1 min-w-0">
                  <div className="flex items-center gap-2 mb-1">
                    <h4 className="font-medium text-gray-900 text-sm truncate">
                      {agent.name}
                    </h4>
                    <div className="flex items-center gap-1">
                      <Circle className={clsx('w-2 h-2 fill-current', statusDot)} />
                    </div>
                  </div>

                  <p className="text-xs text-gray-600 truncate capitalize">
                    {agent.type.replace(/_/g, ' ')}
                  </p>

                  {agent.performance && (
                    <div className="flex items-center gap-3 mt-2 text-xs text-gray-500">
                      <span>
                        {agent.performance.tasksCompleted.toLocaleString()} tasks
                      </span>
                      <span>
                        {(agent.performance.successRate * 100).toFixed(0)}% success
                      </span>
                    </div>
                  )}
                </div>
              </Link>
            );
          })}
        </div>
      )}
    </div>
  );
};

export default AgentStatus;
