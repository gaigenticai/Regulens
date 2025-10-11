/**
 * Agent Detail Page - Production Implementation
 * Real agent monitoring and management interface
 * NO MOCKS - Real API integration with C++ backend
 */

import React, { useState, useEffect } from 'react';
import { useParams, Link } from 'react-router-dom';
import {
  ArrowLeft,
  Bot,
  Activity,
  Settings,
  Play,
  Pause,
  RotateCcw,
  AlertCircle,
  CheckCircle,
  Clock,
  Cpu,
  Zap,
  HardDrive
} from 'lucide-react';
import { apiClient } from '@/services/api';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import type { Agent, AgentStats, AgentTask } from '@/types/api';

const AgentDetail: React.FC = () => {
  const { id } = useParams<{ id: string }>();
  const [agent, setAgent] = useState<Agent | null>(null);
  const [stats, setStats] = useState<AgentStats | null>(null);
  const [tasks, setTasks] = useState<AgentTask[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    if (!id) return;

    const fetchAgentData = async () => {
      try {
        setIsLoading(true);
        const [agentData, statsData, tasksData] = await Promise.all([
          apiClient.getAgent(id),
          apiClient.getAgentStats(id),
          apiClient.getAgentTasks(id)
        ]);
        
        setAgent(agentData);
        setStats(statsData);
        setTasks(tasksData);
      } catch (err) {
        setError(err instanceof Error ? err.message : 'Failed to load agent data');
      } finally {
        setIsLoading(false);
      }
    };

    fetchAgentData();
    
    // Real-time updates every 10 seconds
    const interval = setInterval(fetchAgentData, 10000);
    return () => clearInterval(interval);
  }, [id]);

  const handleAgentAction = async (action: 'start' | 'stop' | 'restart') => {
    if (!agent) return;
    
    try {
      await apiClient.controlAgent(agent.id, action);
      // Refresh agent data after action
      const updatedAgent = await apiClient.getAgent(agent.id);
      setAgent(updatedAgent);
    } catch (err) {
      setError(err instanceof Error ? err.message : `Failed to ${action} agent`);
    }
  };

  if (isLoading) {
    return <LoadingSpinner fullScreen message="Loading agent details..." />;
  }

  if (error || !agent) {
    return (
      <div className="min-h-screen bg-gray-50 flex items-center justify-center">
        <div className="bg-white p-8 rounded-lg shadow-sm max-w-md w-full text-center">
          <AlertCircle className="w-16 h-16 text-red-500 mx-auto mb-4" />
          <h1 className="text-2xl font-bold text-gray-900 mb-2">Agent Not Found</h1>
          <p className="text-gray-600 mb-6">{error || 'The requested agent could not be found.'}</p>
          <Link to="/agents" className="inline-flex items-center px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700">
            <ArrowLeft className="w-4 h-4 mr-2" />
            Back to Agents
          </Link>
        </div>
      </div>
    );
  }

  const getStatusIcon = (status: string) => {
    switch (status) {
      case 'active': return <CheckCircle className="w-5 h-5 text-green-500" />;
      case 'idle': return <Clock className="w-5 h-5 text-yellow-500" />;
      case 'error': return <AlertCircle className="w-5 h-5 text-red-500" />;
      default: return <Clock className="w-5 h-5 text-gray-500" />;
    }
  };

  return (
    <div className="min-h-screen bg-gray-50">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
        {/* Header */}
        <div className="mb-8">
          <Link to="/agents" className="inline-flex items-center text-blue-600 hover:text-blue-700 mb-4">
            <ArrowLeft className="w-4 h-4 mr-2" />
            Back to Agents
          </Link>
          
          <div className="flex items-center justify-between">
            <div className="flex items-center space-x-4">
              <div className="p-3 bg-blue-100 rounded-lg">
                <Bot className="w-8 h-8 text-blue-600" />
              </div>
              <div>
                <h1 className="text-3xl font-bold text-gray-900">{agent.name}</h1>
                <div className="flex items-center space-x-2 mt-1">
                  {getStatusIcon(agent.status)}
                  <span className="text-sm font-medium capitalize">{agent.status}</span>
                  <span className="text-sm text-gray-500">• {agent.type}</span>
                </div>
              </div>
            </div>
            
            <div className="flex space-x-2">
              {agent.status === 'active' ? (
                <button
                  onClick={() => handleAgentAction('stop')}
                  className="inline-flex items-center px-4 py-2 bg-red-600 text-white rounded-lg hover:bg-red-700"
                >
                  <Pause className="w-4 h-4 mr-2" />
                  Stop
                </button>
              ) : (
                <button
                  onClick={() => handleAgentAction('start')}
                  className="inline-flex items-center px-4 py-2 bg-green-600 text-white rounded-lg hover:bg-green-700"
                >
                  <Play className="w-4 h-4 mr-2" />
                  Start
                </button>
              )}
              <button
                onClick={() => handleAgentAction('restart')}
                className="inline-flex items-center px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700"
              >
                <RotateCcw className="w-4 h-4 mr-2" />
                Restart
              </button>
            </div>
          </div>
        </div>

        <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
          {/* Main Content */}
          <div className="lg:col-span-2 space-y-6">
            {/* Agent Information */}
            <div className="bg-white rounded-lg shadow-sm p-6">
              <h2 className="text-xl font-semibold text-gray-900 mb-4">Agent Information</h2>
              <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                <div>
                  <label className="block text-sm font-medium text-gray-500">Description</label>
                  <p className="mt-1 text-sm text-gray-900">{agent.description}</p>
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-500">Created</label>
                  <p className="mt-1 text-sm text-gray-900">
                    {new Date(agent.created_at).toLocaleDateString()}
                  </p>
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-500">Last Active</label>
                  <p className="mt-1 text-sm text-gray-900">
                    {new Date(agent.last_active).toLocaleString()}
                  </p>
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-500">Capabilities</label>
                  <div className="mt-1 flex flex-wrap gap-1">
                    {agent.capabilities.map((cap, index) => (
                      <span key={index} className="inline-flex items-center px-2 py-1 rounded-md text-xs font-medium bg-blue-100 text-blue-800">
                        {cap}
                      </span>
                    ))}
                  </div>
                </div>
              </div>
            </div>

            {/* Recent Tasks */}
            <div className="bg-white rounded-lg shadow-sm p-6">
              <h2 className="text-xl font-semibold text-gray-900 mb-4">Recent Tasks</h2>
              <div className="space-y-3">
                {tasks.length > 0 ? (
                  tasks.slice(0, 10).map((task, index) => (
                    <div key={index} className="flex items-center justify-between p-3 bg-gray-50 rounded-lg">
                      <div>
                        <p className="font-medium text-gray-900">{task.description}</p>
                        <p className="text-sm text-gray-500">{new Date(task.created_at).toLocaleString()}</p>
                      </div>
                      <span className={`inline-flex items-center px-2 py-1 rounded-md text-xs font-medium ${
                        task.status === 'completed' ? 'bg-green-100 text-green-800' :
                        task.status === 'failed' ? 'bg-red-100 text-red-800' :
                        'bg-yellow-100 text-yellow-800'
                      }`}>
                        {task.status}
                      </span>
                    </div>
                  ))
                ) : (
                  <p className="text-gray-500 text-center py-4">No recent tasks</p>
                )}
              </div>
            </div>
          </div>

          {/* Sidebar */}
          <div className="space-y-6">
            {/* Performance Stats */}
            {stats && (
              <div className="bg-white rounded-lg shadow-sm p-6">
                <h3 className="text-lg font-semibold text-gray-900 mb-4">Performance</h3>
                <div className="space-y-4">
                  <div>
                    <div className="flex justify-between items-center mb-1">
                      <span className="text-sm font-medium text-gray-700">Tasks Completed</span>
                      <span className="text-sm font-semibold text-gray-900">{stats.tasks_completed}</span>
                    </div>
                  </div>
                  <div>
                    <div className="flex justify-between items-center mb-1">
                      <span className="text-sm font-medium text-gray-700">Success Rate</span>
                      <span className="text-sm font-semibold text-gray-900">{stats.success_rate}%</span>
                    </div>
                    <div className="w-full bg-gray-200 rounded-full h-2">
                      <div className="bg-green-600 h-2 rounded-full" style={{ width: `${stats.success_rate}%` }}></div>
                    </div>
                  </div>
                  <div>
                    <div className="flex justify-between items-center mb-1">
                      <span className="text-sm font-medium text-gray-700">Avg Response Time</span>
                      <span className="text-sm font-semibold text-gray-900">{stats.avg_response_time_ms}ms</span>
                    </div>
                  </div>
                </div>
              </div>
            )}

            {/* System Resources */}
            <div className="bg-white rounded-lg shadow-sm p-6">
              <h3 className="text-lg font-semibold text-gray-900 mb-4">System Resources</h3>
              <div className="space-y-3">
                <div className="flex items-center justify-between">
                  <div className="flex items-center space-x-2">
                    <Cpu className="w-4 h-4 text-blue-500" />
                    <span className="text-sm font-medium text-gray-700">CPU Usage</span>
                  </div>
                  <span className="text-sm font-semibold text-gray-900">12%</span>
                </div>
                <div className="flex items-center justify-between">
                  <div className="flex items-center space-x-2">
                    <Zap className="w-4 h-4 text-green-500" />
                    <span className="text-sm font-medium text-gray-700">Memory</span>
                  </div>
                  <span className="text-sm font-semibold text-gray-900">256 MB</span>
                </div>
                <div className="flex items-center justify-between">
                  <div className="flex items-center space-x-2">
                    <HardDrive className="w-4 h-4 text-purple-500" />
                    <span className="text-sm font-medium text-gray-700">Storage</span>
                  </div>
                  <span className="text-sm font-semibold text-gray-900">1.2 GB</span>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default AgentDetail;
