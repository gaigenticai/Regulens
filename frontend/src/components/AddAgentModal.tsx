/**
 * Add Agent Modal - Production Implementation
 * Modal for creating new agents with proper validation
 * NO MOCKS - Real API integration with C++ backend
 */

import React, { useState } from 'react';
import { X, AlertCircle } from 'lucide-react';

interface AddAgentModalProps {
  isOpen: boolean;
  onClose: () => void;
  onAgentCreated: () => void;
}

type AgentType = 'transaction_guardian' | 'audit_intelligence' | 'regulatory_assessor' | 'compliance' | 'data_validation' | 'risk_analysis';

const agentTypeOptions: { value: AgentType; label: string; description: string }[] = [
  {
    value: 'transaction_guardian',
    label: 'Transaction Guardian',
    description: 'Monitors transactions for fraud detection and risk assessment',
  },
  {
    value: 'audit_intelligence',
    label: 'Audit Intelligence',
    description: 'Analyzes audit logs and compliance data for anomalies',
  },
  {
    value: 'regulatory_assessor',
    label: 'Regulatory Assessor',
    description: 'Assesses regulatory changes and their impact on operations',
  },
  {
    value: 'compliance',
    label: 'Compliance Agent',
    description: 'Ensures compliance with regulations and policies',
  },
  {
    value: 'data_validation',
    label: 'Data Validation Agent',
    description: 'Validates data quality and integrity',
  },
  {
    value: 'risk_analysis',
    label: 'Risk Analysis Agent',
    description: 'Analyzes and assesses various risk factors',
  },
];

export const AddAgentModal: React.FC<AddAgentModalProps> = ({ isOpen, onClose, onAgentCreated }) => {
  const [agentName, setAgentName] = useState('');
  const [agentType, setAgentType] = useState<AgentType>('transaction_guardian');
  const [isActive, setIsActive] = useState(true);
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [error, setError] = useState<string | null>(null);
  
  // Configuration fields
  const [region, setRegion] = useState<string>('US');
  const [fraudThreshold, setFraudThreshold] = useState<number>(0.75);
  const [riskThreshold, setRiskThreshold] = useState<number>(0.70);
  const [regulatorySources, setRegulatorySources] = useState<string[]>(['SEC', 'FINRA']);
  const [alertEmail, setAlertEmail] = useState<string>('');
  const [highPriorityOnly, setHighPriorityOnly] = useState<boolean>(false);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    
    // Validation
    if (!agentName.trim()) {
      setError('Agent name is required');
      return;
    }
    
    if (agentName.length < 3) {
      setError('Agent name must be at least 3 characters');
      return;
    }
    
    if (!/^[a-zA-Z0-9_-]+$/.test(agentName)) {
      setError('Agent name can only contain letters, numbers, hyphens, and underscores');
      return;
    }
    
    try {
      setIsSubmitting(true);
      setError(null);
      
      // Build configuration based on agent type
      const configuration: Record<string, any> = {
        version: '1.0',
        enabled: true,
        region: region,
        created_via: 'ui',
      };
      
      // Add agent-specific configuration
      if (agentType === 'transaction_guardian') {
        configuration.fraud_threshold = fraudThreshold;
        configuration.risk_threshold = riskThreshold;
        configuration.alert_email = alertEmail || null;
        configuration.high_priority_only = highPriorityOnly;
      } else if (agentType === 'regulatory_assessor') {
        configuration.regulatory_sources = regulatorySources;
        configuration.alert_email = alertEmail || null;
        configuration.auto_assess_impact = true;
      } else if (agentType === 'audit_intelligence') {
        configuration.anomaly_threshold = riskThreshold;
        configuration.alert_email = alertEmail || null;
      }
      
      // Call backend API to create agent
      const response = await fetch('/api/agents', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        credentials: 'include',
        body: JSON.stringify({
          agent_name: agentName,
          agent_type: agentType,
          is_active: isActive,
          configuration: configuration,
        }),
      });
      
      if (!response.ok) {
        const errorData = await response.json().catch(() => ({ error: 'Failed to create agent' }));
        throw new Error(errorData.error || errorData.message || 'Failed to create agent');
      }
      
      // Success - close modal and refresh agent list
      onAgentCreated();
      handleClose();
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to create agent');
    } finally {
      setIsSubmitting(false);
    }
  };
  
  const handleClose = () => {
    setAgentName('');
    setAgentType('transaction_guardian');
    setIsActive(true);
    setError(null);
    onClose();
  };
  
  if (!isOpen) return null;
  
  const selectedType = agentTypeOptions.find(opt => opt.value === agentType);
  
  return (
    <div className="fixed inset-0 z-50 overflow-y-auto">
      <div className="flex items-center justify-center min-h-screen px-4 pt-4 pb-20 text-center sm:block sm:p-0">
        {/* Background overlay */}
        <div 
          className="fixed inset-0 transition-opacity bg-gray-500 bg-opacity-75" 
          onClick={handleClose}
        />
        
        {/* Modal panel */}
        <div className="inline-block align-bottom bg-white rounded-lg text-left overflow-hidden shadow-xl transform transition-all sm:my-8 sm:align-middle sm:max-w-2xl sm:w-full max-h-[90vh] overflow-y-auto">
          <form onSubmit={handleSubmit}>
            {/* Header */}
            <div className="bg-white px-4 pt-5 pb-4 sm:p-6 sm:pb-4">
              <div className="flex items-center justify-between mb-4">
                <h3 className="text-lg font-medium leading-6 text-gray-900">
                  Add New Agent
                </h3>
                <button
                  type="button"
                  onClick={handleClose}
                  className="text-gray-400 hover:text-gray-500"
                >
                  <X className="w-6 h-6" />
                </button>
              </div>
              
              {/* Error message */}
              {error && (
                <div className="mb-4 bg-red-50 border border-red-200 rounded-lg p-3">
                  <div className="flex items-center">
                    <AlertCircle className="w-5 h-5 text-red-500 mr-2" />
                    <span className="text-sm text-red-700">{error}</span>
                  </div>
                </div>
              )}
              
              {/* Form fields */}
              <div className="space-y-4">
                {/* Agent Name */}
                <div>
                  <label htmlFor="agentName" className="block text-sm font-medium text-gray-700 mb-1">
                    Agent Name <span className="text-red-500">*</span>
                  </label>
                  <input
                    type="text"
                    id="agentName"
                    value={agentName}
                    onChange={(e) => setAgentName(e.target.value)}
                    className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                    disabled={isSubmitting}
                  />
                  <p className="mt-1 text-xs text-gray-500">
                    Use lowercase letters, numbers, hyphens, or underscores only
                  </p>
                </div>
                
                {/* Agent Type */}
                <div>
                  <label htmlFor="agentType" className="block text-sm font-medium text-gray-700 mb-1">
                    Agent Type <span className="text-red-500">*</span>
                  </label>
                  <select
                    id="agentType"
                    value={agentType}
                    onChange={(e) => setAgentType(e.target.value as AgentType)}
                    className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                    disabled={isSubmitting}
                  >
                    {agentTypeOptions.map((option) => (
                      <option key={option.value} value={option.value}>
                        {option.label}
                      </option>
                    ))}
                  </select>
                  {selectedType && (
                    <p className="mt-1 text-xs text-gray-500">
                      {selectedType.description}
                    </p>
                  )}
                </div>
                
                {/* Configuration Section */}
                <div className="border-t pt-4 mt-4">
                  <h4 className="text-sm font-medium text-gray-900 mb-3">Agent Configuration</h4>
                  
                  {/* Geographic Region */}
                  <div className="mb-3">
                    <label htmlFor="region" className="block text-sm font-medium text-gray-700 mb-1">
                      Geographic Region
                    </label>
                    <select
                      id="region"
                      value={region}
                      onChange={(e) => setRegion(e.target.value)}
                      className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500"
                      disabled={isSubmitting}
                    >
                      <option value="US">United States</option>
                      <option value="EU">European Union</option>
                      <option value="UK">United Kingdom</option>
                      <option value="APAC">Asia Pacific</option>
                      <option value="GLOBAL">Global</option>
                    </select>
                    <p className="mt-1 text-xs text-gray-500">
                      Determines applicable regulations and thresholds
                    </p>
                  </div>
                  
                  {/* Transaction Guardian specific configs */}
                  {agentType === 'transaction_guardian' && (
                    <>
                      <div className="mb-3">
                        <label htmlFor="fraudThreshold" className="block text-sm font-medium text-gray-700 mb-1">
                          Fraud Detection Threshold
                        </label>
                        <input
                          type="number"
                          id="fraudThreshold"
                          value={fraudThreshold}
                          onChange={(e) => setFraudThreshold(parseFloat(e.target.value))}
                          min="0"
                          max="1"
                          step="0.05"
                          className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500"
                          disabled={isSubmitting}
                        />
                        <p className="mt-1 text-xs text-gray-500">
                          Confidence level (0.0-1.0) required to flag as fraud. Current: {(fraudThreshold * 100).toFixed(0)}%
                        </p>
                      </div>
                      
                      <div className="mb-3">
                        <label htmlFor="riskThreshold" className="block text-sm font-medium text-gray-700 mb-1">
                          Risk Assessment Threshold
                        </label>
                        <input
                          type="number"
                          id="riskThreshold"
                          value={riskThreshold}
                          onChange={(e) => setRiskThreshold(parseFloat(e.target.value))}
                          min="0"
                          max="1"
                          step="0.05"
                          className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500"
                          disabled={isSubmitting}
                        />
                        <p className="mt-1 text-xs text-gray-500">
                          Risk level (0.0-1.0) requiring manual review. Current: {(riskThreshold * 100).toFixed(0)}%
                        </p>
                      </div>
                      
                      <div className="mb-3 flex items-center">
                        <input
                          type="checkbox"
                          id="highPriorityOnly"
                          checked={highPriorityOnly}
                          onChange={(e) => setHighPriorityOnly(e.target.checked)}
                          className="h-4 w-4 text-blue-600 focus:ring-blue-500 border-gray-300 rounded"
                          disabled={isSubmitting}
                        />
                        <label htmlFor="highPriorityOnly" className="ml-2 block text-sm text-gray-700">
                          Only alert on high-priority transactions
                        </label>
                      </div>
                    </>
                  )}
                  
                  {/* Regulatory Assessor specific configs */}
                  {agentType === 'regulatory_assessor' && (
                    <div className="mb-3">
                      <label className="block text-sm font-medium text-gray-700 mb-1">
                        Regulatory Sources to Monitor
                      </label>
                      <div className="space-y-2">
                        {['SEC', 'FINRA', 'FCA', 'ESMA', 'MAS', 'CFTC'].map((source) => (
                          <label key={source} className="flex items-center">
                            <input
                              type="checkbox"
                              checked={regulatorySources.includes(source)}
                              onChange={(e) => {
                                if (e.target.checked) {
                                  setRegulatorySources([...regulatorySources, source]);
                                } else {
                                  setRegulatorySources(regulatorySources.filter((s) => s !== source));
                                }
                              }}
                              className="h-4 w-4 text-blue-600 focus:ring-blue-500 border-gray-300 rounded"
                              disabled={isSubmitting}
                            />
                            <span className="ml-2 text-sm text-gray-700">{source}</span>
                          </label>
                        ))}
                      </div>
                      <p className="mt-1 text-xs text-gray-500">
                        Agent will monitor changes from selected regulatory bodies
                      </p>
                    </div>
                  )}
                  
                  {/* Audit Intelligence specific configs */}
                  {agentType === 'audit_intelligence' && (
                    <div className="mb-3">
                      <label htmlFor="anomalyThreshold" className="block text-sm font-medium text-gray-700 mb-1">
                        Anomaly Detection Threshold
                      </label>
                      <input
                        type="number"
                        id="anomalyThreshold"
                        value={riskThreshold}
                        onChange={(e) => setRiskThreshold(parseFloat(e.target.value))}
                        min="0"
                        max="1"
                        step="0.05"
                        className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500"
                        disabled={isSubmitting}
                      />
                      <p className="mt-1 text-xs text-gray-500">
                        Confidence level for anomaly detection. Current: {(riskThreshold * 100).toFixed(0)}%
                      </p>
                    </div>
                  )}
                  
                  {/* Alert Email (for all types) */}
                  <div className="mb-3">
                    <label htmlFor="alertEmail" className="block text-sm font-medium text-gray-700 mb-1">
                      Alert Email (Optional)
                    </label>
                    <input
                      type="email"
                      id="alertEmail"
                      value={alertEmail}
                      onChange={(e) => setAlertEmail(e.target.value)}
                      className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500"
                      disabled={isSubmitting}
                    />
                    <p className="mt-1 text-xs text-gray-500">
                      Receive email notifications for critical alerts
                    </p>
                  </div>
                </div>
                
                {/* Active Status */}
                <div className="flex items-center border-t pt-4">
                  <input
                    type="checkbox"
                    id="isActive"
                    checked={isActive}
                    onChange={(e) => setIsActive(e.target.checked)}
                    className="h-4 w-4 text-blue-600 focus:ring-blue-500 border-gray-300 rounded"
                    disabled={isSubmitting}
                  />
                  <label htmlFor="isActive" className="ml-2 block text-sm text-gray-700">
                    Start agent immediately after creation
                  </label>
                </div>
              </div>
            </div>
            
            {/* Footer */}
            <div className="bg-gray-50 px-4 py-3 sm:px-6 sm:flex sm:flex-row-reverse">
              <button
                type="submit"
                disabled={isSubmitting}
                className="w-full inline-flex justify-center rounded-lg border border-transparent shadow-sm px-4 py-2 bg-blue-600 text-base font-medium text-white hover:bg-blue-700 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-blue-500 sm:ml-3 sm:w-auto sm:text-sm disabled:opacity-50 disabled:cursor-not-allowed"
              >
                {isSubmitting ? 'Creating...' : 'Create Agent'}
              </button>
              <button
                type="button"
                onClick={handleClose}
                disabled={isSubmitting}
                className="mt-3 w-full inline-flex justify-center rounded-lg border border-gray-300 shadow-sm px-4 py-2 bg-white text-base font-medium text-gray-700 hover:bg-gray-50 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-blue-500 sm:mt-0 sm:ml-3 sm:w-auto sm:text-sm disabled:opacity-50"
              >
                Cancel
              </button>
            </div>
          </form>
        </div>
      </div>
    </div>
  );
};

export default AddAgentModal;

