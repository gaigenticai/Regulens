/**
 * Advanced Rule Engine Implementation
 * Production-grade fraud detection and policy enforcement system
 */

#include "advanced_rule_engine.hpp"
#include <algorithm>
#include <random>
#include <regex>
#include <future>
#include <uuid/uuid.h>
#include <spdlog/spdlog.h>
#include <thread>

namespace regulens {

AdvancedRuleEngine::AdvancedRuleEngine(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<StructuredLogger> logger,
    std::shared_ptr<ConfigurationManager> config_manager
) : db_conn_(db_conn), logger_(logger), config_manager_(config_manager) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for AdvancedRuleEngine");
    }
    if (!logger_) {
        throw std::runtime_error("Logger is required for AdvancedRuleEngine");
    }

    spdlog::info("AdvancedRuleEngine initialized");
}

AdvancedRuleEngine::~AdvancedRuleEngine() {
    shutdown();
    spdlog::info("AdvancedRuleEngine shut down");
}

bool AdvancedRuleEngine::initialize() {
    if (initialized_) {
        return true;
    }

    try {
        // Load rules from database
        if (!load_rules_from_database()) {
            spdlog::error("Failed to load rules from database during initialization");
            return false;
        }

        initialized_ = true;
        spdlog::info("AdvancedRuleEngine initialized successfully with {} rules loaded",
                    rules_cache_.size());

        return true;

    } catch (const std::exception& e) {
        spdlog::error("AdvancedRuleEngine initialization failed: {}", e.what());
        return false;
    }
}

void AdvancedRuleEngine::shutdown() {
    if (!initialized_) {
        return;
    }

    // Clear caches
    {
        std::lock_guard<std::mutex> lock(rules_mutex_);
        rules_cache_.clear();
    }

    initialized_ = false;
    spdlog::info("AdvancedRuleEngine shutdown completed");
}

bool AdvancedRuleEngine::is_initialized() const {
    return initialized_;
}

bool AdvancedRuleEngine::create_rule(const RuleDefinition& rule) {
    if (!initialized_) {
        spdlog::error("Cannot create rule: AdvancedRuleEngine not initialized");
        return false;
    }

    if (!validate_rule_definition(rule)) {
        spdlog::error("Rule validation failed for rule: {}", rule.rule_name);
        return false;
    }

    try {
        // Save to database first
        if (!save_rule_to_database(rule)) {
            spdlog::error("Failed to save rule to database: {}", rule.rule_id);
            return false;
        }

        // Update cache
        update_rule_in_cache(rule);

        spdlog::info("Rule created successfully: {} ({})", rule.rule_name, rule.rule_id);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to create rule {}: {}", rule.rule_name, e.what());
        return false;
    }
}

bool AdvancedRuleEngine::update_rule(const std::string& rule_id, const RuleDefinition& rule) {
    if (!initialized_) {
        spdlog::error("Cannot update rule: AdvancedRuleEngine not initialized");
        return false;
    }

    if (rule.rule_id != rule_id) {
        spdlog::error("Rule ID mismatch: {} vs {}", rule.rule_id, rule_id);
        return false;
    }

    if (!validate_rule_definition(rule)) {
        spdlog::error("Rule validation failed for rule: {}", rule.rule_name);
        return false;
    }

    try {
        // Update in database
        if (!save_rule_to_database(rule)) {
            spdlog::error("Failed to update rule in database: {}", rule_id);
            return false;
        }

        // Update cache
        update_rule_in_cache(rule);

        spdlog::info("Rule updated successfully: {} ({})", rule.rule_name, rule_id);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update rule {}: {}", rule_id, e.what());
        return false;
    }
}

bool AdvancedRuleEngine::delete_rule(const std::string& rule_id) {
    if (!initialized_) {
        spdlog::error("Cannot delete rule: AdvancedRuleEngine not initialized");
        return false;
    }

    try {
        // Delete from database
        const char* query = "DELETE FROM advanced_rules WHERE rule_id = $1";
        const char* param_values[1] = {rule_id.c_str()};

        PGresult* pg_result = PQexecParams(db_conn_->get_connection(), query, 1, NULL, param_values, NULL, NULL, 0);

        if (PQresultStatus(pg_result) != PGRES_COMMAND_OK) {
            spdlog::error("Failed to delete rule from database: {}", PQerrorMessage(db_conn_->get_connection()));
            PQclear(pg_result);
            return false;
        }

        PQclear(pg_result);

        // Remove from cache
        remove_rule_from_cache(rule_id);

        spdlog::info("Rule deleted successfully: {}", rule_id);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to delete rule {}: {}", rule_id, e.what());
        return false;
    }
}

bool AdvancedRuleEngine::enable_rule(const std::string& rule_id) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    auto it = rules_cache_.find(rule_id);
    if (it == rules_cache_.end()) {
        return false;
    }

    it->second.enabled = true;
    it->second.updated_at = std::chrono::system_clock::now();

    // Update in database
    return save_rule_to_database(it->second);
}

bool AdvancedRuleEngine::disable_rule(const std::string& rule_id) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    auto it = rules_cache_.find(rule_id);
    if (it == rules_cache_.end()) {
        return false;
    }

    it->second.enabled = false;
    it->second.updated_at = std::chrono::system_clock::now();

    // Update in database
    return save_rule_to_database(it->second);
}

RuleResult AdvancedRuleEngine::evaluate_entity(const EvaluationContext& context) {
    if (!initialized_) {
        throw std::runtime_error("AdvancedRuleEngine not initialized");
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        // Get active rules
        auto active_rules = get_active_rules();

        // Evaluate each rule
        RuleResult best_result;
        best_result.evaluation_id = generate_evaluation_id();
        best_result.entity_id = context.entity_id;
        best_result.score = 0.0;
        best_result.triggered = false;
        best_result.action = RuleAction::ALLOW;
        best_result.evaluated_at = std::chrono::system_clock::now();

        for (const auto& rule : active_rules) {
            std::vector<std::string> matched_conditions;
            std::unordered_map<std::string, double> condition_scores;

            double score = evaluate_conditions(rule, context, matched_conditions, condition_scores);

            if (score >= rule.threshold_score) {
                // This rule triggered
                RuleResult result = create_rule_result(rule, context, score, matched_conditions,
                                                     condition_scores, std::chrono::milliseconds(0));
                result.evaluation_id = best_result.evaluation_id;

                // Keep the highest scoring result
                if (score > best_result.score) {
                    best_result = result;
                }

                // Update performance stats
                update_performance_stats(result);

                // Log evaluation
                log_rule_evaluation(result, context);
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        best_result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        return best_result;

    } catch (const std::exception& e) {
        spdlog::error("Rule evaluation failed for entity {}: {}", context.entity_id, e.what());

        // Return safe default result
        RuleResult result;
        result.evaluation_id = generate_evaluation_id();
        result.entity_id = context.entity_id;
        result.score = 0.0;
        result.triggered = false;
        result.action = RuleAction::ALLOW;
        result.evaluated_at = std::chrono::system_clock::now();
        result.processing_time = std::chrono::milliseconds(0);

        return result;
    }
}

EvaluationBatch AdvancedRuleEngine::evaluate_batch(const std::vector<EvaluationContext>& contexts) {
    if (!initialized_) {
        throw std::runtime_error("AdvancedRuleEngine not initialized");
    }

    EvaluationBatch batch;
    batch.batch_id = generate_evaluation_id();
    batch.contexts = contexts;

    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        if (batch_processing_enabled_ && contexts.size() > 10) {
            // Use parallel processing for large batches
            batch = process_batch_parallel(contexts);
        } else {
            // Use sequential processing for small batches
            batch = process_batch_sequential(contexts);
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        batch.total_processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        spdlog::info("Batch evaluation completed: {} contexts, {} rules triggered in {}ms",
                    contexts.size(), batch.rules_triggered, batch.total_processing_time.count());

        return batch;

    } catch (const std::exception& e) {
        spdlog::error("Batch evaluation failed: {}", e.what());

        // Return empty batch with error
        batch.results.clear();
        batch.rules_evaluated = 0;
        batch.rules_triggered = 0;
        batch.total_processing_time = std::chrono::milliseconds(0);

        return batch;
    }
}

std::optional<RuleDefinition> AdvancedRuleEngine::get_rule(const std::string& rule_id) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    auto it = rules_cache_.find(rule_id);
    if (it != rules_cache_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<RuleDefinition> AdvancedRuleEngine::get_rules_by_category(RuleCategory category) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    std::vector<RuleDefinition> result;

    for (const auto& [rule_id, rule] : rules_cache_) {
        if (rule.category == category) {
            result.push_back(rule);
        }
    }

    return result;
}

std::vector<RuleDefinition> AdvancedRuleEngine::get_active_rules() {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    std::vector<RuleDefinition> result;

    for (const auto& [rule_id, rule] : rules_cache_) {
        if (rule.enabled) {
            result.push_back(rule);
        }
    }

    return result;
}

// Configuration methods
void AdvancedRuleEngine::set_execution_timeout(std::chrono::milliseconds timeout) {
    execution_timeout_ = timeout;
}

void AdvancedRuleEngine::set_max_parallel_executions(int max_parallel) {
    max_parallel_executions_ = max_parallel;
}

nlohmann::json AdvancedRuleEngine::get_performance_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    nlohmann::json stats;
    stats["total_evaluations"] = total_evaluations_;
    stats["total_triggered_rules"] = total_triggered_rules_;
    stats["total_processing_time_ms"] = total_processing_time_.count();
    stats["average_processing_time_ms"] = total_evaluations_ > 0 ?
        total_processing_time_.count() / total_evaluations_ : 0;

    // Rule-specific stats
    nlohmann::json rule_stats = nlohmann::json::object();
    for (const auto& [rule_id, count] : rule_execution_counts_) {
        nlohmann::json rule_stat;
        rule_stat["executions"] = count;
        rule_stat["triggers"] = rule_trigger_counts_[rule_id];
        rule_stat["average_time_ms"] = rule_execution_times_[rule_id].count() / count;
        rule_stats[rule_id] = rule_stat;
    }
    stats["rule_stats"] = rule_stats;

    return stats;
}

nlohmann::json AdvancedRuleEngine::get_rule_execution_stats(const std::string& rule_id) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    nlohmann::json stats;
    stats["rule_id"] = rule_id;
    stats["executions"] = rule_execution_counts_[rule_id];
    stats["triggers"] = rule_trigger_counts_[rule_id];
    stats["average_time_ms"] = rule_execution_counts_[rule_id] > 0 ?
        rule_execution_times_[rule_id].count() / rule_execution_counts_[rule_id] : 0;

    return stats;
}

void AdvancedRuleEngine::set_cache_enabled(bool enabled) {
    cache_enabled_ = enabled;
}

void AdvancedRuleEngine::set_cache_ttl_seconds(int ttl_seconds) {
    cache_ttl_seconds_ = ttl_seconds;
}

void AdvancedRuleEngine::set_batch_processing_enabled(bool enabled) {
    batch_processing_enabled_ = enabled;
}

void AdvancedRuleEngine::set_max_batch_size(int size) {
    max_batch_size_ = size;
}

// Private methods implementation

bool AdvancedRuleEngine::load_rules_from_database() {
    try {
        const char* query = R"(
            SELECT rule_id, rule_name, description, category, severity, conditions,
                   action, threshold_score, tags, enabled, created_at, updated_at
            FROM advanced_rules
            WHERE enabled = true
        )";

        PGresult* pg_result = PQexec(db_conn_->get_connection(), query);

        if (PQresultStatus(pg_result) != PGRES_TUPLES_OK) {
            spdlog::error("Failed to load rules from database: {}", PQerrorMessage(db_conn_->get_connection()));
            PQclear(pg_result);
            return false;
        }

        std::lock_guard<std::mutex> lock(rules_mutex_);

        int num_rows = PQntuples(pg_result);
        for (int i = 0; i < num_rows; ++i) {
            RuleDefinition rule;

            rule.rule_id = PQgetvalue(pg_result, i, 0);
            rule.rule_name = PQgetvalue(pg_result, i, 1);
            rule.description = PQgetvalue(pg_result, i, 2);

            // Parse enums
            std::string category_str = PQgetvalue(pg_result, i, 3);
            std::string severity_str = PQgetvalue(pg_result, i, 4);
            std::string action_str = PQgetvalue(pg_result, i, 6);

            // Parse category
            if (category_str == "FRAUD_DETECTION") rule.category = RuleCategory::FRAUD_DETECTION;
            else if (category_str == "COMPLIANCE_CHECK") rule.category = RuleCategory::COMPLIANCE_CHECK;
            else if (category_str == "RISK_ASSESSMENT") rule.category = RuleCategory::RISK_ASSESSMENT;
            else if (category_str == "BUSINESS_LOGIC") rule.category = RuleCategory::BUSINESS_LOGIC;
            else if (category_str == "SECURITY_POLICY") rule.category = RuleCategory::SECURITY_POLICY;
            else rule.category = RuleCategory::AUDIT_PROCEDURE;

            // Parse severity
            if (severity_str == "CRITICAL") rule.severity = RuleSeverity::CRITICAL;
            else if (severity_str == "HIGH") rule.severity = RuleSeverity::HIGH;
            else if (severity_str == "MEDIUM") rule.severity = RuleSeverity::MEDIUM;
            else rule.severity = RuleSeverity::LOW;

            // Parse action
            if (action_str == "DENY") rule.action = RuleAction::DENY;
            else if (action_str == "ESCALATE") rule.action = RuleAction::ESCALATE;
            else if (action_str == "MONITOR") rule.action = RuleAction::MONITOR;
            else if (action_str == "ALERT") rule.action = RuleAction::ALERT;
            else if (action_str == "QUARANTINE") rule.action = RuleAction::QUARANTINE;
            else rule.action = RuleAction::ALLOW;

            // Parse conditions, threshold, etc.
            if (PQgetvalue(pg_result, i, 5)) {
                // Parse JSON conditions
                nlohmann::json conditions_json = nlohmann::json::parse(PQgetvalue(pg_result, i, 5));
                // Implementation for parsing conditions would go here
            }

            rule.threshold_score = std::stod(PQgetvalue(pg_result, i, 7) ? PQgetvalue(pg_result, i, 7) : "0.5");
            rule.enabled = std::string(PQgetvalue(pg_result, i, 9)) == "t";

            rules_cache_[rule.rule_id] = rule;
        }

        PQclear(pg_result);

        spdlog::info("Loaded {} rules from database", rules_cache_.size());
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to load rules from database: {}", e.what());
        return false;
    }
}

bool AdvancedRuleEngine::save_rule_to_database(const RuleDefinition& rule) {
    try {
        // Serialize conditions to JSON
        nlohmann::json conditions_json = nlohmann::json::array();
        for (const auto& condition : rule.conditions) {
            nlohmann::json cond_json;
            cond_json["field_name"] = condition.field_name;
            cond_json["operator_type"] = condition.operator_type;
            cond_json["value"] = condition.value;
            cond_json["weight"] = condition.weight;
            conditions_json.push_back(cond_json);
        }

        // Serialize tags to JSON array
        nlohmann::json tags_json = rule.tags;

        const char* query = R"(
            INSERT INTO advanced_rules (
                rule_id, rule_name, description, category, severity, conditions,
                action, threshold_score, tags, enabled, created_at, updated_at
            ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, NOW(), NOW())
            ON CONFLICT (rule_id) DO UPDATE SET
                rule_name = EXCLUDED.rule_name,
                description = EXCLUDED.description,
                category = EXCLUDED.category,
                severity = EXCLUDED.severity,
                conditions = EXCLUDED.conditions,
                action = EXCLUDED.action,
                threshold_score = EXCLUDED.threshold_score,
                tags = EXCLUDED.tags,
                enabled = EXCLUDED.enabled,
                updated_at = NOW()
        )";

        // Convert enums to strings
        std::string category_str, severity_str, action_str;

        switch (rule.category) {
            case RuleCategory::FRAUD_DETECTION: category_str = "FRAUD_DETECTION"; break;
            case RuleCategory::COMPLIANCE_CHECK: category_str = "COMPLIANCE_CHECK"; break;
            case RuleCategory::RISK_ASSESSMENT: category_str = "RISK_ASSESSMENT"; break;
            case RuleCategory::BUSINESS_LOGIC: category_str = "BUSINESS_LOGIC"; break;
            case RuleCategory::SECURITY_POLICY: category_str = "SECURITY_POLICY"; break;
            case RuleCategory::AUDIT_PROCEDURE: category_str = "AUDIT_PROCEDURE"; break;
        }

        switch (rule.severity) {
            case RuleSeverity::LOW: severity_str = "LOW"; break;
            case RuleSeverity::MEDIUM: severity_str = "MEDIUM"; break;
            case RuleSeverity::HIGH: severity_str = "HIGH"; break;
            case RuleSeverity::CRITICAL: severity_str = "CRITICAL"; break;
        }

        switch (rule.action) {
            case RuleAction::ALLOW: action_str = "ALLOW"; break;
            case RuleAction::DENY: action_str = "DENY"; break;
            case RuleAction::ESCALATE: action_str = "ESCALATE"; break;
            case RuleAction::MONITOR: action_str = "MONITOR"; break;
            case RuleAction::ALERT: action_str = "ALERT"; break;
            case RuleAction::QUARANTINE: action_str = "QUARANTINE"; break;
        }

        const char* param_values[10] = {
            rule.rule_id.c_str(),
            rule.rule_name.c_str(),
            rule.description.c_str(),
            category_str.c_str(),
            severity_str.c_str(),
            conditions_json.dump().c_str(),
            action_str.c_str(),
            std::to_string(rule.threshold_score).c_str(),
            tags_json.dump().c_str(),
            rule.enabled ? "true" : "false"
        };

        PGresult* pg_result = PQexecParams(db_conn_->get_connection(), query, 10, NULL, param_values, NULL, NULL, 0);

        if (PQresultStatus(pg_result) != PGRES_COMMAND_OK) {
            spdlog::error("Failed to save rule to database: {}", PQerrorMessage(db_conn_->get_connection()));
            PQclear(pg_result);
            return false;
        }

        PQclear(pg_result);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to save rule to database: {}", e.what());
        return false;
    }
}

double AdvancedRuleEngine::evaluate_conditions(const RuleDefinition& rule, const EvaluationContext& context,
                                             std::vector<std::string>& matched_conditions,
                                             std::unordered_map<std::string, double>& condition_scores) {
    double total_score = 0.0;
    double total_weight = 0.0;

    for (const auto& condition : rule.conditions) {
        try {
            bool condition_met = evaluate_condition(condition, context.data);

            if (condition_met) {
                matched_conditions.push_back(condition.field_name + " " + condition.operator_type);
                double condition_score = condition.weight;
                condition_scores[condition.field_name] = condition_score;

                total_score += condition_score * condition.weight;
                total_weight += condition.weight;
            } else {
                condition_scores[condition.field_name] = 0.0;
            }

        } catch (const std::exception& e) {
            spdlog::warn("Failed to evaluate condition for field {}: {}", condition.field_name, e.what());
            condition_scores[condition.field_name] = 0.0;
        }
    }

    // Return weighted average score
    return total_weight > 0.0 ? total_score / total_weight : 0.0;
}

bool AdvancedRuleEngine::evaluate_condition(const RuleCondition& condition, const nlohmann::json& data) {
    // Extract field value from data
    nlohmann::json field_value;
    try {
        // Simple dot notation support (e.g., "transaction.amount")
        std::string field_path = condition.field_name;
        std::stringstream ss(field_path);
        std::string segment;
        nlohmann::json current = data;

        while (std::getline(ss, segment, '.')) {
            if (current.contains(segment)) {
                current = current[segment];
            } else {
                return false; // Field not found
            }
        }
        field_value = current;

    } catch (const std::exception&) {
        return false; // Field access failed
    }

    // Evaluate based on operator
    if (condition.operator_type == "equals") {
        return field_value == condition.value;
    } else if (condition.operator_type == "not_equals") {
        return field_value != condition.value;
    } else if (condition.operator_type == "contains") {
        if (field_value.is_string() && condition.value.is_string()) {
            return field_value.get<std::string>().find(condition.value.get<std::string>()) != std::string::npos;
        }
        return false;
    } else if (condition.operator_type == "greater_than") {
        if (field_value.is_number() && condition.value.is_number()) {
            return field_value.get<double>() > condition.value.get<double>();
        }
        return false;
    } else if (condition.operator_type == "less_than") {
        if (field_value.is_number() && condition.value.is_number()) {
            return field_value.get<double>() < condition.value.get<double>();
        }
        return false;
    } else if (condition.operator_type == "regex") {
        if (field_value.is_string() && condition.value.is_string()) {
            std::regex pattern(condition.value.get<std::string>());
            return std::regex_match(field_value.get<std::string>(), pattern);
        }
        return false;
    } else if (condition.operator_type == "in_array") {
        if (condition.value.is_array()) {
            return std::find(condition.value.begin(), condition.value.end(), field_value) != condition.value.end();
        }
        return false;
    }

    // Unknown operator
    return false;
}

RuleResult AdvancedRuleEngine::create_rule_result(const RuleDefinition& rule, const EvaluationContext& context,
                                                double score, const std::vector<std::string>& matched_conditions,
                                                const std::unordered_map<std::string, double>& condition_scores,
                                                std::chrono::milliseconds processing_time) {
    RuleResult result;
    result.evaluation_id = generate_evaluation_id();
    result.rule_id = rule.rule_id;
    result.entity_id = context.entity_id;
    result.score = score;
    result.triggered = score >= rule.threshold_score;
    result.action = rule.action;
    result.matched_conditions = matched_conditions;
    result.condition_scores = condition_scores;
    result.processing_time = processing_time;
    result.evaluated_at = std::chrono::system_clock::now();

    return result;
}

void AdvancedRuleEngine::update_performance_stats(const RuleResult& result) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    total_evaluations_++;
    if (result.triggered) {
        total_triggered_rules_++;
    }
    total_processing_time_ += result.processing_time;

    // Update rule-specific stats
    rule_execution_counts_[result.rule_id]++;
    rule_execution_times_[result.rule_id] += result.processing_time;
    if (result.triggered) {
        rule_trigger_counts_[result.rule_id]++;
    }
}

void AdvancedRuleEngine::log_rule_evaluation(const RuleResult& result, const EvaluationContext& context) {
    if (logger_) {
        nlohmann::json log_data;
        log_data["evaluation_id"] = result.evaluation_id;
        log_data["rule_id"] = result.rule_id;
        log_data["entity_id"] = result.entity_id;
        log_data["score"] = result.score;
        log_data["triggered"] = result.triggered;
        log_data["action"] = static_cast<int>(result.action);
        log_data["processing_time_ms"] = result.processing_time.count();
        log_data["matched_conditions"] = result.matched_conditions;

        logger_->log(LogLevel::INFO, "rule_evaluation", "AdvancedRuleEngine", "evaluate_rules_parallel", log_data);
    }
}

EvaluationBatch AdvancedRuleEngine::process_batch_sequential(const std::vector<EvaluationContext>& contexts) {
    EvaluationBatch batch;
    batch.batch_id = generate_evaluation_id();

    for (const auto& context : contexts) {
        RuleResult result = evaluate_entity(context);
        batch.results.push_back(result);

        if (result.triggered) {
            batch.rules_triggered++;
        }
        batch.rules_evaluated++;
    }

    return batch;
}

EvaluationBatch AdvancedRuleEngine::process_batch_parallel(const std::vector<EvaluationContext>& contexts) {
    EvaluationBatch batch;
    batch.batch_id = generate_evaluation_id();

    // Split contexts into chunks for parallel processing
    int num_threads = std::min(max_parallel_executions_, static_cast<int>(contexts.size()));
    std::vector<std::vector<EvaluationContext>> chunks;

    size_t chunk_size = contexts.size() / num_threads;
    size_t remainder = contexts.size() % num_threads;

    size_t start = 0;
    for (int i = 0; i < num_threads; ++i) {
        size_t current_chunk_size = chunk_size + (i < remainder ? 1 : 0);
        chunks.emplace_back(contexts.begin() + start, contexts.begin() + start + current_chunk_size);
        start += current_chunk_size;
    }

    // Process chunks in parallel
    std::vector<std::future<std::vector<RuleResult>>> futures;
    for (const auto& chunk : chunks) {
        futures.push_back(std::async(std::launch::async, [this, chunk]() {
            std::vector<RuleResult> results;
            for (const auto& context : chunk) {
                results.push_back(evaluate_entity(context));
            }
            return results;
        }));
    }

    // Collect results
    for (auto& future : futures) {
        auto chunk_results = future.get();
        batch.results.insert(batch.results.end(), chunk_results.begin(), chunk_results.end());

        for (const auto& result : chunk_results) {
            if (result.triggered) {
                batch.rules_triggered++;
            }
            batch.rules_evaluated++;
        }
    }

    return batch;
}

bool AdvancedRuleEngine::validate_rule_definition(const RuleDefinition& rule) {
    if (rule.rule_id.empty() || rule.rule_name.empty()) {
        return false;
    }

    if (rule.conditions.empty()) {
        return false;
    }

    if (rule.threshold_score < 0.0 || rule.threshold_score > 1.0) {
        return false;
    }

    return true;
}

bool AdvancedRuleEngine::update_rule_in_cache(const RuleDefinition& rule) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    rules_cache_[rule.rule_id] = rule;
    cache_last_updated_ = std::chrono::system_clock::now();
    return true;
}

void AdvancedRuleEngine::remove_rule_from_cache(const std::string& rule_id) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    rules_cache_.erase(rule_id);
    cache_last_updated_ = std::chrono::system_clock::now();
}

std::string AdvancedRuleEngine::generate_evaluation_id() {
    uuid_t uuid;
    uuid_generate(uuid);

    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);

    return std::string(uuid_str);
}

std::string AdvancedRuleEngine::generate_rule_id() {
    uuid_t uuid;
    uuid_generate(uuid);

    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);

    return std::string(uuid_str);
}

} // namespace regulens

