#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <optional>

#include "../../shared/logging/structured_logger.hpp"
#include "../../shared/config/configuration_manager.hpp"

namespace regulens {

/**
 * @brief Singleton test environment manager
 *
 * Manages global test state, temporary resources, and isolated testing environment.
 * Ensures tests can run without external dependencies and with predictable state.
 */
class TestEnvironment {
public:
    static TestEnvironment& get_instance();

    // Lifecycle management
    void initialize();
    void cleanup();

    // Environment control
    void set_test_mode(bool test_mode = true);
    bool is_test_mode() const;

    // Temporary file/directory management
    std::string create_temp_file(const std::string& content = "");
    std::string create_temp_directory(const std::string& prefix = "regulens_test");
    void cleanup_temp_files();

    // Mock service management
    void register_mock_service(const std::string& service_name,
                              std::shared_ptr<void> mock_service);
    std::shared_ptr<void> get_mock_service(const std::string& service_name);
    void unregister_mock_service(const std::string& service_name);

    // Configuration override for tests
    void set_config_override(const std::string& key, const std::string& value);
    void clear_config_overrides();
    std::unordered_map<std::string, std::string> get_config_overrides() const;

    // Environment variable isolation
    void set_environment_variable(const std::string& key, const std::string& value);
    void restore_environment_variables();
    std::string get_environment_variable(const std::string& key) const;

    // Test data management
    void set_test_data(const std::string& key, const nlohmann::json& data);
    nlohmann::json get_test_data(const std::string& key) const;
    void clear_test_data();

    // Isolation verification
    bool verify_isolation() const;
    std::vector<std::string> get_isolation_warnings() const;

private:
    TestEnvironment();
    ~TestEnvironment();

    // Prevent copying
    TestEnvironment(const TestEnvironment&) = delete;
    TestEnvironment& operator=(const TestEnvironment&) = delete;

    // Internal state
    bool initialized_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<ConfigurationManager> config_manager_;

    // Resource tracking
    std::vector<std::filesystem::path> temp_paths_;

    // Environment variables
    std::unordered_map<std::string, std::optional<std::string>> original_env_vars_;
};

/**
 * @brief RAII wrapper for test environment setup/cleanup
 *
 * Automatically manages test environment lifecycle within a scope.
 */
class TestEnvironmentGuard {
public:
    TestEnvironmentGuard();
    ~TestEnvironmentGuard();

    // Disable copying
    TestEnvironmentGuard(const TestEnvironmentGuard&) = delete;
    TestEnvironmentGuard& operator=(const TestEnvironmentGuard&) = delete;
};

/**
 * @brief Test database manager
 *
 * Provides isolated in-memory database functionality for testing.
 */
class TestDatabaseManager {
public:
    static TestDatabaseManager& get_instance();

    void initialize();
    void cleanup();

    // Schema management
    bool create_test_schema();
    bool drop_test_schema();

    // Data operations
    bool insert_test_data(const std::string& table, const nlohmann::json& data);
    bool query_test_data(const std::string& query, nlohmann::json& result);
    bool update_test_data(const std::string& table, const nlohmann::json& data,
                         const std::string& where_clause);
    bool delete_test_data(const std::string& table, const std::string& where_clause);

    // State management
    void reset_database_state();
    size_t get_record_count(const std::string& table) const;

private:
    TestDatabaseManager();
    ~TestDatabaseManager();

    // Prevent copying
    TestDatabaseManager(const TestDatabaseManager&) = delete;
    TestDatabaseManager& operator=(const TestDatabaseManager&) = delete;

    bool initialized_;
    std::unordered_map<std::string, std::vector<nlohmann::json>> test_tables_;
};

/**
 * @brief Test API server simulator
 *
 * Simulates external API responses for testing without network dependencies.
 */
class TestApiServer {
public:
    static TestApiServer& get_instance();

    void initialize();
    void cleanup();

    // Response configuration
    void set_response(const std::string& url, const std::string& method,
                     int status_code, const nlohmann::json& response,
                     const std::unordered_map<std::string, std::string>& headers = {});
    void set_error_response(const std::string& url, const std::string& method,
                           int status_code, const std::string& error_message);
    void clear_responses();

    // Response retrieval
    bool get_response(const std::string& url, const std::string& method,
                     int& status_code, nlohmann::json& response,
                     std::unordered_map<std::string, std::string>& headers) const;

    // Statistics
    size_t get_request_count(const std::string& url = "", const std::string& method = "") const;
    void reset_statistics();

private:
    TestApiServer();
    ~TestApiServer();

    // Prevent copying
    TestApiServer(const TestApiServer&) = delete;
    TestApiServer& operator=(const TestApiServer&) = delete;

    struct ApiResponse {
        int status_code;
        nlohmann::json response;
        std::unordered_map<std::string, std::string> headers;
        bool is_error;
    };

    bool initialized_;
    std::unordered_map<std::string, ApiResponse> responses_;
    mutable std::unordered_map<std::string, size_t> request_counts_;
};

/**
 * @brief Test time controller
 *
 * Allows tests to control time progression for testing time-dependent functionality.
 */
class TestTimeController {
public:
    static TestTimeController& get_instance();

    void initialize();
    void cleanup();

    // Time control
    void set_current_time(std::chrono::system_clock::time_point time);
    void advance_time(std::chrono::seconds duration);
    void freeze_time();
    void unfreeze_time();

    // Time queries
    std::chrono::system_clock::time_point get_current_time() const;
    std::chrono::system_clock::time_point get_real_time() const;

    // Time manipulation
    void reset_to_real_time();

private:
    TestTimeController();
    ~TestTimeController();

    // Prevent copying
    TestTimeController(const TestTimeController&) = delete;
    TestTimeController& operator=(const TestTimeController&) = delete;

    bool initialized_;
    bool time_frozen_;
    std::chrono::system_clock::time_point fake_current_time_;
};

} // namespace regulens
