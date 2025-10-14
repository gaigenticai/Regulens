/**
 * Function Call Debugger
 * Advanced debugging and tracing system for LLM function calls
 */

#ifndef REGULENS_FUNCTION_CALL_DEBUGGER_HPP
#define REGULENS_FUNCTION_CALL_DEBUGGER_HPP

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {
namespace llm {

struct FunctionCallTrace {
    std::string call_id;
    std::string function_name;
    nlohmann::json input_parameters;
    nlohmann::json output_result;
    nlohmann::json execution_trace;
    nlohmann::json error_details;
    int execution_time_ms;
    std::chrono::system_clock::time_point called_at;
    bool success = false;
    std::string session_id;
    std::string user_id;
    nlohmann::json metadata;
};

struct DebugSession {
    std::string session_id;
    std::string user_id;
    std::string session_name;
    std::string description;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
    bool is_active = true;
    std::vector<std::string> tags;
    nlohmann::json metadata;
};

struct FunctionCallReplay {
    std::string replay_id;
    std::string session_id;
    std::string original_call_id;
    nlohmann::json modified_parameters;
    std::string modified_function_name;
    nlohmann::json replay_result;
    nlohmann::json replay_error;
    nlohmann::json execution_trace;
    std::chrono::system_clock::time_point replayed_at;
    std::string replayed_by;
    bool success = false;
    int execution_time_ms;
    nlohmann::json metadata;
};

struct FunctionBreakpoint {
    std::string breakpoint_id;
    std::string session_id;
    std::string function_name;
    std::string condition_expression;
    nlohmann::json condition_parameters;
    std::string action; // 'pause', 'log', 'modify', 'skip'
    nlohmann::json action_parameters;
    bool is_active = true;
    int hit_count = 0;
    std::chrono::system_clock::time_point created_at;
    nlohmann::json metadata;
};

struct FunctionCallMetrics {
    std::string metric_id;
    std::string call_id;
    std::string session_id;
    std::string function_name;
    int execution_time_ms;
    int memory_usage_bytes;
    double cpu_usage_percent;
    int network_calls = 0;
    int network_bytes_transferred = 0;
    bool success = false;
    std::string error_type;
    std::chrono::system_clock::time_point recorded_at;
    nlohmann::json metadata;
};

struct FunctionCallTemplate {
    std::string template_id;
    std::string template_name;
    std::string function_name;
    nlohmann::json template_parameters;
    std::string description;
    std::string created_by;
    bool is_public = false;
    int usage_count = 0;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
};

struct FunctionCallTestCase {
    std::string test_case_id;
    std::string template_id;
    std::string test_name;
    nlohmann::json input_parameters;
    nlohmann::json expected_output;
    nlohmann::json expected_error;
    int timeout_seconds = 30;
    std::string created_by;
    bool is_active = true;
    std::optional<std::chrono::system_clock::time_point> last_run_at;
    std::optional<bool> last_run_success;
    std::chrono::system_clock::time_point created_at;
};

struct CreateSessionRequest {
    std::string session_name;
    std::string description;
    std::vector<std::string> tags;
    nlohmann::json metadata;
};

struct ReplayRequest {
    std::string original_call_id;
    nlohmann::json modified_parameters;
    std::string modified_function_name;
    int timeout_seconds = 30;
};

struct CreateBreakpointRequest {
    std::string function_name;
    std::string condition_expression;
    nlohmann::json condition_parameters;
    std::string action;
    nlohmann::json action_parameters;
};

class FunctionCallDebugger {
public:
    FunctionCallDebugger(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger
    );

    ~FunctionCallDebugger();

    // Session management
    std::optional<DebugSession> create_debug_session(const std::string& user_id, const CreateSessionRequest& request);
    std::optional<DebugSession> get_debug_session(const std::string& session_id, const std::string& user_id);
    std::vector<DebugSession> get_user_sessions(const std::string& user_id, int limit = 50);
    bool update_debug_session(const std::string& session_id, const std::string& user_id, const nlohmann::json& updates);
    bool delete_debug_session(const std::string& session_id, const std::string& user_id);

    // Function call tracing and logging
    bool log_function_call(const FunctionCallTrace& trace);
    std::vector<FunctionCallTrace> get_function_calls(
        const std::string& user_id,
        const std::string& session_id = "",
        int limit = 100,
        int offset = 0
    );
    std::optional<FunctionCallTrace> get_function_call_details(const std::string& call_id, const std::string& user_id);

    // Function call replay
    std::optional<FunctionCallReplay> replay_function_call(
        const std::string& session_id,
        const std::string& user_id,
        const ReplayRequest& request
    );
    std::vector<FunctionCallReplay> get_replay_history(const std::string& session_id, const std::string& user_id, int limit = 50);

    // Breakpoint management
    std::optional<FunctionBreakpoint> create_breakpoint(
        const std::string& session_id,
        const std::string& user_id,
        const CreateBreakpointRequest& request
    );
    std::vector<FunctionBreakpoint> get_session_breakpoints(const std::string& session_id, const std::string& user_id);
    bool update_breakpoint(const std::string& breakpoint_id, const std::string& user_id, const nlohmann::json& updates);
    bool delete_breakpoint(const std::string& breakpoint_id, const std::string& user_id);

    // Performance monitoring
    bool record_function_metrics(const FunctionCallMetrics& metrics);
    std::vector<FunctionCallMetrics> get_function_metrics(
        const std::string& function_name = "",
        const std::string& session_id = "",
        int limit = 100
    );

    // Template management
    std::optional<FunctionCallTemplate> create_template(const FunctionCallTemplate& template_data);
    std::vector<FunctionCallTemplate> get_templates(const std::string& user_id, bool include_public = true);
    std::optional<FunctionCallTemplate> get_template(const std::string& template_id);
    bool update_template(const std::string& template_id, const std::string& user_id, const nlohmann::json& updates);

    // Test case management
    std::optional<FunctionCallTestCase> create_test_case(const FunctionCallTestCase& test_case);
    std::vector<FunctionCallTestCase> get_test_cases(const std::string& template_id = "", const std::string& user_id = "");
    bool run_test_case(const std::string& test_case_id, const std::string& user_id);

    // Analytics and reporting
    nlohmann::json get_debugging_analytics(const std::string& user_id, const std::string& time_range = "7d");
    nlohmann::json get_function_performance_report(const std::string& function_name, const std::string& time_range = "30d");
    nlohmann::json get_error_analysis_report(const std::string& user_id, const std::string& time_range = "7d");

    // Export and import
    nlohmann::json export_session_data(const std::string& session_id, const std::string& user_id);
    bool import_session_data(const std::string& user_id, const nlohmann::json& session_data);

    // Utility methods
    bool validate_function_call(const nlohmann::json& function_call);
    nlohmann::json sanitize_parameters(const nlohmann::json& parameters);
    std::string generate_call_id();

    // Configuration
    void set_max_session_age_days(int days);
    void set_max_calls_per_session(int max_calls);
    void set_debug_log_level(const std::string& level);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;

    // Configuration
    int max_session_age_days_ = 30;
    int max_calls_per_session_ = 10000;
    std::string debug_log_level_ = "info";

    // Internal methods
    std::string generate_uuid();
    nlohmann::json format_timestamp(const std::chrono::system_clock::time_point& timestamp);
    bool validate_session_access(const std::string& session_id, const std::string& user_id);
    bool check_session_limits(const std::string& user_id);

    // Database operations
    bool store_function_call_trace(const FunctionCallTrace& trace);
    bool store_debug_session(const DebugSession& session);
    bool store_function_replay(const FunctionCallReplay& replay);
    bool store_breakpoint(const FunctionBreakpoint& breakpoint);
    bool store_function_metrics(const FunctionCallMetrics& metrics);

    std::optional<FunctionCallTrace> load_function_call_trace(const std::string& call_id);
    std::optional<DebugSession> load_debug_session(const std::string& session_id);

    // Replay execution
    FunctionCallReplay execute_function_replay(
        const std::string& session_id,
        const std::string& original_call_id,
        const ReplayRequest& request,
        const std::string& user_id
    );

    // Breakpoint evaluation
    bool evaluate_breakpoint_condition(
        const FunctionBreakpoint& breakpoint,
        const FunctionCallTrace& trace
    );

    // Analytics helpers
    nlohmann::json calculate_session_statistics(const std::string& user_id, const std::string& time_range);
    nlohmann::json calculate_function_performance(const std::string& function_name, const std::string& time_range);
    nlohmann::json analyze_error_patterns(const std::string& user_id, const std::string& time_range);

    // Cleanup methods
    void cleanup_expired_sessions();
    void cleanup_old_call_logs(int retention_days = 90);
    void cleanup_old_metrics(int retention_days = 30);

    // Audit logging
    void audit_debug_action(
        const std::string& action,
        const std::string& resource_type,
        const std::string& resource_id,
        const std::string& user_id,
        const std::string& session_id = "",
        const nlohmann::json& old_values = nlohmann::json(),
        const nlohmann::json& new_values = nlohmann::json()
    );

    // Validation helpers
    bool is_valid_function_name(const std::string& function_name);
    bool is_valid_session_name(const std::string& session_name);
    bool validate_parameters_schema(const nlohmann::json& parameters);

    // Performance monitoring
    void record_operation_metrics(
        const std::string& operation,
        long execution_time_ms,
        bool success,
        const std::string& details = ""
    );

    // Caching for frequently accessed data
    std::optional<nlohmann::json> get_cached_session_data(const std::string& session_id);
    void cache_session_data(const std::string& session_id, const nlohmann::json& data, int ttl_seconds = 300);

    // Utility methods for JSON operations
    nlohmann::json deep_copy_json(const nlohmann::json& source);
    nlohmann::json merge_json_objects(const nlohmann::json& base, const nlohmann::json& overlay);
    std::string hash_json_for_cache(const nlohmann::json& data);
};

} // namespace llm
} // namespace regulens

#endif // REGULENS_FUNCTION_CALL_DEBUGGER_HPP
