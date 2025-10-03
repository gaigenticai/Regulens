#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <nlohmann/json.hpp>

#include "agent_decision.hpp"
#include "compliance_event.hpp"
#include "human_ai_interaction.hpp"

namespace regulens {

/**
 * @brief Types of feedback that can be collected
 */
enum class FeedbackType {
    HUMAN_EXPLICIT,      // Direct human feedback (agree/disagree/suggestions)
    HUMAN_IMPLICIT,      // Inferred from human behavior (time spent, actions taken)
    SYSTEM_VALIDATION,   // System validation of decision outcomes
    PERFORMANCE_METRIC,  // Performance-based feedback
    COMPLIANCE_OUTCOME,  // Compliance outcome feedback
    BUSINESS_IMPACT      // Business impact feedback
};

/**
 * @brief Feedback priority levels
 */
enum class FeedbackPriority {
    LOW,        // Minor feedback, low impact
    MEDIUM,     // Moderate feedback, consider for learning
    HIGH,       // Important feedback, should influence learning
    CRITICAL    // Critical feedback, immediate action required
};

/**
 * @brief Feedback application strategies
 */
enum class LearningStrategy {
    IMMEDIATE_UPDATE,    // Apply feedback immediately to current model
    BATCH_UPDATE,        // Collect and apply in batches
    REINFORCEMENT,       // Use reinforcement learning
    SUPERVISED,          // Use supervised learning approach
    TRANSFER_LEARNING    // Transfer learning from similar scenarios
};

/**
 * @brief Feedback data structure
 */
struct FeedbackData {
    std::string feedback_id;
    FeedbackType feedback_type;
    FeedbackPriority priority;
    std::string source_entity;    // Agent, human user, or system component
    std::string target_entity;    // Agent or decision being evaluated
    std::string decision_id;      // Associated decision ID
    std::string context;          // Context where feedback was given
    double feedback_score;        // Numerical score (-1.0 to 1.0)
    std::string feedback_text;    // Textual feedback
    std::unordered_map<std::string, std::string> metadata;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::system_clock::time_point applied_at;

    FeedbackData(std::string sid, FeedbackType type, std::string source, std::string target)
        : feedback_id(generate_feedback_id(sid, type, source, target)),
          feedback_type(type), priority(FeedbackPriority::MEDIUM),
          source_entity(std::move(source)), target_entity(std::move(target)),
          feedback_score(0.0), timestamp(std::chrono::system_clock::now()) {}

    nlohmann::json to_json() const {
        nlohmann::json metadata_json;
        for (const auto& [key, value] : metadata) {
            metadata_json[key] = value;
        }

        nlohmann::json result = {
            {"feedback_id", feedback_id},
            {"feedback_type", static_cast<int>(feedback_type)},
            {"priority", static_cast<int>(priority)},
            {"source_entity", source_entity},
            {"target_entity", target_entity},
            {"decision_id", decision_id},
            {"context", context},
            {"feedback_score", feedback_score},
            {"feedback_text", feedback_text},
            {"metadata", metadata_json},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count()}
        };

        if (applied_at != std::chrono::system_clock::time_point{}) {
            result["applied_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                applied_at.time_since_epoch()).count();
        }

        return result;
    }

private:
    static std::string generate_feedback_id(const std::string& sid, FeedbackType type,
                                          const std::string& source, const std::string& target) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return "fb_" + sid + "_" + std::to_string(static_cast<int>(type)) + "_" +
               source + "_" + target + "_" + std::to_string(ms);
    }
};

/**
 * @brief Learning model that incorporates feedback
 */
struct LearningModel {
    std::string model_id;
    std::string model_type;       // "decision_model", "behavior_model", "risk_model"
    std::string target_agent;
    LearningStrategy strategy;
    std::unordered_map<std::string, double> parameters;  // Model parameters
    std::vector<FeedbackData> training_feedback;         // Feedback used for training
    double accuracy_score;        // Model accuracy (0.0 to 1.0)
    double improvement_rate;      // Rate of improvement over time
    std::chrono::system_clock::time_point last_trained;
    std::chrono::system_clock::time_point created_at;

    LearningModel(std::string id, std::string type, std::string agent, LearningStrategy strat)
        : model_id(std::move(id)), model_type(std::move(type)), target_agent(std::move(agent)),
          strategy(strat), accuracy_score(0.5), improvement_rate(0.0),
          last_trained(std::chrono::system_clock::now()), created_at(std::chrono::system_clock::now()) {}

    nlohmann::json to_json() const {
        nlohmann::json params_json;
        for (const auto& [key, value] : parameters) {
            params_json[key] = value;
        }

        nlohmann::json feedback_json = nlohmann::json::array();
        for (const auto& fb : training_feedback) {
            feedback_json.push_back(fb.to_json());
        }

        return {
            {"model_id", model_id},
            {"model_type", model_type},
            {"target_agent", target_agent},
            {"strategy", static_cast<int>(strategy)},
            {"parameters", params_json},
            {"training_feedback", feedback_json},
            {"accuracy_score", accuracy_score},
            {"improvement_rate", improvement_rate},
            {"last_trained", std::chrono::duration_cast<std::chrono::milliseconds>(
                last_trained.time_since_epoch()).count()},
            {"created_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                created_at.time_since_epoch()).count()}
        };
    }

    void add_feedback(const FeedbackData& feedback) {
        training_feedback.push_back(feedback);
        last_trained = std::chrono::system_clock::now();
    }

    void update_accuracy(double new_accuracy) {
        double old_accuracy = accuracy_score;
        accuracy_score = new_accuracy;
        improvement_rate = new_accuracy - old_accuracy;
        last_trained = std::chrono::system_clock::now();
    }
};

/**
 * @brief Feedback analysis results
 */
struct FeedbackAnalysis {
    std::string analysis_id;
    std::string entity_id;        // Agent or system component being analyzed
    std::chrono::system_clock::time_point analysis_period_start;
    std::chrono::system_clock::time_point analysis_period_end;
    size_t total_feedback_count;
    double average_feedback_score;
    std::unordered_map<FeedbackType, size_t> feedback_type_distribution;
    std::unordered_map<FeedbackPriority, size_t> feedback_priority_distribution;
    std::vector<std::string> key_insights;    // Key learnings from feedback
    std::vector<std::string> recommended_actions;  // Recommended improvements
    double confidence_score;     // Confidence in analysis (0.0 to 1.0)

    FeedbackAnalysis(std::string eid, std::chrono::system_clock::time_point start,
                    std::chrono::system_clock::time_point end)
        : analysis_id(generate_analysis_id(eid, start, end)),
          entity_id(std::move(eid)), analysis_period_start(start), analysis_period_end(end),
          total_feedback_count(0), average_feedback_score(0.0), confidence_score(0.0) {}

    nlohmann::json to_json() const {
        nlohmann::json type_dist_json;
        for (const auto& [type, count] : feedback_type_distribution) {
            type_dist_json[std::to_string(static_cast<int>(type))] = count;
        }

        nlohmann::json priority_dist_json;
        for (const auto& [priority, count] : feedback_priority_distribution) {
            priority_dist_json[std::to_string(static_cast<int>(priority))] = count;
        }

        return {
            {"analysis_id", analysis_id},
            {"entity_id", entity_id},
            {"analysis_period_start", std::chrono::duration_cast<std::chrono::milliseconds>(
                analysis_period_start.time_since_epoch()).count()},
            {"analysis_period_end", std::chrono::duration_cast<std::chrono::milliseconds>(
                analysis_period_end.time_since_epoch()).count()},
            {"total_feedback_count", total_feedback_count},
            {"average_feedback_score", average_feedback_score},
            {"feedback_type_distribution", type_dist_json},
            {"feedback_priority_distribution", priority_dist_json},
            {"key_insights", key_insights},
            {"recommended_actions", recommended_actions},
            {"confidence_score", confidence_score}
        };
    }

private:
    static std::string generate_analysis_id(const std::string& eid,
                                          std::chrono::system_clock::time_point start,
                                          std::chrono::system_clock::time_point end) {
        auto start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            start.time_since_epoch()).count();
        auto end_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end.time_since_epoch()).count();
        return "analysis_" + eid + "_" + std::to_string(start_ms) + "_" + std::to_string(end_ms);
    }
};

/**
 * @brief Feedback incorporation configuration
 */
struct FeedbackConfig {
    size_t max_feedback_per_entity = 10000;   // Maximum feedback to keep per entity
    std::chrono::hours feedback_retention_period = std::chrono::hours(168);  // 7 days
    size_t min_feedback_for_learning = 10;    // Minimum feedback for model updates
    double feedback_confidence_threshold = 0.7;  // Minimum confidence for application
    bool enable_real_time_learning = true;    // Enable real-time feedback incorporation
    size_t batch_learning_interval = 50;      // Process feedback in batches
    bool enable_persistence = true;           // Whether to persist feedback to database
    std::unordered_set<std::string> monitored_entities;  // Entities to monitor

    nlohmann::json to_json() const {
        return {
            {"max_feedback_per_entity", max_feedback_per_entity},
            {"feedback_retention_period_hours", feedback_retention_period.count()},
            {"min_feedback_for_learning", min_feedback_for_learning},
            {"feedback_confidence_threshold", feedback_confidence_threshold},
            {"enable_real_time_learning", enable_real_time_learning},
            {"batch_learning_interval", batch_learning_interval},
            {"enable_persistence", enable_persistence},
            {"monitored_entities", std::vector<std::string>(monitored_entities.begin(), monitored_entities.end())}
        };
    }
};

} // namespace regulens
