/**
 * Production TypeScript types for Regulens API
 * These types match the actual C++ backend responses
 * NO MOCKS - Real type definitions from production API
 */

// ============================================================================
// AUTHENTICATION
// ============================================================================

export interface LoginRequest {
  username: string;
  password: string;
}

export interface LoginResponse {
  token: string;
  user: User;
  expiresIn: number;
}

export interface User {
  id: string;
  username: string;
  email: string;
  role: 'admin' | 'analyst' | 'viewer' | 'user';
  permissions: string[];
}

// ============================================================================
// ACTIVITY FEED
// ============================================================================

export interface ActivityItem {
  id: string;
  timestamp: string;
  type: 'regulatory_change' | 'decision_made' | 'data_ingestion' | 'agent_action' | 'compliance_alert';
  title: string;
  description: string;
  priority: 'low' | 'medium' | 'high' | 'critical';
  actor: string;
  metadata?: Record<string, unknown>;
}

export interface ActivityStats {
  total: number;
  byType: Record<string, number>;
  byPriority: Record<string, number>;
  last24Hours: number;
}

// ============================================================================
// MCDA DECISIONS
// ============================================================================

export type MCDAAlgorithm = 'TOPSIS' | 'ELECTRE_III' | 'PROMETHEE_II' | 'AHP' | 'VIKOR';

export interface Decision {
  id: string;
  timestamp: string;
  title: string;
  description: string;
  algorithm: MCDAAlgorithm;
  criteria: Criterion[];
  alternatives: Alternative[];
  selectedAlternative: string;
  confidenceScore: number;
  decisionMaker: string;
  approvedBy?: string;
  status: 'draft' | 'pending' | 'approved' | 'rejected' | 'implemented';
  metadata?: Record<string, unknown>;
}

export interface Criterion {
  name: string;
  weight: number;
  type: 'cost' | 'benefit';
  unit?: string;
}

export interface Alternative {
  id: string;
  name: string;
  scores: Record<string, number>;
  rank?: number;
  similarity?: number; // For TOPSIS
  netFlow?: number; // For PROMETHEE
}

export interface DecisionTreeNode {
  id: string;
  name: string;
  type: 'criterion' | 'alternative' | 'score';
  value?: number;
  children?: DecisionTreeNode[];
  metadata?: Record<string, unknown>;
}

export interface DecisionTree {
  id: string;
  decisionId: string;
  algorithm: MCDAAlgorithm;
  root: DecisionTreeNode;
  visualization: {
    nodes: D3Node[];
    links: D3Link[];
  };
}

export interface D3Node {
  id: string;
  name: string;
  value?: number;
  type: string;
}

export interface D3Link {
  source: string;
  target: string;
  value?: number;
}

// ============================================================================
// REGULATORY MONITORING
// ============================================================================

export interface RegulatoryChange {
  id: string;
  source: 'ECB' | 'SEC' | 'FCA' | 'RSS' | 'WebScraping' | 'Database';
  sourceId?: string;
  title: string;
  description: string;
  category: string;
  severity: 'low' | 'medium' | 'high' | 'critical';
  impact: 'minor' | 'moderate' | 'significant' | 'major';
  publishedDate: string;
  effectiveDate: string;
  detectedAt: string;
  url?: string;
  content?: string;
  tags: string[];
  status?: 'pending' | 'analyzed' | 'implemented' | 'dismissed';
  regulatoryBody: string;
  impactScore?: number;
  affectedSystems?: string[];
  requiresAction?: boolean;
}

export interface RegulatoryStats {
  totalChanges: number;
  pendingChanges: number;
  criticalChanges: number;
  activeSources: number;
}

export interface RegulatorySource {
  id: string;
  name: string;
  type: 'rss' | 'api' | 'scraper' | 'database';
  status: 'active' | 'idle' | 'error' | 'disabled';
  lastSync?: string;
  nextSync?: string;
  itemsIngested: number;
  errorCount: number;
  config: Record<string, unknown>;
}

export interface MonitorStatus {
  isRunning: boolean;
  activeSources: number;
  totalSources: number;
  lastUpdate: string;
  changesDetected24h: number;
}

// ============================================================================
// AUDIT TRAIL
// ============================================================================

export interface AuditEvent {
  id: string;
  timestamp: string;
  eventType: 'compliance_check' | 'policy_violation' | 'data_access' | 'config_change' | 'user_action';
  actor: string;
  action: string;
  resource: string;
  severity: 'info' | 'warning' | 'error' | 'critical';
  status: 'completed' | 'failed' | 'requires_action';
  metadata: Record<string, unknown>;
  correlationId?: string;
}

export interface AuditAnalytics {
  totalEvents: number;
  byType: Record<string, number>;
  bySeverity: Record<string, number>;
  byActor: Record<string, number>;
  violationsCount: number;
  complianceRate: number;
}

export interface AuditEntry extends AuditEvent {
  userId: string;
  entityType: string;
  entityId: string;
  ipAddress?: string;
  details?: Record<string, unknown>;
  success?: boolean;
  errorMessage?: string;
}

export interface AuditStats {
  totalEvents: number;
  uniqueUsers: number;
  criticalEvents: number;
  failedActions: number;
  entityTypes: number;
}

// ============================================================================
// AGENTS
// ============================================================================

export interface Agent {
  id: string;
  name: string;
  displayName: string;
  type: 'compliance' | 'data_validation' | 'risk_analysis' | 'transaction_guardian' | 'audit_intelligence';
  status: 'active' | 'idle' | 'error' | 'disabled';
  capabilities: string[];
  description: string;
  performance: {
    tasksCompleted: number;
    successRate: number;
    avgResponseTimeMs: number;
  };
  created_at: string;
  last_active: string;
}

export interface AgentStats {
  tasks_completed: number;
  success_rate: number;
  avg_response_time_ms: number;
  uptime_seconds: number;
  cpu_usage: number;
  memory_usage: number;
}

export interface AgentTask {
  id: string;
  description: string;
  status: 'pending' | 'running' | 'completed' | 'failed';
  created_at: string;
  completed_at?: string;
  duration_ms?: number;
  result?: Record<string, unknown>;
  error_message?: string;
}

export interface AgentMessage {
  id: string;
  timestamp: string;
  from: string;
  to: string;
  subject: string;
  message: string;
  priority: 'low' | 'medium' | 'high';
  status: 'sent' | 'delivered' | 'read' | 'failed';
  metadata?: Record<string, unknown>;
}

// ============================================================================
// HUMAN-AI COLLABORATION
// ============================================================================

export interface CollaborationSession {
  id: string;
  title: string;
  description?: string;
  createdAt: string;
  updatedAt?: string;
  participants: string[];
  agentIds: string[];
  topic?: string;
  objective?: string;
  status: 'active' | 'paused' | 'completed' | 'archived';
  messageCount: number;
  lastActivity: string;
  context?: Record<string, unknown>;
  settings?: {
    maxDuration?: number;
    autoArchive?: boolean;
    allowExternal?: boolean;
  };
}

export interface CollaborationMessage {
  id: string;
  sessionId: string;
  timestamp: string;
  sender: string;
  agentId?: string;
  senderType: 'human' | 'agent' | 'system';
  content: string;
  type?: 'text' | 'code' | 'file' | 'system';
  attachments?: string[];
  replyToId?: string;
  metadata?: Record<string, unknown>;
}

export interface Feedback {
  id: string;
  timestamp: string;
  decisionId?: string;
  agentId?: string;
  rating: number;
  comment: string;
  category: 'bug' | 'feature' | 'improvement' | 'compliment';
  status: 'open' | 'acknowledged' | 'resolved';
}

// ============================================================================
// PATTERN RECOGNITION
// ============================================================================

export interface Pattern {
  id: string;
  name: string;
  type: 'trend' | 'anomaly' | 'correlation' | 'seasonality';
  confidence: number;
  description: string;
  discoveredAt: string;
  occurrences: number;
  metadata: Record<string, unknown>;
}

export interface PatternStats {
  totalPatterns: number;
  byType: Record<string, number>;
  avgConfidence: number;
  recentDiscoveries: number;
}

// ============================================================================
// LLM INTEGRATION
// ============================================================================

export interface LLMCompletionRequest {
  prompt: string;
  model: 'gpt-4' | 'gpt-3.5-turbo' | 'claude-3-opus' | 'claude-3-sonnet';
  maxTokens?: number;
  temperature?: number;
}

export interface LLMCompletionResponse {
  id: string;
  model: string;
  completion: string;
  usage: {
    promptTokens: number;
    completionTokens: number;
    totalTokens: number;
  };
  finishReason: string;
}

export interface LLMAnalysisRequest {
  text: string;
  type: 'compliance' | 'risk' | 'sentiment' | 'summary';
}

export interface LLMAnalysisResponse {
  id: string;
  type: string;
  result: Record<string, unknown>;
  confidence: number;
  model: string;
}

// ============================================================================
// TRANSACTION GUARDIAN
// ============================================================================

export interface Transaction {
  id: string;
  timestamp: string;
  amount: number;
  currency: string;
  from: string;
  to: string;
  fromAccount: string;
  toAccount: string;
  type: string;
  status: 'pending' | 'approved' | 'rejected' | 'flagged' | 'completed';
  riskScore: number;
  riskLevel: 'low' | 'medium' | 'high' | 'critical';
  flags: string[];
  fraudIndicators?: string[];
  description?: string;
  metadata: Record<string, unknown>;
}

export interface TransactionStats {
  totalVolume: number;
  totalTransactions: number;
  flaggedTransactions: number;
  fraudRate: number;
  averageRiskScore: number;
}

export interface FraudAnalysis {
  transactionId: string;
  riskScore: number;
  riskLevel: 'low' | 'medium' | 'high' | 'critical';
  indicators: string[];
  recommendation: string;
  confidence: number;
}

export interface TransactionPattern {
  id: string;
  name: string;
  description: string;
  frequency: number;
  riskLevel: 'low' | 'medium' | 'high';
}

export interface AnomalyDetectionResult {
  id: string;
  transactionId: string;
  anomalyType: string;
  score: number;
  details: Record<string, unknown>;
}

export interface FraudRule {
  id: string;
  name: string;
  description: string;
  enabled: boolean;
  priority: number;
  conditions: Record<string, unknown>;
  actions: string[];
  ruleType?: 'threshold' | 'pattern' | 'anomaly' | 'velocity' | 'blacklist';
  condition?: string;
  threshold?: number;
  action?: 'flag' | 'block' | 'review' | 'alert';
  severity?: 'low' | 'medium' | 'high' | 'critical';
  triggerCount?: number;
  lastTriggered?: string;
}

export interface CreateFraudRuleRequest {
  name: string;
  description: string;
  ruleType: 'threshold' | 'pattern' | 'anomaly' | 'velocity' | 'blacklist';
  condition: string;
  threshold?: number;
  action: 'flag' | 'block' | 'review' | 'alert';
  severity: 'low' | 'medium' | 'high' | 'critical';
  enabled: boolean;
}

export interface FraudAlert {
  id: string;
  ruleId: string;
  transactionId: string;
  severity: 'low' | 'medium' | 'high' | 'critical';
  status: 'new' | 'investigating' | 'confirmed' | 'false_positive' | 'resolved';
  timestamp: string;
  details?: Record<string, unknown>;
}

export interface FraudStats {
  totalAlerts: number;
  activeRules: number;
  detectionRate: number;
  falsePositives: number;
}

// ============================================================================
// DATA INGESTION
// ============================================================================

export interface IngestionMetrics {
  totalSources: number;
  activeSources: number;
  itemsIngested24h: number;
  errorRate: number;
  avgIngestionTimeMs: number;
  bytesProcessed: number;
}

export interface DataQualityCheck {
  id: string;
  checkName: string;
  status: 'passed' | 'failed' | 'warning';
  timestamp: string;
  details: string;
  affectedRecords: number;
}

// ============================================================================
// KNOWLEDGE BASE
// ============================================================================

export interface KnowledgeEntry {
  id: string;
  title: string;
  content: string;
  category: string;
  tags: string[];
  createdAt: string;
  updatedAt: string;
  relevanceScore?: number;
}

export interface CaseExample {
  id: string;
  title: string;
  situation: string;
  decision: string;
  outcome: string;
  similarity?: number;
  tags: string[];
}

// ============================================================================
// PROMETHEUS METRICS
// ============================================================================

export interface SystemMetrics {
  cpuUsage: number;
  memoryUsage: number;
  diskUsage: number;
  networkIn: number;
  networkOut: number;
  requestRate: number;
  errorRate: number;
  avgResponseTime: number;
}

export interface HealthCheck {
  status: 'healthy' | 'degraded' | 'unhealthy';
  service: string;
  version: string;
  uptimeSeconds: number;
  totalRequests: number;
  checks?: Record<string, boolean>;
}

// ============================================================================
// CIRCUIT BREAKER
// ============================================================================

export interface CircuitBreakerStatus {
  name: string;
  state: 'closed' | 'open' | 'half_open';
  failureCount: number;
  successCount: number;
  lastFailure?: string;
  nextRetry?: string;
}

// ============================================================================
// COMMON
// ============================================================================

export interface APIError {
  error: string;
  message: string;
  code: string;
  timestamp: string;
  path?: string;
  details?: Record<string, unknown>;
}

export interface PaginatedResponse<T> {
  data: T[];
  total: number;
  page: number;
  pageSize: number;
  hasMore: boolean;
}

export interface TimeSeriesDataPoint {
  timestamp: string;
  value: number;
  metadata?: Record<string, unknown>;
}

// ============================================================================
// LLM MODELS & COMPLETIONS (Phase 5)
// ============================================================================

export interface LLMModel {
  id: string;
  name: string;
  provider: 'openai' | 'anthropic' | 'google' | 'custom';
  version: string;
  contextWindow: number;
  maxOutputTokens: number;
  capabilities: string[];
  pricing: {
    inputTokenPrice: number;
    outputTokenPrice: number;
    currency: string;
  };
  status: 'available' | 'deprecated' | 'beta';
  description?: string;
}

export interface LLMCompletion {
  id: string;
  modelId: string;
  prompt: string;
  completion: string;
  usage: {
    promptTokens: number;
    completionTokens: number;
    totalTokens: number;
    cost: number;
  };
  timestamp: string;
  finishReason: 'stop' | 'length' | 'content_filter' | 'error';
  metadata?: Record<string, unknown>;
}

export interface LLMAnalysis {
  id: string;
  modelId: string;
  text: string;
  analysisType: 'sentiment' | 'summary' | 'entities' | 'classification' | 'custom';
  result: Record<string, unknown>;
  confidence: number;
  timestamp: string;
  processingTimeMs: number;
}

export interface LLMConversation {
  id: string;
  title: string;
  modelId: string;
  systemPrompt?: string;
  messages: LLMMessage[];
  createdAt: string;
  updatedAt: string;
  totalTokens: number;
  totalCost: number;
  metadata?: Record<string, unknown>;
}

export interface LLMMessage {
  id: string;
  conversationId: string;
  role: 'user' | 'assistant' | 'system';
  content: string;
  timestamp: string;
  tokens?: number;
  metadata?: Record<string, unknown>;
}

export interface LLMUsageStats {
  totalRequests: number;
  totalTokens: number;
  totalCost: number;
  byModel: Record<string, {
    requests: number;
    tokens: number;
    cost: number;
  }>;
  timeRange: string;
}

export interface LLMBatchJob {
  id: string;
  modelId: string;
  status: 'pending' | 'running' | 'completed' | 'failed';
  totalItems: number;
  processedItems: number;
  failedItems: number;
  createdAt: string;
  completedAt?: string;
  results?: Array<{
    id: string;
    completion: string;
    error?: string;
  }>;
}

export interface LLMFineTuneJob {
  id: string;
  baseModelId: string;
  customModelId?: string;
  status: 'pending' | 'training' | 'completed' | 'failed';
  trainingProgress: number;
  epochs: number;
  currentEpoch: number;
  trainingLoss?: number;
  validationLoss?: number;
  createdAt: string;
  completedAt?: string;
}

export interface LLMBenchmark {
  modelId: string;
  benchmarks: {
    accuracy: number;
    latencyMs: number;
    throughput: number;
    costPerToken: number;
  };
  testDate: string;
}

// ============================================================================
// COLLABORATION SYSTEM (Phase 5)
// ============================================================================

export interface CollaborationAgent {
  id: string;
  name: string;
  type: 'human' | 'ai' | 'system';
  role: 'participant' | 'observer' | 'facilitator';
  status: 'online' | 'offline' | 'away' | 'busy';
  capabilities?: string[];
  metadata?: Record<string, unknown>;
}

export interface CollaborationTask {
  id: string;
  sessionId: string;
  title: string;
  description?: string;
  assignedTo?: string;
  status: 'pending' | 'in_progress' | 'completed' | 'blocked';
  priority: 'low' | 'medium' | 'high' | 'critical';
  progress: number;
  dueDate?: string;
  createdAt: string;
  completedAt?: string;
  dependencies?: string[];
  metadata?: Record<string, unknown>;
}

export interface CollaborationResource {
  id: string;
  sessionId: string;
  type: 'file' | 'code' | 'link' | 'data';
  title: string;
  content: string | object;
  description?: string;
  sharedBy: string;
  sharedAt: string;
  metadata?: Record<string, unknown>;
}

export interface CollaborationDecision {
  id: string;
  sessionId: string;
  proposalId: string;
  title: string;
  description: string;
  votes: Array<{
    agentId: string;
    vote: 'approve' | 'reject' | 'abstain';
    comment?: string;
    timestamp: string;
  }>;
  status: 'pending' | 'approved' | 'rejected';
  createdAt: string;
  decidedAt?: string;
}

export interface CollaborationAnalytics {
  sessionId: string;
  duration: number;
  messageCount: number;
  taskCount: number;
  completedTasks: number;
  participantCount: number;
  activeTime: number;
  decisionsMade: number;
  resourcesShared: number;
}

export interface CollaborationStats {
  totalSessions: number;
  activeSessions: number;
  totalMessages: number;
  totalTasks: number;
  completedTasks: number;
  averageSessionDuration: number;
  topParticipants: Array<{
    agentId: string;
    sessionCount: number;
  }>;
}
