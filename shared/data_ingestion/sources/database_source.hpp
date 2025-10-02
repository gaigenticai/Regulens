/**
 * Database Data Source - Multi-Database Integration
 *
 * Production-grade database connector supporting multiple database types:
 * - PostgreSQL, MySQL, SQL Server, Oracle
 * - Connection pooling and reuse
 * - Query optimization and batching
 * - Change Data Capture (CDC) support
 * - Schema introspection and dynamic querying
 *
 * Retrospective Enhancement: Standardizes database access across all POCs
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include "../data_ingestion_framework.hpp"
#include "../../database/postgresql_connection.hpp"
#include "../../logging/structured_logger.hpp"
#include <nlohmann/json.hpp>

namespace regulens {

enum class DatabaseType {
    POSTGRESQL,
    MYSQL,
    SQL_SERVER,
    ORACLE,
    SQLITE,
    MONGODB,
    REDIS
};

enum class QueryType {
    SELECT_SINGLE,
    SELECT_BATCH,
    STORED_PROCEDURE,
    CHANGE_DATA_CAPTURE,
    INCREMENTAL_LOAD
};

enum class IncrementalStrategy {
    TIMESTAMP_COLUMN,
    SEQUENCE_ID,
    CHANGE_TRACKING,
    LOG_BASED
};

struct DatabaseConnectionConfig {
    DatabaseType db_type = DatabaseType::POSTGRESQL;
    std::string host;
    int port = 5432;
    std::string database;
    std::string username;
    std::string password;
    std::string connection_string; // Alternative to individual params
    int max_connections = 10;
    std::chrono::seconds connection_timeout = std::chrono::seconds(30);
    bool ssl_enabled = false;
    std::unordered_map<std::string, std::string> ssl_params;
    std::unordered_map<std::string, std::string> additional_params;
};

struct DatabaseQuery {
    std::string query_id;
    std::string sql_query;
    QueryType query_type = QueryType::SELECT_BATCH;
    std::unordered_map<std::string, nlohmann::json> parameters;
    int batch_size = 1000;
    std::chrono::seconds execution_timeout = std::chrono::seconds(300);
    bool enable_caching = false;
    std::chrono::seconds cache_ttl = std::chrono::seconds(300);
};

struct IncrementalLoadConfig {
    IncrementalStrategy strategy = IncrementalStrategy::TIMESTAMP_COLUMN;
    std::string incremental_column; // Column for tracking changes
    std::string last_value; // Last processed value
    int batch_size = 1000;
    bool include_deletes = false;
};

struct DatabaseSourceConfig {
    DatabaseConnectionConfig connection;
    std::vector<DatabaseQuery> queries;
    IncrementalLoadConfig incremental_config;
    bool enable_change_tracking = false;
    std::chrono::seconds polling_interval = std::chrono::seconds(300);
    int max_parallel_queries = 3;
    bool validate_schema = true;
    std::unordered_map<std::string, std::string> table_mappings;
};

class DatabaseSource : public DataSource {
public:
    DatabaseSource(const DataIngestionConfig& config,
                   std::shared_ptr<ConnectionPool> db_pool,
                   StructuredLogger* logger);

    ~DatabaseSource() override;

    // DataSource interface implementation
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    std::vector<nlohmann::json> fetch_data() override;
    bool validate_connection() override;

    // Database-specific methods
    void set_database_config(const DatabaseSourceConfig& db_config);
    std::vector<nlohmann::json> execute_query(const DatabaseQuery& query);
    std::vector<nlohmann::json> execute_incremental_load();
    nlohmann::json get_table_schema(const std::string& table_name);

    // Change Data Capture
    bool enable_cdc(const std::string& table_name);
    std::vector<nlohmann::json> get_cdc_changes(const std::string& table_name);
    bool commit_cdc_changes(const std::string& table_name, const std::string& lsn);

private:
    // Database connection management
    bool establish_connection();
    bool test_database_connection();
    void configure_connection_pool();
    std::shared_ptr<PostgreSQLConnection> get_connection();

    // Query execution
    std::vector<nlohmann::json> execute_select_query(const DatabaseQuery& query);
    std::vector<nlohmann::json> execute_stored_procedure(const DatabaseQuery& query);
    nlohmann::json execute_single_row_query(const DatabaseQuery& query);

    // Incremental loading
    std::vector<nlohmann::json> load_by_timestamp(const std::string& table_name, const std::string& timestamp_column);
    std::vector<nlohmann::json> load_by_sequence(const std::string& table_name, const std::string& sequence_column);
    std::vector<nlohmann::json> load_by_change_tracking(const std::string& table_name);

    // Schema introspection
    nlohmann::json introspect_table_schema(const std::string& table_name);
    nlohmann::json introspect_database_schema();
    std::vector<std::string> get_table_list();
    bool validate_table_exists(const std::string& table_name);

    // Change Data Capture implementation
    bool setup_cdc_for_postgresql(const std::string& table_name);
    bool setup_cdc_for_sql_server(const std::string& table_name);
    std::vector<nlohmann::json> poll_cdc_changes_postgresql(const std::string& table_name);
    std::vector<nlohmann::json> poll_cdc_changes_sql_server(const std::string& table_name);

    // Data transformation and mapping
    nlohmann::json transform_database_row(const nlohmann::json& row_data, const std::string& table_name);
    std::string map_column_name(const std::string& original_name, const std::string& table_name);
    nlohmann::json convert_database_type(const std::string& value, const std::string& db_type);

    // Performance optimization
    bool prepare_statement(const DatabaseQuery& query);
    void enable_query_caching(const DatabaseQuery& query);
    nlohmann::json get_cached_query_result(const std::string& query_hash);
    void set_cached_query_result(const std::string& query_hash, const nlohmann::json& result);

    // Error handling and recovery
    bool handle_connection_error(const std::string& error);
    bool handle_query_timeout(const DatabaseQuery& query);
    bool retry_failed_query(const DatabaseQuery& query, int attempt);

    // Monitoring and metrics
    void record_query_metrics(const DatabaseQuery& query, const std::chrono::microseconds& duration, int rows_affected);
    nlohmann::json get_query_performance_stats();
    std::vector<std::string> identify_slow_queries();

    // Internal state
    DatabaseSourceConfig db_config_;
    bool connected_;
    std::shared_ptr<ConnectionPool> external_db_pool_; // For external databases
    std::unordered_map<std::string, std::string> last_incremental_values_;
    std::unordered_map<std::string, nlohmann::json> query_cache_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> cache_timestamps_;

    // Prepared statements cache
    std::unordered_map<std::string, std::string> prepared_statements_;

    // CDC state tracking
    std::unordered_map<std::string, std::string> cdc_positions_; // Table -> Last position

    // Performance metrics
    int total_queries_executed_;
    int successful_queries_;
    int failed_queries_;
    std::chrono::microseconds total_query_time_;
    std::unordered_map<std::string, int> query_error_counts_;

    // Constants
    const int DEFAULT_CONNECTION_POOL_SIZE = 5;
    const std::chrono::seconds DEFAULT_QUERY_TIMEOUT = std::chrono::seconds(300);
    const int MAX_RETRY_ATTEMPTS = 3;
    const std::chrono::seconds RETRY_BASE_DELAY = std::chrono::seconds(1);
};

} // namespace regulens
