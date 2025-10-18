/**
 * Policy Template Component
 * Comprehensive policy template management interface
 * Production-grade template creation, editing, and usage tracking
 */

import React, { useState, useEffect } from 'react';
import { getUserIdFromToken } from '../../utils/auth';
import {
  FileText,
  Plus,
  Edit3,
  Trash2,
  Download,
  Upload,
  Eye,
  Copy,
  Play,
  Settings,
  BarChart3,
  Code,
  Tag,
  Calendar,
  User,
  Zap,
  AlertTriangle,
  CheckCircle,
  Info
} from 'lucide-react';
import { clsx } from 'clsx';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { formatDistanceToNow } from 'date-fns';

// Types matching backend policy generation
interface TemplateVariable {
  name: string;
  type: 'string' | 'number' | 'boolean' | 'array' | 'object';
  description: string;
  required: boolean;
  defaultValue?: any;
  validation?: {
    min?: number;
    max?: number;
    pattern?: string;
    enum?: string[];
  };
}

interface PolicyTemplate {
  id: string;
  name: string;
  description: string;
  category: string;
  ruleType: 'validation_rule' | 'business_rule' | 'compliance_rule' | 'risk_rule' | 'audit_rule' | 'workflow_rule';
  domain: 'financial_compliance' | 'data_privacy' | 'regulatory_reporting' | 'risk_management' | 'operational_controls' | 'security_policy' | 'audit_procedures';
  templateCode: string;
  variables: TemplateVariable[];
  tags: string[];
  createdAt: string;
  updatedAt: string;
  createdBy: string;
  usageCount: number;
  lastUsed?: string;
  averageGenerationTime: number;
  successRate: number;
  version: string;
  isActive: boolean;
  metadata: {
    language: 'json' | 'yaml' | 'javascript' | 'python';
    complexity: 'simple' | 'medium' | 'complex';
    estimatedTokens: number;
    dependencies?: string[];
  };
}

interface TemplateCreationData {
  name: string;
  description: string;
  category: string;
  ruleType: 'validation_rule' | 'business_rule' | 'compliance_rule' | 'risk_rule' | 'audit_rule' | 'workflow_rule';
  domain: 'financial_compliance' | 'data_privacy' | 'regulatory_reporting' | 'risk_management' | 'operational_controls' | 'security_policy' | 'audit_procedures';
  templateCode: string;
  variables: TemplateVariable[];
  tags: string[];
  language: 'json' | 'yaml' | 'javascript' | 'python';
  complexity: 'simple' | 'medium' | 'complex';
}

interface TemplateFilters {
  category?: string;
  ruleType?: string;
  domain?: string;
  language?: string;
  search?: string;
  isActive?: boolean;
}

interface PolicyTemplateProps {
  onUseTemplate?: (template: PolicyTemplate, variables: Record<string, any>) => void;
  className?: string;
}

const AVAILABLE_CATEGORIES = [
  'Financial Compliance',
  'Data Privacy',
  'Regulatory Reporting',
  'Risk Management',
  'Operational Controls',
  'Security Policy',
  'Audit Procedures',
  'Custom Templates'
];

const RULE_TYPES = [
  { value: 'validation_rule', label: 'Validation Rule' },
  { value: 'business_rule', label: 'Business Rule' },
  { value: 'compliance_rule', label: 'Compliance Rule' },
  { value: 'risk_rule', label: 'Risk Rule' },
  { value: 'audit_rule', label: 'Audit Rule' },
  { value: 'workflow_rule', label: 'Workflow Rule' }
];

const DOMAINS = [
  { value: 'financial_compliance', label: 'Financial Compliance' },
  { value: 'data_privacy', label: 'Data Privacy' },
  { value: 'regulatory_reporting', label: 'Regulatory Reporting' },
  { value: 'risk_management', label: 'Risk Management' },
  { value: 'operational_controls', label: 'Operational Controls' },
  { value: 'security_policy', label: 'Security Policy' },
  { value: 'audit_procedures', label: 'Audit Procedures' }
];

const LANGUAGES = [
  { value: 'json', label: 'JSON' },
  { value: 'yaml', label: 'YAML' },
  { value: 'javascript', label: 'JavaScript' },
  { value: 'python', label: 'Python' }
];

const VARIABLE_TYPES = [
  { value: 'string', label: 'String' },
  { value: 'number', label: 'Number' },
  { value: 'boolean', label: 'Boolean' },
  { value: 'array', label: 'Array' },
  { value: 'object', label: 'Object' }
];

const PolicyTemplate: React.FC<PolicyTemplateProps> = ({
  onUseTemplate,
  className = ''
}) => {
  const [templates, setTemplates] = useState<PolicyTemplate[]>([]);
  const [filters, setFilters] = useState<TemplateFilters>({});
  const [selectedTemplate, setSelectedTemplate] = useState<PolicyTemplate | null>(null);
  const [showCreateModal, setShowCreateModal] = useState(false);
  const [showEditModal, setShowEditModal] = useState(false);
  const [showUseModal, setShowUseModal] = useState(false);
  const [isLoading, setIsLoading] = useState(true);
  const [templateVariables, setTemplateVariables] = useState<Record<string, any>>({});
  const [searchTerm, setSearchTerm] = useState('');

  // Simulated template data loading
  useEffect(() => {
    const loadTemplates = async () => {
      try {
        setIsLoading(true);
        await new Promise(resolve => setTimeout(resolve, 1000));

        // Mock template data
        setTemplates([
          {
            id: 'template-1',
            name: 'Transaction Monitoring Rule',
            description: 'Template for creating transaction monitoring rules with customizable thresholds',
            category: 'Financial Compliance',
            ruleType: 'compliance_rule',
            domain: 'financial_compliance',
            templateCode: `{
  "rule": {
    "name": "{{rule_name}}",
    "description": "{{rule_description}}",
    "condition": "transaction.amount > {{threshold}} AND transaction.{{field}} {{operator}} {{value}}",
    "actions": {{actions}},
    "severity": "{{severity}}",
    "enabled": true,
    "metadata": {
      "category": "{{category}}",
      "domain": "{{domain}}"
    }
  }
}`,
            variables: [
              {
                name: 'rule_name',
                type: 'string',
                description: 'Name of the rule',
                required: true
              },
              {
                name: 'rule_description',
                type: 'string',
                description: 'Description of what this rule does',
                required: true
              },
              {
                name: 'threshold',
                type: 'number',
                description: 'Transaction amount threshold',
                required: true,
                defaultValue: 10000,
                validation: { min: 0 }
              },
              {
                name: 'field',
                type: 'string',
                description: 'Transaction field to check',
                required: true,
                defaultValue: 'amount'
              },
              {
                name: 'operator',
                type: 'string',
                description: 'Comparison operator',
                required: true,
                defaultValue: '>',
                validation: { enum: ['>', '>=', '<', '<=', '==', '!='] }
              },
              {
                name: 'value',
                type: 'string',
                description: 'Value to compare against',
                required: true
              },
              {
                name: 'actions',
                type: 'array',
                description: 'Actions to take when rule triggers',
                required: true,
                defaultValue: ['flag_transaction', 'notify_compliance']
              },
              {
                name: 'severity',
                type: 'string',
                description: 'Rule severity level',
                required: true,
                defaultValue: 'medium',
                validation: { enum: ['low', 'medium', 'high', 'critical'] }
              },
              {
                name: 'category',
                type: 'string',
                description: 'Rule category',
                required: false,
                defaultValue: 'Financial Compliance'
              },
              {
                name: 'domain',
                type: 'string',
                description: 'Business domain',
                required: false,
                defaultValue: 'financial_compliance'
              }
            ],
            tags: ['transaction', 'monitoring', 'compliance', 'financial'],
            createdAt: '2024-01-15T10:00:00Z',
            updatedAt: '2024-01-20T14:30:00Z',
            createdBy: 'admin',
            usageCount: 15,
            lastUsed: '2024-01-20T09:00:00Z',
            averageGenerationTime: 850,
            successRate: 0.96,
            version: '1.2.0',
            isActive: true,
            metadata: {
              language: 'json',
              complexity: 'medium',
              estimatedTokens: 450,
              dependencies: []
            }
          },
          {
            id: 'template-2',
            name: 'Data Validation Rule',
            description: 'Template for data validation and quality checks',
            category: 'Data Quality',
            ruleType: 'validation_rule',
            domain: 'operational_controls',
            templateCode: `function validate{{entityType}}Data(data) {
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

  // Additional business logic
  {{business_logic}}

  return {
    valid: true,
    validatedData: data,
    metadata: {
      validationTime: Date.now(),
      ruleVersion: "{{version}}"
    }
  };
}`,
            variables: [
              {
                name: 'entityType',
                type: 'string',
                description: 'Type of entity being validated',
                required: true,
                defaultValue: 'Customer'
              },
              {
                name: 'required_fields',
                type: 'array',
                description: 'List of required field names',
                required: true,
                defaultValue: ['id', 'name', 'email']
              },
              {
                name: 'validation_rules',
                type: 'object',
                description: 'Validation rules for each field',
                required: true,
                defaultValue: {
                  email: { type: 'email', required: true },
                  phone: { type: 'phone', required: false }
                }
              },
              {
                name: 'business_logic',
                type: 'string',
                description: 'Additional business logic code',
                required: false,
                defaultValue: '// Additional validation logic here'
              },
              {
                name: 'version',
                type: 'string',
                description: 'Rule version',
                required: false,
                defaultValue: '1.0.0'
              }
            ],
            tags: ['validation', 'data-quality', 'business-logic'],
            createdAt: '2024-01-10T09:15:00Z',
            updatedAt: '2024-01-18T11:20:00Z',
            createdBy: 'compliance_officer',
            usageCount: 8,
            lastUsed: '2024-01-19T16:30:00Z',
            averageGenerationTime: 1200,
            successRate: 0.94,
            version: '2.1.0',
            isActive: true,
            metadata: {
              language: 'javascript',
              complexity: 'complex',
              estimatedTokens: 680,
              dependencies: ['ValidationError', 'applyValidationRule']
            }
          }
        ]);
        setIsLoading(false);
      } catch (error) {
        console.error('Failed to load templates:', error);
        setIsLoading(false);
      }
    };

    loadTemplates();
  }, []);

  const filteredTemplates = templates.filter(template => {
    if (filters.category && template.category !== filters.category) return false;
    if (filters.ruleType && template.ruleType !== filters.ruleType) return false;
    if (filters.domain && template.domain !== filters.domain) return false;
    if (filters.language && template.metadata.language !== filters.language) return false;
    if (filters.isActive !== undefined && template.isActive !== filters.isActive) return false;
    if (searchTerm) {
      const searchLower = searchTerm.toLowerCase();
      return template.name.toLowerCase().includes(searchLower) ||
             template.description.toLowerCase().includes(searchLower) ||
             template.tags.some(tag => tag.toLowerCase().includes(searchLower));
    }
    return true;
  });

  const handleUseTemplate = (template: PolicyTemplate) => {
    setSelectedTemplate(template);
    setShowUseModal(true);
    // Initialize variables with defaults
    const initialVars: Record<string, any> = {};
    template.variables.forEach(variable => {
      initialVars[variable.name] = variable.defaultValue !== undefined ? variable.defaultValue : '';
    });
    setTemplateVariables(initialVars);
  };

  const handleGenerateFromTemplate = () => {
    if (selectedTemplate && onUseTemplate) {
      onUseTemplate(selectedTemplate, templateVariables);
      setShowUseModal(false);
      setSelectedTemplate(null);
      setTemplateVariables({});
    }
  };

  const handleCreateTemplate = (templateData: TemplateCreationData) => {
    const newTemplate: PolicyTemplate = {
      id: `template-${Date.now()}`,
      ...templateData,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
      createdBy: getUserIdFromToken(),
      usageCount: 0,
      averageGenerationTime: 0,
      successRate: 0,
      version: '1.0.0',
      isActive: true,
      metadata: {
        language: templateData.language,
        complexity: templateData.complexity,
        estimatedTokens: Math.floor(templateData.templateCode.length / 3),
        dependencies: []
      }
    };

    setTemplates(prev => [newTemplate, ...prev]);
    setShowCreateModal(false);
  };

  const getComplexityColor = (complexity: string) => {
    switch (complexity) {
      case 'simple': return 'bg-green-100 text-green-800';
      case 'medium': return 'bg-yellow-100 text-yellow-800';
      case 'complex': return 'bg-red-100 text-red-800';
      default: return 'bg-gray-100 text-gray-800';
    }
  };

  const getLanguageColor = (language: string) => {
    switch (language) {
      case 'json': return 'bg-blue-100 text-blue-800';
      case 'yaml': return 'bg-purple-100 text-purple-800';
      case 'javascript': return 'bg-yellow-100 text-yellow-800';
      case 'python': return 'bg-green-100 text-green-800';
      default: return 'bg-gray-100 text-gray-800';
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
    <div className={clsx('space-y-6', className)}>
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-2xl font-bold text-gray-900 flex items-center gap-2">
            <FileText className="w-6 h-6 text-blue-600" />
            Policy Templates
          </h2>
          <p className="text-gray-600 mt-1">
            Reusable templates for generating policies and rules
          </p>
        </div>
        <button
          onClick={() => setShowCreateModal(true)}
          className="bg-blue-600 hover:bg-blue-700 text-white px-4 py-2 rounded-lg flex items-center gap-2 transition-colors"
        >
          <Plus className="w-4 h-4" />
          Create Template
        </button>
      </div>

      {/* Filters */}
      <div className="bg-white rounded-lg border shadow-sm p-6">
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-2">
              Search
            </label>
            <input
              type="text"
              value={searchTerm}
              onChange={(e) => setSearchTerm(e.target.value)}
              placeholder="Search templates..."
              className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
            />
          </div>

          <div>
            <label className="block text-sm font-medium text-gray-700 mb-2">
              Category
            </label>
            <select
              value={filters.category || ''}
              onChange={(e) => setFilters(prev => ({ ...prev, category: e.target.value || undefined }))}
              className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
            >
              <option value="">All Categories</option>
              {AVAILABLE_CATEGORIES.map(category => (
                <option key={category} value={category}>{category}</option>
              ))}
            </select>
          </div>

          <div>
            <label className="block text-sm font-medium text-gray-700 mb-2">
              Rule Type
            </label>
            <select
              value={filters.ruleType || ''}
              onChange={(e) => setFilters(prev => ({ ...prev, ruleType: e.target.value as any || undefined }))}
              className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
            >
              <option value="">All Types</option>
              {RULE_TYPES.map(type => (
                <option key={type.value} value={type.value}>{type.label}</option>
              ))}
            </select>
          </div>

          <div>
            <label className="block text-sm font-medium text-gray-700 mb-2">
              Language
            </label>
            <select
              value={filters.language || ''}
              onChange={(e) => setFilters(prev => ({ ...prev, language: e.target.value as any || undefined }))}
              className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
            >
              <option value="">All Languages</option>
              {LANGUAGES.map(lang => (
                <option key={lang.value} value={lang.value}>{lang.label}</option>
              ))}
            </select>
          </div>
        </div>
      </div>

      {/* Templates Grid */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
        {filteredTemplates.map((template) => (
          <div key={template.id} className="bg-white rounded-lg border shadow-sm hover:shadow-md transition-shadow">
            <div className="p-6">
              <div className="flex items-start justify-between mb-4">
                <div className="flex-1">
                  <h3 className="text-lg font-semibold text-gray-900 mb-2">
                    {template.name}
                  </h3>
                  <p className="text-sm text-gray-600 mb-3 line-clamp-2">
                    {template.description}
                  </p>
                </div>
                <div className="flex items-center gap-2">
                  <span className={clsx(
                    'px-2 py-1 text-xs font-medium rounded-full',
                    getLanguageColor(template.metadata.language)
                  )}>
                    {template.metadata.language.toUpperCase()}
                  </span>
                  <span className={clsx(
                    'px-2 py-1 text-xs font-medium rounded-full',
                    getComplexityColor(template.metadata.complexity)
                  )}>
                    {template.metadata.complexity}
                  </span>
                </div>
              </div>

              <div className="space-y-2 mb-4">
                <div className="flex items-center gap-4 text-sm text-gray-600">
                  <span className="flex items-center gap-1">
                    <BarChart3 className="w-4 h-4" />
                    Used {template.usageCount} times
                  </span>
                  <span className="flex items-center gap-1">
                    <CheckCircle className="w-4 h-4" />
                    {(template.successRate * 100).toFixed(0)}% success
                  </span>
                </div>

                <div className="flex items-center gap-4 text-sm text-gray-600">
                  <span className="flex items-center gap-1">
                    <Zap className="w-4 h-4" />
                    {template.averageGenerationTime}ms avg
                  </span>
                  <span className="flex items-center gap-1">
                    <Tag className="w-4 h-4" />
                    v{template.version}
                  </span>
                </div>

                {template.lastUsed && (
                  <div className="text-sm text-gray-500">
                    Last used {formatDistanceToNow(new Date(template.lastUsed), { addSuffix: true })}
                  </div>
                )}
              </div>

              <div className="flex items-center gap-2 mb-4">
                {template.tags.slice(0, 3).map((tag) => (
                  <span
                    key={tag}
                    className="px-2 py-1 bg-gray-100 text-gray-700 text-xs rounded-full"
                  >
                    {tag}
                  </span>
                ))}
                {template.tags.length > 3 && (
                  <span className="text-xs text-gray-500">
                    +{template.tags.length - 3} more
                  </span>
                )}
              </div>

              <div className="flex items-center justify-between">
                <div className="text-sm text-gray-600">
                  {template.variables.length} variables
                </div>
                <div className="flex items-center gap-2">
                  <button
                    onClick={() => setSelectedTemplate(template)}
                    className="p-2 text-gray-400 hover:text-blue-600 transition-colors"
                    title="View template"
                  >
                    <Eye className="w-4 h-4" />
                  </button>
                  <button
                    onClick={() => handleUseTemplate(template)}
                    className="p-2 text-gray-400 hover:text-green-600 transition-colors"
                    title="Use template"
                  >
                    <Play className="w-4 h-4" />
                  </button>
                  <button
                    onClick={() => setShowEditModal(true)}
                    className="p-2 text-gray-400 hover:text-yellow-600 transition-colors"
                    title="Edit template"
                  >
                    <Edit3 className="w-4 h-4" />
                  </button>
                  <button
                    onClick={() => {
                      if (confirm('Delete this template?')) {
                        setTemplates(prev => prev.filter(t => t.id !== template.id));
                      }
                    }}
                    className="p-2 text-gray-400 hover:text-red-600 transition-colors"
                    title="Delete template"
                  >
                    <Trash2 className="w-4 h-4" />
                  </button>
                </div>
              </div>
            </div>
          </div>
        ))}
      </div>

      {/* Template Detail Modal */}
      {selectedTemplate && !showUseModal && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50 p-4">
          <div className="bg-white rounded-lg shadow-xl max-w-4xl w-full max-h-[90vh] overflow-hidden">
            <div className="flex items-center justify-between p-6 border-b border-gray-200">
              <div>
                <h3 className="text-2xl font-bold text-gray-900">{selectedTemplate.name}</h3>
                <p className="text-gray-600 mt-1">{selectedTemplate.description}</p>
              </div>
              <button
                onClick={() => setSelectedTemplate(null)}
                className="p-2 hover:bg-gray-100 rounded-lg transition-colors"
              >
                <Trash2 className="w-5 h-5" />
              </button>
            </div>

            <div className="p-6 overflow-y-auto max-h-96">
              <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
                <div>
                  <h4 className="text-lg font-semibold text-gray-900 mb-4">Template Details</h4>
                  <div className="space-y-3">
                    <div>
                      <span className="text-sm font-medium text-gray-700">Category:</span>
                      <span className="ml-2 text-sm text-gray-600">{selectedTemplate.category}</span>
                    </div>
                    <div>
                      <span className="text-sm font-medium text-gray-700">Rule Type:</span>
                      <span className="ml-2 text-sm text-gray-600 capitalize">
                        {selectedTemplate.ruleType.replace('_', ' ')}
                      </span>
                    </div>
                    <div>
                      <span className="text-sm font-medium text-gray-700">Domain:</span>
                      <span className="ml-2 text-sm text-gray-600 capitalize">
                        {selectedTemplate.domain.replace('_', ' ')}
                      </span>
                    </div>
                    <div>
                      <span className="text-sm font-medium text-gray-700">Language:</span>
                      <span className={clsx(
                        'ml-2 px-2 py-1 text-xs font-medium rounded-full',
                        getLanguageColor(selectedTemplate.metadata.language)
                      )}>
                        {selectedTemplate.metadata.language.toUpperCase()}
                      </span>
                    </div>
                    <div>
                      <span className="text-sm font-medium text-gray-700">Complexity:</span>
                      <span className={clsx(
                        'ml-2 px-2 py-1 text-xs font-medium rounded-full',
                        getComplexityColor(selectedTemplate.metadata.complexity)
                      )}>
                        {selectedTemplate.metadata.complexity}
                      </span>
                    </div>
                  </div>
                </div>

                <div>
                  <h4 className="text-lg font-semibold text-gray-900 mb-4">Variables</h4>
                  <div className="space-y-2 max-h-48 overflow-y-auto">
                    {selectedTemplate.variables.map((variable, index) => (
                      <div key={index} className="flex items-center justify-between p-2 bg-gray-50 rounded">
                        <div>
                          <span className="font-medium text-sm">{variable.name}</span>
                          {variable.required && <span className="text-red-500 ml-1">*</span>}
                          <div className="text-xs text-gray-600">{variable.description}</div>
                        </div>
                        <span className="text-xs bg-blue-100 text-blue-800 px-2 py-1 rounded">
                          {variable.type}
                        </span>
                      </div>
                    ))}
                  </div>
                </div>
              </div>

              <div className="mt-6">
                <h4 className="text-lg font-semibold text-gray-900 mb-4">Template Code</h4>
                <div className="bg-gray-900 rounded-lg p-4 overflow-x-auto">
                  <pre className="text-sm text-gray-100">
                    <code>{selectedTemplate.templateCode}</code>
                  </pre>
                </div>
              </div>
            </div>

            <div className="flex items-center justify-end gap-3 p-6 border-t border-gray-200 bg-gray-50">
              <button
                onClick={() => handleUseTemplate(selectedTemplate)}
                className="bg-green-600 hover:bg-green-700 text-white px-6 py-2 rounded-lg transition-colors"
              >
                Use Template
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Use Template Modal */}
      {showUseModal && selectedTemplate && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50 p-4">
          <div className="bg-white rounded-lg shadow-xl max-w-2xl w-full max-h-[90vh] overflow-hidden">
            <div className="flex items-center justify-between p-6 border-b border-gray-200">
              <div>
                <h3 className="text-2xl font-bold text-gray-900">Use Template</h3>
                <p className="text-gray-600 mt-1">{selectedTemplate.name}</p>
              </div>
              <button
                onClick={() => setShowUseModal(false)}
                className="p-2 hover:bg-gray-100 rounded-lg transition-colors"
              >
                <Trash2 className="w-5 h-5" />
              </button>
            </div>

            <div className="p-6 overflow-y-auto max-h-96">
              <div className="space-y-4">
                {selectedTemplate.variables.map((variable) => (
                  <div key={variable.name}>
                    <label className="block text-sm font-medium text-gray-700 mb-2">
                      {variable.name}
                      {variable.required && <span className="text-red-500 ml-1">*</span>}
                    </label>
                    <p className="text-xs text-gray-600 mb-2">{variable.description}</p>

                    {variable.type === 'string' && (
                      <input
                        type="text"
                        value={templateVariables[variable.name] || ''}
                        onChange={(e) => setTemplateVariables(prev => ({
                          ...prev,
                          [variable.name]: e.target.value
                        }))}
                        className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                        placeholder={variable.defaultValue || ''}
                      />
                    )}

                    {variable.type === 'number' && (
                      <input
                        type="number"
                        value={templateVariables[variable.name] || ''}
                        onChange={(e) => setTemplateVariables(prev => ({
                          ...prev,
                          [variable.name]: Number(e.target.value) || 0
                        }))}
                        className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                        placeholder={variable.defaultValue?.toString() || ''}
                      />
                    )}

                    {variable.type === 'boolean' && (
                      <select
                        value={templateVariables[variable.name]?.toString() || 'false'}
                        onChange={(e) => setTemplateVariables(prev => ({
                          ...prev,
                          [variable.name]: e.target.value === 'true'
                        }))}
                        className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                      >
                        <option value="true">True</option>
                        <option value="false">False</option>
                      </select>
                    )}

                    {variable.type === 'array' && (
                      <textarea
                        value={Array.isArray(templateVariables[variable.name])
                          ? JSON.stringify(templateVariables[variable.name], null, 2)
                          : templateVariables[variable.name] || ''}
                        onChange={(e) => {
                          try {
                            const parsed = JSON.parse(e.target.value);
                            setTemplateVariables(prev => ({
                              ...prev,
                              [variable.name]: Array.isArray(parsed) ? parsed : []
                            }));
                          } catch {
                            setTemplateVariables(prev => ({
                              ...prev,
                              [variable.name]: e.target.value
                            }));
                          }
                        }}
                        rows={3}
                        className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500 font-mono text-sm"
                        placeholder="JSON array"
                      />
                    )}

                    {variable.type === 'object' && (
                      <textarea
                        value={typeof templateVariables[variable.name] === 'object'
                          ? JSON.stringify(templateVariables[variable.name], null, 2)
                          : templateVariables[variable.name] || ''}
                        onChange={(e) => {
                          try {
                            const parsed = JSON.parse(e.target.value);
                            setTemplateVariables(prev => ({
                              ...prev,
                              [variable.name]: typeof parsed === 'object' ? parsed : {}
                            }));
                          } catch {
                            setTemplateVariables(prev => ({
                              ...prev,
                              [variable.name]: e.target.value
                            }));
                          }
                        }}
                        rows={4}
                        className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500 font-mono text-sm"
                        placeholder="JSON object"
                      />
                    )}
                  </div>
                ))}
              </div>
            </div>

            <div className="flex items-center justify-end gap-3 p-6 border-t border-gray-200 bg-gray-50">
              <button
                onClick={() => setShowUseModal(false)}
                className="px-4 py-2 text-gray-700 hover:bg-gray-100 rounded-lg transition-colors"
              >
                Cancel
              </button>
              <button
                onClick={handleGenerateFromTemplate}
                className="bg-blue-600 hover:bg-blue-700 text-white px-6 py-2 rounded-lg transition-colors"
              >
                Generate Policy
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Create Template Modal - Placeholder for now */}
      {showCreateModal && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50 p-4">
          <div className="bg-white rounded-lg shadow-xl max-w-2xl w-full">
            <div className="flex items-center justify-between p-6 border-b border-gray-200">
              <div>
                <h3 className="text-2xl font-bold text-gray-900">Create Template</h3>
                <p className="text-gray-600 mt-1">Template creation form coming soon</p>
              </div>
              <button
                onClick={() => setShowCreateModal(false)}
                className="p-2 hover:bg-gray-100 rounded-lg transition-colors"
              >
                <Trash2 className="w-5 h-5" />
              </button>
            </div>
            <div className="p-6">
              <div className="text-center py-12 text-gray-500">
                <Settings className="w-12 h-12 mx-auto mb-4 text-gray-400" />
                <p className="text-lg font-medium">Template Creation</p>
                <p className="text-sm">Advanced template creation interface</p>
                <p className="text-xs mt-2 text-blue-600">Implementation in progress</p>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default PolicyTemplate;
