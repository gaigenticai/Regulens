/**
 * Advanced Rule Engine Implementation
 * Production-grade fraud detection and policy enforcement system
 */

#include "advanced_rule_engine.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <spdlog/spdlog.h>
#include <cmath>

namespace regulens {

AdvancedRuleEngine::AdvancedRuleEngine(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<StructuredLogger> logger,
    std::shared_ptr<DynamicConfigManager> config_manager
) : db_conn_(db_conn), logger_(logger), config_manager_(config_manager) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for AdvancedRuleEngine");
    }
    if (!logger_) {
        throw std::runtime_error("Logger is required for AdvancedRuleEngine");
    }

    // Load configuration
    load_configuration();

    // Initialize rule cache
    reload_rules();

    if (logger_) {
        logger_->info("AdvancedRuleEngine initialized with fraud detection capabilities");
    }
}

AdvancedRuleEngine::~AdvancedRuleEngine() {
    if (logger_) {
        logger_->info("AdvancedRuleEngine shutting down");
    }
}

RuleExecutionResultDetail AdvancedRuleEngine::execute_rule(
    const RuleDefinition& rule,
    const RuleExecutionContext& context,
    RuleExecutionMode mode
) {
    RuleExecutionResultDetail result;
    result.rule_id = rule.rule_id;
    result.rule_name = rule.name;

    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        // Check if rule is active and within validity period
        if (!rule.is_active) {
            result.result = RuleExecutionResult::SKIPPED;
            result.error_message = "Rule is inactive";
            return result;
        }

        if (rule.valid_from && context.execution_time < *rule.valid_from) {
            result.result = RuleExecutionResult::SKIPPED;
            result.error_message = "Rule not yet valid";
            return result;
        }

        if (rule.valid_until && context.execution_time > *rule.valid_until) {
            result.result = RuleExecutionResult::SKIPPED;
            result.error_message = "Rule has expired";
            return result;
        }

        // Execute based on rule type
        if (rule.rule_type == "VALIDATION") {
            result = execute_validation_rule(rule, context);
        } else if (rule.rule_type == "SCORING") {
            result = execute_scoring_rule(rule, context);
        } else if (rule.rule_type == "PATTERN") {
            result = execute_pattern_rule(rule, context);
        } else if (rule.rule_type == "MACHINE_LEARNING") {
            result = execute_ml_rule(rule, context);
        } else {
            result.result = RuleExecutionResult::ERROR;
            result.error_message = "Unknown rule type: " + rule.rule_type;
        }

        // Calculate confidence score
        result.confidence_score = calculate_rule_confidence(rule, result);

        // Determine risk level
        if (result.result == RuleExecutionResult::FAIL) {
            result.risk_level = score_to_risk_level(result.confidence_score);
        }

    } catch (const std::exception& e) {
        result.result = RuleExecutionResult::ERROR;
        result.error_message = std::string("Rule execution failed: ") + e.what();

        if (logger_) {
            logger_->error("Rule execution failed for rule '{}': {}", rule.rule_id, e.what());
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Update performance metrics
    if (enable_performance_monitoring_) {
        update_rule_metrics(rule.rule_id, result);
        record_execution_time(rule.rule_id, result.execution_time);
    }

    return result;
}

FraudDetectionResult AdvancedRuleEngine::evaluate_transaction(
    const RuleExecutionContext& context,
    const std::vector<std::string>& rule_ids
) {
    FraudDetectionResult result;
    result.transaction_id = context.transaction_id;
    result.detection_time = std::chrono::system_clock::now();

    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        // Determine which rules to execute
        std::vector<RuleDefinition> rules_to_execute;
        if (rule_ids.empty()) {
            // Execute all active rules
            rules_to_execute = get_active_rules();
        } else {
            // Execute specified rules
            for (const auto& rule_id : rule_ids) {
                auto rule_opt = get_rule(rule_id);
                if (rule_opt) {
                    rules_to_execute.push_back(*rule_opt);
                }
            }
        }

        // Sort rules by priority (highest first)
        std::sort(rules_to_execute.begin(), rules_to_execute.end(),
                 [](const RuleDefinition& a, const RuleDefinition& b) {
                     return static_cast<int>(a.priority) > static_cast<int>(b.priority);
                 });

        // Execute rules
        for (const auto& rule : rules_to_execute) {
            auto rule_result = execute_rule(rule, context);
            result.rule_results.push_back(rule_result);

            // Aggregate findings
            if (rule_result.result == RuleExecutionResult::FAIL) {
                result.is_fraudulent = true;
                result.aggregated_findings[rule.rule_id] = {
                    {"rule_name", rule.name},
                    {"confidence", rule_result.confidence_score},
                    {"risk_level", static_cast<int>(rule_result.risk_level)},
                    {"output", rule_result.rule_output},
                    {"triggered_conditions", rule_result.triggered_conditions}
                };
            }
        }

        // Calculate overall risk score and level
        result.fraud_score = calculate_aggregated_risk_score(result.rule_results);
        result.overall_risk = determine_overall_risk_level(result.rule_results);
        result.recommendation = generate_fraud_recommendation(result);

        // Store results
        store_fraud_detection_result(result);

        if (logger_) {
            logger_->info("Transaction {} evaluated: fraud_score={}, risk_level={}, recommendation={}",
                         result.transaction_id, result.fraud_score,
                         static_cast<int>(result.overall_risk), result.recommendation);
        }

    } catch (const std::exception& e) {
        result.is_fraudulent = false; // Default to not fraudulent on error
        result.recommendation = "ERROR";

        if (logger_) {
            logger_->error("Transaction evaluation failed for {}: {}", context.transaction_id, e.what());
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    result.processing_duration = std::to_string(duration.count()) + "ms";

    return result;
}

RuleExecutionResultDetail AdvancedRuleEngine::execute_validation_rule(
    const RuleDefinition& rule,
    const RuleExecutionContext& context
) {
    RuleExecutionResultDetail result;
    result.rule_id = rule.rule_id;
    result.rule_name = rule.name;

    try {
        const auto& logic = rule.rule_logic;
        if (!logic.contains("conditions")) {
            result.result = RuleExecutionResult::ERROR;
            result.error_message = "Validation rule missing conditions";
            return result;
        }

        const auto& conditions = logic["conditions"];
        bool all_conditions_met = true;
        std::vector<std::string> failed_conditions;

        for (const auto& condition : conditions) {
            bool condition_met = evaluate_condition(condition, context.transaction_data);
            if (!condition_met) {
                all_conditions_met = false;
                if (condition.contains("description")) {
                    failed_conditions.push_back(condition["description"]);
                }
            } else {
                result.triggered_conditions.push_back(
                    condition.value("description", "Condition met")
                );
            }
        }

        result.result = all_conditions_met ? RuleExecutionResult::PASS : RuleExecutionResult::FAIL;

        if (!all_conditions_met) {
            result.rule_output = {
                {"failed_conditions", failed_conditions},
                {"validation_result", "FAILED"}
            };
        } else {
            result.rule_output = {
                {"validation_result", "PASSED"},
                {"conditions_checked", conditions.size()}
            };
        }

    } catch (const std::exception& e) {
        result.result = RuleExecutionResult::ERROR;
        result.error_message = std::string("Validation rule execution error: ") + e.what();
    }

    return result;
}

RuleExecutionResultDetail AdvancedRuleEngine::execute_scoring_rule(
    const RuleDefinition& rule,
    const RuleExecutionContext& context
) {
    RuleExecutionResultDetail result;
    result.rule_id = rule.rule_id;
    result.rule_name = rule.name;

    try {
        const auto& logic = rule.rule_logic;
        double score = 0.0;
        std::unordered_map<std::string, double> risk_factors;

        if (logic.contains("scoring_factors")) {
            const auto& factors = logic["scoring_factors"];

            for (const auto& factor : factors) {
                std::string field = factor["field"];
                double weight = factor.value("weight", 1.0);
                std::string operation = factor.value("operation", "exists");

                // Extract field value
                auto field_value = extract_field_value(context.transaction_data, field);

                double factor_score = 0.0;
                if (operation == "exists" && !field_value.is_null()) {
                    factor_score = weight;
                } else if (operation == "value" && field_value.is_number()) {
                    factor_score = field_value.get<double>() * weight;
                } else if (operation == "threshold") {
                    double threshold = factor.value("threshold", 0.0);
                    if (field_value.is_number() && field_value.get<double>() > threshold) {
                        factor_score = weight;
                    }
                }

                score += factor_score;
                if (factor_score > 0) {
                    risk_factors[field] = factor_score;
                }
            }
        }

        // Normalize score
        score = normalize_risk_score(score);

        // Determine if rule fails based on threshold
        double threshold = logic.value("threshold", 0.5);
        result.result = (score >= threshold) ? RuleExecutionResult::FAIL : RuleExecutionResult::PASS;

        result.rule_output = {
            {"score", score},
            {"threshold", threshold},
            {"risk_factors", risk_factors}
        };

        result.risk_factors = risk_factors;

    } catch (const std::exception& e) {
        result.result = RuleExecutionResult::ERROR;
        result.error_message = std::string("Scoring rule execution error: ") + e.what();
    }

    return result;
}

RuleExecutionResultDetail AdvancedRuleEngine::execute_pattern_rule(
    const RuleDefinition& rule,
    const RuleExecutionContext& context
) {
    RuleExecutionResultDetail result;
    result.rule_id = rule.rule_id;
    result.rule_name = rule.name;

    try {
        const auto& logic = rule.rule_logic;
        bool pattern_matched = false;
        std::vector<std::string> matched_patterns;

        if (logic.contains("patterns")) {
            const auto& patterns = logic["patterns"];

            for (const auto& pattern : patterns) {
                std::string pattern_type = pattern["type"];

                if (pattern_type == "regex") {
                    std::string field = pattern["field"];
                    std::string regex_pattern = pattern["pattern"];

                    auto field_value = extract_field_value(context.transaction_data, field);
                    if (field_value.is_string()) {
                        std::regex regex(regex_pattern);
                        std::string value_str = field_value.get<std::string>();
                        if (std::regex_match(value_str, regex)) {
                            pattern_matched = true;
                            matched_patterns.push_back("Regex pattern on field '" + field + "'");
                        }
                    }
                } else if (pattern_type == "value_list") {
                    std::string field = pattern["field"];
                    const auto& values = pattern["values"];

                    auto field_value = extract_field_value(context.transaction_data, field);
                    for (const auto& allowed_value : values) {
                        if (field_value == allowed_value) {
                            pattern_matched = true;
                            matched_patterns.push_back("Value list match on field '" + field + "'");
                            break;
                        }
                    }
                }
            }
        }

        result.result = pattern_matched ? RuleExecutionResult::FAIL : RuleExecutionResult::PASS;

        result.rule_output = {
            {"pattern_matched", pattern_matched},
            {"matched_patterns", matched_patterns}
        };

        if (pattern_matched) {
            result.triggered_conditions = matched_patterns;
        }

    } catch (const std::exception& e) {
        result.result = RuleExecutionResult::ERROR;
        result.error_message = std::string("Pattern rule execution error: ") + e.what();
    }

    return result;
}

RuleExecutionResultDetail AdvancedRuleEngine::execute_ml_rule(
    const RuleDefinition& rule,
    const RuleExecutionContext& context
) {
    RuleExecutionResultDetail result;
    result.rule_id = rule.rule_id;
    result.rule_name = rule.name;

    // Placeholder for machine learning-based rule execution
    // In a real implementation, this would integrate with ML models
    result.result = RuleExecutionResult::PASS;
    result.confidence_score = 0.5;
    result.rule_output = {
        {"ml_model", "placeholder"},
        {"prediction", "low_risk"},
        {"confidence", 0.5}
    };

    // TODO: Implement actual ML model integration
    result.error_message = "ML rule execution not yet implemented";

    return result;
}

bool AdvancedRuleEngine::register_rule(const RuleDefinition& rule) {
    try {
        // Validate rule definition
        if (!validate_rule_definition(rule)) {
            if (logger_) {
                logger_->error("Rule validation failed for rule '{}'", rule.rule_id);
            }
            return false;
        }

        // Store in database
        if (!store_rule(rule)) {
            return false;
        }

        // Cache the rule
        cache_rule(rule);

        if (logger_) {
            logger_->info("Rule '{}' registered successfully", rule.rule_id);
        }

        return true;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in register_rule: {}", e.what());
        }
        return false;
    }
}

std::optional<RuleDefinition> AdvancedRuleEngine::get_rule(const std::string& rule_id) {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    // Check cache first
    auto it = rule_cache_.find(rule_id);
    if (it != rule_cache_.end()) {
        return it->second;
    }

    // Load from database
    auto rule_opt = load_rule(rule_id);
    if (rule_opt) {
        rule_cache_[rule_id] = *rule_opt;
    }

    return rule_opt;
}

std::vector<RuleDefinition> AdvancedRuleEngine::get_active_rules() {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    std::vector<RuleDefinition> active_rules;
    for (const auto& [id, rule] : rule_cache_) {
        if (rule.is_active) {
            active_rules.push_back(rule);
        }
    }

    return active_rules;
}

void AdvancedRuleEngine::reload_rules() {
    try {
        std::lock_guard<std::mutex> lock(cache_mutex_);

        rule_cache_.clear();
        auto active_rules = load_active_rules();

        for (const auto& rule : active_rules) {
            rule_cache_[rule.rule_id] = rule;
        }

        if (logger_) {
            logger_->info("Reloaded {} active rules into cache", rule_cache_.size());
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in reload_rules: {}", e.what());
        }
    }
}

double AdvancedRuleEngine::calculate_aggregated_risk_score(const std::vector<RuleExecutionResultDetail>& results) {
    if (results.empty()) return 0.0;

    double total_score = 0.0;
    int fraud_rules = 0;

    for (const auto& result : results) {
        if (result.result == RuleExecutionResult::FAIL) {
            total_score += result.confidence_score;
            fraud_rules++;
        }
    }

    // Weight by number of failing rules and their average confidence
    if (fraud_rules > 0) {
        double average_confidence = total_score / fraud_rules;
        double rule_weight = std::min(1.0, static_cast<double>(fraud_rules) / 5.0); // Cap at 5 rules
        return average_confidence * rule_weight;
    }

    return 0.0;
}

FraudRiskLevel AdvancedRuleEngine::determine_overall_risk_level(const std::vector<RuleExecutionResultDetail>& results) {
    double aggregated_score = calculate_aggregated_risk_score(results);

    if (aggregated_score >= 0.8) return FraudRiskLevel::CRITICAL;
    if (aggregated_score >= 0.6) return FraudRiskLevel::HIGH;
    if (aggregated_score >= 0.4) return FraudRiskLevel::MEDIUM;
    return FraudRiskLevel::LOW;
}

std::string AdvancedRuleEngine::generate_fraud_recommendation(const FraudDetectionResult& result) {
    if (!result.is_fraudulent) {
        return "APPROVE";
    }

    switch (result.overall_risk) {
        case FraudRiskLevel::CRITICAL:
            return "BLOCK";
        case FraudRiskLevel::HIGH:
            return "REVIEW";
        case FraudRiskLevel::MEDIUM:
            return "REVIEW";
        case FraudRiskLevel::LOW:
            return "APPROVE";
        default:
            return "REVIEW";
    }
}

// Utility methods implementation
std::string AdvancedRuleEngine::generate_rule_id() {
    return "rule_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}

std::string AdvancedRuleEngine::generate_transaction_id() {
    return "txn_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}

bool AdvancedRuleEngine::validate_rule_definition(const RuleDefinition& rule) {
    if (rule.rule_id.empty() || rule.name.empty()) {
        return false;
    }

    if (rule.rule_type != "VALIDATION" && rule.rule_type != "SCORING" &&
        rule.rule_type != "PATTERN" && rule.rule_type != "MACHINE_LEARNING") {
        return false;
    }

    return true;
}

nlohmann::json AdvancedRuleEngine::extract_field_value(const nlohmann::json& data, const std::string& field_path) {
    try {
        // Simple dot notation support (e.g., "user.amount")
        std::stringstream ss(field_path);
        std::string field;
        nlohmann::json current = data;

        while (std::getline(ss, field, '.')) {
            if (current.contains(field)) {
                current = current[field];
            } else {
                return nullptr;
            }
        }

        return current;

    } catch (const std::exception&) {
        return nullptr;
    }
}

bool AdvancedRuleEngine::evaluate_condition(const nlohmann::json& condition, const nlohmann::json& data) {
    try {
        std::string field = condition["field"];
        std::string operator_str = condition["operator"];

        auto field_value = extract_field_value(data, field);
        if (field_value.is_null()) {
            return false;
        }

        if (operator_str == "equals") {
            return field_value == condition["value"];
        } else if (operator_str == "not_equals") {
            return field_value != condition["value"];
        } else if (operator_str == "greater_than") {
            if (field_value.is_number() && condition["value"].is_number()) {
                return field_value.get<double>() > condition["value"].get<double>();
            }
        } else if (operator_str == "less_than") {
            if (field_value.is_number() && condition["value"].is_number()) {
                return field_value.get<double>() < condition["value"].get<double>();
            }
        } else if (operator_str == "contains") {
            if (field_value.is_string() && condition["value"].is_string()) {
                return field_value.get<std::string>().find(condition["value"].get<std::string>()) != std::string::npos;
            }
        } else if (operator_str == "exists") {
            return !field_value.is_null();
        }

        return false;

    } catch (const std::exception&) {
        return false;
    }
}

double AdvancedRuleEngine::calculate_rule_confidence(const RuleDefinition& rule, const RuleExecutionResultDetail& result) {
    // Base confidence on rule priority and execution result
    double base_confidence = 0.5;

    if (result.result == RuleExecutionResult::FAIL) {
        base_confidence = 0.8;
    } else if (result.result == RuleExecutionResult::PASS) {
        base_confidence = 0.2;
    }

    // Adjust by priority
    double priority_multiplier = static_cast<double>(static_cast<int>(rule.priority)) / 4.0;

    return std::min(1.0, base_confidence * priority_multiplier);
}

double AdvancedRuleEngine::normalize_risk_score(double raw_score) {
    // Normalize to 0-1 range using sigmoid function
    return 1.0 / (1.0 + std::exp(-raw_score));
}

FraudRiskLevel AdvancedRuleEngine::score_to_risk_level(double score) {
    if (score >= 0.8) return FraudRiskLevel::CRITICAL;
    if (score >= 0.6) return FraudRiskLevel::HIGH;
    if (score >= 0.4) return FraudRiskLevel::MEDIUM;
    return FraudRiskLevel::LOW;
}

// Database operations (simplified implementations)
bool AdvancedRuleEngine::store_rule(const RuleDefinition& rule) {
    try {
        if (!db_conn_) return false;

        std::string query = R"(
            INSERT INTO fraud_detection_rules (
                rule_id, name, description, priority, rule_type, rule_logic,
                parameters, input_fields, output_fields, is_active, valid_from,
                valid_until, created_by, created_at, updated_at
            ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15)
            ON CONFLICT (rule_id) DO UPDATE SET
                name = EXCLUDED.name,
                description = EXCLUDED.description,
                priority = EXCLUDED.priority,
                rule_type = EXCLUDED.rule_type,
                rule_logic = EXCLUDED.rule_logic,
                parameters = EXCLUDED.parameters,
                input_fields = EXCLUDED.input_fields,
                output_fields = EXCLUDED.output_fields,
                is_active = EXCLUDED.is_active,
                valid_from = EXCLUDED.valid_from,
                valid_until = EXCLUDED.valid_until,
                updated_at = EXCLUDED.updated_at
        )";

        std::vector<std::string> params = {
            rule.rule_id,
            rule.name,
            rule.description,
            std::to_string(static_cast<int>(rule.priority)),
            rule.rule_type,
            rule.rule_logic.dump(),
            rule.parameters.dump(),
            nlohmann::json(rule.input_fields).dump(),
            nlohmann::json(rule.output_fields).dump(),
            rule.is_active ? "true" : "false",
            rule.valid_from ? std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                rule.valid_from->time_since_epoch()).count()) : "",
            rule.valid_until ? std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                rule.valid_until->time_since_epoch()).count()) : "",
            rule.created_by,
            std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                rule.created_at.time_since_epoch()).count()),
            std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                rule.updated_at.time_since_epoch()).count())
        };

        return db_conn_->execute_command(query, params);

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in store_rule: {}", e.what());
        }
        return false;
    }
}

std::optional<RuleDefinition> AdvancedRuleEngine::load_rule(const std::string& rule_id) {
    try {
        if (!db_conn_) return std::nullopt;

        std::string query = R"(
            SELECT rule_id, name, description, priority, rule_type, rule_logic,
                   parameters, input_fields, output_fields, is_active, valid_from,
                   valid_until, created_by, created_at, updated_at
            FROM fraud_detection_rules
            WHERE rule_id = $1
        )";

        std::vector<std::string> params = {rule_id};
        auto result = db_conn_->execute_query(query, params);

        if (result.rows.empty()) {
            return std::nullopt;
        }

        const auto& row = result.rows[0];
        RuleDefinition rule;

        rule.rule_id = row.at("rule_id");
        rule.name = row.at("name");
        rule.description = row.at("description");
        rule.priority = static_cast<RulePriority>(std::stoi(row.at("priority")));
        rule.rule_type = row.at("rule_type");
        rule.rule_logic = nlohmann::json::parse(row.at("rule_logic"));
        rule.parameters = nlohmann::json::parse(row.at("parameters"));
        rule.input_fields = nlohmann::json::parse(row.at("input_fields")).get<std::vector<std::string>>();
        rule.output_fields = nlohmann::json::parse(row.at("output_fields")).get<std::vector<std::string>>();
        rule.is_active = (row.at("is_active") == "true");
        rule.created_by = row.at("created_by");

        return rule;

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in load_rule: {}", e.what());
        }
        return std::nullopt;
    }
}

std::vector<RuleDefinition> AdvancedRuleEngine::load_active_rules() {
    std::vector<RuleDefinition> rules;

    try {
        if (!db_conn_) return rules;

        std::string query = R"(
            SELECT rule_id, name, description, priority, rule_type, rule_logic,
                   parameters, input_fields, output_fields, is_active, valid_from,
                   valid_until, created_by, created_at, updated_at
            FROM fraud_detection_rules
            WHERE is_active = true
            ORDER BY priority DESC, created_at DESC
        )";

        auto results = db_conn_->execute_query_multi(query, {});

        for (const auto& row : results) {
            RuleDefinition rule;
            rule.rule_id = row.at("rule_id");
            rule.name = row.at("name");
            rule.description = row.at("description");
            rule.priority = static_cast<RulePriority>(std::stoi(row.at("priority")));
            rule.rule_type = row.at("rule_type");
            rule.rule_logic = nlohmann::json::parse(row.at("rule_logic"));
            rule.parameters = nlohmann::json::parse(row.at("parameters"));
            rule.input_fields = nlohmann::json::parse(row.at("input_fields")).get<std::vector<std::string>>();
            rule.output_fields = nlohmann::json::parse(row.at("output_fields")).get<std::vector<std::string>>();
            rule.is_active = (row.at("is_active") == "true");
            rule.created_by = row.at("created_by");

            rules.push_back(rule);
        }

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in load_active_rules: {}", e.what());
        }
    }

    return rules;
}

bool AdvancedRuleEngine::store_fraud_detection_result(const FraudDetectionResult& result) {
    try {
        if (!db_conn_) return false;

        std::string query = R"(
            INSERT INTO fraud_detection_results (
                transaction_id, is_fraudulent, overall_risk, fraud_score,
                rule_results, aggregated_findings, detection_time, processing_duration,
                recommendation
            ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)
        )";

        std::vector<std::string> params = {
            result.transaction_id,
            result.is_fraudulent ? "true" : "false",
            std::to_string(static_cast<int>(result.overall_risk)),
            std::to_string(result.fraud_score),
            result.rule_results.empty() ? "[]" : nlohmann::json(result.rule_results).dump(),
            result.aggregated_findings.dump(),
            std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                result.detection_time.time_since_epoch()).count()),
            result.processing_duration,
            result.recommendation
        };

        return db_conn_->execute_command(query, params);

    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in store_fraud_detection_result: {}", e.what());
        }
        return false;
    }
}

void AdvancedRuleEngine::update_rule_metrics(const std::string& rule_id, const RuleExecutionResultDetail& result) {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    auto& metrics = metrics_cache_[rule_id];
    metrics.rule_id = rule_id;
    metrics.total_executions++;

    if (result.result == RuleExecutionResult::PASS) {
        metrics.successful_executions++;
    } else if (result.result == RuleExecutionResult::FAIL) {
        metrics.failed_executions++;
        metrics.fraud_detections++;
    }

    metrics.average_confidence_score = (metrics.average_confidence_score + result.confidence_score) / 2.0;
    metrics.last_execution = std::chrono::system_clock::now();
}

void AdvancedRuleEngine::record_execution_time(const std::string& rule_id, std::chrono::milliseconds duration) {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    auto& metrics = metrics_cache_[rule_id];
    metrics.average_execution_time_ms = (metrics.average_execution_time_ms + duration.count()) / 2.0;
}

void AdvancedRuleEngine::cache_rule(const RuleDefinition& rule) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    rule_cache_[rule.rule_id] = rule;
}

void AdvancedRuleEngine::load_configuration() {
    try {
        if (config_manager_) {
            // Load execution timeout
            auto timeout_opt = config_manager_->get_config("rule_engine.execution_timeout_ms");
            if (timeout_opt && timeout_opt->value.is_number()) {
                execution_timeout_ = std::chrono::milliseconds(
                    static_cast<long long>(timeout_opt->value.get<double>()));
            }

            // Load max parallel executions
            auto max_parallel_opt = config_manager_->get_config("rule_engine.max_parallel_executions");
            if (max_parallel_opt && max_parallel_opt->value.is_number()) {
                max_parallel_executions_ = static_cast<int>(max_parallel_opt->value.get<double>());
            }

            // Load performance monitoring setting
            auto monitoring_opt = config_manager_->get_config("rule_engine.enable_performance_monitoring");
            if (monitoring_opt && monitoring_opt->value.is_boolean()) {
                enable_performance_monitoring_ = monitoring_opt->value.get<bool>();
            }
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->warn("Failed to load rule engine configuration: {}", e.what());
        }
    }
}

} // namespace regulens
