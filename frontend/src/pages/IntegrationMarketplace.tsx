import React from 'react';
import { Package, CheckCircle, Activity } from 'lucide-react';
import { useIntegrationConnectors, useIntegrationInstances } from '@/hooks/useIntegrations';
import { LoadingSpinner } from '@/components/LoadingSpinner';

const IntegrationMarketplace: React.FC = () => {
  const { data: connectors = [], isLoading: connectorsLoading } = useIntegrationConnectors();
  const { data: instances = [] } = useIntegrationInstances();

  if (connectorsLoading) {
    return <LoadingSpinner fullScreen message="Loading integrations..." />;
  }

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold text-gray-900">Integration Marketplace</h1>
        <p className="text-gray-600 mt-1">Pre-built connectors for enterprise systems</p>
      </div>

      {/* Active Instances */}
      {instances.length > 0 && (
        <div className="bg-white rounded-lg shadow-sm border border-gray-200">
          <div className="p-6 border-b border-gray-200">
            <h2 className="text-lg font-semibold text-gray-900">Active Integrations</h2>
          </div>
          <div className="divide-y divide-gray-200">
            {instances.map((instance) => (
              <div key={instance.instance_id} className="p-6 flex items-center justify-between">
                <div className="flex items-center gap-4">
                  <div className="p-3 bg-blue-100 rounded-lg">
                    <Activity className="w-6 h-6 text-blue-600" />
                  </div>
                  <div>
                    <h3 className="text-lg font-semibold text-gray-900">{instance.instance_name}</h3>
                    <p className="text-sm text-gray-600">{instance.connector_name}</p>
                    <p className="text-xs text-gray-500 mt-1">
                      Last sync: {instance.last_sync_at ? new Date(instance.last_sync_at).toLocaleString() : 'Never'}
                    </p>
                  </div>
                </div>
                <div className="flex items-center gap-3">
                  <span className={`px-3 py-1 text-sm rounded-full ${
                    instance.sync_status === 'success' ? 'bg-green-100 text-green-800' :
                    instance.sync_status === 'error' ? 'bg-red-100 text-red-800' :
                    'bg-yellow-100 text-yellow-800'
                  }`}>
                    {instance.sync_status}
                  </span>
                  <span className={`w-3 h-3 rounded-full ${instance.is_enabled ? 'bg-green-500' : 'bg-gray-300'}`} />
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Available Connectors */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200">
        <div className="p-6 border-b border-gray-200">
          <h2 className="text-lg font-semibold text-gray-900">Available Connectors</h2>
        </div>
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6 p-6">
          {connectors.length === 0 ? (
            <div className="col-span-3 p-12 text-center">
              <Package className="w-12 h-12 text-gray-400 mx-auto mb-4" />
              <h3 className="text-lg font-medium text-gray-900 mb-2">No connectors available</h3>
              <p className="text-gray-600">Check back soon for new integrations</p>
            </div>
          ) : (
            connectors.map((connector) => (
              <div key={connector.connector_id} className="p-6 border border-gray-200 rounded-lg hover:border-blue-500">
                <div className="flex items-start justify-between mb-3">
                  <div className="p-3 bg-purple-100 rounded-lg">
                    <Package className="w-6 h-6 text-purple-600" />
                  </div>
                  {connector.is_verified && (
                    <CheckCircle className="w-5 h-5 text-green-600" />
                  )}
                </div>
                <h3 className="text-lg font-semibold text-gray-900 mb-2">{connector.connector_name}</h3>
                <p className="text-sm text-gray-600 mb-3">{connector.vendor}</p>
                <div className="flex items-center gap-2 text-xs text-gray-500">
                  <span className="px-2 py-1 bg-blue-100 text-blue-800 rounded-full">
                    {connector.connector_type}
                  </span>
                  <span>{connector.install_count} installs</span>
                  {connector.rating && <span>★ {connector.rating.toFixed(1)}</span>}
                </div>
                <button className="w-full mt-4 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 text-sm font-medium">
                  Install
                </button>
              </div>
            ))
          )}
        </div>
      </div>
    </div>
  );
};

export default IntegrationMarketplace;

