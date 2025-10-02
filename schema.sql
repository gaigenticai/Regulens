-- Regulens Agentic AI Compliance System - Database Schema
-- Production-grade PostgreSQL schema for compliance data management

-- Enable necessary extensions
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pg_trgm";
CREATE EXTENSION IF NOT EXISTS "btree_gin";

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
    configuration JSONB NOT NULL,
    version INTEGER NOT NULL DEFAULT 1,
    is_active BOOLEAN NOT NULL DEFAULT TRUE,
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

-- =============================================================================
-- UPDATE TRIGGERS FOR NEW TABLES
-- =============================================================================

CREATE TRIGGER update_customer_profiles_updated_at BEFORE UPDATE ON customer_profiles FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_business_processes_updated_at BEFORE UPDATE ON business_processes FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_regulatory_impacts_updated_at BEFORE UPDATE ON regulatory_impacts FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_audit_anomalies_updated_at BEFORE UPDATE ON audit_anomalies FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();


