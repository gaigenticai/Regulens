/**
 * Real-Time Collaboration Dashboard
 * Feature 1: Agent collaboration with live reasoning streams
 * NO MOCKS - Production-grade UI with real API integration
 */

import React, { useState } from 'react';
import { Link } from 'react-router-dom';
import {
  Users,
  Activity,
  Brain,
  AlertCircle,
  Plus,
  RefreshCw,
  TrendingUp,
  CheckCircle,
} from 'lucide-react';
import {
  useCollaborationSessions,
  useCollaborationDashboardStats,
  useCreateCollaborationSession,
} from '@/hooks/useCollaborationSessions';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { useAuth } from '@/contexts/AuthContext';
import { toast } from 'react-hot-toast';

const CollaborationDashboard: React.FC = () => {
  const [statusFilter, setStatusFilter] = useState<string>('');
  const [showCreateModal, setShowCreateModal] = useState(false);
  const { user } = useAuth();

  const { data: sessions = [], isLoading: sessionsLoading, refetch: refetchSessions } = useCollaborationSessions(statusFilter, 50);
  const { data: stats, isLoading: statsLoading } = useCollaborationDashboardStats();
  const createSession = useCreateCollaborationSession();

  const handleCreateSession = async (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();

    if (!user) {
      toast.error('You must be logged in to create sessions');
      return;
    }

    const formData = new FormData(e.currentTarget);

    await createSession.mutateAsync({
      title: formData.get('title') as string,
      description: formData.get('description') as string,
      objective: formData.get('objective') as string,
      created_by: user.userId,
    });

    setShowCreateModal(false);
    e.currentTarget.reset();
  };

  if (sessionsLoading || statsLoading) {
    return <LoadingSpinner fullScreen message="Loading collaboration dashboard..." />;
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-gray-900">Collaboration Dashboard</h1>
          <p className="text-gray-600 mt-1">
            Real-time agent collaboration and decision-making transparency
          </p>
        </div>

        <div className="flex gap-3">
          <button
            onClick={() => refetchSessions()}
            className="flex items-center gap-2 px-4 py-2 bg-white border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors"
          >
            <RefreshCw className={`w-5 h-5 ${sessionsLoading ? 'animate-spin' : ''}`} />
            Refresh
          </button>
          
          <button
            onClick={() => setShowCreateModal(true)}
            className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors"
          >
            <Plus className="w-5 h-5" />
            New Session
          </button>
        </div>
      </div>

      {/* Statistics Cards */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-6">
        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-3 mb-2">
            <div className="p-2 bg-blue-100 rounded-lg">
              <Users className="w-5 h-5 text-blue-600" />
            </div>
            <h3 className="text-sm font-medium text-gray-600">Total Sessions</h3>
          </div>
          <p className="text-2xl font-bold text-gray-900">{stats?.total_sessions || 0}</p>
          <p className="text-xs text-gray-500 mt-1">{stats?.active_sessions || 0} active</p>
        </div>

        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-3 mb-2">
            <div className="p-2 bg-green-100 rounded-lg">
              <Activity className="w-5 h-5 text-green-600" />
            </div>
            <h3 className="text-sm font-medium text-gray-600">Active Sessions</h3>
          </div>
          <p className="text-2xl font-bold text-gray-900">{stats?.active_sessions || 0}</p>
          <p className="text-xs text-gray-500 mt-1">In progress</p>
        </div>

        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-3 mb-2">
            <div className="p-2 bg-purple-100 rounded-lg">
              <Brain className="w-5 h-5 text-purple-600" />
            </div>
            <h3 className="text-sm font-medium text-gray-600">Reasoning Steps</h3>
          </div>
          <p className="text-2xl font-bold text-gray-900">{stats?.total_reasoning_steps || 0}</p>
          <p className="text-xs text-gray-500 mt-1">Total analyzed</p>
        </div>

        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-3 mb-2">
            <div className="p-2 bg-orange-100 rounded-lg">
              <AlertCircle className="w-5 h-5 text-orange-600" />
            </div>
            <h3 className="text-sm font-medium text-gray-600">Human Overrides</h3>
          </div>
          <p className="text-2xl font-bold text-gray-900">{stats?.total_overrides || 0}</p>
          <p className="text-xs text-gray-500 mt-1">Interventions</p>
        </div>
      </div>

      {/* Filters */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-4">
        <div className="flex items-center gap-4">
          <label className="text-sm font-medium text-gray-700">Filter by status:</label>
          <div className="flex gap-2">
            {['', 'active', 'paused', 'completed'].map((status) => (
              <button
                key={status}
                onClick={() => setStatusFilter(status)}
                className={`px-3 py-1 rounded-lg text-sm font-medium transition-colors ${
                  statusFilter === status
                    ? 'bg-blue-600 text-white'
                    : 'bg-gray-100 text-gray-700 hover:bg-gray-200'
                }`}
              >
                {status || 'All'}
              </button>
            ))}
          </div>
        </div>
      </div>

      {/* Sessions List */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200">
        <div className="p-6 border-b border-gray-200">
          <h2 className="text-lg font-semibold text-gray-900">Collaboration Sessions</h2>
          <p className="text-sm text-gray-600 mt-1">View and manage agent collaboration sessions</p>
        </div>

        <div className="divide-y divide-gray-200">
          {sessions.length === 0 ? (
            <div className="p-12 text-center">
              <Users className="w-12 h-12 text-gray-400 mx-auto mb-4" />
              <h3 className="text-lg font-medium text-gray-900 mb-2">No sessions found</h3>
              <p className="text-gray-600 mb-4">Create a new collaboration session to get started</p>
              <button
                onClick={() => setShowCreateModal(true)}
                className="inline-flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors"
              >
                <Plus className="w-5 h-5" />
                Create Session
              </button>
            </div>
          ) : (
            sessions.map((session) => (
              <Link
                key={session.session_id}
                to={`/collaboration/${session.session_id}`}
                className="block p-6 hover:bg-gray-50 transition-colors"
              >
                <div className="flex items-start justify-between">
                  <div className="flex-1">
                    <div className="flex items-center gap-3 mb-2">
                      <h3 className="text-lg font-semibold text-gray-900">{session.title}</h3>
                      <span
                        className={`px-2 py-1 text-xs font-medium rounded-full ${
                          session.status === 'active'
                            ? 'bg-green-100 text-green-800'
                            : session.status === 'completed'
                            ? 'bg-blue-100 text-blue-800'
                            : session.status === 'paused'
                            ? 'bg-yellow-100 text-yellow-800'
                            : 'bg-gray-100 text-gray-800'
                        }`}
                      >
                        {session.status}
                      </span>
                    </div>
                    <p className="text-gray-600 text-sm mb-3">{session.description}</p>
                    <div className="flex items-center gap-6 text-sm text-gray-500">
                      <div className="flex items-center gap-1">
                        <Users className="w-4 h-4" />
                        <span>Created by {session.created_by}</span>
                      </div>
                      <div className="flex items-center gap-1">
                        <Activity className="w-4 h-4" />
                        <span>{new Date(session.created_at).toLocaleDateString()}</span>
                      </div>
                    </div>
                  </div>
                  <div className="flex items-center gap-2">
                    {session.status === 'active' && (
                      <div className="flex items-center gap-1 text-green-600 text-sm">
                        <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse" />
                        <span>Live</span>
                      </div>
                    )}
                  </div>
                </div>
              </Link>
            ))
          )}
        </div>
      </div>

      {/* Create Session Modal */}
      {showCreateModal && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
          <div className="bg-white rounded-lg shadow-xl max-w-md w-full mx-4">
            <div className="p-6 border-b border-gray-200">
              <h2 className="text-xl font-semibold text-gray-900">Create Collaboration Session</h2>
            </div>
            
            <form onSubmit={handleCreateSession} className="p-6 space-y-4">
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">
                  Session Title *
                </label>
                <input
                  type="text"
                  name="title"
                  required
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                  placeholder="e.g., Risk Assessment Review"
                />
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">
                  Description
                </label>
                <textarea
                  name="description"
                  rows={3}
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                  placeholder="Describe the purpose of this collaboration session..."
                />
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">
                  Objective
                </label>
                <input
                  type="text"
                  name="objective"
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                  placeholder="e.g., Evaluate compliance implications"
                />
              </div>

              <div className="flex gap-3 pt-4">
                <button
                  type="button"
                  onClick={() => setShowCreateModal(false)}
                  className="flex-1 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors"
                >
                  Cancel
                </button>
                <button
                  type="submit"
                  disabled={createSession.isPending}
                  className="flex-1 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors disabled:opacity-50"
                >
                  {createSession.isPending ? 'Creating...' : 'Create Session'}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  );
};

export default CollaborationDashboard;

