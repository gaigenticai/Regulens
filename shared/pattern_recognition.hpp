#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>

#include "models/pattern_data.hpp"
#include "logging/structured_logger.hpp"
#include "config/configuration_manager.hpp"

namespace regulens {

/**
 * @brief Pattern recognition algorithms for historical data analysis
 *
 * Implements various machine learning algorithms to identify patterns in
 * agent decisions, behaviors, and system activities for continuous learning.
 */
class PatternRecognitionEngine {
public:
    PatternRecognitionEngine(std::shared_ptr<ConfigurationManager> config,
                           std::shared_ptr<StructuredLogger> logger);

    ~PatternRecognitionEngine();

    /**
     * @brief Initialize the pattern recognition engine
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown the pattern recognition engine
     */
    void shutdown();

    /**
     * @brief Add a data point for pattern analysis
     * @param data_point The data point to analyze
     * @return true if processed successfully
     */
    bool add_data_point(const PatternDataPoint& data_point);

    /**
     * @brief Analyze historical data for patterns
     * @param entity_id Entity to analyze (optional, analyze all if empty)
     * @return Vector of discovered patterns
     */
    std::vector<std::shared_ptr<Pattern>> analyze_patterns(const std::string& entity_id = "");

    /**
     * @brief Get patterns of a specific type
     * @param pattern_type Type of patterns to retrieve
     * @param min_confidence Minimum confidence threshold
     * @return Vector of matching patterns
     */
    std::vector<std::shared_ptr<Pattern>> get_patterns(PatternType pattern_type,
                                                     double min_confidence = 0.0);

    /**
     * @brief Get pattern by ID
     * @param pattern_id Pattern identifier
     * @return Pattern if found
     */
    std::optional<std::shared_ptr<Pattern>> get_pattern(const std::string& pattern_id);

    /**
     * @brief Apply learned patterns to new data
     * @param data_point New data point
     * @return Vector of applicable patterns with confidence scores
     */
    std::vector<std::pair<std::shared_ptr<Pattern>, double>> apply_patterns(
        const PatternDataPoint& data_point);

    /**
     * @brief Get pattern analysis statistics
     * @return JSON with analysis statistics
     */
    nlohmann::json get_analysis_stats();

    /**
     * @brief Export patterns for analysis/backup
     * @param pattern_type Type filter (optional)
     * @param format Export format ("json", "csv")
     * @return Exported data as string
     */
    std::string export_patterns(PatternType pattern_type = PatternType::DECISION_PATTERN,
                              const std::string& format = "json");

    /**
     * @brief Force cleanup of old data and patterns
     * @return Number of items cleaned up
     */
    size_t cleanup_old_data();

    // Configuration access
    const PatternAnalysisConfig& get_config() const { return config_; }

private:
    std::shared_ptr<ConfigurationManager> config_manager_;
    std::shared_ptr<StructuredLogger> logger_;

    PatternAnalysisConfig config_;

    // Thread-safe storage
    std::mutex data_mutex_;
    std::unordered_map<std::string, std::deque<PatternDataPoint>> entity_data_; // entity_id -> data points
    std::unordered_map<std::string, std::shared_ptr<Pattern>> discovered_patterns_; // pattern_id -> pattern

    std::mutex analysis_mutex_;
    std::atomic<size_t> total_data_points_;
    std::atomic<size_t> total_patterns_discovered_;

    // Background processing
    std::thread analysis_thread_;
    std::atomic<bool> running_;
    std::mutex analysis_cv_mutex_;
    std::condition_variable analysis_cv_;

    // Pattern analysis algorithms
    std::vector<std::shared_ptr<DecisionPattern>> analyze_decision_patterns(const std::string& entity_id);
    std::vector<std::shared_ptr<BehaviorPattern>> analyze_behavior_patterns(const std::string& entity_id);
    std::vector<std::shared_ptr<AnomalyPattern>> detect_anomalies(const std::string& entity_id);
    std::vector<std::shared_ptr<TrendPattern>> analyze_trends(const std::string& entity_id);
    std::vector<std::shared_ptr<CorrelationPattern>> analyze_correlations(const std::string& entity_id);
    std::vector<std::shared_ptr<SequencePattern>> analyze_sequences(const std::string& entity_id);

    // Helper algorithms
    std::vector<std::shared_ptr<Pattern>> kmeans_clustering(
        const std::vector<PatternDataPoint>& data_points, size_t k);

    std::vector<std::shared_ptr<CorrelationPattern>> calculate_correlations(
        const std::vector<PatternDataPoint>& data_points);

    std::optional<std::shared_ptr<TrendPattern>> detect_linear_trend(
        const std::vector<PatternDataPoint>& data_points, const std::string& metric);

    double calculate_anomaly_score(const PatternDataPoint& data_point,
                                 const std::vector<PatternDataPoint>& historical_data);

    std::vector<std::string> find_frequent_sequences(
        const std::vector<std::string>& events, size_t min_occurrences);

    void extend_sequence(std::vector<std::string> current_sequences,
                        const std::vector<std::string>& events,
                        const std::vector<size_t>& positions,
                        size_t min_occurrences,
                        std::vector<std::string>& frequent_sequences);

    bool matches_sequence(const std::vector<std::string>& events,
                         size_t start_pos,
                         const std::vector<std::string>& sequence);

    size_t count_sequence_occurrences(const std::vector<std::string>& events,
                                     const std::vector<std::string>& sequence);

    // Utility functions
    std::string generate_pattern_id(PatternType type, const std::string& entity_id);
    bool is_pattern_significant(const std::shared_ptr<Pattern>& pattern) const;
    void update_pattern_strength(std::shared_ptr<Pattern>& pattern);
    std::vector<PatternDataPoint> get_recent_data(const std::string& entity_id, size_t count = 1000);

    // Statistical helpers
    double calculate_mean(const std::vector<double>& values);
    double calculate_standard_deviation(const std::vector<double>& values, double mean);
    double calculate_pearson_correlation(const std::vector<double>& x, const std::vector<double>& y);
    double calculate_zscore(double value, double mean, double stddev);

    // Background analysis worker
    void analysis_worker();

    // Pattern persistence (when enabled)
    bool persist_pattern(const std::shared_ptr<Pattern>& pattern);
    bool persist_data_point(const PatternDataPoint& data_point);
    std::vector<std::shared_ptr<Pattern>> load_patterns(PatternType type = PatternType::DECISION_PATTERN);
    std::vector<PatternDataPoint> load_data_points(const std::string& entity_id);
};

// Convenience functions for creating data points from common sources

/**
 * @brief Create data point from agent decision
 */
inline PatternDataPoint create_data_point_from_decision(const AgentDecision& decision,
                                                      const std::string& /*event_id*/) {
    PatternDataPoint dp(decision.get_agent_id(), decision.get_timestamp());

    // Add decision-related features
    dp.categorical_features["decision_type"] = decision_type_to_string(decision.get_type());
    dp.numerical_features["confidence"] = static_cast<double>(decision.get_confidence()) / 100.0; // Normalize

    // Add reasoning factors as features
    const auto& reasoning = decision.get_reasoning();
    for (size_t i = 0; i < reasoning.size(); ++i) {
        dp.numerical_features["factor_" + std::to_string(i) + "_weight"] = reasoning[i].weight;
    }

    // Add raw decision data
    dp.raw_data = decision.to_json();

    return dp;
}

/**
 * @brief Create data point from agent activity
 */
inline PatternDataPoint create_data_point_from_activity(const std::string& agent_id,
                                                      const std::string& activity_type,
                                                      double activity_value,
                                                      std::chrono::system_clock::time_point timestamp) {
    PatternDataPoint dp(agent_id, timestamp);

    dp.categorical_features["activity_type"] = activity_type;
    dp.numerical_features["activity_value"] = activity_value;

    return dp;
}

/**
 * @brief Create data point from compliance event
 */
inline PatternDataPoint create_data_point_from_event(const ComplianceEvent& event) {
    PatternDataPoint dp("system", event.get_timestamp());

    dp.categorical_features["event_type"] = event_type_to_string(event.get_type());
    dp.categorical_features["severity"] = event_severity_to_string(event.get_severity());

    // Add event metadata as features
    for (const auto& [key, value] : event.get_metadata()) {
        if (std::holds_alternative<int>(value)) {
            dp.numerical_features["meta_" + key] = std::get<int>(value);
        } else if (std::holds_alternative<double>(value)) {
            dp.numerical_features["meta_" + key] = std::get<double>(value);
        } else if (std::holds_alternative<std::string>(value)) {
            dp.categorical_features["meta_" + key] = std::get<std::string>(value);
        }
    }

    return dp;
}

} // namespace regulens
