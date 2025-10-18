/**
 * API Service - Production-grade API client
 */

import axios from 'axios';

const API_BASE_URL = import.meta.env.VITE_API_BASE_URL || 'http://localhost:8080';

const api = axios.create({
  baseURL: API_BASE_URL,
  headers: {
    'Content-Type': 'application/json',
  },
});

// Request interceptor for authentication
api.interceptors.request.use((config) => {
  const token = localStorage.getItem('accessToken');
  if (token) {
    config.headers.Authorization = `Bearer ${token}`;
  }
  return config;
});

// Response interceptor for error handling
api.interceptors.response.use(
  (response) => response,
  (error) => {
    if (error.response?.status === 401) {
      localStorage.removeItem('accessToken');
      window.location.href = '/login';
    }
    return Promise.reject(error);
  }
);

export interface LoginRequest {
  username: string;
  password: string;
}

export interface LoginResponse {
  success: boolean;
  accessToken?: string;
  refreshToken?: string;
  tokenType?: string;
  expiresIn?: number;
  user?: {
    id: number;
    username: string;
    permissions: string[];
  };
  error?: string;
}

export interface DashboardData {
  compliance_score: number;
  alerts_count: number;
  transactions_today: number;
  risk_assessments: number;
  active_agents: number;
  system_health: string;
}

export interface Transaction {
  id: string;
  amount: number;
  status: string;
  type: string;
  timestamp: string;
}

export interface Agent {
  id: string;
  name: string;
  type: string;
  status: string;
  last_active: string;
}

export interface Alert {
  id: string;
  severity: string;
  message: string;
  timestamp: string;
}

// Auth API
export const authAPI = {
  login: async (credentials: LoginRequest): Promise<LoginResponse> => {
    const response = await api.post('/api/auth/login', credentials);
    return response.data;
  },

  logout: () => {
    localStorage.removeItem('accessToken');
    localStorage.removeItem('user');
  },
};

// Dashboard API
export const dashboardAPI = {
  getData: async (): Promise<DashboardData> => {
    const response = await api.get('/api/dashboard');
    return response.data.data;
  },
};

// Transactions API
export const transactionsAPI = {
  getTransactions: async (): Promise<Transaction[]> => {
    const response = await api.get('/api/transactions');
    return response.data.data;
  },
};

// Agents API
export const agentsAPI = {
  getAgents: async (): Promise<Agent[]> => {
    const response = await api.get('/api/agents');
    return response.data.data;
  },
};

// Alerts API
export const alertsAPI = {
  getAlerts: async (): Promise<Alert[]> => {
    const response = await api.get('/api/alerts');
    return response.data.data;
  },
};

// Memory API
export const memoryAPI = {
  getVisualization: async () => {
    const response = await api.get('/api/memory/visualize');
    return response.data;
  },
};

// Regulatory Changes API
export const regulatoryAPI = {
  getChanges: async () => {
    const response = await api.get('/api/regulatory-changes');
    return response.data.data;
  },
};

// Audit Trail API
export const auditAPI = {
  getTrail: async () => {
    const response = await api.get('/api/audit-trail');
    return response.data.data;
  },
};

// ============================================================================
// PHASE 7A: ADVANCED ANALYTICS API
// ============================================================================

export const analyticsAPI = {
  // Decision Analytics
  getAlgorithmComparison: async (algorithms: string[], days: number = 30) => {
    const response = await api.post('/api/analytics/decision/algorithm-comparison', {
      algorithms,
    }, { params: { days } });
    return response.data.data;
  },

  getDecisionAccuracyTimeline: async (algorithm: string, days: number = 30) => {
    const response = await api.get('/api/analytics/decision/accuracy-timeline', {
      params: { algorithm, days },
    });
    return response.data.data;
  },

  getEnsembleComparison: async (days: number = 30) => {
    const response = await api.get('/api/analytics/decision/ensemble-comparison', {
      params: { days },
    });
    return response.data.data;
  },

  getDecisionStats: async (days: number = 30) => {
    const response = await api.get('/api/analytics/decision/stats', {
      params: { days },
    });
    return response.data.data;
  },

  recordDecision: async (decisionData: any) => {
    const response = await api.post('/api/analytics/decision/record', decisionData);
    return response.data.data;
  },

  recordDecisionOutcome: async (decisionId: string, outcome: any) => {
    const response = await api.post(`/api/analytics/decision/${decisionId}/outcome`, outcome);
    return response.data.data;
  },

  // Rule Performance Analytics
  getRuleMetrics: async (ruleId: string) => {
    const response = await api.get('/api/analytics/rule/metrics', {
      params: { rule_id: ruleId },
    });
    return response.data.data;
  },

  getRedundantRules: async (threshold: number = 0.7) => {
    const response = await api.post('/api/analytics/rule/redundant', {
      similarity_threshold: threshold,
    });
    return response.data.data;
  },

  getHighFalsePositiveRules: async (limit: number = 10) => {
    const response = await api.get('/api/analytics/rule/high-fp-rules', {
      params: { limit },
    });
    return response.data.data;
  },

  getRuleStats: async (days: number = 30) => {
    const response = await api.get('/api/analytics/rule/stats', {
      params: { days },
    });
    return response.data.data;
  },

  recordRuleExecution: async (ruleId: string, executionData: any) => {
    const response = await api.post('/api/analytics/rule/execution', {
      rule_id: ruleId,
      ...executionData,
    });
    return response.data.data;
  },

  recordRuleOutcome: async (ruleId: string, outcomeData: any) => {
    const response = await api.post(`/api/analytics/rule/${ruleId}/outcome`, outcomeData);
    return response.data.data;
  },

  // Learning Insights Analytics
  getFeedbackEffectiveness: async (days: number = 30) => {
    const response = await api.get('/api/analytics/learning/feedback-effectiveness', {
      params: { days },
    });
    return response.data.data;
  },

  getRewardAnalysis: async (entityId: string = '', days: number = 30) => {
    const response = await api.get('/api/analytics/learning/reward-analysis', {
      params: { entity_id: entityId, days },
    });
    return response.data.data;
  },

  getFeatureImportance: async (limit: number = 20) => {
    const response = await api.get('/api/analytics/learning/feature-importance', {
      params: { limit },
    });
    return response.data.data;
  },

  getConvergenceStatus: async () => {
    const response = await api.get('/api/analytics/learning/convergence-status');
    return response.data.data;
  },

  getLearningRecommendations: async () => {
    const response = await api.get('/api/analytics/learning/recommendations');
    return response.data.data;
  },

  getLearningStats: async (days: number = 30) => {
    const response = await api.get('/api/analytics/learning/stats', {
      params: { days },
    });
    return response.data.data;
  },

  recordFeedback: async (feedbackData: any) => {
    const response = await api.post('/api/analytics/learning/feedback', feedbackData);
    return response.data.data;
  },

  // Aggregated Dashboard
  getSystemAnalyticsDashboard: async (days: number = 30) => {
    const response = await api.get('/api/analytics/dashboard', {
      params: { days },
    });
    return response.data.data;
  },

  getAnalyticsHealth: async () => {
    const response = await api.get('/api/analytics/health');
    return response.data.data;
  },
};

// ============================================================================
// PHASE 7B: SECURITY & COMPLIANCE API
// ============================================================================

export const securityAPI = {
  // RBAC Management
  createRole: async (roleData: any) => {
    const response = await api.post('/api/security/rbac/roles', roleData);
    return response.data.data;
  },

  updateRole: async (roleId: string, roleData: any) => {
    const response = await api.put(`/api/security/rbac/roles/${roleId}`, roleData);
    return response.data.data;
  },

  deleteRole: async (roleId: string) => {
    const response = await api.delete(`/api/security/rbac/roles/${roleId}`);
    return response.data.data;
  },

  getAllRoles: async () => {
    const response = await api.get('/api/security/rbac/roles');
    return response.data.data;
  },

  assignUserRole: async (userId: string, roleId: string, assignmentData: any) => {
    const response = await api.post('/api/security/rbac/assignments', {
      user_id: userId,
      role_id: roleId,
      ...assignmentData,
    });
    return response.data.data;
  },

  getUserRoles: async (userId: string) => {
    const response = await api.get(`/api/security/rbac/users/${userId}/roles`);
    return response.data.data;
  },

  revokeUserRole: async (userId: string, roleId: string) => {
    const response = await api.delete(`/api/security/rbac/assignments/${userId}/${roleId}`);
    return response.data.data;
  },

  checkAccess: async (resourceId: string, resourceType: string, action: string) => {
    const response = await api.post('/api/security/rbac/check-access', {
      resource_id: resourceId,
      resource_type: resourceType,
      action,
    });
    return response.data.data;
  },

  getRBACStats: async () => {
    const response = await api.get('/api/security/rbac/stats');
    return response.data.data;
  },

  // Audit Trail
  getEntityHistory: async (entityId: string, entityType: string, days: number = 90) => {
    const response = await api.get(`/api/security/audit/history/${entityId}`, {
      params: { entity_type: entityType, days },
    });
    return response.data.data;
  },

  getChangeDetails: async (changeId: string) => {
    const response = await api.get(`/api/security/audit/changes/${changeId}`);
    return response.data.data;
  },

  getHighImpactChanges: async (days: number = 7) => {
    const response = await api.get('/api/security/audit/high-impact-changes', {
      params: { days },
    });
    return response.data.data;
  },

  submitRollback: async (changeId: string, reason: string) => {
    const response = await api.post('/api/security/audit/rollback', {
      target_change_id: changeId,
      reason,
    });
    return response.data.data;
  },

  executeRollback: async (rollbackId: string) => {
    const response = await api.post(`/api/security/audit/rollback/${rollbackId}/execute`);
    return response.data.data;
  },

  getAuditStats: async (days: number = 30) => {
    const response = await api.get('/api/security/audit/stats', {
      params: { days },
    });
    return response.data.data;
  },

  generateAuditReport: async (days: number = 90) => {
    const response = await api.get('/api/security/audit/report', {
      params: { days },
    });
    return response.data.data;
  },

  generateSOC2Report: async (days: number = 365) => {
    const response = await api.get('/api/security/audit/soc2-report', {
      params: { days },
    });
    return response.data.data;
  },

  generateGDPRReport: async (days: number = 90) => {
    const response = await api.get('/api/security/compliance/gdpr-report', {
      params: { days },
    });
    return response.data.data;
  },

  // Data Encryption & Privacy
  detectPII: async (data: any) => {
    const response = await api.post('/api/security/encryption/detect-pii', { data });
    return response.data.data;
  },

  maskPII: async (data: any, piiTypes: string[] = []) => {
    const response = await api.post('/api/security/encryption/mask-pii', {
      data,
      pii_types: piiTypes,
    });
    return response.data.data;
  },

  encryptData: async (plaintext: string, keyId: string) => {
    const response = await api.post('/api/security/encryption/encrypt', {
      plaintext,
      key_id: keyId,
    });
    return response.data.data;
  },

  recordGDPRConsent: async (userId: string, dataPurpose: string) => {
    const response = await api.post('/api/security/encryption/consent', {
      user_id: userId,
      data_purpose: dataPurpose,
      consent_given: true,
    });
    return response.data.data;
  },

  exportUserData: async (userId: string) => {
    const response = await api.get(`/api/security/encryption/export/${userId}`);
    return response.data.data;
  },

  deleteUserData: async (userId: string) => {
    const response = await api.delete(`/api/security/encryption/user/${userId}`);
    return response.data.data;
  },

  getEncryptionAuditLog: async (days: number = 30) => {
    const response = await api.get('/api/security/encryption/audit-log', {
      params: { days },
    });
    return response.data.data;
  },
};

export default api;