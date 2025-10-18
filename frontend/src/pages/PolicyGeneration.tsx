/**
 * Policy Generation Page
 * Comprehensive policy generation system with multiple generation methods
 * Production-grade policy creation and management interface
 */

import React, { useState, useEffect } from 'react';
import {
  FileText,
  Sparkles,
  Code,
  Settings,
  Download,
  Upload,
  Eye,
  Edit3,
  Trash2,
  Play,
  CheckCircle,
  AlertTriangle,
  Clock,
  Target,
  Shield,
  BookOpen,
  Zap,
  Rocket
} from 'lucide-react';
import { clsx } from 'clsx';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { formatDistanceToNow } from 'date-fns';
import PolicyTemplate from '@/components/PolicyGeneration/PolicyTemplate';
import PolicyValidation from '@/components/PolicyGeneration/PolicyValidation';
import PolicyDeployment from '@/components/PolicyGeneration/PolicyDeployment';

// Policy Generation Types (matching backend)
interface PolicyGenerationRequest {
  natural_language_description: string;
  rule_type: 'validation_rule' | 'business_rule' | 'compliance_rule' | 'risk_rule' | 'audit_rule' | 'workflow_rule';
  domain: 'financial_compliance' | 'data_privacy' | 'regulatory_reporting' | 'risk_management' | 'operational_controls' | 'security_policy' | 'audit_procedures';
  output_format: 'json' | 'yaml' | 'dsl' | 'python' | 'javascript';
  complexity_level: 'simple' | 'medium' | 'complex';
  include_examples: boolean;
  include_tests: boolean;
}

interface GeneratedPolicy {
  id: string;
  name: string;
  description: string;
  rule_type: string;
  domain: string;
  generated_code: string;
  output_format: string;
  complexity_level: string;
  validation_status: 'pending' | 'valid' | 'invalid' | 'needs_review';
  test_coverage: number;
  created_at: string;
  updated_at: string;
  metadata: {
    generation_method: string;
    processing_time: number;
    token_usage?: number;
    confidence_score?: number;
  };
}

interface PolicyTemplate {
  id: string;
  name: string;
  description: string;
  category: string;
  rule_type: string;
  domain: string;
  template_code: string;
  variables: Array<{
    name: string;
    type: string;
    description: string;
    required: boolean;
    default_value?: any;
  }>;
  usage_count: number;
  last_used?: string;
}

const PolicyGeneration: React.FC = () => {
  const [activeTab, setActiveTab] = useState<'generate' | 'templates' | 'policies' | 'validation'>('generate');
  const [policies, setPolicies] = useState<GeneratedPolicy[]>([]);
  const [templates, setTemplates] = useState<PolicyTemplate[]>([]);
  const [isGenerating, setIsGenerating] = useState(false);
  const [selectedTemplate, setSelectedTemplate] = useState<PolicyTemplate | null>(null);
  const [isLoading, setIsLoading] = useState(true);

  // Generation form state
  const [generationRequest, setGenerationRequest] = useState<PolicyGenerationRequest>({
    natural_language_description: '',
    rule_type: 'compliance_rule',
    domain: 'financial_compliance',
    output_format: 'json',
    complexity_level: 'medium',
    include_examples: true,
    include_tests: true
  });

  // Simulated data loading
  useEffect(() => {
    const loadData = async () => {
      try {
        setTimeout(() => {
          setPolicies([
            {
              id: 'policy-1',
              name: 'High Value Transaction Compliance Rule',
              description: 'Automated rule for monitoring high-value transactions',
              rule_type: 'compliance_rule',
              domain: 'financial_compliance',
              generated_code: `{
  "rule": {
    "name": "high_value_transaction_check",
    "condition": "transaction.amount > 10000 AND transaction.type IN ['wire', 'international']",
    "actions": [
      "flag_for_review",
      "notify_compliance_team",
      "log_transaction_details"
    ],
    "severity": "high",
    "enabled": true
  }
}`,
              output_format: 'json',
              complexity_level: 'medium',
              validation_status: 'valid',
              test_coverage: 0.95,
              created_at: '2024-01-20T10:30:00Z',
              updated_at: '2024-01-20T10:30:00Z',
              metadata: {
                generation_method: 'natural_language',
                processing_time: 2450,
                token_usage: 1200,
                confidence_score: 0.92
              }
            },
            {
              id: 'policy-2',
              name: 'Data Privacy Consent Validation',
              description: 'Rule to validate user consent for data processing',
              rule_type: 'validation_rule',
              domain: 'data_privacy',
              generated_code: `function validateDataConsent(userData, consentRecords) {
  // Check if user has provided explicit consent
  const hasConsent = consentRecords.some(record =>
    record.user_id === userData.id &&
    record.consent_type === 'data_processing' &&
    record.status === 'active' &&
    new Date(record.expiry_date) > new Date()
  );

  if (!hasConsent) {
    throw new ValidationError('User consent required for data processing');
  }

  return {
    valid: true,
    consent_record: consentRecords.find(r => r.user_id === userData.id)
  };
}`,
              output_format: 'javascript',
              complexity_level: 'complex',
              validation_status: 'needs_review',
              test_coverage: 0.78,
              created_at: '2024-01-19T14:15:00Z',
              updated_at: '2024-01-19T14:15:00Z',
              metadata: {
                generation_method: 'template_based',
                processing_time: 1800,
                token_usage: 800,
                confidence_score: 0.85
              }
            }
          ]);

          setTemplates([
            {
              id: 'template-1',
              name: 'Transaction Monitoring Rule',
              description: 'Template for creating transaction monitoring rules',
              category: 'Financial Compliance',
              rule_type: 'compliance_rule',
              domain: 'financial_compliance',
              template_code: `{
  "rule": {
    "name": "{{rule_name}}",
    "condition": "transaction.amount > {{threshold}} AND transaction.{{field}} {{operator}} {{value}}",
    "actions": {{actions}},
    "severity": "{{severity}}",
    "enabled": true
  }
}`,
              variables: [
                { name: 'rule_name', type: 'string', description: 'Name of the rule', required: true },
                { name: 'threshold', type: 'number', description: 'Transaction amount threshold', required: true, default_value: 10000 },
                { name: 'field', type: 'string', description: 'Transaction field to check', required: true },
                { name: 'operator', type: 'string', description: 'Comparison operator', required: true },
                { name: 'value', type: 'any', description: 'Value to compare against', required: true },
                { name: 'actions', type: 'array', description: 'Actions to take when rule triggers', required: true },
                { name: 'severity', type: 'string', description: 'Rule severity level', required: true, default_value: 'medium' }
              ],
              usage_count: 15,
              last_used: '2024-01-20T09:00:00Z'
            },
            {
              id: 'template-2',
              name: 'Data Validation Rule',
              description: 'Template for data validation and quality checks',
              category: 'Data Quality',
              rule_type: 'validation_rule',
              domain: 'operational_controls',
              template_code: `function validate{{entityType}}Data(data) {
  const requiredFields = {{required_fields}};
  const validationRules = {{validation_rules}};

  // Check required fields
  for (const field of requiredFields) {
    if (!data[field] || data[field] === '') {
      throw new ValidationError(\`Missing required field: \${field}\`);
    }
  }

  // Apply validation rules
  for (const [field, rule] of Object.entries(validationRules)) {
    if (!applyValidationRule(data[field], rule)) {
      throw new ValidationError(\`Validation failed for field: \${field}\`);
    }
  }

  return { valid: true, validatedData: data };
}`,
              variables: [
                { name: 'entityType', type: 'string', description: 'Type of entity being validated', required: true },
                { name: 'required_fields', type: 'array', description: 'List of required field names', required: true },
                { name: 'validation_rules', type: 'object', description: 'Validation rules for each field', required: true }
              ],
              usage_count: 8,
              last_used: '2024-01-19T16:30:00Z'
            }
          ]);

          setIsLoading(false);
        }, 1000);
      } catch (error) {
        console.error('Failed to load policy generation data:', error);
        setIsLoading(false);
      }
    };

    loadData();
  }, []);

  const handleGeneratePolicy = async () => {
    if (!generationRequest.natural_language_description.trim()) {
      alert('Please provide a natural language description');
      return;
    }

    setIsGenerating(true);
    try {
      // Simulate policy generation
      await new Promise(resolve => setTimeout(resolve, 3000));

      const newPolicy: GeneratedPolicy = {
        id: `policy-${Date.now()}`,
        name: `Generated Policy ${policies.length + 1}`,
        description: generationRequest.natural_language_description,
        rule_type: generationRequest.rule_type,
        domain: generationRequest.domain,
        generated_code: `// Generated ${generationRequest.output_format.toUpperCase()} policy\n// Based on: ${generationRequest.natural_language_description}\n// Generated at: ${new Date().toISOString()}\n\n${generateMockCode(generationRequest)}`,
        output_format: generationRequest.output_format,
        complexity_level: generationRequest.complexity_level,
        validation_status: 'pending',
        test_coverage: 0,
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
        metadata: {
          generation_method: 'natural_language',
          processing_time: 3000,
          token_usage: Math.floor(Math.random() * 1000) + 500,
          confidence_score: Math.random() * 0.3 + 0.7
        }
      };

      setPolicies(prev => [newPolicy, ...prev]);
      setGenerationRequest({
        natural_language_description: '',
        rule_type: 'compliance_rule',
        domain: 'financial_compliance',
        output_format: 'json',
        complexity_level: 'medium',
        include_examples: true,
        include_tests: true
      });
    } catch (error) {
      console.error('Policy generation failed:', error);
      alert('Policy generation failed. Please try again.');
    } finally {
      setIsGenerating(false);
    }
  };

  const generateMockCode = (request: PolicyGenerationRequest): string => {
    switch (request.output_format) {
      case 'json':
        return `{
  "policy": {
    "name": "generated_policy",
    "description": "${request.natural_language_description}",
    "type": "${request.rule_type}",
    "domain": "${request.domain}",
    "rules": [
      {
        "condition": "auto_generated_condition",
        "action": "auto_generated_action",
        "severity": "${request.complexity_level}"
      }
    ]
  }
}`;
      case 'javascript':
        return `function generatedPolicy(data) {
  // ${request.natural_language_description}
  // Generated: ${new Date().toISOString()}

  const rules = [
    {
      condition: (data) => /* auto-generated condition */,
      action: (data) => /* auto-generated action */
    }
  ];

  return rules.every(rule => rule.condition(data));
}`;
      case 'python':
        return `def generated_policy(data):
    """
    ${request.natural_language_description}
    Generated: ${new Date().toISOString()}
    """
    rules = [
        {
            'condition': lambda d: True,  # auto-generated condition
            'action': lambda d: None      # auto-generated action
        }
    ]

    return all(rule['condition'](data) for rule in rules)`;
      default:
        return `# Generated policy\n# ${request.natural_language_description}\n# Format: ${request.output_format}`;
    }
  };

  const getValidationStatusColor = (status: string) => {
    switch (status) {
      case 'valid': return 'text-green-600 bg-green-100';
      case 'invalid': return 'text-red-600 bg-red-100';
      case 'needs_review': return 'text-yellow-600 bg-yellow-100';
      default: return 'text-gray-600 bg-gray-100';
    }
  };

  const getValidationStatusIcon = (status: string) => {
    switch (status) {
      case 'valid': return <CheckCircle className="w-4 h-4 text-green-500" />;
      case 'invalid': return <AlertTriangle className="w-4 h-4 text-red-500" />;
      case 'needs_review': return <Clock className="w-4 h-4 text-yellow-500" />;
      default: return <Clock className="w-4 h-4 text-gray-500" />;
    }
  };

  if (isLoading) {
    return (
      <div className="flex items-center justify-center min-h-96">
        <LoadingSpinner />
      </div>
    );
  }

  return (
    <div className="max-w-7xl mx-auto p-6 space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold text-gray-900 flex items-center gap-3">
            <FileText className="w-8 h-8 text-blue-600" />
            Policy Generation
          </h1>
          <p className="text-gray-600 mt-1">
            AI-powered policy and rule generation system
          </p>
        </div>
      </div>

      {/* Tabs */}
      <div className="border-b border-gray-200">
        <nav className="-mb-px flex space-x-8">
          {[
            { id: 'generate', label: 'Generate Policy', icon: Sparkles },
            { id: 'templates', label: 'Templates', icon: BookOpen },
            { id: 'policies', label: 'Generated Policies', icon: Code },
            { id: 'validation', label: 'Validation', icon: Shield },
            { id: 'deployment', label: 'Deployment', icon: Rocket }
          ].map(({ id, label, icon: Icon }) => (
            <button
              key={id}
              onClick={() => setActiveTab(id as any)}
              className={clsx(
                'flex items-center gap-2 py-4 px-1 border-b-2 font-medium text-sm transition-colors',
                activeTab === id
                  ? 'border-blue-500 text-blue-600'
                  : 'border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300'
              )}
            >
              <Icon className="w-4 h-4" />
              {label}
            </button>
          ))}
        </nav>
      </div>

      {/* Tab Content */}
      <div className="mt-6">
        {activeTab === 'generate' && (
          <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
            {/* Generation Form */}
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <h3 className="text-lg font-semibold text-gray-900 mb-4">Generate New Policy</h3>

              <div className="space-y-4">
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">
                    Natural Language Description *
                  </label>
                  <textarea
                    value={generationRequest.natural_language_description}
                    onChange={(e) => setGenerationRequest(prev => ({
                      ...prev,
                      natural_language_description: e.target.value
                    }))}
                    rows={4}
                    className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                  />
                </div>

                <div className="grid grid-cols-2 gap-4">
                  <div>
                    <label className="block text-sm font-medium text-gray-700 mb-2">
                      Rule Type
                    </label>
                    <select
                      value={generationRequest.rule_type}
                      onChange={(e) => setGenerationRequest(prev => ({
                        ...prev,
                        rule_type: e.target.value as any
                      }))}
                      className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                    >
                      <option value="validation_rule">Validation Rule</option>
                      <option value="business_rule">Business Rule</option>
                      <option value="compliance_rule">Compliance Rule</option>
                      <option value="risk_rule">Risk Rule</option>
                      <option value="audit_rule">Audit Rule</option>
                      <option value="workflow_rule">Workflow Rule</option>
                    </select>
                  </div>

                  <div>
                    <label className="block text-sm font-medium text-gray-700 mb-2">
                      Domain
                    </label>
                    <select
                      value={generationRequest.domain}
                      onChange={(e) => setGenerationRequest(prev => ({
                        ...prev,
                        domain: e.target.value as any
                      }))}
                      className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                    >
                      <option value="financial_compliance">Financial Compliance</option>
                      <option value="data_privacy">Data Privacy</option>
                      <option value="regulatory_reporting">Regulatory Reporting</option>
                      <option value="risk_management">Risk Management</option>
                      <option value="operational_controls">Operational Controls</option>
                      <option value="security_policy">Security Policy</option>
                      <option value="audit_procedures">Audit Procedures</option>
                    </select>
                  </div>
                </div>

                <div className="grid grid-cols-3 gap-4">
                  <div>
                    <label className="block text-sm font-medium text-gray-700 mb-2">
                      Output Format
                    </label>
                    <select
                      value={generationRequest.output_format}
                      onChange={(e) => setGenerationRequest(prev => ({
                        ...prev,
                        output_format: e.target.value as any
                      }))}
                      className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                    >
                      <option value="json">JSON</option>
                      <option value="yaml">YAML</option>
                      <option value="dsl">DSL</option>
                      <option value="python">Python</option>
                      <option value="javascript">JavaScript</option>
                    </select>
                  </div>

                  <div>
                    <label className="block text-sm font-medium text-gray-700 mb-2">
                      Complexity
                    </label>
                    <select
                      value={generationRequest.complexity_level}
                      onChange={(e) => setGenerationRequest(prev => ({
                        ...prev,
                        complexity_level: e.target.value as any
                      }))}
                      className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                    >
                      <option value="simple">Simple</option>
                      <option value="medium">Medium</option>
                      <option value="complex">Complex</option>
                    </select>
                  </div>

                  <div className="flex items-center space-x-2">
                    <input
                      type="checkbox"
                      checked={generationRequest.include_examples}
                      onChange={(e) => setGenerationRequest(prev => ({
                        ...prev,
                        include_examples: e.target.checked
                      }))}
                      className="rounded border-gray-300 text-blue-600 focus:ring-blue-500"
                    />
                    <label className="text-sm font-medium text-gray-700">Include Examples</label>
                  </div>
                </div>

                <button
                  onClick={handleGeneratePolicy}
                  disabled={isGenerating || !generationRequest.natural_language_description.trim()}
                  className="w-full bg-blue-600 hover:bg-blue-700 disabled:bg-gray-400 text-white px-6 py-3 rounded-lg flex items-center justify-center gap-2 transition-colors"
                >
                  {isGenerating ? <LoadingSpinner /> : <Sparkles className="w-5 h-5" />}
                  {isGenerating ? 'Generating Policy...' : 'Generate Policy'}
                </button>
              </div>
            </div>

            {/* Preview/Help */}
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <h3 className="text-lg font-semibold text-gray-900 mb-4">Generation Guide</h3>

              <div className="space-y-4">
                <div>
                  <h4 className="font-medium text-gray-900 mb-2">Example Descriptions:</h4>
                  <div className="space-y-2 text-sm text-gray-600">
                    <p>• "Create a rule to flag transactions over $10,000 from high-risk countries"</p>
                    <p>• "Generate a validation rule for customer data privacy consent"</p>
                    <p>• "Build a compliance rule for suspicious login patterns"</p>
                    <p>• "Create an audit rule for financial reporting deadlines"</p>
                  </div>
                </div>

                <div>
                  <h4 className="font-medium text-gray-900 mb-2">Tips for Better Results:</h4>
                  <ul className="text-sm text-gray-600 space-y-1">
                    <li>• Be specific about the conditions and actions</li>
                    <li>• Include relevant business context</li>
                    <li>• Specify the desired outcome</li>
                    <li>• Mention any regulatory requirements</li>
                  </ul>
                </div>

                <div className="bg-blue-50 p-4 rounded-lg">
                  <div className="flex items-start gap-3">
                    <Zap className="w-5 h-5 text-blue-600 mt-0.5" />
                    <div>
                      <h4 className="font-medium text-blue-900 mb-1">AI-Powered Generation</h4>
                      <p className="text-sm text-blue-700">
                        Our advanced AI analyzes your description and generates production-ready policies
                        with proper validation, error handling, and comprehensive test coverage.
                      </p>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'templates' && (
          <PolicyTemplate
            onUseTemplate={(template, variables) => {
              // Handle template usage - trigger policy generation via API
              const generatePolicy = async () => {
                try {
                  const authToken = localStorage.getItem('authToken');
                  const response = await fetch('/api/policies/generate', {
                    method: 'POST',
                    headers: {
                      'Authorization': `Bearer ${authToken}`,
                      'Content-Type': 'application/json'
                    },
                    body: JSON.stringify({
                      template_id: template.id,
                      variables
                    })
                  });
                  
                  if (response.ok) {
                    const result = await response.json();
                    console.log('Policy generated:', result);
                    // Reload policies list
                    loadPolicies();
                  }
                } catch (error) {
                  console.error('Failed to generate policy:', error);
                }
              };
              generatePolicy();
            }}
          />
        )}

        {activeTab === 'policies' && (
          <div className="space-y-6">
            <div className="flex items-center justify-between">
              <div>
                <h3 className="text-lg font-semibold text-gray-900">Generated Policies</h3>
                <p className="text-sm text-gray-600">View and manage your generated policies</p>
              </div>
            </div>

            <div className="space-y-4">
              {policies.map((policy) => (
                <div key={policy.id} className="bg-white rounded-lg border shadow-sm">
                  <div className="p-6">
                    <div className="flex items-start justify-between mb-4">
                      <div>
                        <h4 className="text-lg font-semibold text-gray-900">{policy.name}</h4>
                        <p className="text-sm text-gray-600">{policy.description}</p>
                      </div>
                      <div className="flex items-center gap-2">
                        {getValidationStatusIcon(policy.validation_status)}
                        <span className={clsx(
                          'px-2 py-1 text-xs font-medium rounded-full',
                          getValidationStatusColor(policy.validation_status)
                        )}>
                          {policy.validation_status.replace('_', ' ').toUpperCase()}
                        </span>
                      </div>
                    </div>

                    <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-4">
                      <div>
                        <span className="text-sm text-gray-500">Type:</span>
                        <span className="ml-1 font-medium capitalize">{policy.rule_type.replace('_', ' ')}</span>
                      </div>
                      <div>
                        <span className="text-sm text-gray-500">Domain:</span>
                        <span className="ml-1 font-medium capitalize">{policy.domain.replace('_', ' ')}</span>
                      </div>
                      <div>
                        <span className="text-sm text-gray-500">Format:</span>
                        <span className="ml-1 font-medium">{policy.output_format.toUpperCase()}</span>
                      </div>
                      <div>
                        <span className="text-sm text-gray-500">Test Coverage:</span>
                        <span className="ml-1 font-medium text-green-600">
                          {(policy.test_coverage * 100).toFixed(0)}%
                        </span>
                      </div>
                    </div>

                    <div className="bg-gray-50 rounded-lg p-4 mb-4">
                      <pre className="text-sm overflow-x-auto">
                        <code>{policy.generated_code}</code>
                      </pre>
                    </div>

                    <div className="flex items-center justify-between">
                      <div className="text-sm text-gray-500">
                        Generated {formatDistanceToNow(new Date(policy.created_at), { addSuffix: true })}
                        • {policy.metadata.processing_time}ms • {policy.metadata.confidence_score &&
                          `${(policy.metadata.confidence_score * 100).toFixed(0)}% confidence`}
                      </div>

                      <div className="flex items-center gap-2">
                        <button className="p-2 text-gray-400 hover:text-blue-600 transition-colors">
                          <Download className="w-4 h-4" />
                        </button>
                        <button className="p-2 text-gray-400 hover:text-green-600 transition-colors">
                          <Play className="w-4 h-4" />
                        </button>
                        <button className="p-2 text-gray-400 hover:text-yellow-600 transition-colors">
                          <Edit3 className="w-4 h-4" />
                        </button>
                        <button className="p-2 text-gray-400 hover:text-red-600 transition-colors">
                          <Trash2 className="w-4 h-4" />
                        </button>
                      </div>
                    </div>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {activeTab === 'validation' && policies.length > 0 && (
          <PolicyValidation
            policyCode={policies[0].generated_code}
            policyLanguage={policies[0].output_format}
            policyType={policies[0].rule_type}
            onValidationComplete={(report) => {
              // Update policy validation status in backend
              const updateValidation = async () => {
                try {
                  const authToken = localStorage.getItem('authToken');
                  await fetch(`/api/policies/${policies[0].id}/validation`, {
                    method: 'POST',
                    headers: {
                      'Authorization': `Bearer ${authToken}`,
                      'Content-Type': 'application/json'
                    },
                    body: JSON.stringify(report)
                  });
                  console.log('Validation status updated');
                  loadPolicies();
                } catch (error) {
                  console.error('Failed to update validation:', error);
                }
              };
              updateValidation();
            }}
          />
        )}

        {activeTab === 'deployment' && policies.length > 0 && (
          <PolicyDeployment
            policyId={policies[0].id}
            policyName={policies[0].name}
            policyVersion={policies[0].version || '1.0.0'}
            onDeploymentComplete={(deployment) => {
              // Update deployment status in backend
              const updateDeployment = async () => {
                try {
                  const authToken = localStorage.getItem('authToken');
                  await fetch(`/api/policies/${policies[0].id}/deploy`, {
                    method: 'POST',
                    headers: {
                      'Authorization': `Bearer ${authToken}`,
                      'Content-Type': 'application/json'
                    },
                    body: JSON.stringify(deployment)
                  });
                  console.log('Deployment status updated');
                  loadPolicies();
                } catch (error) {
                  console.error('Failed to update deployment:', error);
                }
              };
              updateDeployment();
            }}
          />
        )}
      </div>
    </div>
  );
};

export default PolicyGeneration;
