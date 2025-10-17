/**
 * API Service - Production-grade API client
 * Connects to the temporary API server while C++ backend is being fixed
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

export default api;