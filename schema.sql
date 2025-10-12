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


