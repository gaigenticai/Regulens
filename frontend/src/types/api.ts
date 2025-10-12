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

// ============================================================================
// FEATURE 1: REAL-TIME COLLABORATION DASHBOARD
// ============================================================================

export interface CollaborationSession {
  session_id: string;
  title: string;
  description: string;
  objective: string;
  status: 'active' | 'paused' | 'completed' | 'archived' | 'cancelled';
  created_by: string;
  created_at: string;
  updated_at: string;
  started_at?: string;
  completed_at?: string;
  agent_ids?: string[];
  context?: Record<string, unknown>;
  settings?: Record<string, unknown>;
  metadata?: Record<string, unknown>;
}

export interface ReasoningStep {
  stream_id: string;
  session_id: string;
  agent_id: string;
  agent_name: string;
  agent_type: string;
  reasoning_step: string;
  step_number: number;
  step_type: 'thinking' | 'analyzing' | 'deciding' | 'executing' | 'completed' | 'error';
  confidence_score: number;
  timestamp: string;
  duration_ms?: number;
  metadata?: Record<string, unknown>;
  parent_step_id?: string;
}

export interface ConfidenceMetric {
  metric_id: string;
  session_id: string;
  decision_id?: string;
  stream_id?: string;
  metric_type: 'data_quality' | 'model_confidence' | 'rule_match' | 'historical_accuracy' | 'consensus';
  metric_name: string;
  metric_value: number;
  weight: number;
  contributing_factors: string[];
  calculated_at: string;
}

export interface HumanOverride {
  override_id: string;
  decision_id?: string;
  session_id?: string;
  user_id: string;
  user_name: string;
  original_decision: string;
  override_decision: string;
  reason: string;
  justification?: string;
  impact_assessment?: Record<string, unknown>;
  timestamp: string;
  metadata?: Record<string, unknown>;
}

export interface CollaborationAgent {
  participant_id: string;
  session_id: string;
  agent_id: string;
  agent_name: string;
  agent_type: string;
  role: 'participant' | 'observer' | 'facilitator' | 'leader';
  status: 'active' | 'inactive' | 'disconnected' | 'completed';
  joined_at: string;
  left_at?: string;
  contribution_count: number;
  last_activity_at?: string;
  performance_metrics?: Record<string, unknown>;
}

export interface CollaborationDashboardStats {
  total_sessions: number;
  active_sessions: number;
  total_reasoning_steps: number;
  total_overrides: number;
}

export interface CreateSessionRequest {
  title: string;
  description?: string;
  objective?: string;
  created_by: string;
  agent_ids?: string[];
  context?: Record<string, unknown>;
  settings?: Record<string, unknown>;
}

export interface RecordOverrideRequest {
  session_id?: string;
  decision_id?: string;
  user_id: string;
  user_name: string;
  original_decision: string;
  override_decision: string;
  reason: string;
  justification?: string;
  impact_assessment?: Record<string, unknown>;
}

// ============================================================================
// FEATURE 2: REGULATORY CHANGE ALERTS WITH EMAIL
// ============================================================================

export interface AlertRule {
  rule_id: string;
  name: string;
  description: string;
  enabled: boolean;
  severity_filter: string[];
  source_filter?: string[];
  keyword_filters?: string[];
  notification_channels: string[];
  recipients: string[];
  throttle_minutes: number;
  created_by?: string;
  created_at: string;
  updated_at: string;
  last_triggered_at?: string;
  trigger_count: number;
  metadata?: Record<string, unknown>;
}

export interface AlertDeliveryLog {
  delivery_id: string;
  rule_id: string;
  regulatory_change_id?: string;
  recipient: string;
  channel: string;
  status: 'pending' | 'sent' | 'failed' | 'throttled' | 'bounced';
  sent_at: string;
  delivered_at?: string;
  error_message?: string;
  retry_count: number;
  message_id?: string;
  metadata?: Record<string, unknown>;
}

export interface AlertTemplate {
  template_id: string;
  name: string;
  template_type: 'email' | 'slack' | 'sms';
  subject_template?: string;
  body_template: string;
  variables: string[];
  is_default: boolean;
  created_at: string;
  updated_at: string;
}

export interface AlertStats {
  total_rules: number;
  active_rules: number;
  total_deliveries: number;
  successful_deliveries: number;
}

export interface CreateAlertRuleRequest {
  name: string;
  description?: string;
  enabled?: boolean;
  severity_filter?: string[];
  source_filter?: string[];
  keyword_filters?: string[];
  notification_channels?: string[];
  recipients: string[];
  throttle_minutes?: number;
  created_by?: string;
}

// ============================================================================
// FEATURE 3: EXPORT/REPORTING MODULE
// ============================================================================

export interface ExportRequest {
  export_id: string;
  export_type: 'pdf_audit_trail' | 'excel_transactions' | 'pdf_compliance_report' | 'excel_regulatory_changes' | 'pdf_decision_analysis' | 'csv_data_export';
  requested_by: string;
  status: 'pending' | 'processing' | 'completed' | 'failed' | 'cancelled';
  parameters?: Record<string, unknown>;
  file_path?: string;
  file_size_bytes?: number;
  download_url?: string;
  download_count: number;
  expires_at?: string;
  created_at: string;
  completed_at?: string;
  error_message?: string;
  processing_time_ms?: number;
  metadata?: Record<string, unknown>;
}

export interface ExportTemplate {
  template_id: string;
  name: string;
  export_type: string;
  description: string;
  template_config: Record<string, unknown>;
  is_default: boolean;
  is_public: boolean;
  created_by?: string;
  created_at: string;
  updated_at: string;
  usage_count: number;
}

export interface CreateExportRequest {
  export_type: string;
  requested_by: string;
  parameters?: Record<string, unknown>;
}

// ============================================================================
// FEATURE 4: API KEY MANAGEMENT UI
// ============================================================================

export interface LLMApiKey {
  key_id: string;
  provider: 'openai' | 'anthropic' | 'cohere' | 'huggingface' | 'google' | 'azure_openai' | 'custom';
  key_name: string;
  is_active: boolean;
  created_at: string;
  last_used_at?: string;
  usage_count: number;
  rate_limit_per_minute?: number;
  metadata?: Record<string, unknown>;
}

export interface LLMKeyUsageStats {
  provider: string;
  total_requests: number;
  total_tokens: number;
  total_cost: number;
}

export interface CreateLLMKeyRequest {
  provider: string;
  key_name: string;
  api_key: string;
  created_by: string;
  rate_limit_per_minute?: number;
}

// ============================================================================
// FEATURE 5: PREDICTIVE COMPLIANCE RISK SCORING
// ============================================================================

export interface RiskPrediction {
  prediction_id: string;
  entity_type: 'transaction' | 'regulatory_change' | 'policy' | 'vendor' | 'process';
  entity_id: string;
  risk_score: number;
  risk_level: 'low' | 'medium' | 'high' | 'critical';
  confidence_score: number;
  predicted_at: string;
}

export interface MLModel {
  model_id: string;
  model_name: string;
  model_type: string;
  model_version: string;
  accuracy_score: number;
  is_active: boolean;
}

export interface RiskDashboardStats {
  total_predictions: number;
  critical_risks: number;
  high_risks: number;
  avg_risk_score: number;
}

// ============================================================================
// FEATURE 7: REGULATORY CHANGE SIMULATOR
// ============================================================================

export interface RegulatorySimulation {
  simulation_id: string;
  name: string;
  simulation_type: 'regulatory_change' | 'policy_update' | 'market_shift' | 'risk_event' | 'custom';
  status: 'draft' | 'running' | 'completed' | 'failed' | 'archived';
  created_by: string;
  created_at: string;
  completed_at?: string;
}

export interface SimulationTemplate {
  template_id: string;
  template_name: string;
  template_category: string;
  description: string;
  usage_count: number;
}

export interface CreateSimulationRequest {
  name: string;
  simulation_type: string;
  created_by: string;
}

// ============================================================================
// FEATURE 8: ADVANCED ANALYTICS & BI DASHBOARD
// ============================================================================

export interface BIDashboard {
  dashboard_id: string;
  dashboard_name: string;
  dashboard_type: 'executive' | 'operational' | 'compliance' | 'risk' | 'custom';
  description: string;
  view_count: number;
  created_at: string;
}

export interface AnalyticsMetric {
  metric_name: string;
  metric_category: string;
  metric_value: number;
  metric_unit: string;
  aggregation_period: string;
  calculated_at: string;
}

export interface DataInsight {
  insight_id: string;
  insight_type: 'trend' | 'anomaly' | 'pattern' | 'correlation' | 'recommendation';
  title: string;
  description: string;
  confidence_score: number;
  priority: 'low' | 'medium' | 'high' | 'critical';
  discovered_at: string;
}

export interface AnalyticsStats {
  total_dashboards: number;
  recent_metrics: number;
  active_insights: number;
}

// FEATURE 10: Natural Language Policy Builder
export interface NLPolicyRule {
  rule_id: string;
  rule_name: string;
  natural_language_input: string;
  rule_type: string;
  is_active: boolean;
  confidence_score: number;
  validation_status: 'pending' | 'approved' | 'rejected';
  created_at: string;
}

export interface CreateNLPolicyRequest {
  natural_language_input: string;
  rule_name?: string;
  rule_type?: string;
  created_by: string;
}

// FEATURE 12: Regulatory Chatbot
export interface ChatbotConversation {
  conversation_id: string;
  platform: string;
  user_id: string;
  message_count: number;
  started_at: string;
  is_active: boolean;
}

export interface ChatbotMessage {
  message: string;
  conversation_id?: string;
}

export interface ChatbotResponse {
  response: string;
  confidence: number;
}

// FEATURE 13: Integration Marketplace
export interface IntegrationConnector {
  connector_id: string;
  connector_name: string;
  connector_type: string;
  vendor: string;
  is_verified: boolean;
  is_active: boolean;
  install_count: number;
  rating: number | null;
}

export interface IntegrationInstance {
  instance_id: string;
  instance_name: string;
  connector_name: string;
  is_enabled: boolean;
  last_sync_at: string;
  sync_status: string;
}

// FEATURE 14: Compliance Training Module
export interface TrainingCourse {
  course_id: string;
  course_name: string;
  course_category: string;
  difficulty_level: string;
  estimated_duration_minutes: number | null;
  is_required: boolean;
  passing_score: number;
  points_reward: number;
}

export interface TrainingLeaderboardEntry {
  user_id: string;
  total_points: number;
  courses_completed: number;
  rank: number | null;
}
