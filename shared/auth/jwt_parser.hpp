#ifndef JWT_PARSER_H
#define JWT_PARSER_H

#include <string>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>

namespace regulens {

struct JWTClaims {
    std::string user_id;
    std::string username;
    std::string email;
    std::vector<std::string> roles;
    int64_t exp;  // Expiration timestamp
    int64_t iat;  // Issued at timestamp
    std::string jti;  // JWT ID
};

class JWTParser {
public:
    JWTParser(const std::string& secret_key);

    // Parse and validate JWT token
    std::optional<JWTClaims> parse_token(const std::string& token);

    // Validate token signature
    bool validate_signature(const std::string& token);

    // Check if token is expired
    bool is_expired(const JWTClaims& claims);

    // Extract claims without validation (for debugging)
    std::optional<nlohmann::json> decode_payload(const std::string& token);

private:
    std::string secret_key_;

    std::string base64_url_decode(const std::string& input);
    std::string hmac_sha256(const std::string& data, const std::string& key);
};

} // namespace regulens

#endif // JWT_PARSER_H
