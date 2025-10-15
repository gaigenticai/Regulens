/**
 * MCDA Advanced
 * Advanced Multi-Criteria Decision Analysis with multiple algorithms and sensitivity analysis
 */

#ifndef REGULENS_MCDA_ADVANCED_HPP
#define REGULENS_MCDA_ADVANCED_HPP

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <map>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {
namespace decisions {

struct Criterion {
    std::string id;
    std::string name;
    std::string description;
    std::string type; // 'benefit' or 'cost'
    double weight = 1.0;
    std::string unit;
    nlohmann::json metadata;
};

struct Alternative {
    std::string id;
    std::string name;
    std::string description;
    std::map<std::string, double> scores; // criterion_id -> score
    nlohmann::json metadata;
};

struct MCDAModel {
    std::string model_id;
    std::string name;
    std::string description;
    std::string algorithm; // 'ahp', 'topsis', 'promethee', 'electre'
    std::string normalization_method; // 'minmax', 'zscore', 'vector', 'sum'
    std::string aggregation_method; // 'weighted_sum', 'geometric_mean'
    std::vector<Criterion> criteria;
    std::vector<Alternative> alternatives;
    std::string created_by;
    bool is_public = false;
    std::vector<std::string> tags;
    std::chrono::system_clock::time_point created_at;
    nlohmann::json metadata;
};

struct MCDAResult {
    std::string calculation_id;
    std::string model_id;
    std::string algorithm_used;
    std::vector<std::pair<std::string, double>> ranking; // alternative_id -> score
    std::vector<double> normalized_weights;
    nlohmann::json intermediate_steps;
    nlohmann::json algorithm_specific_results;
    double quality_score = 0.0;
    long execution_time_ms;
    std::chrono::system_clock::time_point calculated_at;
    nlohmann::json metadata;
};

struct SensitivityAnalysis {
    std::string analysis_id;
    std::string model_id;
    std::string parameter_varied;
    std::string parameter_type; // 'criterion_weight', 'alternative_score', 'threshold'
    nlohmann::json variation_range;
    nlohmann::json baseline_result;
    nlohmann::json impact_results;
    nlohmann::json statistical_summary;
    long analysis_time_ms;
    std::chrono::system_clock::time_point created_at;
};

struct AlgorithmConfig {
    std::string config_id;
    std::string algorithm_name;
    std::string config_name;
    nlohmann::json config_parameters;
    std::string description;
    bool is_default = false;
    std::string created_by;
    int usage_count = 0;
    std::chrono::system_clock::time_point created_at;
};

class MCDAAdvanced {
public:
    MCDAAdvanced(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger
    );

    ~MCDAAdvanced();

    // Model management
    std::optional<MCDAModel> create_model(const MCDAModel& model);
    std::optional<MCDAModel> get_model(const std::string& model_id);
    std::vector<MCDAModel> get_models(const std::string& user_id = "", bool include_public = true, int limit = 50);
    bool update_model(const std::string& model_id, const std::string& user_id, const nlohmann::json& updates);
    bool delete_model(const std::string& model_id, const std::string& user_id);

    // MCDA evaluation
    std::optional<MCDAResult> evaluate_model(
        const std::string& model_id,
        const std::string& user_id,
        const std::optional<nlohmann::json>& runtime_parameters = std::nullopt
    );

    // Algorithm implementations
    MCDAResult evaluate_ahp(const MCDAModel& model, const nlohmann::json& parameters = nlohmann::json());
    MCDAResult evaluate_topsis(const MCDAModel& model, const nlohmann::json& parameters = nlohmann::json());
    MCDAResult evaluate_promethee(const MCDAModel& model, const nlohmann::json& parameters = nlohmann::json());
    MCDAResult evaluate_electre(const MCDAModel& model, const nlohmann::json& parameters = nlohmann::json());

    // Normalization methods
    std::vector<std::vector<double>> normalize_minmax(const std::vector<std::vector<double>>& matrix);
    std::vector<std::vector<double>> normalize_zscore(const std::vector<std::vector<double>>& matrix);
    std::vector<std::vector<double>> normalize_vector(const std::vector<std::vector<double>>& matrix);
    std::vector<std::vector<double>> normalize_sum(const std::vector<std::vector<double>>& matrix);

    // Sensitivity analysis
    std::optional<SensitivityAnalysis> run_sensitivity_analysis(
        const std::string& model_id,
        const std::string& parameter_varied,
        const std::string& parameter_type,
        const nlohmann::json& variation_range,
        const std::string& user_id
    );

    // Algorithm configurations
    std::optional<AlgorithmConfig> create_algorithm_config(const AlgorithmConfig& config);
    std::vector<AlgorithmConfig> get_algorithm_configs(const std::string& algorithm_name = "");
    std::optional<AlgorithmConfig> get_default_config(const std::string& algorithm_name);

    // Template management
    std::vector<nlohmann::json> get_model_templates(const std::string& category = "");
    nlohmann::json create_model_from_template(
        const std::string& template_id,
        const std::string& model_name,
        const std::string& user_id
    );

    // Result retrieval and caching
    std::optional<MCDAResult> get_calculation_result(const std::string& calculation_id);
    std::vector<MCDAResult> get_model_history(const std::string& model_id, int limit = 10);

    // Export functionality
    nlohmann::json export_result(
        const std::string& calculation_id,
        const std::string& format, // 'pdf', 'excel', 'csv', 'json'
        const std::string& user_id
    );

    // Analytics and reporting
    nlohmann::json get_mcda_analytics(const std::string& time_range = "30d", const std::string& user_id = "");
    nlohmann::json get_algorithm_performance(const std::string& algorithm_name, const std::string& time_range = "30d");

    // User preferences
    bool save_user_preferences(const std::string& user_id, const nlohmann::json& preferences);
    nlohmann::json get_user_preferences(const std::string& user_id);

    // Comments and collaboration
    bool add_comment(
        const std::string& calculation_id,
        const std::string& author,
        const std::string& comment_text,
        const std::string& comment_type = "general",
        const std::optional<std::string>& parent_comment_id = std::nullopt
    );

    std::vector<nlohmann::json> get_comments(const std::string& calculation_id);

    // Utility methods
    bool validate_model(const MCDAModel& model);
    double calculate_consistency_ratio(const std::vector<std::vector<double>>& pairwise_matrix);
    std::vector<double> normalize_weights(const std::vector<double>& weights);
    nlohmann::json calculate_model_statistics(const MCDAModel& model);

    // Configuration
    void set_default_algorithm(const std::string& algorithm);
    void set_cache_enabled(bool enabled);
    void set_max_calculation_time_ms(int max_time);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;

    // Configuration
    std::string default_algorithm_ = "ahp";
    bool cache_enabled_ = true;
    int max_calculation_time_ms_ = 30000; // 30 seconds
    int cache_ttl_seconds_ = 86400; // 24 hours

    // Internal methods
    std::string generate_uuid();
    nlohmann::json format_timestamp(const std::chrono::system_clock::time_point& timestamp);

    // AHP implementation
    MCDAResult ahp_implementation(const MCDAModel& model, const nlohmann::json& parameters);
    std::vector<std::vector<double>> build_pairwise_matrix(const std::vector<Criterion>& criteria, const std::vector<Alternative>& alternatives);
    std::vector<double> calculate_priorities(const std::vector<std::vector<double>>& matrix);

    // TOPSIS implementation
    MCDAResult topsis_implementation(const MCDAModel& model, const nlohmann::json& parameters);
    std::vector<double> calculate_ideal_solutions(const std::vector<std::vector<double>>& normalized_matrix, const std::vector<std::string>& criterion_types);
    double calculate_separation_measure(const std::vector<double>& alternative_scores, const std::vector<double>& ideal_solution);

    // PROMETHEE implementation
    MCDAResult promethee_implementation(const MCDAModel& model, const nlohmann::json& parameters);
    std::vector<std::vector<double>> calculate_preference_functions(const std::vector<std::vector<double>>& matrix, const nlohmann::json& parameters);
    std::vector<double> calculate_flows(const std::vector<std::vector<double>>& preference_matrix);

    // ELECTRE implementation
    MCDAResult electre_implementation(const MCDAModel& model, const nlohmann::json& parameters);
    std::vector<std::vector<bool>> build_concordance_matrix(const std::vector<std::vector<double>>& normalized_matrix, const std::vector<double>& weights);
    std::vector<std::vector<bool>> build_discordance_matrix(const std::vector<std::vector<double>>& normalized_matrix);

    // Sensitivity analysis helpers
    SensitivityAnalysis analyze_weight_sensitivity(const MCDAModel& model, const std::string& criterion_id, const nlohmann::json& variation_range);
    SensitivityAnalysis analyze_score_sensitivity(const MCDAModel& model, const std::string& alternative_id, const std::string& criterion_id, const nlohmann::json& variation_range);
    nlohmann::json calculate_statistical_summary(const std::vector<double>& values);

    // Database operations
    bool store_calculation_result(const MCDAResult& result);
    bool store_sensitivity_analysis(const SensitivityAnalysis& analysis);
    bool store_visualization_data(const std::string& calculation_id, const std::string& visualization_type, const nlohmann::json& data);

    std::optional<nlohmann::json> load_cached_calculation(const std::string& model_id, const std::string& parameters_hash);
    void cleanup_expired_cache();

    // Performance monitoring
    void record_performance_metric(
        const std::string& operation_type,
        const std::string& algorithm_used,
        long execution_time_ms,
        int alternatives_count,
        int criteria_count,
        const std::string& error_message = ""
    );

    // Validation helpers
    bool validate_criteria(const std::vector<Criterion>& criteria);
    bool validate_alternatives(const std::vector<Alternative>& alternatives, const std::vector<Criterion>& criteria);
    bool validate_weights(const std::vector<double>& weights);

    // Matrix operations
    std::vector<std::vector<double>> transpose_matrix(const std::vector<std::vector<double>>& matrix);
    std::vector<std::vector<double>> multiply_matrices(const std::vector<std::vector<double>>& a, const std::vector<std::vector<double>>& b);
    std::vector<double> multiply_matrix_vector(const std::vector<std::vector<double>>& matrix, const std::vector<double>& vector);

    // Statistical utilities
    double calculate_mean(const std::vector<double>& values);
    double calculate_standard_deviation(const std::vector<double>& values, double mean);
    std::pair<double, double> calculate_confidence_interval(const std::vector<double>& values, double confidence_level = 0.95);

    // Export helpers
    nlohmann::json export_to_pdf(const MCDAResult& result, const MCDAModel& model);
    nlohmann::json export_to_excel(const MCDAResult& result, const MCDAModel& model);
    nlohmann::json export_to_csv(const MCDAResult& result, const MCDAModel& model);

    // Logging helpers
    void log_calculation_start(const std::string& model_id, const std::string& algorithm, int alternatives_count, int criteria_count);
    void log_calculation_complete(const std::string& calculation_id, long execution_time_ms, bool success);
    void log_algorithm_error(const std::string& algorithm, const std::string& error_message);
};

} // namespace decisions
} // namespace regulens

#endif // REGULENS_MCDA_ADVANCED_HPP
