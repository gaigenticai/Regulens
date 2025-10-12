import React, { useState } from 'react';
import { Sparkles, Plus } from 'lucide-react';
import { useNLPolicies, useCreateNLPolicy } from '@/hooks/useNLPolicies';
import { LoadingSpinner } from '@/components/LoadingSpinner';

const NLPolicyBuilder: React.FC = () => {
  const [showCreate, setShowCreate] = useState(false);
  const { data: policies = [], isLoading } = useNLPolicies();
  const createPolicy = useCreateNLPolicy();

  const handleCreate = async (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    const formData = new FormData(e.currentTarget);
    await createPolicy.mutateAsync({
      natural_language_input: formData.get('natural_language_input') as string,
      rule_name: formData.get('rule_name') as string,
      created_by: 'current_user',
    });
    setShowCreate(false);
    e.currentTarget.reset();
  };

  if (isLoading) return <LoadingSpinner fullScreen message="Loading policies..." />;

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-gray-900">Natural Language Policy Builder</h1>
          <p className="text-gray-600 mt-1">GPT-4 powered compliance rule creation</p>
        </div>
        <button onClick={() => setShowCreate(true)} className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700">
          <Plus className="w-5 h-5" />
          Create Policy
        </button>
      </div>

      <div className="bg-white rounded-lg shadow-sm border border-gray-200">
        <div className="p-6 border-b border-gray-200">
          <h2 className="text-lg font-semibold text-gray-900">Generated Policies</h2>
        </div>
        <div className="divide-y divide-gray-200">
          {policies.length === 0 ? (
            <div className="p-12 text-center">
              <Sparkles className="w-12 h-12 text-gray-400 mx-auto mb-4" />
              <h3 className="text-lg font-medium text-gray-900 mb-2">No policies yet</h3>
              <p className="text-gray-600">Create your first policy using natural language</p>
            </div>
          ) : (
            policies.map((policy) => (
              <div key={policy.rule_id} className="p-6">
                <div className="flex items-start justify-between">
                  <div>
                    <h3 className="text-lg font-semibold text-gray-900">{policy.rule_name}</h3>
                    <p className="text-sm text-gray-600 mt-1">{policy.natural_language_input}</p>
                    <div className="flex items-center gap-3 mt-2 text-xs">
                      <span className={`px-2 py-1 rounded-full ${policy.validation_status === 'approved' ? 'bg-green-100 text-green-800' : 'bg-yellow-100 text-yellow-800'}`}>
                        {policy.validation_status}
                      </span>
                      <span>Confidence: {(policy.confidence_score * 100).toFixed(0)}%</span>
                      <span>{new Date(policy.created_at).toLocaleDateString()}</span>
                    </div>
                  </div>
                </div>
              </div>
            ))
          )}
        </div>
      </div>

      {showCreate && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
          <div className="bg-white rounded-lg shadow-xl max-w-md w-full mx-4">
            <div className="p-6 border-b border-gray-200">
              <h2 className="text-xl font-semibold text-gray-900">Create Policy from Natural Language</h2>
            </div>
            <form onSubmit={handleCreate} className="p-6 space-y-4">
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">Rule Name *</label>
                <input type="text" name="rule_name" required className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500" />
              </div>
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">Describe the policy in natural language *</label>
                <textarea name="natural_language_input" required rows={4} className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500" placeholder="e.g., All transactions over $10,000 must be flagged for review within 24 hours..." />
              </div>
              <div className="flex gap-3 pt-4">
                <button type="button" onClick={() => setShowCreate(false)} className="flex-1 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50">
                  Cancel
                </button>
                <button type="submit" disabled={createPolicy.isPending} className="flex-1 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 disabled:opacity-50">
                  {createPolicy.isPending ? 'Generating...' : 'Generate Policy'}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  );
};

export default NLPolicyBuilder;
