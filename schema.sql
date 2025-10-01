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
