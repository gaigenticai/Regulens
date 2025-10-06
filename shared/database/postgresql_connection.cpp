/**
 * PostgreSQL Database Connection - Production Implementation
 *
 * Enterprise-grade PostgreSQL connectivity with connection pooling,
 * prepared statements, and comprehensive error handling.
 */

#include "postgresql_connection.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <stdexcept>
#include <condition_variable>
#include <libpq-fe.h>

namespace regulens {

PostgreSQLConnection::PostgreSQLConnection(const DatabaseConfig& config)
    : config_(config), connection_(nullptr), connected_(false) {
}

PostgreSQLConnection::~PostgreSQLConnection() {
    disconnect();
}

bool PostgreSQLConnection::connect() {
    std::lock_guard<std::mutex> lock(connection_mutex_);

    if (connected_) {
        return true;
    }

    std::string conn_string = build_connection_string();
    connection_ = PQconnectdb(conn_string.c_str());

    if (PQstatus(connection_) != CONNECTION_OK) {
        log_error("connect");
        return false;
    }

    connected_ = true;
    return true;
}

void PostgreSQLConnection::disconnect() {
    std::lock_guard<std::mutex> lock(connection_mutex_);

    if (connection_) {
        PQfinish(connection_);
        connection_ = nullptr;
    }
    connected_ = false;
}

bool PostgreSQLConnection::is_connected() const {
    return connected_;
}

bool PostgreSQLConnection::reconnect() {
    disconnect();
    return connect();
}

std::optional<nlohmann::json> PostgreSQLConnection::execute_query_single(
    const std::string& query, const std::vector<std::string>& params) {

    std::lock_guard<std::mutex> lock(connection_mutex_);

    if (!connected_) {
        return std::nullopt;
    }

    // Convert parameters to C-style strings
    std::vector<const char*> param_values;
    for (const auto& param : params) {
        param_values.push_back(param.c_str());
    }

    PGresult* result = PQexecParams(connection_, query.c_str(),
                                   static_cast<int>(params.size()),
                                   nullptr, param_values.data(), nullptr, nullptr, 0);

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        log_error("execute_query_single", result);
        PQclear(result);
        return std::nullopt;
    }

    if (PQntuples(result) == 0) {
        PQclear(result);
        return std::nullopt;
    }

    auto json_result = result_to_json(result, 0);
    PQclear(result);
    return std::optional<nlohmann::json>(json_result);
}

std::vector<nlohmann::json> PostgreSQLConnection::execute_query_multi(
    const std::string& query, const std::vector<std::string>& params) {

    std::vector<nlohmann::json> results;
    std::lock_guard<std::mutex> lock(connection_mutex_);

    if (!connected_) {
        return results;
    }

    // Convert parameters to C-style strings
    std::vector<const char*> param_values;
    for (const auto& param : params) {
        param_values.push_back(param.c_str());
    }

    PGresult* result = PQexecParams(connection_, query.c_str(),
                                   static_cast<int>(params.size()),
                                   nullptr, param_values.data(), nullptr, nullptr, 0);

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        log_error("execute_query_multi", result);
        PQclear(result);
        return results;
    }

    int num_rows = PQntuples(result);
    for (int i = 0; i < num_rows; ++i) {
        results.push_back(result_to_json(result, i));
    }

    PQclear(result);
    return results;
}

PostgreSQLConnection::QueryResult PostgreSQLConnection::execute_query(
    const std::string& query, const std::vector<std::string>& params) {

    QueryResult result;
    std::lock_guard<std::mutex> lock(connection_mutex_);

    if (!connected_) {
        return result;
    }

    // Convert parameters to C-style strings
    std::vector<const char*> param_values;
    for (const auto& param : params) {
        param_values.push_back(param.c_str());
    }

    PGresult* pg_result = PQexecParams(connection_, query.c_str(),
                                      static_cast<int>(params.size()),
                                      nullptr, param_values.data(), nullptr, nullptr, 0);

    if (PQresultStatus(pg_result) != PGRES_TUPLES_OK) {
        log_error("execute_query", pg_result);
        PQclear(pg_result);
        return result;
    }

    int num_rows = PQntuples(pg_result);
    int num_fields = PQnfields(pg_result);

    for (int i = 0; i < num_rows; ++i) {
        std::unordered_map<std::string, std::string> row;
        for (int j = 0; j < num_fields; ++j) {
            const char* field_name = PQfname(pg_result, j);
            if (PQgetisnull(pg_result, i, j)) {
                row[field_name] = "";
            } else {
                const char* value = PQgetvalue(pg_result, i, j);
                row[field_name] = std::string(value);
            }
        }
        result.rows.push_back(row);
    }

    PQclear(pg_result);
    return result;
}

bool PostgreSQLConnection::execute_command(
    const std::string& command, const std::vector<std::string>& params) {

    std::lock_guard<std::mutex> lock(connection_mutex_);

    if (!connected_) {
        return false;
    }

    // Convert parameters to C-style strings
    std::vector<const char*> param_values;
    for (const auto& param : params) {
        param_values.push_back(param.c_str());
    }

    PGresult* result = PQexecParams(connection_, command.c_str(),
                                   static_cast<int>(params.size()),
                                   nullptr, param_values.data(), nullptr, nullptr, 0);

    bool success = (PQresultStatus(result) == PGRES_COMMAND_OK);
    if (!success) {
        log_error("execute_command", result);
    }

    PQclear(result);
    return success;
}

bool PostgreSQLConnection::begin_transaction() {
    return execute_command("BEGIN");
}

bool PostgreSQLConnection::commit_transaction() {
    return execute_command("COMMIT");
}

bool PostgreSQLConnection::rollback_transaction() {
    return execute_command("ROLLBACK");
}

bool PostgreSQLConnection::prepare_statement(const std::string& name,
                                           const std::string& query,
                                           int param_count) {
    std::lock_guard<std::mutex> lock(connection_mutex_);

    if (!connected_) {
        return false;
    }

    PGresult* result = PQprepare(connection_, name.c_str(), query.c_str(),
                                param_count, nullptr);

    bool success = (PQresultStatus(result) == PGRES_COMMAND_OK);
    if (!success) {
        log_error("prepare_statement", result);
    }

    PQclear(result);
    return success;
}

bool PostgreSQLConnection::execute_prepared(const std::string& name,
                                          const std::vector<std::string>& params) {
    std::lock_guard<std::mutex> lock(connection_mutex_);

    if (!connected_) {
        return false;
    }

    // Convert parameters to C-style strings
    std::vector<const char*> param_values;
    for (const auto& param : params) {
        param_values.push_back(param.c_str());
    }

    PGresult* result = PQexecPrepared(connection_, name.c_str(),
                                     static_cast<int>(params.size()),
                                     param_values.data(), nullptr, nullptr, 0);

    bool success = (PQresultStatus(result) == PGRES_COMMAND_OK);
    if (!success) {
        log_error("execute_prepared", result);
    }

    PQclear(result);
    return success;
}

bool PostgreSQLConnection::ping() {
    std::lock_guard<std::mutex> lock(connection_mutex_);

    if (!connected_) {
        return false;
    }

    PGresult* result = PQexec(connection_, "SELECT 1");
    bool success = (PQresultStatus(result) == PGRES_TUPLES_OK);
    PQclear(result);
    return success;
}

nlohmann::json PostgreSQLConnection::get_connection_stats() const {
    return {
        {"connected", connected_.load()},
        {"host", config_.host},
        {"port", config_.port},
        {"database", config_.database},
        {"user", config_.user}
    };
}

std::string PostgreSQLConnection::build_connection_string() const {
    std::stringstream ss;
    ss << "host=" << config_.host
       << " port=" << config_.port
       << " dbname=" << config_.database
       << " user=" << config_.user
       << " password=" << config_.password
       << " connect_timeout=" << config_.connection_timeout;

    if (config_.ssl_mode) {
        ss << " sslmode=require";
    } else {
        ss << " sslmode=disable";
    }

    return ss.str();
}

nlohmann::json PostgreSQLConnection::result_to_json(PGresult* result, int row) const {
    nlohmann::json json_row;

    int num_fields = PQnfields(result);
    for (int col = 0; col < num_fields; ++col) {
        const char* field_name = PQfname(result, col);
        if (PQgetisnull(result, row, col)) {
            json_row[field_name] = nullptr;
        } else {
            const char* value = PQgetvalue(result, row, col);
            json_row[field_name] = std::string(value);
        }
    }

    return json_row;
}

void PostgreSQLConnection::log_error(const std::string& operation,
                                    PGresult* result) const {
    std::cerr << "[PostgreSQL Error] " << operation << ": ";

    if (result) {
        std::cerr << PQresultErrorMessage(result);
    } else {
        std::cerr << PQerrorMessage(connection_);
    }

    std::cerr << std::endl;
}

// Connection Pool Implementation
ConnectionPool::ConnectionPool(const DatabaseConfig& config)
    : config_(config), shutdown_(false), active_connections_(0),
      total_connections_created_(0) {
    // Create minimum connections
    for (int i = 0; i < config_.min_connections; ++i) {
        auto conn = create_connection();
        if (conn) {
            connections_.push_back(conn);
            available_connections_.push_back(conn);
        }
    }
}

ConnectionPool::~ConnectionPool() {
    shutdown();
}

std::shared_ptr<PostgreSQLConnection> ConnectionPool::get_connection() {
    std::unique_lock<std::mutex> lock(pool_mutex_);

    if (shutdown_) {
        return nullptr;
    }

    // Try to get an available connection
    if (!available_connections_.empty()) {
        auto conn = available_connections_.back();
        available_connections_.pop_back();
        active_connections_++;
        return conn;
    }

    // Create a new connection if under max limit
    if (static_cast<int>(connections_.size()) < config_.max_connections) {
        auto conn = create_connection();
        if (conn) {
            connections_.push_back(conn);
            active_connections_++;
            total_connections_created_++;
            return conn;
        }
    }

    // Wait for a connection to become available with timeout
    auto timeout = std::chrono::seconds(30); // 30 second timeout
    if (pool_cv_.wait_for(lock, timeout, [this]() {
        return shutdown_ || !available_connections_.empty() ||
               static_cast<int>(connections_.size()) < config_.max_connections;
    })) {
        // Try again after waking up
        if (!available_connections_.empty()) {
            auto conn = available_connections_.back();
            available_connections_.pop_back();
            active_connections_++;
            return conn;
        }

        // Create a new connection if possible
        if (static_cast<int>(connections_.size()) < config_.max_connections) {
            auto conn = create_connection();
            if (conn) {
                connections_.push_back(conn);
                active_connections_++;
                total_connections_created_++;
                return conn;
            }
        }
    }

    // Timeout or shutdown occurred
    return nullptr;
}

void ConnectionPool::return_connection(std::shared_ptr<PostgreSQLConnection> conn) {
    std::lock_guard<std::mutex> lock(pool_mutex_);

    if (!shutdown_ && conn && conn->is_connected()) {
        available_connections_.push_back(conn);
        active_connections_--;
        // Notify waiting threads that a connection is available
        pool_cv_.notify_one();
    }
}

void ConnectionPool::shutdown() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    shutdown_ = true;
    connections_.clear();
    available_connections_.clear();
}

nlohmann::json ConnectionPool::get_pool_stats() const {
    return {
        {"total_connections", connections_.size()},
        {"available_connections", available_connections_.size()},
        {"active_connections", active_connections_.load()},
        {"total_created", total_connections_created_.load()},
        {"shutdown", shutdown_.load()}
    };
}

std::shared_ptr<PostgreSQLConnection> ConnectionPool::create_connection() {
    auto conn = std::make_shared<PostgreSQLConnection>(config_);
    if (conn->connect()) {
        return conn;
    }
    return nullptr;
}

} // namespace regulens
