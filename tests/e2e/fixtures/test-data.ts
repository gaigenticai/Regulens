/**
 * Test Data Fixtures
 *
 * Mock data for testing Regulens platform features
 */

/**
 * Sample regulatory changes
 */
export const mockRegulatoryChanges = [
  {
    id: 'reg-001',
    source: 'ECB',
    title: 'New Capital Requirements Directive',
    description: 'Updated capital adequacy requirements for financial institutions',
    category: 'Banking Regulation',
    severity: 'high',
    impact: 'significant',
    published_date: '2025-10-01T10:00:00Z',
    effective_date: '2026-01-01T00:00:00Z',
    url: 'https://www.ecb.europa.eu/press/pr/date/2025/html/example.en.html',
  },
  {
    id: 'reg-002',
    source: 'RSS Feed',
    title: 'GDPR Enforcement Update',
    description: 'New guidelines for data protection compliance',
    category: 'Data Privacy',
    severity: 'medium',
    impact: 'moderate',
    published_date: '2025-09-28T14:30:00Z',
    effective_date: '2025-11-01T00:00:00Z',
    url: 'https://example.com/gdpr-update',
  },
];

/**
 * Sample audit events
 */
export const mockAuditEvents = [
  {
    id: 'audit-001',
    timestamp: '2025-10-07T10:15:00Z',
    event_type: 'compliance_check',
    actor: 'admin@regulens.com',
    action: 'Initiated compliance scan',
    resource: 'regulatory_database',
    severity: 'info',
    status: 'completed',
    metadata: {
      duration_ms: 1250,
      items_checked: 145,
    },
  },
  {
    id: 'audit-002',
    timestamp: '2025-10-07T09:45:00Z',
    event_type: 'policy_violation',
    actor: 'system',
    action: 'Detected GDPR violation',
    resource: 'customer_data',
    severity: 'high',
    status: 'requires_action',
    metadata: {
      violation_type: 'data_retention',
      affected_records: 23,
    },
  },
];

/**
 * Sample agents
 */
export const mockAgents = [
  {
    id: 'agent-001',
    name: 'Compliance Monitor Agent',
    type: 'compliance',
    status: 'active',
    capabilities: ['regulatory_scanning', 'change_detection', 'risk_assessment'],
    description: 'Monitors regulatory changes and assesses compliance impact',
    performance: {
      tasks_completed: 1247,
      success_rate: 98.5,
      avg_response_time_ms: 450,
    },
    created_at: '2025-09-01T00:00:00Z',
    last_active: '2025-10-07T10:20:00Z',
  },
  {
    id: 'agent-002',
    name: 'Data Quality Agent',
    type: 'data_validation',
    status: 'active',
    capabilities: ['data_validation', 'schema_checking', 'anomaly_detection'],
    description: 'Ensures data quality and validates regulatory data feeds',
    performance: {
      tasks_completed: 3421,
      success_rate: 99.2,
      avg_response_time_ms: 320,
    },
    created_at: '2025-09-01T00:00:00Z',
    last_active: '2025-10-07T10:18:00Z',
  },
  {
    id: 'agent-003',
    name: 'Risk Assessment Agent',
    type: 'risk_analysis',
    status: 'idle',
    capabilities: ['risk_modeling', 'mcda_analysis', 'scenario_planning'],
    description: 'Performs multi-criteria decision analysis for compliance risks',
    performance: {
      tasks_completed: 892,
      success_rate: 96.8,
      avg_response_time_ms: 1850,
    },
    created_at: '2025-09-15T00:00:00Z',
    last_active: '2025-10-07T08:30:00Z',
  },
];

/**
 * Sample activity feed items
 */
export const mockActivityFeed = [
  {
    id: 'activity-001',
    timestamp: '2025-10-07T10:25:00Z',
    type: 'regulatory_change',
    title: 'New ECB regulation detected',
    description: 'Banking capital requirements updated',
    priority: 'high',
    actor: 'Compliance Monitor Agent',
  },
  {
    id: 'activity-002',
    timestamp: '2025-10-07T10:20:00Z',
    type: 'decision_made',
    title: 'Risk assessment completed',
    description: 'MCDA analysis for new GDPR requirements',
    priority: 'medium',
    actor: 'Risk Assessment Agent',
  },
  {
    id: 'activity-003',
    timestamp: '2025-10-07T10:10:00Z',
    type: 'data_ingestion',
    title: 'RSS feed synchronized',
    description: 'Ingested 45 new regulatory documents',
    priority: 'low',
    actor: 'Data Ingestion Pipeline',
  },
];

/**
 * Sample decisions
 */
export const mockDecisions = [
  {
    id: 'decision-001',
    timestamp: '2025-10-07T09:00:00Z',
    title: 'Approve GDPR Compliance Update',
    description: 'Decision to implement new data retention policies',
    method: 'ELECTRE III',
    criteria: ['cost', 'timeline', 'risk', 'regulatory_alignment'],
    alternatives: ['immediate_implementation', 'phased_rollout', 'delayed_implementation'],
    selected_alternative: 'phased_rollout',
    confidence_score: 0.87,
    decision_maker: 'Risk Assessment Agent',
    approved_by: 'admin@regulens.com',
    status: 'approved',
  },
  {
    id: 'decision-002',
    timestamp: '2025-10-06T15:30:00Z',
    title: 'Data Source Priority Ranking',
    description: 'Ranked regulatory data sources by reliability',
    method: 'AHP',
    criteria: ['accuracy', 'timeliness', 'coverage', 'authority'],
    alternatives: ['ecb_feed', 'official_journal', 'industry_news'],
    selected_alternative: 'ecb_feed',
    confidence_score: 0.92,
    decision_maker: 'Data Quality Agent',
    status: 'implemented',
  },
];

/**
 * Sample communication messages
 */
export const mockCommunications = [
  {
    id: 'msg-001',
    timestamp: '2025-10-07T10:15:00Z',
    from: 'Compliance Monitor Agent',
    to: 'Risk Assessment Agent',
    subject: 'New regulation requires assessment',
    message: 'Detected new ECB capital requirement. Please assess impact on current portfolio.',
    priority: 'high',
    status: 'delivered',
  },
  {
    id: 'msg-002',
    timestamp: '2025-10-07T10:18:00Z',
    from: 'Risk Assessment Agent',
    to: 'Compliance Monitor Agent',
    subject: 'Re: New regulation requires assessment',
    message: 'Assessment queued. Estimated completion: 15 minutes.',
    priority: 'medium',
    status: 'read',
  },
];

/**
 * Test user credentials
 */
export const testUsers = {
  admin: {
    username: 'admin@regulens.com',
    password: 'Admin123!Test',
    role: 'administrator',
  },
  analyst: {
    username: 'analyst@regulens.com',
    password: 'Analyst123!Test',
    role: 'compliance_analyst',
  },
  viewer: {
    username: 'viewer@regulens.com',
    password: 'Viewer123!Test',
    role: 'read_only',
  },
};

/**
 * API endpoints mapping
 */
export const apiEndpoints = {
  activity: '/api/activity',
  decisions: '/api/decisions',
  regulatoryChanges: '/api/regulatory-changes',
  auditTrail: '/api/audit-trail',
  communication: '/api/communication',
  agents: '/api/agents',
  collaboration: '/api/collaboration',
  patterns: '/api/patterns',
  feedback: '/api/feedback',
  errors: '/api/errors',
  health: '/api/health',
  llm: '/api/llm',
  config: '/api/config',
  db: '/api/db',
  metrics: '/metrics',
};

/**
 * Generate date range for testing
 */
export function getDateRange(daysBack: number = 7) {
  const now = new Date();
  const past = new Date(now.getTime() - daysBack * 24 * 60 * 60 * 1000);

  return {
    from: past.toISOString(),
    to: now.toISOString(),
    fromFormatted: past.toISOString().split('T')[0],
    toFormatted: now.toISOString().split('T')[0],
  };
}

/**
 * Sample error scenarios
 */
export const mockErrors = [
  {
    id: 'error-001',
    timestamp: '2025-10-07T09:30:00Z',
    type: 'api_error',
    severity: 'warning',
    message: 'ECB RSS feed temporarily unavailable',
    component: 'data_ingestion',
    retry_count: 2,
    resolved: false,
  },
  {
    id: 'error-002',
    timestamp: '2025-10-07T08:15:00Z',
    type: 'validation_error',
    severity: 'error',
    message: 'Invalid regulatory document schema detected',
    component: 'data_validation',
    retry_count: 0,
    resolved: true,
  },
];
