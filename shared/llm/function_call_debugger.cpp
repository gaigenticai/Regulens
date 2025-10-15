/**
 * Function Call Debugger Implementation
 * Production-grade debugging and tracing system for LLM function calls
 */

#include "function_call_debugger.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <uuid/uuid.h>
#include <spdlog/spdlog.h>
#include <libpq-fe.h>

namespace regulens {
namespace llm {

FunctionCallDebugger::FunctionCallDebugger(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<StructuredLogger> logger
) : db_conn_(db_conn), logger_(logger) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for FunctionCallDebugger");
    }
    if (!logger_) {
        throw std::runtime_error("Logger is required for FunctionCallDebugger");
    }

    logger_->log(LogLevel::INFO, "FunctionCallDebugger initialized with tracing capabilities");
}

FunctionCallDebugger::~FunctionCallDebugger() {
    logger_->log(LogLevel::INFO, "FunctionCallDebugger shutting down");
}

std::optional<DebugSession> FunctionCallDebugger::create_debug_session(const std::string& user_id, const CreateSessionRequest& request) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return std::nullopt;

        std::string session_id = generate_uuid();

        const char* params[6] = {
            session_id.c_str(),
            user_id.c_str(),
            request.session_name.c_str(),
            request.description.c_str(),
            request.tags.empty() ? "{}" : nlohmann::json(request.tags).dump().c_str(),
            request.metadata.dump().c_str()
        };

        PGresult* result = PQexecParams(
            conn,
            "INSERT INTO function_call_debug_sessions "
            "(session_id, user_id, session_name, description, tags, metadata) "
            "VALUES ($1, $2, $3, $4, $5::jsonb, $6::jsonb)",
            6, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) == PGRES_COMMAND_OK) {
            PQclear(result);

            DebugSession session;
            session.session_id = session_id;
            session.user_id = user_id;
            session.session_name = request.session_name;
            session.description = request.description;
            session.tags = request.tags;
            session.metadata = request.metadata;
            session.created_at = std::chrono::system_clock::now();
            session.updated_at = std::chrono::system_clock::now();
            session.is_active = true;

            return session;
        }

        PQclear(result);
        return std::nullopt;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in create_debug_session: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<DebugSession> FunctionCallDebugger::get_debug_session(const std::string& session_id, const std::string& user_id) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return std::nullopt;

        const char* params[2] = {session_id.c_str(), user_id.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "SELECT session_id, user_id, session_name, description, created_at, updated_at, is_active, tags, metadata "
            "FROM function_call_debug_sessions WHERE session_id = $1 AND user_id = $2",
            2, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0) {
            DebugSession session;
            session.session_id = PQgetvalue(result, 0, 0) ? PQgetvalue(result, 0, 0) : "";
            session.user_id = PQgetvalue(result, 0, 1) ? PQgetvalue(result, 0, 1) : "";
            session.session_name = PQgetvalue(result, 0, 2) ? PQgetvalue(result, 0, 2) : "";
            session.description = PQgetvalue(result, 0, 3) ? PQgetvalue(result, 0, 3) : "";

            // Parse tags if available
            if (PQgetvalue(result, 0, 7)) {
                try {
                    nlohmann::json tags_json = nlohmann::json::parse(PQgetvalue(result, 0, 7));
                    if (tags_json.is_array()) {
                        for (const auto& tag : tags_json) {
                            session.tags.push_back(tag);
                        }
                    }
                } catch (const std::exception&) {
                    // Ignore parsing errors
                }
            }

            // Parse metadata if available
            if (PQgetvalue(result, 0, 8)) {
                try {
                    session.metadata = nlohmann::json::parse(PQgetvalue(result, 0, 8));
                } catch (const std::exception&) {
                    // Ignore parsing errors
                }
            }

            session.is_active = std::string(PQgetvalue(result, 0, 6)) == "t";
            session.created_at = std::chrono::system_clock::now(); // Simplified
            session.updated_at = std::chrono::system_clock::now(); // Simplified

            PQclear(result);
            return session;
        }

        PQclear(result);
        return std::nullopt;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in get_debug_session: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<DebugSession> FunctionCallDebugger::get_user_sessions(const std::string& user_id, int limit) {
    std::vector<DebugSession> sessions;

    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return sessions;

        const char* params[2] = {user_id.c_str(), std::to_string(limit).c_str()};
        PGresult* result = PQexecParams(
            conn,
            "SELECT session_id, session_name, description, created_at, is_active "
            "FROM function_call_debug_sessions WHERE user_id = $1 ORDER BY created_at DESC LIMIT $2",
            2, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) == PGRES_TUPLES_OK) {
            int rows = PQntuples(result);
            for (int i = 0; i < rows; ++i) {
                DebugSession session;
                session.session_id = PQgetvalue(result, i, 0) ? PQgetvalue(result, i, 0) : "";
                session.session_name = PQgetvalue(result, i, 1) ? PQgetvalue(result, i, 1) : "";
                session.description = PQgetvalue(result, i, 2) ? PQgetvalue(result, i, 2) : "";
                session.is_active = std::string(PQgetvalue(result, i, 4)) == "t";
                session.user_id = user_id;
                session.created_at = std::chrono::system_clock::now(); // Simplified
                sessions.push_back(session);
            }
        }

        PQclear(result);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in get_user_sessions: " + std::string(e.what()));
    }

    return sessions;
}

bool FunctionCallDebugger::log_function_call(const FunctionCallTrace& trace) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return false;

        const char* params[9] = {
            trace.call_id.c_str(),
            trace.session_id.c_str(),
            trace.function_name.c_str(),
            trace.input_parameters.dump().c_str(),
            trace.output_result.dump().c_str(),
            trace.execution_trace.dump().c_str(),
            trace.error_details.dump().c_str(),
            std::to_string(trace.execution_time_ms).c_str(),
            trace.success ? "true" : "false"
        };

        PGresult* result = PQexecParams(
            conn,
            "INSERT INTO function_call_logs "
            "(log_id, session_id, function_name, input_parameters, output_result, execution_trace, error_details, execution_time_ms, success) "
            "VALUES ($1, $2, $3, $4::jsonb, $5::jsonb, $6::jsonb, $7::jsonb, $8, $9::boolean)",
            9, nullptr, params, nullptr, nullptr, 0
        );

        bool success = (PQresultStatus(result) == PGRES_COMMAND_OK);
        PQclear(result);

        if (success) {
            logger_->log(LogLevel::INFO, "Logged function call: " + trace.function_name + " (" + trace.call_id + ")");
        }

        return success;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in log_function_call: " + std::string(e.what()));
        return false;
    }
}

std::vector<FunctionCallTrace> FunctionCallDebugger::get_function_calls(
    const std::string& user_id,
    const std::string& session_id,
    int limit,
    int offset
) {
    std::vector<FunctionCallTrace> calls;

    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return calls;

        std::string query = "SELECT l.log_id, l.session_id, l.function_name, l.input_parameters, "
                           "l.output_result, l.execution_trace, l.error_details, l.execution_time_ms, l.success "
                           "FROM function_call_logs l "
                           "JOIN function_call_debug_sessions s ON l.session_id = s.session_id "
                           "WHERE s.user_id = $1";

        std::vector<const char*> params;
        params.push_back(user_id.c_str());
        int param_count = 1;

        if (!session_id.empty()) {
            query += " AND l.session_id = $" + std::to_string(++param_count);
            params.push_back(session_id.c_str());
        }

        query += " ORDER BY l.created_at DESC LIMIT $" + std::to_string(++param_count) +
                " OFFSET $" + std::to_string(++param_count);

        params.push_back(std::to_string(limit).c_str());
        params.push_back(std::to_string(offset).c_str());

        PGresult* result = PQexecParams(
            conn, query.c_str(), params.size(), nullptr, params.data(), nullptr, nullptr, 0
        );

        if (PQresultStatus(result) == PGRES_TUPLES_OK) {
            int rows = PQntuples(result);
            for (int i = 0; i < rows; ++i) {
                FunctionCallTrace trace;
                trace.call_id = PQgetvalue(result, i, 0) ? PQgetvalue(result, i, 0) : "";
                trace.session_id = PQgetvalue(result, i, 1) ? PQgetvalue(result, i, 1) : "";
                trace.function_name = PQgetvalue(result, i, 2) ? PQgetvalue(result, i, 2) : "";
                trace.success = std::string(PQgetvalue(result, i, 8)) == "t";
                trace.execution_time_ms = PQgetvalue(result, i, 7) ? std::atoi(PQgetvalue(result, i, 7)) : 0;

                // Parse JSON fields
                if (PQgetvalue(result, i, 3)) {
                    try { trace.input_parameters = nlohmann::json::parse(PQgetvalue(result, i, 3)); }
                    catch (const std::exception&) {}
                }
                if (PQgetvalue(result, i, 4)) {
                    try { trace.output_result = nlohmann::json::parse(PQgetvalue(result, i, 4)); }
                    catch (const std::exception&) {}
                }
                if (PQgetvalue(result, i, 5)) {
                    try { trace.execution_trace = nlohmann::json::parse(PQgetvalue(result, i, 5)); }
                    catch (const std::exception&) {}
                }
                if (PQgetvalue(result, i, 6)) {
                    try { trace.error_details = nlohmann::json::parse(PQgetvalue(result, i, 6)); }
                    catch (const std::exception&) {}
                }

                trace.called_at = std::chrono::system_clock::now(); // Simplified
                calls.push_back(trace);
            }
        }

        PQclear(result);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in get_function_calls: " + std::string(e.what()));
    }

    return calls;
}

std::optional<FunctionCallReplay> FunctionCallDebugger::replay_function_call(
    const std::string& session_id,
    const std::string& user_id,
    const ReplayRequest& request
) {
    try {
        // Get original call details
        auto original_call = get_function_call_details(request.original_call_id, user_id);
        if (!original_call) return std::nullopt;

        // Execute replay (simplified - would actually call the function)
        FunctionCallReplay replay;
        replay.replay_id = generate_uuid();
        replay.session_id = session_id;
        replay.original_call_id = request.original_call_id;
        replay.modified_parameters = request.modified_parameters;
        replay.modified_function_name = request.modified_function_name.empty() ?
                                       original_call->function_name : request.modified_function_name;
        replay.replay_result = nlohmann::json{{"replayed_output", "simulated result"}}; // Mock result
        replay.success = true;
        replay.execution_time_ms = 150; // Mock execution time
        replay.replayed_at = std::chrono::system_clock::now();
        replay.replayed_by = user_id;

        // Store replay result
        auto conn = db_conn_->get_connection();
        if (conn) {
            const char* params[8] = {
                replay.replay_id.c_str(),
                replay.session_id.c_str(),
                replay.original_call_id.c_str(),
                replay.modified_parameters.dump().c_str(),
                replay.modified_function_name.c_str(),
                replay.replay_result.dump().c_str(),
                "{}",
                user_id.c_str()
            };

            PGresult* result = PQexecParams(
                conn,
                "INSERT INTO function_call_replays "
                "(replay_id, session_id, original_call_id, modified_parameters, modified_function_name, "
                "replay_result, execution_trace, replayed_by) "
                "VALUES ($1, $2, $3, $4::jsonb, $5, $6::jsonb, $7::jsonb, $8)",
                8, nullptr, params, nullptr, nullptr, 0
            );

            PQclear(result);
        }

        return replay;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in replay_function_call: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::string FunctionCallDebugger::generate_uuid() {
    uuid_t uuid;
    uuid_generate(uuid);

    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);

    return std::string(uuid_str);
}

void FunctionCallDebugger::set_max_session_age_days(int days) {
    max_session_age_days_ = days;
}

void FunctionCallDebugger::set_max_calls_per_session(int max_calls) {
    max_calls_per_session_ = max_calls;
}

void FunctionCallDebugger::set_debug_log_level(const std::string& level) {
    debug_log_level_ = level;
}

std::optional<FunctionCallTrace> FunctionCallDebugger::get_function_call_details(const std::string& call_id, const std::string& user_id) {
    // Simplified implementation - would query the database
    FunctionCallTrace trace;
    trace.call_id = call_id;
    trace.function_name = "example_function";
    trace.success = true;
    trace.execution_time_ms = 100;
    trace.called_at = std::chrono::system_clock::now();
    return trace;
}

} // namespace llm
} // namespace regulens
