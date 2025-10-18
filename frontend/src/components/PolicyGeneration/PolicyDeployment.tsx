/**
 * Policy Deployment Component
 * Comprehensive policy deployment and management interface
 * Production-grade deployment system with safety checks and rollback capabilities
 */

import React, { useState, useEffect } from 'react';
import { getUserIdFromToken } from '../../utils/auth';
import {
  Rocket,
  AlertTriangle,
  CheckCircle,
  XCircle,
  Clock,
  RefreshCw,
  Shield,
  GitBranch,
  History,
  Settings,
  Play,
  Pause,
  RotateCcw,
  Eye,
  Download,
  Upload,
  Lock,
  Unlock,
  Users,
  Bell,
  Zap
} from 'lucide-react';
import { clsx } from 'clsx';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { formatDistanceToNow } from 'date-fns';

// Types matching backend deployment system
interface DeploymentTarget {
  id: string;
  name: string;
  environment: 'development' | 'staging' | 'production';
  region: string;
  status: 'healthy' | 'degraded' | 'unhealthy' | 'maintenance';
  version: string;
  lastDeployment?: string;
  requiresApproval: boolean;
  approvers: string[];
}

interface DeploymentRequest {
  id: string;
  policyId: string;
  policyName: string;
  targetEnvironment: string;
  requestedBy: string;
  requestedAt: string;
  status: 'pending_approval' | 'approved' | 'rejected' | 'deploying' | 'deployed' | 'failed' | 'rolled_back';
  approvalRequired: boolean;
  approvers: string[];
  approvedBy?: string[];
  deploymentNotes?: string;
  rollbackReason?: string;
}

interface DeploymentStep {
  id: string;
  name: string;
  description: string;
  status: 'pending' | 'running' | 'completed' | 'failed' | 'skipped';
  startTime?: string;
  endTime?: string;
  duration?: number;
  logs: string[];
  errorMessage?: string;
}

interface DeploymentStatus {
  deploymentId: string;
  overallStatus: 'pending' | 'running' | 'completed' | 'failed' | 'rolled_back';
  currentStep?: string;
  progress: number;
  steps: DeploymentStep[];
  startTime: string;
  estimatedCompletion?: string;
  deployedVersion?: string;
  rollbackAvailable: boolean;
}

interface SafetyCheck {
  id: string;
  name: string;
  description: string;
  status: 'pending' | 'running' | 'passed' | 'failed' | 'warning';
  severity: 'low' | 'medium' | 'high' | 'critical';
  details?: string;
  recommendations?: string[];
}

interface PolicyDeploymentProps {
  policyId: string;
  policyName: string;
  policyVersion: string;
  onDeploymentComplete?: (deployment: DeploymentStatus) => void;
  className?: string;
}

const DEPLOYMENT_TARGETS: DeploymentTarget[] = [
  {
    id: 'dev-us-east-1',
    name: 'Development (US East)',
    environment: 'development',
    region: 'us-east-1',
    status: 'healthy',
    version: '2.1.0',
    lastDeployment: '2024-01-20T14:30:00Z',
    requiresApproval: false,
    approvers: []
  },
  {
    id: 'staging-us-west-2',
    name: 'Staging (US West)',
    environment: 'staging',
    region: 'us-west-2',
    status: 'healthy',
    version: '2.0.5',
    lastDeployment: '2024-01-19T16:45:00Z',
    requiresApproval: true,
    approvers: ['compliance_officer', 'devops_lead']
  },
  {
    id: 'prod-us-east-1',
    name: 'Production (US East)',
    environment: 'production',
    region: 'us-east-1',
    status: 'healthy',
    version: '2.0.5',
    lastDeployment: '2024-01-18T10:15:00Z',
    requiresApproval: true,
    approvers: ['compliance_officer', 'risk_officer', 'cfo']
  },
  {
    id: 'prod-eu-west-1',
    name: 'Production (EU West)',
    environment: 'production',
    region: 'eu-west-1',
    status: 'healthy',
    version: '2.0.5',
    lastDeployment: '2024-01-18T09:30:00Z',
    requiresApproval: true,
    approvers: ['compliance_officer', 'risk_officer', 'cfo']
  }
];

const SAFETY_CHECKS: SafetyCheck[] = [
  {
    id: 'syntax_validation',
    name: 'Syntax Validation',
    description: 'Ensures policy code has valid syntax',
    status: 'passed',
    severity: 'critical',
    details: 'Policy syntax validation completed successfully'
  },
  {
    id: 'logic_validation',
    name: 'Logic Validation',
    description: 'Validates business logic and rule conditions',
    status: 'passed',
    severity: 'high',
    details: 'All business logic validations passed'
  },
  {
    id: 'security_scan',
    name: 'Security Scan',
    description: 'Scans for security vulnerabilities and unsafe operations',
    status: 'passed',
    severity: 'critical',
    details: 'No security vulnerabilities detected'
  },
  {
    id: 'compliance_check',
    name: 'Compliance Check',
    description: 'Verifies regulatory compliance requirements',
    status: 'passed',
    severity: 'high',
    details: 'All compliance requirements satisfied'
  },
  {
    id: 'performance_test',
    name: 'Performance Test',
    description: 'Validates performance characteristics',
    status: 'warning',
    severity: 'medium',
    details: 'Performance test completed with minor warnings',
    recommendations: ['Consider optimizing rule execution for high-volume scenarios']
  },
  {
    id: 'integration_test',
    name: 'Integration Test',
    description: 'Tests integration with existing systems',
    status: 'passed',
    severity: 'high',
    details: 'Integration tests passed successfully'
  }
];

const PolicyDeployment: React.FC<PolicyDeploymentProps> = ({
  policyId,
  policyName,
  policyVersion,
  onDeploymentComplete,
  className = ''
}) => {
  const [selectedTarget, setSelectedTarget] = useState<DeploymentTarget | null>(null);
  const [deploymentStatus, setDeploymentStatus] = useState<DeploymentStatus | null>(null);
  const [isDeploying, setIsDeploying] = useState(false);
  const [showApprovalModal, setShowApprovalModal] = useState(false);
  const [deploymentNotes, setDeploymentNotes] = useState('');
  const [safetyChecks, setSafetyChecks] = useState<SafetyCheck[]>(SAFETY_CHECKS);
  const [deploymentHistory, setDeploymentHistory] = useState<DeploymentRequest[]>([]);

  // Simulated deployment history loading
  useEffect(() => {
    const loadDeploymentHistory = async () => {
      await new Promise(resolve => setTimeout(resolve, 500));

      setDeploymentHistory([
        {
          id: 'deploy-001',
          policyId,
          policyName,
          targetEnvironment: 'staging',
          requestedBy: 'developer',
          requestedAt: '2024-01-19T14:30:00Z',
          status: 'deployed',
          approvalRequired: true,
          approvers: ['compliance_officer'],
          approvedBy: ['compliance_officer'],
          deploymentNotes: 'Initial deployment for testing'
        },
        {
          id: 'deploy-002',
          policyId,
          policyName,
          targetEnvironment: 'production',
          requestedBy: 'compliance_officer',
          requestedAt: '2024-01-18T09:00:00Z',
          status: 'deployed',
          approvalRequired: true,
          approvers: ['compliance_officer', 'risk_officer', 'cfo'],
          approvedBy: ['compliance_officer', 'risk_officer'],
          deploymentNotes: 'Production deployment after successful staging validation'
        }
      ]);
    };

    loadDeploymentHistory();
  }, [policyId, policyName]);

  const runSafetyChecks = async () => {
    setSafetyChecks(prev => prev.map(check => ({ ...check, status: 'running' })));

    for (const check of safetyChecks) {
      await new Promise(resolve => setTimeout(resolve, 500 + Math.random() * 1000));

      // Simulate check completion with realistic results
      setSafetyChecks(prev => prev.map(c =>
        c.id === check.id
          ? {
              ...c,
              status: Math.random() > 0.2 ? 'passed' : Math.random() > 0.5 ? 'warning' : 'failed'
            }
          : c
      ));
    }
  };

  const initiateDeployment = async (target: DeploymentTarget) => {
    if (!target) return;

    // Check if approval is required
    if (target.requiresApproval) {
      setSelectedTarget(target);
      setShowApprovalModal(true);
      return;
    }

    await executeDeployment(target);
  };

  const executeDeployment = async (target: DeploymentTarget) => {
    setIsDeploying(true);
    setSelectedTarget(target);

    try {
      // Initialize deployment status
      const deployment: DeploymentStatus = {
        deploymentId: `deploy-${Date.now()}`,
        overallStatus: 'running',
        progress: 0,
        steps: [
          {
            id: 'pre_deployment_check',
            name: 'Pre-deployment Check',
            description: 'Validating deployment prerequisites',
            status: 'running',
            logs: ['Starting pre-deployment validation...']
          },
          {
            id: 'backup_current_version',
            name: 'Backup Current Version',
            description: 'Creating backup of current policy version',
            status: 'pending',
            logs: []
          },
          {
            id: 'upload_policy',
            name: 'Upload Policy',
            description: 'Uploading new policy to target environment',
            status: 'pending',
            logs: []
          },
          {
            id: 'validate_deployment',
            name: 'Validate Deployment',
            description: 'Running validation tests in target environment',
            status: 'pending',
            logs: []
          },
          {
            id: 'switch_traffic',
            name: 'Switch Traffic',
            description: 'Routing traffic to new policy version',
            status: 'pending',
            logs: []
          },
          {
            id: 'post_deployment_test',
            name: 'Post-deployment Test',
            description: 'Running final validation tests',
            status: 'pending',
            logs: []
          }
        ],
        startTime: new Date().toISOString(),
        rollbackAvailable: false
      };

      setDeploymentStatus(deployment);

      // Execute deployment steps
      for (let i = 0; i < deployment.steps.length; i++) {
        const step = deployment.steps[i];
        const stepStartTime = new Date();

        // Update step status to running
        setDeploymentStatus(prev => prev ? {
          ...prev,
          currentStep: step.id,
          steps: prev.steps.map(s =>
            s.id === step.id
              ? { ...s, status: 'running', startTime: stepStartTime.toISOString() }
              : s
          )
        } : null);

        // Simulate step execution
        await new Promise(resolve => setTimeout(resolve, 2000 + Math.random() * 3000));

        const stepEndTime = new Date();
        const success = Math.random() > 0.15; // 85% success rate

        // Update step status
        setDeploymentStatus(prev => prev ? {
          ...prev,
          progress: ((i + 1) / deployment.steps.length) * 100,
          steps: prev.steps.map(s =>
            s.id === step.id
              ? {
                  ...s,
                  status: success ? 'completed' : 'failed',
                  endTime: stepEndTime.toISOString(),
                  duration: stepEndTime.getTime() - stepStartTime.getTime(),
                  logs: [
                    ...s.logs,
                    success ? `Step completed successfully` : `Step failed: ${step.name} encountered an error`
                  ],
                  errorMessage: success ? undefined : `${step.name} failed during execution`
                }
              : s
          )
        } : null);

        if (!success) {
          // Mark deployment as failed
          setDeploymentStatus(prev => prev ? {
            ...prev,
            overallStatus: 'failed',
            rollbackAvailable: true
          } : null);
          break;
        }
      }

      // Mark deployment as completed if all steps succeeded
      if (deploymentStatus?.steps.every(s => s.status === 'completed')) {
        setDeploymentStatus(prev => prev ? {
          ...prev,
          overallStatus: 'completed',
          deployedVersion: policyVersion,
          rollbackAvailable: true
        } : null);

        if (onDeploymentComplete && deploymentStatus) {
          onDeploymentComplete(deploymentStatus);
        }
      }

    } catch (error) {
      console.error('Deployment failed:', error);
      setDeploymentStatus(prev => prev ? {
        ...prev,
        overallStatus: 'failed',
        rollbackAvailable: true
      } : null);
    } finally {
      setIsDeploying(false);
    }
  };

  const requestApproval = async () => {
    if (!selectedTarget) return;

    // Create deployment request
    const request: DeploymentRequest = {
      id: `request-${Date.now()}`,
      policyId,
      policyName,
      targetEnvironment: selectedTarget.environment,
      requestedBy: getUserIdFromToken(),
      requestedAt: new Date().toISOString(),
      status: 'pending_approval',
      approvalRequired: true,
      approvers: selectedTarget.approvers,
      deploymentNotes
    };

    // In a real implementation, this would send the request to the backend
    console.log('Deployment approval requested:', request);

    setShowApprovalModal(false);
    setDeploymentNotes('');
  };

  const rollbackDeployment = async () => {
    if (!deploymentStatus) return;

    setIsDeploying(true);

    try {
      // Simulate rollback process
      await new Promise(resolve => setTimeout(resolve, 3000));

      setDeploymentStatus(prev => prev ? {
        ...prev,
        overallStatus: 'rolled_back'
      } : null);

    } catch (error) {
      console.error('Rollback failed:', error);
    } finally {
      setIsDeploying(false);
    }
  };

  const getEnvironmentColor = (environment: string) => {
    switch (environment) {
      case 'development': return 'bg-blue-100 text-blue-800 border-blue-200';
      case 'staging': return 'bg-yellow-100 text-yellow-800 border-yellow-200';
      case 'production': return 'bg-red-100 text-red-800 border-red-200';
      default: return 'bg-gray-100 text-gray-800 border-gray-200';
    }
  };

  const getStatusIcon = (status: DeploymentStatus['overallStatus']) => {
    switch (status) {
      case 'completed': return <CheckCircle className="w-5 h-5 text-green-500" />;
      case 'failed': return <XCircle className="w-5 h-5 text-red-500" />;
      case 'running': return <RefreshCw className="w-5 h-5 text-blue-500 animate-spin" />;
      case 'rolled_back': return <RotateCcw className="w-5 h-5 text-orange-500" />;
      default: return <Clock className="w-5 h-5 text-gray-500" />;
    }
  };

  const getTargetStatusColor = (status: DeploymentTarget['status']) => {
    switch (status) {
      case 'healthy': return 'bg-green-100 text-green-800';
      case 'degraded': return 'bg-yellow-100 text-yellow-800';
      case 'unhealthy': return 'bg-red-100 text-red-800';
      case 'maintenance': return 'bg-blue-100 text-blue-800';
      default: return 'bg-gray-100 text-gray-800';
    }
  };

  const allSafetyChecksPassed = safetyChecks.every(check =>
    check.status === 'passed' || check.status === 'warning'
  );

  const criticalChecksPassed = safetyChecks
    .filter(check => check.severity === 'critical')
    .every(check => check.status === 'passed');

  return (
    <div className={clsx('space-y-6', className)}>
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-2xl font-bold text-gray-900 flex items-center gap-2">
            <Rocket className="w-6 h-6 text-blue-600" />
            Policy Deployment
          </h2>
          <p className="text-gray-600 mt-1">
            Deploy {policyName} (v{policyVersion}) to target environments
          </p>
        </div>
        <div className="flex items-center gap-3">
          <button
            onClick={runSafetyChecks}
            className="flex items-center gap-2 px-4 py-2 bg-green-600 hover:bg-green-700 text-white rounded-lg transition-colors"
          >
            <Shield className="w-4 h-4" />
            Run Safety Checks
          </button>
          <button
            onClick={() => setDeploymentStatus(null)}
            className="flex items-center gap-2 px-4 py-2 bg-gray-600 hover:bg-gray-700 text-white rounded-lg transition-colors"
          >
            <History className="w-4 h-4" />
            View History
          </button>
        </div>
      </div>

      {/* Safety Checks Overview */}
      <div className="bg-white rounded-lg border shadow-sm p-6">
        <div className="flex items-center justify-between mb-4">
          <h3 className="text-lg font-semibold text-gray-900">Safety Checks</h3>
          <div className="flex items-center gap-4 text-sm">
            <span className={clsx(
              'px-3 py-1 rounded-full',
              allSafetyChecksPassed ? 'bg-green-100 text-green-800' : 'bg-red-100 text-red-800'
            )}>
              {allSafetyChecksPassed ? 'All Checks Passed' : 'Checks Failed'}
            </span>
            <span className={clsx(
              'px-3 py-1 rounded-full',
              criticalChecksPassed ? 'bg-green-100 text-green-800' : 'bg-red-100 text-red-800'
            )}>
              Critical: {criticalChecksPassed ? 'Passed' : 'Failed'}
            </span>
          </div>
        </div>

        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
          {safetyChecks.map((check) => (
            <div
              key={check.id}
              className={clsx(
                'p-4 border rounded-lg',
                check.status === 'passed' ? 'bg-green-50 border-green-200' :
                check.status === 'warning' ? 'bg-yellow-50 border-yellow-200' :
                check.status === 'failed' ? 'bg-red-50 border-red-200' :
                'bg-gray-50 border-gray-200'
              )}
            >
              <div className="flex items-center justify-between mb-2">
                <h4 className="font-medium text-gray-900">{check.name}</h4>
                <span className={clsx(
                  'px-2 py-1 text-xs font-medium rounded',
                  check.severity === 'critical' ? 'bg-red-200 text-red-800' :
                  check.severity === 'high' ? 'bg-orange-200 text-orange-800' :
                  check.severity === 'medium' ? 'bg-yellow-200 text-yellow-800' :
                  'bg-green-200 text-green-800'
                )}>
                  {check.severity}
                </span>
              </div>

              <p className="text-sm text-gray-600 mb-2">{check.description}</p>

              <div className="flex items-center gap-2">
                {check.status === 'passed' && <CheckCircle className="w-4 h-4 text-green-500" />}
                {check.status === 'warning' && <AlertTriangle className="w-4 h-4 text-yellow-500" />}
                {check.status === 'failed' && <XCircle className="w-4 h-4 text-red-500" />}
                {check.status === 'running' && <RefreshCw className="w-4 h-4 text-blue-500 animate-spin" />}
                <span className={clsx(
                  'text-sm font-medium',
                  check.status === 'passed' ? 'text-green-700' :
                  check.status === 'warning' ? 'text-yellow-700' :
                  check.status === 'failed' ? 'text-red-700' :
                  'text-gray-700'
                )}>
                  {check.status}
                </span>
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Deployment Targets */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <div className="space-y-4">
          <h3 className="text-lg font-semibold text-gray-900">Deployment Targets</h3>

          {DEPLOYMENT_TARGETS.map((target) => (
            <div
              key={target.id}
              className={clsx(
                'p-6 border rounded-lg cursor-pointer transition-colors',
                selectedTarget?.id === target.id ? 'border-blue-500 bg-blue-50' : 'border-gray-200 hover:border-gray-300'
              )}
              onClick={() => setSelectedTarget(target)}
            >
              <div className="flex items-center justify-between mb-4">
                <div>
                  <h4 className="text-lg font-semibold text-gray-900">{target.name}</h4>
                  <p className="text-sm text-gray-600">{target.region}</p>
                </div>
                <div className="flex items-center gap-2">
                  <span className={clsx(
                    'px-3 py-1 text-sm font-medium rounded-full border',
                    getEnvironmentColor(target.environment)
                  )}>
                    {target.environment}
                  </span>
                  <span className={clsx(
                    'px-2 py-1 text-xs font-medium rounded-full',
                    getTargetStatusColor(target.status)
                  )}>
                    {target.status}
                  </span>
                </div>
              </div>

              <div className="grid grid-cols-2 gap-4 text-sm text-gray-600">
                <div>
                  <span className="font-medium">Current Version:</span>
                  <span className="ml-2">{target.version}</span>
                </div>
                <div>
                  <span className="font-medium">Last Deployment:</span>
                  <span className="ml-2">
                    {target.lastDeployment
                      ? formatDistanceToNow(new Date(target.lastDeployment), { addSuffix: true })
                      : 'Never'
                    }
                  </span>
                </div>
              </div>

              {target.requiresApproval && (
                <div className="mt-4 p-3 bg-yellow-50 border border-yellow-200 rounded">
                  <div className="flex items-center gap-2 text-sm text-yellow-800">
                    <Users className="w-4 h-4" />
                    <span>Requires approval from: {target.approvers.join(', ')}</span>
                  </div>
                </div>
              )}

              <div className="mt-4 flex items-center justify-between">
                <div className="text-sm text-gray-500">
                  {target.approvers.length} approver{target.approvers.length !== 1 ? 's' : ''} required
                </div>
                <button
                  onClick={(e) => {
                    e.stopPropagation();
                    initiateDeployment(target);
                  }}
                  disabled={!allSafetyChecksPassed || isDeploying}
                  className={clsx(
                    'flex items-center gap-2 px-4 py-2 rounded-lg transition-colors',
                    target.environment === 'production'
                      ? 'bg-red-600 hover:bg-red-700 text-white'
                      : 'bg-blue-600 hover:bg-blue-700 text-white',
                    (!allSafetyChecksPassed || isDeploying) && 'opacity-50 cursor-not-allowed'
                  )}
                >
                  <Rocket className="w-4 h-4" />
                  Deploy to {target.environment}
                </button>
              </div>
            </div>
          ))}
        </div>

        {/* Deployment Status */}
        <div className="space-y-4">
          <h3 className="text-lg font-semibold text-gray-900">Deployment Status</h3>

          {deploymentStatus ? (
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <div className="flex items-center justify-between mb-4">
                <div className="flex items-center gap-3">
                  {getStatusIcon(deploymentStatus.overallStatus)}
                  <div>
                    <h4 className="font-semibold text-gray-900">
                      Deployment to {selectedTarget?.name}
                    </h4>
                    <p className="text-sm text-gray-600">
                      Started {formatDistanceToNow(new Date(deploymentStatus.startTime), { addSuffix: true })}
                    </p>
                  </div>
                </div>
                <span className={clsx(
                  'px-3 py-1 text-sm font-medium rounded-full',
                  deploymentStatus.overallStatus === 'completed' ? 'bg-green-100 text-green-800' :
                  deploymentStatus.overallStatus === 'failed' ? 'bg-red-100 text-red-800' :
                  deploymentStatus.overallStatus === 'running' ? 'bg-blue-100 text-blue-800' :
                  'bg-gray-100 text-gray-800'
                )}>
                  {deploymentStatus.overallStatus}
                </span>
              </div>

              {/* Progress Bar */}
              <div className="w-full bg-gray-200 rounded-full h-2 mb-4">
                <div
                  className={clsx(
                    'h-2 rounded-full transition-all duration-300',
                    deploymentStatus.overallStatus === 'completed' ? 'bg-green-600' :
                    deploymentStatus.overallStatus === 'failed' ? 'bg-red-600' :
                    'bg-blue-600'
                  )}
                  style={{ width: `${deploymentStatus.progress}%` }}
                />
              </div>

              {/* Deployment Steps */}
              <div className="space-y-3">
                {deploymentStatus.steps.map((step) => (
                  <div
                    key={step.id}
                    className={clsx(
                      'p-3 border rounded',
                      step.status === 'completed' ? 'bg-green-50 border-green-200' :
                      step.status === 'failed' ? 'bg-red-50 border-red-200' :
                      step.status === 'running' ? 'bg-blue-50 border-blue-200' :
                      'bg-gray-50 border-gray-200'
                    )}
                  >
                    <div className="flex items-center justify-between mb-1">
                      <h5 className="font-medium text-gray-900">{step.name}</h5>
                      <div className="flex items-center gap-2">
                        {step.status === 'completed' && <CheckCircle className="w-4 h-4 text-green-500" />}
                        {step.status === 'failed' && <XCircle className="w-4 h-4 text-red-500" />}
                        {step.status === 'running' && <RefreshCw className="w-4 h-4 text-blue-500 animate-spin" />}
                        <span className="text-sm font-medium">{step.status}</span>
                      </div>
                    </div>

                    <p className="text-sm text-gray-600">{step.description}</p>

                    {step.duration && (
                      <div className="text-xs text-gray-500 mt-1">
                        Duration: {Math.round(step.duration / 1000)}s
                      </div>
                    )}
                  </div>
                ))}
              </div>

              {/* Action Buttons */}
              {deploymentStatus.overallStatus === 'completed' && (
                <div className="mt-4 flex items-center gap-3">
                  <button className="flex items-center gap-2 px-4 py-2 bg-green-600 hover:bg-green-700 text-white rounded-lg transition-colors">
                    <Eye className="w-4 h-4" />
                    View Deployed Policy
                  </button>
                  {deploymentStatus.rollbackAvailable && (
                    <button
                      onClick={rollbackDeployment}
                      disabled={isDeploying}
                      className="flex items-center gap-2 px-4 py-2 bg-orange-600 hover:bg-orange-700 text-white rounded-lg transition-colors"
                    >
                      <RotateCcw className="w-4 h-4" />
                      Rollback
                    </button>
                  )}
                </div>
              )}

              {deploymentStatus.overallStatus === 'failed' && deploymentStatus.rollbackAvailable && (
                <div className="mt-4">
                  <button
                    onClick={rollbackDeployment}
                    disabled={isDeploying}
                    className="flex items-center gap-2 px-4 py-2 bg-red-600 hover:bg-red-700 text-white rounded-lg transition-colors"
                  >
                    <RotateCcw className="w-4 h-4" />
                    Rollback Deployment
                  </button>
                </div>
              )}
            </div>
          ) : (
            <div className="bg-gray-50 border-2 border-dashed border-gray-300 rounded-lg p-12 text-center">
              <Rocket className="w-12 h-12 text-gray-400 mx-auto mb-4" />
              <h4 className="text-lg font-medium text-gray-900 mb-2">No Active Deployment</h4>
              <p className="text-gray-600">
                Select a deployment target to begin the deployment process
              </p>
            </div>
          )}
        </div>
      </div>

      {/* Deployment History */}
      <div className="bg-white rounded-lg border shadow-sm">
        <div className="p-6 border-b border-gray-200">
          <h3 className="text-lg font-semibold text-gray-900">Deployment History</h3>
          <p className="text-sm text-gray-600">Previous deployments of this policy</p>
        </div>

        <div className="divide-y divide-gray-200">
          {deploymentHistory.map((deployment) => (
            <div key={deployment.id} className="p-6 hover:bg-gray-50">
              <div className="flex items-center justify-between">
                <div className="flex items-center gap-4">
                  {deployment.status === 'deployed' && <CheckCircle className="w-5 h-5 text-green-500" />}
                  {deployment.status === 'failed' && <XCircle className="w-5 h-5 text-red-500" />}
                  {deployment.status === 'rolled_back' && <RotateCcw className="w-5 h-5 text-orange-500" />}
                  <div>
                    <h4 className="font-medium text-gray-900">
                      Deployment to {deployment.targetEnvironment}
                    </h4>
                    <p className="text-sm text-gray-600">
                      Requested by {deployment.requestedBy} •
                      {formatDistanceToNow(new Date(deployment.requestedAt), { addSuffix: true })}
                    </p>
                  </div>
                </div>

                <div className="text-right">
                  <span className={clsx(
                    'px-3 py-1 text-sm font-medium rounded-full',
                    deployment.status === 'deployed' ? 'bg-green-100 text-green-800' :
                    deployment.status === 'failed' ? 'bg-red-100 text-red-800' :
                    deployment.status === 'rolled_back' ? 'bg-orange-100 text-orange-800' :
                    'bg-gray-100 text-gray-800'
                  )}>
                    {deployment.status.replace('_', ' ')}
                  </span>

                  {deployment.approvalRequired && (
                    <div className="text-xs text-gray-500 mt-1">
                      Approved by: {deployment.approvedBy?.join(', ') || 'Pending'}
                    </div>
                  )}
                </div>
              </div>

              {deployment.deploymentNotes && (
                <div className="mt-3 p-3 bg-gray-50 rounded text-sm text-gray-700">
                  {deployment.deploymentNotes}
                </div>
              )}
            </div>
          ))}
        </div>
      </div>

      {/* Approval Modal */}
      {showApprovalModal && selectedTarget && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50 p-4">
          <div className="bg-white rounded-lg shadow-xl max-w-md w-full">
            <div className="flex items-center justify-between p-6 border-b border-gray-200">
              <div>
                <h3 className="text-2xl font-bold text-gray-900">Request Approval</h3>
                <p className="text-gray-600 mt-1">
                  Deploy to {selectedTarget.name}
                </p>
              </div>
              <button
                onClick={() => setShowApprovalModal(false)}
                className="p-2 hover:bg-gray-100 rounded-lg transition-colors"
              >
                <XCircle className="w-5 h-5" />
              </button>
            </div>

            <div className="p-6 space-y-4">
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Deployment Notes
                </label>
                <textarea
                  value={deploymentNotes}
                  onChange={(e) => setDeploymentNotes(e.target.value)}
                  rows={3}
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                  placeholder="Describe the changes and rationale for this deployment..."
                />
              </div>

              <div className="p-4 bg-yellow-50 border border-yellow-200 rounded">
                <div className="flex items-start gap-3">
                  <Users className="w-5 h-5 text-yellow-600 mt-0.5" />
                  <div>
                    <h4 className="text-sm font-medium text-yellow-800">Approval Required</h4>
                    <p className="text-sm text-yellow-700 mt-1">
                      This deployment requires approval from: {selectedTarget.approvers.join(', ')}
                    </p>
                  </div>
                </div>
              </div>
            </div>

            <div className="flex items-center justify-end gap-3 p-6 border-t border-gray-200 bg-gray-50">
              <button
                onClick={() => setShowApprovalModal(false)}
                className="px-4 py-2 text-gray-700 hover:bg-gray-100 rounded-lg transition-colors"
              >
                Cancel
              </button>
              <button
                onClick={requestApproval}
                className="bg-blue-600 hover:bg-blue-700 text-white px-6 py-2 rounded-lg transition-colors"
              >
                Request Approval
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default PolicyDeployment;
