-- Regulens Agentic AI Compliance System - Data Seeding Script
-- Generates realistic production data for all 3 POCs (>100K records each)

-- =============================================================================
-- HELPER FUNCTIONS
-- =============================================================================

-- Function to generate random names
CREATE OR REPLACE FUNCTION random_first_name()
RETURNS TEXT AS $$
DECLARE
    names TEXT[] := ARRAY[
        'James', 'Mary', 'John', 'Patricia', 'Robert', 'Jennifer', 'Michael', 'Linda',
        'William', 'Elizabeth', 'David', 'Barbara', 'Richard', 'Susan', 'Joseph', 'Margaret',
        'Thomas', 'Dorothy', 'Charles', 'Lisa', 'Christopher', 'Nancy', 'Daniel', 'Karen',
        'Matthew', 'Betty', 'Anthony', 'Helen', 'Donald', 'Sandra', 'Mark', 'Donna',
        'Paul', 'Carol', 'Steven', 'Ruth', 'Andrew', 'Sharon', 'Joshua', 'Michelle',
        'Kenneth', 'Laura', 'Kevin', 'Sarah', 'Brian', 'Kimberly', 'George', 'Deborah',
        'Edward', 'Dorothy', 'Ronald', 'Amanda', 'Timothy', 'Melissa', 'Jason', 'Deborah',
        'Jeffrey', 'Stephanie', 'Ryan', 'Rebecca', 'Jacob', 'Sharon', 'Gary', 'Cynthia'
    ];
BEGIN
    RETURN names[1 + floor(random() * array_length(names, 1))];
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION random_last_name()
RETURNS TEXT AS $$
DECLARE
    names TEXT[] := ARRAY[
        'Smith', 'Johnson', 'Williams', 'Brown', 'Jones', 'Garcia', 'Miller', 'Davis',
        'Rodriguez', 'Martinez', 'Hernandez', 'Lopez', 'Gonzalez', 'Wilson', 'Anderson',
        'Thomas', 'Taylor', 'Moore', 'Jackson', 'Martin', 'Lee', 'Perez', 'Thompson',
        'White', 'Harris', 'Sanchez', 'Clark', 'Ramirez', 'Lewis', 'Robinson', 'Walker',
        'Young', 'Allen', 'King', 'Wright', 'Scott', 'Torres', 'Nguyen', 'Hill', 'Flores',
        'Green', 'Adams', 'Nelson', 'Baker', 'Hall', 'Rivera', 'Campbell', 'Mitchell',
        'Carter', 'Roberts', 'Gomez', 'Phillips', 'Evans', 'Turner', 'Diaz', 'Parker'
    ];
BEGIN
    RETURN names[1 + floor(random() * array_length(names, 1))];
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION random_business_name()
RETURNS TEXT AS $$
DECLARE
    prefixes TEXT[] := ARRAY['Global', 'International', 'Advanced', 'Premier', 'Elite', 'Smart', 'Tech', 'Digital', 'Modern', 'Future'];
    suffixes TEXT[] := ARRAY['Solutions', 'Systems', 'Technologies', 'Services', 'Group', 'Enterprises', 'Corporation', 'LLC', 'Inc', 'Ltd'];
    industries TEXT[] := ARRAY['Financial', 'Trading', 'Investment', 'Consulting', 'Management', 'Capital', 'Asset', 'Wealth', 'Risk', 'Compliance'];
BEGIN
    RETURN prefixes[1 + floor(random() * array_length(prefixes, 1))] || ' ' ||
           industries[1 + floor(random() * array_length(industries, 1))] || ' ' ||
           suffixes[1 + floor(random() * array_length(suffixes, 1))];
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION random_country_code()
RETURNS TEXT AS $$
DECLARE
    codes TEXT[] := ARRAY['USA', 'GBR', 'DEU', 'FRA', 'ITA', 'ESP', 'NLD', 'BEL', 'CHE', 'AUT', 'CAN', 'AUS', 'JPN', 'SGP', 'HKG'];
BEGIN
    RETURN codes[1 + floor(random() * array_length(codes, 1))];
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION random_ip_address()
RETURNS INET AS $$
BEGIN
    RETURN (floor(random() * 255) + 1 || '.' ||
            floor(random() * 255) + 1 || '.' ||
            floor(random() * 255) + 1 || '.' ||
            floor(random() * 255) + 1)::INET;
END;
$$ LANGUAGE plpgsql;

-- =============================================================================
-- POC 1: TRANSACTION GUARDIAN DATA SEEDING (>100K customers + transactions)
-- =============================================================================

-- Insert 150,000 customer profiles
INSERT INTO customer_profiles (
    customer_type, full_name, business_name, tax_id, date_of_birth, nationality,
    residency_country, risk_rating, kyc_status, pep_status, watchlist_flags,
    sanctions_screening, adverse_media, kyc_completed_at, last_review_date
)
SELECT
    CASE WHEN random() < 0.3 THEN 'BUSINESS' ELSE 'INDIVIDUAL' END,
    CASE WHEN random() >= 0.3 THEN random_first_name() || ' ' || random_last_name() ELSE NULL END,
    CASE WHEN random() < 0.3 THEN random_business_name() ELSE NULL END,
    'TAX' || lpad((row_number() over ())::text, 9, '0'),
    CASE WHEN random() >= 0.3 THEN (CURRENT_DATE - INTERVAL '18 years' - (random() * INTERVAL '60 years'))::date ELSE NULL END,
    random_country_code(),
    random_country_code(),
    CASE
        WHEN random() < 0.05 THEN 'VERY_HIGH'
        WHEN random() < 0.15 THEN 'HIGH'
        WHEN random() < 0.35 THEN 'MEDIUM'
        ELSE 'LOW'
    END,
    CASE
        WHEN random() < 0.02 THEN 'REJECTED'
        WHEN random() < 0.08 THEN 'PENDING'
        ELSE 'VERIFIED'
    END,
    random() < 0.01, -- 1% PEP status
    CASE WHEN random() < 0.03 THEN jsonb_build_array('PEP', 'SANCTIONS') ELSE '[]'::jsonb END,
    CASE WHEN random() < 0.05 THEN jsonb_build_object('screened', true, 'matches', floor(random() * 3)) ELSE NULL END,
    CASE WHEN random() < 0.08 THEN jsonb_build_array('Negative article found') ELSE NULL END,
    CASE WHEN random() < 0.08 THEN CURRENT_TIMESTAMP - (random() * INTERVAL '365 days') ELSE NULL END,
    CURRENT_TIMESTAMP - (random() * INTERVAL '180 days')
FROM generate_series(1, 150000);

-- Insert 500,000 transactions (avg 3-4 per customer)
INSERT INTO transactions (
    customer_id, transaction_type, amount, currency, sender_account, receiver_account,
    sender_name, receiver_name, sender_country, receiver_country, transaction_date,
    value_date, description, channel, merchant_category_code, ip_address,
    device_fingerprint, location_lat, location_lng, risk_score, flagged, flag_reason
)
SELECT
    cp.customer_id,
    CASE floor(random() * 8)
        WHEN 0 THEN 'WIRE_TRANSFER'
        WHEN 1 THEN 'ACH_PAYMENT'
        WHEN 2 THEN 'CHECK_PAYMENT'
        WHEN 3 THEN 'CARD_PAYMENT'
        WHEN 4 THEN 'CASH_WITHDRAWAL'
        WHEN 5 THEN 'ATM_WITHDRAWAL'
        WHEN 6 THEN 'ONLINE_TRANSFER'
        ELSE 'INTERNATIONAL_TRANSFER'
    END,
    (random() * 100000)::decimal(20,2), -- Up to $100K
    CASE floor(random() * 5)
        WHEN 0 THEN 'USD' WHEN 1 THEN 'EUR' WHEN 2 THEN 'GBP' WHEN 3 THEN 'JPY' ELSE 'CAD'
    END,
    'ACC' || lpad((random() * 99999999)::int::text, 8, '0'),
    'ACC' || lpad((random() * 99999999)::int::text, 8, '0'),
    CASE WHEN cp.customer_type = 'INDIVIDUAL' THEN cp.full_name ELSE cp.business_name END,
    random_business_name(),
    cp.residency_country,
    random_country_code(),
    CURRENT_TIMESTAMP - (random() * INTERVAL '365 days'),
    CURRENT_TIMESTAMP - (random() * INTERVAL '365 days') + INTERVAL '1 day',
    CASE floor(random() * 5)
        WHEN 0 THEN 'Payment for services'
        WHEN 1 THEN 'Invoice settlement'
        WHEN 2 THEN 'Salary payment'
        WHEN 3 THEN 'Investment transfer'
        ELSE 'Business transaction'
    END,
    CASE floor(random() * 5)
        WHEN 0 THEN 'ONLINE' WHEN 1 THEN 'ATM' WHEN 2 THEN 'BRANCH' WHEN 3 THEN 'WIRE' ELSE 'MOBILE'
    END,
    CASE floor(random() * 10)
        WHEN 0 THEN '5411' WHEN 1 THEN '6011' WHEN 2 THEN '5812' WHEN 3 THEN '5541'
        WHEN 4 THEN '5912' WHEN 5 THEN '7230' WHEN 6 THEN '4814' WHEN 7 THEN '4829'
        ELSE '8999'
    END,
    random_ip_address(),
    'FP' || encode(gen_random_bytes(16), 'hex'),
    (random() * 180 - 90)::decimal(10,8),  -- latitude
    (random() * 360 - 180)::decimal(11,8), -- longitude
    random()::decimal(5,4), -- risk score 0.0000-1.0000
    random() < 0.02, -- 2% flagged
    CASE WHEN random() < 0.02 THEN
        CASE floor(random() * 4)
            WHEN 0 THEN 'High risk country destination'
            WHEN 1 THEN 'Unusual transaction amount'
            WHEN 2 THEN 'Frequent small transactions'
            ELSE 'Suspicious merchant category'
        END
    ELSE NULL END
FROM customer_profiles cp
CROSS JOIN generate_series(1, 4) gs -- 4 transactions per customer on average
WHERE random() < 0.8; -- Some customers have fewer transactions

-- Insert transaction risk assessments (AI-generated)
INSERT INTO transaction_risk_assessments (
    transaction_id, agent_name, risk_score, risk_factors, recommended_actions,
    confidence_level, assessment_reasoning, processing_time_ms, assessed_at
)
SELECT
    t.transaction_id,
    'transaction_guardian_v2',
    LEAST(t.risk_score + (random() * 0.2 - 0.1), 1.0)::decimal(5,4),
    jsonb_build_object(
        'amount_anomaly', random() < 0.1,
        'country_risk', random() < 0.05,
        'velocity_check', random() < 0.08,
        'behavior_pattern', random() < 0.15
    ),
    CASE WHEN random() < 0.05 THEN
        jsonb_build_array('Enhanced due diligence required', 'Transaction hold for review')
    ELSE
        jsonb_build_array('Standard monitoring applied')
    END,
    (0.7 + random() * 0.3)::decimal(3,2),
    CASE floor(random() * 3)
        WHEN 0 THEN 'Transaction amount exceeds customer historical average by 3x'
        WHEN 1 THEN 'Destination country has elevated risk score'
        ELSE 'Unusual transaction velocity detected'
    END,
    (500 + random() * 2500)::int,
    t.transaction_date + INTERVAL '5 minutes'
FROM transactions t
WHERE random() < 0.3; -- 30% of transactions get risk assessments

-- Insert customer behavior patterns (learned by agents)
INSERT INTO customer_behavior_patterns (
    customer_id, pattern_type, pattern_data, confidence_score, last_observed,
    pattern_valid_until, detected_by_agent
)
SELECT
    cp.customer_id,
    CASE floor(random() * 5)
        WHEN 0 THEN 'REGULAR_MONTHLY_PAYMENTS'
        WHEN 1 THEN 'INTERNATIONAL_TRANSFERS'
        WHEN 2 THEN 'HIGH_VALUE_TRANSACTIONS'
        WHEN 3 THEN 'FREQUENT_SMALL_TRANSFERS'
        ELSE 'BUSINESS_CYCLE_PATTERNS'
    END,
    jsonb_build_object(
        'frequency_days', floor(random() * 30) + 1,
        'average_amount', (random() * 50000)::decimal(20,2),
        'transaction_count', floor(random() * 100) + 10,
        'countries_involved', jsonb_build_array(random_country_code(), random_country_code())
    ),
    (0.6 + random() * 0.4)::decimal(3,2),
    CURRENT_TIMESTAMP - (random() * INTERVAL '90 days'),
    CURRENT_TIMESTAMP + INTERVAL '180 days',
    'behavior_analyzer_v1'
FROM customer_profiles cp
WHERE random() < 0.4; -- 40% of customers have identified patterns

-- =============================================================================
-- POC 2: REGULATORY ASSESSOR DATA SEEDING (>100K regulatory docs + impacts)
-- =============================================================================

-- Insert 50,000 regulatory documents
INSERT INTO regulatory_documents (
    regulator, document_type, title, document_number, publication_date,
    effective_date, status, jurisdiction, sector, full_text, summary,
    key_requirements, affected_entities, compliance_deadlines, url
)
SELECT
    CASE floor(random() * 8)
        WHEN 0 THEN 'SEC' WHEN 1 THEN 'FCA' WHEN 2 THEN 'ECB' WHEN 3 THEN 'FED'
        WHEN 4 THEN 'FINRA' WHEN 5 THEN 'ESMA' WHEN 6 THEN 'IOSCO' ELSE 'BIS'
    END,
    CASE floor(random() * 6)
        WHEN 0 THEN 'REGULATION' WHEN 1 THEN 'GUIDANCE' WHEN 2 THEN 'RULE'
        WHEN 3 THEN 'DIRECTIVE' WHEN 4 THEN 'STANDARD' ELSE 'POLICY'
    END,
    CASE floor(random() * 10)
        WHEN 0 THEN 'Enhanced Risk Management Framework for Digital Assets'
        WHEN 1 THEN 'Strengthened Capital Requirements for Market Risk'
        WHEN 2 THEN 'New Reporting Standards for Sustainable Finance'
        WHEN 3 THEN 'Updated Anti-Money Laundering Guidelines'
        WHEN 4 THEN 'Cybersecurity Resilience Requirements'
        WHEN 5 THEN 'Cross-Border Transaction Reporting Standards'
        WHEN 6 THEN 'Enhanced Due Diligence for High-Risk Customers'
        WHEN 7 THEN 'Climate Risk Disclosure Requirements'
        WHEN 8 THEN 'Digital Identity Verification Standards'
        ELSE 'Market Abuse Prevention Measures'
    END || ' - ' || (2018 + floor(random() * 6))::text,
    'REG-' || lpad((row_number() over ())::text, 6, '0'),
    CURRENT_DATE - (random() * INTERVAL '1460 days'), -- Last 4 years
    CURRENT_DATE - (random() * INTERVAL '730 days') + INTERVAL '90 days',
    CASE WHEN random() < 0.05 THEN 'SUPERSEDED' ELSE 'ACTIVE' END,
    CASE floor(random() * 5)
        WHEN 0 THEN 'UNITED_STATES' WHEN 1 THEN 'UNITED_KINGDOM'
        WHEN 2 THEN 'EUROPEAN_UNION' WHEN 3 THEN 'GLOBAL' ELSE 'NORTH_AMERICA'
    END,
    CASE floor(random() * 8)
        WHEN 0 THEN 'BANKING' WHEN 1 THEN 'INVESTMENT' WHEN 2 THEN 'INSURANCE'
        WHEN 3 THEN 'PAYMENTS' WHEN 4 THEN 'CAPITAL_MARKETS' WHEN 5 THEN 'FINTECH'
        WHEN 6 THEN 'ASSET_MANAGEMENT' ELSE 'WEALTH_MANAGEMENT'
    END,
    'This regulatory document establishes comprehensive requirements for [sector] institutions regarding [topic]. The document outlines specific standards, implementation timelines, and compliance expectations. Key provisions include enhanced monitoring, reporting requirements, and risk management frameworks. Institutions are required to implement these measures within the specified timeframes to ensure regulatory compliance and financial stability.',
    'New regulatory framework requiring enhanced [topic] measures for [sector] institutions with implementation deadline of [date].',
    jsonb_build_array(
        'Implement enhanced monitoring systems',
        'Establish regular reporting procedures',
        'Conduct comprehensive risk assessments',
        'Develop incident response protocols'
    ),
    jsonb_build_array(
        'Commercial Banks', 'Investment Firms', 'Insurance Companies', 'Payment Providers'
    ),
    jsonb_build_object(
        'initial_deadline', (CURRENT_DATE + INTERVAL '180 days')::text,
        'completion_deadline', (CURRENT_DATE + INTERVAL '365 days')::text,
        'full_compliance', (CURRENT_DATE + INTERVAL '730 days')::text
    ),
    'https://www.regulator.example.com/document/' || (row_number() over ())::text
FROM generate_series(1, 50000);

-- Insert 25,000 business processes
INSERT INTO business_processes (
    process_name, process_category, department, owner, description, risk_level,
    controls, systems_involved, frequency, regulatory_mappings, next_review_date
)
SELECT
    CASE floor(random() * 15)
        WHEN 0 THEN 'Customer Onboarding Process'
        WHEN 1 THEN 'Transaction Processing and Settlement'
        WHEN 2 THEN 'Anti-Money Laundering Monitoring'
        WHEN 3 THEN 'Know Your Customer Verification'
        WHEN 4 THEN 'Credit Risk Assessment'
        WHEN 5 THEN 'Market Risk Management'
        WHEN 6 THEN 'Liquidity Risk Monitoring'
        WHEN 7 THEN 'Operational Risk Controls'
        WHEN 8 THEN 'Regulatory Reporting'
        WHEN 9 THEN 'Capital Adequacy Management'
        WHEN 10 THEN 'Investment Portfolio Management'
        WHEN 11 THEN 'Trade Execution and Clearing'
        WHEN 12 THEN 'Client Account Management'
        WHEN 13 THEN 'Financial Statement Preparation'
        ELSE 'Audit and Compliance Review'
    END,
    CASE floor(random() * 8)
        WHEN 0 THEN 'CUSTOMER_ONBOARDING' WHEN 1 THEN 'TRANSACTION_PROCESSING'
        WHEN 2 THEN 'RISK_MANAGEMENT' WHEN 3 THEN 'COMPLIANCE_MONITORING'
        WHEN 4 THEN 'REGULATORY_REPORTING' WHEN 5 THEN 'FINANCIAL_OPERATIONS'
        WHEN 6 THEN 'AUDIT_PROCEDURES' ELSE 'GOVERNANCE_CONTROLS'
    END,
    CASE floor(random() * 8)
        WHEN 0 THEN 'Compliance' WHEN 1 THEN 'Risk Management' WHEN 2 THEN 'Operations'
        WHEN 3 THEN 'Finance' WHEN 4 THEN 'IT Security' WHEN 5 THEN 'Legal'
        WHEN 6 THEN 'Audit' ELSE 'Business Development'
    END,
    random_first_name() || ' ' || random_last_name(),
    'Comprehensive process for managing [category] activities with integrated controls and monitoring.',
    CASE floor(random() * 4)
        WHEN 0 THEN 'CRITICAL' WHEN 1 THEN 'HIGH' WHEN 2 THEN 'MEDIUM' ELSE 'LOW'
    END,
    jsonb_build_array(
        'Automated monitoring controls',
        'Manual review checkpoints',
        'Exception reporting mechanisms',
        'Audit trail maintenance'
    ),
    jsonb_build_array(
        'Core Banking System', 'Risk Management Platform', 'Compliance Database', 'Reporting Engine'
    ),
    CASE floor(random() * 5)
        WHEN 0 THEN 'REAL_TIME' WHEN 1 THEN 'DAILY' WHEN 2 THEN 'WEEKLY'
        WHEN 3 THEN 'MONTHLY' ELSE 'QUARTERLY'
    END,
    jsonb_build_object(
        'aml_directive', 'Applicable',
        'kyc_requirements', 'Mandatory',
        'reporting_standards', 'Required'
    ),
    CURRENT_DATE + (random() * INTERVAL '365 days')
FROM generate_series(1, 25000);

-- Insert regulatory impacts (AI assessments)
INSERT INTO regulatory_impacts (
    document_id, process_id, impact_level, impact_description, required_actions,
    implementation_cost_estimate, implementation_time_estimate_days, risk_score,
    priority_score, assessment_confidence, assessed_by_agent, status
)
SELECT
    rd.document_id,
    bp.process_id,
    CASE floor(random() * 5)
        WHEN 0 THEN 'NONE' WHEN 1 THEN 'LOW' WHEN 2 THEN 'MEDIUM' WHEN 3 THEN 'HIGH' ELSE 'CRITICAL'
    END,
    CASE floor(random() * 3)
        WHEN 0 THEN 'Minor process adjustments required'
        WHEN 1 THEN 'Significant system modifications needed'
        ELSE 'Major operational changes required'
    END,
    jsonb_build_array(
        'Update process documentation',
        'Implement new controls',
        'Train staff on requirements',
        'Update system configurations'
    ),
    (random() * 500000)::decimal(15,2), -- Up to $500K
    floor(random() * 180) + 30, -- 30-210 days
    random()::decimal(3,2),
    random()::decimal(3,2),
    (0.7 + random() * 0.3)::decimal(3,2),
    'regulatory_assessor_v2',
    'ASSESSED'
FROM regulatory_documents rd
CROSS JOIN business_processes bp
WHERE random() < 0.001; -- Create impacts for ~12,500 combinations

-- Insert regulatory alerts
INSERT INTO regulatory_alerts (
    document_id, alert_type, title, message, recipients, priority, action_required,
    action_deadline, action_status
)
SELECT
    rd.document_id,
    CASE floor(random() * 4)
        WHEN 0 THEN 'NEW_REGULATION' WHEN 1 THEN 'COMPLIANCE_DEADLINE'
        WHEN 2 THEN 'IMPLEMENTATION_UPDATE' ELSE 'URGENT_ACTION'
    END,
    'URGENT: ' || substring(rd.title, 1, 50) || '...',
    'New regulatory requirement requires immediate attention. Implementation deadline: ' || rd.effective_date::text,
    jsonb_build_array(
        jsonb_build_object('name', 'Compliance Team', 'email', 'compliance@company.com'),
        jsonb_build_object('name', 'Risk Management', 'email', 'risk@company.com')
    ),
    CASE WHEN random() < 0.1 THEN 'CRITICAL' WHEN random() < 0.3 THEN 'HIGH' ELSE 'MEDIUM' END,
    random() < 0.7,
    CURRENT_DATE + (random() * INTERVAL '90 days'),
    'PENDING'
FROM regulatory_documents rd
WHERE random() < 0.05; -- 5% of documents generate alerts

-- =============================================================================
-- POC 3: AUDIT INTELLIGENCE AGENT DATA SEEDING (>100K audit logs + anomalies)
-- =============================================================================

-- Insert 200,000 system audit logs
INSERT INTO system_audit_logs (
    system_name, log_level, event_type, event_description, user_id, session_id,
    ip_address, user_agent, resource_accessed, action_performed, parameters,
    result_status, processing_time_ms, event_timestamp
)
SELECT
    CASE floor(random() * 8)
        WHEN 0 THEN 'CORE_BANKING' WHEN 1 THEN 'RISK_ENGINE' WHEN 2 THEN 'COMPLIANCE_DB'
        WHEN 3 THEN 'TRADING_PLATFORM' WHEN 4 THEN 'PAYMENT_SYSTEM' WHEN 5 THEN 'REPORTING_ENGINE'
        WHEN 6 THEN 'CUSTOMER_PORTAL' ELSE 'ADMIN_CONSOLE'
    END,
    CASE floor(random() * 20)
        WHEN 0 THEN 'FATAL' WHEN 1 THEN 'ERROR' WHEN 2 THEN 'WARN' ELSE 'INFO'
    END,
    CASE floor(random() * 12)
        WHEN 0 THEN 'USER_LOGIN' WHEN 1 THEN 'USER_LOGOUT' WHEN 2 THEN 'DATA_ACCESS'
        WHEN 3 THEN 'TRANSACTION_PROCESS' WHEN 4 THEN 'REPORT_GENERATE' WHEN 5 THEN 'CONFIG_CHANGE'
        WHEN 6 THEN 'SECURITY_CHECK' WHEN 7 THEN 'API_CALL' WHEN 8 THEN 'BATCH_JOB'
        WHEN 9 THEN 'FILE_UPLOAD' WHEN 10 THEN 'PERMISSION_CHANGE' ELSE 'AUDIT_REVIEW'
    END,
    CASE floor(random() * 8)
        WHEN 0 THEN 'User authentication successful'
        WHEN 1 THEN 'Transaction processed successfully'
        WHEN 2 THEN 'Report generated and delivered'
        WHEN 3 THEN 'Configuration parameter updated'
        WHEN 4 THEN 'Security policy violation detected'
        WHEN 5 THEN 'API request processed'
        WHEN 6 THEN 'Batch job completed'
        ELSE 'Audit log entry created'
    END,
    CASE WHEN random() < 0.8 THEN 'USER' || lpad((random() * 99999)::int::text, 5, '0') ELSE NULL END,
    CASE WHEN random() < 0.7 THEN 'SESS' || encode(gen_random_bytes(8), 'hex') ELSE NULL END,
    random_ip_address(),
    'Mozilla/5.0 (compatible; AuditAgent/2.1)',
    CASE floor(random() * 6)
        WHEN 0 THEN '/api/transactions' WHEN 1 THEN '/api/customers' WHEN 2 THEN '/api/reports'
        WHEN 3 THEN '/admin/config' WHEN 4 THEN '/api/compliance' ELSE '/api/audit'
    END,
    CASE floor(random() * 8)
        WHEN 0 THEN 'SELECT' WHEN 1 THEN 'INSERT' WHEN 2 THEN 'UPDATE' WHEN 3 THEN 'DELETE'
        WHEN 4 THEN 'EXECUTE' WHEN 5 THEN 'LOGIN' WHEN 6 THEN 'LOGOUT' ELSE 'ACCESS'
    END,
    CASE WHEN random() < 0.3 THEN
        jsonb_build_object('table', 'transactions', 'filters', jsonb_build_object('amount', '>1000'))
    ELSE NULL END,
    CASE WHEN random() < 0.05 THEN 'ERROR' ELSE 'SUCCESS' END,
    floor(random() * 5000)::int,
    CURRENT_TIMESTAMP - (random() * INTERVAL '90 days')
FROM generate_series(1, 200000);

-- Insert 50,000 security events
INSERT INTO security_events (
    system_name, event_type, severity, description, source_ip, destination_ip,
    user_id, session_id, resource, action, event_data, risk_score, alert_generated,
    event_timestamp
)
SELECT
    CASE floor(random() * 6)
        WHEN 0 THEN 'FIREWALL' WHEN 1 THEN 'IDS' WHEN 2 THEN 'SIEM' WHEN 3 THEN 'AUTH_SYSTEM'
        WHEN 4 THEN 'ENDPOINT_PROTECTION' ELSE 'NETWORK_MONITOR'
    END,
    CASE floor(random() * 8)
        WHEN 0 THEN 'BRUTE_FORCE' WHEN 1 THEN 'MALWARE_DETECTED' WHEN 2 THEN 'UNAUTHORIZED_ACCESS'
        WHEN 3 THEN 'DATA_EXFILTRATION' WHEN 4 THEN 'PHISHING_ATTEMPT' WHEN 5 THEN 'PRIVILEGE_ESCALATION'
        WHEN 6 THEN 'SUSPICIOUS_LOGIN' ELSE 'POLICY_VIOLATION'
    END,
    CASE floor(random() * 20)
        WHEN 0 THEN 'CRITICAL' WHEN 1 THEN 'HIGH' ELSE 'MEDIUM'
    END,
    CASE floor(random() * 6)
        WHEN 0 THEN 'Multiple failed login attempts detected from IP address'
        WHEN 1 THEN 'Suspicious file upload attempt blocked'
        WHEN 2 THEN 'Unauthorized database access attempt'
        WHEN 3 THEN 'Potential data exfiltration detected'
        WHEN 4 THEN 'Privileged account access from unusual location'
        WHEN 5 THEN 'Security policy violation in transaction processing'
        ELSE 'Anomalous network traffic pattern detected'
    END,
    random_ip_address(),
    CASE WHEN random() < 0.7 THEN random_ip_address() ELSE NULL END,
    CASE WHEN random() < 0.6 THEN 'USER' || lpad((random() * 99999)::int::text, 5, '0') ELSE NULL END,
    CASE WHEN random() < 0.5 THEN 'SESS' || encode(gen_random_bytes(8), 'hex') ELSE NULL END,
    CASE floor(random() * 5)
        WHEN 0 THEN '/api/transactions' WHEN 1 THEN '/admin' WHEN 2 THEN 'database'
        WHEN 3 THEN '/api/reports' ELSE '/sensitive-data'
    END,
    CASE floor(random() * 4)
        WHEN 0 THEN 'LOGIN_ATTEMPT' WHEN 1 THEN 'DATA_ACCESS' WHEN 2 THEN 'FILE_UPLOAD' ELSE 'CONFIG_CHANGE'
    END,
    jsonb_build_object(
        'user_agent', 'Suspicious Browser/1.0',
        'referer', 'https://malicious-site.com',
        'payload_size', floor(random() * 10000)
    ),
    random()::decimal(3,2),
    random() < 0.15, -- 15% generate alerts
    CURRENT_TIMESTAMP - (random() * INTERVAL '90 days')
FROM generate_series(1, 50000);

-- =============================================================================
-- DEFAULT USERS FOR AUTHENTICATION
-- =============================================================================

-- Create default admin user (password: Admin123)
-- Password hash generated using PBKDF2 with SHA256 (hex-encoded)
INSERT INTO user_authentication (
    user_id, username, password_hash, password_algorithm, email,
    is_active, failed_login_attempts, last_login_at, created_at
) VALUES (
    '550e8400-e29b-41d4-a716-446655440000'::uuid,
    'admin',
    'pbkdf2_sha256$100000$2e8609fba15497fd983f202a9dfb9299$2848e4384cdad347ee2da056f59faddc8be7202c3c9437d162ce2ea716c35a6c', -- PBKDF2 hash for "Admin123"
    'pbkdf2_sha256',
    'admin@regulens.com',
    true,
    0,
    NULL,
    CURRENT_TIMESTAMP
);

-- Create demo user (password: Demo123)
INSERT INTO user_authentication (
    user_id, username, password_hash, password_algorithm, email,
    is_active, failed_login_attempts, last_login_at, created_at
) VALUES (
    '550e8400-e29b-41d4-a716-446655440001'::uuid,
    'demo',
    'pbkdf2_sha256$100000$08d6085f899fc15bad9e598cc1b0d777$abbc4df349797cd4498734718cb782a789b3e3f26d9c37c6a37d2338232d0b50', -- PBKDF2 hash for "Demo123"
    'pbkdf2_sha256',
    'demo@regulens.com',
    true,
    0,
    NULL,
    CURRENT_TIMESTAMP
);

-- Create test user (password: Test123)
INSERT INTO user_authentication (
    user_id, username, password_hash, password_algorithm, email,
    is_active, failed_login_attempts, last_login_at, created_at
) VALUES (
    '550e8400-e29b-41d4-a716-446655440002'::uuid,
    'test',
    'pbkdf2_sha256$100000$2440cced036bc5380c680d03c69ab0a4$53c31f9615aed72863bcb2cd0455aa90d6a7bd8e9926ebcbf69277b7b3313497', -- PBKDF2 hash for "Test123"
    'pbkdf2_sha256',
    'test@regulens.com',
    true,
    0,
    NULL,
    CURRENT_TIMESTAMP
);

-- Insert audit anomalies (AI-detected patterns)
INSERT INTO audit_anomalies (
    anomaly_type, severity, description, affected_systems, affected_users,
    pattern_data, confidence_score, first_detected, last_observed, frequency_count,
    risk_assessment, recommended_actions, status, detected_by_agent
)
SELECT
    CASE floor(random() * 6)
        WHEN 0 THEN 'UNUSUAL_ACCESS_PATTERN' WHEN 1 THEN 'PRIVILEGE_ABUSE'
        WHEN 2 THEN 'DATA_EXFILTRATION' WHEN 3 THEN 'INSIDER_THREAT'
        WHEN 4 THEN 'SYSTEM_MISUSE' ELSE 'COMPLIANCE_VIOLATION'
    END,
    CASE floor(random() * 3)
        WHEN 0 THEN 'CRITICAL' WHEN 1 THEN 'HIGH' ELSE 'MEDIUM'
    END,
    CASE floor(random() * 5)
        WHEN 0 THEN 'Unusual login pattern detected: access from 15 different countries in 24 hours'
        WHEN 1 THEN 'Privileged user accessing sensitive data outside normal hours'
        WHEN 2 THEN 'Large data export followed by account deletion attempt'
        WHEN 3 THEN 'Multiple failed authentication attempts followed by successful login'
        ELSE 'Anomalous transaction pattern: high-value transfers to unusual destinations'
    END,
    jsonb_build_array('CORE_BANKING', 'RISK_ENGINE', 'COMPLIANCE_DB'),
    jsonb_build_array('USER' || lpad((random() * 99999)::int::text, 5, '0')),
    jsonb_build_object(
        'pattern_type', 'temporal_anomaly',
        'deviation_score', (2.5 + random() * 5)::decimal(4,2),
        'baseline_average', floor(random() * 100),
        'observed_value', floor(random() * 500)
    ),
    (0.75 + random() * 0.25)::decimal(3,2),
    CURRENT_TIMESTAMP - (random() * INTERVAL '60 days'),
    CURRENT_TIMESTAMP - (random() * INTERVAL '7 days'),
    floor(random() * 50) + 5,
    jsonb_build_object(
        'risk_level', CASE WHEN random() < 0.3 THEN 'HIGH' ELSE 'MEDIUM' END,
        'potential_impact', 'Data breach or compliance violation',
        'urgency', CASE WHEN random() < 0.2 THEN 'IMMEDIATE' ELSE 'REVIEW' END
    ),
    jsonb_build_array(
        'Conduct immediate security review',
        'Review user access privileges',
        'Implement additional monitoring',
        'Update security policies'
    ),
    CASE floor(random() * 3)
        WHEN 0 THEN 'ACTIVE' WHEN 1 THEN 'INVESTIGATING' ELSE 'MITIGATED'
    END,
    'audit_intelligence_v2'
FROM generate_series(1, 25000);

-- Insert agent learning data (feedback and improvements)
INSERT INTO agent_learning_data (
    agent_type, agent_name, learning_type, input_data, output_result, feedback_type,
    feedback_score, human_feedback, feedback_provided_by, feedback_timestamp,
    incorporated, performance_impact
)
SELECT
    CASE floor(random() * 3)
        WHEN 0 THEN 'transaction_guardian' WHEN 1 THEN 'regulatory_assessor' ELSE 'audit_intelligence'
    END,
    CASE floor(random() * 3)
        WHEN 0 THEN 'primary_agent_v1' WHEN 1 THEN 'primary_agent_v2' ELSE 'backup_agent'
    END,
    CASE floor(random() * 4)
        WHEN 0 THEN 'PATTERN' WHEN 1 THEN 'THRESHOLD' WHEN 2 THEN 'RULE' ELSE 'MODEL_UPDATE'
    END,
    jsonb_build_object(
        'transaction_amount', (random() * 10000)::decimal(10,2),
        'customer_risk_score', random()::decimal(3,2),
        'country_risk', random_country_code()
    ),
    jsonb_build_object(
        'risk_assessment', CASE WHEN random() < 0.7 THEN 'LOW_RISK' ELSE 'HIGH_RISK' END,
        'confidence', (0.8 + random() * 0.2)::decimal(3,2),
        'recommended_action', 'APPROVE'
    ),
    CASE floor(random() * 3)
        WHEN 0 THEN 'POSITIVE' WHEN 1 THEN 'NEGATIVE' ELSE 'NEUTRAL'
    END,
    (0.3 + random() * 0.7)::decimal(3,2),
    CASE floor(random() * 4)
        WHEN 0 THEN 'Accurate risk assessment - transaction was legitimate'
        WHEN 1 THEN 'False positive - should not have been flagged'
        WHEN 2 THEN 'Good detection of suspicious pattern'
        ELSE 'Model needs adjustment for this transaction type'
    END,
    'COMPLIANCE_OFFICER_' || lpad((random() * 999)::int::text, 3, '0'),
    CURRENT_TIMESTAMP - (random() * INTERVAL '30 days'),
    random() < 0.8, -- 80% incorporated
    CASE WHEN random() < 0.8 THEN
        jsonb_build_object('accuracy_improvement', (random() * 15)::decimal(4,1))
    ELSE NULL END
FROM generate_series(1, 75000);

-- =============================================================================
-- SHARED AGENT LEARNING DATA
-- =============================================================================

-- Insert agent decision history
INSERT INTO agent_decision_history (
    poc_type, agent_name, decision_context, decision_made, outcome_result,
    outcome_score, human_feedback, learning_applied, decision_timestamp,
    feedback_received_at
)
SELECT
    CASE floor(random() * 3)
        WHEN 0 THEN 'TRANSACTION_GUARDIAN' WHEN 1 THEN 'REGULATORY_ASSESSOR' ELSE 'AUDIT_INTELLIGENCE'
    END,
    'agent_' || (floor(random() * 5) + 1)::text,
    jsonb_build_object(
        'input_parameters', jsonb_build_object('amount', (random() * 50000)::decimal(10,2)),
        'risk_factors', jsonb_build_array('high_amount', 'new_customer'),
        'historical_context', 'Similar transaction approved previously'
    ),
    jsonb_build_object(
        'decision', CASE WHEN random() < 0.8 THEN 'APPROVE' ELSE 'REVIEW' END,
        'confidence', (0.7 + random() * 0.3)::decimal(3,2),
        'reasoning', 'Based on customer history and transaction pattern'
    ),
    jsonb_build_object(
        'actual_outcome', CASE WHEN random() < 0.9 THEN 'SUCCESS' ELSE 'VIOLATION_DETECTED' END,
        'processing_time_ms', floor(random() * 2000)
    ),
    CASE WHEN random() < 0.9 THEN (0.8 + random() * 0.2) ELSE (0.2 + random() * 0.3) END,
    CASE WHEN random() < 0.3 THEN
        jsonb_build_object('feedback', 'Good decision', 'accuracy', 'HIGH')
    ELSE NULL END,
    random() < 0.7,
    CURRENT_TIMESTAMP - (random() * INTERVAL '60 days'),
    CASE WHEN random() < 0.3 THEN CURRENT_TIMESTAMP - (random() * INTERVAL '50 days') ELSE NULL END
FROM generate_series(1, 100000);

-- Insert agent performance metrics
INSERT INTO agent_performance_metrics (
    poc_type, agent_name, metric_type, metric_name, metric_value, benchmark_value,
    measurement_period_start, measurement_period_end, calculated_at
)
SELECT
    CASE floor(random() * 3)
        WHEN 0 THEN 'TRANSACTION_GUARDIAN' WHEN 1 THEN 'REGULATORY_ASSESSOR' ELSE 'AUDIT_INTELLIGENCE'
    END,
    'agent_' || (floor(random() * 5) + 1)::text,
    CASE floor(random() * 3)
        WHEN 0 THEN 'ACCURACY' WHEN 1 THEN 'PERFORMANCE' ELSE 'RELIABILITY'
    END,
    CASE floor(random() * 6)
        WHEN 0 THEN 'false_positive_rate' WHEN 1 THEN 'true_positive_rate'
        WHEN 2 THEN 'processing_latency_ms' WHEN 3 THEN 'uptime_percentage'
        WHEN 4 THEN 'accuracy_score' ELSE 'recall_rate'
    END,
    CASE floor(random() * 4)
        WHEN 0 THEN random() * 100  -- percentage
        WHEN 1 THEN 100 + random() * 1900  -- latency ms
        WHEN 2 THEN 95 + random() * 5  -- uptime %
        ELSE 0.7 + random() * 0.3  -- score
    END,
    CASE floor(random() * 4)
        WHEN 0 THEN 85 + random() * 10  -- benchmark %
        WHEN 1 THEN 200 + random() * 800  -- benchmark ms
        WHEN 2 THEN 99.5 + random() * 0.4  -- benchmark %
        ELSE 0.85 + random() * 0.1  -- benchmark score
    END,
    CURRENT_DATE - INTERVAL '30 days',
    CURRENT_DATE - INTERVAL '1 day',
    CURRENT_TIMESTAMP
FROM generate_series(1, 50000);

-- =============================================================================
-- SEED DATA: AGENTS
-- =============================================================================

INSERT INTO agents (agent_name, agent_type, status, capabilities, description) VALUES
    ('TransactionGuardian', 'TRANSACTION_GUARDIAN', 'active',
     '["fraud_detection", "risk_assessment", "transaction_monitoring"]'::jsonb,
     'Real-time transaction monitoring and fraud detection agent'),

    ('RegulatoryAssessor', 'REGULATORY_ASSESSOR', 'active',
     '["compliance_checking", "regulatory_monitoring", "policy_analysis"]'::jsonb,
     'Regulatory compliance assessment and monitoring agent'),

    ('AuditIntelligence', 'AUDIT_INTELLIGENCE', 'active',
     '["audit_trail_analysis", "anomaly_detection", "compliance_reporting"]'::jsonb,
     'Audit trail analysis and compliance reporting agent'),

    ('DataIngestionOrchestrator', 'DATA_INGESTION', 'active',
     '["data_pipeline_management", "quality_checks", "schema_validation"]'::jsonb,
     'Data ingestion pipeline orchestration and quality management agent'),

    ('KnowledgeBaseManager', 'KNOWLEDGE_MANAGEMENT', 'active',
     '["semantic_search", "knowledge_indexing", "rag_queries"]'::jsonb,
     'Knowledge base management and semantic search agent'),

    ('DecisionOrchestrator', 'DECISION_ORCHESTRATION', 'active',
     '["mcda_analysis", "consensus_building", "decision_tracking"]'::jsonb,
     'Multi-criteria decision analysis and consensus orchestration agent')
ON CONFLICT (agent_name) DO NOTHING;

-- =============================================================================
-- FINAL DATA VERIFICATION
-- =============================================================================

-- Display data counts
SELECT
    'Agents' as table_name, COUNT(*) as record_count FROM agents
UNION ALL
SELECT 'Customer Profiles', COUNT(*) FROM customer_profiles
UNION ALL
SELECT 'Transactions', COUNT(*) FROM transactions
UNION ALL
SELECT 'Transaction Risk Assessments', COUNT(*) FROM transaction_risk_assessments
UNION ALL
SELECT 'Customer Behavior Patterns', COUNT(*) FROM customer_behavior_patterns
UNION ALL
SELECT 'Regulatory Documents', COUNT(*) FROM regulatory_documents
UNION ALL
SELECT 'Business Processes', COUNT(*) FROM business_processes
UNION ALL
SELECT 'Regulatory Impacts', COUNT(*) FROM regulatory_impacts
UNION ALL
SELECT 'Regulatory Alerts', COUNT(*) FROM regulatory_alerts
UNION ALL
SELECT 'System Audit Logs', COUNT(*) FROM system_audit_logs
UNION ALL
SELECT 'Security Events', COUNT(*) FROM security_events
UNION ALL
SELECT 'Audit Anomalies', COUNT(*) FROM audit_anomalies
UNION ALL
SELECT 'Agent Learning Data', COUNT(*) FROM agent_learning_data
UNION ALL
SELECT 'Agent Decision History', COUNT(*) FROM agent_decision_history
UNION ALL
SELECT 'Agent Performance Metrics', COUNT(*) FROM agent_performance_metrics
ORDER BY record_count DESC;

-- =============================================================================
-- SYSTEM CONFIGURATION SEED DATA
-- =============================================================================

-- Insert default system configuration settings
INSERT INTO system_configuration (config_key, config_value, config_type, description, is_sensitive, requires_restart) VALUES
('max_concurrent_users', '100', 'integer', 'Maximum number of concurrent users allowed in the system', false, true),
('session_timeout_minutes', '30', 'integer', 'User session timeout in minutes', false, false),
('enable_audit_logging', 'true', 'boolean', 'Enable comprehensive audit logging for all system activities', false, true),
('log_retention_days', '90', 'integer', 'Number of days to retain system logs', false, false),
('alert_email_enabled', 'true', 'boolean', 'Enable email notifications for system alerts', false, false),
('alert_email_recipients', 'admin@company.com,security@company.com', 'string', 'Comma-separated list of email addresses for alert notifications', false, false),
('backup_frequency_hours', '24', 'integer', 'Frequency of automatic system backups in hours', false, true),
('max_file_upload_size_mb', '50', 'integer', 'Maximum file upload size in megabytes', false, false),
('enable_two_factor_auth', 'true', 'boolean', 'Require two-factor authentication for all users', true, false),
('password_min_length', '12', 'integer', 'Minimum password length requirement', false, false),
('password_complexity_required', 'true', 'boolean', 'Require complex passwords with mixed characters', false, false),
('api_rate_limit_per_minute', '1000', 'integer', 'API rate limit per minute per user', false, true),
('enable_ip_whitelisting', 'false', 'boolean', 'Enable IP address whitelisting for enhanced security', false, true),
('system_maintenance_mode', 'false', 'boolean', 'Enable system maintenance mode (read-only access)', false, true),
('debug_mode_enabled', 'false', 'boolean', 'Enable debug mode for detailed logging (not for production)', false, true),
('cache_ttl_seconds', '300', 'integer', 'Default cache time-to-live in seconds', false, false),
('max_database_connections', '50', 'integer', 'Maximum database connections allowed', false, true),
('enable_performance_monitoring', 'true', 'boolean', 'Enable real-time performance monitoring', false, false),
('compliance_check_frequency_minutes', '15', 'integer', 'Frequency of automated compliance checks in minutes', false, false),
('agent_decision_timeout_seconds', '30', 'integer', 'Timeout for agent decision making in seconds', false, false)
ON CONFLICT (config_key) DO NOTHING;

