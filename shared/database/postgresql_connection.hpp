/**
 * PostgreSQL Database Connection - Production Grade
 *
 * Enterprise-grade PostgreSQL connectivity with connection pooling,
 * prepared statements, and comprehensive error handling.
 */

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <condition_variable>
#include <nlohmann/json.hpp>

// Include configuration types
#include "../config/config_types.hpp"

// Forward declaration for PostgreSQL types
struct pg_conn;
typedef struct pg_conn PGconn;
typedef struct pg_result PGresult;

namespace regulens {

class PostgreSQLConnection {
public:
    PostgreSQLConnection(const DatabaseConfig& config);
    ~PostgreSQLConnection();

    // Connection management
    bool connect();
    void disconnect();
    bool is_connected() const;
    bool reconnect();

    // Query execution
    std::optional<nlohmann::json> execute_query_single(const std::string& query,
                                                       const std::vector<std::string>& params = {});
    std::vector<nlohmann::json> execute_query_multi(const std::string& query,
                                                    const std::vector<std::string>& params = {});
    bool execute_command(const std::string& command,
                        const std::vector<std::string>& params = {});

    // Transaction management
    bool begin_transaction();
    bool commit_transaction();
    bool rollback_transaction();

    // Prepared statements
    bool prepare_statement(const std::string& name, const std::string& query,
                          int param_count);
    bool execute_prepared(const std::string& name,
                         const std::vector<std::string>& params);

    // Health checks
    bool ping();
    nlohmann::json get_connection_stats() const;

private:
    DatabaseConfig config_;
    PGconn* connection_;
    std::atomic<bool> connected_;
    mutable std::mutex connection_mutex_;

    std::string build_connection_string() const;
    nlohmann::json result_to_json(PGresult* result, int row = 0) const;
    void log_error(const std::string& operation, PGresult* result = nullptr) const;
};

class ConnectionPool {
public:
    ConnectionPool(const DatabaseConfig& config);
    ~ConnectionPool();

    std::shared_ptr<PostgreSQLConnection> get_connection();
    void return_connection(std::shared_ptr<PostgreSQLConnection> conn);
    void shutdown();

    nlohmann::json get_pool_stats() const;

private:
    DatabaseConfig config_;
    std::vector<std::shared_ptr<PostgreSQLConnection>> connections_;
    std::vector<std::shared_ptr<PostgreSQLConnection>> available_connections_;
    mutable std::mutex pool_mutex_;
    std::condition_variable pool_cv_;
    std::atomic<bool> shutdown_;
    std::atomic<size_t> active_connections_;
    std::atomic<size_t> total_connections_created_;

    std::shared_ptr<PostgreSQLConnection> create_connection();
};

} // namespace regulens

