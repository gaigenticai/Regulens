#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "../database/postgresql_connection.hpp"

namespace regulens {

/**
 * @brief Centralised access-control service that provides reusable, production-grade
 *        authorisation primitives across the platform. The implementation loads
 *        role, permission, and scope metadata from PostgreSQL, applies in-memory
 *        caching for performance, and exposes helpers for fine-grained checks.
 */
class AccessControlService {
public:
    struct PermissionQuery {
        std::string operation;
        std::string resource_type;
        std::string resource_id;
        int minimum_level = 0;
    };

    explicit AccessControlService(std::shared_ptr<PostgreSQLConnection> db_conn,
                                  std::chrono::minutes cache_ttl = std::chrono::minutes(5));

    /**
     * @brief Returns true when the user possesses the requested permission.
     */
    bool has_permission(const std::string& user_id,
                        const std::string& operation,
                        const std::string& resource_type = "",
                        const std::string& resource_id = "",
                        int minimum_level = 0) const;

    /**
     * @brief Convenience wrapper around has_permission using a richer query object.
     */
    bool has_permission(const std::string& user_id, const PermissionQuery& query) const;

    /**
     * @brief Returns true when the user owns any permission in the provided list.
     */
    bool has_any_permission(const std::string& user_id,
                            const std::vector<PermissionQuery>& queries) const;

    /**
     * @brief Determines whether the subject is recognised as a platform administrator.
     */
    bool is_admin(const std::string& user_id) const;

    /**
     * @brief Retrieves the scopes the user may access (config scopes, knowledge domains, etc.).
     */
    std::vector<std::string> get_user_scopes(const std::string& user_id) const;

    /**
     * @brief Checks whether the user has access to the given configuration scope.
     */
    bool has_scope_access(const std::string& user_id, const std::string& scope) const;

    /**
     * @brief Validates whether the user participates in the referenced conversation thread.
     */
    bool is_conversation_participant(const std::string& user_id, const std::string& conversation_id) const;

    /**
     * @brief Validates whether the user is a facilitator (or privileged manager) for the conversation.
     */
    bool is_conversation_facilitator(const std::string& user_id, const std::string& conversation_id) const;

    /**
     * @brief Clears cached permission state for a user. Intended for administrative invalidation.
     */
    void invalidate_user(const std::string& user_id);

private:
    struct PermissionRecord {
        std::string operation;
        std::string resource_type;
        std::string resource_id;
        int level = 0;
        std::chrono::system_clock::time_point expires_at;
    };

    struct UserContext {
        bool valid = false;
        bool is_admin = false;
        std::unordered_map<std::string, std::vector<PermissionRecord>> permissions_by_operation;
        std::unordered_map<std::string, std::vector<PermissionRecord>> permissions_by_resource_type;
        std::unordered_set<std::string> scope_permissions;
        std::chrono::steady_clock::time_point expiry;
    };

    struct ConversationAccess {
        std::unordered_set<std::string> participants;
        std::unordered_set<std::string> facilitators;
        std::chrono::steady_clock::time_point expiry;
    };

    std::optional<UserContext> get_user_context(const std::string& user_id) const;
    std::optional<UserContext> load_user_context(const std::string& user_id) const;
    std::optional<std::string> resolve_internal_user_id(const std::string& user_id) const;
    bool table_exists(const std::string& table_name) const;
    bool has_column(const std::string& table_name, const std::string& column_name) const;
    void refresh_schema_metadata() const;

    std::optional<ConversationAccess> get_conversation_access(const std::string& conversation_id) const;
    std::optional<ConversationAccess> load_conversation_access(const std::string& conversation_id) const;

    static std::string normalize_token(const std::string& value);
    static std::chrono::system_clock::time_point parse_timestamp(const std::string& timestamp_str);
    static bool resource_matches(const std::string& requested, const std::string& candidate);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::chrono::minutes cache_ttl_;

    mutable std::mutex cache_mutex_;
    mutable std::unordered_map<std::string, UserContext> user_cache_;
    mutable std::unordered_map<std::string, ConversationAccess> conversation_cache_;

    mutable std::mutex schema_mutex_;
    mutable std::unordered_map<std::string, std::unordered_set<std::string>> table_columns_;
};

} // namespace regulens
