/**
 * Production API Client for Regulens
 * Real axios-based HTTP client - NO MOCKS, NO STUBS
 * Connects directly to C++ backend APIs
 */

import axios, { AxiosInstance, AxiosError, InternalAxiosRequestConfig } from 'axios';
import type * as API from '@/types/api';

class RegulesAPIClient {
  private client: AxiosInstance;
  private token: string | null = null;

  constructor(baseURL: string = '/api') {
    this.client = axios.create({
      baseURL,
      timeout: 30000,
      headers: {
        'Content-Type': 'application/json',
      },
    });

    this.setupInterceptors();
    this.loadTokenFromStorage();
  }

  private setupInterceptors(): void {
    // Request interceptor - add auth token
    this.client.interceptors.request.use(
      (config: InternalAxiosRequestConfig) => {
        if (this.token && config.headers) {
          config.headers.Authorization = `Bearer ${this.token}`;
        }
        return config;
      },
      (error: AxiosError) => Promise.reject(error)
    );

    // Response interceptor - handle errors
    this.client.interceptors.response.use(
      (response) => response,
      (error: AxiosError<API.APIError>) => {
        if (error.response?.status === 401) {
          this.clearToken();
          window.dispatchEvent(new Event('auth:logout'));
        }
        return Promise.reject(error);
      }
    );
  }

  private loadTokenFromStorage(): void {
    const stored = localStorage.getItem('regulens_token');
    if (stored) {
      this.token = stored;
    }
  }

  setToken(token: string): void {
    this.token = token;
    localStorage.setItem('regulens_token', token);
  }

  clearToken(): void {
    this.token = null;
    localStorage.removeItem('regulens_token');
  }

  getToken(): string | null {
    return this.token;
  }

  // ============================================================================
  // AUTHENTICATION
  // ============================================================================

  async login(credentials: API.LoginRequest): Promise<API.LoginResponse> {
    const response = await this.client.post<API.LoginResponse>('/auth/login', credentials);
    this.setToken(response.data.token);
    return response.data;
  }

  async logout(): Promise<void> {
    await this.client.post('/auth/logout');
    this.clearToken();
  }

  async getCurrentUser(): Promise<API.User> {
    const response = await this.client.get<API.User>('/auth/me');
    return response.data;
  }

  // ============================================================================
  // ACTIVITY FEED
  // ============================================================================

  async getRecentActivity(limit: number = 50): Promise<API.ActivityItem[]> {
    const response = await this.client.get<API.ActivityItem[]>('/activity', {
      params: { limit },
    });
    return response.data;
  }

  async getActivityStats(): Promise<API.ActivityStats> {
    const response = await this.client.get<API.ActivityStats>('/activity/stats');
    return response.data;
  }

  // ============================================================================
  // DECISIONS
  // ============================================================================

  async getRecentDecisions(limit: number = 20): Promise<API.Decision[]> {
    const response = await this.client.get<API.Decision[]>('/decisions', {
      params: { limit },
    });
    return response.data;
  }

  async getDecisionById(id: string): Promise<API.Decision> {
    const response = await this.client.get<API.Decision>(`/decisions/${id}`);
    return response.data;
  }

  async getDecisionTree(decisionId: string): Promise<API.DecisionTree> {
    const response = await this.client.get<API.DecisionTree>('/decisions/tree', {
      params: { decisionId },
    });
    return response.data;
  }

  async visualizeDecision(data: {
    algorithm: API.MCDAAlgorithm;
    criteria: API.Criterion[];
    alternatives: API.Alternative[];
  }): Promise<API.DecisionTree> {
    const response = await this.client.post<API.DecisionTree>('/decisions/visualize', data);
    return response.data;
  }

  async createDecision(decision: Partial<API.Decision>): Promise<API.Decision> {
    const response = await this.client.post<API.Decision>('/decisions', decision);
    return response.data;
  }

  // ============================================================================
  // REGULATORY MONITORING
  // ============================================================================

  async getRegulatoryChanges(params?: {
    from?: string;
    to?: string;
    source?: string;
    severity?: string;
  }): Promise<API.RegulatoryChange[]> {
    const response = await this.client.get<API.RegulatoryChange[]>('/regulatory-changes', { params });
    return response.data;
  }

  async getRegulatoryChange(id: string): Promise<API.RegulatoryChange> {
    const response = await this.client.get<API.RegulatoryChange>(`/regulatory-changes/${id}`);
    return response.data;
  }

  async getRegulatorySources(): Promise<API.RegulatorySource[]> {
    const response = await this.client.get<API.RegulatorySource[]>('/regulatory/sources');
    return response.data;
  }

  async getMonitorStatus(): Promise<API.MonitorStatus> {
    const response = await this.client.get<API.MonitorStatus>('/regulatory/monitor');
    return response.data;
  }

  async startMonitoring(): Promise<void> {
    await this.client.post('/regulatory/start');
  }

  async stopMonitoring(): Promise<void> {
    await this.client.post('/regulatory/stop');
  }

  // ============================================================================
  // AUDIT TRAIL
  // ============================================================================

  async getAuditTrail(params?: {
    from?: string;
    to?: string;
    eventType?: string;
    actor?: string;
    severity?: string;
    limit?: number;
  }): Promise<API.AuditEvent[]> {
    const response = await this.client.get<API.AuditEvent[]>('/audit-trail', { params });
    return response.data;
  }

  async getAuditAnalytics(): Promise<API.AuditAnalytics> {
    const response = await this.client.get<API.AuditAnalytics>('/audit/analytics');
    return response.data;
  }

  async exportAuditTrail(format: 'csv' | 'json'): Promise<Blob> {
    const response = await this.client.get('/audit/export', {
      params: { format },
      responseType: 'blob',
    });
    return response.data;
  }

  // ============================================================================
  // AGENTS
  // ============================================================================

  async getAgents(): Promise<API.Agent[]> {
    const response = await this.client.get<API.Agent[]>('/agents');
    return response.data;
  }

  async getAgentById(id: string): Promise<API.Agent> {
    const response = await this.client.get<API.Agent>(`/agents/${id}`);
    return response.data;
  }

  async getAgentStatus(): Promise<Record<string, string>> {
    const response = await this.client.get<Record<string, string>>('/agents/status');
    return response.data;
  }

  async executeAgentAction(agentId: string, action: string, params?: Record<string, unknown>): Promise<unknown> {
    const response = await this.client.post(`/agents/execute`, { agentId, action, params });
    return response.data;
  }

  async getAgentCommunication(limit: number = 50): Promise<API.AgentMessage[]> {
    const response = await this.client.get<API.AgentMessage[]>('/communication', {
      params: { limit },
    });
    return response.data;
  }

  // ============================================================================
  // COLLABORATION
  // ============================================================================

  async getCollaborationSessions(): Promise<API.CollaborationSession[]> {
    const response = await this.client.get<API.CollaborationSession[]>('/collaboration/sessions');
    return response.data;
  }

  async createCollaborationSession(topic: string): Promise<API.CollaborationSession> {
    const response = await this.client.post<API.CollaborationSession>('/collaboration/session/create', { topic });
    return response.data;
  }

  async getSessionMessages(sessionId: string): Promise<API.CollaborationMessage[]> {
    const response = await this.client.get<API.CollaborationMessage[]>('/collaboration/session/messages', {
      params: { sessionId },
    });
    return response.data;
  }

  async sendCollaborationMessage(sessionId: string, content: string): Promise<void> {
    await this.client.post('/collaboration/message/send', { sessionId, content });
  }

  async submitFeedback(feedback: Partial<API.Feedback>): Promise<API.Feedback> {
    const response = await this.client.post<API.Feedback>('/collaboration/feedback', feedback);
    return response.data;
  }

  async getFeedback(): Promise<API.Feedback[]> {
    const response = await this.client.get<API.Feedback[]>('/feedback');
    return response.data;
  }

  async getLearningInsights(): Promise<unknown> {
    const response = await this.client.get('/feedback/learning');
    return response.data;
  }

  // ============================================================================
  // PATTERN RECOGNITION
  // ============================================================================

  async getPatterns(): Promise<API.Pattern[]> {
    const response = await this.client.get<API.Pattern[]>('/patterns');
    return response.data;
  }

  async discoverPatterns(params: { timeframe: string; minConfidence: number }): Promise<API.Pattern[]> {
    const response = await this.client.post<API.Pattern[]>('/patterns/discover', params);
    return response.data;
  }

  async getPatternStats(): Promise<API.PatternStats> {
    const response = await this.client.get<API.PatternStats>('/patterns/stats');
    return response.data;
  }

  // ============================================================================
  // LLM INTEGRATION
  // ============================================================================

  async llmCompletion(request: API.LLMCompletionRequest): Promise<API.LLMCompletionResponse> {
    const response = await this.client.post<API.LLMCompletionResponse>('/llm/completion', request);
    return response.data;
  }

  async llmAnalysis(request: API.LLMAnalysisRequest): Promise<API.LLMAnalysisResponse> {
    const response = await this.client.post<API.LLMAnalysisResponse>('/llm/analysis', request);
    return response.data;
  }

  async llmComplianceCheck(document: string, framework: string): Promise<unknown> {
    const response = await this.client.post('/llm/compliance', { document, framework });
    return response.data;
  }

  // ============================================================================
  // TRANSACTION GUARDIAN
  // ============================================================================

  async getTransactions(params?: {
    from?: string;
    to?: string;
    riskLevel?: string;
    status?: string;
    limit?: number;
  }): Promise<API.Transaction[]> {
    const response = await this.client.get<API.Transaction[]>('/transactions', { params });
    return response.data;
  }

  async getTransactionById(id: string): Promise<API.Transaction> {
    const response = await this.client.get<API.Transaction>(`/transactions/${id}`);
    return response.data;
  }

  async getFraudRules(): Promise<API.FraudRule[]> {
    const response = await this.client.get<API.FraudRule[]>('/fraud/rules');
    return response.data;
  }

  async approveTransaction(id: string): Promise<void> {
    await this.client.post(`/transactions/${id}/approve`);
  }

  async rejectTransaction(id: string, reason: string): Promise<void> {
    await this.client.post(`/transactions/${id}/reject`, { reason });
  }

  // ============================================================================
  // DATA INGESTION
  // ============================================================================

  async getIngestionMetrics(): Promise<API.IngestionMetrics> {
    const response = await this.client.get<API.IngestionMetrics>('/ingestion/metrics');
    return response.data;
  }

  async getDataQualityChecks(): Promise<API.DataQualityCheck[]> {
    const response = await this.client.get<API.DataQualityCheck[]>('/ingestion/quality-checks');
    return response.data;
  }

  // ============================================================================
  // KNOWLEDGE BASE
  // ============================================================================

  async searchKnowledge(query: string): Promise<API.KnowledgeEntry[]> {
    const response = await this.client.get<API.KnowledgeEntry[]>('/knowledge/search', {
      params: { q: query },
    });
    return response.data;
  }

  async getCaseExamples(situation: string): Promise<API.CaseExample[]> {
    const response = await this.client.get<API.CaseExample[]>('/knowledge/cases', {
      params: { situation },
    });
    return response.data;
  }

  // ============================================================================
  // HEALTH & MONITORING
  // ============================================================================

  async getHealth(): Promise<API.HealthCheck> {
    const response = await this.client.get<API.HealthCheck>('/health');
    return response.data;
  }

  async getSystemMetrics(): Promise<API.SystemMetrics> {
    const response = await this.client.get<API.SystemMetrics>('/metrics/system');
    return response.data;
  }

  async getCircuitBreakerStatus(): Promise<API.CircuitBreakerStatus[]> {
    const response = await this.client.get<API.CircuitBreakerStatus[]>('/circuit-breaker/status');
    return response.data;
  }

  async getPrometheusMetrics(): Promise<string> {
    const response = await this.client.get<string>('/metrics', {
      headers: { Accept: 'text/plain' },
    });
    return response.data;
  }

  // ============================================================================
  // CONFIGURATION
  // ============================================================================

  async getConfig(): Promise<Record<string, unknown>> {
    const response = await this.client.get<Record<string, unknown>>('/config');
    return response.data;
  }

  async updateConfig(config: Record<string, unknown>): Promise<void> {
    await this.client.put('/config', config);
  }

  // ============================================================================
  // DATABASE
  // ============================================================================

  async testDatabaseConnection(): Promise<boolean> {
    const response = await this.client.get<{ connected: boolean }>('/db/test');
    return response.data.connected;
  }

  async getDatabaseStats(): Promise<Record<string, unknown>> {
    const response = await this.client.get<Record<string, unknown>>('/db/stats');
    return response.data;
  }
}

// Export singleton instance
export const apiClient = new RegulesAPIClient();
export default apiClient;
