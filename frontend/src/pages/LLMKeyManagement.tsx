import React, { useState } from 'react';
import { Key, Plus, Trash2, Activity, DollarSign, TrendingUp } from 'lucide-react';
import { useLLMApiKeys, useCreateLLMApiKey, useDeleteLLMApiKey, useLLMKeyUsageStats } from '@/hooks/useLLMKeys';
import { LoadingSpinner } from '@/components/LoadingSpinner';

const LLMKeyManagement: React.FC = () => {
  const [showCreateModal, setShowCreateModal] = useState(false);
  const [showDeleteConfirm, setShowDeleteConfirm] = useState<string | null>(null);
  
  const { data: keys = [], isLoading } = useLLMApiKeys();
  const { data: usageStats = [] } = useLLMKeyUsageStats();
  const createKey = useCreateLLMApiKey();
  const deleteKey = useDeleteLLMApiKey();

  const handleCreateKey = async (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    const formData = new FormData(e.currentTarget);
    
    await createKey.mutateAsync({
      provider: formData.get('provider') as string,
      key_name: formData.get('key_name') as string,
      api_key: formData.get('api_key') as string,
      created_by: 'current_user',
      rate_limit_per_minute: parseInt(formData.get('rate_limit') as string) || 60,
    });
    
    setShowCreateModal(false);
    e.currentTarget.reset();
  };

  const handleDeleteKey = async (keyId: string) => {
    await deleteKey.mutateAsync(keyId);
    setShowDeleteConfirm(null);
  };

  if (isLoading) {
    return <LoadingSpinner fullScreen message="Loading API keys..." />;
  }

  const getProviderColor = (provider: string) => {
    const colors: Record<string, string> = {
      openai: 'bg-green-100 text-green-800',
      anthropic: 'bg-purple-100 text-purple-800',
      cohere: 'bg-blue-100 text-blue-800',
      google: 'bg-red-100 text-red-800',
      azure_openai: 'bg-cyan-100 text-cyan-800',
    };
    return colors[provider] || 'bg-gray-100 text-gray-800';
  };

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-gray-900">LLM API Key Management</h1>
          <p className="text-gray-600 mt-1">Manage API keys for LLM providers</p>
        </div>
        <button
          onClick={() => setShowCreateModal(true)}
          className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700"
        >
          <Plus className="w-5 h-5" />
          Add API Key
        </button>
      </div>

      {/* Usage Statistics */}
      {usageStats.length > 0 && (
        <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
          <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
            <div className="flex items-center gap-3 mb-2">
              <div className="p-2 bg-blue-100 rounded-lg">
                <Activity className="w-5 h-5 text-blue-600" />
              </div>
              <h3 className="text-sm font-medium text-gray-600">Total Requests</h3>
            </div>
            <p className="text-2xl font-bold text-gray-900">
              {usageStats.reduce((sum, s) => sum + s.total_requests, 0).toLocaleString()}
            </p>
            <p className="text-xs text-gray-500 mt-1">Last 7 days</p>
          </div>

          <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
            <div className="flex items-center gap-3 mb-2">
              <div className="p-2 bg-purple-100 rounded-lg">
                <TrendingUp className="w-5 h-5 text-purple-600" />
              </div>
              <h3 className="text-sm font-medium text-gray-600">Total Tokens</h3>
            </div>
            <p className="text-2xl font-bold text-gray-900">
              {usageStats.reduce((sum, s) => sum + s.total_tokens, 0).toLocaleString()}
            </p>
            <p className="text-xs text-gray-500 mt-1">Across all providers</p>
          </div>

          <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
            <div className="flex items-center gap-3 mb-2">
              <div className="p-2 bg-green-100 rounded-lg">
                <DollarSign className="w-5 h-5 text-green-600" />
              </div>
              <h3 className="text-sm font-medium text-gray-600">Total Cost</h3>
            </div>
            <p className="text-2xl font-bold text-gray-900">
              ${usageStats.reduce((sum, s) => sum + s.total_cost, 0).toFixed(2)}
            </p>
            <p className="text-xs text-gray-500 mt-1">Last 7 days</p>
          </div>
        </div>
      )}

      {/* API Keys List */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200">
        <div className="p-6 border-b border-gray-200">
          <h2 className="text-lg font-semibold text-gray-900">Configured API Keys</h2>
          <p className="text-sm text-gray-600 mt-1">Manage your LLM provider API keys</p>
        </div>

        <div className="divide-y divide-gray-200">
          {keys.length === 0 ? (
            <div className="p-12 text-center">
              <Key className="w-12 h-12 text-gray-400 mx-auto mb-4" />
              <h3 className="text-lg font-medium text-gray-900 mb-2">No API keys configured</h3>
              <p className="text-gray-600 mb-4">Add your first LLM provider API key to get started</p>
              <button
                onClick={() => setShowCreateModal(true)}
                className="inline-flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700"
              >
                <Plus className="w-5 h-5" />
                Add API Key
              </button>
            </div>
          ) : (
            keys.map((key) => (
              <div key={key.key_id} className="p-6 hover:bg-gray-50">
                <div className="flex items-center justify-between">
                  <div className="flex-1">
                    <div className="flex items-center gap-3 mb-2">
                      <h3 className="text-lg font-semibold text-gray-900">{key.key_name}</h3>
                      <span className={`px-2 py-1 text-xs font-medium rounded-full ${getProviderColor(key.provider)}`}>
                        {key.provider}
                      </span>
                      <span className={`px-2 py-1 text-xs font-medium rounded-full ${
                        key.is_active ? 'bg-green-100 text-green-800' : 'bg-gray-100 text-gray-800'
                      }`}>
                        {key.is_active ? 'Active' : 'Inactive'}
                      </span>
                    </div>
                    <div className="flex items-center gap-6 text-sm text-gray-500">
                      <div className="flex items-center gap-1">
                        <Activity className="w-4 h-4" />
                        <span>{key.usage_count} requests</span>
                      </div>
                      {key.rate_limit_per_minute && (
                        <div className="flex items-center gap-1">
                          <TrendingUp className="w-4 h-4" />
                          <span>{key.rate_limit_per_minute} req/min</span>
                        </div>
                      )}
                      <div>
                        Created {new Date(key.created_at).toLocaleDateString()}
                      </div>
                      {key.last_used_at && (
                        <div>
                          Last used {new Date(key.last_used_at).toLocaleDateString()}
                        </div>
                      )}
                    </div>
                  </div>
                  <button
                    onClick={() => setShowDeleteConfirm(key.key_id)}
                    className="flex items-center gap-2 px-3 py-2 text-red-600 hover:bg-red-50 rounded-lg"
                  >
                    <Trash2 className="w-4 h-4" />
                    Delete
                  </button>
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
              <h2 className="text-xl font-semibold text-gray-900">Add API Key</h2>
            </div>
            <form onSubmit={handleCreateKey} className="p-6 space-y-4">
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">Provider *</label>
                <select
                  name="provider"
                  required
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500"
                >
                  <option value="">Select provider...</option>
                  <option value="openai">OpenAI</option>
                  <option value="anthropic">Anthropic</option>
                  <option value="cohere">Cohere</option>
                  <option value="google">Google</option>
                  <option value="huggingface">HuggingFace</option>
                  <option value="azure_openai">Azure OpenAI</option>
                  <option value="custom">Custom</option>
                </select>
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">Key Name *</label>
                <input
                  type="text"
                  name="key_name"
                  required
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500"
                />
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">API Key *</label>
                <input
                  type="password"
                  name="api_key"
                  required
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500"
                />
                <p className="text-xs text-gray-500 mt-1">Key will be encrypted before storage</p>
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">Rate Limit (req/min)</label>
                <input
                  type="number"
                  name="rate_limit"
                  defaultValue={60}
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500"
                />
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
                  disabled={createKey.isPending}
                  className="flex-1 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 disabled:opacity-50"
                >
                  {createKey.isPending ? 'Adding...' : 'Add Key'}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* Delete Confirmation */}
      {showDeleteConfirm && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
          <div className="bg-white rounded-lg shadow-xl max-w-md w-full mx-4 p-6">
            <h2 className="text-xl font-semibold text-gray-900 mb-4">Delete API Key?</h2>
            <p className="text-gray-600 mb-6">
              Are you sure you want to delete this API key? This action cannot be undone.
            </p>
            <div className="flex gap-3">
              <button
                onClick={() => setShowDeleteConfirm(null)}
                className="flex-1 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50"
              >
                Cancel
              </button>
              <button
                onClick={() => handleDeleteKey(showDeleteConfirm)}
                disabled={deleteKey.isPending}
                className="flex-1 px-4 py-2 bg-red-600 text-white rounded-lg hover:bg-red-700 disabled:opacity-50"
              >
                {deleteKey.isPending ? 'Deleting...' : 'Delete'}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default LLMKeyManagement;

