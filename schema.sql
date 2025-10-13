-- Regulens Agentic AI Compliance System - Database Schema
-- Production-grade PostgreSQL schema for compliance data management

-- Enable necessary extensions
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pgcrypto";
CREATE EXTENSION IF NOT EXISTS "pg_trgm";
CREATE EXTENSION IF NOT EXISTS "btree_gin";
CREATE EXTENSION IF NOT EXISTS "vector";

-- =============================================================================
-- CORE COMPLIANCE TABLES
-- =============================================================================

-- Compliance events table
CREATE TABLE IF NOT EXISTS compliance_events (
    event_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    event_type VARCHAR(50) NOT NULL,
    severity VARCHAR(20) NOT NULL CHECK (severity IN ('LOW', 'MEDIUM', 'HIGH', 'CRITICAL')),
    description TEXT NOT NULL,
    source_type VARCHAR(100) NOT NULL,
    source_id VARCHAR(255) NOT NULL,
    source_location VARCHAR(255),
    event_metadata JSONB,
    occurred_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    processed_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Agent decisions table
CREATE TABLE IF NOT EXISTS agent_decisions (
    decision_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    event_id UUID NOT NULL REFERENCES compliance_events(event_id),
    agent_type VARCHAR(100) NOT NULL,
    agent_name VARCHAR(255) NOT NULL,
    decision_type VARCHAR(50) NOT NULL,
    confidence_level VARCHAR(20) NOT NULL CHECK (confidence_level IN ('LOW', 'MEDIUM', 'HIGH', 'VERY_HIGH')),
    reasoning JSONB,
    recommended_actions JSONB,
    risk_assessment JSONB,
    decision_timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    execution_time_ms INTEGER,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Regulatory changes table
CREATE TABLE IF NOT EXISTS regulatory_changes (
    change_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    source VARCHAR(100) NOT NULL, -- 'SEC', 'FCA', 'ECB', etc.
    title TEXT NOT NULL,
    description TEXT,
    change_type VARCHAR(50) NOT NULL, -- 'NEW_REGULATION', 'AMENDMENT', 'CLARIFICATION', etc.
    effective_date DATE,
    document_url TEXT,
    document_content TEXT,
    extracted_entities JSONB, -- Named entities extracted from the document
    impact_assessment JSONB, -- AI-generated impact assessment
    status VARCHAR(20) NOT NULL DEFAULT 'DETECTED' CHECK (status IN ('DETECTED', 'ANALYZED', 'DISTRIBUTED', 'ARCHIVED')),
    severity VARCHAR(20) DEFAULT 'MEDIUM' CHECK (severity IN ('LOW', 'MEDIUM', 'HIGH', 'CRITICAL')),
    detected_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    analyzed_at TIMESTAMP WITH TIME ZONE,
    distributed_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Knowledge base table
CREATE TABLE IF NOT EXISTS knowledge_base (
    knowledge_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    key TEXT NOT NULL,
    content TEXT NOT NULL,
    content_type VARCHAR(50) NOT NULL, -- 'REGULATION', 'POLICY', 'PROCEDURE', etc.
    tags TEXT[],
    embeddings VECTOR(384), -- For semantic search (requires pgvector extension)
    source VARCHAR(255),
    confidence_score DECIMAL(3,2),
    last_updated TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Feedback incorporation tables
CREATE TABLE IF NOT EXISTS feedback_events (
    feedback_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    decision_id VARCHAR(255) NOT NULL,
    entity_id VARCHAR(255) NOT NULL,
    entity_type VARCHAR(50),
    feedback_type VARCHAR(50) NOT NULL,
    feedback_source VARCHAR(50) NOT NULL, -- 'human', 'system', 'automated'
    score DECIMAL(3,2) CHECK (score >= 0 AND score <= 1),
    confidence DECIMAL(3,2) CHECK (confidence >= 0 AND confidence <= 1),
    feedback_text TEXT,
    metadata JSONB,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    applied_at TIMESTAMP WITH TIME ZONE
);

CREATE TABLE IF NOT EXISTS learning_models (
    model_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    entity_id VARCHAR(255) NOT NULL,
    entity_type VARCHAR(50),
    model_type VARCHAR(50) NOT NULL,
    model_version INTEGER NOT NULL DEFAULT 1,
    model_data JSONB NOT NULL,
    performance_metrics JSONB,
    training_data_size INTEGER,
    last_trained_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Create indexes for feedback and learning tables
CREATE INDEX IF NOT EXISTS idx_feedback_decision_id ON feedback_events(decision_id);
CREATE INDEX IF NOT EXISTS idx_feedback_entity_id ON feedback_events(entity_id);
CREATE INDEX IF NOT EXISTS idx_feedback_created_at ON feedback_events(created_at);
CREATE INDEX IF NOT EXISTS idx_learning_models_entity_id ON learning_models(entity_id);
CREATE INDEX IF NOT EXISTS idx_learning_models_type ON learning_models(model_type);

-- Agent activity feed table
CREATE TABLE IF NOT EXISTS agent_activity_events (
    event_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    agent_type VARCHAR(100) NOT NULL,
    agent_name VARCHAR(255) NOT NULL,
    event_type VARCHAR(50) NOT NULL,
    event_category VARCHAR(50) NOT NULL,
    description TEXT NOT NULL,
    event_metadata JSONB,
    entity_id VARCHAR(255),
    entity_type VARCHAR(50),
    severity VARCHAR(20) NOT NULL DEFAULT 'INFO' CHECK (severity IN ('DEBUG', 'INFO', 'WARN', 'ERROR', 'CRITICAL')),
    occurred_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    processed_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Create indexes for agent activity table
CREATE INDEX IF NOT EXISTS idx_agent_activity_agent_type ON agent_activity_events(agent_type);
CREATE INDEX IF NOT EXISTS idx_agent_activity_entity_id ON agent_activity_events(entity_id);
CREATE INDEX IF NOT EXISTS idx_agent_activity_occurred_at ON agent_activity_events(occurred_at);
CREATE INDEX IF NOT EXISTS idx_agent_activity_category ON agent_activity_events(event_category);

-- =============================================================================
-- AUDIT AND COMPLIANCE TABLES
-- =============================================================================

-- Audit log table (separate audit database recommended for production)
CREATE TABLE IF NOT EXISTS audit_log (
    audit_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    table_name VARCHAR(100) NOT NULL,
    record_id UUID NOT NULL,
    operation VARCHAR(10) NOT NULL CHECK (operation IN ('INSERT', 'UPDATE', 'DELETE')),
    old_values JSONB,
    new_values JSONB,
    user_id VARCHAR(255),
    session_id VARCHAR(255),
    ip_address INET,
    user_agent TEXT,
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Compliance violations table
CREATE TABLE IF NOT EXISTS compliance_violations (
    violation_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    event_id UUID REFERENCES compliance_events(event_id),
    violation_type VARCHAR(100) NOT NULL,
    severity VARCHAR(20) NOT NULL CHECK (severity IN ('LOW', 'MEDIUM', 'HIGH', 'CRITICAL')),
    description TEXT NOT NULL,
    affected_entities JSONB,
    remediation_actions JSONB,
    status VARCHAR(20) NOT NULL DEFAULT 'OPEN' CHECK (status IN ('OPEN', 'INVESTIGATING', 'REMEDIATED', 'CLOSED')),
    assigned_to VARCHAR(255),
    due_date DATE,
    resolved_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Agent metrics table
CREATE TABLE IF NOT EXISTS agent_metrics (
    metric_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    agent_type VARCHAR(100) NOT NULL,
    agent_name VARCHAR(255) NOT NULL,
    metric_name VARCHAR(255) NOT NULL,
    metric_value DECIMAL(20,6),
    metric_type VARCHAR(20) NOT NULL CHECK (metric_type IN ('GAUGE', 'COUNTER', 'HISTOGRAM')),
    labels JSONB,
    collected_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- =============================================================================
-- CONFIGURATION AND SYSTEM TABLES
-- =============================================================================

-- System configuration table
CREATE TABLE IF NOT EXISTS system_config (
    config_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    config_key VARCHAR(255) UNIQUE NOT NULL,
    config_value TEXT,
    config_type VARCHAR(50) NOT NULL, -- 'STRING', 'INTEGER', 'BOOLEAN', 'JSON'
    description TEXT,
    is_encrypted BOOLEAN NOT NULL DEFAULT FALSE,
    last_modified TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    modified_by VARCHAR(255)
);

-- Agent configurations table
CREATE TABLE IF NOT EXISTS agent_configurations (
    config_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    agent_type VARCHAR(100) NOT NULL,
    agent_name VARCHAR(255) NOT NULL,
    region VARCHAR(50),  -- e.g., 'US', 'EU', 'UK', 'APAC'
    configuration JSONB NOT NULL,
    version INTEGER NOT NULL DEFAULT 1,
    is_active BOOLEAN NOT NULL DEFAULT TRUE,
    status VARCHAR(50) NOT NULL DEFAULT 'created',  -- 'created', 'running', 'stopped', 'failed'
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),

    UNIQUE(agent_type, agent_name, version)
);

-- =============================================================================
-- INDEXES FOR PERFORMANCE
-- =============================================================================

-- Compliance events indexes
CREATE INDEX IF NOT EXISTS idx_compliance_events_type ON compliance_events(event_type);
CREATE INDEX IF NOT EXISTS idx_compliance_events_severity ON compliance_events(severity);
CREATE INDEX IF NOT EXISTS idx_compliance_events_occurred_at ON compliance_events(occurred_at);
CREATE INDEX IF NOT EXISTS idx_compliance_events_source ON compliance_events(source_type, source_id);
CREATE INDEX IF NOT EXISTS idx_compliance_events_metadata ON compliance_events USING GIN(event_metadata);

-- Agent decisions indexes
CREATE INDEX IF NOT EXISTS idx_agent_decisions_event_id ON agent_decisions(event_id);
CREATE INDEX IF NOT EXISTS idx_agent_decisions_agent_type ON agent_decisions(agent_type);
CREATE INDEX IF NOT EXISTS idx_agent_decisions_timestamp ON agent_decisions(decision_timestamp);
CREATE INDEX IF NOT EXISTS idx_agent_decisions_reasoning ON agent_decisions USING GIN(reasoning);

-- Regulatory changes indexes
CREATE INDEX IF NOT EXISTS idx_regulatory_changes_source ON regulatory_changes(source);
CREATE INDEX IF NOT EXISTS idx_regulatory_changes_status ON regulatory_changes(status);
CREATE INDEX IF NOT EXISTS idx_regulatory_changes_effective_date ON regulatory_changes(effective_date);
CREATE INDEX IF NOT EXISTS idx_regulatory_changes_entities ON regulatory_changes USING GIN(extracted_entities);
CREATE INDEX IF NOT EXISTS idx_regulatory_changes_impact ON regulatory_changes USING GIN(impact_assessment);

-- Knowledge base indexes
CREATE INDEX IF NOT EXISTS idx_knowledge_base_key ON knowledge_base USING HASH(key);
CREATE INDEX IF NOT EXISTS idx_knowledge_base_tags ON knowledge_base USING GIN(tags);
CREATE INDEX IF NOT EXISTS idx_knowledge_base_type ON knowledge_base(content_type);
CREATE INDEX IF NOT EXISTS idx_knowledge_base_updated ON knowledge_base(last_updated);

-- Full text search indexes
CREATE INDEX IF NOT EXISTS idx_compliance_events_description_trgm ON compliance_events USING GIN(description gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_regulatory_changes_title_trgm ON regulatory_changes USING GIN(title gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_knowledge_base_content_trgm ON knowledge_base USING GIN(content gin_trgm_ops);

-- =============================================================================
-- TRIGGERS FOR AUTOMATIC UPDATES
-- =============================================================================

-- Function to update updated_at timestamp
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ language 'plpgsql';

-- Apply update trigger to relevant tables
CREATE TRIGGER update_compliance_events_updated_at BEFORE UPDATE ON compliance_events FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_regulatory_changes_updated_at BEFORE UPDATE ON regulatory_changes FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_compliance_violations_updated_at BEFORE UPDATE ON compliance_violations FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_agent_configurations_updated_at BEFORE UPDATE ON agent_configurations FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

-- =============================================================================
-- VIEWS FOR COMMON QUERIES
-- =============================================================================

-- Active regulatory changes view
CREATE OR REPLACE VIEW active_regulatory_changes AS
SELECT *
FROM regulatory_changes
WHERE status IN ('DETECTED', 'ANALYZED', 'DISTRIBUTED')
AND (effective_date IS NULL OR effective_date >= CURRENT_DATE);

-- Recent agent decisions view
CREATE OR REPLACE VIEW recent_agent_decisions AS
SELECT
    ad.*,
    ce.event_type,
    ce.severity,
    ce.description as event_description
FROM agent_decisions ad
JOIN compliance_events ce ON ad.event_id = ce.event_id
WHERE ad.decision_timestamp >= NOW() - INTERVAL '30 days'
ORDER BY ad.decision_timestamp DESC;

-- Compliance health dashboard view
CREATE OR REPLACE VIEW compliance_health_dashboard AS
SELECT
    DATE_TRUNC('day', occurred_at) as date,
    event_type,
    severity,
    COUNT(*) as event_count,
    COUNT(CASE WHEN processed_at IS NOT NULL THEN 1 END) as processed_count
FROM compliance_events
WHERE occurred_at >= NOW() - INTERVAL '30 days'
GROUP BY DATE_TRUNC('day', occurred_at), event_type, severity
ORDER BY date DESC, event_count DESC;

-- =============================================================================
-- INITIAL DATA SEEDING
-- =============================================================================

-- Insert default system configuration
INSERT INTO system_config (config_key, config_value, config_type, description) VALUES
('system.version', '1.0.0', 'STRING', 'Current system version'),
('system.environment', 'production', 'STRING', 'System environment'),
('agent.orchestrator.worker_threads', '4', 'INTEGER', 'Number of worker threads'),
('metrics.collection_interval_seconds', '30', 'INTEGER', 'Metrics collection interval'),
('regulatory_monitor.check_interval_minutes', '15', 'INTEGER', 'Regulatory monitoring interval')
ON CONFLICT (config_key) DO NOTHING;

-- =============================================================================
-- POC 1: TRANSACTION COMPLIANCE GUARDIAN TABLES
-- =============================================================================

-- Customer profiles for KYC/AML monitoring
CREATE TABLE IF NOT EXISTS customer_profiles (
    customer_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    customer_type VARCHAR(20) NOT NULL CHECK (customer_type IN ('INDIVIDUAL', 'BUSINESS', 'INSTITUTION')),
    full_name VARCHAR(255),
    business_name VARCHAR(255),
    tax_id VARCHAR(50) UNIQUE,
    date_of_birth DATE,
    nationality VARCHAR(3), -- ISO 3166-1 alpha-3
    residency_country VARCHAR(3), -- ISO 3166-1 alpha-3
    risk_rating VARCHAR(10) NOT NULL DEFAULT 'LOW' CHECK (risk_rating IN ('LOW', 'MEDIUM', 'HIGH', 'VERY_HIGH')),
    kyc_status VARCHAR(20) NOT NULL DEFAULT 'PENDING' CHECK (kyc_status IN ('PENDING', 'VERIFIED', 'REJECTED', 'EXPIRED')),
    kyc_completed_at TIMESTAMP WITH TIME ZONE,
    last_review_date TIMESTAMP WITH TIME ZONE,
    watchlist_flags JSONB DEFAULT '[]'::jsonb,
    sanctions_screening JSONB,
    pep_status BOOLEAN DEFAULT FALSE,
    adverse_media JSONB,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Transaction records for AML monitoring
CREATE TABLE IF NOT EXISTS transactions (
    transaction_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    customer_id UUID NOT NULL REFERENCES customer_profiles(customer_id),
    transaction_type VARCHAR(50) NOT NULL,
    amount DECIMAL(20,2) NOT NULL,
    currency VARCHAR(3) NOT NULL DEFAULT 'USD',
    sender_account VARCHAR(50),
    receiver_account VARCHAR(50),
    sender_name VARCHAR(255),
    receiver_name VARCHAR(255),
    sender_country VARCHAR(3),
    receiver_country VARCHAR(3),
    transaction_date TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    value_date TIMESTAMP WITH TIME ZONE,
    description TEXT,
    channel VARCHAR(20) NOT NULL DEFAULT 'ONLINE' CHECK (channel IN ('ONLINE', 'ATM', 'BRANCH', 'WIRE', 'MOBILE')),
    status VARCHAR(20) NOT NULL DEFAULT 'completed' CHECK (status IN ('pending', 'processing', 'completed', 'rejected', 'flagged', 'approved')),
    merchant_category_code VARCHAR(10),
    ip_address INET,
    device_fingerprint VARCHAR(255),
    location_lat DECIMAL(10,8),
    location_lng DECIMAL(11,8),
    risk_score DECIMAL(5,4), -- AI-calculated risk score 0.0000-1.0000
    flagged BOOLEAN DEFAULT FALSE,
    flag_reason TEXT,
    processed_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Agent risk assessments for transactions
CREATE TABLE IF NOT EXISTS transaction_risk_assessments (
    assessment_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    transaction_id UUID NOT NULL REFERENCES transactions(transaction_id),
    agent_name VARCHAR(100) NOT NULL,
    risk_score DECIMAL(5,4) NOT NULL,
    risk_factors JSONB,
    recommended_actions JSONB,
    confidence_level DECIMAL(3,2),
    assessment_reasoning TEXT,
    processing_time_ms INTEGER,
    assessed_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    human_override BOOLEAN DEFAULT FALSE,
    human_override_reason TEXT,
    human_override_by VARCHAR(100)
);

-- Customer behavior patterns (learned by agents)
CREATE TABLE IF NOT EXISTS customer_behavior_patterns (
    pattern_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    customer_id UUID NOT NULL REFERENCES customer_profiles(customer_id),
    pattern_type VARCHAR(50) NOT NULL,
    pattern_data JSONB,
    confidence_score DECIMAL(3,2),
    last_observed TIMESTAMP WITH TIME ZONE,
    pattern_valid_until TIMESTAMP WITH TIME ZONE,
    detected_by_agent VARCHAR(100),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- =============================================================================
-- POC 2: REGULATORY CHANGE IMPACT ASSESSOR TABLES
-- =============================================================================

-- Regulatory documents and requirements
CREATE TABLE IF NOT EXISTS regulatory_documents (
    document_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    regulator VARCHAR(10) NOT NULL, -- SEC, FCA, ECB, etc.
    document_type VARCHAR(50) NOT NULL,
    title TEXT NOT NULL,
    document_number VARCHAR(100),
    publication_date DATE NOT NULL,
    effective_date DATE,
    expiry_date DATE,
    status VARCHAR(20) NOT NULL DEFAULT 'ACTIVE' CHECK (status IN ('DRAFT', 'ACTIVE', 'SUPERSEDED', 'WITHDRAWN')),
    jurisdiction VARCHAR(100),
    sector VARCHAR(100),
    full_text TEXT,
    summary TEXT,
    key_requirements JSONB,
    affected_entities JSONB,
    compliance_deadlines JSONB,
    url TEXT,
    last_updated TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Business processes and controls mapping
CREATE TABLE IF NOT EXISTS business_processes (
    process_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    process_name VARCHAR(255) NOT NULL,
    process_category VARCHAR(100) NOT NULL,
    department VARCHAR(100),
    owner VARCHAR(100),
    description TEXT,
    risk_level VARCHAR(10) NOT NULL DEFAULT 'MEDIUM' CHECK (risk_level IN ('LOW', 'MEDIUM', 'HIGH', 'CRITICAL')),
    controls JSONB, -- Associated control activities
    systems_involved JSONB, -- IT systems used in this process
    frequency VARCHAR(20) NOT NULL DEFAULT 'DAILY' CHECK (frequency IN ('REAL_TIME', 'HOURLY', 'DAILY', 'WEEKLY', 'MONTHLY', 'QUARTERLY', 'ANNUALLY')),
    regulatory_mappings JSONB, -- Which regulations apply
    last_assessment_date TIMESTAMP WITH TIME ZONE,
    next_review_date DATE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Regulatory impact assessments (AI-generated)
CREATE TABLE IF NOT EXISTS regulatory_impacts (
    impact_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    document_id UUID NOT NULL REFERENCES regulatory_documents(document_id),
    process_id UUID NOT NULL REFERENCES business_processes(process_id),
    impact_level VARCHAR(10) NOT NULL CHECK (impact_level IN ('NONE', 'LOW', 'MEDIUM', 'HIGH', 'CRITICAL')),
    impact_description TEXT,
    required_actions JSONB,
    implementation_cost_estimate DECIMAL(15,2),
    implementation_time_estimate_days INTEGER,
    risk_score DECIMAL(3,2),
    priority_score DECIMAL(3,2),
    assessment_confidence DECIMAL(3,2),
    assessed_by_agent VARCHAR(100),
    human_review_required BOOLEAN DEFAULT FALSE,
    human_review_completed BOOLEAN DEFAULT FALSE,
    human_review_notes TEXT,
    status VARCHAR(20) NOT NULL DEFAULT 'ASSESSED' CHECK (status IN ('ASSESSED', 'REVIEWED', 'IMPLEMENTED', 'MONITORED')),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Regulatory change notifications and alerts
CREATE TABLE IF NOT EXISTS regulatory_alerts (
    alert_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    document_id UUID NOT NULL REFERENCES regulatory_documents(document_id),
    alert_type VARCHAR(50) NOT NULL,
    title VARCHAR(255) NOT NULL,
    message TEXT NOT NULL,
    recipients JSONB, -- Who should receive this alert
    priority VARCHAR(10) NOT NULL DEFAULT 'MEDIUM' CHECK (priority IN ('LOW', 'MEDIUM', 'HIGH', 'CRITICAL')),
    sent_at TIMESTAMP WITH TIME ZONE,
    read_by JSONB DEFAULT '[]'::jsonb,
    action_required BOOLEAN DEFAULT FALSE,
    action_deadline DATE,
    action_status VARCHAR(20) DEFAULT 'PENDING' CHECK (action_status IN ('PENDING', 'IN_PROGRESS', 'COMPLETED', 'OVERDUE')),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- =============================================================================
-- POC 3: AUDIT TRAIL INTELLIGENCE AGENT TABLES
-- =============================================================================

-- System audit logs from various sources
CREATE TABLE IF NOT EXISTS system_audit_logs (
    log_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    system_name VARCHAR(100) NOT NULL,
    log_level VARCHAR(10) NOT NULL CHECK (log_level IN ('DEBUG', 'INFO', 'WARN', 'ERROR', 'FATAL')),
    event_type VARCHAR(100) NOT NULL,
    event_description TEXT,
    user_id VARCHAR(100),
    session_id VARCHAR(255),
    ip_address INET,
    user_agent TEXT,
    resource_accessed VARCHAR(500),
    action_performed VARCHAR(100),
    parameters JSONB,
    result_status VARCHAR(20),
    error_message TEXT,
    processing_time_ms INTEGER,
    occurred_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    event_timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    ingested_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Security events and anomalies
CREATE TABLE IF NOT EXISTS security_events (
    event_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    system_name VARCHAR(100) NOT NULL,
    event_type VARCHAR(50) NOT NULL,
    severity VARCHAR(10) NOT NULL CHECK (severity IN ('LOW', 'MEDIUM', 'HIGH', 'CRITICAL')),
    description TEXT NOT NULL,
    source_ip INET,
    destination_ip INET,
    user_id VARCHAR(100),
    session_id VARCHAR(255),
    resource VARCHAR(500),
    action VARCHAR(100),
    event_data JSONB,
    detected_anomalies JSONB,
    risk_score DECIMAL(3,2),
    alert_generated BOOLEAN DEFAULT FALSE,
    alert_id UUID,
    investigated BOOLEAN DEFAULT FALSE,
    investigation_notes TEXT,
    false_positive BOOLEAN DEFAULT FALSE,
    event_timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Agent anomaly detections and patterns
CREATE TABLE IF NOT EXISTS audit_anomalies (
    anomaly_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    anomaly_type VARCHAR(50) NOT NULL,
    severity VARCHAR(10) NOT NULL CHECK (severity IN ('LOW', 'MEDIUM', 'HIGH', 'CRITICAL')),
    description TEXT NOT NULL,
    affected_systems JSONB,
    affected_users JSONB,
    pattern_data JSONB,
    confidence_score DECIMAL(3,2),
    first_detected TIMESTAMP WITH TIME ZONE NOT NULL,
    last_observed TIMESTAMP WITH TIME ZONE NOT NULL,
    frequency_count INTEGER DEFAULT 1,
    risk_assessment JSONB,
    recommended_actions JSONB,
    status VARCHAR(20) NOT NULL DEFAULT 'ACTIVE' CHECK (status IN ('ACTIVE', 'INVESTIGATING', 'MITIGATED', 'FALSE_POSITIVE')),
    assigned_to VARCHAR(100),
    resolution_notes TEXT,
    resolved_at TIMESTAMP WITH TIME ZONE,
    detected_by_agent VARCHAR(100),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Agent learning data and feedback
CREATE TABLE IF NOT EXISTS agent_learning_data (
    learning_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    agent_type VARCHAR(50) NOT NULL,
    agent_name VARCHAR(100) NOT NULL,
    learning_type VARCHAR(50) NOT NULL, -- 'PATTERN', 'THRESHOLD', 'RULE', 'MODEL_UPDATE'
    input_data JSONB,
    output_result JSONB,
    feedback_type VARCHAR(20) NOT NULL CHECK (feedback_type IN ('POSITIVE', 'NEGATIVE', 'NEUTRAL')),
    feedback_score DECIMAL(3,2),
    human_feedback TEXT,
    feedback_provided_by VARCHAR(100),
    feedback_timestamp TIMESTAMP WITH TIME ZONE,
    incorporated BOOLEAN DEFAULT FALSE,
    incorporated_at TIMESTAMP WITH TIME ZONE,
    performance_impact JSONB,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- =============================================================================
-- SHARED AGENT LEARNING TABLES
-- =============================================================================

-- Agent decision history for learning
CREATE TABLE IF NOT EXISTS agent_decision_history (
    history_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    poc_type VARCHAR(50) NOT NULL, -- 'TRANSACTION_GUARDIAN', 'REGULATORY_ASSESSOR', 'AUDIT_INTELLIGENCE'
    agent_name VARCHAR(100) NOT NULL,
    decision_context JSONB,
    decision_made JSONB,
    outcome_result JSONB,
    outcome_score DECIMAL(3,2),
    human_feedback JSONB,
    learning_applied BOOLEAN DEFAULT FALSE,
    decision_timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    feedback_received_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Agent performance metrics
CREATE TABLE IF NOT EXISTS agent_performance_metrics (
    metric_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    poc_type VARCHAR(50) NOT NULL,
    agent_name VARCHAR(100) NOT NULL,
    metric_type VARCHAR(50) NOT NULL,
    metric_name VARCHAR(100) NOT NULL,
    metric_value DECIMAL(15,6),
    benchmark_value DECIMAL(15,6),
    measurement_period_start TIMESTAMP WITH TIME ZONE,
    measurement_period_end TIMESTAMP WITH TIME ZONE,
    calculated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- =============================================================================
-- VECTOR KNOWLEDGE BASE - Advanced Semantic Search and Memory System
-- =============================================================================

-- Vector knowledge entities - Core knowledge storage with embeddings
CREATE TABLE IF NOT EXISTS knowledge_entities (
    entity_id VARCHAR(255) PRIMARY KEY,
    domain VARCHAR(50) NOT NULL CHECK (domain IN ('REGULATORY_COMPLIANCE', 'TRANSACTION_MONITORING', 'AUDIT_INTELLIGENCE', 'BUSINESS_PROCESSES', 'RISK_MANAGEMENT', 'LEGAL_FRAMEWORKS', 'FINANCIAL_INSTRUMENTS', 'MARKET_INTELLIGENCE')),
    knowledge_type VARCHAR(20) NOT NULL CHECK (knowledge_type IN ('FACT', 'RULE', 'PATTERN', 'RELATIONSHIP', 'CONTEXT', 'EXPERIENCE', 'DECISION', 'PREDICTION')),
    title TEXT NOT NULL,
    content TEXT,
    metadata JSONB DEFAULT '{}'::jsonb,
    embedding VECTOR(384), -- Vector embeddings for semantic search
    retention_policy VARCHAR(10) NOT NULL DEFAULT 'PERSISTENT' CHECK (retention_policy IN ('EPHEMERAL', 'SESSION', 'PERSISTENT', 'ARCHIVAL')),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    last_accessed TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    expires_at TIMESTAMP WITH TIME ZONE,
    access_count INTEGER NOT NULL DEFAULT 0,
    confidence_score REAL NOT NULL DEFAULT 1.0 CHECK (confidence_score >= 0.0 AND confidence_score <= 1.0),
    tags TEXT[] DEFAULT ARRAY[]::TEXT[],
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Knowledge entity relationships - Graph structure for connected knowledge
CREATE TABLE IF NOT EXISTS knowledge_relationships (
    relationship_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    source_entity_id VARCHAR(255) NOT NULL REFERENCES knowledge_entities(entity_id) ON DELETE CASCADE,
    target_entity_id VARCHAR(255) NOT NULL REFERENCES knowledge_entities(entity_id) ON DELETE CASCADE,
    relationship_type VARCHAR(100) NOT NULL,
    properties JSONB DEFAULT '{}'::jsonb,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    UNIQUE(source_entity_id, target_entity_id, relationship_type)
);

-- Learning interactions - Track how agents learn from knowledge
CREATE TABLE IF NOT EXISTS learning_interactions (
    interaction_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    agent_type VARCHAR(50) NOT NULL,
    agent_name VARCHAR(100) NOT NULL,
    query_text TEXT NOT NULL,
    selected_entity_id VARCHAR(255) REFERENCES knowledge_entities(entity_id),
    similarity_score REAL,
    reward_score REAL NOT NULL DEFAULT 1.0,
    interaction_context JSONB DEFAULT '{}'::jsonb,
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Knowledge access patterns - Analytics for knowledge usage
CREATE TABLE IF NOT EXISTS knowledge_access_patterns (
    pattern_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    entity_id VARCHAR(255) NOT NULL REFERENCES knowledge_entities(entity_id) ON DELETE CASCADE,
    access_type VARCHAR(20) NOT NULL CHECK (access_type IN ('SEARCH', 'RETRIEVAL', 'UPDATE', 'RELATIONSHIP')),
    agent_type VARCHAR(50),
    agent_name VARCHAR(100),
    query_context TEXT,
    access_timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Vector search cache - Performance optimization
CREATE TABLE IF NOT EXISTS vector_search_cache (
    cache_key VARCHAR(255) PRIMARY KEY,
    query_embedding VECTOR(384),
    result_entity_ids TEXT[] DEFAULT ARRAY[]::TEXT[],
    result_scores REAL[] DEFAULT ARRAY[]::REAL[],
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    expires_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT (NOW() + INTERVAL '1 hour')
);

-- Knowledge quality metrics - Track knowledge accuracy and usefulness
CREATE TABLE IF NOT EXISTS knowledge_quality_metrics (
    metric_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    entity_id VARCHAR(255) NOT NULL REFERENCES knowledge_entities(entity_id) ON DELETE CASCADE,
    metric_type VARCHAR(50) NOT NULL,
    metric_value REAL NOT NULL,
    measured_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    measurement_context JSONB DEFAULT '{}'::jsonb
);

-- =============================================================================
-- DECISION AUDIT & EXPLANATION SYSTEM
-- =============================================================================

-- Decision audit trails - Complete record of agent decision-making process
CREATE TABLE IF NOT EXISTS decision_audit_trails (
    trail_id VARCHAR(255) PRIMARY KEY,
    decision_id VARCHAR(255) UNIQUE NOT NULL,
    agent_type VARCHAR(50) NOT NULL,
    agent_name VARCHAR(100) NOT NULL,
    trigger_event TEXT NOT NULL,
    original_input JSONB NOT NULL,
    final_decision JSONB NOT NULL,
    final_confidence INTEGER NOT NULL CHECK (final_confidence BETWEEN 0 AND 4),
    decision_tree JSONB DEFAULT '{}'::jsonb,
    risk_assessment JSONB DEFAULT '{}'::jsonb,
    alternative_options JSONB DEFAULT '{}'::jsonb,
    started_at TIMESTAMP WITH TIME ZONE NOT NULL,
    completed_at TIMESTAMP WITH TIME ZONE,
    total_processing_time_ms INTEGER,
    requires_human_review BOOLEAN NOT NULL DEFAULT false,
    human_review_reason TEXT,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Individual decision steps - Micro-level audit trail
CREATE TABLE IF NOT EXISTS decision_steps (
    step_id VARCHAR(255) PRIMARY KEY,
    decision_id VARCHAR(255) NOT NULL REFERENCES decision_audit_trails(decision_id) ON DELETE CASCADE,
    event_type INTEGER NOT NULL,
    description TEXT NOT NULL,
    input_data JSONB DEFAULT '{}'::jsonb,
    output_data JSONB DEFAULT '{}'::jsonb,
    metadata JSONB DEFAULT '{}'::jsonb,
    processing_time_us BIGINT NOT NULL DEFAULT 0,
    confidence_impact REAL NOT NULL DEFAULT 0.0,
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    agent_id VARCHAR(255),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Decision explanations - Human-readable explanations generated for each decision
CREATE TABLE IF NOT EXISTS decision_explanations (
    explanation_id VARCHAR(255) PRIMARY KEY,
    decision_id VARCHAR(255) NOT NULL REFERENCES decision_audit_trails(decision_id) ON DELETE CASCADE,
    explanation_level INTEGER NOT NULL CHECK (explanation_level BETWEEN 0 AND 3),
    natural_language_summary TEXT NOT NULL,
    key_factors TEXT[] DEFAULT ARRAY[]::TEXT[],
    risk_indicators TEXT[] DEFAULT ARRAY[]::TEXT[],
    confidence_factors TEXT[] DEFAULT ARRAY[]::TEXT[],
    decision_flowchart JSONB DEFAULT '{}'::jsonb,
    technical_details JSONB DEFAULT '{}'::jsonb,
    human_readable_reasoning TEXT,
    generated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Human reviews and feedback - Human-AI collaboration tracking
CREATE TABLE IF NOT EXISTS human_reviews (
    review_id VARCHAR(255) PRIMARY KEY,
    decision_id VARCHAR(255) NOT NULL REFERENCES decision_audit_trails(decision_id) ON DELETE CASCADE,
    reviewer_id VARCHAR(255) NOT NULL,
    feedback TEXT NOT NULL,
    approved BOOLEAN NOT NULL,
    review_timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    processing_time_ms INTEGER DEFAULT 0,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Indexes for performance
CREATE INDEX IF NOT EXISTS idx_decision_audit_trails_agent ON decision_audit_trails(agent_type, agent_name);
CREATE INDEX IF NOT EXISTS idx_decision_audit_trails_started ON decision_audit_trails(started_at);
CREATE INDEX IF NOT EXISTS idx_decision_audit_trails_human_review ON decision_audit_trails(requires_human_review);
CREATE INDEX IF NOT EXISTS idx_decision_steps_decision ON decision_steps(decision_id);
CREATE INDEX IF NOT EXISTS idx_decision_steps_timestamp ON decision_steps(timestamp);
CREATE INDEX IF NOT EXISTS idx_decision_explanations_decision ON decision_explanations(decision_id);
CREATE INDEX IF NOT EXISTS idx_human_reviews_decision ON human_reviews(decision_id);
CREATE INDEX IF NOT EXISTS idx_human_reviews_reviewer ON human_reviews(reviewer_id);

-- =============================================================================
-- EVENT-DRIVEN ARCHITECTURE - Real-time Event Processing System
-- =============================================================================

-- Events table for event-driven architecture
CREATE TABLE IF NOT EXISTS events (
    event_id VARCHAR(255) PRIMARY KEY,
    category VARCHAR(50) NOT NULL,
    source VARCHAR(255) NOT NULL,
    event_type VARCHAR(100) NOT NULL,
    payload JSONB NOT NULL,
    priority VARCHAR(20) NOT NULL DEFAULT 'NORMAL',
    state VARCHAR(20) NOT NULL DEFAULT 'CREATED',
    retry_count INTEGER NOT NULL DEFAULT 0,
    created_at BIGINT NOT NULL,
    expires_at BIGINT,
    processed_at BIGINT,
    headers JSONB DEFAULT '{}'::jsonb,
    correlation_id VARCHAR(255),
    trace_id VARCHAR(255),
    error_message TEXT
);

-- Indexes for event querying and performance
CREATE INDEX IF NOT EXISTS idx_events_category ON events(category);
CREATE INDEX IF NOT EXISTS idx_events_source ON events(source);
CREATE INDEX IF NOT EXISTS idx_events_created ON events(created_at);
CREATE INDEX IF NOT EXISTS idx_events_state ON events(state);
CREATE INDEX IF NOT EXISTS idx_events_priority ON events(priority);
CREATE INDEX IF NOT EXISTS idx_events_correlation ON events(correlation_id);
CREATE INDEX IF NOT EXISTS idx_events_trace ON events(trace_id);

-- Event handlers registry (for dynamic handler management)
CREATE TABLE IF NOT EXISTS event_handlers (
    handler_id VARCHAR(255) PRIMARY KEY,
    handler_type VARCHAR(50) NOT NULL,
    supported_categories TEXT[] DEFAULT ARRAY[]::TEXT[],
    configuration JSONB DEFAULT '{}'::jsonb,
    is_active BOOLEAN NOT NULL DEFAULT true,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Event routing rules (for complex routing logic)
CREATE TABLE IF NOT EXISTS event_routing_rules (
    rule_id VARCHAR(255) PRIMARY KEY,
    rule_name VARCHAR(100) NOT NULL,
    priority INTEGER NOT NULL DEFAULT 0,
    conditions JSONB NOT NULL,
    actions JSONB NOT NULL,
    is_active BOOLEAN NOT NULL DEFAULT true,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Event processing statistics (for monitoring and analytics)
CREATE TABLE IF NOT EXISTS event_processing_stats (
    stat_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    time_window_start TIMESTAMP WITH TIME ZONE NOT NULL,
    time_window_end TIMESTAMP WITH TIME ZONE NOT NULL,
    events_published BIGINT NOT NULL DEFAULT 0,
    events_processed BIGINT NOT NULL DEFAULT 0,
    events_failed BIGINT NOT NULL DEFAULT 0,
    events_dead_lettered BIGINT NOT NULL DEFAULT 0,
    avg_processing_time_ms REAL,
    peak_queue_size INTEGER,
    active_handlers_count INTEGER,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_event_stats_time_window ON event_processing_stats(time_window_start, time_window_end);

-- =============================================================================
-- TOOL INTEGRATION LAYER - Enterprise Tool Integration Framework
-- =============================================================================

-- Tool configurations registry
CREATE TABLE IF NOT EXISTS tool_configs (
    tool_id VARCHAR(255) PRIMARY KEY,
    tool_name VARCHAR(255) NOT NULL,
    description TEXT,
    category VARCHAR(50) NOT NULL,
    capabilities TEXT[] DEFAULT ARRAY[]::TEXT[],
    auth_type VARCHAR(50) DEFAULT 'NONE',
    auth_config JSONB DEFAULT '{}'::jsonb,
    connection_config JSONB DEFAULT '{}'::jsonb,
    timeout_seconds INTEGER NOT NULL DEFAULT 30,
    max_retries INTEGER NOT NULL DEFAULT 3,
    retry_delay_ms INTEGER NOT NULL DEFAULT 1000,
    rate_limit_per_minute INTEGER NOT NULL DEFAULT 60,
    enabled BOOLEAN NOT NULL DEFAULT true,
    metadata JSONB DEFAULT '{}'::jsonb,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Tool health and metrics
CREATE TABLE IF NOT EXISTS tool_health_metrics (
    tool_id VARCHAR(255) NOT NULL REFERENCES tool_configs(tool_id) ON DELETE CASCADE,
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    status VARCHAR(20) NOT NULL,
    response_time_ms REAL,
    operations_total BIGINT NOT NULL DEFAULT 0,
    operations_successful BIGINT NOT NULL DEFAULT 0,
    operations_failed BIGINT NOT NULL DEFAULT 0,
    operations_retried BIGINT NOT NULL DEFAULT 0,
    rate_limit_hits BIGINT NOT NULL DEFAULT 0,
    timeouts BIGINT NOT NULL DEFAULT 0,
    error_message TEXT,
    PRIMARY KEY (tool_id, timestamp)
);

-- Tool operation logs (for audit and debugging)
CREATE TABLE IF NOT EXISTS tool_operation_logs (
    operation_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tool_id VARCHAR(255) NOT NULL REFERENCES tool_configs(tool_id) ON DELETE CASCADE,
    operation_type VARCHAR(100) NOT NULL,
    parameters JSONB DEFAULT '{}'::jsonb,
    result JSONB DEFAULT '{}'::jsonb,
    success BOOLEAN NOT NULL,
    execution_time_ms INTEGER NOT NULL,
    retry_count INTEGER NOT NULL DEFAULT 0,
    error_message TEXT,
    correlation_id VARCHAR(255),
    trace_id VARCHAR(255),
    started_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    completed_at TIMESTAMP WITH TIME ZONE
);

-- Tool templates and configurations
CREATE TABLE IF NOT EXISTS tool_templates (
    template_id VARCHAR(255) PRIMARY KEY,
    template_name VARCHAR(255) NOT NULL,
    category VARCHAR(50) NOT NULL,
    description TEXT,
    config_template JSONB NOT NULL,
    required_parameters TEXT[] DEFAULT ARRAY[]::TEXT[],
    optional_parameters TEXT[] DEFAULT ARRAY[]::TEXT[],
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Indexes for performance
CREATE INDEX IF NOT EXISTS idx_tool_configs_category ON tool_configs(category);
CREATE INDEX IF NOT EXISTS idx_tool_configs_enabled ON tool_configs(enabled);
CREATE INDEX IF NOT EXISTS idx_tool_health_metrics_timestamp ON tool_health_metrics(timestamp);
CREATE INDEX IF NOT EXISTS idx_tool_health_metrics_status ON tool_health_metrics(status);
CREATE INDEX IF NOT EXISTS idx_tool_operation_logs_tool_id ON tool_operation_logs(tool_id);
CREATE INDEX IF NOT EXISTS idx_tool_operation_logs_started_at ON tool_operation_logs(started_at);
CREATE INDEX IF NOT EXISTS idx_tool_operation_logs_success ON tool_operation_logs(success);
CREATE INDEX IF NOT EXISTS idx_tool_templates_category ON tool_templates(category);

-- Domain-specific indexes for performance
CREATE INDEX IF NOT EXISTS idx_knowledge_entities_domain ON knowledge_entities(domain);
CREATE INDEX IF NOT EXISTS idx_knowledge_entities_type ON knowledge_entities(knowledge_type);
CREATE INDEX IF NOT EXISTS idx_knowledge_entities_created ON knowledge_entities(created_at);
CREATE INDEX IF NOT EXISTS idx_knowledge_entities_accessed ON knowledge_entities(last_accessed);
CREATE INDEX IF NOT EXISTS idx_knowledge_entities_tags ON knowledge_entities USING GIN(tags);
CREATE INDEX IF NOT EXISTS idx_knowledge_entities_embedding ON knowledge_entities USING ivfflat(embedding vector_cosine_ops) WITH (lists = 100);

-- Vector similarity search indexes
CREATE INDEX IF NOT EXISTS idx_vector_search_cache_embedding ON vector_search_cache USING ivfflat(query_embedding vector_cosine_ops) WITH (lists = 50);

-- Relationship indexes for graph traversal
CREATE INDEX IF NOT EXISTS idx_knowledge_relationships_source ON knowledge_relationships(source_entity_id);
CREATE INDEX IF NOT EXISTS idx_knowledge_relationships_target ON knowledge_relationships(target_entity_id);
CREATE INDEX IF NOT EXISTS idx_knowledge_relationships_type ON knowledge_relationships(relationship_type);

-- Learning analytics indexes
CREATE INDEX IF NOT EXISTS idx_learning_interactions_agent ON learning_interactions(agent_type, agent_name);
CREATE INDEX IF NOT EXISTS idx_learning_interactions_timestamp ON learning_interactions(timestamp);

-- Access pattern analytics
CREATE INDEX IF NOT EXISTS idx_access_patterns_entity ON knowledge_access_patterns(entity_id);
CREATE INDEX IF NOT EXISTS idx_access_patterns_timestamp ON knowledge_access_patterns(access_timestamp);

-- Quality metrics indexes
CREATE INDEX IF NOT EXISTS idx_quality_metrics_entity ON knowledge_quality_metrics(entity_id);
CREATE INDEX IF NOT EXISTS idx_quality_metrics_type ON knowledge_quality_metrics(metric_type);

-- Updated at triggers for knowledge entities
CREATE OR REPLACE FUNCTION update_knowledge_entities_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trigger_knowledge_entities_updated_at
    BEFORE UPDATE ON knowledge_entities
    FOR EACH ROW
    EXECUTE FUNCTION update_knowledge_entities_updated_at();

-- Updated at triggers for relationships
CREATE OR REPLACE FUNCTION update_knowledge_relationships_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trigger_knowledge_relationships_updated_at
    BEFORE UPDATE ON knowledge_relationships
    FOR EACH ROW
    EXECUTE FUNCTION update_knowledge_relationships_updated_at();

-- Cleanup functions for expired knowledge
CREATE OR REPLACE FUNCTION cleanup_expired_knowledge()
RETURNS INTEGER AS $$
DECLARE
    deleted_count INTEGER;
BEGIN
    DELETE FROM knowledge_entities
    WHERE expires_at IS NOT NULL AND expires_at < NOW();

    GET DIAGNOSTICS deleted_count = ROW_COUNT;
    RETURN deleted_count;
END;
$$ LANGUAGE plpgsql;

-- Function to get similar entities by vector similarity
CREATE OR REPLACE FUNCTION find_similar_entities(
    query_embedding VECTOR(384),
    similarity_threshold REAL DEFAULT 0.7,
    max_results INTEGER DEFAULT 10,
    target_domain VARCHAR(50) DEFAULT NULL
)
RETURNS TABLE(
    entity_id VARCHAR(255),
    similarity_score REAL
) AS $$
BEGIN
    RETURN QUERY
    SELECT
        ke.entity_id,
        1 - (ke.embedding <=> query_embedding) as similarity_score
    FROM knowledge_entities ke
    WHERE (target_domain IS NULL OR ke.domain = target_domain)
    AND 1 - (ke.embedding <=> query_embedding) >= similarity_threshold
    ORDER BY ke.embedding <=> query_embedding
    LIMIT max_results;
END;
$$ LANGUAGE plpgsql;

-- Function to update access patterns
CREATE OR REPLACE FUNCTION record_knowledge_access(
    p_entity_id VARCHAR(255),
    p_access_type VARCHAR(20),
    p_agent_type VARCHAR(50) DEFAULT NULL,
    p_agent_name VARCHAR(100) DEFAULT NULL,
    p_query_context TEXT DEFAULT NULL
)
RETURNS VOID AS $$
BEGIN
    -- Update last accessed timestamp
    UPDATE knowledge_entities
    SET last_accessed = NOW(),
        access_count = access_count + 1
    WHERE entity_id = p_entity_id;

    -- Record access pattern
    INSERT INTO knowledge_access_patterns (
        entity_id, access_type, agent_type, agent_name, query_context
    ) VALUES (
        p_entity_id, p_access_type, p_agent_type, p_agent_name, p_query_context
    );
END;
$$ LANGUAGE plpgsql;

-- Insert default agent configurations
INSERT INTO agent_configurations (agent_type, agent_name, configuration) VALUES
('transaction_guardian', 'primary_transaction_agent', '{
    "max_concurrent_tasks": 10,
    "processing_timeout_ms": 30000,
    "risk_thresholds": {
        "low": 0.3,
        "medium": 0.6,
        "high": 0.8
    }
}'),
('regulatory_assessor', 'primary_regulatory_agent', '{
    "max_concurrent_tasks": 5,
    "processing_timeout_ms": 60000,
    "analysis_depth": "comprehensive",
    "impact_scoring_enabled": true
}'),
('audit_intelligence', 'primary_audit_agent', '{
    "max_concurrent_tasks": 8,
    "processing_timeout_ms": 45000,
    "anomaly_detection_sensitivity": 0.85,
    "pattern_recognition_enabled": true
}')
ON CONFLICT (agent_type, agent_name, version) DO NOTHING;

-- =============================================================================
-- INDEXES FOR NEW POC TABLES
-- =============================================================================

-- POC 1: Transaction Guardian indexes
CREATE INDEX IF NOT EXISTS idx_customer_profiles_tax_id ON customer_profiles(tax_id);
CREATE INDEX IF NOT EXISTS idx_customer_profiles_risk_rating ON customer_profiles(risk_rating);
CREATE INDEX IF NOT EXISTS idx_customer_profiles_kyc_status ON customer_profiles(kyc_status);
CREATE INDEX IF NOT EXISTS idx_customer_profiles_nationality ON customer_profiles(nationality);
CREATE INDEX IF NOT EXISTS idx_customer_profiles_watchlist ON customer_profiles USING GIN(watchlist_flags);

CREATE INDEX IF NOT EXISTS idx_transactions_customer_id ON transactions(customer_id);
CREATE INDEX IF NOT EXISTS idx_transactions_transaction_date ON transactions(transaction_date);
CREATE INDEX IF NOT EXISTS idx_transactions_amount ON transactions(amount);
CREATE INDEX IF NOT EXISTS idx_transactions_risk_score ON transactions(risk_score);
CREATE INDEX IF NOT EXISTS idx_transactions_flagged ON transactions(flagged);
CREATE INDEX IF NOT EXISTS idx_transactions_status ON transactions(status);
CREATE INDEX IF NOT EXISTS idx_transactions_sender_country ON transactions(sender_country);
CREATE INDEX IF NOT EXISTS idx_transactions_receiver_country ON transactions(receiver_country);

CREATE INDEX IF NOT EXISTS idx_transaction_risk_assessments_transaction_id ON transaction_risk_assessments(transaction_id);
CREATE INDEX IF NOT EXISTS idx_transaction_risk_assessments_agent ON transaction_risk_assessments(agent_name);
CREATE INDEX IF NOT EXISTS idx_transaction_risk_assessments_risk_score ON transaction_risk_assessments(risk_score);
CREATE INDEX IF NOT EXISTS idx_transaction_risk_assessments_assessed_at ON transaction_risk_assessments(assessed_at);

CREATE INDEX IF NOT EXISTS idx_customer_behavior_patterns_customer_id ON customer_behavior_patterns(customer_id);
CREATE INDEX IF NOT EXISTS idx_customer_behavior_patterns_type ON customer_behavior_patterns(pattern_type);
CREATE INDEX IF NOT EXISTS idx_customer_behavior_patterns_agent ON customer_behavior_patterns(detected_by_agent);

-- POC 2: Regulatory Assessor indexes
CREATE INDEX IF NOT EXISTS idx_regulatory_documents_regulator ON regulatory_documents(regulator);
CREATE INDEX IF NOT EXISTS idx_regulatory_documents_status ON regulatory_documents(status);
CREATE INDEX IF NOT EXISTS idx_regulatory_documents_effective_date ON regulatory_documents(effective_date);
CREATE INDEX IF NOT EXISTS idx_regulatory_documents_sector ON regulatory_documents(sector);
CREATE INDEX IF NOT EXISTS idx_regulatory_documents_requirements ON regulatory_documents USING GIN(key_requirements);
CREATE INDEX IF NOT EXISTS idx_regulatory_documents_entities ON regulatory_documents USING GIN(affected_entities);

CREATE INDEX IF NOT EXISTS idx_business_processes_category ON business_processes(process_category);
CREATE INDEX IF NOT EXISTS idx_business_processes_department ON business_processes(department);
CREATE INDEX IF NOT EXISTS idx_business_processes_risk_level ON business_processes(risk_level);
CREATE INDEX IF NOT EXISTS idx_business_processes_owner ON business_processes(owner);
CREATE INDEX IF NOT EXISTS idx_business_processes_controls ON business_processes USING GIN(controls);
CREATE INDEX IF NOT EXISTS idx_business_processes_systems ON business_processes USING GIN(systems_involved);

CREATE INDEX IF NOT EXISTS idx_regulatory_impacts_document_id ON regulatory_impacts(document_id);
CREATE INDEX IF NOT EXISTS idx_regulatory_impacts_process_id ON regulatory_impacts(process_id);
CREATE INDEX IF NOT EXISTS idx_regulatory_impacts_impact_level ON regulatory_impacts(impact_level);
CREATE INDEX IF NOT EXISTS idx_regulatory_impacts_status ON regulatory_impacts(status);
CREATE INDEX IF NOT EXISTS idx_regulatory_impacts_agent ON regulatory_impacts(assessed_by_agent);
CREATE INDEX IF NOT EXISTS idx_regulatory_impacts_priority ON regulatory_impacts(priority_score);

CREATE INDEX IF NOT EXISTS idx_regulatory_alerts_document_id ON regulatory_alerts(document_id);
CREATE INDEX IF NOT EXISTS idx_regulatory_alerts_priority ON regulatory_alerts(priority);
CREATE INDEX IF NOT EXISTS idx_regulatory_alerts_status ON regulatory_alerts(action_status);
CREATE INDEX IF NOT EXISTS idx_regulatory_alerts_sent_at ON regulatory_alerts(sent_at);

-- POC 3: Audit Intelligence indexes
CREATE INDEX IF NOT EXISTS idx_system_audit_logs_system_name ON system_audit_logs(system_name);
CREATE INDEX IF NOT EXISTS idx_system_audit_logs_event_type ON system_audit_logs(event_type);
CREATE INDEX IF NOT EXISTS idx_system_audit_logs_user_id ON system_audit_logs(user_id);
CREATE INDEX IF NOT EXISTS idx_system_audit_logs_event_timestamp ON system_audit_logs(event_timestamp);
CREATE INDEX IF NOT EXISTS idx_system_audit_logs_ip_address ON system_audit_logs(ip_address);
CREATE INDEX IF NOT EXISTS idx_system_audit_logs_session_id ON system_audit_logs(session_id);
CREATE INDEX IF NOT EXISTS idx_system_audit_logs_parameters ON system_audit_logs USING GIN(parameters);

CREATE INDEX IF NOT EXISTS idx_security_events_system_name ON security_events(system_name);
CREATE INDEX IF NOT EXISTS idx_security_events_event_type ON security_events(event_type);
CREATE INDEX IF NOT EXISTS idx_security_events_severity ON security_events(severity);
CREATE INDEX IF NOT EXISTS idx_security_events_user_id ON security_events(user_id);
CREATE INDEX IF NOT EXISTS idx_security_events_risk_score ON security_events(risk_score);
CREATE INDEX IF NOT EXISTS idx_security_events_alert_generated ON security_events(alert_generated);
CREATE INDEX IF NOT EXISTS idx_security_events_false_positive ON security_events(false_positive);
CREATE INDEX IF NOT EXISTS idx_security_events_source_ip ON security_events(source_ip);
CREATE INDEX IF NOT EXISTS idx_security_events_event_timestamp ON security_events(event_timestamp);

CREATE INDEX IF NOT EXISTS idx_audit_anomalies_type ON audit_anomalies(anomaly_type);
CREATE INDEX IF NOT EXISTS idx_audit_anomalies_severity ON audit_anomalies(severity);
CREATE INDEX IF NOT EXISTS idx_audit_anomalies_status ON audit_anomalies(status);
CREATE INDEX IF NOT EXISTS idx_audit_anomalies_agent ON audit_anomalies(detected_by_agent);
CREATE INDEX IF NOT EXISTS idx_audit_anomalies_first_detected ON audit_anomalies(first_detected);
CREATE INDEX IF NOT EXISTS idx_audit_anomalies_last_observed ON audit_anomalies(last_observed);

CREATE INDEX IF NOT EXISTS idx_agent_learning_data_agent ON agent_learning_data(agent_type, agent_name);
CREATE INDEX IF NOT EXISTS idx_agent_learning_data_type ON agent_learning_data(learning_type);
CREATE INDEX IF NOT EXISTS idx_agent_learning_data_feedback ON agent_learning_data(feedback_type);
CREATE INDEX IF NOT EXISTS idx_agent_learning_data_incorporated ON agent_learning_data(incorporated);

-- Shared agent learning indexes
CREATE INDEX IF NOT EXISTS idx_agent_decision_history_poc ON agent_decision_history(poc_type);
CREATE INDEX IF NOT EXISTS idx_agent_decision_history_agent ON agent_decision_history(agent_name);
CREATE INDEX IF NOT EXISTS idx_agent_decision_history_timestamp ON agent_decision_history(decision_timestamp);
CREATE INDEX IF NOT EXISTS idx_agent_decision_history_learning ON agent_decision_history(learning_applied);

CREATE INDEX IF NOT EXISTS idx_agent_performance_metrics_poc ON agent_performance_metrics(poc_type);
CREATE INDEX IF NOT EXISTS idx_agent_performance_metrics_agent ON agent_performance_metrics(agent_name);
CREATE INDEX IF NOT EXISTS idx_agent_performance_metrics_type ON agent_performance_metrics(metric_type);

-- Full text search indexes for new tables
CREATE INDEX IF NOT EXISTS idx_customer_profiles_name_trgm ON customer_profiles USING GIN(full_name gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_transactions_description_trgm ON transactions USING GIN(description gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_regulatory_documents_title_trgm ON regulatory_documents USING GIN(title gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_regulatory_documents_text_trgm ON regulatory_documents USING GIN(full_text gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_business_processes_name_trgm ON business_processes USING GIN(process_name gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_system_audit_logs_description_trgm ON system_audit_logs USING GIN(event_description gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_security_events_description_trgm ON security_events USING GIN(description gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_audit_anomalies_description_trgm ON audit_anomalies USING GIN(description gin_trgm_ops);

-- Function call audit logs table
CREATE TABLE IF NOT EXISTS function_call_audit (
    audit_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    correlation_id VARCHAR(255) NOT NULL,
    agent_id VARCHAR(255) NOT NULL,
    agent_type VARCHAR(100) NOT NULL,
    function_name VARCHAR(255) NOT NULL,
    function_category VARCHAR(50) NOT NULL,
    parameters JSONB,
    result JSONB,
    success BOOLEAN NOT NULL,
    error_message TEXT,
    execution_time_ms INTEGER NOT NULL,
    required_permissions TEXT[],
    actual_permissions TEXT[],
    ip_address INET,
    user_agent TEXT,
    request_timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    completion_timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Function call metrics table
CREATE TABLE IF NOT EXISTS function_call_metrics (
    metric_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    function_name VARCHAR(255) NOT NULL,
    total_calls BIGINT NOT NULL DEFAULT 0,
    successful_calls BIGINT NOT NULL DEFAULT 0,
    failed_calls BIGINT NOT NULL DEFAULT 0,
    average_execution_time_ms DOUBLE PRECISION DEFAULT 0,
    min_execution_time_ms INTEGER DEFAULT 0,
    max_execution_time_ms INTEGER DEFAULT 0,
    last_called_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    UNIQUE(function_name)
);

-- =============================================================================
-- UPDATE TRIGGERS FOR NEW TABLES
-- =============================================================================

-- Function call audit indexes
CREATE INDEX IF NOT EXISTS idx_function_call_audit_correlation ON function_call_audit(correlation_id);
CREATE INDEX IF NOT EXISTS idx_function_call_audit_agent ON function_call_audit(agent_id, agent_type);
CREATE INDEX IF NOT EXISTS idx_function_call_audit_function ON function_call_audit(function_name);
CREATE INDEX IF NOT EXISTS idx_function_call_audit_success ON function_call_audit(success);
CREATE INDEX IF NOT EXISTS idx_function_call_audit_timestamp ON function_call_audit(request_timestamp);
CREATE INDEX IF NOT EXISTS idx_function_call_audit_completion ON function_call_audit(completion_timestamp);

-- Function call metrics indexes
CREATE INDEX IF NOT EXISTS idx_function_call_metrics_function ON function_call_metrics(function_name);
CREATE INDEX IF NOT EXISTS idx_function_call_metrics_calls ON function_call_metrics(total_calls);
CREATE INDEX IF NOT EXISTS idx_function_call_metrics_performance ON function_call_metrics(average_execution_time_ms);

CREATE TRIGGER update_customer_profiles_updated_at BEFORE UPDATE ON customer_profiles FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_business_processes_updated_at BEFORE UPDATE ON business_processes FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_regulatory_impacts_updated_at BEFORE UPDATE ON regulatory_impacts FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_audit_anomalies_updated_at BEFORE UPDATE ON audit_anomalies FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_function_call_metrics_updated_at BEFORE UPDATE ON function_call_metrics FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

-- =============================================================================
-- ADVANCED MEMORY SYSTEMS - Conversation Memory and Learning
-- =============================================================================

-- Conversation memory table - Persistent storage of conversation history
CREATE TABLE IF NOT EXISTS conversation_memory (
    conversation_id VARCHAR(255) PRIMARY KEY,
    agent_type VARCHAR(50) NOT NULL,
    agent_name VARCHAR(100) NOT NULL,
    context_type VARCHAR(50) NOT NULL, -- 'REGULATORY_COMPLIANCE', 'RISK_ASSESSMENT', etc.
    conversation_topic TEXT,
    participants JSONB NOT NULL DEFAULT '[]'::jsonb, -- Array of participant IDs/roles
    memory_type VARCHAR(20) NOT NULL DEFAULT 'EPISODIC' CHECK (memory_type IN ('EPISODIC', 'SEMANTIC', 'PROCEDURAL', 'WORKING')),
    importance_score REAL NOT NULL DEFAULT 1.0 CHECK (importance_score >= 0.0 AND importance_score <= 1.0),
    emotional_valence REAL DEFAULT 0.0 CHECK (emotional_valence >= -1.0 AND emotional_valence <= 1.0),
    confidence_score REAL NOT NULL DEFAULT 1.0 CHECK (confidence_score >= 0.0 AND confidence_score <= 1.0),
    embedding VECTOR(384), -- Semantic embedding for similarity search
    summary TEXT, -- AI-generated summary of the conversation
    key_insights TEXT[] DEFAULT ARRAY[]::TEXT[], -- Extracted key points
    tags TEXT[] DEFAULT ARRAY[]::TEXT[], -- User-defined tags
    metadata JSONB DEFAULT '{}'::jsonb, -- Additional context
    last_accessed TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    expires_at TIMESTAMP WITH TIME ZONE,
    access_count INTEGER NOT NULL DEFAULT 0,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Individual conversation turns/messages
CREATE TABLE IF NOT EXISTS conversation_turns (
    turn_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    conversation_id VARCHAR(255) NOT NULL REFERENCES conversation_memory(conversation_id) ON DELETE CASCADE,
    turn_number INTEGER NOT NULL,
    speaker_role VARCHAR(50) NOT NULL, -- 'AGENT', 'HUMAN', 'SYSTEM', etc.
    speaker_id VARCHAR(255), -- Specific agent or user ID
    message_type VARCHAR(20) NOT NULL DEFAULT 'TEXT' CHECK (message_type IN ('TEXT', 'DECISION', 'ACTION', 'QUESTION', 'RESPONSE')),
    content TEXT NOT NULL,
    intent VARCHAR(100), -- AI-detected intent of the message
    sentiment REAL, -- Sentiment score (-1 to 1)
    confidence REAL, -- Confidence in analysis
    decision_context JSONB, -- If this turn involved a decision
    action_taken JSONB, -- If this turn resulted in an action
    metadata JSONB DEFAULT '{}'::jsonb,
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    UNIQUE(conversation_id, turn_number)
);

-- Learning feedback table - Human feedback on agent decisions and learning
CREATE TABLE IF NOT EXISTS learning_feedback (
    feedback_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    conversation_id VARCHAR(255) REFERENCES conversation_memory(conversation_id),
    decision_id VARCHAR(255), -- Link to specific decision if applicable
    agent_type VARCHAR(50) NOT NULL,
    agent_name VARCHAR(100) NOT NULL,
    feedback_type VARCHAR(20) NOT NULL CHECK (feedback_type IN ('POSITIVE', 'NEGATIVE', 'NEUTRAL', 'CORRECTION', 'SUGGESTION')),
    feedback_score REAL NOT NULL CHECK (feedback_score >= -1.0 AND feedback_score <= 1.0),
    feedback_text TEXT,
    human_reviewer_id VARCHAR(255) NOT NULL,
    human_reviewer_role VARCHAR(100),
    correction_provided JSONB, -- If correction was suggested
    learning_applied BOOLEAN NOT NULL DEFAULT FALSE,
    learning_notes TEXT, -- What the agent learned
    metadata JSONB DEFAULT '{}'::jsonb,
    feedback_timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    learning_applied_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Case-based reasoning knowledge base
CREATE TABLE IF NOT EXISTS case_base (
    case_id VARCHAR(255) PRIMARY KEY,
    domain VARCHAR(50) NOT NULL, -- 'REGULATORY_COMPLIANCE', 'RISK_ASSESSMENT', etc.
    case_type VARCHAR(50) NOT NULL, -- 'SUCCESS', 'FAILURE', 'PARTIAL_SUCCESS', 'EDGE_CASE'
    problem_description TEXT NOT NULL,
    solution_description TEXT NOT NULL,
    context_factors JSONB NOT NULL, -- Environmental/contextual factors
    outcome_metrics JSONB, -- Quantitative outcomes
    lessons_learned TEXT[] DEFAULT ARRAY[]::TEXT[],
    similar_cases TEXT[] DEFAULT ARRAY[]::TEXT[], -- IDs of similar cases
    confidence_score REAL NOT NULL DEFAULT 1.0 CHECK (confidence_score >= 0.0 AND confidence_score <= 1.0),
    embedding VECTOR(384), -- Semantic embedding for case retrieval
    tags TEXT[] DEFAULT ARRAY[]::TEXT[],
    retention_policy VARCHAR(10) NOT NULL DEFAULT 'PERSISTENT' CHECK (retention_policy IN ('EPHEMERAL', 'SESSION', 'PERSISTENT', 'ARCHIVAL')),
    last_used TIMESTAMP WITH TIME ZONE,
    usage_count INTEGER NOT NULL DEFAULT 0,
    expires_at TIMESTAMP WITH TIME ZONE,
    created_by_agent VARCHAR(100),
    reviewed_by_human BOOLEAN DEFAULT FALSE,
    human_review_notes TEXT,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Memory consolidation log - Tracks memory optimization and cleanup
CREATE TABLE IF NOT EXISTS memory_consolidation_log (
    consolidation_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    consolidation_type VARCHAR(50) NOT NULL, -- 'FORGETTING', 'GENERALIZATION', 'COMPRESSION', 'MERGE'
    memory_type VARCHAR(20) NOT NULL CHECK (memory_type IN ('EPISODIC', 'SEMANTIC', 'PROCEDURAL', 'WORKING')),
    target_memory_ids TEXT[] NOT NULL, -- IDs of memories affected
    consolidation_criteria JSONB, -- What triggered this consolidation
    consolidation_result JSONB, -- What changed
    performance_impact REAL, -- Measured impact on performance
    space_saved_bytes BIGINT,
    triggered_by VARCHAR(50) DEFAULT 'AUTOMATIC', -- 'AUTOMATIC', 'MANUAL', 'PRESSURE'
    executed_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    execution_time_ms INTEGER,
    success BOOLEAN NOT NULL DEFAULT TRUE,
    error_message TEXT
);

-- Memory access patterns - Analytics for memory usage
CREATE TABLE IF NOT EXISTS memory_access_patterns (
    pattern_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    memory_id VARCHAR(255) NOT NULL, -- Could reference conversation_id or case_id
    memory_type VARCHAR(20) NOT NULL CHECK (memory_type IN ('CONVERSATION', 'CASE', 'FEEDBACK')),
    access_type VARCHAR(20) NOT NULL CHECK (access_type IN ('RETRIEVAL', 'UPDATE', 'SEARCH', 'LEARNING')),
    agent_type VARCHAR(50),
    agent_name VARCHAR(100),
    query_context TEXT, -- What was being searched for
    similarity_score REAL, -- If retrieved via similarity search
    access_result VARCHAR(20) NOT NULL DEFAULT 'SUCCESS' CHECK (access_result IN ('SUCCESS', 'PARTIAL', 'FAILED', 'NOT_FOUND')),
    access_timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    processing_time_ms INTEGER,
    user_satisfaction_score REAL CHECK (user_satisfaction_score >= 0.0 AND user_satisfaction_score <= 1.0)
);

-- Learning model versions - Track evolution of learned behaviors
CREATE TABLE IF NOT EXISTS learning_model_versions (
    model_version_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    agent_type VARCHAR(50) NOT NULL,
    agent_name VARCHAR(100) NOT NULL,
    learning_type VARCHAR(50) NOT NULL, -- 'SUPERVISED', 'REINFORCEMENT', 'IMITATION', 'SELF_SUPERVISED'
    model_data JSONB NOT NULL, -- Serialized model parameters/weights
    training_data_summary JSONB, -- Summary of training data used
    performance_metrics JSONB, -- Accuracy, precision, recall, etc.
    validation_metrics JSONB, -- Cross-validation results
    model_size_bytes BIGINT,
    training_time_ms BIGINT,
    inference_time_ms_avg REAL,
    version_number INTEGER NOT NULL,
    is_active BOOLEAN NOT NULL DEFAULT FALSE,
    deployed_at TIMESTAMP WITH TIME ZONE,
    retired_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    UNIQUE(agent_type, agent_name, version_number)
);

-- =============================================================================
-- MEMORY SYSTEM INDEXES
-- =============================================================================

-- Conversation memory indexes
CREATE INDEX IF NOT EXISTS idx_conversation_memory_agent ON conversation_memory(agent_type, agent_name);
CREATE INDEX IF NOT EXISTS idx_conversation_memory_type ON conversation_memory(memory_type);
CREATE INDEX IF NOT EXISTS idx_conversation_memory_context ON conversation_memory(context_type);
CREATE INDEX IF NOT EXISTS idx_conversation_memory_importance ON conversation_memory(importance_score);
CREATE INDEX IF NOT EXISTS idx_conversation_memory_accessed ON conversation_memory(last_accessed);
CREATE INDEX IF NOT EXISTS idx_conversation_memory_created ON conversation_memory(created_at);
CREATE INDEX IF NOT EXISTS idx_conversation_memory_tags ON conversation_memory USING GIN(tags);
CREATE INDEX IF NOT EXISTS idx_conversation_memory_embedding ON conversation_memory USING ivfflat(embedding vector_cosine_ops) WITH (lists = 100);

-- Conversation turns indexes
CREATE INDEX IF NOT EXISTS idx_conversation_turns_conversation ON conversation_turns(conversation_id);
CREATE INDEX IF NOT EXISTS idx_conversation_turns_speaker ON conversation_turns(speaker_role, speaker_id);
CREATE INDEX IF NOT EXISTS idx_conversation_turns_type ON conversation_turns(message_type);
CREATE INDEX IF NOT EXISTS idx_conversation_turns_timestamp ON conversation_turns(timestamp);

-- Learning feedback indexes
CREATE INDEX IF NOT EXISTS idx_learning_feedback_conversation ON learning_feedback(conversation_id);
CREATE INDEX IF NOT EXISTS idx_learning_feedback_decision ON learning_feedback(decision_id);
CREATE INDEX IF NOT EXISTS idx_learning_feedback_agent ON learning_feedback(agent_type, agent_name);
CREATE INDEX IF NOT EXISTS idx_learning_feedback_type ON learning_feedback(feedback_type);
CREATE INDEX IF NOT EXISTS idx_learning_feedback_score ON learning_feedback(feedback_score);
CREATE INDEX IF NOT EXISTS idx_learning_feedback_timestamp ON learning_feedback(feedback_timestamp);
CREATE INDEX IF NOT EXISTS idx_learning_feedback_applied ON learning_feedback(learning_applied);

-- Case base indexes
CREATE INDEX IF NOT EXISTS idx_case_base_domain ON case_base(domain);
CREATE INDEX IF NOT EXISTS idx_case_base_type ON case_base(case_type);
CREATE INDEX IF NOT EXISTS idx_case_base_confidence ON case_base(confidence_score);
CREATE INDEX IF NOT EXISTS idx_case_base_used ON case_base(last_used);
CREATE INDEX IF NOT EXISTS idx_case_base_created ON case_base(created_at);
CREATE INDEX IF NOT EXISTS idx_case_base_tags ON case_base USING GIN(tags);
CREATE INDEX IF NOT EXISTS idx_case_base_similar ON case_base USING GIN(similar_cases);
CREATE INDEX IF NOT EXISTS idx_case_base_lessons ON case_base USING GIN(lessons_learned);
CREATE INDEX IF NOT EXISTS idx_case_base_embedding ON case_base USING ivfflat(embedding vector_cosine_ops) WITH (lists = 100);

-- Memory consolidation indexes
CREATE INDEX IF NOT EXISTS idx_memory_consolidation_type ON memory_consolidation_log(consolidation_type);
CREATE INDEX IF NOT EXISTS idx_memory_consolidation_memory_type ON memory_consolidation_log(memory_type);
CREATE INDEX IF NOT EXISTS idx_memory_consolidation_executed ON memory_consolidation_log(executed_at);
CREATE INDEX IF NOT EXISTS idx_memory_consolidation_success ON memory_consolidation_log(success);

-- Memory access patterns indexes
CREATE INDEX IF NOT EXISTS idx_memory_access_patterns_memory ON memory_access_patterns(memory_id, memory_type);
CREATE INDEX IF NOT EXISTS idx_memory_access_patterns_agent ON memory_access_patterns(agent_type, agent_name);
CREATE INDEX IF NOT EXISTS idx_memory_access_patterns_type ON memory_access_patterns(access_type);
CREATE INDEX IF NOT EXISTS idx_memory_access_patterns_timestamp ON memory_access_patterns(access_timestamp);
CREATE INDEX IF NOT EXISTS idx_memory_access_patterns_result ON memory_access_patterns(access_result);

-- Learning model versions indexes
CREATE INDEX IF NOT EXISTS idx_learning_model_versions_agent ON learning_model_versions(agent_type, agent_name);
CREATE INDEX IF NOT EXISTS idx_learning_model_versions_type ON learning_model_versions(learning_type);
CREATE INDEX IF NOT EXISTS idx_learning_model_versions_active ON learning_model_versions(is_active);
CREATE INDEX IF NOT EXISTS idx_learning_model_versions_deployed ON learning_model_versions(deployed_at);

-- Full text search indexes for memory system
CREATE INDEX IF NOT EXISTS idx_conversation_memory_topic_trgm ON conversation_memory USING GIN(conversation_topic gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_conversation_memory_summary_trgm ON conversation_memory USING GIN(summary gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_conversation_turns_content_trgm ON conversation_turns USING GIN(content gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_case_base_problem_trgm ON case_base USING GIN(problem_description gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_case_base_solution_trgm ON case_base USING GIN(solution_description gin_trgm_ops);

-- =============================================================================
-- MEMORY SYSTEM TRIGGERS
-- =============================================================================

CREATE TRIGGER update_conversation_memory_updated_at BEFORE UPDATE ON conversation_memory FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_case_base_updated_at BEFORE UPDATE ON case_base FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_learning_model_versions_updated_at BEFORE UPDATE ON learning_model_versions FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

-- =============================================================================
-- MEMORY SYSTEM UTILITY FUNCTIONS
-- =============================================================================

-- Function to find similar conversations by embedding
CREATE OR REPLACE FUNCTION find_similar_conversations(
    query_embedding VECTOR(384),
    similarity_threshold REAL DEFAULT 0.7,
    max_results INTEGER DEFAULT 10,
    target_agent_type VARCHAR(50) DEFAULT NULL,
    target_context_type VARCHAR(50) DEFAULT NULL
)
RETURNS TABLE(
    conversation_id VARCHAR(255),
    similarity_score REAL,
    agent_type VARCHAR(50),
    agent_name VARCHAR(100),
    conversation_topic TEXT,
    summary TEXT
) AS $$
BEGIN
    RETURN QUERY
    SELECT
        cm.conversation_id,
        1 - (cm.embedding <=> query_embedding) as similarity_score,
        cm.agent_type,
        cm.agent_name,
        cm.conversation_topic,
        cm.summary
    FROM conversation_memory cm
    WHERE (target_agent_type IS NULL OR cm.agent_type = target_agent_type)
    AND (target_context_type IS NULL OR cm.context_type = target_context_type)
    AND cm.embedding IS NOT NULL
    AND 1 - (cm.embedding <=> query_embedding) >= similarity_threshold
    ORDER BY cm.embedding <=> query_embedding
    LIMIT max_results;
END;
$$ LANGUAGE plpgsql;

-- Function to find similar cases by embedding
CREATE OR REPLACE FUNCTION find_similar_cases(
    query_embedding VECTOR(384),
    similarity_threshold REAL DEFAULT 0.7,
    max_results INTEGER DEFAULT 10,
    target_domain VARCHAR(50) DEFAULT NULL
)
RETURNS TABLE(
    case_id VARCHAR(255),
    similarity_score REAL,
    domain VARCHAR(50),
    case_type VARCHAR(50),
    problem_description TEXT,
    solution_description TEXT,
    confidence_score REAL
) AS $$
BEGIN
    RETURN QUERY
    SELECT
        cb.case_id,
        1 - (cb.embedding <=> query_embedding) as similarity_score,
        cb.domain,
        cb.case_type,
        cb.problem_description,
        cb.solution_description,
        cb.confidence_score
    FROM case_base cb
    WHERE (target_domain IS NULL OR cb.domain = target_domain)
    AND cb.embedding IS NOT NULL
    AND 1 - (cb.embedding <=> query_embedding) >= similarity_threshold
    ORDER BY cb.embedding <=> query_embedding
    LIMIT max_results;
END;
$$ LANGUAGE plpgsql;

-- Function to consolidate old memories based on importance and access patterns
CREATE OR REPLACE FUNCTION consolidate_memory_by_importance(
    memory_type_filter VARCHAR(20) DEFAULT NULL,
    max_age_days INTEGER DEFAULT 90,
    importance_threshold REAL DEFAULT 0.3,
    max_memories_to_consolidate INTEGER DEFAULT 1000
)
RETURNS INTEGER AS $$
DECLARE
    consolidated_count INTEGER := 0;
    memory_record RECORD;
BEGIN
    -- Mark low-importance, old memories for consolidation
    FOR memory_record IN
        SELECT conversation_id, importance_score, created_at, last_accessed
        FROM conversation_memory
        WHERE (memory_type_filter IS NULL OR memory_type = memory_type_filter)
        AND importance_score < importance_threshold
        AND (last_accessed IS NULL OR last_accessed < NOW() - INTERVAL '1 day' * max_age_days)
        AND expires_at IS NULL
        ORDER BY importance_score ASC, last_accessed ASC
        LIMIT max_memories_to_consolidate
    LOOP
        -- Mark for expiration in 30 days
        UPDATE conversation_memory
        SET expires_at = NOW() + INTERVAL '30 days',
            updated_at = NOW()
        WHERE conversation_id = memory_record.conversation_id;

        consolidated_count := consolidated_count + 1;
    END LOOP;

    -- Log the consolidation
    INSERT INTO memory_consolidation_log (
        consolidation_type, memory_type, target_memory_ids, consolidation_criteria,
        consolidation_result, space_saved_bytes, triggered_by
    ) VALUES (
        'FORGETTING', COALESCE(memory_type_filter, 'ALL'),
        ARRAY[]::TEXT[], -- Would need to collect actual IDs for full logging
        jsonb_build_object(
            'importance_threshold', importance_threshold,
            'max_age_days', max_age_days,
            'max_memories', max_memories_to_consolidate
        ),
        jsonb_build_object('consolidated_count', consolidated_count),
        NULL, -- Space calculation would require additional logic
        'AUTOMATIC'
    );

    RETURN consolidated_count;
END;
$$ LANGUAGE plpgsql;

-- Function to update memory access patterns
CREATE OR REPLACE FUNCTION record_memory_access(
    p_memory_id VARCHAR(255),
    p_memory_type VARCHAR(20),
    p_access_type VARCHAR(20),
    p_agent_type VARCHAR(50) DEFAULT NULL,
    p_agent_name VARCHAR(100) DEFAULT NULL,
    p_query_context TEXT DEFAULT NULL,
    p_similarity_score REAL DEFAULT NULL,
    p_access_result VARCHAR(20) DEFAULT 'SUCCESS',
    p_processing_time_ms INTEGER DEFAULT NULL,
    p_user_satisfaction_score REAL DEFAULT NULL
)
RETURNS VOID AS $$
BEGIN
    -- Update last accessed timestamp for conversations
    IF p_memory_type = 'CONVERSATION' THEN
        UPDATE conversation_memory
        SET last_accessed = NOW(),
            access_count = access_count + 1,
            updated_at = NOW()
        WHERE conversation_id = p_memory_id;
    END IF;

    -- Update last used timestamp for cases
    IF p_memory_type = 'CASE' THEN
        UPDATE case_base
        SET last_used = NOW(),
            usage_count = usage_count + 1,
            updated_at = NOW()
        WHERE case_id = p_memory_id;
    END IF;

    -- Record the access pattern
    INSERT INTO memory_access_patterns (
        memory_id, memory_type, access_type, agent_type, agent_name,
        query_context, similarity_score, access_result, processing_time_ms,
        user_satisfaction_score
    ) VALUES (
        p_memory_id, p_memory_type, p_access_type, p_agent_type, p_agent_name,
        p_query_context, p_similarity_score, p_access_result, p_processing_time_ms,
        p_user_satisfaction_score
    );
END;
$$ LANGUAGE plpgsql;

-- =============================================================================
-- DATA INGESTION METRICS TABLES
-- =============================================================================

-- Data ingestion metrics table - Production-grade monitoring for data pipelines
CREATE TABLE IF NOT EXISTS ingestion_metrics (
    metric_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    source_id VARCHAR(255) NOT NULL,
    metric_type VARCHAR(50) NOT NULL, -- 'BATCH', 'PERFORMANCE', 'HEALTH', 'ERROR'
    metric_data JSONB NOT NULL,
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Create indexes for ingestion metrics
CREATE INDEX IF NOT EXISTS idx_ingestion_metrics_source ON ingestion_metrics(source_id);
CREATE INDEX IF NOT EXISTS idx_ingestion_metrics_type ON ingestion_metrics(metric_type);
CREATE INDEX IF NOT EXISTS idx_ingestion_metrics_timestamp ON ingestion_metrics(timestamp);

-- Add trigger for updated_at
CREATE TRIGGER update_ingestion_metrics_updated_at BEFORE UPDATE ON ingestion_metrics FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();


-- =============================================================================
-- NEW FEATURES TABLES (Added for Circuit Breaker, Redis, Kubernetes, Monitoring)
-- =============================================================================

-- =============================================================================
-- NEW FEATURES TABLES (Added for Circuit Breaker, Redis, Kubernetes, Monitoring)
-- =============================================================================

-- Circuit Breaker Metrics and State Tracking
CREATE TABLE IF NOT EXISTS circuit_breaker_states (
    state_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    service_name VARCHAR(100) NOT NULL,
    current_state VARCHAR(20) NOT NULL CHECK (current_state IN ('CLOSED', 'OPEN', 'HALF_OPEN')),
    failure_count INTEGER NOT NULL DEFAULT 0,
    success_count INTEGER NOT NULL DEFAULT 0,
    consecutive_failures INTEGER NOT NULL DEFAULT 0,
    consecutive_successes INTEGER NOT NULL DEFAULT 0,
    last_failure_time TIMESTAMP WITH TIME ZONE,
    last_success_time TIMESTAMP WITH TIME ZONE,
    state_change_time TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    failure_threshold INTEGER NOT NULL,
    recovery_timeout_seconds INTEGER NOT NULL,
    monitoring_enabled BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    UNIQUE(service_name)
);

-- Circuit Breaker Events and Transitions
CREATE TABLE IF NOT EXISTS circuit_breaker_events (
    event_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    service_name VARCHAR(100) NOT NULL,
    event_type VARCHAR(50) NOT NULL,
    from_state VARCHAR(20),
    to_state VARCHAR(20),
    failure_count INTEGER,
    success_count INTEGER,
    error_message TEXT,
    request_duration_ms INTEGER,
    metadata JSONB,
    event_time TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Redis Cache Operations and Metrics
CREATE TABLE IF NOT EXISTS redis_cache_operations (
    operation_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    cache_type VARCHAR(50) NOT NULL,
    operation VARCHAR(20) NOT NULL CHECK (operation IN ('GET', 'SET', 'DELETE', 'EXISTS')),
    key_hash VARCHAR(128) NOT NULL,
    success BOOLEAN NOT NULL,
    response_time_ms INTEGER NOT NULL,
    hit BOOLEAN,
    ttl_seconds INTEGER,
    data_size_bytes INTEGER,
    error_message TEXT,
    operation_time TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Health Check Results and Metrics
CREATE TABLE IF NOT EXISTS health_check_results (
    result_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    check_name VARCHAR(100) NOT NULL,
    check_type VARCHAR(20) NOT NULL CHECK (check_type IN ('READINESS', 'LIVENESS', 'STARTUP')),
    healthy BOOLEAN NOT NULL,
    status VARCHAR(20) NOT NULL,
    message TEXT,
    response_time_ms INTEGER,
    details JSONB,
    check_time TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Kubernetes Operator Events and Actions
CREATE TABLE IF NOT EXISTS kubernetes_operator_events (
    event_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    operator_name VARCHAR(100) NOT NULL,
    resource_type VARCHAR(50) NOT NULL,
    resource_name VARCHAR(255) NOT NULL,
    namespace VARCHAR(100) NOT NULL,
    event_type VARCHAR(50) NOT NULL,
    reason VARCHAR(100),
    message TEXT,
    involved_object JSONB,
    metadata JSONB,
    event_time TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Alert History and Resolution Tracking
CREATE TABLE IF NOT EXISTS alert_history (
    alert_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    alert_name VARCHAR(255) NOT NULL,
    severity VARCHAR(20) NOT NULL CHECK (severity IN ('critical', 'warning', 'info')),
    status VARCHAR(20) NOT NULL DEFAULT 'firing' CHECK (status IN ('firing', 'resolved', 'acknowledged')),
    description TEXT,
    summary TEXT,
    labels JSONB,
    annotations JSONB,
    starts_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    ends_at TIMESTAMP WITH TIME ZONE,
    resolved_by VARCHAR(100),
    resolution_notes TEXT,
    false_positive BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Create indexes for performance
CREATE INDEX IF NOT EXISTS idx_circuit_breaker_states_service ON circuit_breaker_states(service_name);
CREATE INDEX IF NOT EXISTS idx_circuit_breaker_events_service ON circuit_breaker_events(service_name);
CREATE INDEX IF NOT EXISTS idx_circuit_breaker_events_time ON circuit_breaker_events(event_time);
CREATE INDEX IF NOT EXISTS idx_redis_operations_type_time ON redis_cache_operations(cache_type, operation_time);
CREATE INDEX IF NOT EXISTS idx_k8s_operator_events_resource ON kubernetes_operator_events(resource_type, resource_name);
CREATE INDEX IF NOT EXISTS idx_k8s_operator_events_time ON kubernetes_operator_events(event_time);
CREATE INDEX IF NOT EXISTS idx_health_results_check_time ON health_check_results(check_name, check_time);
CREATE INDEX IF NOT EXISTS idx_alert_history_status_time ON alert_history(status, starts_at);

-- =============================================================================
-- AUTHENTICATION AND AUTHORIZATION TABLES
-- =============================================================================

-- User authentication table for secure login
CREATE TABLE IF NOT EXISTS user_authentication (
    user_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    username VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    password_algorithm VARCHAR(50) NOT NULL DEFAULT 'bcrypt',
    email VARCHAR(255) UNIQUE NOT NULL,
    is_active BOOLEAN NOT NULL DEFAULT true,
    failed_login_attempts INTEGER NOT NULL DEFAULT 0,
    account_locked_until TIMESTAMP WITH TIME ZONE,
    last_login_at TIMESTAMP WITH TIME ZONE,
    last_login_ip VARCHAR(45),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- API keys table for secure API access
CREATE TABLE IF NOT EXISTS api_keys (
    key_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID REFERENCES user_authentication(user_id) ON DELETE CASCADE,
    key_hash VARCHAR(64) NOT NULL UNIQUE,
    key_name VARCHAR(255) NOT NULL,
    key_prefix VARCHAR(20) NOT NULL,
    is_active BOOLEAN NOT NULL DEFAULT true,
    expires_at TIMESTAMP WITH TIME ZONE,
    rate_limit_per_minute INTEGER DEFAULT 1000,
    last_used_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- JWT refresh tokens table
CREATE TABLE IF NOT EXISTS jwt_refresh_tokens (
    token_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID NOT NULL REFERENCES user_authentication(user_id) ON DELETE CASCADE,
    token_hash VARCHAR(64) NOT NULL UNIQUE,
    expires_at TIMESTAMP WITH TIME ZONE NOT NULL,
    is_revoked BOOLEAN NOT NULL DEFAULT false,
    revoked_at TIMESTAMP WITH TIME ZONE,
    revoked_reason VARCHAR(255),
    device_info VARCHAR(500),
    ip_address VARCHAR(45),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Login history for audit and security monitoring
CREATE TABLE IF NOT EXISTS login_history (
    login_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID REFERENCES user_authentication(user_id) ON DELETE SET NULL,
    username VARCHAR(255) NOT NULL,
    login_successful BOOLEAN NOT NULL,
    failure_reason VARCHAR(255),
    ip_address VARCHAR(45),
    user_agent TEXT,
    login_timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- =============================================================================
-- CACHING AND PERFORMANCE TABLES
-- =============================================================================

-- Query cache for database query results
CREATE TABLE IF NOT EXISTS query_cache (
    cache_key VARCHAR(255) PRIMARY KEY,
    query_sql TEXT NOT NULL,
    result_data JSONB NOT NULL,
    result_count INTEGER,
    cached_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    expires_at TIMESTAMP WITH TIME ZONE NOT NULL,
    hit_count INTEGER NOT NULL DEFAULT 0,
    last_accessed_at TIMESTAMP WITH TIME ZONE
);

-- CDC (Change Data Capture) tracking
CREATE TABLE IF NOT EXISTS cdc_tracking (
    tracking_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    source_database VARCHAR(255) NOT NULL,
    source_table VARCHAR(255) NOT NULL,
    last_lsn VARCHAR(100),
    last_sync_timestamp TIMESTAMP WITH TIME ZONE,
    replication_slot_name VARCHAR(255),
    cdc_status VARCHAR(50) NOT NULL DEFAULT 'active',
    error_message TEXT,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    UNIQUE(source_database, source_table)
);

-- Performance baselines for anomaly detection
CREATE TABLE IF NOT EXISTS performance_baselines (
    baseline_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    metric_name VARCHAR(255) NOT NULL,
    entity_id VARCHAR(255),
    entity_type VARCHAR(100),
    baseline_mean DECIMAL(15,4),
    baseline_stddev DECIMAL(15,4),
    baseline_p95 DECIMAL(15,4),
    baseline_p99 DECIMAL(15,4),
    sample_count INTEGER,
    baseline_start_date DATE NOT NULL,
    baseline_end_date DATE NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    UNIQUE(metric_name, entity_id, entity_type, baseline_start_date)
);

-- Activity feed persistence
CREATE TABLE IF NOT EXISTS activity_feed_persistence (
    activity_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    agent_type VARCHAR(100),
    agent_name VARCHAR(255),
    entity_id VARCHAR(255),
    entity_type VARCHAR(100),
    event_type VARCHAR(100) NOT NULL,
    event_category VARCHAR(50),
    event_severity VARCHAR(20),
    event_description TEXT,
    event_metadata JSONB,
    user_id VARCHAR(255),  -- User who performed the action
    occurred_at TIMESTAMP WITH TIME ZONE NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- =============================================================================
-- INDEXES FOR NEW TABLES
-- =============================================================================

-- Authentication indexes
CREATE INDEX IF NOT EXISTS idx_user_auth_username ON user_authentication(username);
CREATE INDEX IF NOT EXISTS idx_user_auth_email ON user_authentication(email);
CREATE INDEX IF NOT EXISTS idx_user_auth_active ON user_authentication(is_active);

-- API keys indexes
CREATE INDEX IF NOT EXISTS idx_api_keys_user_id ON api_keys(user_id);
CREATE INDEX IF NOT EXISTS idx_api_keys_hash ON api_keys(key_hash);
CREATE INDEX IF NOT EXISTS idx_api_keys_active ON api_keys(is_active);
CREATE INDEX IF NOT EXISTS idx_api_keys_expires ON api_keys(expires_at);

-- JWT tokens indexes
CREATE INDEX IF NOT EXISTS idx_jwt_tokens_user_id ON jwt_refresh_tokens(user_id);
CREATE INDEX IF NOT EXISTS idx_jwt_tokens_hash ON jwt_refresh_tokens(token_hash);
CREATE INDEX IF NOT EXISTS idx_jwt_tokens_expires ON jwt_refresh_tokens(expires_at);
CREATE INDEX IF NOT EXISTS idx_jwt_tokens_revoked ON jwt_refresh_tokens(is_revoked);

-- Login history indexes
CREATE INDEX IF NOT EXISTS idx_login_history_user_id ON login_history(user_id);
CREATE INDEX IF NOT EXISTS idx_login_history_timestamp ON login_history(login_timestamp);
CREATE INDEX IF NOT EXISTS idx_login_history_username ON login_history(username);

-- Query cache indexes
CREATE INDEX IF NOT EXISTS idx_query_cache_expires ON query_cache(expires_at);
CREATE INDEX IF NOT EXISTS idx_query_cache_accessed ON query_cache(last_accessed_at);

-- CDC tracking indexes
CREATE INDEX IF NOT EXISTS idx_cdc_source ON cdc_tracking(source_database, source_table);
CREATE INDEX IF NOT EXISTS idx_cdc_status ON cdc_tracking(cdc_status);

-- Performance baselines indexes
CREATE INDEX IF NOT EXISTS idx_perf_baseline_metric ON performance_baselines(metric_name);
CREATE INDEX IF NOT EXISTS idx_perf_baseline_entity ON performance_baselines(entity_id, entity_type);
CREATE INDEX IF NOT EXISTS idx_perf_baseline_dates ON performance_baselines(baseline_start_date, baseline_end_date);

-- Activity feed indexes
CREATE INDEX IF NOT EXISTS idx_activity_feed_agent ON activity_feed_persistence(agent_type, agent_name);
CREATE INDEX IF NOT EXISTS idx_activity_feed_entity ON activity_feed_persistence(entity_id, entity_type);
CREATE INDEX IF NOT EXISTS idx_activity_feed_occurred ON activity_feed_persistence(occurred_at);
CREATE INDEX IF NOT EXISTS idx_activity_feed_event_type ON activity_feed_persistence(event_type);

-- =============================================================================
-- PRODUCTION-GRADE ENHANCEMENTS - ADDED FOR RULE #1 COMPLIANCE
-- Tables supporting production implementations (no stubs/placeholders)
-- =============================================================================

-- Enrichment data cache for lookup operations
CREATE TABLE IF NOT EXISTS geo_enrichment (
    lookup_key VARCHAR(255) PRIMARY KEY,
    country VARCHAR(100),
    city VARCHAR(255),
    latitude DECIMAL(10, 8),
    longitude DECIMAL(11, 8),
    timezone VARCHAR(100),
    region VARCHAR(255),
    cached_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    expires_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT (NOW() + INTERVAL '30 days')
);

CREATE TABLE IF NOT EXISTS customer_enrichment (
    customer_id VARCHAR(255) PRIMARY KEY,
    segment VARCHAR(100),
    lifetime_value DECIMAL(15, 2),
    acquisition_channel VARCHAR(100),
    preferences JSONB,
    churn_risk DECIMAL(3, 2),
    last_interaction_date TIMESTAMP WITH TIME ZONE,
    cached_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS product_enrichment (
    product_id VARCHAR(255) PRIMARY KEY,
    category VARCHAR(100),
    brand VARCHAR(255),
    price DECIMAL(15, 2),
    stock_level INTEGER,
    supplier_id VARCHAR(255),
    warranty_months INTEGER,
    rating DECIMAL(3, 2),
    cached_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Duplicate detection tracking
CREATE TABLE IF NOT EXISTS processed_records (
    record_hash VARCHAR(255) PRIMARY KEY,
    processed_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    pipeline_id VARCHAR(255) NOT NULL,
    source_id VARCHAR(255)
);

CREATE INDEX IF NOT EXISTS idx_processed_records_source ON processed_records(source_id);
CREATE INDEX IF NOT EXISTS idx_processed_records_pipeline ON processed_records(pipeline_id);

-- Health check metrics persistence
CREATE TABLE IF NOT EXISTS health_metrics (
    metric_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    probe_type INTEGER NOT NULL,
    success BOOLEAN NOT NULL,
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    response_time_ms INTEGER,
    metadata JSONB
);

CREATE INDEX IF NOT EXISTS idx_health_metrics_probe ON health_metrics(probe_type);
CREATE INDEX IF NOT EXISTS idx_health_metrics_timestamp ON health_metrics(timestamp);

-- Event bus operations
CREATE TABLE IF NOT EXISTS event_log (
    event_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    event_type VARCHAR(100) NOT NULL,
    event_data JSONB NOT NULL,
    status VARCHAR(50) NOT NULL DEFAULT 'PENDING',
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    processed_at TIMESTAMP WITH TIME ZONE,
    expiry_time TIMESTAMP WITH TIME ZONE,
    retry_count INTEGER DEFAULT 0,
    error_message TEXT
);

CREATE INDEX IF NOT EXISTS idx_event_log_status ON event_log(status);
CREATE INDEX IF NOT EXISTS idx_event_log_created ON event_log(created_at);
CREATE INDEX IF NOT EXISTS idx_event_log_expiry ON event_log(expiry_time);

-- Data ingestion source management
CREATE TABLE IF NOT EXISTS ingestion_sources (
    source_id VARCHAR(255) PRIMARY KEY,
    source_type VARCHAR(100) NOT NULL,
    state VARCHAR(50) NOT NULL DEFAULT 'STOPPED',
    config JSONB,
    paused_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_ingestion_sources_state ON ingestion_sources(state);
CREATE INDEX IF NOT EXISTS idx_ingestion_sources_type ON ingestion_sources(source_type);

-- Schema migration tracking
CREATE TABLE IF NOT EXISTS schema_migrations (
    version VARCHAR(50) PRIMARY KEY,
    description TEXT,
    executed_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    checksum VARCHAR(255)
);

-- Ingestion history for gap detection
CREATE TABLE IF NOT EXISTS ingestion_history (
    history_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    source_id VARCHAR(255) NOT NULL,
    ingestion_date TIMESTAMP WITH TIME ZONE NOT NULL,
    records_ingested INTEGER,
    status VARCHAR(50) NOT NULL,
    error_message TEXT
);

CREATE INDEX IF NOT EXISTS idx_ingestion_history_source ON ingestion_history(source_id);
CREATE INDEX IF NOT EXISTS idx_ingestion_history_date ON ingestion_history(ingestion_date);

-- Function definitions for LLM compliance functions
CREATE TABLE IF NOT EXISTS function_definitions (
    function_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    function_name VARCHAR(255) UNIQUE NOT NULL,
    description TEXT,
    parameters JSONB,
    category VARCHAR(100),
    version VARCHAR(50),
    active BOOLEAN NOT NULL DEFAULT true,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_function_definitions_category ON function_definitions(category);
CREATE INDEX IF NOT EXISTS idx_function_definitions_active ON function_definitions(active);

-- Learning pattern success tracking
CREATE TABLE IF NOT EXISTS learning_patterns (
    pattern_id VARCHAR(255) PRIMARY KEY,
    pattern_name VARCHAR(255),
    pattern_data JSONB,
    success_count INTEGER NOT NULL DEFAULT 0,
    failure_count INTEGER NOT NULL DEFAULT 0,
    total_applications INTEGER NOT NULL DEFAULT 0,
    average_confidence DECIMAL(5, 4),
    last_applied TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_learning_patterns_success ON learning_patterns(success_count);
CREATE INDEX IF NOT EXISTS idx_learning_patterns_last_applied ON learning_patterns(last_applied);

-- Compliance cases with vector support (requires pgvector extension)
CREATE EXTENSION IF NOT EXISTS vector;

CREATE TABLE IF NOT EXISTS compliance_cases (
    case_id VARCHAR(255) PRIMARY KEY,
    transaction_data JSONB NOT NULL,
    regulatory_context JSONB NOT NULL,
    decision JSONB NOT NULL,
    outcome VARCHAR(50) NOT NULL,
    similarity_features vector(128),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    access_count INTEGER DEFAULT 0,
    success_rate DOUBLE PRECISION DEFAULT 0.0
);

CREATE INDEX IF NOT EXISTS idx_compliance_cases_outcome ON compliance_cases(outcome);
CREATE INDEX IF NOT EXISTS idx_compliance_cases_created ON compliance_cases(created_at);

-- Vector similarity index for compliance cases
CREATE INDEX IF NOT EXISTS idx_case_similarity 
ON compliance_cases USING ivfflat (similarity_features vector_cosine_ops);

-- Human-AI collaboration responses
CREATE TABLE IF NOT EXISTS human_responses (
    response_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    request_id VARCHAR(255) NOT NULL,
    user_id VARCHAR(255) NOT NULL,
    agent_id VARCHAR(255) NOT NULL,
    response_data JSONB NOT NULL,
    processed_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_human_responses_request ON human_responses(request_id);
CREATE INDEX IF NOT EXISTS idx_human_responses_user ON human_responses(user_id);
CREATE INDEX IF NOT EXISTS idx_human_responses_agent ON human_responses(agent_id);

-- Decision trees for visualization
CREATE TABLE IF NOT EXISTS decision_trees (
    tree_id VARCHAR(255) PRIMARY KEY,
    agent_id VARCHAR(255) NOT NULL,
    decision_type VARCHAR(100) NOT NULL,
    confidence_level VARCHAR(50),
    reasoning_data JSONB,
    actions_data JSONB,
    metadata JSONB,
    node_count INTEGER,
    edge_count INTEGER,
    success_rate DOUBLE PRECISION DEFAULT 0.0,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_decision_trees_agent ON decision_trees(agent_id);
CREATE INDEX IF NOT EXISTS idx_decision_trees_type ON decision_trees(decision_type);
CREATE INDEX IF NOT EXISTS idx_decision_trees_created ON decision_trees(created_at);

-- Fuzzy matching cache for near-duplicate detection
CREATE TABLE IF NOT EXISTS fuzzy_match_cache (
    record_hash VARCHAR(255) PRIMARY KEY,
    minhash_signature JSONB NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_fuzzy_match_created ON fuzzy_match_cache(created_at);

-- =============================================================================
-- COMMENTS FOR PRODUCTION TABLES
-- =============================================================================

COMMENT ON TABLE geo_enrichment IS 'Geographic enrichment data cache for lookup operations - supports production geographic analysis';
COMMENT ON TABLE customer_enrichment IS 'Customer profile enrichment cache - integrates with CRM systems';
COMMENT ON TABLE product_enrichment IS 'Product catalog enrichment cache - supports inventory and pricing lookups';
COMMENT ON TABLE processed_records IS 'Duplicate detection tracking - prevents reprocessing of ingested records';
COMMENT ON TABLE health_metrics IS 'Health check metrics persistence - supports Prometheus and database-backed monitoring';
COMMENT ON TABLE event_log IS 'Event bus operation log - tracks all events with retry and expiry support';
COMMENT ON TABLE ingestion_sources IS 'Data ingestion source state management - supports pause/resume capability';
COMMENT ON TABLE schema_migrations IS 'Database schema migration tracking - version control for schema changes';
COMMENT ON TABLE ingestion_history IS 'Historical ingestion tracking - enables gap detection and backfill operations';
COMMENT ON TABLE function_definitions IS 'LLM compliance function definitions - database-backed function catalog';
COMMENT ON TABLE learning_patterns IS 'Learning pattern success tracking - production pattern learning with metrics';
COMMENT ON TABLE compliance_cases IS 'Case-based reasoning storage with vector similarity - supports semantic case matching';
COMMENT ON TABLE human_responses IS 'Human-AI collaboration response tracking - audit trail for human decisions';
COMMENT ON TABLE decision_trees IS 'Decision tree visualization data - supports real-time decision analysis';
COMMENT ON TABLE fuzzy_match_cache IS 'MinHash signatures for near-duplicate detection - enables efficient similarity matching at scale';
-- Create sessions table for database-backed session management
-- This provides secure, server-side session handling without client-side storage

CREATE TABLE IF NOT EXISTS sessions (
    session_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES user_authentication(user_id) ON DELETE CASCADE,
    session_token VARCHAR(255) UNIQUE NOT NULL,
    user_agent TEXT,
    ip_address INET,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    last_active TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    expires_at TIMESTAMP WITH TIME ZONE NOT NULL,
    is_active BOOLEAN DEFAULT true
);

-- Create indexes for fast session lookups
CREATE INDEX IF NOT EXISTS idx_sessions_token ON sessions(session_token) WHERE is_active = true;
CREATE INDEX IF NOT EXISTS idx_sessions_user ON sessions(user_id) WHERE is_active = true;
CREATE INDEX IF NOT EXISTS idx_sessions_expiry ON sessions(expires_at) WHERE is_active = true;
CREATE INDEX IF NOT EXISTS idx_sessions_last_active ON sessions(last_active);

-- Comment the table
COMMENT ON TABLE sessions IS 'Server-side session management with database storage';
COMMENT ON COLUMN sessions.session_token IS 'Secure random token sent as HttpOnly cookie';
COMMENT ON COLUMN sessions.expires_at IS 'Session expiration time (default 24 hours)';

-- =============================================================================
-- REGULATORY MONITORING TABLES (Phase 2)
-- Production-grade tables for regulatory source management and monitoring
-- =============================================================================

CREATE TABLE IF NOT EXISTS regulatory_sources (
    source_id VARCHAR(50) PRIMARY KEY,
    source_name VARCHAR(255) NOT NULL,
    source_type VARCHAR(50) NOT NULL CHECK (source_type IN ('government', 'self-regulatory', 'international')),
    base_url TEXT,
    api_endpoint TEXT,
    authentication_type VARCHAR(50),
    api_key_encrypted TEXT,
    is_active BOOLEAN DEFAULT true,
    monitoring_frequency_hours INTEGER DEFAULT 24,
    last_check_at TIMESTAMP WITH TIME ZONE,
    last_change_detected TIMESTAMP WITH TIME ZONE,
    total_changes_detected INTEGER DEFAULT 0,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Insert default regulatory sources
INSERT INTO regulatory_sources (source_id, source_name, source_type, base_url, is_active) VALUES
('SEC', 'Securities and Exchange Commission', 'government', 'https://www.sec.gov', true),
('FINRA', 'Financial Industry Regulatory Authority', 'self-regulatory', 'https://www.finra.org', true),
('CFTC', 'Commodity Futures Trading Commission', 'government', 'https://www.cftc.gov', true),
('FDIC', 'Federal Deposit Insurance Corporation', 'government', 'https://www.fdic.gov', true),
('FCA', 'Financial Conduct Authority', 'government', 'https://www.fca.org.uk', true),
('ECB', 'European Central Bank', 'international', 'https://www.ecb.europa.eu', true),
('BIS', 'Bank for International Settlements', 'international', 'https://www.bis.org', true)
ON CONFLICT (source_id) DO NOTHING;

CREATE INDEX IF NOT EXISTS idx_regulatory_sources_active ON regulatory_sources(is_active, last_check_at);
CREATE INDEX IF NOT EXISTS idx_regulatory_sources_type ON regulatory_sources(source_type);

COMMENT ON TABLE regulatory_sources IS 'Registry of regulatory sources monitored by the system';

-- =============================================================================
-- SYSTEM METRICS TABLES (Phase 2 - HIGH PRIORITY)
-- Real-time system performance monitoring
-- =============================================================================

CREATE TABLE IF NOT EXISTS system_metrics_realtime (
    metric_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    metric_name VARCHAR(100) NOT NULL,
    metric_value DECIMAL(15,6) NOT NULL,
    metric_unit VARCHAR(20),
    metric_tags JSONB DEFAULT '{}'::jsonb,
    collected_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_system_metrics_name_time ON system_metrics_realtime(metric_name, collected_at DESC);
CREATE INDEX IF NOT EXISTS idx_system_metrics_collected ON system_metrics_realtime(collected_at DESC);

COMMENT ON TABLE system_metrics_realtime IS 'Real-time system performance metrics (CPU, memory, disk, network)';

-- =============================================================================
-- PATTERN ANALYSIS TABLES (Phase 3.2)
-- Production-grade pattern recognition and analysis
-- =============================================================================

CREATE TABLE IF NOT EXISTS pattern_definitions (
    pattern_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    pattern_name VARCHAR(255) NOT NULL UNIQUE,
    pattern_type VARCHAR(50) NOT NULL,
    pattern_description TEXT,
    pattern_rules JSONB NOT NULL,
    confidence_threshold DECIMAL(3,2) DEFAULT 0.75,
    severity VARCHAR(20) CHECK (severity IN ('LOW', 'MEDIUM', 'HIGH', 'CRITICAL')),
    is_active BOOLEAN DEFAULT true,
    created_by VARCHAR(255),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS pattern_analysis_results (
    result_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    pattern_id UUID REFERENCES pattern_definitions(pattern_id) ON DELETE CASCADE,
    entity_type VARCHAR(50) NOT NULL,
    entity_id VARCHAR(255) NOT NULL,
    match_confidence DECIMAL(5,4) NOT NULL,
    matched_data JSONB,
    additional_context JSONB,
    detected_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    reviewed_at TIMESTAMP WITH TIME ZONE,
    reviewed_by VARCHAR(255),
    status VARCHAR(20) DEFAULT 'PENDING' CHECK (status IN ('PENDING', 'CONFIRMED', 'FALSE_POSITIVE', 'INVESTIGATING'))
);

CREATE INDEX IF NOT EXISTS idx_pattern_results_pattern ON pattern_analysis_results(pattern_id, detected_at DESC);
CREATE INDEX IF NOT EXISTS idx_pattern_results_entity ON pattern_analysis_results(entity_type, entity_id);
CREATE INDEX IF NOT EXISTS idx_pattern_results_status ON pattern_analysis_results(status, detected_at DESC);

COMMENT ON TABLE pattern_definitions IS 'Defines patterns for automated detection and analysis';
COMMENT ON TABLE pattern_analysis_results IS 'Results of pattern matching against real data';

-- =============================================================================
-- AGENT COMMUNICATION TABLES (Phase 3.8)
-- Inter-agent communication tracking
-- =============================================================================

CREATE TABLE IF NOT EXISTS agent_communications (
    comm_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    from_agent VARCHAR(100) NOT NULL,
    to_agent VARCHAR(100) NOT NULL,
    message_type VARCHAR(50) NOT NULL,
    message_content TEXT NOT NULL,
    message_priority VARCHAR(20) DEFAULT 'NORMAL' CHECK (message_priority IN ('LOW', 'NORMAL', 'HIGH', 'URGENT')),
    metadata JSONB DEFAULT '{}'::jsonb,
    sent_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    received_at TIMESTAMP WITH TIME ZONE,
    processed_at TIMESTAMP WITH TIME ZONE,
    status VARCHAR(20) DEFAULT 'SENT' CHECK (status IN ('SENT', 'RECEIVED', 'PROCESSED', 'FAILED'))
);

CREATE INDEX IF NOT EXISTS idx_agent_comm_from ON agent_communications(from_agent, sent_at DESC);
CREATE INDEX IF NOT EXISTS idx_agent_comm_to ON agent_communications(to_agent, received_at DESC);
CREATE INDEX IF NOT EXISTS idx_agent_comm_type ON agent_communications(message_type, sent_at DESC);

COMMENT ON TABLE agent_communications IS 'Inter-agent communication logs for collaboration and coordination';

-- =============================================================================
-- MCDA (Multi-Criteria Decision Analysis) TABLES (Phase 3.11)
-- Advanced decision-making framework
-- =============================================================================

CREATE TABLE IF NOT EXISTS mcda_models (
    model_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    model_name VARCHAR(255) NOT NULL UNIQUE,
    model_description TEXT,
    decision_method VARCHAR(50) NOT NULL CHECK (decision_method IN ('TOPSIS', 'ELECTRE', 'AHP', 'PROMETHEE', 'WEIGHTED_SUM')),
    model_configuration JSONB NOT NULL,
    is_active BOOLEAN DEFAULT true,
    created_by VARCHAR(255),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS mcda_criteria (
    criterion_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    model_id UUID REFERENCES mcda_models(model_id) ON DELETE CASCADE,
    criterion_name VARCHAR(255) NOT NULL,
    criterion_type VARCHAR(50) NOT NULL CHECK (criterion_type IN ('BENEFIT', 'COST')),
    weight DECIMAL(5,4) NOT NULL CHECK (weight >= 0 AND weight <= 1),
    normalization_method VARCHAR(50),
    unit_of_measure VARCHAR(50),
    min_value DECIMAL(15,6),
    max_value DECIMAL(15,6),
    sort_order INTEGER,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS mcda_evaluations (
    evaluation_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    model_id UUID REFERENCES mcda_models(model_id),
    alternative_name VARCHAR(255) NOT NULL,
    criterion_id UUID REFERENCES mcda_criteria(criterion_id),
    criterion_value DECIMAL(15,6) NOT NULL,
    normalized_value DECIMAL(15,6),
    weighted_score DECIMAL(15,6),
    evaluation_metadata JSONB,
    evaluated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_mcda_criteria_model ON mcda_criteria(model_id, sort_order);
CREATE INDEX IF NOT EXISTS idx_mcda_eval_model ON mcda_evaluations(model_id, evaluation_id);

COMMENT ON TABLE mcda_models IS 'Multi-criteria decision analysis model definitions';
COMMENT ON TABLE mcda_criteria IS 'Criteria used in MCDA models with weights and constraints';
COMMENT ON TABLE mcda_evaluations IS 'Evaluation results for alternatives against criteria';

-- =============================================================================
-- FUNCTION CALLING DEBUG TABLES (Phase 3.12)
-- LLM function calling debugging and monitoring
-- =============================================================================

CREATE TABLE IF NOT EXISTS function_call_logs (
    log_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    agent_name VARCHAR(100),
    function_name VARCHAR(255) NOT NULL,
    function_parameters JSONB,
    function_result JSONB,
    execution_time_ms INTEGER,
    success BOOLEAN NOT NULL,
    error_message TEXT,
    llm_provider VARCHAR(50),
    model_name VARCHAR(100),
    tokens_used INTEGER,
    call_context TEXT,
    called_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_function_calls_agent ON function_call_logs(agent_name, called_at DESC);
CREATE INDEX IF NOT EXISTS idx_function_calls_name ON function_call_logs(function_name, called_at DESC);
CREATE INDEX IF NOT EXISTS idx_function_calls_success ON function_call_logs(success, called_at DESC);

COMMENT ON TABLE function_call_logs IS 'Detailed logs of LLM function calls for debugging and optimization';

-- =============================================================================
-- AGENT SYSTEM TABLES - Event Subscription & Output Routing
-- Production-grade tables for agent lifecycle management
-- =============================================================================

-- Regulatory event subscriptions
CREATE TABLE IF NOT EXISTS regulatory_subscriptions (
    subscription_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    agent_id UUID NOT NULL REFERENCES agent_configurations(config_id) ON DELETE CASCADE,
    filter_criteria JSONB NOT NULL DEFAULT '{}'::jsonb,
    is_active BOOLEAN NOT NULL DEFAULT true,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    UNIQUE(agent_id)
);

CREATE INDEX IF NOT EXISTS idx_regulatory_subscriptions_agent ON regulatory_subscriptions(agent_id);
CREATE INDEX IF NOT EXISTS idx_regulatory_subscriptions_active ON regulatory_subscriptions(is_active);

COMMENT ON TABLE regulatory_subscriptions IS 'Agent subscriptions to regulatory change events';
COMMENT ON COLUMN regulatory_subscriptions.filter_criteria IS 'JSON filter: sources, change_types, severities, jurisdictions';

-- Agent outputs (generic storage for all agent outputs)
CREATE TABLE IF NOT EXISTS agent_outputs (
    output_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    agent_id UUID NOT NULL REFERENCES agent_configurations(config_id) ON DELETE CASCADE,
    agent_name VARCHAR(255) NOT NULL,
    agent_type VARCHAR(100) NOT NULL,
    output_type VARCHAR(50) NOT NULL CHECK (output_type IN (
        'DECISION', 'RISK_ASSESSMENT', 'COMPLIANCE_CHECK', 
        'PATTERN_DETECTION', 'ALERT', 'RECOMMENDATION', 'ANALYSIS_RESULT'
    )),
    output_data JSONB NOT NULL,
    confidence_score DECIMAL(3,2) CHECK (confidence_score >= 0 AND confidence_score <= 1),
    severity VARCHAR(20) CHECK (severity IN ('LOW', 'MEDIUM', 'HIGH', 'CRITICAL')),
    requires_human_review BOOLEAN DEFAULT false,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_agent_outputs_agent ON agent_outputs(agent_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_agent_outputs_type ON agent_outputs(output_type, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_agent_outputs_severity ON agent_outputs(severity, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_agent_outputs_human_review ON agent_outputs(requires_human_review) WHERE requires_human_review = true;

COMMENT ON TABLE agent_outputs IS 'Generic storage for all agent outputs with routing metadata';

-- Agent runtime status (for lifecycle management)
CREATE TABLE IF NOT EXISTS agent_runtime_status (
    agent_id UUID PRIMARY KEY REFERENCES agent_configurations(config_id) ON DELETE CASCADE,
    status VARCHAR(20) NOT NULL CHECK (status IN ('STOPPED', 'STARTING', 'RUNNING', 'STOPPING', 'ERROR', 'RESTARTING')),
    started_at TIMESTAMP WITH TIME ZONE,
    last_health_check TIMESTAMP WITH TIME ZONE,
    tasks_processed BIGINT DEFAULT 0,
    tasks_failed BIGINT DEFAULT 0,
    health_score DECIMAL(3,2) DEFAULT 1.00 CHECK (health_score >= 0 AND health_score <= 1),
    last_error TEXT,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_agent_runtime_status ON agent_runtime_status(status);
CREATE INDEX IF NOT EXISTS idx_agent_runtime_health ON agent_runtime_status(health_score);

COMMENT ON TABLE agent_runtime_status IS 'Real-time status tracking for running agents';

-- Tool usage logs (for tool system tracking)
CREATE TABLE IF NOT EXISTS tool_usage_logs (
    log_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    agent_id UUID REFERENCES agent_configurations(config_id) ON DELETE CASCADE,
    tool_name VARCHAR(100) NOT NULL,
    parameters JSONB,
    result JSONB,
    success BOOLEAN NOT NULL,
    execution_time_ms INTEGER,
    tokens_used INTEGER DEFAULT 0,
    error_message TEXT,
    executed_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_tool_usage_agent ON tool_usage_logs(agent_id, executed_at DESC);
CREATE INDEX IF NOT EXISTS idx_tool_usage_tool_name ON tool_usage_logs(tool_name, executed_at DESC);
CREATE INDEX IF NOT EXISTS idx_tool_usage_success ON tool_usage_logs(success, executed_at DESC);

COMMENT ON TABLE tool_usage_logs IS 'Audit log of all tool calls made by agents (HTTP, DB, LLM)';

-- =============================================================================
-- MIGRATIONS - Transaction Status Column
-- =============================================================================
-- Migration to add status column to existing transactions table
-- This handles existing tables that don't have the status column yet
DO $$ 
BEGIN
    -- Check if status column exists
    IF NOT EXISTS (
        SELECT 1 FROM information_schema.columns 
        WHERE table_name = 'transactions' AND column_name = 'status'
    ) THEN
        -- Add status column with default 'completed'
        ALTER TABLE transactions ADD COLUMN status VARCHAR(20) NOT NULL DEFAULT 'completed' 
            CHECK (status IN ('pending', 'processing', 'completed', 'rejected', 'flagged', 'approved'));
        
        -- Update status based on flagged column for existing records
        UPDATE transactions SET status = 'flagged' WHERE flagged = TRUE;
        UPDATE transactions SET status = 'completed' WHERE flagged = FALSE OR flagged IS NULL;
        
        RAISE NOTICE 'Added status column to transactions table and migrated existing data';
    ELSE
        RAISE NOTICE 'Status column already exists in transactions table';
    END IF;
END $$;

-- =============================================================================
-- FEATURE 1: REAL-TIME COLLABORATION DASHBOARD
-- Production-grade collaboration and agent streaming infrastructure
-- =============================================================================

-- Collaboration sessions for multi-agent coordination
CREATE TABLE IF NOT EXISTS collaboration_sessions (
    session_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    title TEXT NOT NULL,
    description TEXT,
    objective TEXT,
    status VARCHAR(20) NOT NULL DEFAULT 'active' CHECK (status IN ('active', 'paused', 'completed', 'archived', 'cancelled')),
    created_by TEXT NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    started_at TIMESTAMP WITH TIME ZONE,
    completed_at TIMESTAMP WITH TIME ZONE,
    agents JSONB NOT NULL DEFAULT '[]'::jsonb,
    context JSONB DEFAULT '{}'::jsonb,
    settings JSONB DEFAULT '{}'::jsonb,
    metadata JSONB DEFAULT '{}'::jsonb,
    INDEX idx_collaboration_sessions_status (status),
    INDEX idx_collaboration_sessions_created_by (created_by),
    INDEX idx_collaboration_sessions_created_at (created_at DESC)
);

-- Real-time reasoning stream for agent decision-making visualization
CREATE TABLE IF NOT EXISTS collaboration_reasoning_stream (
    stream_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    session_id UUID NOT NULL REFERENCES collaboration_sessions(session_id) ON DELETE CASCADE,
    agent_id TEXT NOT NULL,
    agent_name TEXT,
    agent_type TEXT,
    reasoning_step TEXT NOT NULL,
    step_number INTEGER NOT NULL,
    step_type VARCHAR(50) DEFAULT 'thinking' CHECK (step_type IN ('thinking', 'analyzing', 'deciding', 'executing', 'completed', 'error')),
    confidence_score DECIMAL(5,4) CHECK (confidence_score >= 0 AND confidence_score <= 1),
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    duration_ms INTEGER,
    metadata JSONB DEFAULT '{}'::jsonb,
    parent_step_id UUID REFERENCES collaboration_reasoning_stream(stream_id),
    INDEX idx_reasoning_stream_session (session_id, timestamp DESC),
    INDEX idx_reasoning_stream_agent (agent_id, timestamp DESC),
    INDEX idx_reasoning_stream_type (step_type)
);

-- Human overrides for agent decisions
CREATE TABLE IF NOT EXISTS human_overrides (
    override_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    decision_id UUID,
    session_id UUID REFERENCES collaboration_sessions(session_id) ON DELETE SET NULL,
    user_id TEXT NOT NULL,
    user_name TEXT,
    original_decision TEXT NOT NULL,
    override_decision TEXT NOT NULL,
    reason TEXT NOT NULL,
    justification TEXT,
    impact_assessment JSONB,
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    metadata JSONB DEFAULT '{}'::jsonb,
    INDEX idx_human_overrides_decision (decision_id),
    INDEX idx_human_overrides_session (session_id),
    INDEX idx_human_overrides_user (user_id),
    INDEX idx_human_overrides_timestamp (timestamp DESC)
);

-- Collaboration agents participating in sessions
CREATE TABLE IF NOT EXISTS collaboration_agents (
    participant_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    session_id UUID NOT NULL REFERENCES collaboration_sessions(session_id) ON DELETE CASCADE,
    agent_id TEXT NOT NULL,
    agent_name TEXT NOT NULL,
    agent_type TEXT NOT NULL,
    role VARCHAR(50) DEFAULT 'participant' CHECK (role IN ('participant', 'observer', 'facilitator', 'leader')),
    status VARCHAR(20) DEFAULT 'active' CHECK (status IN ('active', 'inactive', 'disconnected', 'completed')),
    joined_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    left_at TIMESTAMP WITH TIME ZONE,
    contribution_count INTEGER DEFAULT 0,
    last_activity_at TIMESTAMP WITH TIME ZONE,
    performance_metrics JSONB DEFAULT '{}'::jsonb,
    UNIQUE(session_id, agent_id),
    INDEX idx_collaboration_agents_session (session_id),
    INDEX idx_collaboration_agents_agent (agent_id),
    INDEX idx_collaboration_agents_status (status)
);

-- Confidence metrics breakdown for decision transparency
CREATE TABLE IF NOT EXISTS collaboration_confidence_metrics (
    metric_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    session_id UUID NOT NULL REFERENCES collaboration_sessions(session_id) ON DELETE CASCADE,
    decision_id UUID,
    stream_id UUID REFERENCES collaboration_reasoning_stream(stream_id) ON DELETE CASCADE,
    metric_type VARCHAR(50) NOT NULL CHECK (metric_type IN ('data_quality', 'model_confidence', 'rule_match', 'historical_accuracy', 'consensus')),
    metric_name TEXT NOT NULL,
    metric_value DECIMAL(5,4) NOT NULL CHECK (metric_value >= 0 AND metric_value <= 1),
    weight DECIMAL(5,4) DEFAULT 1.0 CHECK (weight >= 0 AND weight <= 1),
    contributing_factors JSONB DEFAULT '[]'::jsonb,
    calculated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    INDEX idx_confidence_metrics_session (session_id),
    INDEX idx_confidence_metrics_decision (decision_id),
    INDEX idx_confidence_metrics_type (metric_type)
);

-- Session activity log for audit and replay
CREATE TABLE IF NOT EXISTS collaboration_activity_log (
    activity_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    session_id UUID NOT NULL REFERENCES collaboration_sessions(session_id) ON DELETE CASCADE,
    activity_type VARCHAR(50) NOT NULL CHECK (activity_type IN ('session_started', 'session_completed', 'agent_joined', 'agent_left', 'decision_made', 'override_applied', 'message_sent', 'consensus_reached')),
    actor_id TEXT NOT NULL,
    actor_type VARCHAR(20) CHECK (actor_type IN ('agent', 'human', 'system')),
    description TEXT NOT NULL,
    details JSONB DEFAULT '{}'::jsonb,
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    INDEX idx_activity_log_session (session_id, timestamp DESC),
    INDEX idx_activity_log_type (activity_type),
    INDEX idx_activity_log_actor (actor_id)
);

-- Create indexes for performance
CREATE INDEX IF NOT EXISTS idx_reasoning_stream_confidence ON collaboration_reasoning_stream(confidence_score DESC) WHERE confidence_score IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_human_overrides_recent ON human_overrides(timestamp DESC) WHERE timestamp > NOW() - INTERVAL '30 days';
CREATE INDEX IF NOT EXISTS idx_collaboration_sessions_active ON collaboration_sessions(status, updated_at DESC) WHERE status IN ('active', 'paused');

-- Create materialized view for session summaries (performance optimization)
CREATE MATERIALIZED VIEW IF NOT EXISTS collaboration_session_summary AS
SELECT 
    cs.session_id,
    cs.title,
    cs.status,
    cs.created_at,
    cs.updated_at,
    COUNT(DISTINCT ca.agent_id) as agent_count,
    COUNT(DISTINCT crs.stream_id) as reasoning_steps_count,
    COUNT(DISTINCT ho.override_id) as overrides_count,
    AVG(crs.confidence_score) as avg_confidence,
    MAX(crs.timestamp) as last_activity
FROM collaboration_sessions cs
LEFT JOIN collaboration_agents ca ON cs.session_id = ca.session_id
LEFT JOIN collaboration_reasoning_stream crs ON cs.session_id = crs.session_id
LEFT JOIN human_overrides ho ON cs.session_id = ho.session_id
GROUP BY cs.session_id, cs.title, cs.status, cs.created_at, cs.updated_at;

-- Create unique index on materialized view for fast refresh
CREATE UNIQUE INDEX IF NOT EXISTS idx_session_summary_id ON collaboration_session_summary(session_id);

-- Refresh function for materialized view
CREATE OR REPLACE FUNCTION refresh_collaboration_session_summary()
RETURNS void AS $$
BEGIN
    REFRESH MATERIALIZED VIEW CONCURRENTLY collaboration_session_summary;
END;
$$ LANGUAGE plpgsql;

-- Comments for documentation
COMMENT ON TABLE collaboration_sessions IS 'Central table for multi-agent collaboration sessions with real-time coordination';
COMMENT ON TABLE collaboration_reasoning_stream IS 'Real-time stream of agent reasoning steps for transparency and human oversight';
COMMENT ON TABLE human_overrides IS 'Audit log of human interventions in agent decisions';
COMMENT ON TABLE collaboration_agents IS 'Agents participating in collaboration sessions';
COMMENT ON TABLE collaboration_confidence_metrics IS 'Detailed confidence score breakdowns for decision transparency';
COMMENT ON TABLE collaboration_activity_log IS 'Comprehensive activity log for session audit and replay capability';

-- =============================================================================
-- FEATURE 2: REGULATORY CHANGE ALERTS WITH EMAIL
-- Production-grade alerting and notification infrastructure
-- =============================================================================

-- Alert rules for regulatory change monitoring
CREATE TABLE IF NOT EXISTS alert_rules (
    rule_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name TEXT NOT NULL,
    description TEXT,
    enabled BOOLEAN DEFAULT true,
    severity_filter TEXT[] DEFAULT ARRAY['critical', 'high', 'medium', 'low'],
    source_filter TEXT[], -- 'SEC', 'FCA', 'ECB', etc. NULL = all sources
    keyword_filters JSONB DEFAULT '[]'::jsonb,
    notification_channels TEXT[] DEFAULT ARRAY['email'], -- 'email', 'slack', 'sms'
    recipients TEXT[] NOT NULL,
    throttle_minutes INTEGER DEFAULT 60,
    created_by TEXT,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    last_triggered_at TIMESTAMP WITH TIME ZONE,
    trigger_count INTEGER DEFAULT 0,
    metadata JSONB DEFAULT '{}'::jsonb,
    INDEX idx_alert_rules_enabled (enabled) WHERE enabled = true,
    INDEX idx_alert_rules_created_by (created_by)
);

-- Alert delivery log for tracking sent notifications
CREATE TABLE IF NOT EXISTS alert_delivery_log (
    delivery_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    rule_id UUID REFERENCES alert_rules(rule_id) ON DELETE CASCADE,
    regulatory_change_id UUID,
    recipient TEXT NOT NULL,
    channel TEXT NOT NULL CHECK (channel IN ('email', 'slack', 'sms', 'webhook')),
    status TEXT DEFAULT 'pending' CHECK (status IN ('pending', 'sent', 'failed', 'throttled', 'bounced')),
    sent_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    delivered_at TIMESTAMP WITH TIME ZONE,
    error_message TEXT,
    retry_count INTEGER DEFAULT 0,
    message_id TEXT, -- External message ID from email/SMS provider
    metadata JSONB DEFAULT '{}'::jsonb,
    INDEX idx_delivery_log_rule (rule_id, sent_at DESC),
    INDEX idx_delivery_log_status (status, sent_at DESC),
    INDEX idx_delivery_log_recipient (recipient, sent_at DESC)
);

-- Alert templates for customizable messaging
CREATE TABLE IF NOT EXISTS alert_templates (
    template_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name TEXT NOT NULL UNIQUE,
    template_type TEXT NOT NULL CHECK (template_type IN ('email', 'slack', 'sms')),
    subject_template TEXT,
    body_template TEXT NOT NULL,
    variables JSONB DEFAULT '[]'::jsonb, -- Available template variables
    is_default BOOLEAN DEFAULT false,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    INDEX idx_alert_templates_type (template_type)
);

-- Alert statistics for monitoring and reporting
CREATE TABLE IF NOT EXISTS alert_statistics (
    stat_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    rule_id UUID REFERENCES alert_rules(rule_id) ON DELETE CASCADE,
    date DATE NOT NULL DEFAULT CURRENT_DATE,
    total_triggered INTEGER DEFAULT 0,
    total_sent INTEGER DEFAULT 0,
    total_failed INTEGER DEFAULT 0,
    total_throttled INTEGER DEFAULT 0,
    avg_delivery_time_ms INTEGER,
    metadata JSONB DEFAULT '{}'::jsonb,
    UNIQUE(rule_id, date),
    INDEX idx_alert_stats_date (date DESC),
    INDEX idx_alert_stats_rule (rule_id, date DESC)
);

-- Create indexes for performance
CREATE INDEX IF NOT EXISTS idx_alert_rules_severity ON alert_rules USING GIN (severity_filter);
CREATE INDEX IF NOT EXISTS idx_alert_rules_sources ON alert_rules USING GIN (source_filter) WHERE source_filter IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_alert_delivery_recent ON alert_delivery_log(sent_at DESC) WHERE sent_at > NOW() - INTERVAL '30 days';

-- Insert default email template
INSERT INTO alert_templates (name, template_type, subject_template, body_template, variables, is_default)
VALUES (
    'Default Regulatory Alert Email',
    'email',
    '🚨 REGULENS ALERT: {{severity}} - {{title}}',
    E'<!DOCTYPE html>
<html>
<head>
    <style>
        body { font-family: Arial, sans-serif; line-height: 1.6; color: #333; }
        .header { background-color: #dc2626; color: white; padding: 20px; text-align: center; }
        .content { padding: 20px; }
        .alert-box { background-color: #fef2f2; border-left: 4px solid #dc2626; padding: 15px; margin: 20px 0; }
        .footer { background-color: #f3f4f6; padding: 15px; text-align: center; font-size: 12px; color: #6b7280; }
    </style>
</head>
<body>
    <div class="header">
        <h1>Regulatory Compliance Alert</h1>
    </div>
    <div class="content">
        <h2>{{title}}</h2>
        <div class="alert-box">
            <p><strong>Severity:</strong> {{severity}}</p>
            <p><strong>Source:</strong> {{source}}</p>
            <p><strong>Detected:</strong> {{timestamp}}</p>
        </div>
        <h3>Description:</h3>
        <p>{{description}}</p>
        <h3>Recommended Actions:</h3>
        <p>{{recommendations}}</p>
    </div>
    <div class="footer">
        <p>This alert was generated by Regulens Agentic AI System</p>
        <p>Powered by production-grade regulatory monitoring</p>
    </div>
</body>
</html>',
    '["title", "severity", "source", "timestamp", "description", "recommendations"]'::jsonb,
    true
) ON CONFLICT (name) DO NOTHING;

-- Comments for documentation
COMMENT ON TABLE alert_rules IS 'Configurable alert rules for regulatory change monitoring';
COMMENT ON TABLE alert_delivery_log IS 'Audit log of all sent notifications with delivery status';
COMMENT ON TABLE alert_templates IS 'Customizable message templates for different notification channels';
COMMENT ON TABLE alert_statistics IS 'Daily statistics for alert monitoring and reporting';

-- =============================================================================
-- FEATURE 3: EXPORT/REPORTING MODULE
-- Production-grade PDF and Excel export infrastructure
-- =============================================================================

-- Export requests tracking
CREATE TABLE IF NOT EXISTS export_requests (
    export_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    export_type TEXT NOT NULL CHECK (export_type IN ('pdf_audit_trail', 'excel_transactions', 'pdf_compliance_report', 'excel_regulatory_changes', 'pdf_decision_analysis', 'csv_data_export')),
    requested_by TEXT NOT NULL,
    status TEXT DEFAULT 'pending' CHECK (status IN ('pending', 'processing', 'completed', 'failed', 'cancelled')),
    parameters JSONB DEFAULT '{}'::jsonb,
    file_path TEXT,
    file_size_bytes BIGINT,
    download_url TEXT,
    download_count INTEGER DEFAULT 0,
    expires_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    completed_at TIMESTAMP WITH TIME ZONE,
    error_message TEXT,
    processing_time_ms INTEGER,
    metadata JSONB DEFAULT '{}'::jsonb,
    INDEX idx_export_requests_user (requested_by, created_at DESC),
    INDEX idx_export_requests_status (status, created_at DESC),
    INDEX idx_export_requests_type (export_type)
);

-- Export templates for custom reports
CREATE TABLE IF NOT EXISTS export_templates (
    template_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name TEXT NOT NULL UNIQUE,
    export_type TEXT NOT NULL,
    description TEXT,
    template_config JSONB NOT NULL,
    is_default BOOLEAN DEFAULT false,
    is_public BOOLEAN DEFAULT true,
    created_by TEXT,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    usage_count INTEGER DEFAULT 0,
    INDEX idx_export_templates_type (export_type),
    INDEX idx_export_templates_public (is_public) WHERE is_public = true
);

-- Report schedules for automatic exports
CREATE TABLE IF NOT EXISTS report_schedules (
    schedule_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name TEXT NOT NULL,
    template_id UUID REFERENCES export_templates(template_id) ON DELETE CASCADE,
    export_type TEXT NOT NULL,
    schedule_cron TEXT NOT NULL,
    enabled BOOLEAN DEFAULT true,
    recipients TEXT[] NOT NULL,
    parameters JSONB DEFAULT '{}'::jsonb,
    created_by TEXT,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    last_run_at TIMESTAMP WITH TIME ZONE,
    next_run_at TIMESTAMP WITH TIME ZONE,
    run_count INTEGER DEFAULT 0,
    INDEX idx_report_schedules_enabled (enabled, next_run_at) WHERE enabled = true,
    INDEX idx_report_schedules_user (created_by)
);

-- Export statistics for monitoring
CREATE TABLE IF NOT EXISTS export_statistics (
    stat_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    date DATE NOT NULL DEFAULT CURRENT_DATE,
    export_type TEXT NOT NULL,
    total_requests INTEGER DEFAULT 0,
    successful_exports INTEGER DEFAULT 0,
    failed_exports INTEGER DEFAULT 0,
    avg_processing_time_ms INTEGER,
    total_file_size_bytes BIGINT DEFAULT 0,
    total_downloads INTEGER DEFAULT 0,
    UNIQUE(date, export_type),
    INDEX idx_export_stats_date (date DESC),
    INDEX idx_export_stats_type (export_type, date DESC)
);

-- Create indexes for performance
CREATE INDEX IF NOT EXISTS idx_export_requests_expires ON export_requests(expires_at) WHERE expires_at IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_export_requests_recent ON export_requests(created_at DESC) WHERE created_at > NOW() - INTERVAL '30 days';

-- Insert default export templates
INSERT INTO export_templates (name, export_type, description, template_config, is_default)
VALUES
(
    'Standard Audit Trail PDF',
    'pdf_audit_trail',
    'Comprehensive audit trail report with all compliance activities',
    '{"page_size": "A4", "orientation": "portrait", "include_signatures": true, "include_timestamps": true, "grouping": "by_date"}'::jsonb,
    true
),
(
    'Transaction Export (Excel)',
    'excel_transactions',
    'Detailed transaction list with all metadata',
    '{"include_flagged": true, "include_metadata": true, "date_format": "YYYY-MM-DD HH:mm:ss"}'::jsonb,
    true
),
(
    'Compliance Summary Report',
    'pdf_compliance_report',
    'Executive summary of compliance status and violations',
    '{"include_charts": true, "include_recommendations": true, "summary_level": "executive"}'::jsonb,
    true
)
ON CONFLICT (name) DO NOTHING;

-- Comments for documentation
COMMENT ON TABLE export_requests IS 'Tracking of all export and report generation requests';
COMMENT ON TABLE export_templates IS 'Reusable templates for report generation';
COMMENT ON TABLE report_schedules IS 'Automated report generation schedules';
COMMENT ON TABLE export_statistics IS 'Daily statistics for export and reporting monitoring';



-- =============================================================================
-- FEATURE 4: API KEY MANAGEMENT UI
-- Production-grade LLM API key management with encryption
-- =============================================================================

-- API keys for LLM providers
CREATE TABLE IF NOT EXISTS llm_api_keys (
    key_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    provider TEXT NOT NULL CHECK (provider IN ('openai', 'anthropic', 'cohere', 'huggingface', 'google', 'azure_openai', 'custom')),
    key_name TEXT NOT NULL,
    encrypted_key TEXT NOT NULL,
    encryption_version INTEGER DEFAULT 1,
    is_active BOOLEAN DEFAULT true,
    created_by TEXT NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    last_used_at TIMESTAMP WITH TIME ZONE,
    usage_count INTEGER DEFAULT 0,
    rate_limit_per_minute INTEGER,
    metadata JSONB DEFAULT '{}'::jsonb,
    UNIQUE(provider, key_name),
    INDEX idx_llm_keys_provider (provider, is_active) WHERE is_active = true,
    INDEX idx_llm_keys_user (created_by)
);

-- API key usage tracking
CREATE TABLE IF NOT EXISTS llm_key_usage (
    usage_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    key_id UUID REFERENCES llm_api_keys(key_id) ON DELETE CASCADE,
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    model_used TEXT,
    tokens_used INTEGER DEFAULT 0,
    cost_usd DECIMAL(10,6) DEFAULT 0,
    request_type TEXT,
    success BOOLEAN DEFAULT true,
    error_message TEXT,
    INDEX idx_key_usage_key (key_id, timestamp DESC),
    INDEX idx_key_usage_time (timestamp DESC) WHERE timestamp > NOW() - INTERVAL '7 days'
);

COMMENT ON TABLE llm_api_keys IS 'Encrypted storage of LLM provider API keys';
COMMENT ON TABLE llm_key_usage IS 'Usage tracking for API keys and cost monitoring';



-- =============================================================================
-- FEATURE 5: PREDICTIVE COMPLIANCE RISK SCORING
-- Production-grade ML-based risk prediction infrastructure
-- =============================================================================

-- ML models for risk prediction
CREATE TABLE IF NOT EXISTS compliance_ml_models (
    model_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    model_name TEXT NOT NULL UNIQUE,
    model_type TEXT NOT NULL CHECK (model_type IN ('risk_classification', 'impact_regression', 'time_series_forecast', 'anomaly_detection')),
    model_version TEXT NOT NULL,
    algorithm TEXT NOT NULL,
    training_data_size INTEGER,
    accuracy_score DECIMAL(5,4),
    precision_score DECIMAL(5,4),
    recall_score DECIMAL(5,4),
    f1_score DECIMAL(5,4),
    is_active BOOLEAN DEFAULT true,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    last_trained_at TIMESTAMP WITH TIME ZONE,
    trained_by TEXT,
    hyperparameters JSONB DEFAULT '{}'::jsonb,
    feature_importance JSONB DEFAULT '{}'::jsonb,
    metadata JSONB DEFAULT '{}'::jsonb,
    INDEX idx_ml_models_type (model_type, is_active) WHERE is_active = true
);

-- Risk predictions and scores
CREATE TABLE IF NOT EXISTS compliance_risk_predictions (
    prediction_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    model_id UUID REFERENCES compliance_ml_models(model_id),
    entity_type TEXT NOT NULL CHECK (entity_type IN ('transaction', 'regulatory_change', 'policy', 'vendor', 'process')),
    entity_id TEXT NOT NULL,
    risk_score DECIMAL(5,4) NOT NULL CHECK (risk_score >= 0 AND risk_score <= 1),
    risk_level TEXT NOT NULL CHECK (risk_level IN ('low', 'medium', 'high', 'critical')),
    confidence_score DECIMAL(5,4) CHECK (confidence_score >= 0 AND confidence_score <= 1),
    predicted_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    prediction_horizon_days INTEGER,
    contributing_factors JSONB DEFAULT '[]'::jsonb,
    recommended_actions JSONB DEFAULT '[]'::jsonb,
    actual_outcome TEXT,
    outcome_verified_at TIMESTAMP WITH TIME ZONE,
    metadata JSONB DEFAULT '{}'::jsonb,
    INDEX idx_risk_predictions_entity (entity_type, entity_id),
    INDEX idx_risk_predictions_score (risk_score DESC, predicted_at DESC),
    INDEX idx_risk_predictions_level (risk_level, predicted_at DESC)
);

-- Training data for continuous learning
CREATE TABLE IF NOT EXISTS ml_training_data (
    training_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    model_id UUID REFERENCES compliance_ml_models(model_id) ON DELETE CASCADE,
    feature_vector JSONB NOT NULL,
    label TEXT NOT NULL,
    label_confidence DECIMAL(5,4),
    data_source TEXT,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    is_validated BOOLEAN DEFAULT false,
    validated_by TEXT,
    validated_at TIMESTAMP WITH TIME ZONE,
    INDEX idx_training_data_model (model_id, created_at DESC),
    INDEX idx_training_data_validated (is_validated, created_at DESC)
);

-- Model performance tracking
CREATE TABLE IF NOT EXISTS ml_model_performance (
    performance_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    model_id UUID REFERENCES compliance_ml_models(model_id) ON DELETE CASCADE,
    evaluation_date DATE NOT NULL,
    accuracy DECIMAL(5,4),
    precision DECIMAL(5,4),
    recall DECIMAL(5,4),
    f1_score DECIMAL(5,4),
    auc_roc DECIMAL(5,4),
    predictions_count INTEGER DEFAULT 0,
    correct_predictions INTEGER DEFAULT 0,
    false_positives INTEGER DEFAULT 0,
    false_negatives INTEGER DEFAULT 0,
    UNIQUE(model_id, evaluation_date),
    INDEX idx_model_performance_date (evaluation_date DESC)
);

-- Create indexes for performance
CREATE INDEX IF NOT EXISTS idx_risk_predictions_recent ON compliance_risk_predictions(predicted_at DESC) WHERE predicted_at > NOW() - INTERVAL '30 days';
CREATE INDEX IF NOT EXISTS idx_ml_models_active ON compliance_ml_models(model_name) WHERE is_active = true;

-- Insert default ML model
INSERT INTO compliance_ml_models (model_name, model_type, model_version, algorithm, is_active, accuracy_score, precision_score, recall_score, f1_score)
VALUES
(
    'Regulatory Impact Predictor v1.0',
    'risk_classification',
    '1.0.0',
    'Random Forest Classifier',
    true,
    0.8750,
    0.8900,
    0.8600,
    0.8745
)
ON CONFLICT (model_name) DO NOTHING;

-- Comments for documentation
COMMENT ON TABLE compliance_ml_models IS 'Production ML models for compliance risk prediction';
COMMENT ON TABLE compliance_risk_predictions IS 'Real-time risk predictions with confidence scores';
COMMENT ON TABLE ml_training_data IS 'Training data for continuous model improvement';
COMMENT ON TABLE ml_model_performance IS 'Daily performance tracking for ML models';



-- =============================================================================
-- FEATURE 7: REGULATORY CHANGE SIMULATOR
-- Production-grade sandbox for "what-if" scenario analysis
-- =============================================================================

-- Simulation scenarios for regulatory impact analysis
CREATE TABLE IF NOT EXISTS regulatory_simulations (
    simulation_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name TEXT NOT NULL,
    description TEXT,
    simulation_type TEXT NOT NULL CHECK (simulation_type IN ('regulatory_change', 'policy_update', 'market_shift', 'risk_event', 'custom')),
    status TEXT DEFAULT 'draft' CHECK (status IN ('draft', 'running', 'completed', 'failed', 'archived')),
    created_by TEXT NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    started_at TIMESTAMP WITH TIME ZONE,
    completed_at TIMESTAMP WITH TIME ZONE,
    parameters JSONB NOT NULL DEFAULT '{}'::jsonb,
    baseline_snapshot JSONB DEFAULT '{}'::jsonb,
    results JSONB DEFAULT '{}'::jsonb,
    metadata JSONB DEFAULT '{}'::jsonb,
    INDEX idx_simulations_user (created_by, created_at DESC),
    INDEX idx_simulations_status (status, created_at DESC)
);

-- Simulation scenarios - hypothetical regulatory changes to test
CREATE TABLE IF NOT EXISTS simulation_scenarios (
    scenario_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    simulation_id UUID REFERENCES regulatory_simulations(simulation_id) ON DELETE CASCADE,
    scenario_name TEXT NOT NULL,
    scenario_description TEXT,
    change_type TEXT NOT NULL,
    affected_entities TEXT[] DEFAULT ARRAY[]::TEXT[],
    change_parameters JSONB NOT NULL,
    impact_predictions JSONB DEFAULT '{}'::jsonb,
    execution_order INTEGER DEFAULT 0,
    metadata JSONB DEFAULT '{}'::jsonb,
    INDEX idx_scenarios_simulation (simulation_id, execution_order)
);

-- Simulation results and impact analysis
CREATE TABLE IF NOT EXISTS simulation_results (
    result_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    simulation_id UUID REFERENCES regulatory_simulations(simulation_id) ON DELETE CASCADE,
    scenario_id UUID REFERENCES simulation_scenarios(scenario_id) ON DELETE CASCADE,
    entity_type TEXT NOT NULL,
    entity_id TEXT NOT NULL,
    metric_name TEXT NOT NULL,
    baseline_value JSONB,
    simulated_value JSONB,
    delta_value JSONB,
    delta_percentage DECIMAL(10,4),
    impact_severity TEXT CHECK (impact_severity IN ('minimal', 'low', 'moderate', 'high', 'severe')),
    confidence_score DECIMAL(5,4),
    calculated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    INDEX idx_results_simulation (simulation_id, calculated_at DESC),
    INDEX idx_results_scenario (scenario_id),
    INDEX idx_results_impact (impact_severity, confidence_score DESC)
);

-- Simulation comparisons for side-by-side analysis
CREATE TABLE IF NOT EXISTS simulation_comparisons (
    comparison_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name TEXT NOT NULL,
    description TEXT,
    simulation_ids UUID[] NOT NULL,
    created_by TEXT NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    comparison_metrics TEXT[] DEFAULT ARRAY[]::TEXT[],
    comparison_results JSONB DEFAULT '{}'::jsonb,
    INDEX idx_comparisons_user (created_by, created_at DESC)
);

-- Simulation templates for common scenarios
CREATE TABLE IF NOT EXISTS simulation_templates (
    template_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    template_name TEXT NOT NULL UNIQUE,
    template_category TEXT NOT NULL,
    description TEXT,
    default_parameters JSONB NOT NULL,
    required_inputs TEXT[] DEFAULT ARRAY[]::TEXT[],
    is_public BOOLEAN DEFAULT true,
    created_by TEXT,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    usage_count INTEGER DEFAULT 0,
    INDEX idx_templates_category (template_category),
    INDEX idx_templates_public (is_public) WHERE is_public = true
);

-- Create indexes for performance
CREATE INDEX IF NOT EXISTS idx_simulations_recent ON regulatory_simulations(created_at DESC) WHERE created_at > NOW() - INTERVAL '90 days';
CREATE INDEX IF NOT EXISTS idx_results_entity ON simulation_results(entity_type, entity_id);

-- Insert default simulation templates
INSERT INTO simulation_templates (template_name, template_category, description, default_parameters, required_inputs)
VALUES
(
    'SEC Reporting Rule Change',
    'regulatory_change',
    'Simulate impact of new SEC reporting requirements',
    '{"affected_forms": ["10-K", "10-Q"], "new_disclosure_requirements": [], "implementation_timeline_days": 180}'::jsonb,
    ARRAY['affected_forms', 'new_disclosure_requirements']
),
(
    'Capital Requirement Adjustment',
    'policy_update',
    'Simulate changes to capital adequacy ratios',
    '{"current_ratio": 0.08, "new_ratio": 0.10, "adjustment_period_months": 12}'::jsonb,
    ARRAY['current_ratio', 'new_ratio']
),
(
    'Transaction Volume Stress Test',
    'risk_event',
    'Test system resilience under high transaction volumes',
    '{"baseline_tps": 1000, "stress_tps": 10000, "duration_minutes": 30}'::jsonb,
    ARRAY['stress_tps', 'duration_minutes']
)
ON CONFLICT (template_name) DO NOTHING;

-- Comments for documentation
COMMENT ON TABLE regulatory_simulations IS 'Main simulations for what-if regulatory scenario analysis';
COMMENT ON TABLE simulation_scenarios IS 'Individual scenarios within a simulation run';
COMMENT ON TABLE simulation_results IS 'Detailed impact analysis results per entity and metric';
COMMENT ON TABLE simulation_comparisons IS 'Side-by-side comparison of multiple simulation runs';
COMMENT ON TABLE simulation_templates IS 'Reusable templates for common simulation scenarios';



-- =============================================================================
-- FEATURE 8: ADVANCED ANALYTICS & BI DASHBOARD
-- Production-grade executive analytics and data storytelling
-- =============================================================================

-- BI dashboards and reports
CREATE TABLE IF NOT EXISTS bi_dashboards (
    dashboard_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    dashboard_name TEXT NOT NULL UNIQUE,
    dashboard_type TEXT NOT NULL CHECK (dashboard_type IN ('executive', 'operational', 'compliance', 'risk', 'custom')),
    description TEXT,
    layout_config JSONB NOT NULL DEFAULT '{}'::jsonb,
    widgets JSONB NOT NULL DEFAULT '[]'::jsonb,
    refresh_interval_seconds INTEGER DEFAULT 300,
    is_public BOOLEAN DEFAULT false,
    created_by TEXT NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    last_viewed_at TIMESTAMP WITH TIME ZONE,
    view_count INTEGER DEFAULT 0,
    INDEX idx_dashboards_type (dashboard_type),
    INDEX idx_dashboards_public (is_public) WHERE is_public = true
);

-- Analytics metrics and KPIs
CREATE TABLE IF NOT EXISTS analytics_metrics (
    metric_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    metric_name TEXT NOT NULL,
    metric_category TEXT NOT NULL,
    metric_value DECIMAL(20,4),
    metric_unit TEXT,
    aggregation_period TEXT CHECK (aggregation_period IN ('realtime', 'hourly', 'daily', 'weekly', 'monthly')),
    calculated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    dimensions JSONB DEFAULT '{}'::jsonb,
    metadata JSONB DEFAULT '{}'::jsonb,
    INDEX idx_analytics_metrics_name (metric_name, calculated_at DESC),
    INDEX idx_analytics_metrics_category (metric_category, calculated_at DESC),
    INDEX idx_analytics_metrics_period (aggregation_period, calculated_at DESC)
);

-- Data insights and automated discoveries
CREATE TABLE IF NOT EXISTS data_insights (
    insight_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    insight_type TEXT NOT NULL CHECK (insight_type IN ('trend', 'anomaly', 'pattern', 'correlation', 'recommendation')),
    title TEXT NOT NULL,
    description TEXT NOT NULL,
    confidence_score DECIMAL(5,4),
    priority TEXT CHECK (priority IN ('low', 'medium', 'high', 'critical')),
    discovered_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    affected_metrics TEXT[] DEFAULT ARRAY[]::TEXT[],
    supporting_data JSONB DEFAULT '{}'::jsonb,
    recommended_actions TEXT[],
    is_dismissed BOOLEAN DEFAULT false,
    dismissed_by TEXT,
    dismissed_at TIMESTAMP WITH TIME ZONE,
    INDEX idx_insights_type (insight_type, discovered_at DESC),
    INDEX idx_insights_priority (priority, discovered_at DESC) WHERE is_dismissed = false
);

-- Scheduled reports
CREATE TABLE IF NOT EXISTS scheduled_reports (
    report_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    report_name TEXT NOT NULL,
    dashboard_id UUID REFERENCES bi_dashboards(dashboard_id) ON DELETE CASCADE,
    schedule_cron TEXT NOT NULL,
    recipients TEXT[] NOT NULL,
    delivery_format TEXT[] DEFAULT ARRAY['pdf','email'],
    is_enabled BOOLEAN DEFAULT true,
    created_by TEXT NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    last_sent_at TIMESTAMP WITH TIME ZONE,
    next_scheduled_at TIMESTAMP WITH TIME ZONE,
    send_count INTEGER DEFAULT 0,
    INDEX idx_scheduled_reports_enabled (is_enabled, next_scheduled_at) WHERE is_enabled = true
);

-- Create indexes for performance
CREATE INDEX IF NOT EXISTS idx_metrics_recent ON analytics_metrics(calculated_at DESC) WHERE calculated_at > NOW() - INTERVAL '7 days';
CREATE INDEX IF NOT EXISTS idx_insights_active ON data_insights(discovered_at DESC) WHERE is_dismissed = false;

-- Insert default executive dashboard
INSERT INTO bi_dashboards (dashboard_name, dashboard_type, description, created_by)
VALUES
(
    'Executive Compliance Overview',
    'executive',
    'High-level compliance metrics and KPIs for executive decision making',
    'system'
)
ON CONFLICT (dashboard_name) DO NOTHING;

-- Comments for documentation
COMMENT ON TABLE bi_dashboards IS 'Business intelligence dashboards with customizable layouts';
COMMENT ON TABLE analytics_metrics IS 'Time-series analytics metrics and KPIs';
COMMENT ON TABLE data_insights IS 'Automated insights and data discoveries';
COMMENT ON TABLE scheduled_reports IS 'Automated report scheduling and delivery';



-- =============================================================================
-- FEATURES 10, 12, 13, 14: Remaining Production Features
-- =============================================================================

-- FEATURE 10: Natural Language Policy Builder (GPT-4 powered)
CREATE TABLE IF NOT EXISTS nl_policy_rules (
    rule_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    rule_name TEXT NOT NULL,
    natural_language_input TEXT NOT NULL,
    generated_rule_logic JSONB NOT NULL,
    rule_type TEXT NOT NULL,
    created_by TEXT NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    is_active BOOLEAN DEFAULT false,
    confidence_score DECIMAL(5,4),
    validation_status TEXT CHECK (validation_status IN ('pending', 'approved', 'rejected'))
);

-- FEATURE 12: Regulatory Chatbot (Slack/Teams integration)
CREATE TABLE IF NOT EXISTS chatbot_conversations (
    conversation_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id TEXT NOT NULL,
    platform TEXT CHECK (platform IN ('slack', 'teams', 'web')),
    started_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    last_message_at TIMESTAMP WITH TIME ZONE,
    message_count INTEGER DEFAULT 0,
    context JSONB DEFAULT '{}'::jsonb
);

CREATE TABLE IF NOT EXISTS chatbot_messages (
    message_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    conversation_id UUID REFERENCES chatbot_conversations(conversation_id) ON DELETE CASCADE,
    message_type TEXT CHECK (message_type IN ('user', 'bot')),
    message_text TEXT NOT NULL,
    intent_detected TEXT,
    entities_extracted JSONB,
    response_confidence DECIMAL(5,4),
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- FEATURE 13: Integration Marketplace
CREATE TABLE IF NOT EXISTS integrations (
    integration_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    integration_name TEXT NOT NULL UNIQUE,
    integration_type TEXT NOT NULL,
    description TEXT,
    provider TEXT,
    configuration JSONB NOT NULL,
    is_enabled BOOLEAN DEFAULT false,
    is_verified BOOLEAN DEFAULT false,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    install_count INTEGER DEFAULT 0
);

CREATE TABLE IF NOT EXISTS integration_logs (
    log_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    integration_id UUID REFERENCES integrations(integration_id) ON DELETE CASCADE,
    event_type TEXT NOT NULL,
    event_data JSONB,
    status TEXT CHECK (status IN ('success', 'failure', 'pending')),
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- FEATURE 14: Compliance Training Module
CREATE TABLE IF NOT EXISTS training_courses (
    course_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    course_name TEXT NOT NULL,
    course_category TEXT NOT NULL,
    content JSONB NOT NULL,
    duration_minutes INTEGER,
    passing_score INTEGER DEFAULT 80,
    is_published BOOLEAN DEFAULT false,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    enrollment_count INTEGER DEFAULT 0
);

CREATE TABLE IF NOT EXISTS training_enrollments (
    enrollment_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    course_id UUID REFERENCES training_courses(course_id) ON DELETE CASCADE,
    user_id TEXT NOT NULL,
    enrolled_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    completed_at TIMESTAMP WITH TIME ZONE,
    score INTEGER,
    certificate_issued BOOLEAN DEFAULT false,
    progress_data JSONB DEFAULT '{}'::jsonb
);

-- Comments
COMMENT ON TABLE nl_policy_rules IS 'GPT-4 generated compliance rules from natural language';
COMMENT ON TABLE chatbot_conversations IS 'Regulatory chatbot conversation tracking';
COMMENT ON TABLE integrations IS 'Enterprise system integrations marketplace';
COMMENT ON TABLE training_courses IS 'Gamified compliance training courses';



-- ============================================================================
-- FEATURE 10: NATURAL LANGUAGE POLICY BUILDER (GPT-4 POWERED)
-- ============================================================================

CREATE TABLE IF NOT EXISTS nl_policy_rules (
    rule_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    rule_name VARCHAR(255) NOT NULL,
    natural_language_input TEXT NOT NULL,
    generated_rule_logic JSONB,
    rule_type VARCHAR(50) DEFAULT 'compliance',
    is_active BOOLEAN DEFAULT false,
    created_by VARCHAR(255),
    confidence_score DECIMAL(3,2),
    validation_status VARCHAR(50) DEFAULT 'pending',
    activated_at TIMESTAMP,
    deactivated_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_nlpolicy_active ON nl_policy_rules(is_active, validation_status);
CREATE INDEX IF NOT EXISTS idx_nlpolicy_type ON nl_policy_rules(rule_type);

-- ============================================================================
-- FEATURE 12: REGULATORY CHATBOT (SLACK/TEAMS/WEB)
-- ============================================================================

CREATE TABLE IF NOT EXISTS chatbot_conversations (
    conversation_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    platform VARCHAR(50) NOT NULL,
    user_id VARCHAR(255),
    channel_id VARCHAR(255),
    started_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_message_at TIMESTAMP,
    message_count INTEGER DEFAULT 0,
    is_active BOOLEAN DEFAULT true
);

CREATE TABLE IF NOT EXISTS chatbot_messages (
    message_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    conversation_id UUID REFERENCES chatbot_conversations(conversation_id) ON DELETE CASCADE,
    role VARCHAR(20) NOT NULL,
    message_text TEXT NOT NULL,
    intent VARCHAR(100),
    confidence_score DECIMAL(3,2),
    response_time_ms INTEGER,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS chatbot_knowledge_queries (
    query_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    conversation_id UUID REFERENCES chatbot_conversations(conversation_id),
    query_text TEXT NOT NULL,
    knowledge_base_hits JSONB,
    response_generated TEXT,
    was_helpful BOOLEAN,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_chatbot_conv_platform ON chatbot_conversations(platform, is_active);
CREATE INDEX IF NOT EXISTS idx_chatbot_messages_conv ON chatbot_messages(conversation_id, created_at DESC);

-- ============================================================================
-- KNOWLEDGE BASE EXTENDED FEATURES (Cases, Q&A, Embeddings, Relationships)
-- ============================================================================

-- Knowledge base cases - Documented case examples for learning and reference
CREATE TABLE IF NOT EXISTS knowledge_cases (
    case_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    case_title VARCHAR(500) NOT NULL,
    case_description TEXT NOT NULL,
    domain VARCHAR(50) NOT NULL CHECK (domain IN ('REGULATORY_COMPLIANCE', 'TRANSACTION_MONITORING', 'AUDIT_INTELLIGENCE', 'BUSINESS_PROCESSES', 'RISK_MANAGEMENT', 'LEGAL_FRAMEWORKS', 'FINANCIAL_INSTRUMENTS', 'MARKET_INTELLIGENCE')),
    case_type VARCHAR(50) NOT NULL CHECK (case_type IN ('SUCCESS', 'FAILURE', 'LESSON_LEARNED', 'BEST_PRACTICE', 'REGULATORY_BREACH', 'FRAUD_DETECTION', 'COMPLIANCE_AUDIT', 'RISK_MITIGATION')),
    situation TEXT NOT NULL,  -- The situation/problem
    action TEXT NOT NULL,     -- Actions taken
    result TEXT NOT NULL,     -- Outcomes/results
    lessons_learned TEXT,     -- Key takeaways
    related_regulations TEXT[],
    related_entities UUID[], -- References to knowledge_entities
    severity VARCHAR(20) CHECK (severity IN ('LOW', 'MEDIUM', 'HIGH', 'CRITICAL')),
    outcome_status VARCHAR(30) CHECK (outcome_status IN ('POSITIVE', 'NEGATIVE', 'MIXED', 'PENDING')),
    financial_impact DECIMAL(15, 2),
    tags TEXT[] DEFAULT ARRAY[]::TEXT[],
    metadata JSONB DEFAULT '{}'::jsonb,
    created_by VARCHAR(255),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    is_archived BOOLEAN DEFAULT false,
    view_count INTEGER DEFAULT 0,
    usefulness_score DECIMAL(3, 2) DEFAULT 0.0 CHECK (usefulness_score >= 0 AND usefulness_score <= 1)
);

-- Knowledge entry relationships (for similarity and related content)
CREATE TABLE IF NOT EXISTS knowledge_entry_relationships (
    relationship_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    entry_a_id VARCHAR(255) NOT NULL REFERENCES knowledge_entities(entity_id) ON DELETE CASCADE,
    entry_b_id VARCHAR(255) NOT NULL REFERENCES knowledge_entities(entity_id) ON DELETE CASCADE,
    relationship_type VARCHAR(50) NOT NULL CHECK (relationship_type IN ('SIMILAR', 'RELATED', 'PARENT_CHILD', 'SUPERSEDES', 'REFERENCES', 'CONTRADICTS', 'EXTENDS', 'IMPLEMENTS')),
    similarity_score DECIMAL(5, 4) CHECK (similarity_score >= 0 AND similarity_score <= 1),
    relationship_metadata JSONB DEFAULT '{}'::jsonb,
    computed_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    computed_by VARCHAR(100), -- 'SYSTEM', 'USER', specific AI model name
    is_validated BOOLEAN DEFAULT false,
    validated_by VARCHAR(255),
    validated_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    UNIQUE(entry_a_id, entry_b_id, relationship_type),
    CHECK (entry_a_id != entry_b_id)  -- Prevent self-referencing
);

-- Knowledge Q&A sessions (for interactive questioning and answers)
CREATE TABLE IF NOT EXISTS knowledge_qa_sessions (
    session_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    question TEXT NOT NULL,
    question_embedding VECTOR(384), -- Question vector for semantic search
    answer TEXT,
    answer_type VARCHAR(50) CHECK (answer_type IN ('DIRECT', 'SYNTHESIZED', 'REFERENCED', 'UNCERTAIN', 'NO_ANSWER')),
    context_ids JSONB, -- Array of entry IDs used as context
    sources JSONB, -- Detailed source information with snippets
    confidence DECIMAL(3, 2) CHECK (confidence >= 0 AND confidence <= 1),
    model_used VARCHAR(100), -- LLM or system used for answer generation
    processing_time_ms INTEGER,
    tokens_used INTEGER,
    user_id VARCHAR(255),
    session_context JSONB DEFAULT '{}'::jsonb, -- Additional context about the query
    feedback_rating INTEGER CHECK (feedback_rating >= 1 AND feedback_rating <= 5),
    feedback_comment TEXT,
    feedback_at TIMESTAMP WITH TIME ZONE,
    was_helpful BOOLEAN,
    follow_up_questions TEXT[],
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    answered_at TIMESTAMP WITH TIME ZONE
);

-- Embedding cache (for fast similarity search and vector operations)
CREATE TABLE IF NOT EXISTS knowledge_embeddings (
    embedding_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    entry_id VARCHAR(255) NOT NULL REFERENCES knowledge_entities(entity_id) ON DELETE CASCADE,
    embedding_model VARCHAR(100) NOT NULL, -- e.g., 'sentence-transformers/all-MiniLM-L6-v2', 'openai/text-embedding-ada-002'
    embedding_vector VECTOR(384), -- Vector representation (dimension varies by model)
    embedding_type VARCHAR(50) CHECK (embedding_type IN ('TITLE', 'CONTENT', 'FULL', 'CHUNK', 'SUMMARY')),
    chunk_index INTEGER DEFAULT 0, -- For entries split into chunks
    chunk_text TEXT, -- The actual text that was embedded
    chunk_metadata JSONB DEFAULT '{}'::jsonb,
    generation_metadata JSONB DEFAULT '{}'::jsonb, -- Model params, API version, etc.
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    expires_at TIMESTAMP WITH TIME ZONE, -- For cache invalidation
    is_current BOOLEAN DEFAULT true, -- Mark old embeddings as outdated
    UNIQUE(entry_id, embedding_model, chunk_index, embedding_type)
);

-- Batch embedding jobs (for tracking large-scale embedding generation)
CREATE TABLE IF NOT EXISTS knowledge_embedding_jobs (
    job_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    job_name VARCHAR(255),
    model_name VARCHAR(100) NOT NULL,
    target_filter JSONB, -- Filter criteria for which entities to embed
    total_entries INTEGER DEFAULT 0,
    processed_entries INTEGER DEFAULT 0,
    failed_entries INTEGER DEFAULT 0,
    status VARCHAR(30) NOT NULL CHECK (status IN ('PENDING', 'RUNNING', 'COMPLETED', 'FAILED', 'CANCELLED')),
    error_message TEXT,
    started_at TIMESTAMP WITH TIME ZONE,
    completed_at TIMESTAMP WITH TIME ZONE,
    created_by VARCHAR(255),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Indices for knowledge base extended features
CREATE INDEX IF NOT EXISTS idx_knowledge_cases_domain ON knowledge_cases(domain);
CREATE INDEX IF NOT EXISTS idx_knowledge_cases_type ON knowledge_cases(case_type);
CREATE INDEX IF NOT EXISTS idx_knowledge_cases_created ON knowledge_cases(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_knowledge_cases_severity ON knowledge_cases(severity);

CREATE INDEX IF NOT EXISTS idx_knowledge_entry_rel_entry_a ON knowledge_entry_relationships(entry_a_id);
CREATE INDEX IF NOT EXISTS idx_knowledge_entry_rel_entry_b ON knowledge_entry_relationships(entry_b_id);
CREATE INDEX IF NOT EXISTS idx_knowledge_entry_rel_type ON knowledge_entry_relationships(relationship_type);
CREATE INDEX IF NOT EXISTS idx_knowledge_entry_rel_similarity ON knowledge_entry_relationships(similarity_score DESC);

CREATE INDEX IF NOT EXISTS idx_knowledge_qa_created ON knowledge_qa_sessions(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_knowledge_qa_user ON knowledge_qa_sessions(user_id);
CREATE INDEX IF NOT EXISTS idx_knowledge_qa_confidence ON knowledge_qa_sessions(confidence DESC);

CREATE INDEX IF NOT EXISTS idx_knowledge_embeddings_entry ON knowledge_embeddings(entry_id);
CREATE INDEX IF NOT EXISTS idx_knowledge_embeddings_model ON knowledge_embeddings(embedding_model);
CREATE INDEX IF NOT EXISTS idx_knowledge_embeddings_current ON knowledge_embeddings(entry_id, is_current);
-- Vector similarity search index (requires pgvector extension)
-- CREATE INDEX IF NOT EXISTS idx_knowledge_embeddings_vector ON knowledge_embeddings USING ivfflat (embedding_vector vector_cosine_ops) WITH (lists = 100);

CREATE INDEX IF NOT EXISTS idx_knowledge_embedding_jobs_status ON knowledge_embedding_jobs(status);
CREATE INDEX IF NOT EXISTS idx_knowledge_embedding_jobs_created ON knowledge_embedding_jobs(created_at DESC);

-- ============================================================================
-- LLM INTEGRATION (Conversations, Batch, Fine-tuning, Analytics)
-- ============================================================================

-- LLM model registry (available models across providers)
CREATE TABLE IF NOT EXISTS llm_model_registry (
    model_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    model_name VARCHAR(255) NOT NULL UNIQUE,
    provider VARCHAR(50) NOT NULL CHECK (provider IN ('openai', 'anthropic', 'local', 'custom', 'huggingface', 'cohere', 'google')),
    model_version VARCHAR(50),
    model_type VARCHAR(50) CHECK (model_type IN ('completion', 'chat', 'embedding', 'fine_tuned', 'instruct')),
    context_length INTEGER,
    max_tokens INTEGER,
    cost_per_1k_input_tokens DECIMAL(10, 6),
    cost_per_1k_output_tokens DECIMAL(10, 6),
    capabilities JSONB DEFAULT '[]'::jsonb, -- Array of capabilities
    parameters JSONB DEFAULT '{}'::jsonb, -- Model-specific parameters
    is_available BOOLEAN DEFAULT true,
    is_deprecated BOOLEAN DEFAULT false,
    base_model_id UUID, -- For fine-tuned models
    description TEXT,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    FOREIGN KEY (base_model_id) REFERENCES llm_model_registry(model_id) ON DELETE SET NULL
);

-- LLM conversations (chat history and context management)
CREATE TABLE IF NOT EXISTS llm_conversations (
    conversation_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    title VARCHAR(255),
    model_id UUID REFERENCES llm_model_registry(model_id) ON DELETE SET NULL,
    system_prompt TEXT,
    user_id VARCHAR(255),
    status VARCHAR(20) DEFAULT 'active' CHECK (status IN ('active', 'archived', 'deleted')),
    message_count INTEGER DEFAULT 0,
    total_tokens INTEGER DEFAULT 0,
    total_cost DECIMAL(10, 4) DEFAULT 0,
    temperature DECIMAL(3, 2),
    max_tokens INTEGER,
    metadata JSONB DEFAULT '{}'::jsonb,
    tags TEXT[] DEFAULT ARRAY[]::TEXT[],
    last_activity_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- LLM messages (individual messages in conversations)
CREATE TABLE IF NOT EXISTS llm_messages (
    message_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    conversation_id UUID REFERENCES llm_conversations(conversation_id) ON DELETE CASCADE,
    role VARCHAR(20) NOT NULL CHECK (role IN ('user', 'assistant', 'system', 'function')),
    content TEXT NOT NULL,
    tokens INTEGER,
    cost DECIMAL(10, 6),
    model_id UUID REFERENCES llm_model_registry(model_id),
    latency_ms INTEGER,
    metadata JSONB DEFAULT '{}'::jsonb,
    finish_reason VARCHAR(50), -- 'stop', 'length', 'content_filter', 'function_call'
    function_call JSONB, -- For function calling support
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- LLM usage statistics (aggregated usage tracking)
CREATE TABLE IF NOT EXISTS llm_usage_stats (
    stat_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    model_id UUID REFERENCES llm_model_registry(model_id),
    user_id VARCHAR(255),
    usage_date DATE NOT NULL,
    request_count INTEGER DEFAULT 0,
    input_tokens INTEGER DEFAULT 0,
    output_tokens INTEGER DEFAULT 0,
    total_tokens INTEGER DEFAULT 0,
    total_cost DECIMAL(10, 4) DEFAULT 0,
    avg_latency_ms INTEGER,
    error_count INTEGER DEFAULT 0,
    success_count INTEGER DEFAULT 0,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    UNIQUE(model_id, user_id, usage_date)
);

-- LLM batch processing jobs (async batch processing)
CREATE TABLE IF NOT EXISTS llm_batch_jobs (
    job_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    job_name VARCHAR(255),
    model_id UUID REFERENCES llm_model_registry(model_id),
    status VARCHAR(20) NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'running', 'completed', 'failed', 'cancelled')),
    items JSONB NOT NULL, -- Array of {id, prompt, context} objects
    system_prompt TEXT,
    temperature DECIMAL(3, 2) DEFAULT 0.7,
    max_tokens INTEGER DEFAULT 1000,
    batch_size INTEGER DEFAULT 10,
    total_items INTEGER,
    completed_items INTEGER DEFAULT 0,
    failed_items INTEGER DEFAULT 0,
    results JSONB DEFAULT '[]'::jsonb, -- Array of results
    total_tokens INTEGER DEFAULT 0,
    total_cost DECIMAL(10, 4) DEFAULT 0,
    progress DECIMAL(5, 2) DEFAULT 0,
    error_message TEXT,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    started_at TIMESTAMP WITH TIME ZONE,
    completed_at TIMESTAMP WITH TIME ZONE,
    created_by VARCHAR(255)
);

-- LLM fine-tuning jobs (model customization)
CREATE TABLE IF NOT EXISTS llm_fine_tune_jobs (
    job_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    job_name VARCHAR(255),
    base_model_id UUID REFERENCES llm_model_registry(model_id),
    status VARCHAR(20) NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'running', 'completed', 'failed', 'cancelled')),
    training_dataset TEXT NOT NULL, -- Path or URL to dataset
    validation_dataset TEXT,
    epochs INTEGER DEFAULT 3,
    learning_rate DECIMAL(10, 8) DEFAULT 0.00001,
    batch_size INTEGER DEFAULT 4,
    fine_tuned_model_id UUID, -- Reference to new model after completion
    training_progress DECIMAL(5, 2) DEFAULT 0 CHECK (training_progress >= 0 AND training_progress <= 100),
    training_loss DECIMAL(10, 6),
    validation_loss DECIMAL(10, 6),
    training_samples INTEGER,
    training_tokens BIGINT,
    cost DECIMAL(10, 4),
    hyperparameters JSONB DEFAULT '{}'::jsonb,
    metrics JSONB DEFAULT '{}'::jsonb,
    checkpoints JSONB DEFAULT '[]'::jsonb,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    started_at TIMESTAMP WITH TIME ZONE,
    completed_at TIMESTAMP WITH TIME ZONE,
    error_message TEXT,
    created_by VARCHAR(255),
    FOREIGN KEY (fine_tuned_model_id) REFERENCES llm_model_registry(model_id) ON DELETE SET NULL
);

-- LLM model benchmarks (performance evaluation)
CREATE TABLE IF NOT EXISTS llm_model_benchmarks (
    benchmark_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    model_id UUID REFERENCES llm_model_registry(model_id) ON DELETE CASCADE,
    benchmark_name VARCHAR(255) NOT NULL,
    benchmark_type VARCHAR(50) CHECK (benchmark_type IN ('accuracy', 'latency', 'cost', 'quality', 'comprehensive', 'compliance', 'bias')),
    score DECIMAL(8, 4),
    percentile DECIMAL(5, 2),
    comparison_baseline VARCHAR(100),
    test_cases_count INTEGER,
    passed_cases INTEGER,
    failed_cases INTEGER,
    avg_latency_ms INTEGER,
    avg_tokens_per_request INTEGER,
    avg_cost_per_request DECIMAL(10, 6),
    details JSONB DEFAULT '{}'::jsonb,
    dataset_used VARCHAR(255),
    methodology TEXT,
    tested_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    tested_by VARCHAR(255)
);

-- LLM text analysis results (for analyze endpoint)
CREATE TABLE IF NOT EXISTS llm_text_analysis (
    analysis_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    text_input TEXT NOT NULL,
    model_id UUID REFERENCES llm_model_registry(model_id),
    analysis_type VARCHAR(50) CHECK (analysis_type IN ('sentiment', 'entities', 'summary', 'classification', 'compliance_check', 'risk_assessment', 'comprehensive')),
    sentiment_score DECIMAL(3, 2), -- -1 to 1
    sentiment_label VARCHAR(20),
    entities JSONB DEFAULT '[]'::jsonb,
    summary TEXT,
    classifications JSONB DEFAULT '[]'::jsonb,
    key_points JSONB DEFAULT '[]'::jsonb,
    compliance_findings JSONB DEFAULT '[]'::jsonb,
    risk_score DECIMAL(5, 2),
    confidence DECIMAL(5, 4),
    tokens_used INTEGER,
    cost DECIMAL(10, 6),
    processing_time_ms INTEGER,
    user_id VARCHAR(255),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- LLM report exports
CREATE TABLE IF NOT EXISTS llm_report_exports (
    export_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    export_type VARCHAR(30) CHECK (export_type IN ('conversation', 'analysis', 'usage', 'batch', 'benchmark', 'fine_tune')),
    format VARCHAR(20) CHECK (format IN ('pdf', 'json', 'txt', 'md', 'csv', 'excel')),
    conversation_id UUID,
    analysis_id UUID,
    batch_job_id UUID,
    include_metadata BOOLEAN DEFAULT false,
    include_token_stats BOOLEAN DEFAULT false,
    include_cost_breakdown BOOLEAN DEFAULT false,
    time_range_start TIMESTAMP WITH TIME ZONE,
    time_range_end TIMESTAMP WITH TIME ZONE,
    file_path TEXT,
    file_url TEXT,
    file_size_bytes BIGINT,
    status VARCHAR(20) DEFAULT 'pending' CHECK (status IN ('pending', 'generating', 'completed', 'failed', 'expired')),
    generated_at TIMESTAMP WITH TIME ZONE,
    expires_at TIMESTAMP WITH TIME ZONE,
    download_count INTEGER DEFAULT 0,
    created_by VARCHAR(255),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    FOREIGN KEY (conversation_id) REFERENCES llm_conversations(conversation_id) ON DELETE SET NULL,
    FOREIGN KEY (analysis_id) REFERENCES llm_text_analysis(analysis_id) ON DELETE SET NULL,
    FOREIGN KEY (batch_job_id) REFERENCES llm_batch_jobs(job_id) ON DELETE SET NULL
);

-- Indices for LLM integration
CREATE INDEX IF NOT EXISTS idx_llm_models_provider ON llm_model_registry(provider, is_available);
CREATE INDEX IF NOT EXISTS idx_llm_conversations_user ON llm_conversations(user_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_llm_conversations_status ON llm_conversations(status, last_activity_at DESC);
CREATE INDEX IF NOT EXISTS idx_llm_messages_conversation ON llm_messages(conversation_id, created_at);
CREATE INDEX IF NOT EXISTS idx_llm_usage_date ON llm_usage_stats(usage_date DESC, model_id);
CREATE INDEX IF NOT EXISTS idx_llm_usage_user ON llm_usage_stats(user_id, usage_date DESC);
CREATE INDEX IF NOT EXISTS idx_llm_batch_jobs_status ON llm_batch_jobs(status, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_llm_batch_jobs_user ON llm_batch_jobs(created_by, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_llm_fine_tune_jobs_status ON llm_fine_tune_jobs(status, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_llm_fine_tune_jobs_user ON llm_fine_tune_jobs(created_by, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_llm_benchmarks_model ON llm_model_benchmarks(model_id, tested_at DESC);
CREATE INDEX IF NOT EXISTS idx_llm_text_analysis_user ON llm_text_analysis(user_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_llm_exports_status ON llm_report_exports(status, expires_at);

-- ============================================================================
-- FEATURE 13: INTEGRATION MARKETPLACE (PRE-BUILT CONNECTORS)
-- ============================================================================

CREATE TABLE IF NOT EXISTS integration_connectors (
    connector_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    connector_name VARCHAR(255) NOT NULL,
    connector_type VARCHAR(100) NOT NULL,
    vendor VARCHAR(255),
    description TEXT,
    is_verified BOOLEAN DEFAULT false,
    is_active BOOLEAN DEFAULT false,
    configuration_schema JSONB,
    icon_url TEXT,
    documentation_url TEXT,
    pricing_tier VARCHAR(50) DEFAULT 'free',
    install_count INTEGER DEFAULT 0,
    rating DECIMAL(2,1),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS integration_instances (
    instance_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    connector_id UUID REFERENCES integration_connectors(connector_id) ON DELETE CASCADE,
    instance_name VARCHAR(255) NOT NULL,
    configuration JSONB,
    is_enabled BOOLEAN DEFAULT true,
    last_sync_at TIMESTAMP,
    sync_status VARCHAR(50) DEFAULT 'pending',
    error_count INTEGER DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS integration_sync_logs (
    log_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    instance_id UUID REFERENCES integration_instances(instance_id) ON DELETE CASCADE,
    sync_direction VARCHAR(20),
    records_processed INTEGER DEFAULT 0,
    records_succeeded INTEGER DEFAULT 0,
    records_failed INTEGER DEFAULT 0,
    started_at TIMESTAMP,
    completed_at TIMESTAMP,
    error_message TEXT,
    sync_duration_ms INTEGER
);

CREATE INDEX IF NOT EXISTS idx_integration_active ON integration_connectors(is_active, connector_type);
CREATE INDEX IF NOT EXISTS idx_integration_inst_enabled ON integration_instances(is_enabled);

-- ============================================================================
-- FEATURE 14: COMPLIANCE TRAINING MODULE (GAMIFIED LEARNING)
-- ============================================================================

CREATE TABLE IF NOT EXISTS training_courses (
    course_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    course_name VARCHAR(255) NOT NULL,
    course_category VARCHAR(100),
    description TEXT,
    difficulty_level VARCHAR(50) DEFAULT 'intermediate',
    estimated_duration_minutes INTEGER,
    is_required BOOLEAN DEFAULT false,
    is_published BOOLEAN DEFAULT true,
    passing_score INTEGER DEFAULT 80,
    points_reward INTEGER DEFAULT 100,
    badge_icon VARCHAR(255),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS training_modules (
    module_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    course_id UUID REFERENCES training_courses(course_id) ON DELETE CASCADE,
    module_name VARCHAR(255) NOT NULL,
    module_order INTEGER,
    content_type VARCHAR(50),
    content_data JSONB,
    duration_minutes INTEGER,
    is_interactive BOOLEAN DEFAULT false
);

CREATE TABLE IF NOT EXISTS training_enrollments (
    enrollment_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    course_id UUID REFERENCES training_courses(course_id) ON DELETE CASCADE,
    user_id VARCHAR(255) NOT NULL,
    enrolled_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    started_at TIMESTAMP,
    completed_at TIMESTAMP,
    current_module_id UUID,
    progress_percent INTEGER DEFAULT 0,
    score INTEGER,
    attempts_count INTEGER DEFAULT 0,
    certificate_issued_at TIMESTAMP,
    certificate_url TEXT
);

CREATE TABLE IF NOT EXISTS training_quiz_results (
    result_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    enrollment_id UUID REFERENCES training_enrollments(enrollment_id) ON DELETE CASCADE,
    module_id UUID REFERENCES training_modules(module_id),
    quiz_data JSONB,
    score INTEGER,
    max_score INTEGER,
    time_spent_seconds INTEGER,
    passed BOOLEAN,
    completed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS training_leaderboard (
    entry_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id VARCHAR(255) NOT NULL,
    total_points INTEGER DEFAULT 0,
    courses_completed INTEGER DEFAULT 0,
    badges_earned JSONB,
    rank INTEGER,
    last_activity_at TIMESTAMP,
    UNIQUE(user_id)
);

CREATE INDEX IF NOT EXISTS idx_training_courses_published ON training_courses(is_published, course_category);
CREATE INDEX IF NOT EXISTS idx_training_enrollments_user ON training_enrollments(user_id, completed_at);
CREATE INDEX IF NOT EXISTS idx_training_leaderboard_rank ON training_leaderboard(rank);

-- ============================================================================
-- JURISDICTION RISK RATINGS TABLE
-- Production-grade country/jurisdiction risk assessment database
-- Integrates FATF, OFAC, EU regulatory risk ratings
-- ============================================================================

CREATE TABLE IF NOT EXISTS jurisdiction_risk_ratings (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    country_code VARCHAR(3) NOT NULL,
    country_name VARCHAR(255) NOT NULL,
    risk_tier VARCHAR(50) NOT NULL CHECK (risk_tier IN ('EXTREME', 'HIGH', 'MODERATE', 'LOW', 'MINIMAL')),
    risk_score DECIMAL(3, 2) NOT NULL CHECK (risk_score >= 0.0 AND risk_score <= 1.0),
    fatf_listing VARCHAR(50) CHECK (fatf_listing IN ('BLACK', 'GREY', 'WHITE', 'NOT_LISTED')),
    ofac_sanctions BOOLEAN DEFAULT false,
    eu_high_risk BOOLEAN DEFAULT false,
    transparency_score DECIMAL(3, 2) CHECK (transparency_score >= 0.0 AND transparency_score <= 1.0),
    corruption_index INTEGER CHECK (corruption_index >= 0 AND corruption_index <= 100),
    aml_compliance_score DECIMAL(3, 2) CHECK (aml_compliance_score >= 0.0 AND aml_compliance_score <= 1.0),
    cfr_rating VARCHAR(50),
    regulatory_strength VARCHAR(50) CHECK (regulatory_strength IN ('STRONG', 'ADEQUATE', 'WEAK', 'VERY_WEAK')),
    last_updated TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    source_references JSONB,
    notes TEXT,
    is_active BOOLEAN DEFAULT true,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_jurisdiction_risk_country_code ON jurisdiction_risk_ratings(country_code);
CREATE INDEX IF NOT EXISTS idx_jurisdiction_risk_tier ON jurisdiction_risk_ratings(risk_tier);
CREATE INDEX IF NOT EXISTS idx_jurisdiction_risk_active ON jurisdiction_risk_ratings(is_active, last_updated);
CREATE INDEX IF NOT EXISTS idx_jurisdiction_risk_score ON jurisdiction_risk_ratings(risk_score DESC);

COMMENT ON TABLE jurisdiction_risk_ratings IS 'Production-grade jurisdiction risk assessment database integrating FATF, OFAC, EU, and other regulatory risk sources';
COMMENT ON COLUMN jurisdiction_risk_ratings.country_code IS 'ISO 3166-1 alpha-2 or alpha-3 country code';
COMMENT ON COLUMN jurisdiction_risk_ratings.risk_tier IS 'Overall risk tier classification: EXTREME, HIGH, MODERATE, LOW, MINIMAL';
COMMENT ON COLUMN jurisdiction_risk_ratings.risk_score IS 'Normalized risk score from 0.0 (lowest risk) to 1.0 (highest risk)';
COMMENT ON COLUMN jurisdiction_risk_ratings.fatf_listing IS 'FATF list status: BLACK (high-risk), GREY (monitoring), WHITE (compliant)';
COMMENT ON COLUMN jurisdiction_risk_ratings.ofac_sanctions IS 'US OFAC sanctions program active';
COMMENT ON COLUMN jurisdiction_risk_ratings.eu_high_risk IS 'EU high-risk third country designation';
COMMENT ON COLUMN jurisdiction_risk_ratings.transparency_score IS 'Transparency International CPI normalized score';
COMMENT ON COLUMN jurisdiction_risk_ratings.aml_compliance_score IS 'AML/CFT compliance effectiveness score';

-- ============================================================================
-- FRAUD DETECTION MANAGEMENT TABLES
-- Production-grade fraud detection system with ML models, alerts, and rules
-- ============================================================================

-- Fraud alerts (if not already in schema, create complete version)
CREATE TABLE IF NOT EXISTS fraud_alerts (
    alert_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    transaction_id UUID,
    rule_id UUID,
    model_id UUID,
    alert_type VARCHAR(50) NOT NULL CHECK (alert_type IN ('rule_violation', 'ml_detection', 'pattern_match', 'anomaly', 'manual_review', 'velocity_check', 'amount_threshold')),
    severity VARCHAR(20) NOT NULL CHECK (severity IN ('low', 'medium', 'high', 'critical')),
    status VARCHAR(20) NOT NULL DEFAULT 'open' CHECK (status IN ('open', 'investigating', 'resolved', 'false_positive', 'confirmed_fraud', 'escalated')),
    risk_score DECIMAL(5, 2) CHECK (risk_score >= 0 AND risk_score <= 100),
    triggered_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    details JSONB,
    indicators JSONB, -- Array of fraud indicators that triggered this alert
    assigned_to VARCHAR(255),
    investigated_at TIMESTAMP,
    investigation_notes TEXT,
    resolved_at TIMESTAMP,
    resolution_action VARCHAR(100),
    resolution_notes TEXT,
    false_positive_reason VARCHAR(255),
    customer_id VARCHAR(255),
    amount DECIMAL(18, 2),
    currency VARCHAR(3),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Fraud detection models (ML models for fraud detection)
CREATE TABLE IF NOT EXISTS fraud_detection_models (
    model_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    model_name VARCHAR(255) NOT NULL UNIQUE,
    model_type VARCHAR(50) NOT NULL CHECK (model_type IN ('random_forest', 'neural_network', 'xgboost', 'isolation_forest', 'svm', 'ensemble', 'logistic_regression', 'gradient_boosting')),
    model_version VARCHAR(50) NOT NULL,
    framework VARCHAR(50) NOT NULL, -- scikit-learn, tensorflow, pytorch, xgboost
    model_binary BYTEA, -- Serialized model (optional, can use file path)
    model_path TEXT, -- Path to model file on disk
    training_dataset_id UUID,
    training_dataset_path TEXT,
    training_date TIMESTAMP,
    training_samples_count INTEGER,
    training_duration_seconds INTEGER,
    accuracy DECIMAL(5, 4) CHECK (accuracy >= 0 AND accuracy <= 1),
    precision_score DECIMAL(5, 4) CHECK (precision_score >= 0 AND precision_score <= 1),
    recall DECIMAL(5, 4) CHECK (recall >= 0 AND recall <= 1),
    f1_score DECIMAL(5, 4) CHECK (f1_score >= 0 AND f1_score <= 1),
    roc_auc DECIMAL(5, 4) CHECK (roc_auc >= 0 AND roc_auc <= 1),
    confusion_matrix JSONB, -- [[TP, FP], [FN, TN]]
    feature_importance JSONB, -- {feature_name: importance_score}
    hyperparameters JSONB, -- Model hyperparameters used
    threshold DECIMAL(3, 2) DEFAULT 0.5 CHECK (threshold >= 0 AND threshold <= 1),
    is_active BOOLEAN DEFAULT false,
    deployment_date TIMESTAMP,
    last_prediction_at TIMESTAMP,
    prediction_count INTEGER DEFAULT 0,
    true_positive_count INTEGER DEFAULT 0,
    false_positive_count INTEGER DEFAULT 0,
    true_negative_count INTEGER DEFAULT 0,
    false_negative_count INTEGER DEFAULT 0,
    description TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_by VARCHAR(255),
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Model performance tracking (historical performance metrics)
CREATE TABLE IF NOT EXISTS model_performance_metrics (
    metric_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    model_id UUID REFERENCES fraud_detection_models(model_id) ON DELETE CASCADE,
    evaluation_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    dataset_type VARCHAR(20) NOT NULL CHECK (dataset_type IN ('train', 'validation', 'test', 'production')),
    dataset_size INTEGER,
    accuracy DECIMAL(5, 4) CHECK (accuracy >= 0 AND accuracy <= 1),
    precision_score DECIMAL(5, 4) CHECK (precision_score >= 0 AND precision_score <= 1),
    recall DECIMAL(5, 4) CHECK (recall >= 0 AND recall <= 1),
    f1_score DECIMAL(5, 4) CHECK (f1_score >= 0 AND f1_score <= 1),
    roc_auc DECIMAL(5, 4) CHECK (roc_auc >= 0 AND roc_auc <= 1),
    confusion_matrix JSONB, -- [[TP, FP], [FN, TN]]
    threshold DECIMAL(3, 2) CHECK (threshold >= 0 AND threshold <= 1),
    sample_size INTEGER,
    false_positive_rate DECIMAL(5, 4) CHECK (false_positive_rate >= 0 AND false_positive_rate <= 1),
    false_negative_rate DECIMAL(5, 4) CHECK (false_negative_rate >= 0 AND false_negative_rate <= 1),
    true_positive_rate DECIMAL(5, 4) CHECK (true_positive_rate >= 0 AND true_positive_rate <= 1),
    true_negative_rate DECIMAL(5, 4) CHECK (true_negative_rate >= 0 AND true_negative_rate <= 1),
    avg_prediction_time_ms INTEGER,
    notes TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Batch fraud scan jobs (for bulk transaction scanning)
CREATE TABLE IF NOT EXISTS fraud_batch_scan_jobs (
    job_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    job_name VARCHAR(255),
    status VARCHAR(20) NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'running', 'completed', 'failed', 'cancelled')),
    scan_type VARCHAR(50) NOT NULL CHECK (scan_type IN ('transaction_range', 'customer_profile', 'pattern_detection', 'anomaly_detection', 'full_rescan', 'scheduled_scan')),
    start_date TIMESTAMP,
    end_date TIMESTAMP,
    transaction_ids JSONB, -- Array of specific transaction IDs to scan
    rule_ids JSONB, -- Array of fraud rule IDs to apply
    model_ids JSONB, -- Array of ML model IDs to use
    customer_ids JSONB, -- Array of customer IDs to scan
    priority INTEGER DEFAULT 5 CHECK (priority >= 1 AND priority <= 10),
    progress DECIMAL(5, 2) DEFAULT 0 CHECK (progress >= 0 AND progress <= 100),
    transactions_scanned INTEGER DEFAULT 0,
    transactions_total INTEGER DEFAULT 0,
    alerts_generated INTEGER DEFAULT 0,
    high_risk_count INTEGER DEFAULT 0,
    medium_risk_count INTEGER DEFAULT 0,
    low_risk_count INTEGER DEFAULT 0,
    processing_rate_per_second DECIMAL(10, 2),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    started_at TIMESTAMP,
    completed_at TIMESTAMP,
    estimated_completion_at TIMESTAMP,
    error_message TEXT,
    error_count INTEGER DEFAULT 0,
    result_summary JSONB,
    created_by VARCHAR(255),
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Fraud reports export (for generating fraud detection reports)
CREATE TABLE IF NOT EXISTS fraud_report_exports (
    export_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    export_type VARCHAR(20) NOT NULL CHECK (export_type IN ('pdf', 'csv', 'json', 'excel', 'html')),
    report_name VARCHAR(255) NOT NULL,
    report_title VARCHAR(500),
    time_range_start TIMESTAMP,
    time_range_end TIMESTAMP,
    include_alerts BOOLEAN DEFAULT true,
    include_rules BOOLEAN DEFAULT true,
    include_stats BOOLEAN DEFAULT true,
    include_transactions BOOLEAN DEFAULT false,
    include_models BOOLEAN DEFAULT false,
    include_charts BOOLEAN DEFAULT true,
    filter_criteria JSONB, -- Additional filters applied
    file_path TEXT,
    file_url TEXT,
    file_size_bytes BIGINT,
    status VARCHAR(20) DEFAULT 'pending' CHECK (status IN ('pending', 'generating', 'completed', 'failed', 'expired')),
    progress DECIMAL(5, 2) DEFAULT 0 CHECK (progress >= 0 AND progress <= 100),
    generated_at TIMESTAMP,
    expires_at TIMESTAMP,
    download_count INTEGER DEFAULT 0,
    last_downloaded_at TIMESTAMP,
    created_by VARCHAR(255),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    error_message TEXT
);

-- Fraud rule test results (for testing rules against historical data)
CREATE TABLE IF NOT EXISTS fraud_rule_test_results (
    test_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    rule_id UUID NOT NULL,
    test_name VARCHAR(255),
    time_range_start TIMESTAMP NOT NULL,
    time_range_end TIMESTAMP NOT NULL,
    transactions_tested INTEGER DEFAULT 0,
    matches_found INTEGER DEFAULT 0,
    true_positives INTEGER DEFAULT 0,
    false_positives INTEGER DEFAULT 0,
    true_negatives INTEGER DEFAULT 0,
    false_negatives INTEGER DEFAULT 0,
    accuracy DECIMAL(5, 4) CHECK (accuracy >= 0 AND accuracy <= 1),
    precision_score DECIMAL(5, 4) CHECK (precision_score >= 0 AND precision_score <= 1),
    recall DECIMAL(5, 4) CHECK (recall >= 0 AND recall <= 1),
    f1_score DECIMAL(5, 4) CHECK (f1_score >= 0 AND f1_score <= 1),
    match_count INTEGER DEFAULT 0,
    false_positive_count INTEGER DEFAULT 0,
    matched_transaction_ids JSONB, -- Array of transaction IDs that matched
    false_positive_transaction_ids JSONB, -- Sample false positives
    test_duration_ms INTEGER,
    tested_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    tested_by VARCHAR(255),
    notes TEXT
);

-- Indexes for fraud detection tables
CREATE INDEX IF NOT EXISTS idx_fraud_alerts_status ON fraud_alerts(status, severity, triggered_at DESC);
CREATE INDEX IF NOT EXISTS idx_fraud_alerts_transaction ON fraud_alerts(transaction_id);
CREATE INDEX IF NOT EXISTS idx_fraud_alerts_rule ON fraud_alerts(rule_id);
CREATE INDEX IF NOT EXISTS idx_fraud_alerts_model ON fraud_alerts(model_id);
CREATE INDEX IF NOT EXISTS idx_fraud_alerts_customer ON fraud_alerts(customer_id, triggered_at DESC);
CREATE INDEX IF NOT EXISTS idx_fraud_alerts_assigned ON fraud_alerts(assigned_to, status);

CREATE INDEX IF NOT EXISTS idx_fraud_models_active ON fraud_detection_models(is_active, accuracy DESC);
CREATE INDEX IF NOT EXISTS idx_fraud_models_type ON fraud_detection_models(model_type, is_active);
CREATE INDEX IF NOT EXISTS idx_fraud_models_name ON fraud_detection_models(model_name);

CREATE INDEX IF NOT EXISTS idx_model_performance_model ON model_performance_metrics(model_id, evaluation_date DESC);
CREATE INDEX IF NOT EXISTS idx_model_performance_dataset ON model_performance_metrics(dataset_type, evaluation_date DESC);

CREATE INDEX IF NOT EXISTS idx_batch_jobs_status ON fraud_batch_scan_jobs(status, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_batch_jobs_priority ON fraud_batch_scan_jobs(priority DESC, created_at);

CREATE INDEX IF NOT EXISTS idx_fraud_exports_status ON fraud_report_exports(status, expires_at);
CREATE INDEX IF NOT EXISTS idx_fraud_exports_created ON fraud_report_exports(created_by, created_at DESC);

CREATE INDEX IF NOT EXISTS idx_rule_test_results_rule ON fraud_rule_test_results(rule_id, tested_at DESC);

-- Comments for fraud detection tables
COMMENT ON TABLE fraud_alerts IS 'Production fraud alerts triggered by rules, ML models, or manual review';
COMMENT ON TABLE fraud_detection_models IS 'ML models for fraud detection with performance tracking';
COMMENT ON TABLE model_performance_metrics IS 'Historical performance metrics for fraud detection models';
COMMENT ON TABLE fraud_batch_scan_jobs IS 'Batch jobs for scanning transactions for fraud';
COMMENT ON TABLE fraud_report_exports IS 'Export jobs for fraud detection reports';
COMMENT ON TABLE fraud_rule_test_results IS 'Test results for fraud rules against historical data';

-- ============================================================================
-- DATA INGESTION MONITORING TABLES
-- Production-grade data pipeline monitoring and quality checks
-- ============================================================================

-- Data ingestion metrics (real-time and historical)
CREATE TABLE IF NOT EXISTS data_ingestion_metrics (
    metric_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    source_id UUID,
    source_name VARCHAR(255) NOT NULL,
    source_type VARCHAR(50) NOT NULL CHECK (source_type IN ('database', 'api', 'file', 'stream', 'queue', 'kafka', 's3', 'ftp')),
    pipeline_name VARCHAR(255),
    ingestion_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    records_ingested INTEGER DEFAULT 0,
    records_failed INTEGER DEFAULT 0,
    records_skipped INTEGER DEFAULT 0,
    records_updated INTEGER DEFAULT 0,
    records_deleted INTEGER DEFAULT 0,
    bytes_processed BIGINT DEFAULT 0,
    duration_ms INTEGER,
    throughput_records_per_sec DECIMAL(12, 2),
    throughput_mb_per_sec DECIMAL(12, 2),
    error_count INTEGER DEFAULT 0,
    error_messages JSONB,
    warning_count INTEGER DEFAULT 0,
    status VARCHAR(20) NOT NULL CHECK (status IN ('success', 'partial', 'failed', 'running', 'queued', 'cancelled')),
    lag_seconds INTEGER,
    last_record_timestamp TIMESTAMP,
    batch_id VARCHAR(255),
    job_id VARCHAR(255),
    execution_host VARCHAR(255),
    memory_used_mb INTEGER,
    cpu_usage_percent DECIMAL(5, 2),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    metadata JSONB
);

-- Data quality check results
CREATE TABLE IF NOT EXISTS data_quality_checks (
    check_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    source_id UUID,
    source_name VARCHAR(255),
    table_name VARCHAR(255) NOT NULL,
    column_name VARCHAR(255),
    check_type VARCHAR(50) NOT NULL CHECK (check_type IN ('completeness', 'accuracy', 'consistency', 'validity', 'uniqueness', 'timeliness', 'schema_compliance', 'referential_integrity', 'range', 'format', 'pattern')),
    check_name VARCHAR(255) NOT NULL,
    check_description TEXT,
    check_rule JSONB NOT NULL,
    executed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    passed BOOLEAN NOT NULL,
    quality_score DECIMAL(5, 2) CHECK (quality_score >= 0 AND quality_score <= 100),
    records_checked INTEGER,
    records_passed INTEGER,
    records_failed INTEGER,
    null_count INTEGER DEFAULT 0,
    failure_rate DECIMAL(5, 4),
    failure_examples JSONB,
    severity VARCHAR(20) NOT NULL CHECK (severity IN ('low', 'medium', 'high', 'critical')),
    threshold_min DECIMAL(18, 4),
    threshold_max DECIMAL(18, 4),
    measured_value DECIMAL(18, 4),
    expected_value DECIMAL(18, 4),
    deviation DECIMAL(18, 4),
    recommendation TEXT,
    remediation_action VARCHAR(255),
    remediation_status VARCHAR(20) CHECK (remediation_status IN ('pending', 'in_progress', 'completed', 'failed', 'skipped')),
    remediated_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    metadata JSONB
);

-- Data quality dimensions summary
CREATE TABLE IF NOT EXISTS data_quality_summary (
    summary_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    source_id UUID,
    source_name VARCHAR(255),
    table_name VARCHAR(255) NOT NULL,
    snapshot_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    completeness_score DECIMAL(5, 2) CHECK (completeness_score >= 0 AND completeness_score <= 100),
    accuracy_score DECIMAL(5, 2) CHECK (accuracy_score >= 0 AND accuracy_score <= 100),
    consistency_score DECIMAL(5, 2) CHECK (consistency_score >= 0 AND consistency_score <= 100),
    validity_score DECIMAL(5, 2) CHECK (validity_score >= 0 AND validity_score <= 100),
    uniqueness_score DECIMAL(5, 2) CHECK (uniqueness_score >= 0 AND uniqueness_score <= 100),
    timeliness_score DECIMAL(5, 2) CHECK (timeliness_score >= 0 AND timeliness_score <= 100),
    overall_quality_score DECIMAL(5, 2) CHECK (overall_quality_score >= 0 AND overall_quality_score <= 100),
    total_records BIGINT,
    null_records INTEGER DEFAULT 0,
    duplicate_records INTEGER DEFAULT 0,
    invalid_records INTEGER DEFAULT 0,
    quality_issues_count INTEGER DEFAULT 0,
    critical_issues_count INTEGER DEFAULT 0,
    high_issues_count INTEGER DEFAULT 0,
    medium_issues_count INTEGER DEFAULT 0,
    low_issues_count INTEGER DEFAULT 0,
    data_freshness_hours DECIMAL(10, 2),
    last_ingestion_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    metadata JSONB
);

-- Indexes for data ingestion tables
CREATE INDEX IF NOT EXISTS idx_ingestion_metrics_source ON data_ingestion_metrics(source_name, ingestion_timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_ingestion_metrics_source_id ON data_ingestion_metrics(source_id, ingestion_timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_ingestion_metrics_status ON data_ingestion_metrics(status, ingestion_timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_ingestion_metrics_timestamp ON data_ingestion_metrics(ingestion_timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_ingestion_metrics_batch ON data_ingestion_metrics(batch_id);

CREATE INDEX IF NOT EXISTS idx_quality_checks_source ON data_quality_checks(source_name, executed_at DESC);
CREATE INDEX IF NOT EXISTS idx_quality_checks_table ON data_quality_checks(table_name, executed_at DESC);
CREATE INDEX IF NOT EXISTS idx_quality_checks_passed ON data_quality_checks(passed, severity, executed_at DESC);
CREATE INDEX IF NOT EXISTS idx_quality_checks_type ON data_quality_checks(check_type, executed_at DESC);

CREATE INDEX IF NOT EXISTS idx_quality_summary_source ON data_quality_summary(source_name, snapshot_date DESC);
CREATE INDEX IF NOT EXISTS idx_quality_summary_table ON data_quality_summary(table_name, snapshot_date DESC);
CREATE INDEX IF NOT EXISTS idx_quality_summary_score ON data_quality_summary(overall_quality_score, snapshot_date DESC);

-- Comments for data ingestion tables
COMMENT ON TABLE data_ingestion_metrics IS 'Real-time and historical data ingestion pipeline metrics';
COMMENT ON TABLE data_quality_checks IS 'Data quality validation check results with remediation tracking';
COMMENT ON TABLE data_quality_summary IS 'Aggregated data quality scores across multiple dimensions';

COMMENT ON COLUMN data_ingestion_metrics.lag_seconds IS 'Data freshness lag - time between source data creation and ingestion';
COMMENT ON COLUMN data_ingestion_metrics.throughput_records_per_sec IS 'Ingestion rate in records per second';
COMMENT ON COLUMN data_quality_checks.check_rule IS 'JSONB definition of the quality rule being validated';
COMMENT ON COLUMN data_quality_checks.failure_examples IS 'Sample records that failed the quality check';
COMMENT ON COLUMN data_quality_summary.overall_quality_score IS 'Weighted average of all quality dimension scores';

-- ============================================================================
-- END OF SPEC.md FEATURE IMPLEMENTATIONS
-- ============================================================================

-- ============================================================================
-- DECISION TREE VISUALIZATION TABLES (GET /decisions/tree, POST /decisions/visualize, POST /decisions)
-- ============================================================================

-- Decisions master table (for MCDA analysis and decision trees)
CREATE TABLE IF NOT EXISTS decisions (
    decision_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    decision_problem TEXT NOT NULL,
    decision_context TEXT,
    decision_method VARCHAR(50) NOT NULL CHECK (decision_method IN ('WEIGHTED_SUM', 'WEIGHTED_PRODUCT', 'TOPSIS', 'ELECTRE', 'PROMETHEE', 'AHP', 'VIKOR')),
    recommended_alternative_id UUID,
    expected_value DECIMAL(18, 6),
    confidence_score DECIMAL(5, 4) CHECK (confidence_score >= 0 AND confidence_score <= 1),
    status VARCHAR(20) NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'analyzing', 'completed', 'failed', 'archived')),
    created_by VARCHAR(255),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    completed_at TIMESTAMP,
    ai_analysis JSONB,
    risk_assessment JSONB,
    sensitivity_analysis JSONB,
    metadata JSONB
);

-- Decision tree nodes for visualization
CREATE TABLE IF NOT EXISTS decision_tree_nodes (
    node_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    decision_id UUID REFERENCES decisions(decision_id) ON DELETE CASCADE,
    parent_node_id UUID REFERENCES decision_tree_nodes(node_id) ON DELETE CASCADE,
    node_type VARCHAR(50) NOT NULL CHECK (node_type IN ('ROOT', 'DECISION', 'CHANCE', 'TERMINAL', 'UTILITY', 'CONDITION', 'ACTION', 'FACTOR', 'EVIDENCE', 'OUTCOME')),
    node_label VARCHAR(255) NOT NULL,
    node_description TEXT,
    node_value JSONB,
    probabilities JSONB, -- For chance nodes: {outcome: probability}
    utility_values JSONB, -- For utility nodes: {criterion: value}
    node_position JSONB, -- {x: number, y: number} for visualization
    level INTEGER DEFAULT 0,
    order_index INTEGER DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    metadata JSONB
);

-- Decision criteria weights (for MCDA analysis)
CREATE TABLE IF NOT EXISTS decision_criteria (
    criterion_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    decision_id UUID REFERENCES decisions(decision_id) ON DELETE CASCADE,
    criterion_name VARCHAR(255) NOT NULL,
    criterion_type VARCHAR(50) NOT NULL CHECK (criterion_type IN ('FINANCIAL_IMPACT', 'REGULATORY_COMPLIANCE', 'RISK_LEVEL', 'OPERATIONAL_IMPACT', 'STRATEGIC_ALIGNMENT', 'ETHICAL_CONSIDERATIONS', 'LEGAL_RISK', 'REPUTATIONAL_IMPACT', 'TIME_TO_IMPLEMENT', 'RESOURCE_REQUIREMENTS', 'STAKEHOLDER_IMPACT', 'MARKET_POSITION')),
    weight DECIMAL(5, 4) NOT NULL CHECK (weight >= 0 AND weight <= 1),
    benefit_criterion BOOLEAN DEFAULT TRUE, -- TRUE = maximize, FALSE = minimize
    description TEXT,
    threshold_min DECIMAL(18, 6),
    threshold_max DECIMAL(18, 6),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    metadata JSONB
);

-- Decision alternatives (options being evaluated)
CREATE TABLE IF NOT EXISTS decision_alternatives (
    alternative_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    decision_id UUID REFERENCES decisions(decision_id) ON DELETE CASCADE,
    alternative_name VARCHAR(255) NOT NULL,
    alternative_description TEXT,
    scores JSONB NOT NULL, -- {criterion_id: score}
    total_score DECIMAL(18, 6),
    normalized_score DECIMAL(18, 6),
    ranking INTEGER,
    selected BOOLEAN DEFAULT false,
    advantages TEXT[],
    disadvantages TEXT[],
    risks TEXT[],
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    metadata JSONB
);

-- Indexes for decision tree tables
CREATE INDEX IF NOT EXISTS idx_decisions_status ON decisions(status, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_decisions_method ON decisions(decision_method, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_decisions_created_by ON decisions(created_by, created_at DESC);

CREATE INDEX IF NOT EXISTS idx_decision_tree_nodes_decision ON decision_tree_nodes(decision_id, level);
CREATE INDEX IF NOT EXISTS idx_decision_tree_nodes_parent ON decision_tree_nodes(parent_node_id);
CREATE INDEX IF NOT EXISTS idx_decision_tree_nodes_type ON decision_tree_nodes(decision_id, node_type);

CREATE INDEX IF NOT EXISTS idx_decision_criteria_decision ON decision_criteria(decision_id);
CREATE INDEX IF NOT EXISTS idx_decision_criteria_type ON decision_criteria(decision_id, criterion_type);

CREATE INDEX IF NOT EXISTS idx_decision_alternatives_decision ON decision_alternatives(decision_id);
CREATE INDEX IF NOT EXISTS idx_decision_alternatives_ranking ON decision_alternatives(decision_id, ranking);
CREATE INDEX IF NOT EXISTS idx_decision_alternatives_score ON decision_alternatives(decision_id, total_score DESC);

-- Comments for decision tree tables
COMMENT ON TABLE decisions IS 'Master table for MCDA decision analyses and decision tree evaluations';
COMMENT ON TABLE decision_tree_nodes IS 'Hierarchical decision tree structure for visualization';
COMMENT ON TABLE decision_criteria IS 'Multi-criteria decision analysis evaluation criteria with weights';
COMMENT ON TABLE decision_alternatives IS 'Decision alternatives/options being evaluated with scores';

COMMENT ON COLUMN decisions.decision_method IS 'MCDA algorithm used: WEIGHTED_SUM, TOPSIS, ELECTRE, PROMETHEE, AHP, VIKOR';
COMMENT ON COLUMN decisions.expected_value IS 'Expected utility value calculated from decision tree';
COMMENT ON COLUMN decision_tree_nodes.node_type IS 'Type: ROOT, DECISION (choice), CHANCE (probability), TERMINAL (outcome), UTILITY, etc.';
COMMENT ON COLUMN decision_tree_nodes.probabilities IS 'JSON map of {outcome: probability} for chance nodes';
COMMENT ON COLUMN decision_criteria.benefit_criterion IS 'TRUE = maximize (benefit), FALSE = minimize (cost)';
COMMENT ON COLUMN decision_alternatives.scores IS 'JSON map of {criterion_id: score_value} for each criterion';

-- ============================================================================
-- END OF DECISION TREE VISUALIZATION TABLES
-- ============================================================================

-- ============================================================================
-- TRANSACTION ANALYSIS & PATTERN DETECTION TABLES (POST /transactions/{id}/analyze, GET /transactions/patterns, etc.)
-- ============================================================================

-- Transaction fraud analysis results (detailed fraud analysis for single transaction)
CREATE TABLE IF NOT EXISTS transaction_fraud_analysis (
    analysis_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    transaction_id UUID NOT NULL,
    analyzed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    risk_score DECIMAL(5, 2) NOT NULL CHECK (risk_score >= 0 AND risk_score <= 100),
    risk_level VARCHAR(20) NOT NULL CHECK (risk_level IN ('low', 'medium', 'high', 'critical')),
    fraud_indicators JSONB, -- Array of indicator objects: [{indicator: string, severity: string, description: string}]
    ml_model_used VARCHAR(100),
    confidence DECIMAL(3, 2) CHECK (confidence >= 0 AND confidence <= 1),
    recommendation TEXT,
    analyzed_by VARCHAR(100), -- agent_id or user_id
    velocity_check_passed BOOLEAN,
    amount_check_passed BOOLEAN,
    location_check_passed BOOLEAN,
    device_check_passed BOOLEAN,
    behavioral_check_passed BOOLEAN,
    analysis_details JSONB, -- Detailed breakdown of analysis
    FOREIGN KEY (transaction_id) REFERENCES transactions(transaction_id) ON DELETE CASCADE
);

-- Transaction patterns (detected behavioral patterns across transactions)
CREATE TABLE IF NOT EXISTS transaction_patterns (
    pattern_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    pattern_name VARCHAR(255) NOT NULL,
    pattern_type VARCHAR(50) NOT NULL CHECK (pattern_type IN ('velocity', 'amount', 'geographic', 'temporal', 'network', 'sequence', 'behavioral', 'merchant', 'channel')),
    pattern_description TEXT,
    detection_algorithm VARCHAR(100), -- Algorithm used: statistical, ml_clustering, rule_based, time_series
    pattern_definition JSONB NOT NULL, -- Pattern parameters and rules
    frequency INTEGER DEFAULT 0, -- How often pattern has been detected
    risk_association VARCHAR(20) CHECK (risk_association IN ('low', 'medium', 'high', 'critical')),
    severity_score DECIMAL(5, 2) CHECK (severity_score >= 0 AND severity_score <= 100),
    customer_segments JSONB, -- Which customer segments exhibit this pattern
    first_detected TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_detected TIMESTAMP,
    is_active BOOLEAN DEFAULT true,
    is_anomalous BOOLEAN DEFAULT false,
    statistical_significance DECIMAL(5, 4), -- P-value or confidence level
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_by VARCHAR(100),
    metadata JSONB
);

-- Transaction pattern occurrences (instances of patterns in specific transactions)
CREATE TABLE IF NOT EXISTS transaction_pattern_occurrences (
    occurrence_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    pattern_id UUID REFERENCES transaction_patterns(pattern_id) ON DELETE CASCADE,
    transaction_id UUID,
    customer_id VARCHAR(255),
    detected_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    confidence DECIMAL(3, 2) CHECK (confidence >= 0 AND confidence <= 1),
    context JSONB, -- Additional context about this occurrence
    impact_score DECIMAL(5, 2), -- Impact/risk score for this specific occurrence
    triggered_alert BOOLEAN DEFAULT false,
    alert_id UUID,
    FOREIGN KEY (transaction_id) REFERENCES transactions(transaction_id) ON DELETE SET NULL
);

-- Anomaly detection results (statistical anomalies in transactions)
CREATE TABLE IF NOT EXISTS transaction_anomalies (
    anomaly_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    transaction_id UUID,
    anomaly_type VARCHAR(50) NOT NULL CHECK (anomaly_type IN ('statistical', 'behavioral', 'temporal', 'geographic', 'network', 'value', 'frequency', 'sequence')),
    anomaly_score DECIMAL(5, 2) NOT NULL CHECK (anomaly_score >= 0 AND anomaly_score <= 100),
    severity VARCHAR(20) CHECK (severity IN ('low', 'medium', 'high', 'critical')),
    description TEXT,
    baseline_value DECIMAL(18, 2),
    observed_value DECIMAL(18, 2),
    deviation_percent DECIMAL(8, 2),
    detection_method VARCHAR(100), -- Algorithm used: isolation_forest, z_score, iqr, clustering
    model_id VARCHAR(100), -- ML model identifier if ML-based
    feature_name VARCHAR(100), -- Which feature/attribute showed anomaly
    detected_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    resolved BOOLEAN DEFAULT false,
    resolution_notes TEXT,
    resolved_at TIMESTAMP,
    resolved_by VARCHAR(100),
    false_positive BOOLEAN,
    FOREIGN KEY (transaction_id) REFERENCES transactions(transaction_id) ON DELETE CASCADE
);

-- Transaction metrics aggregation (for GET /transactions/metrics endpoint)
CREATE TABLE IF NOT EXISTS transaction_metrics_snapshot (
    snapshot_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    snapshot_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    time_period VARCHAR(20) NOT NULL CHECK (time_period IN ('hourly', 'daily', 'weekly', 'monthly')),
    total_transactions BIGINT DEFAULT 0,
    total_volume DECIMAL(20, 2) DEFAULT 0,
    avg_transaction_amount DECIMAL(20, 2),
    median_transaction_amount DECIMAL(20, 2),
    max_transaction_amount DECIMAL(20, 2),
    min_transaction_amount DECIMAL(20, 2),
    flagged_transactions INTEGER DEFAULT 0,
    high_risk_transactions INTEGER DEFAULT 0,
    anomalies_detected INTEGER DEFAULT 0,
    patterns_detected INTEGER DEFAULT 0,
    unique_customers INTEGER DEFAULT 0,
    unique_merchants INTEGER DEFAULT 0,
    cross_border_transactions INTEGER DEFAULT 0,
    currency_distribution JSONB, -- {currency: count}
    channel_distribution JSONB, -- {channel: count}
    country_distribution JSONB, -- {country: count}
    fraud_detection_rate DECIMAL(5, 4), -- % of transactions flagged
    false_positive_rate DECIMAL(5, 4), -- Estimated false positive rate
    processing_time_avg_ms INTEGER,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    metadata JSONB
);

-- Indexes for transaction analysis tables
CREATE INDEX IF NOT EXISTS idx_fraud_analysis_transaction ON transaction_fraud_analysis(transaction_id);
CREATE INDEX IF NOT EXISTS idx_fraud_analysis_risk ON transaction_fraud_analysis(risk_level, analyzed_at DESC);
CREATE INDEX IF NOT EXISTS idx_fraud_analysis_analyzed_at ON transaction_fraud_analysis(analyzed_at DESC);
CREATE INDEX IF NOT EXISTS idx_fraud_analysis_risk_score ON transaction_fraud_analysis(risk_score DESC);

CREATE INDEX IF NOT EXISTS idx_transaction_patterns_type ON transaction_patterns(pattern_type, is_active);
CREATE INDEX IF NOT EXISTS idx_transaction_patterns_risk ON transaction_patterns(risk_association, severity_score DESC);
CREATE INDEX IF NOT EXISTS idx_transaction_patterns_detected ON transaction_patterns(last_detected DESC);
CREATE INDEX IF NOT EXISTS idx_transaction_patterns_active ON transaction_patterns(is_active, is_anomalous);

CREATE INDEX IF NOT EXISTS idx_pattern_occurrences_pattern ON transaction_pattern_occurrences(pattern_id);
CREATE INDEX IF NOT EXISTS idx_pattern_occurrences_transaction ON transaction_pattern_occurrences(transaction_id);
CREATE INDEX IF NOT EXISTS idx_pattern_occurrences_customer ON transaction_pattern_occurrences(customer_id, detected_at DESC);
CREATE INDEX IF NOT EXISTS idx_pattern_occurrences_detected ON transaction_pattern_occurrences(detected_at DESC);

CREATE INDEX IF NOT EXISTS idx_anomalies_transaction ON transaction_anomalies(transaction_id);
CREATE INDEX IF NOT EXISTS idx_anomalies_severity ON transaction_anomalies(severity, detected_at DESC);
CREATE INDEX IF NOT EXISTS idx_anomalies_type ON transaction_anomalies(anomaly_type, severity);
CREATE INDEX IF NOT EXISTS idx_anomalies_resolved ON transaction_anomalies(resolved, detected_at DESC);
CREATE INDEX IF NOT EXISTS idx_anomalies_score ON transaction_anomalies(anomaly_score DESC);

CREATE INDEX IF NOT EXISTS idx_metrics_snapshot_period ON transaction_metrics_snapshot(time_period, snapshot_timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_metrics_snapshot_timestamp ON transaction_metrics_snapshot(snapshot_timestamp DESC);

-- Comments for transaction analysis tables
COMMENT ON TABLE transaction_fraud_analysis IS 'Detailed fraud analysis results for individual transactions';
COMMENT ON TABLE transaction_patterns IS 'Detected behavioral and statistical patterns across transactions';
COMMENT ON TABLE transaction_pattern_occurrences IS 'Instances where patterns were detected in specific transactions';
COMMENT ON TABLE transaction_anomalies IS 'Statistical and behavioral anomalies detected in transactions';
COMMENT ON TABLE transaction_metrics_snapshot IS 'Aggregated transaction metrics for monitoring and reporting';

COMMENT ON COLUMN transaction_fraud_analysis.fraud_indicators IS 'Array of fraud indicators: [{indicator, severity, description}]';
COMMENT ON COLUMN transaction_patterns.pattern_definition IS 'JSON definition of pattern rules and parameters';
COMMENT ON COLUMN transaction_patterns.detection_algorithm IS 'Algorithm: statistical, ml_clustering, rule_based, time_series';
COMMENT ON COLUMN transaction_anomalies.detection_method IS 'Method: isolation_forest, z_score, iqr, clustering, etc.';
COMMENT ON COLUMN transaction_metrics_snapshot.time_period IS 'Aggregation period: hourly, daily, weekly, monthly';

-- ============================================================================
-- END OF TRANSACTION ANALYSIS & PATTERN DETECTION TABLES
-- ============================================================================

-- ============================================================================
-- PATTERN ANALYSIS TABLES (GET /patterns, POST /patterns/detect, etc.)
-- Production-grade pattern detection for compliance, regulatory, and behavioral analysis
-- ============================================================================

-- Detected patterns (compliance/regulatory/transaction/behavioral patterns)
CREATE TABLE IF NOT EXISTS detected_patterns (
    pattern_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    pattern_name VARCHAR(255) NOT NULL,
    pattern_type VARCHAR(50) NOT NULL CHECK (pattern_type IN ('compliance', 'regulatory', 'transaction', 'behavioral', 'temporal', 'network', 'anomaly', 'fraud', 'operational')),
    pattern_category VARCHAR(100),
    detection_algorithm VARCHAR(100), -- clustering, sequential, association, neural, ml_based, rule_based
    pattern_definition JSONB NOT NULL, -- Pattern structure and rules
    support DECIMAL(5, 4) CHECK (support >= 0 AND support <= 1), -- Frequency of pattern
    confidence DECIMAL(5, 4) CHECK (confidence >= 0 AND confidence <= 1), -- Reliability score
    lift DECIMAL(8, 4), -- Association strength (how much more likely than random)
    occurrence_count INTEGER DEFAULT 0,
    first_detected TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_detected TIMESTAMP,
    data_source VARCHAR(255), -- Source table/system
    sample_instances JSONB, -- Example instances of the pattern
    is_significant BOOLEAN DEFAULT false,
    risk_association VARCHAR(20) CHECK (risk_association IN ('low', 'medium', 'high', 'critical')),
    severity_level VARCHAR(20) CHECK (severity_level IN ('informational', 'low', 'medium', 'high', 'critical')),
    description TEXT,
    recommendation TEXT, -- Action recommendation
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_by VARCHAR(100),
    metadata JSONB
);

-- Pattern detection jobs (async processing for long-running pattern detection)
CREATE TABLE IF NOT EXISTS pattern_detection_jobs (
    job_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    job_name VARCHAR(255),
    status VARCHAR(20) NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'running', 'completed', 'failed', 'cancelled')),
    data_source VARCHAR(255) NOT NULL, -- transactions, customers, regulations, etc.
    algorithm VARCHAR(50) CHECK (algorithm IN ('clustering', 'sequential', 'association', 'neural', 'auto', 'apriori', 'fp_growth')),
    time_range_start TIMESTAMP,
    time_range_end TIMESTAMP,
    parameters JSONB, -- minSupport, minConfidence, maxPatterns, etc.
    progress DECIMAL(5, 2) DEFAULT 0 CHECK (progress >= 0 AND progress <= 100),
    records_analyzed BIGINT DEFAULT 0,
    patterns_found INTEGER DEFAULT 0,
    significant_patterns INTEGER DEFAULT 0,
    validation_passed INTEGER DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    started_at TIMESTAMP,
    completed_at TIMESTAMP,
    error_message TEXT,
    result_summary JSONB,
    created_by VARCHAR(255)
);

-- Pattern predictions (future occurrence likelihood)
CREATE TABLE IF NOT EXISTS pattern_predictions (
    prediction_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    pattern_id UUID REFERENCES detected_patterns(pattern_id) ON DELETE CASCADE,
    prediction_timestamp TIMESTAMP NOT NULL,
    predicted_value DECIMAL(18, 4),
    probability DECIMAL(5, 4) CHECK (probability >= 0 AND probability <= 1),
    confidence_interval_lower DECIMAL(18, 4),
    confidence_interval_upper DECIMAL(18, 4),
    prediction_horizon VARCHAR(20), -- 1h, 1d, 1w, 1m, 1q, 1y
    model_used VARCHAR(100), -- arima, lstm, prophet, etc.
    actual_value DECIMAL(18, 4), -- Filled in after event occurs
    prediction_error DECIMAL(18, 4),
    prediction_accuracy DECIMAL(5, 4),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    metadata JSONB
);

-- Pattern correlations (relationships between patterns)
CREATE TABLE IF NOT EXISTS pattern_correlations (
    correlation_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    pattern_a_id UUID REFERENCES detected_patterns(pattern_id) ON DELETE CASCADE,
    pattern_b_id UUID REFERENCES detected_patterns(pattern_id) ON DELETE CASCADE,
    correlation_coefficient DECIMAL(5, 4) CHECK (correlation_coefficient >= -1 AND correlation_coefficient <= 1),
    correlation_type VARCHAR(50) CHECK (correlation_type IN ('positive', 'negative', 'causal', 'coincidental', 'sequential', 'inverse')),
    statistical_significance DECIMAL(5, 4), -- P-value
    lag_seconds INTEGER, -- Time lag between patterns
    co_occurrence_count INTEGER DEFAULT 0,
    description TEXT,
    discovered_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    discovered_by VARCHAR(100),
    UNIQUE(pattern_a_id, pattern_b_id)
);

-- Pattern timeline (historical occurrences for visualization)
CREATE TABLE IF NOT EXISTS pattern_timeline (
    timeline_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    pattern_id UUID REFERENCES detected_patterns(pattern_id) ON DELETE CASCADE,
    occurred_at TIMESTAMP NOT NULL,
    occurrence_value DECIMAL(18, 4),
    occurrence_context JSONB, -- Additional context about this occurrence
    entity_id VARCHAR(255), -- Transaction ID, customer ID, regulation ID, etc.
    entity_type VARCHAR(50), -- transaction, customer, regulation, agent, etc.
    strength DECIMAL(5, 4) CHECK (strength >= 0 AND strength <= 1), -- How strongly pattern matched
    impact_score DECIMAL(5, 2), -- Impact of this occurrence
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Pattern anomalies (deviations from expected patterns)
CREATE TABLE IF NOT EXISTS pattern_anomalies (
    anomaly_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    pattern_id UUID REFERENCES detected_patterns(pattern_id) ON DELETE CASCADE,
    anomaly_type VARCHAR(50) NOT NULL CHECK (anomaly_type IN ('frequency_deviation', 'value_deviation', 'timing_deviation', 'missing_occurrence', 'unexpected_occurrence', 'intensity_change')),
    detected_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    severity VARCHAR(20) CHECK (severity IN ('low', 'medium', 'high', 'critical')),
    expected_value DECIMAL(18, 4),
    observed_value DECIMAL(18, 4),
    deviation_percent DECIMAL(8, 2),
    z_score DECIMAL(8, 4), -- Statistical z-score
    details JSONB,
    impact_assessment TEXT,
    investigated BOOLEAN DEFAULT false,
    investigation_notes TEXT,
    resolved_at TIMESTAMP,
    resolved_by VARCHAR(100),
    false_positive BOOLEAN
);

-- Pattern export reports (for generating pattern analysis reports)
CREATE TABLE IF NOT EXISTS pattern_export_reports (
    export_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    export_format VARCHAR(20) CHECK (export_format IN ('pdf', 'csv', 'json', 'excel')),
    pattern_ids JSONB, -- Array of pattern IDs to include
    include_visualization BOOLEAN DEFAULT true,
    include_stats BOOLEAN DEFAULT true,
    include_predictions BOOLEAN DEFAULT false,
    include_timeline BOOLEAN DEFAULT false,
    include_correlations BOOLEAN DEFAULT false,
    file_path TEXT,
    file_url TEXT,
    file_size_bytes BIGINT,
    status VARCHAR(20) DEFAULT 'pending' CHECK (status IN ('pending', 'generating', 'completed', 'failed', 'expired')),
    generated_at TIMESTAMP,
    expires_at TIMESTAMP,
    download_count INTEGER DEFAULT 0,
    created_by VARCHAR(255),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Pattern validation results (for POST /patterns/{id}/validate)
CREATE TABLE IF NOT EXISTS pattern_validation_results (
    validation_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    pattern_id UUID REFERENCES detected_patterns(pattern_id) ON DELETE CASCADE,
    validated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    validation_method VARCHAR(50) CHECK (validation_method IN ('cross_validation', 'holdout', 'temporal', 'manual')),
    validation_passed BOOLEAN NOT NULL,
    accuracy DECIMAL(5, 4) CHECK (accuracy >= 0 AND accuracy <= 1),
    precision_score DECIMAL(5, 4),
    recall_score DECIMAL(5, 4),
    f1_score DECIMAL(5, 4),
    true_positives INTEGER DEFAULT 0,
    false_positives INTEGER DEFAULT 0,
    true_negatives INTEGER DEFAULT 0,
    false_negatives INTEGER DEFAULT 0,
    validation_notes TEXT,
    validated_by VARCHAR(100)
);

-- Indexes for pattern analysis tables
CREATE INDEX IF NOT EXISTS idx_patterns_type ON detected_patterns(pattern_type, confidence DESC);
CREATE INDEX IF NOT EXISTS idx_patterns_detected ON detected_patterns(last_detected DESC);
CREATE INDEX IF NOT EXISTS idx_patterns_significant ON detected_patterns(is_significant, occurrence_count DESC);
CREATE INDEX IF NOT EXISTS idx_patterns_risk ON detected_patterns(risk_association, severity_level);
CREATE INDEX IF NOT EXISTS idx_patterns_category ON detected_patterns(pattern_category, pattern_type);

CREATE INDEX IF NOT EXISTS idx_pattern_jobs_status ON pattern_detection_jobs(status, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_pattern_jobs_source ON pattern_detection_jobs(data_source, status);

CREATE INDEX IF NOT EXISTS idx_pattern_predictions_pattern ON pattern_predictions(pattern_id, prediction_timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_pattern_predictions_timestamp ON pattern_predictions(prediction_timestamp DESC);

CREATE INDEX IF NOT EXISTS idx_pattern_correlations_pattern_a ON pattern_correlations(pattern_a_id);
CREATE INDEX IF NOT EXISTS idx_pattern_correlations_pattern_b ON pattern_correlations(pattern_b_id);
CREATE INDEX IF NOT EXISTS idx_pattern_correlations_type ON pattern_correlations(correlation_type, correlation_coefficient);

CREATE INDEX IF NOT EXISTS idx_pattern_timeline_pattern ON pattern_timeline(pattern_id, occurred_at DESC);
CREATE INDEX IF NOT EXISTS idx_pattern_timeline_entity ON pattern_timeline(entity_id, entity_type, occurred_at DESC);

CREATE INDEX IF NOT EXISTS idx_pattern_anomalies_pattern ON pattern_anomalies(pattern_id, detected_at DESC);
CREATE INDEX IF NOT EXISTS idx_pattern_anomalies_severity ON pattern_anomalies(severity, detected_at DESC);
CREATE INDEX IF NOT EXISTS idx_pattern_anomalies_investigated ON pattern_anomalies(investigated, resolved_at);

CREATE INDEX IF NOT EXISTS idx_pattern_exports_status ON pattern_export_reports(status, created_at DESC);

CREATE INDEX IF NOT EXISTS idx_pattern_validation_pattern ON pattern_validation_results(pattern_id, validated_at DESC);

-- Comments for pattern analysis tables
COMMENT ON TABLE detected_patterns IS 'Master table for detected patterns across all domains (compliance, fraud, behavioral, etc.)';
COMMENT ON TABLE pattern_detection_jobs IS 'Async pattern detection jobs with progress tracking';
COMMENT ON TABLE pattern_predictions IS 'Predictions for future pattern occurrences using time series models';
COMMENT ON TABLE pattern_correlations IS 'Relationships and correlations between different patterns';
COMMENT ON TABLE pattern_timeline IS 'Historical timeline of pattern occurrences for visualization';
COMMENT ON TABLE pattern_anomalies IS 'Anomalies and deviations from expected pattern behavior';
COMMENT ON TABLE pattern_export_reports IS 'Pattern analysis report exports in various formats';
COMMENT ON TABLE pattern_validation_results IS 'Pattern validation results and accuracy metrics';

COMMENT ON COLUMN detected_patterns.support IS 'Frequency: percentage of data that contains the pattern';
COMMENT ON COLUMN detected_patterns.confidence IS 'Reliability: probability that the pattern holds true';
COMMENT ON COLUMN detected_patterns.lift IS 'How much more likely the pattern is than random occurrence';
COMMENT ON COLUMN pattern_correlations.correlation_coefficient IS 'Pearson correlation: -1 (inverse) to +1 (direct)';
COMMENT ON COLUMN pattern_timeline.strength IS 'Pattern match strength: 0 (weak) to 1 (strong)';
COMMENT ON COLUMN pattern_anomalies.z_score IS 'Statistical z-score showing deviation from normal';

-- ============================================================================
-- END OF PATTERN ANALYSIS TABLES
-- ============================================================================

-- ============================================================================
-- AGENT COMMUNICATION SYSTEM
-- ============================================================================

-- Agent messages table for inter-agent communication
CREATE TABLE IF NOT EXISTS agent_messages (
    message_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    from_agent_id UUID NOT NULL,
    to_agent_id UUID,  -- NULL for broadcasts
    message_type VARCHAR(50) NOT NULL,
    content JSONB NOT NULL,
    priority INTEGER DEFAULT 3 CHECK (priority BETWEEN 1 AND 5), -- 1=urgent, 5=low
    status VARCHAR(20) DEFAULT 'pending' CHECK (status IN ('pending', 'delivered', 'acknowledged', 'failed', 'expired')),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    delivered_at TIMESTAMP WITH TIME ZONE,
    acknowledged_at TIMESTAMP WITH TIME ZONE,
    read_at TIMESTAMP WITH TIME ZONE,
    retry_count INTEGER DEFAULT 0,
    max_retries INTEGER DEFAULT 3,
    expires_at TIMESTAMP WITH TIME ZONE,
    error_message TEXT,
    correlation_id UUID, -- For request/response correlation
    parent_message_id UUID REFERENCES agent_messages(message_id), -- For threading
    created_by UUID, -- User who initiated the message (if applicable)
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Indexes for efficient message retrieval
CREATE INDEX IF NOT EXISTS idx_agent_messages_recipient ON agent_messages(to_agent_id, status) WHERE to_agent_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_agent_messages_sender ON agent_messages(from_agent_id, status);
CREATE INDEX IF NOT EXISTS idx_agent_messages_created ON agent_messages(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_agent_messages_type ON agent_messages(message_type);
CREATE INDEX IF NOT EXISTS idx_agent_messages_priority ON agent_messages(priority, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_agent_messages_correlation ON agent_messages(correlation_id) WHERE correlation_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_agent_messages_parent ON agent_messages(parent_message_id) WHERE parent_message_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_agent_messages_expires ON agent_messages(expires_at) WHERE expires_at IS NOT NULL;

-- Dead letter queue for failed messages
CREATE TABLE IF NOT EXISTS agent_messages_failed (
    message_id UUID PRIMARY KEY,
    original_message JSONB NOT NULL,
    failure_reason TEXT,
    failed_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    retry_after TIMESTAMP WITH TIME ZONE
);

-- Agent conversation threads (for multi-agent discussions)
CREATE TABLE IF NOT EXISTS agent_conversations (
    conversation_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    topic VARCHAR(500) NOT NULL,
    participant_agents UUID[] NOT NULL, -- Array of agent IDs
    status VARCHAR(20) DEFAULT 'active' CHECK (status IN ('active', 'completed', 'archived')),
    priority VARCHAR(20) DEFAULT 'normal' CHECK (priority IN ('low', 'normal', 'high', 'urgent')),
    started_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    last_activity TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    message_count INTEGER DEFAULT 0,
    created_by UUID,
    metadata JSONB, -- Additional conversation metadata
    expires_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Link messages to conversations
ALTER TABLE agent_messages ADD COLUMN IF NOT EXISTS conversation_id UUID REFERENCES agent_conversations(conversation_id);

-- Indexes for conversations
CREATE INDEX IF NOT EXISTS idx_agent_conversations_participants ON agent_conversations USING GIN(participant_agents);
CREATE INDEX IF NOT EXISTS idx_agent_conversations_status ON agent_conversations(status, last_activity DESC);
CREATE INDEX IF NOT EXISTS idx_agent_conversations_topic ON agent_conversations(topic);

-- Message delivery attempts (for failed deliveries)
CREATE TABLE IF NOT EXISTS message_delivery_attempts (
    attempt_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    message_id UUID NOT NULL REFERENCES agent_messages(message_id) ON DELETE CASCADE,
    attempt_number INTEGER NOT NULL,
    attempted_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    error_code VARCHAR(100),
    error_message TEXT,
    next_retry_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Indexes for delivery attempts
CREATE INDEX IF NOT EXISTS idx_delivery_attempts_message ON message_delivery_attempts(message_id, attempt_number);
CREATE INDEX IF NOT EXISTS idx_delivery_attempts_retry ON message_delivery_attempts(next_retry_at) WHERE next_retry_at IS NOT NULL;

-- Message templates for common agent communications
CREATE TABLE IF NOT EXISTS message_templates (
    template_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    template_name VARCHAR(100) NOT NULL UNIQUE,
    message_type VARCHAR(50) NOT NULL,
    template_content JSONB NOT NULL,
    description TEXT,
    is_active BOOLEAN DEFAULT true,
    created_by UUID,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Message type definitions
CREATE TABLE IF NOT EXISTS message_types (
    message_type VARCHAR(50) PRIMARY KEY,
    description TEXT NOT NULL,
    schema_definition JSONB, -- JSON schema for message validation
    requires_response BOOLEAN DEFAULT false,
    default_priority INTEGER DEFAULT 3,
    default_expiry_hours INTEGER DEFAULT 24,
    is_system_type BOOLEAN DEFAULT false,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Insert standard message types
INSERT INTO message_types (message_type, description, requires_response, default_priority, default_expiry_hours, is_system_type) VALUES
('TASK_ASSIGNMENT', 'Assign a task to another agent', true, 2, 168, false),
('STATUS_UPDATE', 'Report agent status or progress', false, 4, 24, false),
('DATA_REQUEST', 'Request data from another agent', true, 2, 72, false),
('DATA_RESPONSE', 'Respond with requested data', false, 3, 168, false),
('COLLABORATION_INVITE', 'Invite to collaborative session', true, 3, 24, false),
('CONSENSUS_REQUEST', 'Request consensus input', true, 2, 72, false),
('CONSENSUS_VOTE', 'Submit consensus vote', false, 3, 168, false),
('ALERT', 'Urgent notification or alert', false, 1, 24, true),
('HEARTBEAT', 'Agent health check', false, 5, 1, true),
('ERROR_REPORT', 'Report an error or failure', false, 1, 168, true),
('CONFIGURATION_UPDATE', 'Notify of configuration changes', false, 2, 168, true),
('RESOURCE_REQUEST', 'Request system resources', true, 3, 24, false)
ON CONFLICT (message_type) DO NOTHING;

-- Comments for agent communication tables
COMMENT ON TABLE agent_messages IS 'Inter-agent communication messages with delivery tracking and retry logic';
COMMENT ON TABLE agent_conversations IS 'Multi-agent conversation threads for collaborative discussions';
COMMENT ON TABLE message_delivery_attempts IS 'Audit trail of message delivery attempts and failures';
COMMENT ON TABLE message_templates IS 'Reusable message templates for common communications';
COMMENT ON TABLE message_types IS 'Definitions of supported message types with validation schemas';

COMMENT ON COLUMN agent_messages.priority IS '1=urgent, 2=high, 3=normal, 4=low, 5=bulk';
COMMENT ON COLUMN agent_messages.correlation_id IS 'Links request/response message pairs';
COMMENT ON COLUMN agent_messages.parent_message_id IS 'For message threading in conversations';
COMMENT ON COLUMN agent_conversations.participant_agents IS 'Array of agent IDs participating in conversation';

-- ============================================================================
-- END OF AGENT COMMUNICATION SYSTEM
-- ============================================================================

-- ============================================================================
-- GPT-4 CHATBOT INTEGRATION SYSTEM
-- ============================================================================

-- Chatbot conversations table
CREATE TABLE IF NOT EXISTS chatbot_conversations (
    conversation_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID NOT NULL,
    platform VARCHAR(50) DEFAULT 'web', -- web, api, slack, discord, etc.
    title VARCHAR(255), -- Auto-generated or user-provided
    message_count INTEGER DEFAULT 0,
    token_count INTEGER DEFAULT 0,
    total_cost DECIMAL(10,6) DEFAULT 0, -- Cost in USD
    started_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    last_message_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    is_active BOOLEAN DEFAULT true,
    metadata JSONB, -- Conversation metadata (model used, settings, etc.)
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Chatbot messages table
CREATE TABLE IF NOT EXISTS chatbot_messages (
    message_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    conversation_id UUID NOT NULL REFERENCES chatbot_conversations(conversation_id) ON DELETE CASCADE,
    role VARCHAR(20) NOT NULL CHECK (role IN ('user', 'assistant', 'system')),
    content TEXT NOT NULL,
    token_count INTEGER,
    model_used VARCHAR(100), -- gpt-4, gpt-3.5-turbo, etc.
    message_cost DECIMAL(8,6), -- Cost for this specific message
    confidence_score DECIMAL(3,2), -- AI confidence in response (0-1)
    sources_used JSONB, -- Knowledge base sources referenced
    processing_time_ms INTEGER, -- How long it took to generate response
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Chatbot knowledge retrieval cache (for RAG)
CREATE TABLE IF NOT EXISTS chatbot_knowledge_cache (
    cache_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    query_hash VARCHAR(64) NOT NULL UNIQUE, -- SHA256 hash of query
    query_text TEXT NOT NULL,
    retrieved_context JSONB NOT NULL, -- Retrieved knowledge base entries
    relevance_scores JSONB, -- Scores for each retrieved item
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    expires_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT (NOW() + INTERVAL '24 hours')
);

-- Chatbot rate limiting and usage tracking
CREATE TABLE IF NOT EXISTS chatbot_usage_stats (
    stat_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID,
    api_key_hash VARCHAR(64), -- For API key rate limiting
    request_count INTEGER DEFAULT 0,
    token_count INTEGER DEFAULT 0,
    cost_accumulated DECIMAL(10,4) DEFAULT 0,
    time_window_start TIMESTAMP WITH TIME ZONE NOT NULL,
    time_window_end TIMESTAMP WITH TIME ZONE NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Indexes for chatbot system
CREATE INDEX IF NOT EXISTS idx_chatbot_conversations_user ON chatbot_conversations(user_id, last_message_at DESC);
CREATE INDEX IF NOT EXISTS idx_chatbot_conversations_active ON chatbot_conversations(is_active, last_message_at DESC);
CREATE INDEX IF NOT EXISTS idx_chatbot_messages_conversation ON chatbot_messages(conversation_id, created_at ASC);
CREATE INDEX IF NOT EXISTS idx_chatbot_knowledge_cache_hash ON chatbot_knowledge_cache(query_hash);
CREATE INDEX IF NOT EXISTS idx_chatbot_knowledge_cache_expires ON chatbot_knowledge_cache(expires_at);
CREATE INDEX IF NOT EXISTS idx_chatbot_usage_user_window ON chatbot_usage_stats(user_id, time_window_start, time_window_end);
CREATE INDEX IF NOT EXISTS idx_chatbot_usage_api_window ON chatbot_usage_stats(api_key_hash, time_window_start, time_window_end);

-- ============================================================================
-- END OF GPT-4 CHATBOT INTEGRATION SYSTEM
-- ============================================================================

-- ============================================================================
-- LLM TEXT ANALYSIS SYSTEM
-- ============================================================================

-- Text analysis cache table
CREATE TABLE IF NOT EXISTS text_analysis_cache (
    cache_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    text_hash VARCHAR(128) NOT NULL,
    tasks_hash VARCHAR(128) NOT NULL,
    analysis_result JSONB NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    expires_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT (NOW() + INTERVAL '24 hours'),
    access_count INTEGER DEFAULT 0,
    last_accessed TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    UNIQUE(text_hash, tasks_hash)
);

-- Text analysis results table (for analytics and history)
CREATE TABLE IF NOT EXISTS text_analysis_results (
    result_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    request_id VARCHAR(64) NOT NULL UNIQUE,
    text_hash VARCHAR(128) NOT NULL,
    user_id UUID,
    source VARCHAR(50) DEFAULT 'api',
    text_length INTEGER NOT NULL,
    tasks_performed JSONB NOT NULL, -- Array of analysis tasks
    processing_time_ms INTEGER NOT NULL,
    total_tokens INTEGER NOT NULL,
    total_cost DECIMAL(8,6) NOT NULL,
    success BOOLEAN NOT NULL DEFAULT true,
    error_message TEXT,
    sentiment_result JSONB,
    entity_results JSONB,
    summary_result JSONB,
    classification_result JSONB,
    language_result JSONB,
    keywords_result JSONB,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Text analysis usage statistics
CREATE TABLE IF NOT EXISTS text_analysis_usage (
    usage_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID,
    task_type VARCHAR(50) NOT NULL, -- sentiment, entities, summary, etc.
    request_count INTEGER DEFAULT 0,
    token_count INTEGER DEFAULT 0,
    cost_accumulated DECIMAL(10,6) DEFAULT 0,
    time_window_start TIMESTAMP WITH TIME ZONE NOT NULL,
    time_window_end TIMESTAMP WITH TIME ZONE NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Indexes for text analysis system
CREATE INDEX IF NOT EXISTS idx_text_analysis_cache_hash ON text_analysis_cache(text_hash, tasks_hash);
CREATE INDEX IF NOT EXISTS idx_text_analysis_cache_expires ON text_analysis_cache(expires_at);
CREATE INDEX IF NOT EXISTS idx_text_analysis_results_user ON text_analysis_results(user_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_text_analysis_results_hash ON text_analysis_results(text_hash);
CREATE INDEX IF NOT EXISTS idx_text_analysis_usage_user_window ON text_analysis_usage(user_id, time_window_start, time_window_end);
CREATE INDEX IF NOT EXISTS idx_text_analysis_usage_task_window ON text_analysis_usage(task_type, time_window_start, time_window_end);

-- ============================================================================
-- END OF LLM TEXT ANALYSIS SYSTEM
-- ============================================================================

-- ============================================================================
-- NATURAL LANGUAGE POLICY GENERATION SYSTEM
-- ============================================================================

-- Policy generation results table
CREATE TABLE IF NOT EXISTS policy_generation_results (
    policy_id VARCHAR(64) PRIMARY KEY,
    request_id VARCHAR(64) NOT NULL UNIQUE,
    primary_rule_id VARCHAR(64),
    alternative_rules JSONB, -- Array of alternative rule IDs
    validation_result VARCHAR(50) DEFAULT 'pending', -- 'valid', 'invalid', 'pending'
    processing_time_ms INTEGER NOT NULL,
    tokens_used INTEGER NOT NULL,
    cost DECIMAL(8,6) NOT NULL,
    success BOOLEAN NOT NULL DEFAULT true,
    error_message TEXT,
    version VARCHAR(20) DEFAULT '1.0.0',
    parent_version VARCHAR(20),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Generated rules table
CREATE TABLE IF NOT EXISTS generated_rules (
    rule_id VARCHAR(64) PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    description TEXT NOT NULL,
    natural_language_input TEXT NOT NULL,
    rule_type VARCHAR(50) NOT NULL,
    domain VARCHAR(50) NOT NULL,
    format VARCHAR(20) NOT NULL,
    generated_code TEXT NOT NULL,
    rule_metadata JSONB,
    validation_tests JSONB,
    documentation TEXT,
    confidence_score DECIMAL(3,2) DEFAULT 0.0,
    suggested_improvements JSONB,
    generated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Rule deployments table
CREATE TABLE IF NOT EXISTS rule_deployments (
    deployment_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    rule_id VARCHAR(64) NOT NULL REFERENCES generated_rules(rule_id),
    target_environment VARCHAR(50) NOT NULL, -- 'development', 'staging', 'production'
    deployed_by VARCHAR(100) NOT NULL,
    review_comments TEXT,
    status VARCHAR(50) DEFAULT 'pending_approval', -- 'pending_approval', 'deployed', 'failed', 'rejected'
    deployed_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    error_message TEXT,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Rule templates table (pre-defined templates for common scenarios)
CREATE TABLE IF NOT EXISTS rule_templates (
    template_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name VARCHAR(255) NOT NULL,
    description TEXT NOT NULL,
    domain VARCHAR(50) NOT NULL,
    rule_type VARCHAR(50) NOT NULL,
    template_data JSONB NOT NULL,
    usage_count INTEGER DEFAULT 0,
    created_by VARCHAR(100),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Rule generation analytics
CREATE TABLE IF NOT EXISTS policy_generation_analytics (
    analytic_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id VARCHAR(100),
    operation VARCHAR(50) NOT NULL, -- 'generate', 'validate', 'deploy', etc.
    domain VARCHAR(50),
    rule_type VARCHAR(50),
    format VARCHAR(20),
    tokens_used INTEGER,
    cost DECIMAL(8,6),
    success BOOLEAN NOT NULL DEFAULT true,
    processing_time_ms INTEGER,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Indexes for policy generation system
CREATE INDEX IF NOT EXISTS idx_policy_generation_results_user ON policy_generation_results(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_generated_rules_domain ON generated_rules(domain, generated_at DESC);
CREATE INDEX IF NOT EXISTS idx_generated_rules_type ON generated_rules(rule_type, generated_at DESC);
CREATE INDEX IF NOT EXISTS idx_rule_deployments_rule ON rule_deployments(rule_id, deployed_at DESC);
CREATE INDEX IF NOT EXISTS idx_rule_deployments_status ON rule_deployments(status, deployed_at DESC);
CREATE INDEX IF NOT EXISTS idx_rule_templates_domain ON rule_templates(domain, usage_count DESC);
CREATE INDEX IF NOT EXISTS idx_policy_analytics_user ON policy_generation_analytics(user_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_policy_analytics_operation ON policy_generation_analytics(operation, created_at DESC);

-- ============================================================================
-- END OF NATURAL LANGUAGE POLICY GENERATION SYSTEM
-- ============================================================================

-- ============================================================================
-- DYNAMIC CONFIGURATION MANAGEMENT SYSTEM
-- ============================================================================

-- Configuration values table
CREATE TABLE IF NOT EXISTS configuration_values (
    key VARCHAR(255) NOT NULL,
    value TEXT NOT NULL,
    data_type VARCHAR(20) NOT NULL DEFAULT 'STRING',
    scope VARCHAR(20) NOT NULL DEFAULT 'GLOBAL',
    module_name VARCHAR(100) DEFAULT '',
    description TEXT,
    validation_regex VARCHAR(500),
    allowed_values JSONB,
    is_encrypted BOOLEAN NOT NULL DEFAULT false,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_by VARCHAR(100),
    version INTEGER NOT NULL DEFAULT 1,
    PRIMARY KEY (key, scope)
);

-- Configuration changes audit table
CREATE TABLE IF NOT EXISTS configuration_changes (
    change_id VARCHAR(64) PRIMARY KEY,
    config_key VARCHAR(255) NOT NULL,
    old_value TEXT,
    new_value TEXT NOT NULL,
    changed_by VARCHAR(100) NOT NULL,
    change_reason TEXT,
    changed_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    change_type VARCHAR(20) NOT NULL -- 'CREATE', 'UPDATE', 'DELETE'
);

-- Configuration backups table
CREATE TABLE IF NOT EXISTS configuration_backups (
    backup_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    backup_name VARCHAR(255) NOT NULL,
    scope VARCHAR(20) NOT NULL,
    config_data JSONB NOT NULL,
    created_by VARCHAR(100) NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    restored_at TIMESTAMP WITH TIME ZONE,
    restored_by VARCHAR(100),
    UNIQUE(backup_name, scope)
);

-- Configuration access logs
CREATE TABLE IF NOT EXISTS configuration_access_logs (
    log_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    config_key VARCHAR(255) NOT NULL,
    scope VARCHAR(20) NOT NULL,
    user_id VARCHAR(100) NOT NULL,
    operation VARCHAR(50) NOT NULL, -- 'GET', 'SET', 'UPDATE', 'DELETE'
    ip_address INET,
    user_agent TEXT,
    accessed_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Indexes for configuration system
CREATE INDEX IF NOT EXISTS idx_config_values_scope ON configuration_values(scope);
CREATE INDEX IF NOT EXISTS idx_config_values_module ON configuration_values(module_name);
CREATE INDEX IF NOT EXISTS idx_config_values_updated ON configuration_values(updated_at DESC);
CREATE INDEX IF NOT EXISTS idx_config_changes_key ON configuration_changes(config_key, changed_at DESC);
CREATE INDEX IF NOT EXISTS idx_config_changes_user ON configuration_changes(changed_by, changed_at DESC);
CREATE INDEX IF NOT EXISTS idx_config_backups_name ON configuration_backups(backup_name);
CREATE INDEX IF NOT EXISTS idx_config_access_key ON configuration_access_logs(config_key, accessed_at DESC);
CREATE INDEX IF NOT EXISTS idx_config_access_user ON configuration_access_logs(user_id, accessed_at DESC);

-- ============================================================================
-- END OF DYNAMIC CONFIGURATION MANAGEMENT SYSTEM
-- ============================================================================

-- ============================================================================
-- ADVANCED RULE ENGINE SYSTEM
-- ============================================================================

-- Fraud detection rules table
CREATE TABLE IF NOT EXISTS fraud_detection_rules (
    rule_id VARCHAR(64) PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    description TEXT,
    priority INTEGER NOT NULL DEFAULT 2, -- 1=LOW, 2=MEDIUM, 3=HIGH, 4=CRITICAL
    rule_type VARCHAR(50) NOT NULL, -- 'VALIDATION', 'SCORING', 'PATTERN', 'MACHINE_LEARNING'
    rule_logic JSONB NOT NULL,
    parameters JSONB,
    input_fields JSONB, -- Array of field names
    output_fields JSONB, -- Array of field names
    is_active BOOLEAN NOT NULL DEFAULT true,
    valid_from TIMESTAMP WITH TIME ZONE,
    valid_until TIMESTAMP WITH TIME ZONE,
    created_by VARCHAR(100) NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Fraud detection results table
CREATE TABLE IF NOT EXISTS fraud_detection_results (
    result_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    transaction_id VARCHAR(255) NOT NULL,
    is_fraudulent BOOLEAN NOT NULL DEFAULT false,
    overall_risk INTEGER NOT NULL DEFAULT 0, -- 0=LOW, 1=MEDIUM, 2=HIGH, 3=CRITICAL
    fraud_score DECIMAL(5,4) NOT NULL DEFAULT 0.0,
    rule_results JSONB, -- Array of rule execution results
    aggregated_findings JSONB,
    detection_time TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    processing_duration VARCHAR(50),
    recommendation VARCHAR(20), -- 'APPROVE', 'REVIEW', 'BLOCK'
    INDEX idx_fraud_results_transaction (transaction_id),
    INDEX idx_fraud_results_time (detection_time DESC),
    INDEX idx_fraud_results_risk (overall_risk, detection_time DESC)
);

-- Rule execution logs table
CREATE TABLE IF NOT EXISTS rule_execution_logs (
    log_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    transaction_id VARCHAR(255) NOT NULL,
    rule_id VARCHAR(64) NOT NULL,
    execution_result VARCHAR(20) NOT NULL, -- 'PASS', 'FAIL', 'ERROR', 'TIMEOUT', 'SKIPPED'
    confidence_score DECIMAL(3,2) DEFAULT 0.0,
    risk_level INTEGER DEFAULT 0,
    rule_output JSONB,
    execution_time_ms INTEGER NOT NULL,
    triggered_conditions JSONB,
    error_message TEXT,
    executed_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    INDEX idx_rule_execution_transaction (transaction_id),
    INDEX idx_rule_execution_rule (rule_id),
    INDEX idx_rule_execution_time (executed_at DESC),
    INDEX idx_rule_execution_result (execution_result, executed_at DESC)
);

-- Rule performance metrics table
CREATE TABLE IF NOT EXISTS rule_performance_metrics (
    metric_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    rule_id VARCHAR(64) NOT NULL UNIQUE,
    total_executions INTEGER NOT NULL DEFAULT 0,
    successful_executions INTEGER NOT NULL DEFAULT 0,
    failed_executions INTEGER NOT NULL DEFAULT 0,
    fraud_detections INTEGER NOT NULL DEFAULT 0,
    false_positives INTEGER NOT NULL DEFAULT 0,
    average_execution_time_ms DECIMAL(8,2) DEFAULT 0.0,
    average_confidence_score DECIMAL(3,2) DEFAULT 0.0,
    last_execution TIMESTAMP WITH TIME ZONE,
    error_counts JSONB, -- Map of error types to counts
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    INDEX idx_rule_metrics_rule (rule_id),
    INDEX idx_rule_metrics_executions (total_executions DESC)
);

-- Batch processing jobs table
CREATE TABLE IF NOT EXISTS batch_processing_jobs (
    job_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    batch_id VARCHAR(64) NOT NULL UNIQUE,
    job_type VARCHAR(50) NOT NULL, -- 'FRAUD_EVALUATION', 'RULE_TESTING', etc.
    status VARCHAR(20) NOT NULL DEFAULT 'PENDING', -- 'PENDING', 'PROCESSING', 'COMPLETED', 'FAILED'
    total_items INTEGER NOT NULL DEFAULT 0,
    processed_items INTEGER NOT NULL DEFAULT 0,
    progress DECIMAL(5,4) DEFAULT 0.0,
    parameters JSONB,
    results JSONB,
    error_message TEXT,
    created_by VARCHAR(100),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    started_at TIMESTAMP WITH TIME ZONE,
    completed_at TIMESTAMP WITH TIME ZONE,
    INDEX idx_batch_jobs_batch (batch_id),
    INDEX idx_batch_jobs_status (status, created_at DESC),
    INDEX idx_batch_jobs_type (job_type, created_at DESC)
);

-- ============================================================================
-- END OF ADVANCED RULE ENGINE SYSTEM
-- ============================================================================

-- ============================================================================
-- TOOL CATEGORIES SYSTEM
-- ============================================================================

-- Tool execution results table
CREATE TABLE IF NOT EXISTS tool_execution_results (
    result_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tool_name VARCHAR(100) NOT NULL,
    user_id VARCHAR(100) NOT NULL,
    success BOOLEAN NOT NULL DEFAULT true,
    message TEXT,
    data JSONB,
    error_details TEXT,
    execution_time_ms INTEGER,
    executed_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    INDEX idx_tool_results_tool (tool_name, executed_at DESC),
    INDEX idx_tool_results_user (user_id, executed_at DESC),
    INDEX idx_tool_results_success (success, executed_at DESC)
);

-- Tool performance metrics table
CREATE TABLE IF NOT EXISTS tool_performance_metrics (
    metric_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tool_name VARCHAR(100) NOT NULL,
    total_executions INTEGER NOT NULL DEFAULT 0,
    successful_executions INTEGER NOT NULL DEFAULT 0,
    failed_executions INTEGER NOT NULL DEFAULT 0,
    average_execution_time_ms DECIMAL(8,2) DEFAULT 0.0,
    last_execution TIMESTAMP WITH TIME ZONE,
    error_counts JSONB, -- Map of error types to counts
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    INDEX idx_tool_metrics_tool (tool_name),
    INDEX idx_tool_metrics_executions (total_executions DESC)
);

-- Tool configurations table
CREATE TABLE IF NOT EXISTS tool_configurations (
    config_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tool_name VARCHAR(100) NOT NULL,
    config_key VARCHAR(255) NOT NULL,
    config_value JSONB,
    description TEXT,
    is_active BOOLEAN NOT NULL DEFAULT true,
    created_by VARCHAR(100),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    UNIQUE(tool_name, config_key),
    INDEX idx_tool_config_tool (tool_name),
    INDEX idx_tool_config_active (is_active)
);

-- Tool usage analytics table
CREATE TABLE IF NOT EXISTS tool_usage_analytics (
    analytic_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tool_name VARCHAR(100) NOT NULL,
    user_id VARCHAR(100),
    operation VARCHAR(50) NOT NULL, -- 'execute', 'configure', 'monitor'
    success BOOLEAN NOT NULL DEFAULT true,
    execution_time_ms INTEGER,
    data_processed INTEGER, -- Records, bytes, etc.
    recorded_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    INDEX idx_tool_analytics_tool (tool_name, recorded_at DESC),
    INDEX idx_tool_analytics_user (user_id, recorded_at DESC),
    INDEX idx_tool_analytics_operation (operation, recorded_at DESC)
);

-- ============================================================================
-- END OF TOOL CATEGORIES SYSTEM
-- ============================================================================

-- ============================================================================
-- CONSENSUS ENGINE SYSTEM
-- ============================================================================

-- Consensus processes table
CREATE TABLE IF NOT EXISTS consensus_processes (
    consensus_id VARCHAR(64) PRIMARY KEY,
    topic VARCHAR(500) NOT NULL,
    algorithm INTEGER NOT NULL DEFAULT 1, -- 0=UNANIMOUS, 1=MAJORITY, etc.
    participants JSONB NOT NULL,
    max_rounds INTEGER NOT NULL DEFAULT 3,
    timeout_per_round_min INTEGER NOT NULL DEFAULT 10,
    consensus_threshold DECIMAL(3,2) NOT NULL DEFAULT 0.7,
    min_participants INTEGER NOT NULL DEFAULT 3,
    allow_discussion BOOLEAN NOT NULL DEFAULT true,
    require_justification BOOLEAN NOT NULL DEFAULT true,
    custom_rules JSONB,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    INDEX idx_consensus_topic (topic),
    INDEX idx_consensus_algorithm (algorithm),
    INDEX idx_consensus_created (created_at DESC)
);

-- Consensus agents table
CREATE TABLE IF NOT EXISTS consensus_agents (
    agent_id VARCHAR(64) PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    role INTEGER NOT NULL DEFAULT 0, -- 0=EXPERT, 1=REVIEWER, etc.
    voting_weight DECIMAL(5,2) NOT NULL DEFAULT 1.0,
    domain_expertise VARCHAR(255),
    confidence_threshold DECIMAL(3,2) NOT NULL DEFAULT 0.7,
    is_active BOOLEAN NOT NULL DEFAULT true,
    last_active TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    INDEX idx_agents_active (is_active, last_active DESC),
    INDEX idx_agents_role (role)
);

-- Consensus results table
CREATE TABLE IF NOT EXISTS consensus_results (
    result_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    consensus_id VARCHAR(64) NOT NULL,
    final_decision TEXT,
    confidence_level INTEGER NOT NULL DEFAULT 3, -- 1=VERY_LOW, 5=VERY_HIGH
    algorithm_used INTEGER NOT NULL,
    final_state INTEGER NOT NULL, -- 0=INITIALIZING, 6=REACHED_CONSENSUS, etc.
    total_participants INTEGER NOT NULL,
    agreement_percentage DECIMAL(5,4),
    total_duration_ms BIGINT,
    resolution_details JSONB,
    dissenting_opinions JSONB,
    completed_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    INDEX idx_results_consensus (consensus_id),
    INDEX idx_results_completed (completed_at DESC),
    INDEX idx_results_state (final_state, completed_at DESC)
);

-- Voting rounds table
CREATE TABLE IF NOT EXISTS consensus_voting_rounds (
    round_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    consensus_id VARCHAR(64) NOT NULL,
    round_number INTEGER NOT NULL,
    topic VARCHAR(500),
    description TEXT,
    opinions JSONB,
    vote_counts JSONB,
    state INTEGER NOT NULL DEFAULT 0,
    started_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    ended_at TIMESTAMP WITH TIME ZONE,
    metadata JSONB,
    INDEX idx_rounds_consensus (consensus_id, round_number),
    INDEX idx_rounds_state (state, started_at DESC)
);

-- Agent opinions table
CREATE TABLE IF NOT EXISTS consensus_agent_opinions (
    opinion_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    consensus_id VARCHAR(64) NOT NULL,
    agent_id VARCHAR(64) NOT NULL,
    round_number INTEGER NOT NULL,
    decision TEXT NOT NULL,
    confidence_score DECIMAL(3,2) NOT NULL DEFAULT 0.5,
    reasoning TEXT,
    supporting_data JSONB,
    concerns JSONB,
    submitted_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    INDEX idx_opinions_consensus (consensus_id, round_number),
    INDEX idx_opinions_agent (agent_id, submitted_at DESC),
    INDEX idx_opinions_decision (decision, confidence_score DESC)
);

-- Consensus performance metrics table
CREATE TABLE IF NOT EXISTS consensus_performance_metrics (
    metric_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    consensus_id VARCHAR(64),
    agent_id VARCHAR(64),
    metric_type VARCHAR(50) NOT NULL, -- 'execution_time', 'agreement_rate', etc.
    metric_value DECIMAL(10,4),
    recorded_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    INDEX idx_metrics_consensus (consensus_id, recorded_at DESC),
    INDEX idx_metrics_agent (agent_id, recorded_at DESC),
    INDEX idx_metrics_type (metric_type, recorded_at DESC)
);

-- ============================================================================
-- END OF CONSENSUS ENGINE SYSTEM
-- ============================================================================

-- ============================================================================
-- MESSAGE TRANSLATOR SYSTEM
-- ============================================================================

-- Translation rules table
CREATE TABLE IF NOT EXISTS translation_rules (
    rule_id VARCHAR(64) PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    from_protocol INTEGER NOT NULL,
    to_protocol INTEGER NOT NULL,
    transformation_rules JSONB,
    bidirectional BOOLEAN NOT NULL DEFAULT false,
    priority INTEGER NOT NULL DEFAULT 1,
    active BOOLEAN NOT NULL DEFAULT true,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    INDEX idx_translation_rules_protocols (from_protocol, to_protocol),
    INDEX idx_translation_rules_active (active, priority DESC),
    INDEX idx_translation_rules_created (created_at DESC)
);

-- Protocol schemas table
CREATE TABLE IF NOT EXISTS protocol_schemas (
    protocol_id INTEGER PRIMARY KEY,
    protocol_name VARCHAR(50) NOT NULL,
    schema_definition JSONB NOT NULL,
    content_type VARCHAR(100),
    supports_binary BOOLEAN NOT NULL DEFAULT false,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    INDEX idx_protocol_schemas_name (protocol_name)
);

-- Translation statistics table
CREATE TABLE IF NOT EXISTS translation_statistics (
    stat_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    from_protocol VARCHAR(50) NOT NULL,
    to_protocol VARCHAR(50) NOT NULL,
    result_status VARCHAR(20) NOT NULL,
    processing_time_ms INTEGER,
    message_size_bytes INTEGER,
    recorded_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    INDEX idx_translation_stats_protocols (from_protocol, to_protocol, recorded_at DESC),
    INDEX idx_translation_stats_result (result_status, recorded_at DESC),
    INDEX idx_translation_stats_time (processing_time_ms, recorded_at DESC)
);

-- Translation cache table
CREATE TABLE IF NOT EXISTS translation_cache (
    cache_key VARCHAR(255) PRIMARY KEY,
    original_message TEXT NOT NULL,
    translated_message TEXT NOT NULL,
    from_protocol INTEGER NOT NULL,
    to_protocol INTEGER NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    last_accessed TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    access_count INTEGER NOT NULL DEFAULT 0,
    INDEX idx_translation_cache_access (last_accessed DESC, access_count DESC)
);

-- ============================================================================
-- END OF MESSAGE TRANSLATOR SYSTEM
-- ============================================================================

-- ============================================================================
-- COMMUNICATION MEDIATOR SYSTEM
-- ============================================================================

-- Conversation contexts table
CREATE TABLE IF NOT EXISTS conversation_contexts (
    conversation_id VARCHAR(64) PRIMARY KEY,
    topic VARCHAR(500) NOT NULL,
    objective TEXT NOT NULL,
    state INTEGER NOT NULL DEFAULT 0, -- 0=INITIALIZING, 1=ACTIVE, etc.
    participants JSONB NOT NULL,
    started_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    last_activity TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    timeout_duration_min INTEGER NOT NULL DEFAULT 30,
    metadata JSONB,
    INDEX idx_conversation_topic (topic),
    INDEX idx_conversation_state (state, last_activity DESC),
    INDEX idx_conversation_started (started_at DESC)
);

-- Conversation messages table
CREATE TABLE IF NOT EXISTS conversation_messages (
    message_id VARCHAR(64) PRIMARY KEY,
    conversation_id VARCHAR(64) NOT NULL,
    sender_agent_id VARCHAR(64) NOT NULL,
    recipient_agent_id VARCHAR(64), -- NULL for broadcast
    message_type VARCHAR(50) NOT NULL DEFAULT 'message',
    content JSONB NOT NULL,
    sent_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    delivered_at TIMESTAMP WITH TIME ZONE,
    acknowledged BOOLEAN NOT NULL DEFAULT false,
    metadata JSONB,
    INDEX idx_messages_conversation (conversation_id, sent_at DESC),
    INDEX idx_messages_sender (sender_agent_id, sent_at DESC),
    INDEX idx_messages_recipient (recipient_agent_id, sent_at DESC),
    INDEX idx_messages_type (message_type, sent_at DESC)
);

-- Message deliveries table (for tracking delivery status)
CREATE TABLE IF NOT EXISTS message_deliveries (
    delivery_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    message_id VARCHAR(64) NOT NULL,
    agent_id VARCHAR(64) NOT NULL,
    delivered_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    acknowledged_at TIMESTAMP WITH TIME ZONE,
    status VARCHAR(20) NOT NULL DEFAULT 'delivered', -- delivered, acknowledged, failed
    UNIQUE(message_id, agent_id),
    INDEX idx_deliveries_message (message_id),
    INDEX idx_deliveries_agent (agent_id, delivered_at DESC),
    INDEX idx_deliveries_status (status, delivered_at DESC)
);

-- Conflict resolutions table
CREATE TABLE IF NOT EXISTS conflict_resolutions (
    conflict_id VARCHAR(64) PRIMARY KEY,
    conversation_id VARCHAR(64) NOT NULL,
    conflict_type INTEGER NOT NULL, -- 0=CONTRADICTORY_RESPONSES, etc.
    description TEXT NOT NULL,
    involved_agents JSONB NOT NULL,
    strategy_used INTEGER NOT NULL, -- 0=MAJORITY_VOTE, etc.
    conflict_details JSONB,
    resolution_result JSONB,
    detected_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    resolved_at TIMESTAMP WITH TIME ZONE,
    resolved_successfully BOOLEAN NOT NULL DEFAULT false,
    resolution_summary TEXT,
    INDEX idx_conflicts_conversation (conversation_id, detected_at DESC),
    INDEX idx_conflicts_type (conflict_type, detected_at DESC),
    INDEX idx_conflicts_resolved (resolved_successfully, resolved_at DESC)
);

-- Conversation performance metrics table
CREATE TABLE IF NOT EXISTS conversation_performance_metrics (
    metric_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    conversation_id VARCHAR(64),
    agent_id VARCHAR(64),
    metric_type VARCHAR(50) NOT NULL, -- 'participation_rate', 'response_time', etc.
    metric_value DECIMAL(10,4),
    recorded_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    INDEX idx_conversation_metrics_conversation (conversation_id, recorded_at DESC),
    INDEX idx_conversation_metrics_agent (agent_id, recorded_at DESC),
    INDEX idx_conversation_metrics_type (metric_type, recorded_at DESC)
);

-- ============================================================================
-- END OF COMMUNICATION MEDIATOR SYSTEM
-- ============================================================================

-- ============================================================================
-- CONSENSUS ENGINE SYSTEM
-- ============================================================================

-- Consensus type enumeration
CREATE TYPE consensus_type AS ENUM (
    'unanimous',        -- All agents must agree
    'majority',         -- Simple majority (>50%)
    'supermajority',    -- 2/3 or 3/4 agreement
    'weighted_voting',  -- Votes weighted by agent confidence/expertise
    'ranked_choice',    -- Agents rank options, instant runoff
    'bayesian'          -- Combine probabilistic beliefs
);

-- Consensus status enumeration
CREATE TYPE consensus_status AS ENUM (
    'open',             -- Accepting votes
    'closed',           -- No more votes accepted
    'reached',          -- Consensus achieved
    'failed',           -- Couldn't reach consensus
    'timeout'           -- Deadline passed
);

-- Consensus sessions table
CREATE TABLE IF NOT EXISTS consensus_sessions (
    session_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    topic VARCHAR(500) NOT NULL,
    description TEXT,
    consensus_type consensus_type NOT NULL,
    status consensus_status DEFAULT 'open',
    required_votes INTEGER NOT NULL,
    current_votes INTEGER DEFAULT 0,
    threshold DECIMAL(5,2),  -- For majority/supermajority
    started_at TIMESTAMP DEFAULT NOW(),
    deadline TIMESTAMP,
    closed_at TIMESTAMP,
    result JSONB,
    result_confidence DECIMAL(5,2),
    initiator_agent_id UUID REFERENCES agents(agent_id),
    metadata JSONB
);

-- Consensus votes table
CREATE TABLE IF NOT EXISTS consensus_votes (
    vote_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    session_id UUID REFERENCES consensus_sessions(session_id) ON DELETE CASCADE,
    agent_id UUID NOT NULL REFERENCES agents(agent_id),
    vote_value JSONB NOT NULL,  -- Flexible: boolean, number, ranking, etc.
    confidence DECIMAL(5,2),    -- Agent's confidence in their vote
    reasoning TEXT,
    cast_at TIMESTAMP DEFAULT NOW(),
    UNIQUE(session_id, agent_id)  -- One vote per agent per session
);

-- Indexes for performance
CREATE INDEX IF NOT EXISTS idx_consensus_status ON consensus_sessions(status, deadline);
CREATE INDEX IF NOT EXISTS idx_consensus_votes_session ON consensus_votes(session_id, cast_at);

-- ============================================================================
-- END OF CONSENSUS ENGINE SYSTEM
-- ============================================================================

-- =============================================================================
-- TOOL INTEGRATION SYSTEM
-- =============================================================================

-- Tool credentials table for secure storage of tool authentication data
CREATE TABLE IF NOT EXISTS tool_credentials (
    credential_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    tool_name VARCHAR(100) NOT NULL,
    credential_type VARCHAR(50) NOT NULL, -- api_key, oauth, basic_auth, jwt, certificate, etc.
    encrypted_credentials BYTEA NOT NULL, -- AES-256 encrypted credential data
    credential_metadata JSONB, -- Additional metadata (expiry, scopes, etc.)
    created_by UUID NOT NULL, -- User or agent who created the credential
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    expires_at TIMESTAMP WITH TIME ZONE, -- Optional expiration date
    last_used TIMESTAMP WITH TIME ZONE,
    is_active BOOLEAN NOT NULL DEFAULT TRUE,
    usage_count INTEGER NOT NULL DEFAULT 0,

    -- Constraints
    CHECK (credential_type IN ('api_key', 'oauth', 'basic_auth', 'jwt', 'certificate', 'kerberos', 'saml')),
    CHECK (expires_at IS NULL OR expires_at > created_at)
);

-- Tool usage metrics table for tracking tool performance and usage
CREATE TABLE IF NOT EXISTS tool_usage_metrics (
    usage_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    tool_name VARCHAR(100) NOT NULL,
    operation VARCHAR(100) NOT NULL,
    success BOOLEAN NOT NULL,
    execution_time_ms INTEGER,
    error_message TEXT,
    error_code VARCHAR(50),
    user_id UUID, -- User who initiated the operation (if applicable)
    agent_id UUID, -- Agent who initiated the operation (if applicable)
    session_id UUID, -- Session context
    request_metadata JSONB, -- Request parameters, headers, etc.
    response_metadata JSONB, -- Response details, status codes, etc.
    executed_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    tool_version VARCHAR(50), -- Version of the tool used
    rate_limit_status VARCHAR(20), -- 'ok', 'limited', 'blocked'

    -- Constraints
    CHECK (rate_limit_status IN ('ok', 'limited', 'blocked')),
    CHECK (execution_time_ms >= 0)
);

-- Tool health status table for monitoring tool availability
CREATE TABLE IF NOT EXISTS tool_health_status (
    health_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    tool_name VARCHAR(100) NOT NULL,
    status VARCHAR(20) NOT NULL, -- 'healthy', 'degraded', 'unhealthy', 'offline', 'maintenance'
    last_check TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    next_check TIMESTAMP WITH TIME ZONE,
    uptime_percentage DECIMAL(5,2), -- Percentage of time tool was available
    average_response_time_ms INTEGER,
    error_rate DECIMAL(5,4), -- Error rate as decimal (0.0000 to 1.0000)
    consecutive_failures INTEGER NOT NULL DEFAULT 0,
    last_failure TIMESTAMP WITH TIME ZONE,
    health_metadata JSONB, -- Additional health information
    alert_sent BOOLEAN NOT NULL DEFAULT FALSE,

    -- Constraints
    CHECK (status IN ('healthy', 'degraded', 'unhealthy', 'offline', 'maintenance')),
    CHECK (uptime_percentage >= 0.00 AND uptime_percentage <= 100.00),
    CHECK (error_rate >= 0.0000 AND error_rate <= 1.0000),
    CHECK (consecutive_failures >= 0)
);

-- Tool configuration templates table
CREATE TABLE IF NOT EXISTS tool_config_templates (
    template_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    tool_category VARCHAR(50) NOT NULL,
    tool_type VARCHAR(100) NOT NULL,
    template_name VARCHAR(255) NOT NULL,
    template_version VARCHAR(20) NOT NULL DEFAULT '1.0',
    config_schema JSONB NOT NULL, -- JSON Schema for configuration validation
    default_config JSONB NOT NULL, -- Default configuration values
    required_capabilities JSONB, -- Required tool capabilities
    description TEXT,
    is_active BOOLEAN NOT NULL DEFAULT TRUE,
    created_by UUID NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),

    -- Constraints
    CHECK (tool_category IN ('communication', 'erp', 'crm', 'dms', 'storage', 'analytics', 'workflow', 'integration', 'security', 'monitoring', 'web_search', 'mcp_tools')),
    UNIQUE (tool_type, template_name, template_version)
);

-- Indexes for tool integration system
CREATE INDEX IF NOT EXISTS idx_tool_credentials_tool_name ON tool_credentials(tool_name, is_active);
CREATE INDEX IF NOT EXISTS idx_tool_credentials_created_by ON tool_credentials(created_by);
CREATE INDEX IF NOT EXISTS idx_tool_credentials_expires_at ON tool_credentials(expires_at) WHERE expires_at IS NOT NULL;

CREATE INDEX IF NOT EXISTS idx_tool_usage_metrics_tool_name ON tool_usage_metrics(tool_name, executed_at);
CREATE INDEX IF NOT EXISTS idx_tool_usage_metrics_user ON tool_usage_metrics(user_id, executed_at);
CREATE INDEX IF NOT EXISTS idx_tool_usage_metrics_agent ON tool_usage_metrics(agent_id, executed_at);
CREATE INDEX IF NOT EXISTS idx_tool_usage_metrics_success ON tool_usage_metrics(success, executed_at);
CREATE INDEX IF NOT EXISTS idx_tool_usage_metrics_operation ON tool_usage_metrics(operation, tool_name);

CREATE INDEX IF NOT EXISTS idx_tool_health_status_tool ON tool_health_status(tool_name, last_check);
CREATE INDEX IF NOT EXISTS idx_tool_health_status_status ON tool_health_status(status, last_check);

CREATE INDEX IF NOT EXISTS idx_tool_config_templates_category ON tool_config_templates(tool_category, is_active);
CREATE INDEX IF NOT EXISTS idx_tool_config_templates_type ON tool_config_templates(tool_type, is_active);

-- ============================================================================
-- END OF TOOL INTEGRATION SYSTEM
-- ============================================================================

-- ============================================================================
-- CUSTOMER MANAGEMENT SYSTEM
-- ============================================================================

-- Customers table
CREATE TABLE IF NOT EXISTS customers (
    customer_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    full_name VARCHAR(200) NOT NULL,
    email VARCHAR(255) UNIQUE,
    phone VARCHAR(50),
    date_of_birth DATE,
    nationality VARCHAR(3),  -- ISO 3166-1 alpha-3
    residency_country VARCHAR(3),
    occupation VARCHAR(100),

    -- KYC/AML Fields
    kyc_status VARCHAR(20) DEFAULT 'PENDING' CHECK (kyc_status IN ('PENDING', 'IN_PROGRESS', 'VERIFIED', 'FAILED', 'EXPIRED')),
    kyc_completed_date DATE,
    kyc_expiry_date DATE,
    kyc_documents_uploaded BOOLEAN DEFAULT false,
    pep_status BOOLEAN DEFAULT false,
    pep_details TEXT,
    watchlist_flags TEXT[],
    sanctions_screening_date DATE,
    sanctions_match_found BOOLEAN DEFAULT false,

    -- Risk Assessment
    risk_rating VARCHAR(20) DEFAULT 'LOW' CHECK (risk_rating IN ('LOW', 'MEDIUM', 'HIGH', 'CRITICAL')),
    risk_score INTEGER CHECK (risk_score >= 0 AND risk_score <= 100),
    risk_factors JSONB,
    last_risk_assessment_date TIMESTAMP WITH TIME ZONE,
    risk_assessment_reason TEXT,

    -- Account Info
    account_opened_date DATE DEFAULT CURRENT_DATE,
    account_status VARCHAR(20) DEFAULT 'active' CHECK (account_status IN ('active', 'suspended', 'closed', 'frozen')),
    account_type VARCHAR(50),  -- individual, business, joint

    -- Aggregates (updated via triggers)
    total_transactions INTEGER DEFAULT 0,
    total_volume_usd DECIMAL(15,2) DEFAULT 0,
    last_transaction_date TIMESTAMP WITH TIME ZONE,
    flagged_transactions INTEGER DEFAULT 0,

    -- Metadata
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    created_by UUID,
    updated_by UUID
);

-- Customer address history
CREATE TABLE IF NOT EXISTS customer_addresses (
    address_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    customer_id UUID NOT NULL REFERENCES customers(customer_id) ON DELETE CASCADE,
    address_type VARCHAR(20) CHECK (address_type IN ('residential', 'business', 'mailing')),
    street_address TEXT,
    city VARCHAR(100),
    state_province VARCHAR(100),
    postal_code VARCHAR(20),
    country VARCHAR(3),
    is_current BOOLEAN DEFAULT true,
    valid_from DATE DEFAULT CURRENT_DATE,
    valid_to DATE,
    verified BOOLEAN DEFAULT false,
    verification_date DATE
);

-- Customer risk factors
CREATE TABLE IF NOT EXISTS customer_risk_events (
    event_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    customer_id UUID NOT NULL REFERENCES customers(customer_id) ON DELETE CASCADE,
    event_type VARCHAR(50) NOT NULL,
    event_description TEXT,
    severity VARCHAR(20) CHECK (severity IN ('LOW', 'MEDIUM', 'HIGH', 'CRITICAL')),
    risk_score_impact INTEGER,
    detected_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    resolved BOOLEAN DEFAULT false,
    resolved_at TIMESTAMP WITH TIME ZONE,
    resolution_notes TEXT
);

-- Indexes for customer system
CREATE INDEX IF NOT EXISTS idx_customers_risk_rating ON customers(risk_rating);
CREATE INDEX IF NOT EXISTS idx_customers_kyc_status ON customers(kyc_status);
CREATE INDEX IF NOT EXISTS idx_customers_pep ON customers(pep_status) WHERE pep_status = true;
CREATE INDEX IF NOT EXISTS idx_customers_sanctions ON customers(sanctions_match_found) WHERE sanctions_match_found = true;
CREATE INDEX IF NOT EXISTS idx_customers_email ON customers(email);
CREATE INDEX IF NOT EXISTS idx_customers_created_at ON customers(created_at);
CREATE INDEX IF NOT EXISTS idx_customers_updated_at ON customers(updated_at);

CREATE INDEX IF NOT EXISTS idx_customer_addresses_customer ON customer_addresses(customer_id, is_current);
CREATE INDEX IF NOT EXISTS idx_customer_addresses_country ON customer_addresses(country);

CREATE INDEX IF NOT EXISTS idx_customer_risk_events_customer ON customer_risk_events(customer_id, detected_at);
CREATE INDEX IF NOT EXISTS idx_customer_risk_events_severity ON customer_risk_events(severity, resolved);
CREATE INDEX IF NOT EXISTS idx_customer_risk_events_type ON customer_risk_events(event_type);

-- ============================================================================
-- FRAUD DETECTION BATCH PROCESSING SYSTEM
-- ============================================================================

-- Job queue for batch fraud scanning
CREATE TABLE IF NOT EXISTS fraud_scan_job_queue (
    job_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    status VARCHAR(20) DEFAULT 'queued' CHECK (status IN ('queued', 'processing', 'completed', 'failed')),
    worker_id VARCHAR(50),
    priority INTEGER DEFAULT 3 CHECK (priority >= 1 AND priority <= 10),
    filters JSONB NOT NULL,  -- Transaction selection criteria
    claimed_at TIMESTAMP,
    started_at TIMESTAMP,
    completed_at TIMESTAMP,
    progress INTEGER DEFAULT 0 CHECK (progress >= 0 AND progress <= 100),
    transactions_processed INTEGER DEFAULT 0,
    transactions_total INTEGER,
    transactions_flagged INTEGER DEFAULT 0,
    results_summary JSONB,
    error_message TEXT,
    created_by UUID REFERENCES user_authentication(user_id) ON DELETE SET NULL,
    created_at TIMESTAMP DEFAULT NOW()
);

-- Indexes for job queue performance
CREATE INDEX IF NOT EXISTS idx_job_queue_status ON fraud_scan_job_queue(status, priority DESC, created_at ASC);
CREATE INDEX IF NOT EXISTS idx_job_queue_worker ON fraud_scan_job_queue(worker_id, status);

-- ============================================================================
-- END OF CUSTOMER MANAGEMENT SYSTEM
-- ============================================================================

-- ============================================================================
-- SYSTEM CONFIGURATION MANAGEMENT
-- ============================================================================

-- System configuration table for dynamic runtime configuration
CREATE TABLE IF NOT EXISTS system_configuration (
    config_key VARCHAR(100) PRIMARY KEY,
    config_value JSONB NOT NULL,
    config_type VARCHAR(50) NOT NULL CHECK (config_type IN ('string', 'integer', 'float', 'boolean', 'json')),
    description TEXT,
    is_sensitive BOOLEAN DEFAULT false,
    validation_rules JSONB,  -- Min/max, allowed values, regex, etc.
    last_updated TIMESTAMP DEFAULT NOW(),
    updated_by UUID REFERENCES user_authentication(user_id) ON DELETE SET NULL,
    requires_restart BOOLEAN DEFAULT false,
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Configuration history table for audit trail
CREATE TABLE IF NOT EXISTS configuration_history (
    history_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    config_key VARCHAR(100) NOT NULL,
    old_value JSONB,
    new_value JSONB,
    changed_by UUID NOT NULL REFERENCES user_authentication(user_id) ON DELETE CASCADE,
    changed_at TIMESTAMP DEFAULT NOW(),
    change_reason TEXT,
    change_source VARCHAR(50) DEFAULT 'manual' CHECK (change_source IN ('manual', 'api', 'automated', 'migration')),
    approved_by UUID REFERENCES user_authentication(user_id) ON DELETE SET NULL,
    approval_timestamp TIMESTAMP,
    rollback_available BOOLEAN DEFAULT false
);

-- Indexes for performance and auditing
CREATE INDEX IF NOT EXISTS idx_config_history_key ON configuration_history(config_key, changed_at DESC);
CREATE INDEX IF NOT EXISTS idx_config_history_time ON configuration_history(changed_at DESC);
CREATE INDEX IF NOT EXISTS idx_config_history_user ON configuration_history(changed_by, changed_at DESC);
CREATE INDEX IF NOT EXISTS idx_config_history_source ON configuration_history(change_source, changed_at DESC);

-- Trigger to update updated_at timestamp
CREATE OR REPLACE FUNCTION update_configuration_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trigger_update_config_updated_at
    BEFORE UPDATE ON system_configuration
    FOR EACH ROW
    EXECUTE FUNCTION update_configuration_updated_at();

-- ============================================================================
-- END OF SYSTEM CONFIGURATION MANAGEMENT
-- ============================================================================
