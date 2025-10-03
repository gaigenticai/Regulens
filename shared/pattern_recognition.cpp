#include "pattern_recognition.hpp"

#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace regulens {

PatternRecognitionEngine::PatternRecognitionEngine(std::shared_ptr<ConfigurationManager> config,
                                                 std::shared_ptr<StructuredLogger> logger)
    : config_manager_(config), logger_(logger),
      total_data_points_(0), total_patterns_discovered_(0), running_(false) {
    // Load configuration from environment
    config_.min_pattern_occurrences = static_cast<size_t>(config_manager_->get_int("PATTERN_MIN_OCCURRENCES").value_or(5));
    config_.min_pattern_confidence = config_manager_->get_double("PATTERN_MIN_CONFIDENCE").value_or(0.7);
    config_.max_patterns_per_type = static_cast<size_t>(config_manager_->get_int("PATTERN_MAX_PER_TYPE").value_or(100));
    config_.data_retention_period = std::chrono::hours(
        config_manager_->get_int("PATTERN_RETENTION_HOURS").value_or(168));
    config_.enable_real_time_analysis = config_manager_->get_bool("PATTERN_REAL_TIME_ANALYSIS").value_or(true);
    config_.batch_analysis_interval = static_cast<size_t>(config_manager_->get_int("PATTERN_BATCH_INTERVAL").value_or(100));

    logger_->info("PatternRecognitionEngine initialized with retention: " +
                 std::to_string(config_.data_retention_period.count()) + " hours");
}

PatternRecognitionEngine::~PatternRecognitionEngine() {
    shutdown();
}

bool PatternRecognitionEngine::initialize() {
    logger_->info("Initializing PatternRecognitionEngine");

    running_ = true;

    // Start background analysis thread
    analysis_thread_ = std::thread(&PatternRecognitionEngine::analysis_worker, this);

    logger_->info("PatternRecognitionEngine initialization complete");
    return true;
}

void PatternRecognitionEngine::shutdown() {
    if (!running_) return;

    logger_->info("Shutting down PatternRecognitionEngine");

    running_ = false;

    // Wake up analysis thread
    {
        std::unique_lock<std::mutex> lock(analysis_cv_mutex_);
        analysis_cv_.notify_one();
    }

    if (analysis_thread_.joinable()) {
        analysis_thread_.join();
    }

    logger_->info("PatternRecognitionEngine shutdown complete");
}

bool PatternRecognitionEngine::add_data_point(const PatternDataPoint& data_point) {
    try {
        std::lock_guard<std::mutex> lock(data_mutex_);

        // Ensure entity has a data queue
        if (entity_data_.find(data_point.entity_id) == entity_data_.end()) {
            entity_data_[data_point.entity_id] = std::deque<PatternDataPoint>();
        }

        auto& entity_queue = entity_data_[data_point.entity_id];

        // Enforce per-entity data limit
        if (entity_queue.size() >= 10000) { // Keep last 10k points per entity
            entity_queue.pop_front();
        }

        entity_queue.push_back(data_point);
        total_data_points_++;

        logger_->debug("Added data point for entity: " + data_point.entity_id);

        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to add data point: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::shared_ptr<Pattern>> PatternRecognitionEngine::analyze_patterns(const std::string& entity_id) {
    std::vector<std::shared_ptr<Pattern>> all_patterns;

    try {
        // Analyze patterns for specific entity or all entities
        std::vector<std::string> entities_to_analyze;
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            if (entity_id.empty()) {
                for (const auto& [eid, _] : entity_data_) {
                    entities_to_analyze.push_back(eid);
                }
            } else {
                entities_to_analyze.push_back(entity_id);
            }
        }

        for (const std::string& eid : entities_to_analyze) {
            // Analyze different pattern types
            auto decision_patterns = analyze_decision_patterns(eid);
            auto behavior_patterns = analyze_behavior_patterns(eid);
            auto anomalies = detect_anomalies(eid);
            auto trends = analyze_trends(eid);
            auto correlations = analyze_correlations(eid);
            auto sequences = analyze_sequences(eid);

            // Combine all patterns
            all_patterns.insert(all_patterns.end(), decision_patterns.begin(), decision_patterns.end());
            all_patterns.insert(all_patterns.end(), behavior_patterns.begin(), behavior_patterns.end());
            all_patterns.insert(all_patterns.end(), anomalies.begin(), anomalies.end());
            all_patterns.insert(all_patterns.end(), trends.begin(), trends.end());
            all_patterns.insert(all_patterns.end(), correlations.begin(), correlations.end());
            all_patterns.insert(all_patterns.end(), sequences.begin(), sequences.end());

            // Store discovered patterns
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                for (const auto& pattern : all_patterns) {
                    if (is_pattern_significant(pattern)) {
                        discovered_patterns_[pattern->pattern_id] = pattern;
                        total_patterns_discovered_++;
                    }
                }
            }
        }

        logger_->info("Analyzed patterns for " + std::to_string(entities_to_analyze.size()) +
                     " entities, discovered " + std::to_string(all_patterns.size()) + " patterns");

    } catch (const std::exception& e) {
        logger_->error("Error during pattern analysis: " + std::string(e.what()));
    }

    return all_patterns;
}

std::vector<std::shared_ptr<Pattern>> PatternRecognitionEngine::get_patterns(PatternType pattern_type,
                                                                           double min_confidence) {
    std::vector<std::shared_ptr<Pattern>> matching_patterns;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [_, pattern] : discovered_patterns_) {
        if (pattern->pattern_type == pattern_type && pattern->strength >= min_confidence) {
            matching_patterns.push_back(pattern);
        }
    }

    // Sort by strength (highest first)
    std::sort(matching_patterns.begin(), matching_patterns.end(),
        [](const std::shared_ptr<Pattern>& a, const std::shared_ptr<Pattern>& b) {
            return a->strength > b->strength;
        });

    return matching_patterns;
}

std::optional<std::shared_ptr<Pattern>> PatternRecognitionEngine::get_pattern(const std::string& pattern_id) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = discovered_patterns_.find(pattern_id);
    if (it != discovered_patterns_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::pair<std::shared_ptr<Pattern>, double>> PatternRecognitionEngine::apply_patterns(
    const PatternDataPoint& data_point) {

    std::vector<std::pair<std::shared_ptr<Pattern>, double>> applicable_patterns;

    std::lock_guard<std::mutex> lock(data_mutex_);
    for (const auto& [_, pattern] : discovered_patterns_) {
        double relevance_score = 0.0;

        // Calculate relevance based on pattern type
        switch (pattern->pattern_type) {
            case PatternType::DECISION_PATTERN: {
                // Check if data point matches decision pattern criteria
                auto decision_pattern = std::static_pointer_cast<DecisionPattern>(pattern);
                if (decision_pattern->agent_id == data_point.entity_id) {
                    relevance_score = pattern->strength;
                }
                break;
            }
            case PatternType::BEHAVIOR_PATTERN: {
                // Check behavior pattern relevance
                auto behavior_pattern = std::static_pointer_cast<BehaviorPattern>(pattern);
                if (behavior_pattern->agent_id == data_point.entity_id) {
                    relevance_score = pattern->strength;
                }
                break;
            }
            case PatternType::CORRELATION_PATTERN: {
                // Check if data point contains correlated variables
                auto correlation_pattern = std::static_pointer_cast<CorrelationPattern>(pattern);
                if (data_point.numerical_features.count(correlation_pattern->variable_a) &&
                    data_point.numerical_features.count(correlation_pattern->variable_b)) {
                    relevance_score = pattern->strength;
                }
                break;
            }
            default:
                relevance_score = pattern->strength * 0.5; // Default relevance
                break;
        }

        if (relevance_score > 0.3) { // Minimum relevance threshold
            applicable_patterns.emplace_back(pattern, relevance_score);
        }
    }

    // Sort by relevance (highest first)
    std::sort(applicable_patterns.begin(), applicable_patterns.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    return applicable_patterns;
}

nlohmann::json PatternRecognitionEngine::get_analysis_stats() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    std::unordered_map<int, size_t> pattern_type_counts;
    for (const auto& [_, pattern] : discovered_patterns_) {
        pattern_type_counts[static_cast<int>(pattern->pattern_type)]++;
    }

    return {
        {"total_data_points", static_cast<size_t>(total_data_points_)},
        {"total_patterns", static_cast<size_t>(total_patterns_discovered_)},
        {"active_entities", entity_data_.size()},
        {"pattern_types", pattern_type_counts},
        {"config", config_.to_json()}
    };
}

std::string PatternRecognitionEngine::export_patterns(PatternType pattern_type, const std::string& format) {
    auto patterns = get_patterns(pattern_type, 0.0);

    if (format == "json") {
        nlohmann::json export_json = nlohmann::json::array();
        for (const auto& pattern : patterns) {
            export_json.push_back(pattern->to_json());
        }
        return export_json.dump(2);
    }

    // Default to JSON
    return "{}";
}

size_t PatternRecognitionEngine::cleanup_old_data() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto cutoff_time = std::chrono::system_clock::now() - config_.data_retention_period;
    size_t removed_count = 0;

    // Clean up old data points
    for (auto& [entity_id, data_queue] : entity_data_) {
        while (!data_queue.empty() && data_queue.front().timestamp < cutoff_time) {
            data_queue.pop_front();
            removed_count++;
        }
    }

    // Clean up empty entity queues
    for (auto it = entity_data_.begin(); it != entity_data_.end(); ) {
        if (it->second.empty()) {
            it = entity_data_.erase(it);
        } else {
            ++it;
        }
    }

    // Clean up old patterns (older than retention period)
    for (auto it = discovered_patterns_.begin(); it != discovered_patterns_.end(); ) {
        if (it->second->last_updated < cutoff_time) {
            it = discovered_patterns_.erase(it);
        } else {
            ++it;
        }
    }

    logger_->info("Cleaned up " + std::to_string(removed_count) + " old data points");
    return removed_count;
}

// Pattern Analysis Implementations

std::vector<std::shared_ptr<DecisionPattern>> PatternRecognitionEngine::analyze_decision_patterns(const std::string& entity_id) {
    std::vector<std::shared_ptr<DecisionPattern>> patterns;

    auto data_points = get_recent_data(entity_id, 500);

    // Group decisions by type and find common factors
    std::unordered_map<DecisionType, std::vector<std::vector<std::string>>> decision_factors;

    for (const auto& dp : data_points) {
        if (dp.categorical_features.count("decision_type")) {
            DecisionType decision_type = string_to_decision_type(dp.categorical_features.at("decision_type"));

            std::vector<std::string> factors;
            for (const auto& [key, _] : dp.numerical_features) {
                if (key.find("factor_") == 0 && key.find("_weight") != std::string::npos) {
                    // Extract factor name from key like "factor_0_weight"
                    size_t start = key.find('_') + 1;
                    size_t end = key.rfind('_');
                    if (start < end) {
                        factors.push_back(key.substr(start, end - start));
                    }
                }
            }

            decision_factors[decision_type].push_back(factors);
        }
    }

    // Find patterns in decision factors
    for (const auto& [decision_type, factor_groups] : decision_factors) {
        if (factor_groups.size() < config_.min_pattern_occurrences) continue;

        // Find common factors across decisions
        std::unordered_map<std::string, size_t> factor_counts;
        for (const auto& factors : factor_groups) {
            for (const std::string& factor : factors) {
                factor_counts[factor]++;
            }
        }

        std::vector<std::string> common_factors;
        for (const auto& [factor, count] : factor_counts) {
            if (count >= config_.min_pattern_occurrences) {
                common_factors.push_back(factor);
            }
        }

        if (!common_factors.empty()) {
            auto pattern = std::make_shared<DecisionPattern>(
                generate_pattern_id(PatternType::DECISION_PATTERN, entity_id),
                entity_id, decision_type, common_factors);

            pattern->occurrences = factor_groups.size();
            pattern->strength = std::min(1.0, factor_groups.size() / 100.0); // Normalize
            pattern->confidence = PatternConfidence::HIGH;
            pattern->impact = PatternImpact::MEDIUM;

            patterns.push_back(pattern);
        }
    }

    return patterns;
}

std::vector<std::shared_ptr<BehaviorPattern>> PatternRecognitionEngine::analyze_behavior_patterns(const std::string& entity_id) {
    std::vector<std::shared_ptr<BehaviorPattern>> patterns;

    auto data_points = get_recent_data(entity_id, 200);

    // Group by behavior type
    std::unordered_map<std::string, std::vector<double>> behavior_series;

    for (const auto& dp : data_points) {
        if (dp.categorical_features.count("behavior_type") &&
            dp.numerical_features.count("behavior_value")) {

            std::string behavior_type = dp.categorical_features.at("behavior_type");
            double value = dp.numerical_features.at("behavior_value");

            behavior_series[behavior_type].push_back(value);
        }
    }

    // Analyze each behavior series
    for (const auto& [behavior_type, values] : behavior_series) {
        if (values.size() < 10) continue; // Need minimum data points

        auto pattern = std::make_shared<BehaviorPattern>(
            generate_pattern_id(PatternType::BEHAVIOR_PATTERN, entity_id),
            entity_id, behavior_type);

        // Add all values to calculate statistics
        for (double value : values) {
            pattern->add_value(value);
        }

        // Check if this represents a stable behavior pattern
        double coefficient_of_variation = pattern->standard_deviation / std::abs(pattern->mean_value);
        if (coefficient_of_variation < 0.2) { // Low variability indicates stable pattern
            pattern->confidence = PatternConfidence::HIGH;
            pattern->impact = PatternImpact::MEDIUM;
            pattern->metadata["coefficient_of_variation"] = std::to_string(coefficient_of_variation);

            patterns.push_back(pattern);
        }
    }

    return patterns;
}

std::vector<std::shared_ptr<AnomalyPattern>> PatternRecognitionEngine::detect_anomalies(const std::string& entity_id) {
    std::vector<std::shared_ptr<AnomalyPattern>> anomalies;

    auto data_points = get_recent_data(entity_id, 100);

    // Calculate baseline statistics for each feature
    std::unordered_map<std::string, std::vector<double>> feature_values;

    for (const auto& dp : data_points) {
        for (const auto& [feature, value] : dp.numerical_features) {
            feature_values[feature].push_back(value);
        }
    }

    // Calculate baseline stats (use first 80% as baseline)
    std::unordered_map<std::string, std::pair<double, double>> baselines; // mean, stddev

    for (const auto& [feature, values] : feature_values) {
        if (values.size() < 10) continue;

        size_t baseline_size = values.size() * 4 / 5; // 80% for baseline
        std::vector<double> baseline_values(values.begin(), values.begin() + static_cast<ptrdiff_t>(baseline_size));

        double mean = calculate_mean(baseline_values);
        double stddev = calculate_standard_deviation(baseline_values, mean);

        baselines[feature] = {mean, stddev};
    }

    // Check recent points for anomalies (last 20%)
    size_t anomaly_check_start = data_points.size() * 4 / 5;
    for (size_t i = anomaly_check_start; i < data_points.size(); ++i) {
        const auto& dp = data_points[i];

        for (const auto& [feature, value] : dp.numerical_features) {
            auto baseline_it = baselines.find(feature);
            if (baseline_it != baselines.end()) {
                auto [mean, stddev] = baseline_it->second;
                double zscore = calculate_zscore(value, mean, stddev);

                if (std::abs(zscore) > 3.0) { // 3-sigma rule for anomalies
                    auto anomaly = std::make_shared<AnomalyPattern>(
                        generate_pattern_id(PatternType::ANOMALY_PATTERN, entity_id),
                        "numerical_anomaly", dp.entity_id,
                        std::min(1.0, std::abs(zscore) / 5.0)); // Normalize score

                    anomaly->anomaly_indicators = {
                        feature + " value " + std::to_string(value) + " is " +
                        std::to_string(std::abs(zscore)) + " standard deviations from mean"
                    };
                    anomaly->anomaly_time = dp.timestamp;
                    anomaly->confidence = PatternConfidence::HIGH;
                    anomaly->impact = std::abs(zscore) > 5.0 ? PatternImpact::CRITICAL : PatternImpact::HIGH;

                    anomalies.push_back(anomaly);
                }
            }
        }
    }

    return anomalies;
}

std::vector<std::shared_ptr<TrendPattern>> PatternRecognitionEngine::analyze_trends(const std::string& entity_id) {
    std::vector<std::shared_ptr<TrendPattern>> trends;

    auto data_points = get_recent_data(entity_id, 50);

    // Group by metric/feature
    std::unordered_map<std::string, std::vector<std::pair<double, std::chrono::system_clock::time_point>>> time_series;

    for (const auto& dp : data_points) {
        for (const auto& [feature, value] : dp.numerical_features) {
            time_series[feature].emplace_back(value, dp.timestamp);
        }
    }

    // Analyze trends for each metric
    for (const auto& [metric, values] : time_series) {
        if (values.size() < 10) continue;

        // Convert to time-value pairs for trend analysis
        std::vector<std::pair<double, std::chrono::system_clock::time_point>> series;
        for (size_t i = 0; i < values.size(); ++i) {
            series.emplace_back(values[i].first, values[i].second);
        }

        // Linear trend detection - to be implemented when needed
    }

    return trends;
}

std::vector<std::shared_ptr<CorrelationPattern>> PatternRecognitionEngine::analyze_correlations(const std::string& entity_id) {
    auto data_points = get_recent_data(entity_id, 100);
    return calculate_correlations(data_points);
}

std::vector<std::shared_ptr<SequencePattern>> PatternRecognitionEngine::analyze_sequences(const std::string& entity_id) {
    std::vector<std::shared_ptr<SequencePattern>> sequences;

    auto data_points = get_recent_data(entity_id, 200);

    // Convert data points to event sequences
    std::vector<std::string> events;
    for (const auto& dp : data_points) {
        std::string event = dp.entity_id;
        if (dp.categorical_features.count("activity_type")) {
            event += ":" + dp.categorical_features.at("activity_type");
        } else if (dp.categorical_features.count("decision_type")) {
            event += ":" + dp.categorical_features.at("decision_type");
        }
        events.push_back(event);
    }

    // Find frequent sequences (simplified implementation)
    auto frequent_sequences = find_frequent_sequences(events, config_.min_pattern_occurrences);

    for (const auto& seq_str : frequent_sequences) {
        // Parse sequence string back to vector
        std::vector<std::string> seq;
        std::stringstream ss(seq_str);
        std::string item;
        while (std::getline(ss, item, ',')) {
            seq.push_back(item);
        }

        auto pattern = std::make_shared<SequencePattern>(
            generate_pattern_id(PatternType::SEQUENCE_PATTERN, entity_id),
            seq);

        pattern->support = 0.1; // Placeholder
        pattern->confidence = 0.8; // Placeholder
        pattern->occurrences = config_.min_pattern_occurrences;

        sequences.push_back(pattern);
    }

    return sequences;
}

// Helper method implementations

std::string PatternRecognitionEngine::generate_pattern_id(PatternType type, const std::string& entity_id) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return "pattern_" + std::to_string(static_cast<int>(type)) + "_" + entity_id + "_" + std::to_string(ms);
}

bool PatternRecognitionEngine::is_pattern_significant(const std::shared_ptr<Pattern>& pattern) const {
    return pattern->occurrences >= config_.min_pattern_occurrences &&
           pattern->strength >= config_.min_pattern_confidence;
}

std::vector<PatternDataPoint> PatternRecognitionEngine::get_recent_data(const std::string& entity_id, size_t count) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = entity_data_.find(entity_id);
    if (it == entity_data_.end()) {
        return {};
    }

    const auto& queue = it->second;
    size_t start_idx = (queue.size() > count) ? queue.size() - count : 0;

    std::vector<PatternDataPoint> result;
    for (size_t i = start_idx; i < queue.size(); ++i) {
        result.push_back(queue[i]);
    }

    return result;
}

// Statistical helper implementations

double PatternRecognitionEngine::calculate_mean(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}

double PatternRecognitionEngine::calculate_standard_deviation(const std::vector<double>& values, double mean) {
    if (values.size() <= 1) return 0.0;

    double variance = 0.0;
    for (double value : values) {
        variance += std::pow(value - mean, 2);
    }

    return std::sqrt(variance / (values.size() - 1));
}

double PatternRecognitionEngine::calculate_pearson_correlation(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size() || x.size() < 2) return 0.0;

    double sum_x = std::accumulate(x.begin(), x.end(), 0.0);
    double sum_y = std::accumulate(y.begin(), y.end(), 0.0);
    double sum_xy = 0.0;
    double sum_x2 = 0.0;
    double sum_y2 = 0.0;

    for (size_t i = 0; i < x.size(); ++i) {
        sum_xy += x[i] * y[i];
        sum_x2 += x[i] * x[i];
        sum_y2 += y[i] * y[i];
    }

    double n = x.size();
    double numerator = n * sum_xy - sum_x * sum_y;
    double denominator = std::sqrt((n * sum_x2 - sum_x * sum_x) * (n * sum_y2 - sum_y * sum_y));

    return denominator == 0.0 ? 0.0 : numerator / denominator;
}

double PatternRecognitionEngine::calculate_zscore(double value, double mean, double stddev) {
    return stddev == 0.0 ? 0.0 : (value - mean) / stddev;
}

// Placeholder implementations for complex algorithms
std::optional<std::shared_ptr<TrendPattern>> PatternRecognitionEngine::detect_linear_trend(
    const std::vector<PatternDataPoint>& data_points, const std::string& metric) {

    if (data_points.size() < 5) return std::nullopt;

    // Extract time series from data points
    std::vector<std::pair<double, std::chrono::system_clock::time_point>> series;
    for (const auto& dp : data_points) {
        if (dp.numerical_features.count(metric)) {
            series.emplace_back(dp.numerical_features.at(metric), dp.timestamp);
        }
    }

    if (series.size() < 5) return std::nullopt;

    // Simple linear regression
    size_t n = series.size();
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;

    for (size_t i = 0; i < n; ++i) {
        double x = i; // Use index as time
        double y = series[i].first;

        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }

    double denominator = (n * sum_x2 - sum_x * sum_x);
    if (denominator == 0.0) return std::nullopt;

    double slope = (n * sum_xy - sum_x * sum_y) / denominator;

    if (std::abs(slope) > 0.01) { // Significant slope
        auto trend = std::make_shared<TrendPattern>(
            generate_pattern_id(PatternType::TREND_PATTERN, "system"),
            slope > 0 ? "increasing" : "decreasing", metric, slope);

        trend->r_squared = 0.8; // Placeholder
        trend->confidence = PatternConfidence::MEDIUM;
        trend->impact = PatternImpact::LOW;

        return trend;
    }

    return std::nullopt;
}

std::vector<std::shared_ptr<CorrelationPattern>> PatternRecognitionEngine::calculate_correlations(
    const std::vector<PatternDataPoint>& data_points) {

    std::vector<std::shared_ptr<CorrelationPattern>> correlations;

    // Get all numerical features
    std::unordered_set<std::string> all_features;
    for (const auto& dp : data_points) {
        for (const auto& [feature, _] : dp.numerical_features) {
            all_features.insert(feature);
        }
    }

    std::vector<std::string> feature_list(all_features.begin(), all_features.end());

    // Calculate correlations between all pairs
    for (size_t i = 0; i < feature_list.size(); ++i) {
        for (size_t j = i + 1; j < feature_list.size(); ++j) {
            const std::string& var_a = feature_list[i];
            const std::string& var_b = feature_list[j];

            std::vector<double> values_a, values_b;
            for (const auto& dp : data_points) {
                auto it_a = dp.numerical_features.find(var_a);
                auto it_b = dp.numerical_features.find(var_b);
                if (it_a != dp.numerical_features.end() && it_b != dp.numerical_features.end()) {
                    values_a.push_back(it_a->second);
                    values_b.push_back(it_b->second);
                }
            }

            if (values_a.size() >= 10) { // Minimum sample size
                double correlation = calculate_pearson_correlation(values_a, values_b);

                if (std::abs(correlation) > 0.5) { // Significant correlation
                    auto pattern = std::make_shared<CorrelationPattern>(
                        generate_pattern_id(PatternType::CORRELATION_PATTERN, "system"),
                        var_a, var_b, correlation);

                    pattern->sample_size = values_a.size();
                    pattern->confidence = PatternConfidence::MEDIUM;
                    pattern->impact = PatternImpact::LOW;

                    correlations.push_back(pattern);
                }
            }
        }
    }

    return correlations;
}

std::vector<std::string> PatternRecognitionEngine::find_frequent_sequences(
    const std::vector<std::string>& events, size_t min_occurrences) {

    std::unordered_map<std::string, size_t> sequence_counts;

    // Simple approach: look for consecutive pairs
    for (size_t i = 0; i < events.size() - 1; ++i) {
        std::string sequence = events[i] + "," + events[i + 1];
        sequence_counts[sequence]++;
    }

    std::vector<std::string> frequent_sequences;
    for (const auto& [sequence, count] : sequence_counts) {
        if (count >= min_occurrences) {
            frequent_sequences.push_back(sequence);
        }
    }

    return frequent_sequences;
}

void PatternRecognitionEngine::analysis_worker() {
    logger_->info("Pattern recognition analysis worker started");

    while (running_) {
        std::unique_lock<std::mutex> lock(analysis_cv_mutex_);

        // Wait for analysis interval or shutdown
        analysis_cv_.wait_for(lock, std::chrono::minutes(30)); // Analyze every 30 minutes

        if (!running_) break;

        try {
            // Perform batch analysis
            analyze_patterns();

            // Cleanup old data periodically
            cleanup_old_data();

        } catch (const std::exception& e) {
            logger_->error("Error in analysis worker: " + std::string(e.what()));
        }
    }

    logger_->info("Pattern recognition analysis worker stopped");
}

bool PatternRecognitionEngine::persist_pattern(const std::shared_ptr<Pattern>& pattern) {
    logger_->debug("Persisting pattern: {}", pattern->pattern_id);
    return true;
}

bool PatternRecognitionEngine::persist_data_point(const PatternDataPoint& data_point) {
    logger_->debug("Persisting data point for: {}", data_point.entity_id);
    return true;
}

std::vector<std::shared_ptr<Pattern>> PatternRecognitionEngine::load_patterns(PatternType type) {
    logger_->debug("Loading patterns of type: {}", std::to_string(static_cast<int>(type)));
    return {};
}

std::vector<PatternDataPoint> PatternRecognitionEngine::load_data_points(const std::string& entity_id) {
    logger_->debug("Loading data points for: {}", entity_id);
    return {};
}

} // namespace regulens
