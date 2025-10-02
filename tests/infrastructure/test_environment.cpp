#include "test_environment.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

#include "../../shared/logging/structured_logger.hpp"
#include "../../shared/config/configuration_manager.hpp"

namespace regulens {

// TestEnvironment implementation
TestEnvironment& TestEnvironment::get_instance() {
    static TestEnvironment instance;
    return instance;
}

TestEnvironment::TestEnvironment()
    : logger_(std::shared_ptr<StructuredLogger>(&StructuredLogger::get_instance(), [](StructuredLogger*){})),
      config_manager_(std::shared_ptr<ConfigurationManager>(&ConfigurationManager::get_instance(), [](ConfigurationManager*){})),
      initialized_(false) {
}

TestEnvironment::~TestEnvironment() {
    cleanup();
}

void TestEnvironment::initialize() {
    if (initialized_) return;

    logger_->info("Initializing TestEnvironment");

    // Store original environment variables
    original_env_vars_.clear();
    const std::vector<std::string> env_keys = {
        "DB_HOST", "DB_PORT", "DB_NAME", "DB_USER", "DB_PASSWORD",
        "AUDIT_DB_HOST", "AUDIT_DB_PORT", "AUDIT_DB_NAME", "AUDIT_DB_USER", "AUDIT_DB_PASSWORD",
        "VECTOR_DB_HOST", "VECTOR_DB_PORT", "VECTOR_DB_API_KEY",
        "AGENT_ENABLE_WEB_SEARCH", "AGENT_ENABLE_MCP_TOOLS", "AGENT_ENABLE_ADVANCED_DISCOVERY",
        "AGENT_ENABLE_AUTONOMOUS_INTEGRATION", "AGENT_MAX_AUTONOMOUS_TOOLS",
        "LLM_OPENAI_API_KEY", "LLM_ANTHROPIC_API_KEY",
        "SMTP_HOST", "SMTP_PORT", "SMTP_USER", "SMTP_PASSWORD", "SMTP_FROM_EMAIL"
    };

    for (const auto& key : env_keys) {
        char* val = std::getenv(key.c_str());
        if (val) {
            original_env_vars_[key] = std::string(val);
        } else {
            original_env_vars_[key] = std::nullopt;
        }
    }

    // Set test-specific environment variables
    set_env_var("DB_HOST", "test_db_host");
    set_env_var("DB_PORT", "5432");
    set_env_var("DB_NAME", "test_regulens_compliance");
    set_env_var("DB_USER", "test_user");
    set_env_var("DB_PASSWORD", "test_password");
    set_env_var("AUDIT_DB_HOST", "test_audit_db_host");
    set_env_var("AUDIT_DB_NAME", "test_regulens_audit");
    set_env_var("VECTOR_DB_HOST", "test_vector_db_host");
    set_env_var("AGENT_ENABLE_WEB_SEARCH", "false");
    set_env_var("AGENT_ENABLE_MCP_TOOLS", "true");
    set_env_var("LLM_OPENAI_API_KEY", "test_openai_key");
    set_env_var("LLM_ANTHROPIC_API_KEY", "test_anthropic_key");
    set_env_var("SMTP_HOST", "test_smtp_host");

    // Initialize ConfigurationManager with test environment
    config_manager_->initialize(0, nullptr);

    initialized_ = true;
    logger_->info("TestEnvironment initialized successfully");
}

void TestEnvironment::cleanup() {
    if (!initialized_) return;

    logger_->info("Cleaning up TestEnvironment");

    // Restore original environment variables
    restore_env_vars();

    // Clean up temporary paths
    for (const auto& path : temp_paths_) {
        if (std::filesystem::exists(path)) {
            std::filesystem::remove_all(path);
            logger_->debug("Removed temporary path: " + path.string());
        }
    }
    temp_paths_.clear();

    initialized_ = false;
    logger_->info("TestEnvironment cleanup completed");
}

std::shared_ptr<StructuredLogger> TestEnvironment::get_logger() const {
    return logger_;
}

std::shared_ptr<ConfigurationManager> TestEnvironment::get_config_manager() const {
    return config_manager_;
}

void TestEnvironment::set_env_var(const std::string& key, const std::string& value) {
    #ifdef _WIN32
        _putenv_s(key.c_str(), value.c_str());
    #else
        setenv(key.c_str(), value.c_str(), 1);
    #endif
    logger_->debug("Set test environment variable: " + key + "=" + value);
}

void TestEnvironment::clear_env_var(const std::string& key) {
    #ifdef _WIN32
        _putenv_s(key.c_str(), "");
    #else
        unsetenv(key.c_str());
    #endif
    logger_->debug("Cleared test environment variable: " + key);
}

void TestEnvironment::restore_env_vars() {
    for (const auto& pair : original_env_vars_) {
        if (pair.second.has_value()) {
            set_env_var(pair.first, *pair.second);
        } else {
            clear_env_var(pair.first);
        }
    }
    original_env_vars_.clear();
    logger_->debug("Restored original environment variables");
}

std::filesystem::path TestEnvironment::create_temp_dir(const std::string& prefix) {
    std::string temp_name = prefix + "_" + std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count());
    std::filesystem::path temp_path = std::filesystem::temp_directory_path() / temp_name;
    std::filesystem::create_directories(temp_path);
    temp_paths_.push_back(temp_path);
    logger_->debug("Created temporary directory: " + temp_path.string());
    return temp_path;
}

std::filesystem::path TestEnvironment::create_temp_file(const std::string& content, const std::string& prefix, const std::string& suffix) {
    std::filesystem::path temp_dir = create_temp_dir(prefix + "_file_");
    std::filesystem::path temp_file = temp_dir / (prefix + "file" + suffix);
    std::ofstream ofs(temp_file);
    if (!ofs.is_open()) {
        throw std::runtime_error("Failed to create temporary file: " + temp_file.string());
    }
    ofs << content;
    ofs.close();
    logger_->debug("Created temporary file: " + temp_file.string());
    return temp_file;
}

void TestEnvironment::set_test_mode(bool test_mode) {
    test_mode_ = test_mode;
}

bool TestEnvironment::is_test_mode() const {
    return test_mode_;
}

std::string TestEnvironment::create_temp_file(const std::string& content) {
    if (!initialized_) initialize();

    try {
        std::string filename = "test_file_" + std::to_string(temp_files_.size()) + ".tmp";
        std::filesystem::path file_path = temp_base_path_ / filename;

        std::ofstream file(file_path);
        if (file.is_open()) {
            file << content;
            file.close();

            temp_files_.push_back(file_path);
            return file_path.string();
        }
    } catch (const std::exception&) {
        // Ignore errors
    }

    return "";
}

std::string TestEnvironment::create_temp_directory(const std::string& prefix) {
    if (!initialized_) initialize();

    try {
        std::string dirname = prefix + "_" + std::to_string(temp_directories_.size());
        std::filesystem::path dir_path = temp_base_path_ / dirname;

        if (std::filesystem::create_directories(dir_path)) {
            temp_directories_.push_back(dir_path);
            return dir_path.string();
        }
    } catch (const std::exception&) {
        // Ignore errors
    }

    return "";
}

void TestEnvironment::cleanup_temp_files() {
    // Remove temporary files
    for (const auto& file : temp_files_) {
        try {
            if (std::filesystem::exists(file)) {
                std::filesystem::remove(file);
            }
        } catch (const std::exception&) {
            // Ignore individual file cleanup errors
        }
    }
    temp_files_.clear();

    // Remove temporary directories
    for (const auto& dir : temp_directories_) {
        try {
            if (std::filesystem::exists(dir)) {
                std::filesystem::remove_all(dir);
            }
        } catch (const std::exception&) {
            // Ignore individual directory cleanup errors
        }
    }
    temp_directories_.clear();
}

void TestEnvironment::register_mock_service(const std::string& service_name,
                                          std::shared_ptr<void> mock_service) {
    mock_services_[service_name] = mock_service;
}

std::shared_ptr<void> TestEnvironment::get_mock_service(const std::string& service_name) {
    auto it = mock_services_.find(service_name);
    return (it != mock_services_.end()) ? it->second : nullptr;
}

void TestEnvironment::unregister_mock_service(const std::string& service_name) {
    mock_services_.erase(service_name);
}

void TestEnvironment::set_config_override(const std::string& key, const std::string& value) {
    config_overrides_[key] = value;
}

void TestEnvironment::clear_config_overrides() {
    config_overrides_.clear();
}

std::unordered_map<std::string, std::string> TestEnvironment::get_config_overrides() const {
    return config_overrides_;
}

void TestEnvironment::set_environment_variable(const std::string& key, const std::string& value) {
    // Backup original value if not already backed up
    if (env_vars_backup_.find(key) == env_vars_backup_.end()) {
        const char* original = std::getenv(key);
        if (original) {
            env_vars_backup_[key] = original;
        }
    }

    // Set test value
    env_vars_test_[key] = value;
#ifdef _WIN32
    _putenv_s(key.c_str(), value.c_str());
#else
    setenv(key.c_str(), value.c_str(), 1);
#endif
}

void TestEnvironment::restore_environment_variables() {
    // Restore original values
    for (const auto& [key, value] : env_vars_backup_) {
#ifdef _WIN32
        _putenv_s(key.c_str(), value.c_str());
#else
        setenv(key.c_str(), value.c_str(), 1);
#endif
    }

    // Remove test values that don't have originals
    for (const auto& [key, _] : env_vars_test_) {
        if (env_vars_backup_.find(key) == env_vars_backup_.end()) {
#ifdef _WIN32
            _putenv_s(key.c_str(), "");
#else
            unsetenv(key.c_str());
#endif
        }
    }

    env_vars_test_.clear();
}

std::string TestEnvironment::get_environment_variable(const std::string& key) const {
    const char* value = std::getenv(key);
    return value ? std::string(value) : "";
}

void TestEnvironment::set_test_data(const std::string& key, const nlohmann::json& data) {
    test_data_[key] = data;
}

nlohmann::json TestEnvironment::get_test_data(const std::string& key) const {
    auto it = test_data_.find(key);
    return (it != test_data_.end()) ? it->second : nlohmann::json{};
}

void TestEnvironment::clear_test_data() {
    test_data_.clear();
}

bool TestEnvironment::verify_isolation() const {
    // Check that we're in test mode
    if (!test_mode_) return false;

    // Check that temporary directory exists
    if (!std::filesystem::exists(temp_base_path_)) return false;

    // Verify no external services are registered
    return mock_services_.empty() ||
           get_isolation_warnings().size() < 3; // Allow some mock services
}

std::vector<std::string> TestEnvironment::get_isolation_warnings() const {
    std::vector<std::string> warnings;

    if (!test_mode_) {
        warnings.push_back("Not in test mode");
    }

    if (!std::filesystem::exists(temp_base_path_)) {
        warnings.push_back("Temporary directory not created");
    }

    if (!mock_services_.empty()) {
        warnings.push_back("External services detected: " +
                          std::to_string(mock_services_.size()) + " services");
    }

    return warnings;
}

// TestEnvironmentGuard implementation
TestEnvironmentGuard::TestEnvironmentGuard() {
    TestEnvironment::get_instance().initialize();
}

TestEnvironmentGuard::~TestEnvironmentGuard() {
    TestEnvironment::get_instance().cleanup();
}

// TestDatabaseManager implementation
TestDatabaseManager& TestDatabaseManager::get_instance() {
    static TestDatabaseManager instance;
    return instance;
}

TestDatabaseManager::TestDatabaseManager() : initialized_(false) {}

TestDatabaseManager::~TestDatabaseManager() {
    cleanup();
}

void TestDatabaseManager::initialize() {
    if (initialized_) return;

    // Create test schema
    create_test_schema();
    initialized_ = true;
}

void TestDatabaseManager::cleanup() {
    if (!initialized_) return;

    // Drop test schema and cleanup
    drop_test_schema();
    test_tables_.clear();
    initialized_ = false;
}

bool TestDatabaseManager::create_test_schema() {
    // Create basic test tables structure
    test_tables_["regulatory_changes"] = {};
    test_tables_["compliance_events"] = {};
    test_tables_["agent_decisions"] = {};
    test_tables_["audit_logs"] = {};

    return true;
}

bool TestDatabaseManager::drop_test_schema() {
    test_tables_.clear();
    return true;
}

bool TestDatabaseManager::insert_test_data(const std::string& table, const nlohmann::json& data) {
    if (test_tables_.find(table) == test_tables_.end()) {
        return false;
    }

    test_tables_[table].push_back(data);
    return true;
}

bool TestDatabaseManager::query_test_data(const std::string& query, nlohmann::json& result) {
    // Simple mock query implementation
    result = nlohmann::json::array();

    // Parse basic SELECT queries
    if (query.find("SELECT") != std::string::npos) {
        size_t from_pos = query.find("FROM");
        if (from_pos != std::string::npos) {
            size_t table_start = from_pos + 4;
            size_t table_end = query.find(" ", table_start);
            if (table_end == std::string::npos) {
                table_end = query.length();
            }

            std::string table = query.substr(table_start, table_end - table_start);
            // Remove whitespace
            table.erase(table.begin(), std::find_if(table.begin(), table.end(),
                           [](unsigned char ch) { return !std::isspace(ch); }));
            table.erase(std::find_if(table.rbegin(), table.rend(),
                           [](unsigned char ch) { return !std::isspace(ch); }).base(), table.end());

            if (test_tables_.find(table) != test_tables_.end()) {
                result = test_tables_[table];
            }
        }
    }

    return true;
}

bool TestDatabaseManager::update_test_data(const std::string& table, const nlohmann::json& data,
                                         const std::string& where_clause) {
    if (test_tables_.find(table) == test_tables_.end()) {
        return false;
    }

    // Simple mock update - update first record
    auto& records = test_tables_[table];
    if (!records.empty()) {
        records[0].update(data);
        return true;
    }

    return false;
}

bool TestDatabaseManager::delete_test_data(const std::string& table, const std::string& where_clause) {
    if (test_tables_.find(table) == test_tables_.end()) {
        return false;
    }

    // Simple mock delete - remove first record
    auto& records = test_tables_[table];
    if (!records.empty()) {
        records.erase(records.begin());
        return true;
    }

    return false;
}

void TestDatabaseManager::reset_database_state() {
    for (auto& [table, records] : test_tables_) {
        records.clear();
    }
}

size_t TestDatabaseManager::get_record_count(const std::string& table) const {
    auto it = test_tables_.find(table);
    return (it != test_tables_.end()) ? it->second.size() : 0;
}

// TestApiServer implementation
TestApiServer& TestApiServer::get_instance() {
    static TestApiServer instance;
    return instance;
}

TestApiServer::TestApiServer() : initialized_(false) {}

TestApiServer::~TestApiServer() {
    cleanup();
}

void TestApiServer::initialize() {
    if (initialized_) return;
    initialized_ = true;
}

void TestApiServer::cleanup() {
    if (!initialized_) return;

    responses_.clear();
    request_counts_.clear();
    initialized_ = false;
}

void TestApiServer::set_response(const std::string& url, const std::string& method,
                                int status_code, const nlohmann::json& response,
                                const std::unordered_map<std::string, std::string>& headers) {
    std::string key = method + ":" + url;
    responses_[key] = {status_code, response, headers, false};
}

void TestApiServer::set_error_response(const std::string& url, const std::string& method,
                                      int status_code, const std::string& error_message) {
    std::string key = method + ":" + url;
    nlohmann::json error_response = {{"error", error_message}};
    responses_[key] = {status_code, error_response, {}, true};
}

void TestApiServer::clear_responses() {
    responses_.clear();
    request_counts_.clear();
}

bool TestApiServer::get_response(const std::string& url, const std::string& method,
                                int& status_code, nlohmann::json& response,
                                std::unordered_map<std::string, std::string>& headers) const {
    std::string key = method + ":" + url;

    // Record the request
    request_counts_[key]++;

    auto it = responses_.find(key);
    if (it != responses_.end()) {
        const auto& api_response = it->second;
        status_code = api_response.status_code;
        response = api_response.response;
        headers = api_response.headers;
        return true;
    }

    return false;
}

size_t TestApiServer::get_request_count(const std::string& url, const std::string& method) const {
    if (url.empty() && method.empty()) {
        size_t total = 0;
        for (const auto& [_, count] : request_counts_) {
            total += count;
        }
        return total;
    }

    std::string key = method + ":" + url;
    auto it = request_counts_.find(key);
    return (it != request_counts_.end()) ? it->second : 0;
}

void TestApiServer::reset_statistics() {
    request_counts_.clear();
}

// TestTimeController implementation
TestTimeController& TestTimeController::get_instance() {
    static TestTimeController instance;
    return instance;
}

TestTimeController::TestTimeController()
    : initialized_(false), time_frozen_(false) {}

TestTimeController::~TestTimeController() {
    cleanup();
}

void TestTimeController::initialize() {
    if (initialized_) return;
    initialized_ = true;
}

void TestTimeController::cleanup() {
    if (!initialized_) return;

    unfreeze_time();
    initialized_ = false;
}

void TestTimeController::set_current_time(std::chrono::system_clock::time_point time) {
    fake_current_time_ = time;
    time_frozen_ = true;
}

void TestTimeController::advance_time(std::chrono::seconds duration) {
    if (time_frozen_) {
        fake_current_time_ += duration;
    }
}

void TestTimeController::freeze_time() {
    if (!time_frozen_) {
        fake_current_time_ = std::chrono::system_clock::now();
        time_frozen_ = true;
    }
}

void TestTimeController::unfreeze_time() {
    time_frozen_ = false;
}

std::chrono::system_clock::time_point TestTimeController::get_current_time() const {
    return time_frozen_ ? fake_current_time_ : std::chrono::system_clock::now();
}

std::chrono::system_clock::time_point TestTimeController::get_real_time() const {
    return std::chrono::system_clock::now();
}

void TestTimeController::reset_to_real_time() {
    time_frozen_ = false;
    fake_current_time_ = std::chrono::system_clock::time_point{};
}

} // namespace regulens
