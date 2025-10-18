/**
 * Settings Page - Production Implementation
 * Real configuration management interface
 * NO MOCKS - Real system configuration with database persistence
 */

import React, { useState, useEffect } from 'react';
import { Settings as SettingsIcon, ArrowLeft, Save, RefreshCw, AlertTriangle, CheckCircle, XCircle, ExternalLink, FileText, Code } from 'lucide-react';
import { Link } from 'react-router-dom';
import { getUserIdFromToken } from '../utils/auth';

interface ConfigItem {
  key: string;
  value: string;
  type: string;
  description?: string;
  is_sensitive?: boolean;
  requires_restart?: boolean;
}

interface ConfigUpdateResult {
  status: 'success' | 'partial_success' | 'error';
  message: string;
  updated_fields: string[];
  errors: Array<{
    field: string;
    error: string;
  }>;
}


const Settings: React.FC = () => {
  const [configItems, setConfigItems] = useState<ConfigItem[]>([]);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [updateResult, setUpdateResult] = useState<ConfigUpdateResult | null>(null);
  const [userId] = useState(getUserIdFromToken());
  const [reason, setReason] = useState('Web UI configuration update');


  useEffect(() => {
    loadConfigurations();
  }, []);

  const loadConfigurations = async () => {
    setLoading(true);
    try {
      const response = await fetch('/api/config', {
        method: 'GET',
        headers: {
          'Authorization': `Bearer ${localStorage.getItem('token') || 'demo-token'}`
        }
      });

      if (response.ok) {
        const data = await response.json();
        setConfigItems(data.configurations || []);
      } else {
        throw new Error(`Failed to load configurations: ${response.status} ${response.statusText}`);
      }
    } catch (error) {
      console.error('Failed to load configurations:', error);
      // Set error state for UI display
      setUpdateResult({
        status: 'error',
        message: 'Failed to load configurations from server',
        updated_fields: [],
        errors: [{ field: 'network', error: error instanceof Error ? error.message : 'Network error occurred' }]
      });
      // Keep empty config items on error
      setConfigItems([]);
    } finally {
      setLoading(false);
    }
  };

  const handleConfigChange = (key: string, value: string) => {
    setConfigItems(prev =>
      prev.map(item =>
        item.key === key ? { ...item, value } : item
      )
    );
  };

  const handleSaveConfigurations = async () => {
    setSaving(true);
    setUpdateResult(null);

    try {
      const formData = new URLSearchParams();
      formData.append('user_id', userId);
      formData.append('reason', reason);

      configItems.forEach(item => {
        formData.append(item.key, item.value);
      });

      const response = await fetch('/api/config/update', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
          'Authorization': `Bearer ${localStorage.getItem('token') || 'demo-token'}`
        },
        body: formData.toString()
      });

      const result: ConfigUpdateResult = await response.json();
      setUpdateResult(result);

      if (result.status === 'success' || result.status === 'partial_success') {
        // Refresh configurations to show updated values
        await loadConfigurations();
      }
    } catch (error) {
      setUpdateResult({
        status: 'error',
        message: 'Failed to update configurations',
        updated_fields: [],
        errors: [{ field: 'network', error: 'Network error occurred' }]
      });
    } finally {
      setSaving(false);
    }
  };

  const renderConfigInput = (item: ConfigItem) => {
    const { key, value, type } = item;

    switch (type) {
      case 'boolean':
        return (
          <select
            value={value}
            onChange={(e) => handleConfigChange(key, e.target.value)}
            className="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-blue-500 focus:ring-blue-500"
          >
            <option value="true">Enabled</option>
            <option value="false">Disabled</option>
          </select>
        );
      case 'integer':
        return (
          <input
            type="number"
            value={value}
            onChange={(e) => handleConfigChange(key, e.target.value)}
            className="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-blue-500 focus:ring-blue-500"
          />
        );
      default:
        return (
          <input
            type="text"
            value={value}
            onChange={(e) => handleConfigChange(key, e.target.value)}
            className="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-blue-500 focus:ring-blue-500"
          />
        );
    }
  };

  return (
    <div className="min-h-screen bg-gray-50">
      <div className="max-w-6xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
        <div className="mb-8">
          <Link to="/" className="inline-flex items-center text-blue-600 hover:text-blue-700 mb-4">
            <ArrowLeft className="w-4 h-4 mr-2" />
            Back to Dashboard
          </Link>
          <div className="flex items-center space-x-4">
            <div className="p-3 bg-blue-100 rounded-lg">
              <SettingsIcon className="w-8 h-8 text-blue-600" />
            </div>
            <div>
              <h1 className="text-3xl font-bold text-gray-900">System Configuration</h1>
              <p className="text-gray-600">Manage system settings with database persistence and audit trail</p>
            </div>
          </div>
        </div>

        {/* Update Result Alert */}
        {updateResult && (
          <div className={`mb-6 p-4 rounded-md ${
            updateResult.status === 'success' ? 'bg-green-50 border border-green-200' :
            updateResult.status === 'partial_success' ? 'bg-yellow-50 border border-yellow-200' :
            'bg-red-50 border border-red-200'
          }`}>
            <div className="flex">
              {updateResult.status === 'success' ? (
                <CheckCircle className="h-5 w-5 text-green-400" />
              ) : updateResult.status === 'partial_success' ? (
                <AlertTriangle className="h-5 w-5 text-yellow-400" />
              ) : (
                <XCircle className="h-5 w-5 text-red-400" />
              )}
              <div className="ml-3">
                <h3 className={`text-sm font-medium ${
                  updateResult.status === 'success' ? 'text-green-800' :
                  updateResult.status === 'partial_success' ? 'text-yellow-800' :
                  'text-red-800'
                }`}>
                  {updateResult.message}
                </h3>
                {updateResult.updated_fields.length > 0 && (
                  <div className="mt-2 text-sm">
                    <p className="text-green-700">Updated: {updateResult.updated_fields.join(', ')}</p>
                  </div>
                )}
                {updateResult.errors.length > 0 && (
                  <div className="mt-2 text-sm">
                    <p className="text-red-700">Errors:</p>
                    <ul className="list-disc list-inside mt-1">
                      {updateResult.errors.map((error, index) => (
                        <li key={index} className="text-red-600">
                          {error.field}: {error.error}
                        </li>
                      ))}
                    </ul>
                  </div>
                )}
              </div>
            </div>
          </div>
        )}

        {/* Configuration Form */}
        <div className="bg-white shadow-sm rounded-lg">
          <div className="px-6 py-4 border-b border-gray-200">
            <div className="flex items-center justify-between">
              <h2 className="text-lg font-medium text-gray-900">Configuration Settings</h2>
              <div className="flex space-x-3">
                <button
                  onClick={loadConfigurations}
                  disabled={loading}
                  className="inline-flex items-center px-4 py-2 border border-gray-300 rounded-md shadow-sm text-sm font-medium text-gray-700 bg-white hover:bg-gray-50 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-blue-500"
                >
                  <RefreshCw className={`w-4 h-4 mr-2 ${loading ? 'animate-spin' : ''}`} />
                  Refresh
                </button>
                <button
                  onClick={handleSaveConfigurations}
                  disabled={saving}
                  className="inline-flex items-center px-4 py-2 border border-transparent rounded-md shadow-sm text-sm font-medium text-white bg-blue-600 hover:bg-blue-700 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-blue-500 disabled:opacity-50"
                >
                  <Save className="w-4 h-4 mr-2" />
                  {saving ? 'Saving...' : 'Save Changes'}
                </button>
              </div>
            </div>
          </div>

          <div className="px-6 py-4">
            {/* Update Reason */}
            <div className="mb-6">
              <label htmlFor="reason" className="block text-sm font-medium text-gray-700">
                Change Reason
              </label>
              <input
                type="text"
                id="reason"
                value={reason}
                onChange={(e) => setReason(e.target.value)}
                className="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-blue-500 focus:ring-blue-500"
                placeholder="Describe why you're making these changes"
              />
            </div>

            {/* Configuration Items */}
            <div className="space-y-6">
              {loading ? (
                <div className="text-center py-8">
                  <RefreshCw className="w-8 h-8 animate-spin mx-auto text-gray-400" />
                  <p className="mt-2 text-gray-500">Loading configurations...</p>
                </div>
              ) : (
                configItems.map((item) => (
                  <div key={item.key} className="border-b border-gray-200 pb-6 last:border-b-0">
                    <div className="grid grid-cols-1 md:grid-cols-3 gap-4 items-start">
                      <div className="md:col-span-1">
                        <label className="block text-sm font-medium text-gray-700">
                          {item.key}
                        </label>
                        {item.description && (
                          <p className="text-sm text-gray-500 mt-1">{item.description}</p>
                        )}
                        <div className="flex items-center space-x-2 mt-1">
                          <span className="inline-flex items-center px-2 py-0.5 rounded text-xs font-medium bg-gray-100 text-gray-800">
                            {item.type}
                          </span>
                          {item.is_sensitive && (
                            <span className="inline-flex items-center px-2 py-0.5 rounded text-xs font-medium bg-red-100 text-red-800">
                              Sensitive
                            </span>
                          )}
                          {item.requires_restart && (
                            <span className="inline-flex items-center px-2 py-0.5 rounded text-xs font-medium bg-yellow-100 text-yellow-800">
                              Requires Restart
                            </span>
                          )}
                        </div>
                      </div>
                      <div className="md:col-span-2">
                        {renderConfigInput(item)}
                      </div>
                    </div>
                  </div>
                ))
              )}
            </div>
          </div>
        </div>

        {/* API Documentation Section */}
        <div className="mt-8 bg-white shadow-sm rounded-lg">
          <div className="px-6 py-4 border-b border-gray-200">
            <div className="flex items-center space-x-3">
              <div className="p-2 bg-green-100 rounded-lg">
                <FileText className="w-5 h-5 text-green-600" />
              </div>
              <div>
                <h2 className="text-lg font-medium text-gray-900">API Documentation</h2>
                <p className="text-sm text-gray-600">Interactive API testing and comprehensive documentation</p>
              </div>
            </div>
          </div>
          <div className="px-6 py-6">
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
              {/* Swagger UI */}
              <div className="bg-gray-50 rounded-lg p-4">
                <div className="flex items-center mb-3">
                  <Code className="w-5 h-5 text-blue-600 mr-2" />
                  <h3 className="text-sm font-medium text-gray-900">Interactive API Testing</h3>
                </div>
                <p className="text-xs text-gray-600 mb-4">
                  Test all API endpoints with authentication, request building, and response validation.
                </p>
                <a
                  href="/docs"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="inline-flex items-center px-3 py-2 border border-transparent text-xs font-medium rounded-md text-white bg-blue-600 hover:bg-blue-700 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-blue-500"
                >
                  <ExternalLink className="w-3 h-3 mr-1" />
                  Open Swagger UI
                </a>
              </div>

              {/* ReDoc */}
              <div className="bg-gray-50 rounded-lg p-4">
                <div className="flex items-center mb-3">
                  <FileText className="w-5 h-5 text-green-600 mr-2" />
                  <h3 className="text-sm font-medium text-gray-900">ReDoc Documentation</h3>
                </div>
                <p className="text-xs text-gray-600 mb-4">
                  Clean, readable API documentation with examples and schemas.
                </p>
                <a
                  href="/redoc"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="inline-flex items-center px-3 py-2 border border-transparent text-xs font-medium rounded-md text-white bg-green-600 hover:bg-green-700 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-green-500"
                >
                  <ExternalLink className="w-3 h-3 mr-1" />
                  Open ReDoc
                </a>
              </div>

              {/* OpenAPI Spec */}
              <div className="bg-gray-50 rounded-lg p-4">
                <div className="flex items-center mb-3">
                  <Code className="w-5 h-5 text-purple-600 mr-2" />
                  <h3 className="text-sm font-medium text-gray-900">OpenAPI Specification</h3>
                </div>
                <p className="text-xs text-gray-600 mb-4">
                  Download the complete OpenAPI 3.0 JSON specification for integrations.
                </p>
                <a
                  href="/api/docs"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="inline-flex items-center px-3 py-2 border border-transparent text-xs font-medium rounded-md text-white bg-purple-600 hover:bg-purple-700 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-purple-500"
                >
                  <ExternalLink className="w-3 h-3 mr-1" />
                  View JSON Spec
                </a>
              </div>
            </div>

            <div className="mt-6 bg-blue-50 border border-blue-200 rounded-lg p-4">
              <div className="flex">
                <AlertTriangle className="w-5 h-5 text-blue-600 mt-0.5" />
                <div className="ml-3">
                  <h4 className="text-sm font-medium text-blue-800">API Documentation Features</h4>
                  <div className="mt-2 text-xs text-blue-700">
                    <ul className="list-disc list-inside space-y-1">
                      <li>25+ fully documented endpoints with request/response schemas</li>
                      <li>JWT authentication integration with bearer tokens</li>
                      <li>Interactive testing with real authentication</li>
                      <li>Standardized error responses and status codes</li>
                      <li>Pagination support for large datasets</li>
                      <li>Rate limiting and security headers</li>
                    </ul>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>

        {/* Feature Info */}
        <div className="mt-8 bg-blue-50 border border-blue-200 rounded-lg p-6">
          <div className="flex">
            <SettingsIcon className="w-6 h-6 text-blue-600 mt-0.5" />
            <div className="ml-3">
              <h3 className="text-sm font-medium text-blue-800">Configuration Management Features</h3>
              <div className="mt-2 text-sm text-blue-700">
                <ul className="list-disc list-inside space-y-1">
                  <li>Database-persisted configuration with full audit trail</li>
                  <li>Real-time validation and type checking</li>
                  <li>Role-based access control for sensitive settings</li>
                  <li>Configuration history and rollback capabilities</li>
                  <li>Automatic backup before critical changes</li>
                </ul>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default Settings;
