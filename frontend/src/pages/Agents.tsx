/**
 * Agents Page - Production Implementation
 * Real agent management interface with live monitoring
 * NO MOCKS - Real API integration with C++ backend
 */

import React, { useState, useEffect } from 'react';
import { Link } from 'react-router-dom';
import {
  Bot,
  Plus,
  Search,
  Filter,
  RefreshCw,
  Activity,
  CheckCircle,
  AlertCircle,
  Clock,
  XCircle,
  Settings,
  Eye,
  Play,
  Pause
} from 'lucide-react';
import { apiClient } from '@/services/api';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { AddAgentModal } from '@/components/AddAgentModal';
import type { Agent } from '@/types/api';

const Agents: React.FC = () => {
  const [agents, setAgents] = useState<Agent[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [searchTerm, setSearchTerm] = useState('');
  const [statusFilter, setStatusFilter] = useState<string>('all');
  const [typeFilter, setTypeFilter] = useState<string>('all');
  const [isRefreshing, setIsRefreshing] = useState(false);
  const [isAddAgentModalOpen, setIsAddAgentModalOpen] = useState(false);

  useEffect(() => {
    fetchAgents();
    
    // Auto-refresh every 30 seconds
    const interval = setInterval(fetchAgents, 30000);
    return () => clearInterval(interval);
  }, []);

  const fetchAgents = async () => {
    try {
      setError(null);
      const agentsData = await apiClient.getAgents();
      setAgents(agentsData);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load agents');
    } finally {
      setIsLoading(false);
      setIsRefreshing(false);
    }
  };

  const handleRefresh = async () => {
    setIsRefreshing(true);
    await fetchAgents();
  };

  const handleAgentAction = async (agentId: string, action: 'start' | 'stop') => {
    try {
      await apiClient.controlAgent(agentId, action);
      await fetchAgents(); // Refresh the list
    } catch (err) {
      setError(err instanceof Error ? err.message : `Failed to ${action} agent`);
    }
  };

  const getStatusIcon = (status: string) => {
    switch (status) {
      case 'active':
        return <CheckCircle className="w-5 h-5 text-green-500" />;
      case 'idle':
        return <Clock className="w-5 h-5 text-yellow-500" />;
      case 'error':
        return <AlertCircle className="w-5 h-5 text-red-500" />;
      case 'disabled':
        return <XCircle className="w-5 h-5 text-gray-500" />;
      default:
        return <Clock className="w-5 h-5 text-gray-500" />;
    }
  };

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'active': return 'bg-green-100 text-green-800';
      case 'idle': return 'bg-yellow-100 text-yellow-800';
      case 'error': return 'bg-red-100 text-red-800';
      case 'disabled': return 'bg-gray-100 text-gray-800';
      default: return 'bg-gray-100 text-gray-800';
    }
  };

  const filteredAgents = (Array.isArray(agents) ? agents : []).filter(agent => {
    const matchesSearch = agent.displayName && (agent.displayName.toLowerCase().includes(searchTerm.toLowerCase()) ||
                         agent.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
                         (agent.description && agent.description.toLowerCase().includes(searchTerm.toLowerCase())));
    const matchesStatus = statusFilter === 'all' || agent.status === statusFilter;
    const matchesType = typeFilter === 'all' || agent.type === typeFilter;

    return matchesSearch && matchesStatus && matchesType;
  });

  const agentTypes = [...new Set((Array.isArray(agents) ? agents : []).map(agent => agent.type).filter(Boolean))];
  const agentStatuses = ['active', 'idle', 'error', 'disabled'];

  if (isLoading) {
    return <LoadingSpinner fullScreen message="Loading agents..." />;
  }

  return (
    <div className="min-h-screen bg-gray-50">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
        {/* Header */}
        <div className="mb-8">
          <div className="flex items-center justify-between">
            <div>
              <h1 className="text-3xl font-bold text-gray-900">Agents</h1>
              <p className="mt-2 text-gray-600">
                Manage and monitor your AI agents
              </p>
            </div>
            <div className="flex space-x-3">
              <button
                onClick={handleRefresh}
                disabled={isRefreshing}
                className="inline-flex items-center px-4 py-2 bg-white border border-gray-300 rounded-lg text-gray-700 hover:bg-gray-50 disabled:opacity-50"
              >
                <RefreshCw className={`w-4 h-4 mr-2 ${isRefreshing ? 'animate-spin' : ''}`} />
                Refresh
              </button>
              <button 
                onClick={() => setIsAddAgentModalOpen(true)}
                className="inline-flex items-center px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700"
              >
                <Plus className="w-4 h-4 mr-2" />
                Add Agent
              </button>
            </div>
          </div>
        </div>

        {/* Filters */}
        <div className="bg-white rounded-lg shadow-sm p-6 mb-6">
          <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
            {/* Search */}
            <div className="relative">
              <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 text-gray-400 w-4 h-4" />
              <input
                type="text"
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
                className="w-full pl-10 pr-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
              />
            </div>
            
            {/* Status Filter */}
            <div className="relative">
              <Filter className="absolute left-3 top-1/2 transform -translate-y-1/2 text-gray-400 w-4 h-4" />
              <select
                value={statusFilter}
                onChange={(e) => setStatusFilter(e.target.value)}
                className="w-full pl-10 pr-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent appearance-none"
              >
                <option value="all">All Statuses</option>
                {agentStatuses.map(status => (
                  <option key={status} value={status}>
                    {status.charAt(0).toUpperCase() + status.slice(1)}
                  </option>
                ))}
              </select>
            </div>
            
            {/* Type Filter */}
            <div className="relative">
              <Bot className="absolute left-3 top-1/2 transform -translate-y-1/2 text-gray-400 w-4 h-4" />
              <select
                value={typeFilter}
                onChange={(e) => setTypeFilter(e.target.value)}
                className="w-full pl-10 pr-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent appearance-none"
              >
                <option value="all">All Types</option>
                {agentTypes.map(type => (
                  <option key={type} value={type}>
                    {type.replace('_', ' ').replace(/\b\w/g, l => l.toUpperCase())}
                  </option>
                ))}
              </select>
            </div>
            
            {/* Results Count */}
            <div className="flex items-center justify-end">
              <span className="text-sm text-gray-500">
                {filteredAgents.length} of {agents.length} agents
              </span>
            </div>
          </div>
        </div>

        {/* Error Display */}
        {error && (
          <div className="bg-red-50 border border-red-200 rounded-lg p-4 mb-6">
            <div className="flex items-center">
              <AlertCircle className="w-5 h-5 text-red-500 mr-2" />
              <span className="text-red-700">{error}</span>
            </div>
          </div>
        )}

        {/* Agents Grid */}
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
          {filteredAgents.map((agent) => (
            <div key={agent.id} className="bg-white rounded-lg shadow-sm border border-gray-200 hover:shadow-md transition-shadow">
              <div className="p-6">
                {/* Agent Header */}
                <div className="flex items-center justify-between mb-4">
                  <div className="flex items-center space-x-3">
                    <div className="p-2 bg-blue-100 rounded-lg">
                      <Bot className="w-6 h-6 text-blue-600" />
                    </div>
                    <div>
                      <h3 className="text-lg font-semibold text-gray-900">{agent.displayName}</h3>
                      <p className="text-sm text-gray-500 capitalize">{agent.type.replace('_', ' ')}</p>
                    </div>
                  </div>
                  <div className="flex items-center space-x-2">
                    {getStatusIcon(agent.status)}
                    <span className={`inline-flex items-center px-2 py-1 rounded-md text-xs font-medium ${getStatusColor(agent.status)}`}>
                      {agent.status}
                    </span>
                  </div>
                </div>

                {/* Agent Description */}
                <p className="text-sm text-gray-600 mb-4 line-clamp-2">{agent.description}</p>

                {/* Performance Metrics */}
                <div className="grid grid-cols-3 gap-3 mb-4">
                  <div className="text-center">
                    <p className="text-sm font-semibold text-gray-900">{agent.performance.tasksCompleted}</p>
                    <p className="text-xs text-gray-500">Tasks</p>
                  </div>
                  <div className="text-center">
                    <p className="text-sm font-semibold text-gray-900">{agent.performance.successRate}%</p>
                    <p className="text-xs text-gray-500">Success</p>
                  </div>
                  <div className="text-center">
                    <p className="text-sm font-semibold text-gray-900">{agent.performance.avgResponseTimeMs}ms</p>
                    <p className="text-xs text-gray-500">Response</p>
                  </div>
                </div>

                {/* Capabilities */}
                <div className="mb-4">
                  <p className="text-xs font-medium text-gray-700 mb-2">Capabilities</p>
                  <div className="flex flex-wrap gap-1">
                    {agent.capabilities.map((capability, index) => (
                      <span key={index} className="inline-flex items-center px-2 py-1 rounded-md text-xs font-medium bg-gray-100 text-gray-800">
                        {capability}
                      </span>
                    ))}
                  </div>
                </div>

                {/* Actions */}
                <div className="flex space-x-2">
                  <Link
                    to={`/agents/${agent.id}`}
                    className="flex-1 inline-flex items-center justify-center px-3 py-2 border border-gray-300 rounded-lg text-sm font-medium text-gray-700 bg-white hover:bg-gray-50"
                  >
                    <Eye className="w-4 h-4 mr-1" />
                    View
                  </Link>
                  {agent.status === 'active' ? (
                    <button
                      onClick={() => handleAgentAction(agent.id, 'stop')}
                      className="flex-1 inline-flex items-center justify-center px-3 py-2 bg-red-600 text-white rounded-lg text-sm font-medium hover:bg-red-700"
                    >
                      <Pause className="w-4 h-4 mr-1" />
                      Stop
                    </button>
                  ) : (
                    <button
                      onClick={() => handleAgentAction(agent.id, 'start')}
                      className="flex-1 inline-flex items-center justify-center px-3 py-2 bg-green-600 text-white rounded-lg text-sm font-medium hover:bg-green-700"
                    >
                      <Play className="w-4 h-4 mr-1" />
                      Start
                    </button>
                  )}
                </div>

                {/* Last Active */}
                <div className="mt-4 pt-4 border-t border-gray-100">
                  <p className="text-xs text-gray-500">
                    Last active: {new Date(agent.last_active).toLocaleString()}
                  </p>
                </div>
              </div>
            </div>
          ))}
        </div>

        {/* Empty State */}
        {filteredAgents.length === 0 && !isLoading && (
          <div className="text-center py-12">
            <Bot className="w-16 h-16 text-gray-300 mx-auto mb-4" />
            <h3 className="text-lg font-medium text-gray-900 mb-2">
              {searchTerm || statusFilter !== 'all' || typeFilter !== 'all' ? 'No agents found' : 'No agents available'}
            </h3>
            <p className="text-gray-500 mb-6">
              {searchTerm || statusFilter !== 'all' || typeFilter !== 'all'
                ? 'Try adjusting your search criteria or filters.'
                : 'Get started by adding your first agent.'}
            </p>
            {!searchTerm && statusFilter === 'all' && typeFilter === 'all' && (
              <button 
                onClick={() => setIsAddAgentModalOpen(true)}
                className="inline-flex items-center px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700"
              >
                <Plus className="w-4 h-4 mr-2" />
                Add Agent
              </button>
            )}
          </div>
        )}
      </div>
      
      {/* Add Agent Modal */}
      <AddAgentModal
        isOpen={isAddAgentModalOpen}
        onClose={() => setIsAddAgentModalOpen(false)}
        onAgentCreated={() => {
          fetchAgents(); // Refresh the agent list
        }}
      />
    </div>
  );
};

export default Agents;
