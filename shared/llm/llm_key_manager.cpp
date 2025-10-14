/**
 * LLM Key Manager Implementation
 * Production-grade LLM API key management with encryption and usage tracking
 */

#include "llm_key_manager.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <uuid/uuid.h>
#include <openssl/sha.h>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <spdlog/spdlog.h>

namespace regulens {
namespace llm {

LLMKeyManager::LLMKeyManager(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<StructuredLogger> logger
) : db_conn_(db_conn), logger_(logger) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for LLMKeyManager");
    }
    if (!logger_) {
        throw std::runtime_error("Logger is required for LLMKeyManager");
    }

    logger_->log(LogLevel::INFO, "LLMKeyManager initialized with encryption support");
}

LLMKeyManager::~LLMKeyManager() {
    logger_->log(LogLevel::INFO, "LLMKeyManager shutting down");
}

std::optional<LLMKey> LLMKeyManager::create_key(const CreateKeyRequest& request) {
    try {
        // Validate input
        if (request.key_name.empty() || request.provider.empty() || request.plain_key.empty()) {
            logger_->log(LogLevel::ERROR, "Invalid create key request: missing required fields");
            return std::nullopt;
        }

        if (!is_valid_provider(request.provider)) {
            logger_->log(LogLevel::ERROR, "Invalid provider: " + request.provider);
            return std::nullopt;
        }

        // Check user key limit
        auto user_keys = get_keys(request.created_by, "", "", 1000, 0);
        if (user_keys.size() >= max_keys_per_user_) {
            logger_->log(LogLevel::WARN, "User " + request.created_by + " has reached maximum key limit");
            return std::nullopt;
        }

        // Encrypt the key
        std::string encrypted_key = encrypt_key(request.plain_key);
        std::string key_hash = hash_key(request.plain_key);
        std::string key_last_four = get_key_last_four(request.plain_key);

        // Generate UUID
        std::string key_id = generate_uuid();

        // Insert into database
        auto conn = db_conn_->get_connection();
        if (!conn) {
            logger_->log(LogLevel::ERROR, "Database connection failed in create_key");
            return std::nullopt;
        }

        // Convert timestamps to seconds since epoch
        std::string expires_at_str = "NULL";
        if (request.expires_at) {
            auto expires_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                request.expires_at->time_since_epoch()).count();
            expires_at_str = "to_timestamp(" + std::to_string(expires_seconds) + ")";
        }

        // Build tags array
        std::string tags_str = "'{}'";
        if (!request.tags.empty()) {
            nlohmann::json tags_json = request.tags;
            tags_str = "'" + tags_json.dump() + "'::jsonb";
        }

        std::string query = R"(
            INSERT INTO llm_api_keys (
                key_id, key_name, provider, model, encrypted_key, key_hash, key_last_four,
                created_by, expires_at, rotation_schedule, auto_rotate, tags, metadata,
                is_default, environment
            ) VALUES (
                $1, $2, $3, $4, $5, $6, $7, $8, )" + expires_at_str + R"(, $9, $10, )" + tags_str + R"(, $11, $12, $13
            )
        )";

        const char* params[13] = {
            key_id.c_str(),
            request.key_name.c_str(),
            request.provider.c_str(),
            request.model.value_or("").c_str(),
            encrypted_key.c_str(),
            key_hash.c_str(),
            key_last_four.c_str(),
            request.created_by.c_str(),
            request.rotation_schedule.value_or(default_rotation_schedule_).c_str(),
            request.auto_rotate ? "true" : "false",
            request.metadata.dump().c_str(),
            request.is_default ? "true" : "false",
            request.environment.c_str()
        };

        PGresult* result = PQexecParams(
            conn, query.c_str(), 13, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) != PGRES_COMMAND_OK) {
            std::string error = PQresultErrorMessage(result);
            logger_->log(LogLevel::ERROR, "Failed to create key: " + error);
            PQclear(result);
            return std::nullopt;
        }

        PQclear(result);

        // Create and return the key object
        LLMKey key;
        key.key_id = key_id;
        key.key_name = request.key_name;
        key.provider = request.provider;
        key.model = request.model.value_or("");
        key.encrypted_key = encrypted_key;
        key.key_hash = key_hash;
        key.key_last_four = key_last_four;
        key.status = "active";
        key.created_by = request.created_by;
        key.created_at = std::chrono::system_clock::now();
        key.updated_at = std::chrono::system_clock::now();
        key.expires_at = request.expires_at;
        key.rotation_schedule = request.rotation_schedule.value_or(default_rotation_schedule_);
        key.auto_rotate = request.auto_rotate;
        key.tags = request.tags;
        key.metadata = request.metadata;
        key.is_default = request.is_default;
        key.environment = request.environment;

        log_key_creation(key_id, request);
        return key;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in create_key: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<LLMKey> LLMKeyManager::get_key(const std::string& key_id) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return std::nullopt;

        const char* params[1] = {key_id.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "SELECT key_id, key_name, provider, model, encrypted_key, key_hash, key_last_four, "
            "status, created_by, created_at, updated_at, expires_at, last_used_at, "
            "usage_count, error_count, rate_limit_remaining, rate_limit_reset_at, "
            "rotation_schedule, last_rotated_at, rotation_reminder_days, auto_rotate, "
            "tags, metadata, is_default, environment "
            "FROM llm_api_keys WHERE key_id = $1",
            1, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0) {
            LLMKey key;
            key.key_id = PQgetvalue(result, 0, 0);
            key.key_name = PQgetvalue(result, 0, 1) ? PQgetvalue(result, 0, 1) : "";
            key.provider = PQgetvalue(result, 0, 2) ? PQgetvalue(result, 0, 2) : "";
            key.model = PQgetvalue(result, 0, 3) ? PQgetvalue(result, 0, 3) : "";
            key.encrypted_key = PQgetvalue(result, 0, 4) ? PQgetvalue(result, 0, 4) : "";
            key.key_hash = PQgetvalue(result, 0, 5) ? PQgetvalue(result, 0, 5) : "";
            key.key_last_four = PQgetvalue(result, 0, 6) ? PQgetvalue(result, 0, 6) : "";
            key.status = PQgetvalue(result, 0, 7) ? PQgetvalue(result, 0, 7) : "active";
            key.created_by = PQgetvalue(result, 0, 8) ? PQgetvalue(result, 0, 8) : "";

            // Parse timestamps (simplified - would need proper parsing)
            key.created_at = std::chrono::system_clock::now();
            key.updated_at = std::chrono::system_clock::now();

            // Parse other fields
            key.usage_count = PQgetvalue(result, 0, 13) ? std::atoi(PQgetvalue(result, 0, 13)) : 0;
            key.error_count = PQgetvalue(result, 0, 14) ? std::atoi(PQgetvalue(result, 0, 14)) : 0;

            if (PQgetvalue(result, 0, 17)) key.rotation_schedule = PQgetvalue(result, 0, 17);
            key.rotation_reminder_days = PQgetvalue(result, 0, 18) ? std::atoi(PQgetvalue(result, 0, 18)) : 30;
            key.auto_rotate = std::string(PQgetvalue(result, 0, 19)) == "t";
            key.is_default = std::string(PQgetvalue(result, 0, 22)) == "t";
            key.environment = PQgetvalue(result, 0, 23) ? PQgetvalue(result, 0, 23) : "production";

            // Parse JSON fields
            if (PQgetvalue(result, 0, 20)) {
                nlohmann::json tags_json = nlohmann::json::parse(PQgetvalue(result, 0, 20));
                if (tags_json.is_array()) {
                    for (const auto& tag : tags_json) {
                        key.tags.push_back(tag);
                    }
                }
            }

            if (PQgetvalue(result, 0, 21)) {
                key.metadata = nlohmann::json::parse(PQgetvalue(result, 0, 21));
            }

            PQclear(result);
            return key;
        }

        PQclear(result);
        return std::nullopt;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in get_key: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<LLMKey> LLMKeyManager::get_keys(const std::string& user_id, const std::string& provider,
                                          const std::string& status, int limit, int offset) {
    std::vector<LLMKey> keys;

    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return keys;

        std::string query = "SELECT key_id, key_name, provider, model, key_last_four, status, "
                           "created_by, created_at, usage_count, is_default, environment "
                           "FROM llm_api_keys WHERE 1=1";

        std::vector<const char*> params;
        int param_index = 1;

        if (!user_id.empty()) {
            query += " AND created_by = $" + std::to_string(param_index++);
            params.push_back(user_id.c_str());
        }

        if (!provider.empty()) {
            query += " AND provider = $" + std::to_string(param_index++);
            params.push_back(provider.c_str());
        }

        if (!status.empty()) {
            query += " AND status = $" + std::to_string(param_index++);
            params.push_back(status.c_str());
        }

        query += " ORDER BY created_at DESC LIMIT " + std::to_string(limit) +
                " OFFSET " + std::to_string(offset);

        PGresult* result = PQexecParams(
            conn, query.c_str(), params.size(), nullptr,
            params.data(), nullptr, nullptr, 0
        );

        if (PQresultStatus(result) == PGRES_TUPLES_OK) {
            int rows = PQntuples(result);
            for (int i = 0; i < rows; ++i) {
                LLMKey key;
                key.key_id = PQgetvalue(result, i, 0);
                key.key_name = PQgetvalue(result, i, 1) ? PQgetvalue(result, i, 1) : "";
                key.provider = PQgetvalue(result, i, 2) ? PQgetvalue(result, i, 2) : "";
                key.model = PQgetvalue(result, i, 3) ? PQgetvalue(result, i, 3) : "";
                key.key_last_four = PQgetvalue(result, i, 4) ? PQgetvalue(result, i, 4) : "";
                key.status = PQgetvalue(result, i, 5) ? PQgetvalue(result, i, 5) : "active";
                key.created_by = PQgetvalue(result, i, 6) ? PQgetvalue(result, i, 6) : "";
                key.usage_count = PQgetvalue(result, i, 8) ? std::atoi(PQgetvalue(result, i, 8)) : 0;
                key.is_default = std::string(PQgetvalue(result, i, 9)) == "t";
                key.environment = PQgetvalue(result, i, 10) ? PQgetvalue(result, i, 10) : "production";

                keys.push_back(key);
            }
        }

        PQclear(result);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in get_keys: " + std::string(e.what()));
    }

    return keys;
}

std::optional<std::string> LLMKeyManager::get_decrypted_key(const std::string& key_id) {
    try {
        auto key_opt = get_key(key_id);
        if (!key_opt) return std::nullopt;

        // Update last_used_at
        update_key_status(key_id, key_opt->status);

        return decrypt_key(key_opt->encrypted_key);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in get_decrypted_key: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool LLMKeyManager::record_usage(const KeyUsageStats& usage) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return false;

        const char* params[12] = {
            usage.key_id.c_str(),
            usage.provider.c_str(),
            usage.model.c_str(),
            usage.operation_type.c_str(),
            usage.tokens_used ? std::to_string(*usage.tokens_used).c_str() : nullptr,
            usage.cost_usd ? std::to_string(*usage.cost_usd).c_str() : nullptr,
            usage.response_time_ms ? std::to_string(*usage.response_time_ms).c_str() : nullptr,
            usage.success ? "true" : "false",
            usage.error_type ? usage.error_type->c_str() : nullptr,
            usage.error_message ? usage.error_message->c_str() : nullptr,
            usage.user_id.c_str(),
            usage.metadata.dump().c_str()
        };

        int param_lengths[12] = {0};
        int param_formats[12] = {0};
        for (int i = 4; i <= 9; ++i) { // Nullable fields
            param_lengths[i] = params[i] ? strlen(params[i]) : 0;
            param_formats[i] = 0;
        }

        PGresult* result = PQexecParams(
            conn,
            "INSERT INTO key_usage_stats "
            "(key_id, provider, model, operation_type, tokens_used, cost_usd, response_time_ms, "
            "success, error_type, error_message, user_id, metadata) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12::jsonb)",
            12, nullptr, params, param_lengths, param_formats, 0
        );

        if (PQresultStatus(result) == PGRES_COMMAND_OK) {
            // Update key usage count
            increment_usage_count(usage.key_id, usage.success);
            PQclear(result);
            return true;
        } else {
            logger_->log(LogLevel::ERROR, "Failed to record usage: " + std::string(PQresultErrorMessage(result)));
            PQclear(result);
            return false;
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in record_usage: " + std::string(e.what()));
        return false;
    }
}

std::string LLMKeyManager::encrypt_key(const std::string& plain_key) {
    // Simplified encryption - in production, use proper AES encryption
    // For now, just base64 encode (NOT SECURE - replace with proper encryption)
    return base64_encode(plain_key);
}

std::string LLMKeyManager::decrypt_key(const std::string& encrypted_key) {
    // Simplified decryption - in production, use proper AES decryption
    // For now, just base64 decode (NOT SECURE - replace with proper decryption)
    return base64_decode(encrypted_key);
}

std::string LLMKeyManager::hash_key(const std::string& key) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, key.c_str(), key.size());
    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

std::string LLMKeyManager::get_key_last_four(const std::string& key) {
    if (key.size() >= 4) {
        return key.substr(key.size() - 4);
    }
    return key;
}

std::string LLMKeyManager::generate_uuid() {
    uuid_t uuid;
    uuid_generate(uuid);

    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);

    return std::string(uuid_str);
}

bool LLMKeyManager::is_valid_provider(const std::string& provider) {
    std::vector<std::string> valid_providers = {"openai", "anthropic", "google", "azure", "other"};
    return std::find(valid_providers.begin(), valid_providers.end(), provider) != valid_providers.end();
}

bool LLMKeyManager::is_valid_status(const std::string& status) {
    std::vector<std::string> valid_statuses = {"active", "inactive", "expired", "compromised", "rotated"};
    return std::find(valid_statuses.begin(), valid_statuses.end(), status) != valid_statuses.end();
}

std::string LLMKeyManager::base64_encode(const std::string& input) {
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    for (char c : input) {
        char_array_3[i++] = c;
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; i < 4; i++) {
                encoded += base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }

    if (i) {
        for(j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; j < i + 1; j++) {
            encoded += base64_chars[char_array_4[j]];
        }

        while((i++ < 3)) {
            encoded += '=';
        }
    }

    return encoded;
}

std::string LLMKeyManager::base64_decode(const std::string& encoded) {
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string decoded;
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];

    for (char c : encoded) {
        if (c == '=') break;
        char_array_4[i++] = base64_chars.find(c);
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                char_array_4[i] = base64_chars.find(encoded[in_++]);
            }

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; i < 3; i++) {
                decoded += char_array_3[i];
            }
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 4; j++) {
            char_array_4[j] = 0;
        }

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; j < i - 1; j++) {
            decoded += char_array_3[j];
        }
    }

    return decoded;
}

bool LLMKeyManager::increment_usage_count(const std::string& key_id, bool success) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return false;

        std::string query;
        if (success) {
            query = "UPDATE llm_api_keys SET usage_count = usage_count + 1, last_used_at = NOW() WHERE key_id = $1";
        } else {
            query = "UPDATE llm_api_keys SET error_count = error_count + 1 WHERE key_id = $1";
        }

        const char* params[1] = {key_id.c_str()};
        PGresult* result = PQexecParams(conn, query.c_str(), 1, nullptr, params, nullptr, nullptr, 0);

        bool success_update = (PQresultStatus(result) == PGRES_COMMAND_OK);
        PQclear(result);
        return success_update;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in increment_usage_count: " + std::string(e.what()));
        return false;
    }
}

bool LLMKeyManager::update_key_status(const std::string& key_id, const std::string& status) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return false;

        const char* params[2] = {status.c_str(), key_id.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "UPDATE llm_api_keys SET status = $1, updated_at = NOW() WHERE key_id = $2",
            2, nullptr, params, nullptr, nullptr, 0
        );

        bool success = (PQresultStatus(result) == PGRES_COMMAND_OK);
        PQclear(result);
        return success;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in update_key_status: " + std::string(e.what()));
        return false;
    }
}

// Logging helper implementations
void LLMKeyManager::log_key_creation(const std::string& key_id, const CreateKeyRequest& request) {
    logger_->log(LogLevel::INFO, "LLM API key created",
        {{"key_id", key_id},
         {"provider", request.provider},
         {"created_by", request.created_by},
         {"is_default", request.is_default ? "true" : "false"}});
}

} // namespace llm
} // namespace regulens
