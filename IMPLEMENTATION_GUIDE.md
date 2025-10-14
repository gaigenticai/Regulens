# Implementation Guide: Complete Production-Grade Features

**Document Version:** 1.0
**Created:** 2025-10-14
**Target Audience:** LLM or Developer completing final 13 features
**Estimated Effort:** 40-60 hours for all features

---

## 📋 Overview

This document provides detailed specifications for completing 13 advanced features in the Regulens AI Compliance System. The core system is 100% production-grade, but these specialized features need additional backend implementation to achieve full feature parity with their frontend UIs.

### Current State
- ✅ Core backend: PostgreSQL, Redis, JWT auth, WebSocket, Multi-agent system
- ✅ Core features: Fraud detection, regulatory monitoring, transactions, audit trail
- ✅ Frontend UIs: All 13 features have complete React components and hooks
- ⚠️ Gap: Backend API handlers need implementation for full functionality

### Architecture Context
- **Language:** C++ (backend), TypeScript/React (frontend)
- **Database:** PostgreSQL with pgvector extension
- **API Style:** RESTful JSON with JWT authentication
- **Real-time:** WebSocket for streaming updates
- **AI/ML:** OpenAI API (GPT-4), Anthropic Claude API
- **Pattern:** Follow existing handlers in `shared/*/api_handlers.cpp`

---

## 🎯 Implementation Priorities

### High Priority (Complete First)
1. **Compliance Training System** - Training and certification tracking
2. **Alert Management System** - Alert configuration and routing
3. **Data Quality Monitor** - Data validation and quality metrics
4. **Regulatory Chatbot** - AI-powered regulatory Q&A

### Medium Priority
5. **NL Policy Builder** - Natural language to policy rules
6. **Regulatory Simulator** - Impact simulation system
7. **LLM Key Management** - API key lifecycle management
8. **Memory Management** - Agent memory visualization

### Low Priority (Advanced Features)
9. **Embeddings Explorer** - Vector space visualization
10. **Function Calling Debugger** - LLM function call debugging
11. **MCDA Advanced** - Multi-criteria decision analysis
12. **Tool Categories Testing** - Tool registry testing UI
13. **Integration Marketplace** - Plugin/extension system
14. ❌ Activity feed persistence - Uses memory, should use database
15. ❌ Missing features (change detection, alert system in regulatory UI)
---

## 📦 Feature 1: Compliance Training System

### Frontend Location
- **Page:** `frontend/src/pages/ComplianceTraining.tsx`
- **Hook:** `frontend/src/hooks/useTraining.ts`
- **Expected API calls:**
  - `GET /api/training/courses` - List available courses
  - `GET /api/training/courses/:id` - Course details
  - `POST /api/training/courses/:id/enroll` - Enroll user
  - `GET /api/training/progress/:userId` - User progress
  - `POST /api/training/courses/:id/complete` - Mark course complete
  - `GET /api/training/certifications/:userId` - User certifications
  - `POST /api/training/quiz/:id/submit` - Submit quiz answers
  - `GET /api/training/leaderboard` - Training leaderboard

### Database Schema Additions

```sql
-- Training courses table
CREATE TABLE IF NOT EXISTS training_courses (
    course_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    title VARCHAR(255) NOT NULL,
    description TEXT,
    course_type VARCHAR(50) NOT NULL, -- 'regulation', 'fraud', 'aml', 'kyc'
    difficulty_level VARCHAR(20) NOT NULL, -- 'beginner', 'intermediate', 'advanced'
    duration_minutes INT NOT NULL,
    pass_threshold DECIMAL(5,2) NOT NULL DEFAULT 80.0,
    course_content JSONB NOT NULL, -- Modules, lessons, quizzes
    prerequisites JSONB, -- Array of course_ids
    tags TEXT[],
    is_active BOOLEAN DEFAULT true,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    created_by VARCHAR(255)
);

CREATE INDEX idx_training_courses_type ON training_courses(course_type);
CREATE INDEX idx_training_courses_difficulty ON training_courses(difficulty_level);

-- User enrollments and progress
CREATE TABLE IF NOT EXISTS training_enrollments (
    enrollment_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id VARCHAR(255) NOT NULL,
    course_id UUID NOT NULL REFERENCES training_courses(course_id),
    enrollment_date TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    progress DECIMAL(5,2) DEFAULT 0.0, -- 0-100%
    current_module INT DEFAULT 1,
    status VARCHAR(50) DEFAULT 'in_progress', -- 'in_progress', 'completed', 'failed', 'expired'
    quiz_attempts INT DEFAULT 0,
    quiz_score DECIMAL(5,2),
    completed_at TIMESTAMP WITH TIME ZONE,
    certificate_issued BOOLEAN DEFAULT false,
    certificate_url TEXT,
    last_accessed TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    UNIQUE(user_id, course_id)
);

CREATE INDEX idx_training_enrollments_user ON training_enrollments(user_id, status);
CREATE INDEX idx_training_enrollments_course ON training_enrollments(course_id, status);

-- Quiz submissions
CREATE TABLE IF NOT EXISTS training_quiz_submissions (
    submission_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    enrollment_id UUID NOT NULL REFERENCES training_enrollments(enrollment_id),
    quiz_id VARCHAR(255) NOT NULL,
    user_answers JSONB NOT NULL,
    score DECIMAL(5,2) NOT NULL,
    passed BOOLEAN NOT NULL,
    submitted_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    time_taken_seconds INT,
    feedback JSONB -- Per-question feedback
);

CREATE INDEX idx_quiz_submissions_enrollment ON training_quiz_submissions(enrollment_id, submitted_at DESC);

-- Certifications
CREATE TABLE IF NOT EXISTS training_certifications (
    certification_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id VARCHAR(255) NOT NULL,
    course_id UUID NOT NULL REFERENCES training_courses(course_id),
    certification_name VARCHAR(255) NOT NULL,
    issued_date TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    expiry_date TIMESTAMP WITH TIME ZONE,
    certificate_url TEXT,
    certificate_hash VARCHAR(255), -- For verification
    verification_code VARCHAR(50) UNIQUE,
    is_valid BOOLEAN DEFAULT true,
    revoked_at TIMESTAMP WITH TIME ZONE,
    revocation_reason TEXT
);

CREATE INDEX idx_certifications_user ON training_certifications(user_id, is_valid);
CREATE INDEX idx_certifications_verification ON training_certifications(verification_code);
```

### Backend Implementation

**File to create:** `shared/training/training_api_handlers.hpp`

```cpp
#ifndef REGULENS_TRAINING_API_HANDLERS_HPP
#define REGULENS_TRAINING_API_HANDLERS_HPP

#include <string>
#include <map>
#include <memory>
#include <libpq-fe.h>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {
namespace training {

class TrainingAPIHandlers {
public:
    TrainingAPIHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger
    );

    // Course management
    std::string handle_get_courses(const std::map<std::string, std::string>& query_params);
    std::string handle_get_course_by_id(const std::string& course_id);
    std::string handle_create_course(const std::string& request_body, const std::string& user_id);
    std::string handle_update_course(const std::string& course_id, const std::string& request_body);

    // Enrollment management
    std::string handle_enroll_user(const std::string& course_id, const std::string& request_body, const std::string& user_id);
    std::string handle_get_user_progress(const std::string& user_id, const std::map<std::string, std::string>& query_params);
    std::string handle_update_progress(const std::string& enrollment_id, const std::string& request_body);
    std::string handle_mark_complete(const std::string& course_id, const std::string& user_id);

    // Quiz management
    std::string handle_submit_quiz(const std::string& quiz_id, const std::string& request_body, const std::string& user_id);
    std::string handle_get_quiz_results(const std::string& enrollment_id);

    // Certifications
    std::string handle_get_certifications(const std::string& user_id);
    std::string handle_issue_certificate(const std::string& enrollment_id);
    std::string handle_verify_certificate(const std::string& verification_code);

    // Analytics
    std::string handle_get_leaderboard(const std::map<std::string, std::string>& query_params);
    std::string handle_get_training_stats(const std::string& user_id);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;

    // Helper methods
    double calculate_quiz_score(const nlohmann::json& user_answers, const nlohmann::json& correct_answers);
    std::string generate_certificate_url(const std::string& user_id, const std::string& course_id);
    std::string generate_verification_code();
    bool check_prerequisites(const std::string& user_id, const nlohmann::json& prerequisites);
};

} // namespace training
} // namespace regulens

#endif // REGULENS_TRAINING_API_HANDLERS_HPP
```

**File to create:** `shared/training/training_api_handlers.cpp`

```cpp
#include "training_api_handlers.hpp"
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <iomanip>

namespace regulens {
namespace training {

TrainingAPIHandlers::TrainingAPIHandlers(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<StructuredLogger> logger
) : db_conn_(db_conn), logger_(logger) {}

std::string TrainingAPIHandlers::handle_get_courses(const std::map<std::string, std::string>& query_params) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        std::string course_type = query_params.count("type") ? query_params.at("type") : "";
        std::string difficulty = query_params.count("difficulty") ? query_params.at("difficulty") : "";
        int limit = query_params.count("limit") ? std::stoi(query_params.at("limit")) : 50;

        std::stringstream query;
        query << "SELECT course_id, title, description, course_type, difficulty_level, "
              << "duration_minutes, pass_threshold, tags, is_active, created_at "
              << "FROM training_courses WHERE is_active = true";

        std::vector<const char*> param_values;
        int param_count = 0;

        if (!course_type.empty()) {
            query << " AND course_type = $" << (++param_count);
            param_values.push_back(course_type.c_str());
        }

        if (!difficulty.empty()) {
            query << " AND difficulty_level = $" << (++param_count);
            param_values.push_back(difficulty.c_str());
        }

        query << " ORDER BY created_at DESC LIMIT " << limit;

        PGresult* result = PQexecParams(
            conn,
            query.str().c_str(),
            param_count,
            nullptr,
            param_values.data(),
            nullptr,
            nullptr,
            0
        );

        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            std::string error = PQresultErrorMessage(result);
            PQclear(result);
            logger_->log(LogLevel::ERROR, "Failed to fetch training courses: " + error);
            return R"({"error": "Failed to fetch courses"})";
        }

        nlohmann::json courses = nlohmann::json::array();
        int num_rows = PQntuples(result);

        for (int i = 0; i < num_rows; i++) {
            nlohmann::json course = {
                {"id", PQgetvalue(result, i, 0)},
                {"title", PQgetvalue(result, i, 1)},
                {"description", PQgetvalue(result, i, 2)},
                {"courseType", PQgetvalue(result, i, 3)},
                {"difficultyLevel", PQgetvalue(result, i, 4)},
                {"durationMinutes", std::stoi(PQgetvalue(result, i, 5))},
                {"passThreshold", std::stod(PQgetvalue(result, i, 6))},
                {"tags", nlohmann::json::parse(std::string(PQgetvalue(result, i, 7)))},
                {"isActive", std::string(PQgetvalue(result, i, 8)) == "t"},
                {"createdAt", PQgetvalue(result, i, 9)}
            };
            courses.push_back(course);
        }

        PQclear(result);

        nlohmann::json response = {
            {"courses", courses},
            {"count", num_rows}
        };

        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_get_courses: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

std::string TrainingAPIHandlers::handle_submit_quiz(
    const std::string& quiz_id,
    const std::string& request_body,
    const std::string& user_id
) {
    try {
        nlohmann::json request = nlohmann::json::parse(request_body);
        std::string course_id = request["courseId"];
        nlohmann::json user_answers = request["answers"];
        int time_taken = request.value("timeTaken", 0);

        auto conn = db_conn_->get_connection();
        if (!conn) {
            return R"({"error": "Database connection failed"})";
        }

        // Get enrollment
        const char* enrollment_params[2] = {user_id.c_str(), course_id.c_str()};
        PGresult* enrollment_result = PQexecParams(
            conn,
            "SELECT enrollment_id FROM training_enrollments WHERE user_id = $1 AND course_id = $2",
            2, nullptr, enrollment_params, nullptr, nullptr, 0
        );

        if (PQntuples(enrollment_result) == 0) {
            PQclear(enrollment_result);
            return R"({"error": "Enrollment not found"})";
        }

        std::string enrollment_id = PQgetvalue(enrollment_result, 0, 0);
        PQclear(enrollment_result);

        // Get course with correct answers
        const char* course_params[1] = {course_id.c_str()};
        PGresult* course_result = PQexecParams(
            conn,
            "SELECT course_content, pass_threshold FROM training_courses WHERE course_id = $1",
            1, nullptr, course_params, nullptr, nullptr, 0
        );

        if (PQntuples(course_result) == 0) {
            PQclear(course_result);
            return R"({"error": "Course not found"})";
        }

        nlohmann::json course_content = nlohmann::json::parse(PQgetvalue(course_result, 0, 0));
        double pass_threshold = std::stod(PQgetvalue(course_result, 0, 1));
        PQclear(course_result);

        // Calculate score
        nlohmann::json correct_answers = course_content["quizzes"][quiz_id]["answers"];
        double score = calculate_quiz_score(user_answers, correct_answers);
        bool passed = score >= pass_threshold;

        // Build feedback
        nlohmann::json feedback = nlohmann::json::array();
        for (auto& [question_id, user_answer] : user_answers.items()) {
            bool correct = (user_answer == correct_answers[question_id]);
            feedback.push_back({
                {"questionId", question_id},
                {"userAnswer", user_answer},
                {"correctAnswer", correct_answers[question_id]},
                {"isCorrect", correct}
            });
        }

        // Store submission
        std::string feedback_str = feedback.dump();
        const char* submission_params[6] = {
            enrollment_id.c_str(),
            quiz_id.c_str(),
            user_answers.dump().c_str(),
            std::to_string(score).c_str(),
            passed ? "true" : "false",
            std::to_string(time_taken).c_str()
        };

        PGresult* insert_result = PQexecParams(
            conn,
            "INSERT INTO training_quiz_submissions (enrollment_id, quiz_id, user_answers, score, passed, time_taken_seconds, feedback) "
            "VALUES ($1, $2, $3::jsonb, $4, $5, $6, $7::jsonb) RETURNING submission_id",
            6, nullptr, submission_params, nullptr, nullptr, 0
        );

        if (PQresultStatus(insert_result) != PGRES_TUPLES_OK) {
            std::string error = PQresultErrorMessage(insert_result);
            PQclear(insert_result);
            return R"({"error": "Failed to save quiz submission"})";
        }

        std::string submission_id = PQgetvalue(insert_result, 0, 0);
        PQclear(insert_result);

        // Update enrollment
        const char* update_params[3] = {
            std::to_string(score).c_str(),
            enrollment_id.c_str(),
            std::to_string(score).c_str()
        };

        PQexecParams(
            conn,
            "UPDATE training_enrollments SET quiz_score = $1, quiz_attempts = quiz_attempts + 1, "
            "status = CASE WHEN $3 >= pass_threshold THEN 'completed' ELSE status END "
            "WHERE enrollment_id = $2",
            3, nullptr, update_params, nullptr, nullptr, 0
        );

        nlohmann::json response = {
            {"submissionId", submission_id},
            {"score", score},
            {"passed", passed},
            {"feedback", feedback},
            {"passThreshold", pass_threshold}
        };

        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_submit_quiz: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}

double TrainingAPIHandlers::calculate_quiz_score(
    const nlohmann::json& user_answers,
    const nlohmann::json& correct_answers
) {
    int total_questions = correct_answers.size();
    if (total_questions == 0) return 0.0;

    int correct_count = 0;
    for (auto& [question_id, correct_answer] : correct_answers.items()) {
        if (user_answers.contains(question_id) && user_answers[question_id] == correct_answer) {
            correct_count++;
        }
    }

    return (static_cast<double>(correct_count) / total_questions) * 100.0;
}

std::string TrainingAPIHandlers::generate_verification_code() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 35);

    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::stringstream ss;

    for (int i = 0; i < 12; i++) {
        if (i > 0 && i % 4 == 0) ss << "-";
        ss << chars[dis(gen)];
    }

    return ss.str();
}

} // namespace training
} // namespace regulens
```

### Integration into Main Server

**Add to `server_with_auth.cpp`:**

```cpp
#include "shared/training/training_api_handlers.hpp"

// In initialization section:
std::shared_ptr<regulens::training::TrainingAPIHandlers> training_handlers;
training_handlers = std::make_shared<regulens::training::TrainingAPIHandlers>(db_connection, logger);

// In routing section:
else if (path == "/api/training/courses" && method == "GET") {
    response = training_handlers->handle_get_courses(query_params);
}
else if (path.find("/api/training/courses/") == 0 && method == "GET") {
    std::string course_id = path.substr(23); // After "/api/training/courses/"
    response = training_handlers->handle_get_course_by_id(course_id);
}
else if (path.find("/api/training/courses/") == 0 && path.find("/enroll") != std::string::npos && method == "POST") {
    size_t slash_pos = path.find("/", 23);
    std::string course_id = path.substr(23, slash_pos - 23);
    response = training_handlers->handle_enroll_user(course_id, body, user_id);
}
else if (path.find("/api/training/quiz/") == 0 && path.find("/submit") != std::string::npos && method == "POST") {
    size_t slash_pos = path.find("/", 19);
    std::string quiz_id = path.substr(19, slash_pos - 19);
    response = training_handlers->handle_submit_quiz(quiz_id, body, user_id);
}
// ... add remaining routes
```

### Testing Checklist
- [ ] Can create training courses via API
- [ ] Users can enroll in courses
- [ ] Quiz submissions calculate correct scores
- [ ] Certificates are issued upon completion
- [ ] Verification codes work correctly
- [ ] Leaderboard reflects accurate data
- [ ] Prerequisites are enforced

---

## 📦 Feature 2: Alert Management System

### Frontend Location
- **Page:** `frontend/src/pages/AlertManagement.tsx`
- **Hook:** `frontend/src/hooks/useAlerts.ts`
- **Expected API calls:**
  - `GET /api/alerts/rules` - List alert rules
  - `POST /api/alerts/rules` - Create alert rule
  - `PUT /api/alerts/rules/:id` - Update alert rule
  - `DELETE /api/alerts/rules/:id` - Delete alert rule
  - `GET /api/alerts/history` - Alert history
  - `POST /api/alerts/:id/acknowledge` - Acknowledge alert
  - `GET /api/alerts/channels` - Notification channels
  - `POST /api/alerts/test` - Test alert delivery

### Database Schema Additions

```sql
-- Alert rules configuration
CREATE TABLE IF NOT EXISTS alert_rules (
    rule_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    rule_name VARCHAR(255) NOT NULL,
    description TEXT,
    rule_type VARCHAR(50) NOT NULL, -- 'threshold', 'pattern', 'anomaly', 'scheduled'
    severity VARCHAR(20) NOT NULL, -- 'low', 'medium', 'high', 'critical'
    condition JSONB NOT NULL, -- Rule condition configuration
    notification_channels JSONB NOT NULL, -- ['email', 'slack', 'webhook', 'sms']
    notification_config JSONB, -- Channel-specific configuration
    cooldown_minutes INT DEFAULT 5, -- Minimum time between alerts
    is_enabled BOOLEAN DEFAULT true,
    created_by VARCHAR(255),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    last_triggered_at TIMESTAMP WITH TIME ZONE
);

CREATE INDEX idx_alert_rules_type ON alert_rules(rule_type, is_enabled);
CREATE INDEX idx_alert_rules_severity ON alert_rules(severity);

-- Alert history/incidents
CREATE TABLE IF NOT EXISTS alert_incidents (
    incident_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    rule_id UUID REFERENCES alert_rules(rule_id),
    severity VARCHAR(20) NOT NULL,
    title VARCHAR(255) NOT NULL,
    message TEXT NOT NULL,
    incident_data JSONB, -- Contextual data that triggered alert
    triggered_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    acknowledged_at TIMESTAMP WITH TIME ZONE,
    acknowledged_by VARCHAR(255),
    resolved_at TIMESTAMP WITH TIME ZONE,
    resolved_by VARCHAR(255),
    resolution_notes TEXT,
    status VARCHAR(50) DEFAULT 'active', -- 'active', 'acknowledged', 'resolved', 'false_positive'
    notification_status JSONB -- Per-channel delivery status
);

CREATE INDEX idx_alert_incidents_rule ON alert_incidents(rule_id, triggered_at DESC);
CREATE INDEX idx_alert_incidents_status ON alert_incidents(status, severity);
CREATE INDEX idx_alert_incidents_time ON alert_incidents(triggered_at DESC);

-- Notification channels configuration
CREATE TABLE IF NOT EXISTS notification_channels (
    channel_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    channel_type VARCHAR(50) NOT NULL, -- 'email', 'slack', 'webhook', 'sms', 'pagerduty'
    channel_name VARCHAR(255) NOT NULL,
    configuration JSONB NOT NULL, -- Channel-specific config (URLs, tokens, etc.)
    is_enabled BOOLEAN DEFAULT true,
    last_tested_at TIMESTAMP WITH TIME ZONE,
    test_status VARCHAR(50), -- 'success', 'failed'
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX idx_notification_channels_type ON notification_channels(channel_type, is_enabled);

-- Alert notification log
CREATE TABLE IF NOT EXISTS alert_notifications (
    notification_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    incident_id UUID REFERENCES alert_incidents(incident_id),
    channel_id UUID REFERENCES notification_channels(channel_id),
    sent_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    delivery_status VARCHAR(50) NOT NULL, -- 'sent', 'delivered', 'failed', 'bounced'
    error_message TEXT,
    retry_count INT DEFAULT 0
);

CREATE INDEX idx_alert_notifications_incident ON alert_notifications(incident_id);
CREATE INDEX idx_alert_notifications_status ON alert_notifications(delivery_status, sent_at DESC);
```

### Backend Implementation

**File to create:** `shared/alerts/alert_management_handlers.hpp`

```cpp
#ifndef REGULENS_ALERT_MANAGEMENT_HANDLERS_HPP
#define REGULENS_ALERT_MANAGEMENT_HANDLERS_HPP

#include <string>
#include <map>
#include <memory>
#include <libpq-fe.h>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {
namespace alerts {

class AlertManagementHandlers {
public:
    AlertManagementHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger
    );

    // Alert rules
    std::string handle_get_alert_rules(const std::map<std::string, std::string>& query_params);
    std::string handle_create_alert_rule(const std::string& request_body, const std::string& user_id);
    std::string handle_update_alert_rule(const std::string& rule_id, const std::string& request_body);
    std::string handle_delete_alert_rule(const std::string& rule_id);
    std::string handle_toggle_alert_rule(const std::string& rule_id, bool enabled);

    // Alert incidents
    std::string handle_get_alert_history(const std::map<std::string, std::string>& query_params);
    std::string handle_acknowledge_alert(const std::string& incident_id, const std::string& user_id);
    std::string handle_resolve_alert(const std::string& incident_id, const std::string& request_body, const std::string& user_id);

    // Notification channels
    std::string handle_get_notification_channels();
    std::string handle_create_notification_channel(const std::string& request_body);
    std::string handle_test_notification_channel(const std::string& channel_id);

    // Alert testing
    std::string handle_test_alert(const std::string& request_body);

    // Alert statistics
    std::string handle_get_alert_stats(const std::map<std::string, std::string>& query_params);

    // Trigger evaluation (called by monitoring system)
    bool evaluate_alert_rules();
    bool trigger_alert(const std::string& rule_id, const nlohmann::json& incident_data);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;

    // Helper methods
    bool evaluate_threshold_condition(const nlohmann::json& condition);
    bool send_notification(const std::string& channel_id, const nlohmann::json& alert_data);
    bool check_cooldown(const std::string& rule_id, int cooldown_minutes);
};

} // namespace alerts
} // namespace regulens

#endif // REGULENS_ALERT_MANAGEMENT_HANDLERS_HPP
```

### Key Implementation Notes

1. **Alert Evaluation Engine:**
   - Create background thread that runs every 30 seconds
   - Evaluates all enabled alert rules
   - Checks conditions against current system metrics
   - Respects cooldown periods

2. **Notification Delivery:**
   - Implement email via SMTP (use SMTP_* env vars)
   - Implement webhook via HTTP POST
   - Implement Slack via webhook URL
   - Queue failed deliveries for retry (exponential backoff)

3. **Alert Conditions:**
   - Threshold: `{"metric": "fraud_rate", "operator": ">", "value": 0.15}`
   - Pattern: `{"pattern": "consecutive_failures", "count": 5}`
   - Anomaly: `{"baseline": "avg_last_7_days", "deviation": 2.5}`

---

## 📦 Feature 3: Data Quality Monitor

### Frontend Location
- **Page:** `frontend/src/pages/DataQualityMonitor.tsx`
- **Hook:** `frontend/src/hooks/useDataIngestion.ts` (extend)

### Database Schema Additions

```sql
-- Data quality rules
CREATE TABLE IF NOT EXISTS data_quality_rules (
    rule_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    rule_name VARCHAR(255) NOT NULL,
    data_source VARCHAR(100) NOT NULL, -- 'transactions', 'customers', 'regulatory_changes'
    rule_type VARCHAR(50) NOT NULL, -- 'completeness', 'accuracy', 'consistency', 'timeliness', 'validity'
    validation_logic JSONB NOT NULL,
    severity VARCHAR(20) NOT NULL,
    is_enabled BOOLEAN DEFAULT true,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Data quality check results
CREATE TABLE IF NOT EXISTS data_quality_checks (
    check_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    rule_id UUID REFERENCES data_quality_rules(rule_id),
    check_timestamp TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    records_checked INT NOT NULL,
    records_passed INT NOT NULL,
    records_failed INT NOT NULL,
    quality_score DECIMAL(5,2) NOT NULL, -- 0-100
    failed_records JSONB, -- Sample of failed records
    execution_time_ms INT,
    status VARCHAR(50) NOT NULL -- 'passed', 'warning', 'failed'
);

CREATE INDEX idx_data_quality_checks_time ON data_quality_checks(check_timestamp DESC);
CREATE INDEX idx_data_quality_checks_rule ON data_quality_checks(rule_id, check_timestamp DESC);
```

### API Endpoints

- `GET /api/data-quality/rules` - List quality rules
- `POST /api/data-quality/rules` - Create quality rule
- `GET /api/data-quality/checks` - Recent check results
- `POST /api/data-quality/run/:ruleId` - Run specific check
- `GET /api/data-quality/dashboard` - Quality dashboard metrics

### Implementation Notes

1. **Validation Types:**
   - **Completeness:** Check for NULL/missing required fields
   - **Accuracy:** Validate against known patterns (email, phone, etc.)
   - **Consistency:** Cross-field validation (e.g., transaction_date <= settlement_date)
   - **Timeliness:** Check data freshness (records within expected time window)
   - **Validity:** Range checks, enum validation

2. **Scheduled Execution:**
   - Run quality checks every 15 minutes via background thread
   - Store results in data_quality_checks table
   - Trigger alerts if quality score drops below threshold

---

## 📦 Feature 4: Regulatory Chatbot

### Frontend Location
- **Page:** `frontend/src/pages/RegulatoryChatbot.tsx`
- **Hook:** `frontend/src/hooks/useChatbot.ts`

### Database Schema Additions

```sql
-- Chatbot conversation sessions
CREATE TABLE IF NOT EXISTS chatbot_sessions (
    session_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id VARCHAR(255) NOT NULL,
    session_title VARCHAR(255),
    started_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    last_activity_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    is_active BOOLEAN DEFAULT true,
    session_metadata JSONB
);

-- Chatbot messages
CREATE TABLE IF NOT EXISTS chatbot_messages (
    message_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    session_id UUID REFERENCES chatbot_sessions(session_id),
    role VARCHAR(20) NOT NULL, -- 'user', 'assistant', 'system'
    content TEXT NOT NULL,
    timestamp TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    sources JSONB, -- Referenced knowledge base entries
    confidence_score DECIMAL(5,2),
    feedback VARCHAR(20) -- 'helpful', 'not_helpful'
);

CREATE INDEX idx_chatbot_messages_session ON chatbot_messages(session_id, timestamp);
```

### Implementation

**Extend existing:** `shared/chatbot/chatbot_service.cpp`

Add methods:
- `handle_regulatory_query()` - Specialized regulatory Q&A
- `search_regulatory_knowledge()` - Search knowledge base with regulatory focus
- `cite_sources()` - Return references for compliance audit trail

**Integration points:**
- Use existing `ChatbotService` class from line 1374 in server_with_auth.cpp
- Add regulatory-specific prompt engineering
- Connect to `VectorKnowledgeBase` for RAG
- Store all conversations for audit compliance

---

## 📦 Feature 5: NL Policy Builder

### Frontend Location
- **Page:** `frontend/src/pages/NLPolicyBuilder.tsx`
- **Hook:** `frontend/src/hooks/useNLPolicies.ts`

### Database Schema Additions

```sql
-- NL policy conversion history
CREATE TABLE IF NOT EXISTS nl_policy_conversions (
    conversion_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id VARCHAR(255) NOT NULL,
    natural_language_input TEXT NOT NULL,
    generated_policy JSONB NOT NULL,
    policy_type VARCHAR(50) NOT NULL, -- 'fraud_rule', 'compliance_rule', 'validation_rule'
    confidence_score DECIMAL(5,2),
    status VARCHAR(50) DEFAULT 'draft', -- 'draft', 'approved', 'deployed'
    deployed_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    feedback VARCHAR(500) -- User feedback on accuracy
);

CREATE INDEX idx_nl_policy_conversions_user ON nl_policy_conversions(user_id, created_at DESC);
```

### API Endpoints

- `POST /api/policy/nl-convert` - Convert natural language to policy
- `GET /api/policy/conversions/:userId` - User's conversion history
- `POST /api/policy/conversions/:id/deploy` - Deploy converted policy
- `POST /api/policy/conversions/:id/feedback` - Submit feedback

### Implementation

**File to create:** `shared/policy/nl_policy_converter.cpp`

**Key logic:**
1. Send natural language to LLM with structured prompt
2. LLM returns JSON policy definition
3. Validate generated policy against schema
4. Store in conversions table
5. User reviews and can deploy to fraud_rules or compliance_rules tables

**Example prompt:**
```
Convert this natural language policy to a structured fraud detection rule:

"Flag any transaction over $10,000 to a high-risk country"

Return JSON with:
{
  "rule_type": "threshold",
  "conditions": [...],
  "actions": [...]
}
```

---

## 📦 Feature 6-13: Abbreviated Specifications

### Feature 6: Regulatory Simulator
- **Database:** `regulatory_simulations`, `simulation_scenarios`, `simulation_results`
- **Endpoints:** `/api/simulations/*`
- **Logic:** Run "what-if" analysis on regulatory changes
- **Implementation:** Create simulation engine that applies regulatory changes to historical data

### Feature 7: LLM Key Management
- **Database:** `api_keys_inventory`, `key_rotation_history`
- **Endpoints:** `/api/llm/keys/*`
- **Logic:** Manage OpenAI/Anthropic keys, rotation, usage tracking
- **Implementation:** Extend existing config management with key lifecycle

### Feature 8: Memory Management
- **Database:** Extend `agent_memory` table with visualization metadata
- **Endpoints:** `/api/agents/:id/memory`
- **Logic:** Expose agent memory graph for visualization
- **Implementation:** Add read-only endpoints to existing agent system

### Feature 9: Embeddings Explorer
- **Database:** Query existing `knowledge_base` table with vector operations
- **Endpoints:** `/api/embeddings/explore`, `/api/embeddings/visualize`
- **Logic:** t-SNE/UMAP dimensionality reduction for 2D visualization
- **Implementation:** Use Python integration for ML visualization

### Feature 10: Function Calling Debugger
- **Database:** `llm_function_calls`, `function_call_traces`
- **Endpoints:** `/api/llm/debug/*`
- **Logic:** Log and replay LLM function calls for debugging
- **Implementation:** Add instrumentation to existing LLM interface

### Feature 11: MCDA Advanced
- **Database:** `mcda_criteria`, `mcda_decisions`
- **Endpoints:** `/api/decisions/mcda/*`
- **Logic:** Multi-criteria decision analysis with AHP/TOPSIS algorithms
- **Implementation:** Mathematical optimization for complex decisions

### Feature 12: Tool Categories Testing
- **Database:** Query existing `tool_configurations`, `tool_execution_logs`
- **Endpoints:** `/api/tools/test/*`
- **Logic:** Test tool execution with mock data
- **Implementation:** Add test harness to existing tool registry

### Feature 13: Integration Marketplace
- **Database:** `integration_plugins`, `plugin_installations`
- **Endpoints:** `/api/marketplace/*`
- **Logic:** Plugin/extension installation system
- **Implementation:** Plugin loader with sandboxing

---

## 🔧 Implementation Guidelines

### General Patterns to Follow

1. **Database Connections:**
```cpp
auto conn = db_conn_->get_connection();
if (!conn) {
    return R"({"error": "Database connection failed"})";
}
```

2. **Parameterized Queries:**
```cpp
const char* params[2] = {user_id.c_str(), rule_id.c_str()};
PGresult* result = PQexecParams(conn, query.c_str(), 2, nullptr, params, nullptr, nullptr, 0);
```

3. **JSON Responses:**
```cpp
nlohmann::json response = {
    {"success", true},
    {"data", data_object}
};
return response.dump();
```

4. **Error Handling:**
```cpp
try {
    // Implementation
} catch (const std::exception& e) {
    logger_->log(LogLevel::ERROR, "Exception: " + std::string(e.what()));
    return R"({"error": "Internal server error"})";
}
```

5. **Authentication:**
All endpoints must verify JWT token and extract `user_id` before processing.

### Testing Strategy

1. **Unit Tests:**
   - Test each handler method independently
   - Mock database connections
   - Validate JSON responses

2. **Integration Tests:**
   - Test with real PostgreSQL database
   - Verify end-to-end workflows
   - Test error conditions

3. **Frontend Testing:**
   - Verify React hooks receive expected data
   - Test loading states
   - Test error handling in UI

### Performance Considerations

1. **Database Indexing:**
   - All tables have proper indexes (see schema sections)
   - Use EXPLAIN ANALYZE for query optimization

2. **Caching:**
   - Cache frequently accessed data (courses, rules) in Redis
   - Set TTL based on data volatility

3. **Pagination:**
   - All list endpoints support `limit` and `offset`
   - Default limit: 50, max: 500

4. **Background Processing:**
   - Long-running tasks (simulations, batch checks) use job queues
   - Return job_id immediately, poll for status

---

## 📊 Progress Tracking

### Completion Checklist

- [ ] **Feature 1: Compliance Training System**
  - [ ] Database schema created
  - [ ] API handlers implemented
  - [ ] Routes registered in main server
  - [ ] Frontend integration tested
  - [ ] Certificate generation working
  - [ ] Quiz scoring accurate

- [ ] **Feature 2: Alert Management System**
  - [ ] Database schema created
  - [ ] API handlers implemented
  - [ ] Alert evaluation engine running
  - [ ] Notification delivery working (email, webhook)
  - [ ] Cooldown logic implemented
  - [ ] Frontend integration tested

- [ ] **Feature 3: Data Quality Monitor**
  - [ ] Database schema created
  - [ ] Quality rules implemented
  - [ ] Background checking operational
  - [ ] Dashboard metrics accurate
  - [ ] Frontend integration tested

- [ ] **Feature 4: Regulatory Chatbot**
  - [ ] Database schema created
  - [ ] RAG integration working
  - [ ] Source citation implemented
  - [ ] Conversation history persisted
  - [ ] Frontend integration tested

- [ ] **Feature 5: NL Policy Builder**
  - [ ] Database schema created
  - [ ] LLM integration working
  - [ ] Policy validation implemented
  - [ ] Deployment workflow tested
  - [ ] Frontend integration tested

- [ ] **Feature 6: Regulatory Simulator**
  - [ ] Database schema created
  - [ ] Simulation engine implemented
  - [ ] Scenario management working
  - [ ] Frontend integration tested

- [ ] **Feature 7: LLM Key Management**
  - [ ] Database schema created
  - [ ] Key rotation logic implemented
  - [ ] Usage tracking working
  - [ ] Frontend integration tested

- [ ] **Feature 8: Memory Management**
  - [ ] Memory visualization API implemented
  - [ ] Graph data correctly formatted
  - [ ] Frontend integration tested

- [ ] **Feature 9: Embeddings Explorer**
  - [ ] Dimensionality reduction implemented
  - [ ] Visualization API working
  - [ ] Frontend integration tested

- [ ] **Feature 10: Function Calling Debugger**
  - [ ] Call logging implemented
  - [ ] Replay functionality working
  - [ ] Frontend integration tested

- [ ] **Feature 11: MCDA Advanced**
  - [ ] MCDA algorithms implemented (AHP, TOPSIS)
  - [ ] Criteria management working
  - [ ] Frontend integration tested

- [ ] **Feature 12: Tool Categories Testing**
  - [ ] Test harness implemented
  - [ ] Mock data generation working
  - [ ] Frontend integration tested

- [ ] **Feature 13: Integration Marketplace**
  - [ ] Plugin system architecture designed
  - [ ] Plugin loader implemented
  - [ ] Security sandbox operational
  - [ ] Frontend integration tested

---

## 🚀 Deployment Notes

### Environment Variables to Add

Add to `.env.example`:

```bash
# Training System
TRAINING_CERTIFICATE_BASE_URL=https://yourdomain.com/certificates

# Alert Management
ALERT_EVALUATION_INTERVAL_SECONDS=30
ALERT_MAX_NOTIFICATIONS_PER_HOUR=100

# Data Quality
DATA_QUALITY_CHECK_INTERVAL_MINUTES=15
DATA_QUALITY_MIN_SCORE_THRESHOLD=80

# NL Policy Builder
NL_POLICY_CONFIDENCE_THRESHOLD=0.85

# Simulator
SIMULATION_MAX_SCENARIOS=10
SIMULATION_TIMEOUT_MINUTES=30
```

### Database Migrations

Run all SQL schema additions in order:
1. Compliance Training tables
2. Alert Management tables
3. Data Quality tables
4. Chatbot tables
5. NL Policy tables
6. Simulator tables
7. Remaining feature tables

### Post-Deployment Verification

1. Check logs for initialization messages
2. Verify all routes respond with 200 OK
3. Test one workflow end-to-end for each feature
4. Monitor database connection pool usage
5. Verify background threads are running

---

## 📚 Reference Documentation

### Existing Codebase Patterns

**Study these files for reference:**
- `shared/fraud_detection/fraud_api_handlers.cpp` - API handler pattern
- `shared/transactions/transaction_api_handlers.cpp` - Transaction processing
- `server_with_auth.cpp` - Route registration pattern (lines 3400-16500)
- `shared/rules/advanced_rule_engine.cpp` - Rule execution pattern
- `shared/chatbot/chatbot_service.cpp` - LLM integration pattern

### External Documentation

- PostgreSQL parameterized queries: https://www.postgresql.org/docs/current/libpq-exec.html
- nlohmann/json library: https://github.com/nlohmann/json
- OpenAI API: https://platform.openai.com/docs/api-reference
- Anthropic Claude API: https://docs.anthropic.com/claude/reference

---

## 🎯 Success Criteria

**When complete, you should be able to:**

1. ✅ Navigate to all 13 frontend pages and see real data
2. ✅ Execute all workflows without mock/placeholder messages
3. ✅ See database tables populated with real data
4. ✅ Verify all API endpoints return proper JSON responses
5. ✅ Confirm no "Not implemented" or "Coming soon" messages in UI
6. ✅ Pass integration tests for all features
7. ✅ Audit logs show all user actions
8. ✅ System handles errors gracefully without crashes

---

## 📞 Support

**Questions or Issues?**

1. Check existing implementations in codebase first
2. Review PostgreSQL docs for database issues
3. Test endpoints with curl/Postman before frontend integration
4. Use structured logging for debugging

**Common Pitfalls:**
- Forgetting to parameterize SQL queries (CRITICAL for security)
- Not checking PQresultStatus before accessing results
- Missing PQclear() calls (memory leaks)
- Not handling NULL values from database
- Forgetting to add routes to main server file

---

**Good luck! This implementation will bring the Regulens system to 100% production-grade completion.** 🚀
