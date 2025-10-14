#include "access_control_service.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace regulens {

namespace {

inline bool to_bool(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    const std::string lower = AccessControlService::normalize_token(value);
    return lower == "true" || lower == "t" || lower == "1" || lower == "yes" || lower == "y";
}

} // namespace

AccessControlService::AccessControlService(std::shared_ptr<PostgreSQLConnection> db_conn,
                                           std::chrono::minutes cache_ttl)
    : db_conn_(std::move(db_conn)), cache_ttl_(cache_ttl) {
    if (!db_conn_) {
        throw std::invalid_argument("AccessControlService requires a valid PostgreSQLConnection");
    }
    if (!db_conn_->is_connected() && !db_conn_->connect()) {
        spdlog::warn("AccessControlService: database connection is not established");
    }

    refresh_schema_metadata();
}

void AccessControlService::refresh_schema_metadata() const {
    if (!db_conn_ || !db_conn_->is_connected()) {
        return;
    }

    static const std::string query =
        "SELECT LOWER(table_name) AS table_name, LOWER(column_name) AS column_name "
        "FROM information_schema.columns "
        "WHERE table_schema NOT IN ('pg_catalog','information_schema') "
        "AND table_name IN ('users','user_permissions','user_roles','roles','conversation_contexts')";

    auto result = db_conn_->execute_query(query, {});

    std::lock_guard<std::mutex> lock(schema_mutex_);
    table_columns_.clear();
    for (const auto& row : result.rows) {
        auto table_it = row.find("table_name");
        auto column_it = row.find("column_name");
        if (table_it == row.end() || column_it == row.end()) {
            continue;
        }
        table_columns_[table_it->second].insert(column_it->second);
    }
}

bool AccessControlService::table_exists(const std::string& table_name) const {
    std::lock_guard<std::mutex> lock(schema_mutex_);
    return table_columns_.find(normalize_token(table_name)) != table_columns_.end();
}

bool AccessControlService::has_column(const std::string& table_name, const std::string& column_name) const {
    std::lock_guard<std::mutex> lock(schema_mutex_);
    const auto table_it = table_columns_.find(normalize_token(table_name));
    if (table_it == table_columns_.end()) {
        return false;
    }
    return table_it->second.find(normalize_token(column_name)) != table_it->second.end();
}

std::string AccessControlService::normalize_token(const std::string& value) {
    std::string normalised;
    normalised.reserve(value.size());
    for (char ch : value) {
        normalised.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return normalised;
}

std::chrono::system_clock::time_point AccessControlService::parse_timestamp(const std::string& timestamp_str) {
    if (timestamp_str.empty()) {
        return std::chrono::system_clock::time_point::max();
    }

    std::tm tm{};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        ss.clear();
        ss.str(timestamp_str);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    }
    if (ss.fail()) {
        return std::chrono::system_clock::time_point::max();
    }

    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

bool AccessControlService::resource_matches(const std::string& requested, const std::string& candidate) {
    if (candidate.empty() || candidate == "*") {
        return true;
    }
    if (requested.empty() || requested == "*") {
        return true;
    }
    return normalize_token(requested) == normalize_token(candidate);
}

std::optional<std::string> AccessControlService::resolve_internal_user_id(const std::string& user_id) const {
    if (user_id.empty() || !db_conn_ || !db_conn_->is_connected()) {
        return std::nullopt;
    }

    if (table_exists("users")) {
        std::string query = R"(
            SELECT u.id::text AS internal_id, u.is_active
            FROM users u
            WHERE u.user_id = $1
            LIMIT 1
        )";

        auto result = db_conn_->execute_query(query, {user_id});
        if (!result.rows.empty()) {
            const auto& row = result.rows.front();
            if (!to_bool(row.at("is_active"))) {
                return std::nullopt;
            }
            auto id_it = row.find("internal_id");
            if (id_it != row.end()) {
                return id_it->second;
            }
        }
    }

    if (table_exists("user_authentication")) {
        std::string query = R"(
            SELECT ua.user_id::text AS internal_id, ua.is_active
            FROM user_authentication ua
            WHERE ua.user_id::text = $1 OR ua.username = $1 OR ua.email = $1
            LIMIT 1
        )";

        auto result = db_conn_->execute_query(query, {user_id});
        if (!result.rows.empty()) {
            const auto& row = result.rows.front();
            if (!to_bool(row.at("is_active"))) {
                return std::nullopt;
            }
            auto id_it = row.find("internal_id");
            if (id_it != row.end()) {
                return id_it->second;
            }
        }
    }

    return std::nullopt;
}

std::optional<AccessControlService::UserContext> AccessControlService::get_user_context(const std::string& user_id) const {
    if (user_id.empty()) {
        return std::nullopt;
    }

    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = user_cache_.find(user_id);
        if (it != user_cache_.end() && it->second.expiry > now) {
            return it->second;
        }
    }

    auto context = load_user_context(user_id);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        if (context) {
            user_cache_[user_id] = *context;
        } else {
            user_cache_.erase(user_id);
        }
    }

    return context;
}

std::optional<AccessControlService::UserContext> AccessControlService::load_user_context(const std::string& user_id) const {
    if (!db_conn_ || !db_conn_->is_connected() || !table_exists("user_permissions")) {
        return std::nullopt;
    }

    auto internal_id = resolve_internal_user_id(user_id);
    if (!internal_id) {
        return std::nullopt;
    }

    UserContext context;
    context.valid = true;
    context.expiry = std::chrono::steady_clock::now() + cache_ttl_;

    // Determine administrative roles
    if (table_exists("user_roles") && table_exists("roles") && table_exists("users")) {
        std::string role_query = R"(
            SELECT r.role_name, r.role_level
            FROM user_roles ur
            INNER JOIN roles r ON r.id = ur.role_id
            INNER JOIN users u ON u.id = ur.user_id
            WHERE u.user_id = $1 AND ur.is_active = true AND u.is_active = true
            ORDER BY r.role_level DESC
        )";

        auto roles = db_conn_->execute_query(role_query, {user_id});
        if (!roles.rows.empty()) {
            const auto& row = roles.rows.front();
            const std::string role_name = normalize_token(row.at("role_name"));
            const int role_level = row.count("role_level") ? std::stoi(row.at("role_level")) : 0;
            if (role_level >= 90 || role_name == "administrator" || role_name == "super_admin" || role_name == "admin") {
                context.is_admin = true;
            }
        }
    }

    // Load explicit permissions
    std::string permission_query = R"(
        SELECT
            COALESCE(LOWER(p.operation), '') AS operation,
            COALESCE(LOWER(p.resource_type), '') AS resource_type,
            COALESCE(p.resource_id, '') AS resource_id,
            COALESCE(p.permission_level, '0') AS permission_level,
            COALESCE(p.scope, '') AS scope,
            COALESCE(p.expires_at::text, '') AS expires_at
        FROM user_permissions p
        INNER JOIN users u ON u.id = p.user_id
        WHERE u.user_id = $1 AND u.is_active = true AND p.is_active = true
    )";

    auto permissions = db_conn_->execute_query(permission_query, {user_id});
    const auto now_tp = std::chrono::system_clock::now();

    for (const auto& row : permissions.rows) {
        PermissionRecord record;
        record.operation = row.at("operation");
        if (record.operation.empty()) {
            continue;
        }
        record.resource_type = row.at("resource_type");
        record.resource_id = row.at("resource_id");
        try {
            record.level = std::stoi(row.at("permission_level"));
        } catch (...) {
            record.level = 0;
        }
        record.expires_at = parse_timestamp(row.at("expires_at"));

        if (record.expires_at <= now_tp) {
            continue; // expired permission
        }

        context.permissions_by_operation[record.operation].push_back(record);
        context.permissions_by_resource_type[normalize_token(record.resource_type)].push_back(record);

        const std::string scope_value = normalize_token(row.at("scope"));
        if (!scope_value.empty()) {
            context.scope_permissions.insert(scope_value);
        }
        if (normalize_token(record.resource_type) == "config_scope" || normalize_token(record.resource_type) == "scope") {
            if (record.resource_id.empty()) {
                context.scope_permissions.insert("*");
            } else {
                context.scope_permissions.insert(normalize_token(record.resource_id));
            }
        }
        if (record.operation == "*") {
            context.is_admin = true;
        }
    }

    return context;
}

bool AccessControlService::has_permission(const std::string& user_id,
                                          const std::string& operation,
                                          const std::string& resource_type,
                                          const std::string& resource_id,
                                          int minimum_level) const {
    if (user_id.empty() || operation.empty()) {
        return false;
    }

    auto context_opt = get_user_context(user_id);
    if (!context_opt || !context_opt->valid) {
        return false;
    }

    const auto& context = *context_opt;
    if (context.is_admin) {
        return true;
    }

    const std::string op = normalize_token(operation);
    const std::string res_type = normalize_token(resource_type);
    const std::string res_id = resource_id;

    auto evaluate = [&](const std::vector<PermissionRecord>& records) {
        for (const auto& record : records) {
            if (record.level < minimum_level) {
                continue;
            }
            if (!res_type.empty()) {
                if (!resource_matches(res_type, record.resource_type)) {
                    continue;
                }
            }
            if (!res_id.empty()) {
                if (!resource_matches(res_id, record.resource_id)) {
                    continue;
                }
            }
            return true;
        }
        return false;
    };

    auto it = context.permissions_by_operation.find(op);
    if (it != context.permissions_by_operation.end() && evaluate(it->second)) {
        return true;
    }

    auto wildcard = context.permissions_by_operation.find("*");
    if (wildcard != context.permissions_by_operation.end() && evaluate(wildcard->second)) {
        return true;
    }

    return false;
}

bool AccessControlService::has_permission(const std::string& user_id, const PermissionQuery& query) const {
    return has_permission(user_id, query.operation, query.resource_type, query.resource_id, query.minimum_level);
}

bool AccessControlService::has_any_permission(const std::string& user_id,
                                              const std::vector<PermissionQuery>& queries) const {
    for (const auto& query : queries) {
        if (has_permission(user_id, query)) {
            return true;
        }
    }
    return false;
}

bool AccessControlService::is_admin(const std::string& user_id) const {
    auto context_opt = get_user_context(user_id);
    return context_opt && context_opt->valid && context_opt->is_admin;
}

std::vector<std::string> AccessControlService::get_user_scopes(const std::string& user_id) const {
    auto context_opt = get_user_context(user_id);
    if (!context_opt || !context_opt->valid) {
        return {};
    }

    const auto& scopes = context_opt->scope_permissions;
    return std::vector<std::string>(scopes.begin(), scopes.end());
}

bool AccessControlService::has_scope_access(const std::string& user_id, const std::string& scope) const {
    auto context_opt = get_user_context(user_id);
    if (!context_opt || !context_opt->valid) {
        return false;
    }

    if (context_opt->is_admin) {
        return true;
    }

    const auto& scopes = context_opt->scope_permissions;
    if (scopes.find("*") != scopes.end()) {
        return true;
    }

    const std::string normalized_scope = normalize_token(scope);
    return scopes.find(normalized_scope) != scopes.end();
}

std::optional<AccessControlService::ConversationAccess> AccessControlService::get_conversation_access(
    const std::string& conversation_id) const {
    if (conversation_id.empty()) {
        return std::nullopt;
    }

    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = conversation_cache_.find(conversation_id);
        if (it != conversation_cache_.end() && it->second.expiry > now) {
            return it->second;
        }
    }

    auto access = load_conversation_access(conversation_id);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        if (access) {
            conversation_cache_[conversation_id] = *access;
        } else {
            conversation_cache_.erase(conversation_id);
        }
    }

    return access;
}

std::optional<AccessControlService::ConversationAccess> AccessControlService::load_conversation_access(
    const std::string& conversation_id) const {
    if (!db_conn_ || !db_conn_->is_connected() || !table_exists("conversation_contexts")) {
        return std::nullopt;
    }

    std::string query = R"(
        SELECT participants
        FROM conversation_contexts
        WHERE conversation_id = $1
        LIMIT 1
    )";

    auto result = db_conn_->execute_query_single(query, {conversation_id});
    if (!result.has_value()) {
        return std::nullopt;
    }

    const auto participants_raw = result->value("participants", "");
    if (participants_raw.empty()) {
        return std::nullopt;
    }

    ConversationAccess access;
    access.expiry = std::chrono::steady_clock::now() + cache_ttl_;

    try {
        auto participants_json = nlohmann::json::parse(participants_raw);
        if (participants_json.is_array()) {
            for (const auto& entry : participants_json) {
                if (!entry.is_object()) {
                    continue;
                }
                std::string agent_id = entry.value("agent_id", "");
                std::string user_reference = entry.value("user_id", "");
                std::string facilitator_flag = normalize_token(entry.value("role", ""));

                if (!agent_id.empty()) {
                    access.participants.insert(normalize_token(agent_id));
                }
                if (!user_reference.empty()) {
                    access.participants.insert(normalize_token(user_reference));
                }
                if (facilitator_flag == "facilitator" || facilitator_flag == "leader" || facilitator_flag == "moderator") {
                    if (!user_reference.empty()) {
                        access.facilitators.insert(normalize_token(user_reference));
                    }
                    if (!agent_id.empty()) {
                        access.facilitators.insert(normalize_token(agent_id));
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("AccessControlService: failed to parse participants for conversation {}: {}",
                     conversation_id, e.what());
        return std::nullopt;
    }

    return access;
}

bool AccessControlService::is_conversation_participant(const std::string& user_id,
                                                       const std::string& conversation_id) const {
    auto access = get_conversation_access(conversation_id);
    if (!access) {
        return false;
    }
    const std::string normalized = normalize_token(user_id);
    return access->participants.find(normalized) != access->participants.end() ||
           access->facilitators.find(normalized) != access->facilitators.end();
}

bool AccessControlService::is_conversation_facilitator(const std::string& user_id,
                                                       const std::string& conversation_id) const {
    auto access = get_conversation_access(conversation_id);
    if (!access) {
        return false;
    }
    const std::string normalized = normalize_token(user_id);
    return access->facilitators.find(normalized) != access->facilitators.end();
}

void AccessControlService::invalidate_user(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    user_cache_.erase(user_id);
}

} // namespace regulens
