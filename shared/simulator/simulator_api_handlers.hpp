/**
 * Regulatory Simulator API Handlers
 * REST API endpoints for regulatory impact simulation and scenario management
 */

#ifndef REGULENS_SIMULATOR_API_HANDLERS_HPP
#define REGULENS_SIMULATOR_API_HANDLERS_HPP

#include <string>
#include <map>
#include <memory>
#include <libpq-fe.h>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include "regulatory_simulator.hpp"

namespace regulens {
namespace simulator {

class SimulatorAPIHandlers {
public:
    SimulatorAPIHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        StructuredLogger& logger,
        std::shared_ptr<RegulatorySimulator> simulator
    );

    // Scenario management endpoints
    std::string handle_create_scenario(const std::string& request_body, const std::string& user_id);
    std::string handle_get_scenarios(const std::string& user_id, const std::map<std::string, std::string>& query_params);
    std::string handle_get_scenario(const std::string& scenario_id, const std::string& user_id);
    std::string handle_update_scenario(const std::string& scenario_id, const std::string& request_body, const std::string& user_id);
    std::string handle_delete_scenario(const std::string& scenario_id, const std::string& user_id);

    // Template management endpoints
    std::string handle_get_templates(const std::map<std::string, std::string>& query_params);
    std::string handle_get_template(const std::string& template_id);
    std::string handle_create_scenario_from_template(const std::string& template_id, const std::string& user_id);

    // Simulation execution endpoints
    std::string handle_run_simulation(const std::string& request_body, const std::string& user_id);
    std::string handle_get_execution_status(const std::string& execution_id, const std::string& user_id);
    std::string handle_cancel_simulation(const std::string& execution_id, const std::string& user_id);

    // Results and analytics endpoints
    std::string handle_get_simulation_result(const std::string& execution_id, const std::string& user_id);
    std::string handle_get_simulation_history(const std::string& user_id, const std::map<std::string, std::string>& query_params);
    std::string handle_get_result_details(const std::string& result_id, const std::string& user_id, const std::map<std::string, std::string>& query_params);
    std::string handle_get_simulation_analytics(const std::string& user_id, const std::map<std::string, std::string>& query_params);
    std::string handle_get_scenario_metrics(const std::string& scenario_id, const std::string& user_id);

    // Administrative endpoints
    std::string handle_get_popular_scenarios(const std::map<std::string, std::string>& query_params);
    std::string handle_process_pending_simulations();
    std::string handle_cleanup_old_simulations(const std::map<std::string, std::string>& query_params);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    StructuredLogger& logger_;
    std::shared_ptr<RegulatorySimulator> simulator_;

    // Helper methods
    SimulationScenario parse_scenario_request(const nlohmann::json& request_json, const std::string& user_id);
    SimulationRequest parse_simulation_request(const nlohmann::json& request_json, const std::string& user_id);

    nlohmann::json format_scenario_response(const SimulationScenario& scenario);
    nlohmann::json format_execution_response(const SimulationExecution& execution);
    nlohmann::json format_result_response(const SimulationResult& result);
    nlohmann::json format_template_response(const SimulationTemplate& tmpl);

    bool validate_scenario_access(const std::string& scenario_id, const std::string& user_id);
    bool validate_execution_access(const std::string& execution_id, const std::string& user_id);
    bool validate_admin_access(const std::string& user_id);

    std::string create_error_response(const std::string& message, int status_code = 400);
    std::string create_success_response(const nlohmann::json& data, const std::string& message = "");

    // Database query helpers
    std::vector<SimulationScenario> query_scenarios_paginated(const std::string& user_id,
                                                            const std::map<std::string, std::string>& filters,
                                                            int limit, int offset);
    std::vector<SimulationExecution> query_user_executions(const std::string& user_id, int limit, int offset);
    std::vector<SimulationResult> query_simulation_results(const std::string& user_id, int limit, int offset);

    // Validation helpers
    bool validate_scenario_data(const nlohmann::json& scenario_data);
    bool validate_regulatory_changes(const nlohmann::json& changes);
    bool validate_simulation_parameters(const nlohmann::json& params);

    // Utility methods
    std::map<std::string, std::string> parse_query_parameters(const std::map<std::string, std::string>& query_params);
    std::string extract_user_id_from_scenario(const std::string& scenario_id);
    std::string extract_user_id_from_execution(const std::string& execution_id);

    // Rate limiting and security
    bool check_simulation_rate_limit(const std::string& user_id);
    void record_simulation_attempt(const std::string& user_id);

    // Background processing helpers
    void trigger_background_processing();
    bool is_background_processing_enabled();

    // Caching helpers (for analytics and frequently accessed data)
    std::optional<nlohmann::json> get_cached_analytics(const std::string& cache_key, const std::string& user_id);
    void cache_analytics_result(const std::string& cache_key, const std::string& user_id, const nlohmann::json& data, int ttl_seconds = 300);

    // Metrics and monitoring
    void record_api_metrics(const std::string& endpoint, const std::string& user_id, double response_time_ms, bool success);
    void update_scenario_usage_stats(const std::string& scenario_id);
};

} // namespace simulator
} // namespace regulens

#endif // REGULENS_SIMULATOR_API_HANDLERS_HPP
