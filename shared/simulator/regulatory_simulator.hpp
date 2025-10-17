/**
 * Regulatory Simulator Service
 * Simulates impact of hypothetical regulatory changes on compliance frameworks
 * Production-grade what-if analysis with comprehensive impact assessment
 */

#ifndef REGULENS_REGULATORY_SIMULATOR_HPP
#define REGULENS_REGULATORY_SIMULATOR_HPP

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <future>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {
namespace simulator {

struct SimulationScenario {
    std::string scenario_id;
    std::string scenario_name;
    std::string description;
    std::string scenario_type; // 'regulatory_change', 'market_change', 'operational_change'
    nlohmann::json regulatory_changes;
    nlohmann::json impact_parameters;
    nlohmann::json baseline_data;
    nlohmann::json test_data;
    std::string created_by;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
    bool is_template = false;
    bool is_active = true;
    std::vector<std::string> tags;
    nlohmann::json metadata;
    int estimated_runtime_seconds = 0;
    int max_concurrent_simulations = 1;
};

struct SimulationExecution {
    std::string execution_id;
    std::string scenario_id;
    std::string user_id;
    std::string execution_status; // 'pending', 'running', 'completed', 'failed', 'cancelled'
    nlohmann::json execution_parameters;
    std::optional<std::chrono::system_clock::time_point> started_at;
    std::optional<std::chrono::system_clock::time_point> completed_at;
    std::optional<std::chrono::system_clock::time_point> cancelled_at;
    std::optional<std::string> error_message;
    double progress_percentage = 0.0;
    std::chrono::system_clock::time_point created_at;
    nlohmann::json metadata;
};

struct SimulationRequest {
    std::string scenario_id;
    std::string user_id;
    std::optional<nlohmann::json> custom_parameters;
    std::optional<nlohmann::json> test_data_override;
    bool async_execution = true;
    int priority = 1; // 1=low, 5=high
};

struct SimulationResult {
    std::string result_id;
    std::string execution_id;
    std::string scenario_id;
    std::string user_id;
    std::string result_type; // 'impact_analysis', 'compliance_check', 'risk_assessment'
    nlohmann::json impact_summary;
    nlohmann::json detailed_results;
    nlohmann::json affected_entities;
    nlohmann::json recommendations;
    nlohmann::json risk_assessment;
    nlohmann::json cost_impact;
    nlohmann::json compliance_impact;
    nlohmann::json operational_impact;
    std::chrono::system_clock::time_point created_at;
    nlohmann::json metadata;
};

struct ImpactMetrics {
    int total_entities_affected = 0;
    int high_risk_entities = 0;
    int medium_risk_entities = 0;
    int low_risk_entities = 0;
    double compliance_score_change = 0.0;
    double risk_score_change = 0.0;
    double operational_cost_increase = 0.0;
    double estimated_implementation_time_days = 0.0;
    std::vector<std::string> critical_violations;
    std::vector<std::string> recommended_actions;
};

struct SimulationTemplate {
    std::string template_id;
    std::string template_name;
    std::string template_description;
    std::string category; // 'aml', 'kyc', 'fraud', 'privacy', 'reporting'
    std::string jurisdiction; // 'us', 'eu', 'global', etc.
    std::string regulatory_body; // 'sec', 'finra', 'ecb', 'fca', etc.
    nlohmann::json template_data;
    int usage_count = 0;
    double success_rate = 0.0;
    int average_runtime_seconds = 0;
    std::string created_by;
    std::chrono::system_clock::time_point created_at;
    bool is_active = true;
    std::vector<std::string> tags;
};

class RegulatorySimulator {
public:
    RegulatorySimulator(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        StructuredLogger& logger
    );

    ~RegulatorySimulator();

    // Scenario management
    std::optional<SimulationScenario> create_scenario(const SimulationScenario& scenario, const std::string& user_id);
    std::optional<SimulationScenario> get_scenario(const std::string& scenario_id);
    std::vector<SimulationScenario> get_scenarios(const std::string& user_id = "", int limit = 50, int offset = 0);
    bool update_scenario(const std::string& scenario_id, const SimulationScenario& updates);
    bool delete_scenario(const std::string& scenario_id);

    // Template management
    std::vector<SimulationTemplate> get_templates(const std::string& category = "", const std::string& jurisdiction = "");
    std::optional<SimulationTemplate> get_template(const std::string& template_id);
    std::optional<SimulationScenario> create_scenario_from_template(const std::string& template_id, const std::string& user_id);

    // Simulation execution
    std::string run_simulation(const SimulationRequest& request);
    std::optional<SimulationExecution> get_execution_status(const std::string& execution_id);
    bool cancel_simulation(const std::string& execution_id, const std::string& user_id);

    // Results management
    std::optional<SimulationResult> get_simulation_result(const std::string& execution_id);
    std::vector<SimulationResult> get_user_simulation_history(const std::string& user_id, int limit = 50, int offset = 0);
    std::vector<nlohmann::json> get_simulation_result_details(const std::string& result_id, const std::string& detail_type = "");

    // Analytics and reporting
    nlohmann::json get_simulation_analytics(const std::string& user_id, const std::optional<std::string>& time_range = std::nullopt);
    nlohmann::json get_scenario_performance_metrics(const std::string& scenario_id);
    std::vector<std::string> get_popular_scenarios(int limit = 10);

    // Background processing
    void process_pending_simulations();
    void cleanup_old_simulations(int retention_days = 90);

    // Configuration
    void set_max_concurrent_simulations(int max_simulations);
    void set_simulation_timeout_seconds(int timeout_seconds);
    void set_result_retention_days(int days);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    StructuredLogger& logger_;

    // Configuration
    int max_concurrent_simulations_ = 5;
    int simulation_timeout_seconds_ = 3600; // 1 hour
    int result_retention_days_ = 90;
    int max_execution_history_per_user_ = 1000;

    // Internal methods
    std::string generate_uuid();
    SimulationExecution create_execution_record(const SimulationRequest& request);
    bool update_execution_status(const std::string& execution_id, const std::string& status,
                               double progress = 0.0, const std::optional<std::string>& error_message = std::nullopt);

    // Simulation execution methods
    void execute_simulation_async(const std::string& execution_id);
    SimulationResult execute_simulation_sync(const std::string& execution_id);

    // Impact analysis methods
    ImpactMetrics analyze_regulatory_impact(const SimulationScenario& scenario, const nlohmann::json& test_data);
    ImpactMetrics analyze_transaction_impact(const nlohmann::json& regulatory_changes, const nlohmann::json& transactions);
    ImpactMetrics analyze_policy_impact(const nlohmann::json& regulatory_changes, const nlohmann::json& policies);
    ImpactMetrics analyze_risk_impact(const nlohmann::json& regulatory_changes, const nlohmann::json& risk_data);

    // Result processing methods
    void store_simulation_result(const std::string& execution_id, const SimulationResult& result);
    void store_result_details(const std::string& result_id, const std::vector<nlohmann::json>& details);
    void store_simulation_analytics(const std::string& execution_id, const std::string& scenario_id, const ImpactMetrics& metrics);

    // Recommendation generation
    std::vector<std::string> generate_recommendations(const ImpactMetrics& metrics, const SimulationScenario& scenario);
    std::vector<std::string> generate_compliance_recommendations(const ImpactMetrics& metrics);
    std::vector<std::string> generate_operational_recommendations(const ImpactMetrics& metrics);

    // Utility methods
    nlohmann::json calculate_baseline_metrics(const nlohmann::json& baseline_data);
    nlohmann::json calculate_scenario_metrics(const SimulationScenario& scenario, const nlohmann::json& test_data);
    double calculate_compliance_score_change(const nlohmann::json& baseline, const nlohmann::json& scenario);
    double calculate_risk_score_change(const nlohmann::json& baseline, const nlohmann::json& scenario);

    // Data validation methods
    bool validate_scenario_data(const nlohmann::json& scenario_data);
    bool validate_test_data(const nlohmann::json& test_data);
    bool validate_regulatory_changes(const nlohmann::json& changes);

    // Background processing
    std::vector<std::string> get_pending_executions(int limit = 10);
    void process_execution(const std::string& execution_id);

    // Analytics helpers
    nlohmann::json aggregate_simulation_metrics(const std::string& user_id, const std::string& time_range);
    nlohmann::json calculate_scenario_success_rates();
    std::map<std::string, double> calculate_impact_distribution(const std::vector<SimulationResult>& results);

    // Logging helpers
    void log_simulation_start(const std::string& execution_id, const SimulationRequest& request);
    void log_simulation_complete(const std::string& execution_id, const ImpactMetrics& metrics);
    void log_simulation_error(const std::string& execution_id, const std::string& error);
};

} // namespace simulator
} // namespace regulens

#endif // REGULENS_REGULATORY_SIMULATOR_HPP
