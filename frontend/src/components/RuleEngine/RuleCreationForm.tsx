/**
 * Rule Creation Form Component
 * Comprehensive form for creating new fraud detection rules
 */

import React, { useState } from 'react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '../ui/card';
import { Button } from '../ui/button';
import { Input } from '../ui/input';
import { Textarea } from '../ui/textarea';
import { Label } from '../ui/label';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '../ui/select';
import { Switch } from '../ui/switch';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '../ui/tabs';
import { Badge } from '../ui/badge';
import { Alert, AlertDescription } from '../ui/alert';
import {
  Save,
  TestTube,
  Code,
  Settings,
  Brain,
  AlertTriangle,
  CheckCircle,
  Plus,
  Minus,
  HelpCircle
} from 'lucide-react';
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from '../ui/tooltip';

interface RuleFormData {
  name: string;
  description: string;
  type: 'VALIDATION' | 'SCORING' | 'PATTERN' | 'MACHINE_LEARNING';
  priority: 'LOW' | 'MEDIUM' | 'HIGH' | 'CRITICAL';
  isActive: boolean;

  // Rule-specific data
  validationRules?: ValidationRule[];
  scoringConfig?: ScoringConfig;
  patternConfig?: PatternConfig;
  mlConfig?: MLConfig;
}

interface ValidationRule {
  field: string;
  operator: 'equals' | 'not_equals' | 'greater_than' | 'less_than' | 'contains' | 'regex';
  value: string;
  severity: 'low' | 'medium' | 'high' | 'critical';
}

interface ScoringConfig {
  baseScore: number;
  factors: ScoringFactor[];
  threshold: number;
  action: 'flag' | 'block' | 'review';
}

interface ScoringFactor {
  field: string;
  weight: number;
  transformation: 'linear' | 'logarithmic' | 'exponential';
}

interface PatternConfig {
  patternType: 'velocity' | 'geographic' | 'amount' | 'time_based' | 'custom';
  timeWindow: number; // minutes
  threshold: number;
  conditions: PatternCondition[];
}

interface PatternCondition {
  field: string;
  operator: string;
  value: string;
}

interface MLConfig {
  modelType: 'supervised' | 'unsupervised' | 'semi_supervised';
  algorithm: 'random_forest' | 'neural_network' | 'svm' | 'xgboost';
  features: string[];
  trainingDataSize: number;
  accuracyThreshold: number;
}

const RuleCreationForm: React.FC = () => {
  const [formData, setFormData] = useState<RuleFormData>({
    name: '',
    description: '',
    type: 'VALIDATION',
    priority: 'MEDIUM',
    isActive: true
  });

  const [isSubmitting, setIsSubmitting] = useState(false);
  const [validationErrors, setValidationErrors] = useState<string[]>([]);
  const [showPreview, setShowPreview] = useState(false);

  const handleTypeChange = (type: RuleFormData['type']) => {
    setFormData(prev => ({
      ...prev,
      type,
      // Reset type-specific data when type changes
      validationRules: type === 'VALIDATION' ? [] : undefined,
      scoringConfig: type === 'SCORING' ? {
        baseScore: 0,
        factors: [],
        threshold: 50,
        action: 'review'
      } : undefined,
      patternConfig: type === 'PATTERN' ? {
        patternType: 'velocity',
        timeWindow: 60,
        threshold: 3,
        conditions: []
      } : undefined,
      mlConfig: type === 'MACHINE_LEARNING' ? {
        modelType: 'supervised',
        algorithm: 'random_forest',
        features: [],
        trainingDataSize: 1000,
        accuracyThreshold: 0.8
      } : undefined
    }));
  };

  const validateForm = (): boolean => {
    const errors: string[] = [];

    if (!formData.name.trim()) errors.push('Rule name is required');
    if (!formData.description.trim()) errors.push('Rule description is required');

    switch (formData.type) {
      case 'VALIDATION':
        if (!formData.validationRules?.length) {
          errors.push('At least one validation rule is required');
        }
        break;
      case 'SCORING':
        if (!formData.scoringConfig?.factors.length) {
          errors.push('At least one scoring factor is required');
        }
        break;
      case 'PATTERN':
        if (!formData.patternConfig?.conditions.length) {
          errors.push('At least one pattern condition is required');
        }
        break;
      case 'MACHINE_LEARNING':
        if (!formData.mlConfig?.features.length) {
          errors.push('At least one feature is required for ML model');
        }
        break;
    }

    setValidationErrors(errors);
    return errors.length === 0;
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();

    if (!validateForm()) return;

    setIsSubmitting(true);

    try {
      // In production, this would call the API
      console.log('Creating rule:', formData);

      // Simulate API call
      await new Promise(resolve => setTimeout(resolve, 2000));

      // Reset form on success
      setFormData({
        name: '',
        description: '',
        type: 'VALIDATION',
        priority: 'MEDIUM',
        isActive: true
      });

      alert('Rule created successfully!');
    } catch (error) {
      console.error('Error creating rule:', error);
      alert('Error creating rule. Please try again.');
    } finally {
      setIsSubmitting(false);
    }
  };

  const addValidationRule = () => {
    if (!formData.validationRules) return;

    const newRule: ValidationRule = {
      field: '',
      operator: 'equals',
      value: '',
      severity: 'medium'
    };

    setFormData(prev => ({
      ...prev,
      validationRules: [...(prev.validationRules || []), newRule]
    }));
  };

  const removeValidationRule = (index: number) => {
    setFormData(prev => ({
      ...prev,
      validationRules: prev.validationRules?.filter((_, i) => i !== index) || []
    }));
  };

  const updateValidationRule = (index: number, updates: Partial<ValidationRule>) => {
    setFormData(prev => ({
      ...prev,
      validationRules: prev.validationRules?.map((rule, i) =>
        i === index ? { ...rule, ...updates } : rule
      ) || []
    }));
  };

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-2xl font-bold">Create New Rule</h2>
          <p className="text-gray-600">Define a new fraud detection rule</p>
        </div>
        <div className="flex gap-2">
          <Button variant="outline" onClick={() => setShowPreview(!showPreview)}>
            <Code className="h-4 w-4 mr-2" />
            {showPreview ? 'Hide' : 'Show'} Preview
          </Button>
          <Button variant="outline">
            <TestTube className="h-4 w-4 mr-2" />
            Test Rule
          </Button>
        </div>
      </div>

      {/* Validation Errors */}
      {validationErrors.length > 0 && (
        <Alert variant="destructive">
          <AlertTriangle className="h-4 w-4" />
          <AlertDescription>
            <ul className="list-disc list-inside">
              {validationErrors.map((error, index) => (
                <li key={index}>{error}</li>
              ))}
            </ul>
          </AlertDescription>
        </Alert>
      )}

      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        {/* Main Form */}
        <div className="lg:col-span-2">
          <Card>
            <CardHeader>
              <CardTitle>Rule Configuration</CardTitle>
              <CardDescription>
                Configure the basic settings and logic for your fraud detection rule
              </CardDescription>
            </CardHeader>
            <CardContent className="space-y-6">
              <form onSubmit={handleSubmit} className="space-y-6">
                {/* Basic Information */}
                <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                  <div className="space-y-2">
                    <Label htmlFor="name">Rule Name *</Label>
                    <Input
                      id="name"
                      value={formData.name}
                      onChange={(e) => setFormData(prev => ({ ...prev, name: e.target.value }))}
                      placeholder="Enter rule name"
                      required
                    />
                  </div>

                  <div className="space-y-2">
                    <Label htmlFor="type">Rule Type *</Label>
                    <Select value={formData.type} onValueChange={handleTypeChange}>
                      <SelectTrigger>
                        <SelectValue />
                      </SelectTrigger>
                      <SelectContent>
                        <SelectItem value="VALIDATION">
                          <div className="flex items-center gap-2">
                            <CheckCircle className="h-4 w-4" />
                            Validation Rule
                          </div>
                        </SelectItem>
                        <SelectItem value="SCORING">
                          <div className="flex items-center gap-2">
                            <Settings className="h-4 w-4" />
                            Scoring Rule
                          </div>
                        </SelectItem>
                        <SelectItem value="PATTERN">
                          <div className="flex items-center gap-2">
                            <AlertTriangle className="h-4 w-4" />
                            Pattern Rule
                          </div>
                        </SelectItem>
                        <SelectItem value="MACHINE_LEARNING">
                          <div className="flex items-center gap-2">
                            <Brain className="h-4 w-4" />
                            Machine Learning Rule
                          </div>
                        </SelectItem>
                      </SelectContent>
                    </Select>
                  </div>
                </div>

                <div className="space-y-2">
                  <Label htmlFor="description">Description *</Label>
                  <Textarea
                    id="description"
                    value={formData.description}
                    onChange={(e) => setFormData(prev => ({ ...prev, description: e.target.value }))}
                    placeholder="Describe what this rule does and when it should trigger"
                    rows={3}
                    required
                  />
                </div>

                <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                  <div className="space-y-2">
                    <Label htmlFor="priority">Priority *</Label>
                    <Select value={formData.priority} onValueChange={(value: RuleFormData['priority']) =>
                      setFormData(prev => ({ ...prev, priority: value }))
                    }>
                      <SelectTrigger>
                        <SelectValue />
                      </SelectTrigger>
                      <SelectContent>
                        <SelectItem value="LOW">Low</SelectItem>
                        <SelectItem value="MEDIUM">Medium</SelectItem>
                        <SelectItem value="HIGH">High</SelectItem>
                        <SelectItem value="CRITICAL">Critical</SelectItem>
                      </SelectContent>
                    </Select>
                  </div>

                  <div className="flex items-center space-x-2 pt-8">
                    <Switch
                      id="isActive"
                      checked={formData.isActive}
                      onCheckedChange={(checked) => setFormData(prev => ({ ...prev, isActive: checked }))}
                    />
                    <Label htmlFor="isActive">Activate rule immediately</Label>
                  </div>
                </div>

                {/* Rule-Specific Configuration */}
                <Tabs value={formData.type} className="w-full">
                  <TabsList className="grid w-full grid-cols-4">
                    <TabsTrigger value="VALIDATION" disabled={formData.type !== 'VALIDATION'}>
                      Validation
                    </TabsTrigger>
                    <TabsTrigger value="SCORING" disabled={formData.type !== 'SCORING'}>
                      Scoring
                    </TabsTrigger>
                    <TabsTrigger value="PATTERN" disabled={formData.type !== 'PATTERN'}>
                      Pattern
                    </TabsTrigger>
                    <TabsTrigger value="MACHINE_LEARNING" disabled={formData.type !== 'MACHINE_LEARNING'}>
                      ML
                    </TabsTrigger>
                  </TabsList>

                  {/* Validation Rules */}
                  <TabsContent value="VALIDATION" className="space-y-4">
                    <div className="space-y-4">
                      <div className="flex items-center justify-between">
                        <h3 className="text-lg font-medium">Validation Rules</h3>
                        <Button type="button" variant="outline" size="sm" onClick={addValidationRule}>
                          <Plus className="h-4 w-4 mr-2" />
                          Add Rule
                        </Button>
                      </div>

                      {formData.validationRules?.map((rule, index) => (
                        <Card key={index}>
                          <CardContent className="pt-4">
                            <div className="grid grid-cols-1 md:grid-cols-5 gap-4">
                              <div>
                                <Label>Field</Label>
                                <Input
                                  value={rule.field}
                                  onChange={(e) => updateValidationRule(index, { field: e.target.value })}
                                  placeholder="e.g., amount"
                                />
                              </div>
                              <div>
                                <Label>Operator</Label>
                                <Select
                                  value={rule.operator}
                                  onValueChange={(value: ValidationRule['operator']) =>
                                    updateValidationRule(index, { operator: value })
                                  }
                                >
                                  <SelectTrigger>
                                    <SelectValue />
                                  </SelectTrigger>
                                  <SelectContent>
                                    <SelectItem value="equals">Equals</SelectItem>
                                    <SelectItem value="not_equals">Not Equals</SelectItem>
                                    <SelectItem value="greater_than">Greater Than</SelectItem>
                                    <SelectItem value="less_than">Less Than</SelectItem>
                                    <SelectItem value="contains">Contains</SelectItem>
                                    <SelectItem value="regex">Regex</SelectItem>
                                  </SelectContent>
                                </Select>
                              </div>
                              <div>
                                <Label>Value</Label>
                                <Input
                                  value={rule.value}
                                  onChange={(e) => updateValidationRule(index, { value: e.target.value })}
                                  placeholder="threshold value"
                                />
                              </div>
                              <div>
                                <Label>Severity</Label>
                                <Select
                                  value={rule.severity}
                                  onValueChange={(value: ValidationRule['severity']) =>
                                    updateValidationRule(index, { severity: value })
                                  }
                                >
                                  <SelectTrigger>
                                    <SelectValue />
                                  </SelectTrigger>
                                  <SelectContent>
                                    <SelectItem value="low">Low</SelectItem>
                                    <SelectItem value="medium">Medium</SelectItem>
                                    <SelectItem value="high">High</SelectItem>
                                    <SelectItem value="critical">Critical</SelectItem>
                                  </SelectContent>
                                </Select>
                              </div>
                              <div className="flex items-end">
                                <Button
                                  type="button"
                                  variant="outline"
                                  size="sm"
                                  onClick={() => removeValidationRule(index)}
                                >
                                  <Minus className="h-4 w-4" />
                                </Button>
                              </div>
                            </div>
                          </CardContent>
                        </Card>
                      ))}
                    </div>
                  </TabsContent>

                  {/* Scoring Configuration */}
                  <TabsContent value="SCORING" className="space-y-4">
                    <div className="text-center text-gray-500 py-8">
                      Scoring rule configuration coming soon...
                    </div>
                  </TabsContent>

                  {/* Pattern Configuration */}
                  <TabsContent value="PATTERN" className="space-y-4">
                    <div className="text-center text-gray-500 py-8">
                      Pattern rule configuration coming soon...
                    </div>
                  </TabsContent>

                  {/* Machine Learning Configuration */}
                  <TabsContent value="MACHINE_LEARNING" className="space-y-4">
                    <div className="text-center text-gray-500 py-8">
                      Machine learning rule configuration coming soon...
                    </div>
                  </TabsContent>
                </Tabs>

                {/* Submit Button */}
                <div className="flex gap-4 pt-6">
                  <Button type="submit" disabled={isSubmitting} className="flex-1">
                    <Save className="h-4 w-4 mr-2" />
                    {isSubmitting ? 'Creating Rule...' : 'Create Rule'}
                  </Button>
                  <Button type="button" variant="outline" onClick={() => {
                    setFormData({
                      name: '',
                      description: '',
                      type: 'VALIDATION',
                      priority: 'MEDIUM',
                      isActive: true
                    });
                    setValidationErrors([]);
                  }}>
                    Reset Form
                  </Button>
                </div>
              </form>
            </CardContent>
          </Card>
        </div>

        {/* Preview Panel */}
        {showPreview && (
          <div className="lg:col-span-1">
            <Card>
              <CardHeader>
                <CardTitle>Rule Preview</CardTitle>
                <CardDescription>
                  JSON representation of the rule configuration
                </CardDescription>
              </CardHeader>
              <CardContent>
                <pre className="text-xs bg-gray-50 p-4 rounded overflow-auto max-h-96">
                  {JSON.stringify(formData, null, 2)}
                </pre>
              </CardContent>
            </Card>
          </div>
        )}
      </div>
    </div>
  );
};

export default RuleCreationForm;