#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <nlohmann/json.hpp>

#include "agent_decision.hpp"
#include "compliance_event.hpp"

namespace regulens {

/**
 * @brief Types of patterns that can be recognized
 */
enum class PatternType {
    DECISION_PATTERN,        // Patterns in agent decision-making
    BEHAVIOR_PATTERN,        // Agent behavior patterns
    ANOMALY_PATTERN,         // Anomalous activities or decisions
    TREND_PATTERN,          // Temporal trends in data
    CORRELATION_PATTERN,    // Correlations between variables
    SEQUENCE_PATTERN,       // Sequential patterns in events
    CLUSTER_PATTERN         // Clustering patterns in data
};

/**
 * @brief Confidence levels for pattern recognition
 */
enum class PatternConfidence {
    LOW,        // Pattern has low confidence (< 60%)
    MEDIUM,     // Pattern has medium confidence (60-80%)
    HIGH,       // Pattern has high confidence (80-95%)
    VERY_HIGH   // Pattern has very high confidence (> 95%)
};

/**
 * @brief Impact levels for discovered patterns
 */
enum class PatternImpact {
    LOW,        // Minimal impact on decision-making
    MEDIUM,     // Moderate impact, worth considering
    HIGH,       // Significant impact, should influence decisions
    CRITICAL    // Critical impact, requires immediate attention
};

/**
 * @brief Base pattern structure
 */
struct Pattern {
    std::string pattern_id;
    PatternType pattern_type;
    std::string name;
    std::string description;
    PatternConfidence confidence;
    PatternImpact impact;
    std::chrono::system_clock::time_point discovered_at;
    std::chrono::system_clock::time_point last_updated;
    size_t occurrences;      // How many times this pattern has been observed
    double strength;         // Pattern strength (0.0 to 1.0)

    // Pattern metadata
    std::unordered_map<std::string, std::string> metadata;
    std::unordered_map<std::string, double> features;  // Numerical features

    Pattern(std::string id, PatternType type, std::string n, std::string desc)
        : pattern_id(std::move(id)), pattern_type(type), name(std::move(n)),
          description(std::move(desc)), confidence(PatternConfidence::MEDIUM),
          impact(PatternImpact::MEDIUM), discovered_at(std::chrono::system_clock::now()),
          last_updated(std::chrono::system_clock::now()), occurrences(1), strength(0.5) {}

    virtual ~Pattern() = default;

    virtual nlohmann::json to_json() const {
        nlohmann::json metadata_json;
        for (const auto& [key, value] : metadata) {
            metadata_json[key] = value;
        }

        nlohmann::json features_json;
        for (const auto& [key, value] : features) {
            features_json[key] = value;
        }

        return {
            {"pattern_id", pattern_id},
            {"pattern_type", static_cast<int>(pattern_type)},
            {"name", name},
            {"description", description},
            {"confidence", static_cast<int>(confidence)},
            {"impact", static_cast<int>(impact)},
            {"discovered_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                discovered_at.time_since_epoch()).count()},
            {"last_updated", std::chrono::duration_cast<std::chrono::milliseconds>(
                last_updated.time_since_epoch()).count()},
            {"occurrences", occurrences},
            {"strength", strength},
            {"metadata", metadata_json},
            {"features", features_json}
        };
    }

    void update_occurrence() {
        occurrences++;
        last_updated = std::chrono::system_clock::now();
        // Update strength using exponential decay for temporal degradation
        strength = std::min(1.0, strength + (1.0 - strength) * 0.1);
    }
};

/**
 * @brief Decision pattern - patterns in agent decision-making
 */
struct DecisionPattern : public Pattern {
    std::string agent_id;
    DecisionType decision_type;
    std::vector<std::string> triggering_factors;  // Factors that trigger this decision
    std::unordered_map<std::string, double> factor_weights;  // Importance of each factor
    std::vector<DecisionType> alternative_decisions;  // Other decisions considered

    DecisionPattern(std::string id, std::string agent, DecisionType decision,
                   const std::vector<std::string>& factors)
        : Pattern(std::move(id), PatternType::DECISION_PATTERN,
                 "Decision Pattern", "Pattern in agent decision-making"),
          agent_id(std::move(agent)), decision_type(decision),
          triggering_factors(factors) {}

    nlohmann::json to_json() const override {
        auto base_json = Pattern::to_json();

        nlohmann::json factors_json = triggering_factors;
        nlohmann::json weights_json;
        for (const auto& [key, value] : factor_weights) {
            weights_json[key] = value;
        }
        nlohmann::json alternatives_json;
        for (auto alt : alternative_decisions) {
            alternatives_json.push_back(static_cast<int>(alt));
        }

        base_json["agent_id"] = agent_id;
        base_json["decision_type"] = static_cast<int>(decision_type);
        base_json["triggering_factors"] = factors_json;
        base_json["factor_weights"] = weights_json;
        base_json["alternative_decisions"] = alternatives_json;

        return base_json;
    }
};

/**
 * @brief Behavior pattern - patterns in agent behavior
 */
struct BehaviorPattern : public Pattern {
    std::string agent_id;
    std::string behavior_type;  // "response_time", "error_rate", "decision_consistency", etc.
    std::vector<double> behavior_values;  // Historical behavior values
    double mean_value;
    double standard_deviation;
    std::chrono::system_clock::time_point pattern_start;
    std::chrono::system_clock::time_point pattern_end;

    BehaviorPattern(std::string id, std::string agent, std::string behavior)
        : Pattern(std::move(id), PatternType::BEHAVIOR_PATTERN,
                 "Behavior Pattern", "Pattern in agent behavior"),
          agent_id(std::move(agent)), behavior_type(std::move(behavior)),
          mean_value(0.0), standard_deviation(0.0),
          pattern_start(std::chrono::system_clock::now()) {}

    nlohmann::json to_json() const override {
        auto base_json = Pattern::to_json();

        base_json["agent_id"] = agent_id;
        base_json["behavior_type"] = behavior_type;
        base_json["behavior_values"] = behavior_values;
        base_json["mean_value"] = mean_value;
        base_json["standard_deviation"] = standard_deviation;
        base_json["pattern_start"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            pattern_start.time_since_epoch()).count();
        base_json["pattern_end"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            pattern_end.time_since_epoch()).count();

        return base_json;
    }

    void add_value(double value) {
        behavior_values.push_back(value);
        update_statistics();
        update_occurrence();
    }

private:
    void update_statistics() {
        if (behavior_values.empty()) return;

        // Calculate mean
        double sum = 0.0;
        for (double val : behavior_values) {
            sum += val;
        }
        mean_value = sum / behavior_values.size();

        // Calculate standard deviation
        double variance = 0.0;
        for (double val : behavior_values) {
            variance += std::pow(val - mean_value, 2);
        }
        standard_deviation = std::sqrt(variance / behavior_values.size());
    }
};

/**
 * @brief Anomaly pattern - detection of anomalous activities
 */
struct AnomalyPattern : public Pattern {
    std::string anomaly_type;  // "decision_anomaly", "behavior_anomaly", "performance_anomaly"
    std::string affected_entity;  // Agent ID, event ID, etc.
    double anomaly_score;  // How anomalous this is (0.0 to 1.0)
    std::vector<std::string> anomaly_indicators;  // What made this anomalous
    std::chrono::system_clock::time_point anomaly_time;

    AnomalyPattern(std::string id, std::string type, std::string entity, double score)
        : Pattern(std::move(id), PatternType::ANOMALY_PATTERN,
                 "Anomaly Pattern", "Detected anomalous activity"),
          anomaly_type(std::move(type)), affected_entity(std::move(entity)),
          anomaly_score(score), anomaly_time(std::chrono::system_clock::now()) {}

    nlohmann::json to_json() const override {
        auto base_json = Pattern::to_json();

        base_json["anomaly_type"] = anomaly_type;
        base_json["affected_entity"] = affected_entity;
        base_json["anomaly_score"] = anomaly_score;
        base_json["anomaly_indicators"] = anomaly_indicators;
        base_json["anomaly_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            anomaly_time.time_since_epoch()).count();

        return base_json;
    }
};

/**
 * @brief Trend pattern - temporal trends in data
 */
struct TrendPattern : public Pattern {
    std::string trend_type;  // "increasing", "decreasing", "cyclical", "seasonal"
    std::string metric_name;  // What metric is trending
    double trend_slope;  // Rate of change
    double r_squared;  // Goodness of fit for the trend
    std::chrono::system_clock::time_point trend_start;
    std::chrono::system_clock::time_point trend_end;

    TrendPattern(std::string id, std::string type, std::string metric, double slope)
        : Pattern(std::move(id), PatternType::TREND_PATTERN,
                 "Trend Pattern", "Temporal trend in data"),
          trend_type(std::move(type)), metric_name(std::move(metric)),
          trend_slope(slope), r_squared(0.0),
          trend_start(std::chrono::system_clock::now()) {}

    nlohmann::json to_json() const override {
        auto base_json = Pattern::to_json();

        base_json["trend_type"] = trend_type;
        base_json["metric_name"] = metric_name;
        base_json["trend_slope"] = trend_slope;
        base_json["r_squared"] = r_squared;
        base_json["trend_start"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            trend_start.time_since_epoch()).count();
        base_json["trend_end"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            trend_end.time_since_epoch()).count();

        return base_json;
    }
};

/**
 * @brief Correlation pattern - relationships between variables
 */
struct CorrelationPattern : public Pattern {
    std::string variable_a;
    std::string variable_b;
    double correlation_coefficient;  // Pearson correlation (-1.0 to 1.0)
    std::string correlation_type;  // "positive", "negative", "no_correlation"
    size_t sample_size;  // Number of data points used

    CorrelationPattern(std::string id, std::string var_a, std::string var_b, double coeff)
        : Pattern(std::move(id), PatternType::CORRELATION_PATTERN,
                 "Correlation Pattern", "Correlation between variables"),
          variable_a(std::move(var_a)), variable_b(std::move(var_b)),
          correlation_coefficient(coeff), sample_size(0) {
        // Determine correlation type
        if (std::abs(coeff) < 0.3) {
            correlation_type = "no_correlation";
        } else if (coeff > 0) {
            correlation_type = "positive";
        } else {
            correlation_type = "negative";
        }
    }

    nlohmann::json to_json() const override {
        auto base_json = Pattern::to_json();

        base_json["variable_a"] = variable_a;
        base_json["variable_b"] = variable_b;
        base_json["correlation_coefficient"] = correlation_coefficient;
        base_json["correlation_type"] = correlation_type;
        base_json["sample_size"] = sample_size;

        return base_json;
    }
};

/**
 * @brief Sequence pattern - patterns in event sequences
 */
struct SequencePattern : public Pattern {
    std::vector<std::string> event_sequence;  // Sequence of events
    double support;  // How frequently this sequence occurs
    double confidence;  // Confidence in the pattern
    size_t min_occurrences;  // Minimum occurrences to be considered a pattern

    SequencePattern(std::string id, const std::vector<std::string>& sequence)
        : Pattern(std::move(id), PatternType::SEQUENCE_PATTERN,
                 "Sequence Pattern", "Pattern in event sequences"),
          event_sequence(sequence), support(0.0), confidence(0.0), min_occurrences(3) {}

    nlohmann::json to_json() const override {
        auto base_json = Pattern::to_json();

        base_json["event_sequence"] = event_sequence;
        base_json["support"] = support;
        base_json["confidence"] = confidence;
        base_json["min_occurrences"] = min_occurrences;

        return base_json;
    }
};

/**
 * @brief Data point for pattern analysis
 */
struct PatternDataPoint {
    std::string entity_id;  // Agent ID, event ID, etc.
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, double> numerical_features;
    std::unordered_map<std::string, std::string> categorical_features;
    std::optional<nlohmann::json> raw_data;  // Original data if needed

    PatternDataPoint(std::string id, std::chrono::system_clock::time_point ts)
        : entity_id(std::move(id)), timestamp(ts) {}

    nlohmann::json to_json() const {
        nlohmann::json num_features_json;
        for (const auto& [key, value] : numerical_features) {
            num_features_json[key] = value;
        }

        nlohmann::json cat_features_json;
        for (const auto& [key, value] : categorical_features) {
            cat_features_json[key] = value;
        }

        nlohmann::json result = {
            {"entity_id", entity_id},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count()},
            {"numerical_features", num_features_json},
            {"categorical_features", cat_features_json}
        };

        if (raw_data) {
            result["raw_data"] = *raw_data;
        }

        return result;
    }
};

/**
 * @brief Pattern analysis configuration
 */
struct PatternAnalysisConfig {
    size_t min_pattern_occurrences = 5;     // Minimum occurrences for pattern recognition
    double min_pattern_confidence = 0.7;    // Minimum confidence threshold
    size_t max_patterns_per_type = 100;     // Maximum patterns to keep per type
    std::chrono::hours data_retention_period = std::chrono::hours(168);  // 7 days
    bool enable_real_time_analysis = true;  // Enable real-time pattern detection
    size_t batch_analysis_interval = 100;   // Analyze every N data points
    std::unordered_set<std::string> monitored_entities;  // Entities to monitor

    nlohmann::json to_json() const {
        return {
            {"min_pattern_occurrences", min_pattern_occurrences},
            {"min_pattern_confidence", min_pattern_confidence},
            {"max_patterns_per_type", max_patterns_per_type},
            {"data_retention_period_hours", data_retention_period.count()},
            {"enable_real_time_analysis", enable_real_time_analysis},
            {"batch_analysis_interval", batch_analysis_interval},
            {"monitored_entities", std::vector<std::string>(monitored_entities.begin(), monitored_entities.end())}
        };
    }
};

} // namespace regulens
