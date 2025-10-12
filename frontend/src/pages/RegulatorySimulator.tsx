import React, { useState } from 'react';
import { Play, Plus, FileText } from 'lucide-react';
import { useSimulations, useCreateSimulation, useSimulationTemplates } from '@/hooks/useSimulations';
import { LoadingSpinner } from '@/components/LoadingSpinner';

const RegulatorySimulator: React.FC = () => {
  const [showCreateModal, setShowCreateModal] = useState(false);
  const { data: simulations = [], isLoading } = useSimulations();
  const { data: templates = [] } = useSimulationTemplates();
  const createSim = useCreateSimulation();

  const handleCreate = async (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    const formData = new FormData(e.currentTarget);
    await createSim.mutateAsync({
      name: formData.get('name') as string,
      simulation_type: formData.get('simulation_type') as string,
      created_by: 'current_user',
    });
    setShowCreateModal(false);
    e.currentTarget.reset();
  };

  if (isLoading) {
    return <LoadingSpinner fullScreen message="Loading simulator..." />;
  }

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'completed': return 'text-green-600 bg-green-100';
      case 'running': return 'text-blue-600 bg-blue-100';
      case 'draft': return 'text-gray-600 bg-gray-100';
      case 'failed': return 'text-red-600 bg-red-100';
      default: return 'text-gray-600 bg-gray-100';
    }
  };

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-gray-900">Regulatory Change Simulator</h1>
          <p className="text-gray-600 mt-1">What-if scenario analysis sandbox</p>
        </div>
        <button
          onClick={() => setShowCreateModal(true)}
          className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700"
        >
          <Plus className="w-5 h-5" />
          New Simulation
        </button>
      </div>

      {/* Templates */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
        <h2 className="text-lg font-semibold text-gray-900 mb-4">Simulation Templates</h2>
        <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
          {templates.map((template) => (
            <div key={template.template_id} className="p-4 border border-gray-200 rounded-lg hover:border-blue-500 cursor-pointer">
              <div className="flex items-start gap-3">
                <div className="p-2 bg-blue-100 rounded-lg">
                  <FileText className="w-5 h-5 text-blue-600" />
                </div>
                <div className="flex-1">
                  <h3 className="font-semibold text-gray-900">{template.template_name}</h3>
                  <p className="text-sm text-gray-600 mt-1">{template.description}</p>
                  <p className="text-xs text-gray-500 mt-2">Used {template.usage_count} times</p>
                </div>
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Simulations */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200">
        <div className="p-6 border-b border-gray-200">
          <h2 className="text-lg font-semibold text-gray-900">Your Simulations</h2>
        </div>
        <div className="divide-y divide-gray-200">
          {simulations.length === 0 ? (
            <div className="p-12 text-center">
              <Play className="w-12 h-12 text-gray-400 mx-auto mb-4" />
              <h3 className="text-lg font-medium text-gray-900 mb-2">No simulations yet</h3>
              <p className="text-gray-600 mb-4">Create your first simulation to analyze regulatory impacts</p>
            </div>
          ) : (
            simulations.map((sim) => (
              <div key={sim.simulation_id} className="p-6 hover:bg-gray-50">
                <div className="flex items-start justify-between">
                  <div>
                    <div className="flex items-center gap-3 mb-2">
                      <h3 className="text-lg font-semibold text-gray-900">{sim.name}</h3>
                      <span className={`px-2 py-1 text-xs font-medium rounded-full ${getStatusColor(sim.status)}`}>
                        {sim.status}
                      </span>
                    </div>
                    <div className="text-sm text-gray-600">
                      <p>Type: {sim.simulation_type}</p>
                      <p>Created: {new Date(sim.created_at).toLocaleString()}</p>
                      {sim.completed_at && <p>Completed: {new Date(sim.completed_at).toLocaleString()}</p>}
                    </div>
                  </div>
                  {sim.status === 'completed' && (
                    <button className="px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700">
                      View Results
                    </button>
                  )}
                </div>
              </div>
            ))
          )}
        </div>
      </div>

      {/* Create Modal */}
      {showCreateModal && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
          <div className="bg-white rounded-lg shadow-xl max-w-md w-full mx-4">
            <div className="p-6 border-b border-gray-200">
              <h2 className="text-xl font-semibold text-gray-900">Create Simulation</h2>
            </div>
            <form onSubmit={handleCreate} className="p-6 space-y-4">
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">Name *</label>
                <input
                  type="text"
                  name="name"
                  required
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500"
                />
              </div>
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">Type *</label>
                <select
                  name="simulation_type"
                  required
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500"
                >
                  <option value="regulatory_change">Regulatory Change</option>
                  <option value="policy_update">Policy Update</option>
                  <option value="market_shift">Market Shift</option>
                  <option value="risk_event">Risk Event</option>
                  <option value="custom">Custom</option>
                </select>
              </div>
              <div className="flex gap-3 pt-4">
                <button
                  type="button"
                  onClick={() => setShowCreateModal(false)}
                  className="flex-1 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50"
                >
                  Cancel
                </button>
                <button
                  type="submit"
                  disabled={createSim.isPending}
                  className="flex-1 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 disabled:opacity-50"
                >
                  {createSim.isPending ? 'Creating...' : 'Create'}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  );
};

export default RegulatorySimulator;

