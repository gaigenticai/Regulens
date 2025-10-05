#include "decision_audit_trail.hpp"
#include <pqxx/pqxx>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <random>

namespace regulens {

DecisionAuditTrailManager::DecisionAuditTrailManager(
    std::shared_ptr<ConnectionPool> db_pool,
    StructuredLogger* logger
) : db_pool_(db_pool), logger_(logger) {
}

DecisionAuditTrailManager::~DecisionAuditTrailManager() {
    shutdown();
}

bool DecisionAuditTrailManager::initialize() {
    logger_->log(LogLevel::INFO, "Initializing Decision Audit Trail Manager");

    try {
        // Verify database connection and create tables if needed
        auto conn = db_pool_->get_connection();
        if (!conn) {
            logger_->log(LogLevel::ERROR, "Failed to get database connection for audit initialization");
            return false;
        }

        // Tables are already created in schema.sql, just verify they exist
        std::string check_query = "SELECT COUNT(*) FROM information_schema.tables WHERE table_name IN ('decision_audit_trails', 'decision_steps', 'decision_explanations', 'human_reviews')";

        auto result = conn->execute_query_single(check_query);
        if (!result) {
            logger_->log(LogLevel::ERROR, "Failed to verify audit tables exist");
            return false;
        }

        int table_count = std::stoi(std::string((*result)["count"]));
        if (table_count < 4) {
            logger_->log(LogLevel::ERROR, "Not all required audit tables exist. Expected 4, found: " + std::to_string(table_count));
            return false;
        }

        logger_->log(LogLevel::INFO, "Decision Audit Trail Manager initialized successfully");
        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to initialize Decision Audit Trail Manager: " + std::string(e.what()));
        return false;
    }
}

void DecisionAuditTrailManager::shutdown() {
    logger_->log(LogLevel::INFO, "Shutting down Decision Audit Trail Manager");

    // Finalize any remaining active trails
    std::lock_guard<std::mutex> lock(trails_mutex_);
    for (auto& [decision_id, trail] : active_trails_) {
        if (trail.completed_at == std::chrono::system_clock::time_point{}) {
            // Trail was never completed, mark as interrupted
            trail.final_decision = nlohmann::json{{"status", "interrupted"}, {"reason", "system_shutdown"}};
            trail.completed_at = std::chrono::system_clock::now();
            trail.total_processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                trail.completed_at - trail.started_at
            );

            update_decision_trail(trail);

            // Store any pending steps for this trail
            std::lock_guard<std::mutex> steps_lock(steps_mutex_);
            auto steps_it = pending_steps_.find(decision_id);
            if (steps_it != pending_steps_.end()) {
                for (const auto& step : steps_it->second) {
                    store_decision_step(step);
                }
                pending_steps_.erase(steps_it);
            }
        }
    }

    active_trails_.clear();
    pending_steps_.clear();
    logger_->log(LogLevel::INFO, "Decision Audit Trail Manager shutdown complete");
}

std::string DecisionAuditTrailManager::start_decision_audit(
    const std::string& agent_type,
    const std::string& agent_name,
    const std::string& trigger_event,
    const nlohmann::json& input_data
) {
    std::string decision_id = generate_unique_id();

    DecisionAuditTrail trail;
    trail.trail_id = generate_unique_id();
    trail.decision_id = decision_id;
    trail.agent_type = agent_type;
    trail.agent_name = agent_name;
    trail.trigger_event = trigger_event;
    trail.original_input = input_data;
    trail.started_at = std::chrono::system_clock::now();
    trail.final_confidence = DecisionConfidence::MEDIUM; // Default
    trail.requires_human_review = false;

    {
        std::lock_guard<std::mutex> lock(trails_mutex_);
        active_trails_[decision_id] = trail;
    }

    // Record the initial step
    record_decision_step(
        decision_id,
        AuditEventType::DECISION_STARTED,
        "Decision audit trail initialized",
        input_data,
        nlohmann::json{{"decision_id", decision_id}, {"trail_id", trail.trail_id}},
        nlohmann::json{{"agent_type", agent_type}, {"agent_name", agent_name}}
    );

    total_decisions_audited_++;
    logger_->log(LogLevel::INFO, "Started decision audit trail: " + decision_id + " for agent: " + agent_name);

    return decision_id;
}

bool DecisionAuditTrailManager::record_decision_step(
    const std::string& decision_id,
    AuditEventType event_type,
    const std::string& description,
    const nlohmann::json& input_data,
    const nlohmann::json& output_data,
    const nlohmann::json& metadata
) {
    auto start_time = std::chrono::high_resolution_clock::now();

    DecisionStep step;
    step.step_id = generate_unique_id();
    step.event_type = event_type;
    step.description = description;
    step.input_data = input_data;
    step.output_data = output_data;
    step.metadata = metadata;
    step.timestamp = std::chrono::system_clock::now();
    step.decision_id = decision_id;

    // Calculate sophisticated confidence impact based on step type, data quality, and context
    step.confidence_impact = calculate_confidence_impact(event_type, input_data, output_data, metadata);

    // Store in active trail
    {
        std::lock_guard<std::mutex> lock(trails_mutex_);
        auto it = active_trails_.find(decision_id);
        if (it != active_trails_.end()) {
            it->second.steps.push_back(step);
        }
    }

    // Cache step for later storage (after trail is finalized)
    {
        std::lock_guard<std::mutex> lock(steps_mutex_);
        pending_steps_[decision_id].push_back(step);
    }

    bool stored = true; // Always return true since we're caching

    auto end_time = std::chrono::high_resolution_clock::now();
    step.processing_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    // Update the step with actual processing time
    if (stored) {
        std::lock_guard<std::mutex> lock(trails_mutex_);
        auto it = active_trails_.find(decision_id);
        if (it != active_trails_.end()) {
            auto& steps = it->second.steps;
            if (!steps.empty()) {
                steps.back().processing_time = step.processing_time;
            }
        }
    }

    logger_->log(LogLevel::DEBUG, "Recorded decision step: " + description + " for decision: " + decision_id);

    return stored;
}

bool DecisionAuditTrailManager::finalize_decision_audit(
    const std::string& decision_id,
    const nlohmann::json& final_decision,
    DecisionConfidence confidence,
    const nlohmann::json& decision_tree,
    const nlohmann::json& risk_assessment,
    const nlohmann::json& alternative_options
) {
    std::lock_guard<std::mutex> lock(trails_mutex_);
    auto it = active_trails_.find(decision_id);
    if (it == active_trails_.end()) {
        logger_->log(LogLevel::ERROR, "Cannot finalize unknown decision: " + decision_id);
        return false;
    }

    auto& trail = it->second;
    trail.final_decision = final_decision;
    trail.final_confidence = confidence;
    trail.decision_tree = decision_tree;
    trail.risk_assessment = risk_assessment;
    trail.alternative_options = alternative_options;
    trail.completed_at = std::chrono::system_clock::now();
    trail.total_processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        trail.completed_at - trail.started_at
    );

    // Calculate overall confidence from steps if not explicitly provided
    if (trail.steps.size() > 1) {
        DecisionConfidence calculated_confidence = calculate_overall_confidence(trail.steps);
        if (confidence == DecisionConfidence::MEDIUM) { // If default was used
            trail.final_confidence = calculated_confidence;
        }
    }

    // Determine if human review is needed
    trail.requires_human_review = should_request_human_review(trail);
    if (trail.requires_human_review) {
        decisions_requiring_review_++;
        trail.human_review_reason = generate_human_review_reason(trail);
    }

    // Store final trail
    bool success = update_decision_trail(trail);

    // Store all pending steps now that the trail record exists
    if (success) {
        std::lock_guard<std::mutex> steps_lock(steps_mutex_);
        auto steps_it = pending_steps_.find(decision_id);
        if (steps_it != pending_steps_.end()) {
            for (const auto& step : steps_it->second) {
                if (!store_decision_step(step)) {
                    logger_->log(LogLevel::ERROR, "Failed to store decision step after trail finalization: " + step.step_id);
                }
            }
            pending_steps_.erase(steps_it);
        }
    }

    // Generate and store explanation
    if (success) {
        auto explanation = generate_explanation(decision_id, ExplanationLevel::DETAILED);
        if (explanation) {
            store_decision_explanation(*explanation);
        }
    }

    // Remove from active trails
    active_trails_.erase(it);

    logger_->log(LogLevel::INFO, "Finalized decision audit trail: " + decision_id +
                 " (confidence: " + std::to_string(static_cast<int>(trail.final_confidence)) +
                 ", human review: " + (trail.requires_human_review ? "yes" : "no") + ")");

    return success;
}

std::optional<DecisionExplanation> DecisionAuditTrailManager::generate_explanation(
    const std::string& decision_id,
    ExplanationLevel level
) {
    auto trail_opt = get_decision_audit(decision_id);
    if (!trail_opt) {
        return std::nullopt;
    }

    const auto& trail = *trail_opt;

    DecisionExplanation explanation;
    explanation.explanation_id = generate_unique_id();
    explanation.decision_id = decision_id;
    explanation.level = level;
    explanation.generated_at = std::chrono::system_clock::now();

    explanation.natural_language_summary = generate_natural_language_summary(trail, level);
    explanation.key_factors = extract_key_factors(trail);
    explanation.risk_indicators = identify_risk_indicators(trail);
    explanation.confidence_factors = analyze_confidence_factors(trail);
    explanation.decision_flowchart = build_decision_flowchart(trail);
    explanation.human_readable_reasoning = generate_detailed_reasoning(trail);

    // Technical details for debugging/analysis
    explanation.technical_details = nlohmann::json{
        {"total_steps", trail.steps.size()},
        {"processing_time_ms", trail.total_processing_time.count()},
        {"agent_type", trail.agent_type},
        {"agent_name", trail.agent_name},
        {"final_confidence", static_cast<int>(trail.final_confidence)},
        {"requires_human_review", trail.requires_human_review}
    };

    return explanation;
}

std::optional<DecisionAuditTrail> DecisionAuditTrailManager::get_decision_audit(
    const std::string& decision_id
) {
    // Check active trails first
    {
        std::lock_guard<std::mutex> lock(trails_mutex_);
        auto it = active_trails_.find(decision_id);
        if (it != active_trails_.end()) {
            return it->second;
        }
    }

    // Load from database
    return load_decision_trail(decision_id);
}

std::vector<DecisionAuditTrail> DecisionAuditTrailManager::get_agent_decisions(
    const std::string& agent_type,
    const std::string& agent_name,
    std::chrono::system_clock::time_point since
) {
    std::vector<DecisionAuditTrail> trails;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return trails;

        std::string query = R"(
            SELECT trail_id FROM decision_audit_trails
            WHERE agent_type = $1 AND agent_name = $2 AND started_at >= $3
            ORDER BY started_at DESC
        )";

        auto since_str = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
            since.time_since_epoch()).count());

        std::vector<std::string> params = {agent_type, agent_name, since_str};
        auto results = conn->execute_query_multi(query, params);

        for (const auto& row : results) {
            auto trail = load_decision_trail(row["trail_id"]);
            if (trail) {
                trails.push_back(*trail);
            }
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to get agent decisions: " + std::string(e.what()));
    }

    return trails;
}

std::vector<DecisionAuditTrail> DecisionAuditTrailManager::get_decisions_requiring_review() {
    std::vector<DecisionAuditTrail> trails;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return trails;

        std::string query = R"(
            SELECT trail_id FROM decision_audit_trails
            WHERE requires_human_review = true AND completed_at IS NOT NULL
            ORDER BY completed_at DESC
        )";

        auto results = conn->execute_query_multi(query);
        for (const auto& row : results) {
            auto trail = load_decision_trail(row["trail_id"]);
            if (trail) {
                trails.push_back(*trail);
            }
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to get decisions requiring review: " + std::string(e.what()));
    }

    return trails;
}

bool DecisionAuditTrailManager::request_human_review(
    const std::string& decision_id,
    const std::string& reason
) {
    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return false;

        std::string query = R"(
            UPDATE decision_audit_trails
            SET requires_human_review = true, human_review_reason = $2
            WHERE decision_id = $1
        )";

        std::vector<std::string> params = {decision_id, reason};
        bool success = conn->execute_command(query, params);

        if (success) {
            decisions_requiring_review_++;
            logger_->log(LogLevel::INFO, "Human review requested for decision: " + decision_id);
        }

        return success;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to request human review: " + std::string(e.what()));
        return false;
    }
}

bool DecisionAuditTrailManager::record_human_feedback(
    const std::string& decision_id,
    const std::string& human_feedback,
    bool approved,
    const std::string& reviewer_id
) {
    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return false;

        std::string insert_query = R"(
            INSERT INTO human_reviews (
                review_id, decision_id, reviewer_id, feedback,
                approved, review_timestamp, processing_time_ms
            ) VALUES (
                $1, $2, $3, $4, $5, NOW(), 0
            )
        )";

        std::vector<std::string> params = {
            generate_unique_id(),
            decision_id,
            reviewer_id,
            human_feedback,
            approved ? "true" : "false"
        };

        bool success = conn->execute_command(insert_query, params);

        if (success) {
            human_reviews_completed_++;
            logger_->log(LogLevel::INFO, "Human feedback recorded for decision: " + decision_id +
                         " (approved: " + (approved ? "yes" : "no") + ")");

            // Update the decision trail to reflect the review
            std::string update_query = R"(
                UPDATE decision_audit_trails
                SET requires_human_review = false
                WHERE decision_id = $1
            )";

            conn->execute_command(update_query, {decision_id});
        }

        return success;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to record human feedback: " + std::string(e.what()));
        return false;
    }
}

// Private implementation methods

bool DecisionAuditTrailManager::store_decision_step(const DecisionStep& step) {
    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return false;

        std::string query = R"(
            INSERT INTO decision_steps (
                step_id, decision_id, event_type, description,
                input_data, output_data, metadata, processing_time_us,
                confidence_impact, timestamp, agent_id
            ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11)
        )";

        std::vector<std::string> params = {
            step.step_id,
            step.decision_id,
            std::to_string(static_cast<int>(step.event_type)),
            step.description,
            step.input_data.dump(),
            step.output_data.dump(),
            step.metadata.dump(),
            std::to_string(step.processing_time.count()),
            std::to_string(step.confidence_impact),
            std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                step.timestamp.time_since_epoch()).count()),
            step.agent_id
        };

        return conn->execute_command(query, params);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to store decision step: " + std::string(e.what()));
        return false;
    }
}

bool DecisionAuditTrailManager::update_decision_trail(const DecisionAuditTrail& trail) {
    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return false;

        std::string query = R"(
            INSERT INTO decision_audit_trails (
                trail_id, decision_id, agent_type, agent_name,
                trigger_event, original_input, final_decision,
                final_confidence, decision_tree, risk_assessment,
                alternative_options, started_at, completed_at,
                total_processing_time_ms, requires_human_review, human_review_reason
            ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16)
            ON CONFLICT (decision_id) DO UPDATE SET
                final_decision = EXCLUDED.final_decision,
                final_confidence = EXCLUDED.final_confidence,
                decision_tree = EXCLUDED.decision_tree,
                risk_assessment = EXCLUDED.risk_assessment,
                alternative_options = EXCLUDED.alternative_options,
                completed_at = EXCLUDED.completed_at,
                total_processing_time_ms = EXCLUDED.total_processing_time_ms,
                requires_human_review = EXCLUDED.requires_human_review,
                human_review_reason = EXCLUDED.human_review_reason
        )";

        std::vector<std::string> params = {
            trail.trail_id,
            trail.decision_id,
            trail.agent_type,
            trail.agent_name,
            trail.trigger_event,
            trail.original_input.dump(),
            trail.final_decision.dump(),
            std::to_string(static_cast<int>(trail.final_confidence)),
            trail.decision_tree.dump(),
            trail.risk_assessment.dump(),
            trail.alternative_options.dump(),
            std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                trail.started_at.time_since_epoch()).count()),
            std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                trail.completed_at.time_since_epoch()).count()),
            std::to_string(trail.total_processing_time.count()),
            trail.requires_human_review ? "true" : "false",
            trail.human_review_reason
        };

        return conn->execute_command(query, params);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to update decision trail: " + std::string(e.what()));
        return false;
    }
}

bool DecisionAuditTrailManager::store_decision_explanation(const DecisionExplanation& explanation) {
    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return false;

        std::string query = R"(
            INSERT INTO decision_explanations (
                explanation_id, decision_id, explanation_level,
                natural_language_summary, key_factors, risk_indicators,
                confidence_factors, decision_flowchart, technical_details,
                human_readable_reasoning, generated_at
            ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11)
        )";

        std::vector<std::string> params = {
            explanation.explanation_id,
            explanation.decision_id,
            std::to_string(static_cast<int>(explanation.level)),
            explanation.natural_language_summary,
            nlohmann::json(explanation.key_factors).dump(),
            nlohmann::json(explanation.risk_indicators).dump(),
            nlohmann::json(explanation.confidence_factors).dump(),
            explanation.decision_flowchart.dump(),
            explanation.technical_details.dump(),
            explanation.human_readable_reasoning,
            std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                explanation.generated_at.time_since_epoch()).count())
        };

        return conn->execute_command(query, params);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to store decision explanation: " + std::string(e.what()));
        return false;
    }
}

std::string DecisionAuditTrailManager::generate_unique_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    ss << std::hex;

    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-4";  // Version 4 UUID
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    ss << dis2(gen);  // Variant
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);

    return ss.str();
}

DecisionConfidence DecisionAuditTrailManager::calculate_overall_confidence(const std::vector<DecisionStep>& steps) {
    double total_confidence = 0.0;
    int confidence_steps = 0;

    for (const auto& step : steps) {
        if (step.event_type == AuditEventType::CONFIDENCE_CALCULATION ||
            step.event_type == AuditEventType::RISK_ASSESSMENT) {
            if (step.output_data.contains("confidence_score")) {
                total_confidence += step.output_data["confidence_score"].get<double>();
                confidence_steps++;
            }
        }
    }

    if (confidence_steps == 0) return DecisionConfidence::MEDIUM;

    double avg_confidence = total_confidence / confidence_steps;

    if (avg_confidence < 0.3) return DecisionConfidence::VERY_LOW;
    if (avg_confidence < 0.5) return DecisionConfidence::LOW;
    if (avg_confidence < 0.7) return DecisionConfidence::MEDIUM;
    if (avg_confidence < 0.9) return DecisionConfidence::HIGH;
    return DecisionConfidence::VERY_HIGH;
}

bool DecisionAuditTrailManager::should_request_human_review(const DecisionAuditTrail& trail) {
    // Request human review for high-risk decisions or low confidence
    if (trail.final_confidence == DecisionConfidence::VERY_LOW ||
        trail.final_confidence == DecisionConfidence::LOW) {
        return true;
    }

    // Request review for decisions with high financial impact
    if (trail.final_decision.contains("financial_impact") &&
        trail.final_decision["financial_impact"].get<double>() > 1000000.0) {
        return true;
    }

    // Request review for regulatory compliance decisions
    if (trail.agent_type == "REGULATORY_ASSESSOR") {
        return true;
    }

    return false;
}

std::string DecisionAuditTrailManager::generate_human_review_reason(const DecisionAuditTrail& trail) {
    if (trail.final_confidence == DecisionConfidence::VERY_LOW ||
        trail.final_confidence == DecisionConfidence::LOW) {
        return "Low confidence in decision requires human validation";
    }

    if (trail.final_decision.contains("financial_impact") &&
        trail.final_decision["financial_impact"].get<double>() > 1000000.0) {
        return "High financial impact decision requires human approval";
    }

    if (trail.agent_type == "REGULATORY_ASSESSOR") {
        return "Regulatory compliance decision requires human oversight";
    }

    return "Decision flagged for human review";
}

// Explanation generation helpers
std::string DecisionAuditTrailManager::generate_natural_language_summary(
    const DecisionAuditTrail& trail, ExplanationLevel level
) {
    std::stringstream ss;

    ss << "Agent " << trail.agent_name << " (" << trail.agent_type << ") made a decision ";
    ss << "with " << confidence_to_string(trail.final_confidence) << " confidence ";

    if (level == ExplanationLevel::HIGH_LEVEL) {
        ss << "based on analysis of " << trail.steps.size() << " decision factors.";
    } else {
        ss << "after processing " << trail.steps.size() << " steps in ";
        ss << trail.total_processing_time.count() << " milliseconds.";
    }

    if (trail.requires_human_review) {
        ss << " Human review has been requested.";
    }

    return ss.str();
}

std::vector<std::string> DecisionAuditTrailManager::extract_key_factors(const DecisionAuditTrail& trail) {
    std::vector<std::string> factors;

    for (const auto& step : trail.steps) {
        if (step.event_type == AuditEventType::DATA_RETRIEVAL ||
            step.event_type == AuditEventType::PATTERN_ANALYSIS ||
            step.event_type == AuditEventType::KNOWLEDGE_QUERY) {
            if (step.output_data.contains("key_findings")) {
                factors.push_back(step.output_data["key_findings"].get<std::string>());
            }
        }
    }

    if (factors.empty()) {
        factors.push_back("Decision based on standard operating procedures");
    }

    return factors;
}

std::vector<std::string> DecisionAuditTrailManager::identify_risk_indicators(const DecisionAuditTrail& trail) {
    std::vector<std::string> indicators;

    if (trail.risk_assessment.contains("indicators")) {
        for (const auto& indicator : trail.risk_assessment["indicators"]) {
            indicators.push_back(indicator.get<std::string>());
        }
    }

    // Extract from decision steps
    for (const auto& step : trail.steps) {
        if (step.event_type == AuditEventType::RISK_ASSESSMENT) {
            if (step.output_data.contains("risk_level") &&
                step.output_data["risk_level"].get<std::string>() != "LOW") {
                indicators.push_back("Elevated risk detected in " + step.description);
            }
        }
    }

    return indicators;
}

std::vector<std::string> DecisionAuditTrailManager::analyze_confidence_factors(const DecisionAuditTrail& trail) {
    std::vector<std::string> factors;

    for (const auto& step : trail.steps) {
        if (step.confidence_impact > 0.1) {
            factors.push_back(step.description + " (+" +
                            std::to_string(step.confidence_impact * 100) + "% confidence)");
        } else if (step.confidence_impact < -0.1) {
            factors.push_back(step.description + " (" +
                            std::to_string(step.confidence_impact * 100) + "% confidence)");
        }
    }

    return factors;
}

nlohmann::json DecisionAuditTrailManager::build_decision_flowchart(const DecisionAuditTrail& trail) {
    nlohmann::json flowchart;

    flowchart["nodes"] = nlohmann::json::array();
    flowchart["edges"] = nlohmann::json::array();

    // Add decision steps as nodes
    for (size_t i = 0; i < trail.steps.size(); ++i) {
        const auto& step = trail.steps[i];

        nlohmann::json node = {
            {"id", "step_" + std::to_string(i)},
            {"label", step.description},
            {"type", event_type_to_string(step.event_type)},
            {"data", step.output_data},
            {"confidence_impact", step.confidence_impact}
        };

        flowchart["nodes"].push_back(node);

        // Add edge to next step
        if (i < trail.steps.size() - 1) {
            nlohmann::json edge = {
                {"from", "step_" + std::to_string(i)},
                {"to", "step_" + std::to_string(i + 1)},
                {"label", "next"}
            };
            flowchart["edges"].push_back(edge);
        }
    }

    return flowchart;
}

std::string DecisionAuditTrailManager::generate_detailed_reasoning(const DecisionAuditTrail& trail) {
    std::stringstream ss;

    ss << "Decision Process Analysis:\n\n";
    ss << "Agent: " << trail.agent_name << " (" << trail.agent_type << ")\n";
    ss << "Trigger: " << trail.trigger_event << "\n";
    ss << "Duration: " << trail.total_processing_time.count() << "ms\n";
    ss << "Confidence: " << confidence_to_string(trail.final_confidence) << "\n\n";

    ss << "Decision Steps:\n";
    for (size_t i = 0; i < trail.steps.size(); ++i) {
        const auto& step = trail.steps[i];
        ss << std::to_string(i + 1) << ". " << step.description << "\n";
        ss << "   Type: " << event_type_to_string(step.event_type) << "\n";
        ss << "   Processing: " << step.processing_time.count() << "μs\n";
        if (step.confidence_impact != 0.0) {
            ss << "   Confidence Impact: " << (step.confidence_impact > 0 ? "+" : "") <<
               std::to_string(step.confidence_impact * 100) << "%\n";
        }
        ss << "\n";
    }

    if (trail.requires_human_review) {
        ss << "HUMAN REVIEW REQUIRED: " << trail.human_review_reason << "\n";
    }

    return ss.str();
}

nlohmann::json DecisionAuditTrailManager::get_agent_performance_analytics(
    const std::string& agent_type,
    std::chrono::system_clock::time_point since
) {
    nlohmann::json analytics;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return analytics;

        // Get basic performance metrics
        std::string query = R"(
            SELECT
                COUNT(*) as total_decisions,
                AVG(final_confidence) as avg_confidence,
                COUNT(*) FILTER (WHERE requires_human_review = true) as human_reviews,
                AVG(total_processing_time_ms) as avg_processing_time_ms,
                COUNT(*) FILTER (WHERE final_confidence >= 3) as high_confidence_decisions,
                COUNT(*) FILTER (WHERE final_confidence <= 1) as low_confidence_decisions
            FROM decision_audit_trails
            WHERE agent_type = $1 AND started_at >= $2
        )";

        auto since_str = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
            since.time_since_epoch()).count());

        std::vector<std::string> params = {agent_type, since_str};
        auto result = conn->execute_query_single(query, params);

        if (result) {
            analytics["total_decisions"] = std::stoll(std::string((*result)["total_decisions"]));
            analytics["avg_confidence"] = std::stod(std::string((*result)["avg_confidence"]));
            analytics["human_reviews"] = std::stoll(std::string((*result)["human_reviews"]));
            analytics["avg_processing_time_ms"] = std::stod(std::string((*result)["avg_processing_time_ms"]));
            analytics["high_confidence_decisions"] = std::stoll(std::string((*result)["high_confidence_decisions"]));
            analytics["low_confidence_decisions"] = std::stoll(std::string((*result)["low_confidence_decisions"]));

            // Calculate success rate (decisions with high/very high confidence)
            size_t total = analytics["total_decisions"];
            size_t successful = analytics["high_confidence_decisions"];
            analytics["success_rate"] = total > 0 ? static_cast<double>(successful) / total : 0.0;
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to get performance analytics: " + std::string(e.what()));
    }

    return analytics;
}

nlohmann::json DecisionAuditTrailManager::get_decision_pattern_analysis(
    const std::string& agent_type,
    std::chrono::system_clock::time_point since
) {
    nlohmann::json patterns;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return patterns;

        // Analyze decision patterns
        std::string decision_query = R"(
            SELECT final_decision->>'decision' as decision_type, COUNT(*) as count
            FROM decision_audit_trails
            WHERE agent_type = $1 AND started_at >= $2
            GROUP BY final_decision->>'decision'
            ORDER BY count DESC
            LIMIT 5
        )";

        auto since_str = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
            since.time_since_epoch()).count());

        std::vector<std::string> params = {agent_type, since_str};
        auto results = conn->execute_query_multi(decision_query, params);

        nlohmann::json decision_distribution;
        for (const auto& row : results) {
            decision_distribution[std::string(row["decision_type"])] = std::stoll(std::string(row["count"]));
        }

        // Find most common decision
        std::string most_common = "APPROVE"; // default
        size_t max_count = 0;
        for (const auto& [decision, count] : decision_distribution.items()) {
            if (count > max_count) {
                max_count = count;
                most_common = decision;
            }
        }

        patterns["most_common_decision"] = most_common;
        patterns["decision_distribution"] = decision_distribution;

        // Analyze peak decision times (by hour)
        std::string time_query = R"(
            SELECT EXTRACT(hour from started_at) as decision_hour, COUNT(*) as count
            FROM decision_audit_trails
            WHERE agent_type = $1 AND started_at >= $2
            GROUP BY decision_hour
            ORDER BY count DESC
            LIMIT 1
        )";

        auto time_result = conn->execute_query_single(time_query, params);
        if (time_result) {
            patterns["peak_decision_hour"] = std::stod(std::string((*time_result)["decision_hour"]));
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to get decision pattern analysis: " + std::string(e.what()));
    }

    return patterns;
}

std::vector<nlohmann::json> DecisionAuditTrailManager::get_audit_trail_for_compliance(
    std::chrono::system_clock::time_point start_date,
    std::chrono::system_clock::time_point end_date
) {
    std::vector<nlohmann::json> audit_data;

    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return audit_data;

        std::string query = R"(
            SELECT
                trail_id,
                decision_id,
                agent_type,
                agent_name,
                final_decision,
                final_confidence,
                requires_human_review,
                started_at,
                completed_at,
                total_processing_time_ms
            FROM decision_audit_trails
            WHERE started_at >= $1 AND started_at <= $2
            ORDER BY started_at
        )";

        std::vector<std::string> params = {
            std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                start_date.time_since_epoch()).count()),
            std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                end_date.time_since_epoch()).count())
        };

        auto results = conn->execute_query_multi(query, params);
        for (const auto& row : results) {
            nlohmann::json record;
            record["trail_id"] = std::string(row["trail_id"]);
            record["decision_id"] = std::string(row["decision_id"]);
            record["agent_type"] = std::string(row["agent_type"]);
            record["agent_name"] = std::string(row["agent_name"]);
            record["final_decision"] = nlohmann::json::parse(std::string(row["final_decision"]));
            record["final_confidence"] = std::stoi(std::string(row["final_confidence"]));
            record["requires_human_review"] = std::string(row["requires_human_review"]) == "t";
            record["started_at"] = std::string(row["started_at"]);
            record["completed_at"] = std::string(row["completed_at"]);
            record["total_processing_time_ms"] = std::stoll(std::string(row["total_processing_time_ms"]));

            audit_data.push_back(record);
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to get compliance audit trail: " + std::string(e.what()));
    }

    return audit_data;
}

bool DecisionAuditTrailManager::export_audit_data(
    const std::string& file_path,
    std::chrono::system_clock::time_point start_date,
    std::chrono::system_clock::time_point end_date
) {
    try {
        auto audit_data = get_audit_trail_for_compliance(start_date, end_date);

        nlohmann::json export_json;
        export_json["export_timestamp"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        export_json["start_date"] = std::chrono::system_clock::to_time_t(start_date);
        export_json["end_date"] = std::chrono::system_clock::to_time_t(end_date);
        export_json["total_records"] = audit_data.size();
        export_json["audit_trails"] = audit_data;

        // Write to file
        std::ofstream file(file_path);
        if (!file.is_open()) {
            logger_->log(LogLevel::ERROR, "Failed to open export file: " + file_path);
            return false;
        }

        file << export_json.dump(2);
        file.close();

        logger_->log(LogLevel::INFO, "Exported " + std::to_string(audit_data.size()) +
                     " audit records to " + file_path);

        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to export audit data: " + std::string(e.what()));
        return false;
    }
}

std::optional<DecisionAuditTrail> DecisionAuditTrailManager::load_decision_trail(const std::string& decision_id) {
    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return std::nullopt;

        // Load trail header
        std::string trail_query = "SELECT * FROM decision_audit_trails WHERE decision_id = $1";
        std::vector<std::string> params = {decision_id};

        auto trail_result = conn->execute_query_single(trail_query, params);
        if (!trail_result) return std::nullopt;

        DecisionAuditTrail trail;
        trail.trail_id = (*trail_result)["trail_id"];
        trail.decision_id = (*trail_result)["decision_id"];
        trail.agent_type = (*trail_result)["agent_type"];
        trail.agent_name = (*trail_result)["agent_name"];
        trail.trigger_event = (*trail_result)["trigger_event"];
        trail.original_input = nlohmann::json::parse(std::string((*trail_result)["original_input"]));
        trail.final_decision = nlohmann::json::parse(std::string((*trail_result)["final_decision"]));
        trail.final_confidence = static_cast<DecisionConfidence>(std::stoi(std::string((*trail_result)["final_confidence"])));
        trail.decision_tree = nlohmann::json::parse(std::string((*trail_result)["decision_tree"]));
        trail.risk_assessment = nlohmann::json::parse(std::string((*trail_result)["risk_assessment"]));
        trail.alternative_options = nlohmann::json::parse(std::string((*trail_result)["alternative_options"]));
        trail.started_at = std::chrono::system_clock::time_point(std::chrono::seconds(
            std::stoll(std::string((*trail_result)["started_at"]))));
        trail.completed_at = std::chrono::system_clock::time_point(std::chrono::seconds(
            std::stoll(std::string((*trail_result)["completed_at"]))));
        trail.total_processing_time = std::chrono::milliseconds(std::stoll(std::string((*trail_result)["total_processing_time_ms"])));
        trail.requires_human_review = std::string((*trail_result)["requires_human_review"]) == "t";
        trail.human_review_reason = (*trail_result)["human_review_reason"];

        // Load decision steps
        std::string steps_query = "SELECT step_id FROM decision_steps WHERE decision_id = $1 ORDER BY timestamp";
        auto steps_results = conn->execute_query_multi(steps_query, params);

        for (const auto& row : steps_results) {
            auto step = load_decision_step(row["step_id"]);
            if (step) {
                trail.steps.push_back(*step);
            }
        }

        return trail;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to load decision trail: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<DecisionStep> DecisionAuditTrailManager::load_decision_step(const std::string& step_id) {
    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return std::nullopt;

        std::string query = "SELECT * FROM decision_steps WHERE step_id = $1";
        std::vector<std::string> params = {step_id};

        auto result = conn->execute_query_single(query, params);
        if (!result) return std::nullopt;

        DecisionStep step;
        step.step_id = (*result)["step_id"];
        step.decision_id = (*result)["decision_id"];
        step.event_type = static_cast<AuditEventType>(std::stoi(std::string((*result)["event_type"])));
        step.description = (*result)["description"];
        step.input_data = nlohmann::json::parse(std::string((*result)["input_data"]));
        step.output_data = nlohmann::json::parse(std::string((*result)["output_data"]));
        step.metadata = nlohmann::json::parse(std::string((*result)["metadata"]));
        step.processing_time = std::chrono::microseconds(std::stoll(std::string((*result)["processing_time_us"])));
        step.confidence_impact = std::stod(std::string((*result)["confidence_impact"]));
        step.timestamp = std::chrono::system_clock::time_point(std::chrono::seconds(
            std::stoll(std::string((*result)["timestamp"]))));
        step.agent_id = (*result)["agent_id"];

        return step;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to load decision step: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<DecisionExplanation> DecisionAuditTrailManager::load_decision_explanation(const std::string& explanation_id) {
    try {
        auto conn = db_pool_->get_connection();
        if (!conn) return std::nullopt;

        std::string query = "SELECT * FROM decision_explanations WHERE explanation_id = $1";
        std::vector<std::string> params = {explanation_id};

        auto result = conn->execute_query_single(query, params);
        if (!result) return std::nullopt;

        DecisionExplanation explanation;
        explanation.explanation_id = (*result)["explanation_id"];
        explanation.decision_id = (*result)["decision_id"];
        explanation.level = static_cast<ExplanationLevel>(std::stoi(std::string((*result)["explanation_level"])));
        explanation.natural_language_summary = (*result)["natural_language_summary"];
        explanation.key_factors = nlohmann::json::parse(std::string((*result)["key_factors"])).get<std::vector<std::string>>();
        explanation.risk_indicators = nlohmann::json::parse(std::string((*result)["risk_indicators"])).get<std::vector<std::string>>();
        explanation.confidence_factors = nlohmann::json::parse(std::string((*result)["confidence_factors"])).get<std::vector<std::string>>();
        explanation.decision_flowchart = nlohmann::json::parse(std::string((*result)["decision_flowchart"]));
        explanation.technical_details = nlohmann::json::parse(std::string((*result)["technical_details"]));
        explanation.human_readable_reasoning = (*result)["human_readable_reasoning"];
        explanation.generated_at = std::chrono::system_clock::time_point(std::chrono::seconds(
            std::stoll(std::string((*result)["generated_at"]))));

        return explanation;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to load decision explanation: " + std::string(e.what()));
        return std::nullopt;
    }
}

// Utility functions
std::string DecisionAuditTrailManager::confidence_to_string(DecisionConfidence confidence) {
    switch (confidence) {
        case DecisionConfidence::VERY_LOW: return "Very Low";
        case DecisionConfidence::LOW: return "Low";
        case DecisionConfidence::MEDIUM: return "Medium";
        case DecisionConfidence::HIGH: return "High";
        case DecisionConfidence::VERY_HIGH: return "Very High";
        default: return "Unknown";
    }
}

std::string DecisionAuditTrailManager::event_type_to_string(AuditEventType type) {
    switch (type) {
        case AuditEventType::DECISION_STARTED: return "Decision Started";
        case AuditEventType::DATA_RETRIEVAL: return "Data Retrieval";
        case AuditEventType::PATTERN_ANALYSIS: return "Pattern Analysis";
        case AuditEventType::RISK_ASSESSMENT: return "Risk Assessment";
        case AuditEventType::KNOWLEDGE_QUERY: return "Knowledge Query";
        case AuditEventType::LLM_INFERENCE: return "LLM Inference";
        case AuditEventType::RULE_EVALUATION: return "Rule Evaluation";
        case AuditEventType::CONFIDENCE_CALCULATION: return "Confidence Calculation";
        case AuditEventType::DECISION_FINALIZED: return "Decision Finalized";
        case AuditEventType::HUMAN_REVIEW_REQUESTED: return "Human Review Requested";
        case AuditEventType::HUMAN_FEEDBACK_RECEIVED: return "Human Feedback Received";
        default: return "Unknown";
    }
}

double DecisionAuditTrailManager::calculate_confidence_impact(
    AuditEventType event_type,
    const nlohmann::json& input_data,
    const nlohmann::json& output_data,
    const nlohmann::json& metadata
) {
    double impact = 0.0;

    // Base impact factors by event type
    std::unordered_map<AuditEventType, double> base_impact_factors = {
        {AuditEventType::DATA_RETRIEVAL, 0.05},           // Data quality affects confidence
        {AuditEventType::PATTERN_ANALYSIS, 0.15},         // Pattern recognition is significant
        {AuditEventType::RISK_ASSESSMENT, 0.20},          // Risk assessment is critical
        {AuditEventType::KNOWLEDGE_QUERY, 0.10},          // Knowledge base queries are reliable
        {AuditEventType::LLM_INFERENCE, 0.08},            // LLM outputs vary in reliability
        {AuditEventType::RULE_EVALUATION, 0.12},          // Rule-based decisions are predictable
        {AuditEventType::CONFIDENCE_CALCULATION, 0.25},   // Direct confidence calculation
        {AuditEventType::HUMAN_FEEDBACK_RECEIVED, 0.30},  // Human input is highly reliable
        {AuditEventType::DECISION_STARTED, 0.0},          // No impact
        {AuditEventType::DECISION_FINALIZED, 0.0},        // No impact
        {AuditEventType::HUMAN_REVIEW_REQUESTED, -0.10}   // Requesting review indicates uncertainty
    };

    auto base_it = base_impact_factors.find(event_type);
    if (base_it != base_impact_factors.end()) {
        impact = base_it->second;
    }

    // Adjust impact based on output data quality and content
    if (output_data.contains("confidence_score")) {
        double confidence_score = output_data["confidence_score"].get<double>();
        // Confidence scores directly contribute to impact
        impact += confidence_score * 0.3;
    }

    if (output_data.contains("data_quality_score")) {
        double quality_score = output_data["data_quality_score"].get<double>();
        // High-quality data increases confidence impact
        impact *= (0.8 + 0.4 * quality_score); // Range: 0.8 to 1.2
    }

    if (output_data.contains("consistency_score")) {
        double consistency = output_data["consistency_score"].get<double>();
        // Consistent results increase confidence
        impact *= (0.9 + 0.2 * consistency); // Range: 0.9 to 1.1
    }

    // Adjust for data source reliability
    if (metadata.contains("data_source")) {
        std::string source = metadata["data_source"];
        std::unordered_map<std::string, double> source_reliability = {
            {"primary_database", 1.0},
            {"cache", 0.9},
            {"external_api", 0.8},
            {"user_input", 0.95},
            {"llm_generated", 0.7},
            {"inferred", 0.6}
        };

        auto source_it = source_reliability.find(source);
        if (source_it != source_reliability.end()) {
            impact *= source_it->second;
        }
    }

    // Adjust for processing time (faster processing may indicate more reliable methods)
    if (metadata.contains("processing_time_ms")) {
        double processing_time = metadata["processing_time_ms"].get<double>();
        // Penalize very slow processing (may indicate complex/computationally intensive operations)
        if (processing_time > 5000) { // More than 5 seconds
            impact *= 0.9;
        } else if (processing_time < 100) { // Very fast (may be too simplistic)
            impact *= 0.95;
        }
    }

    // Adjust for error indicators
    if (output_data.contains("error_rate")) {
        double error_rate = output_data["error_rate"].get<double>();
        impact *= (1.0 - error_rate * 0.5); // Reduce impact based on error rate
    }

    if (output_data.contains("warning_count")) {
        int warning_count = output_data["warning_count"].get<int>();
        impact *= std::max(0.7, 1.0 - warning_count * 0.05); // Reduce for warnings
    }

    // Special handling for risk assessment
    if (event_type == AuditEventType::RISK_ASSESSMENT) {
        if (output_data.contains("risk_level")) {
            std::string risk_level = output_data["risk_level"];
            if (risk_level == "CRITICAL" || risk_level == "HIGH") {
                impact *= 0.8; // High risk reduces confidence impact
            } else if (risk_level == "LOW") {
                impact *= 1.1; // Low risk increases confidence impact
            }
        }
    }

    // Special handling for pattern analysis
    if (event_type == AuditEventType::PATTERN_ANALYSIS) {
        if (output_data.contains("pattern_strength")) {
            double pattern_strength = output_data["pattern_strength"].get<double>();
            impact *= (0.7 + 0.6 * pattern_strength); // Stronger patterns increase impact
        }

        if (output_data.contains("sample_size")) {
            int sample_size = output_data["sample_size"].get<int>();
            // Larger sample sizes increase confidence
            double sample_factor = std::min(1.2, 0.8 + sample_size / 1000.0);
            impact *= sample_factor;
        }
    }

    // LLM inference confidence adjustment
    if (event_type == AuditEventType::LLM_INFERENCE) {
        if (output_data.contains("model_confidence")) {
            double model_confidence = output_data["model_confidence"].get<double>();
            impact *= model_confidence;
        }

        if (output_data.contains("temperature")) {
            double temperature = output_data["temperature"].get<double>();
            // Higher temperature (more creative) reduces confidence impact
            impact *= (1.0 - temperature * 0.1);
        }
    }

    // Human feedback is highly reliable
    if (event_type == AuditEventType::HUMAN_FEEDBACK_RECEIVED) {
        if (output_data.contains("approved") && output_data["approved"].get<bool>()) {
            impact = std::abs(impact); // Ensure positive impact for approvals
        } else {
            impact *= -0.5; // Negative impact for disapprovals
        }
    }

    // Clamp impact to reasonable range [-0.5, 0.5]
    impact = std::max(-0.5, std::min(0.5, impact));

    return impact;
}

} // namespace regulens
