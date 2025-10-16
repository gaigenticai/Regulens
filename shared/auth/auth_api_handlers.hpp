/**
 * Authentication API Handlers - Header File
 * Production-grade authentication endpoint declarations
 * Implements JWT-based authentication with secure token management
 */

#ifndef REGULENS_AUTH_API_HANDLERS_HPP
#define REGULENS_AUTH_API_HANDLERS_HPP

#include <string>
#include <map>
#include <optional>
#include <libpq-fe.h>

namespace regulens {
namespace auth {

// Authentication endpoints
std::string login_user(PGconn* db_conn, const std::string& request_body);
std::string logout_user(PGconn* db_conn, const std::map<std::string, std::string>& headers);
std::string get_current_user(PGconn* db_conn, const std::map<std::string, std::string>& headers);
std::string refresh_token(PGconn* db_conn, const std::string& request_body);

// Helper functions
std::string generate_jwt_token(const std::string& user_id, const std::string& username,
                              const std::vector<std::string>& roles, int expires_in_hours = 24);
std::string generate_refresh_token(const std::string& user_id);
bool validate_refresh_token(PGconn* db_conn, const std::string& refresh_token, std::string& user_id);
void revoke_refresh_token(PGconn* db_conn, const std::string& refresh_token);
bool verify_password(const std::string& password, const std::string& hashed_password);
std::string hash_password(const std::string& password);
std::string base64_url_encode(const std::string& input);
std::string base64_url_decode(const std::string& input);
std::string hmac_sha256(const std::string& data, const std::string& key);
std::string sha256(const std::string& input);

} // namespace auth
} // namespace regulens

#endif // REGULENS_AUTH_API_HANDLERS_HPP