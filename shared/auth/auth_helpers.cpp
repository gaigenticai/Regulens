#include "auth_helpers.hpp"
#include <iostream>

namespace regulens {

// Helper function to extract user_id from Authorization header
std::optional<std::string> extract_user_id_from_request(
    const std::map<std::string, std::string>& headers,
    regulens::JWTParser& jwt_parser) {

    // Find Authorization header
    auto auth_it = headers.find("authorization");
    if (auth_it == headers.end()) {
        auth_it = headers.find("Authorization");
        if (auth_it == headers.end()) {
            return std::nullopt;
        }
    }

    std::string auth_header = auth_it->second;

    // Extract token (format: "Bearer <token>")
    if (auth_header.find("Bearer ") != 0) {
        return std::nullopt;
    }

    std::string token = auth_header.substr(7);  // Skip "Bearer "

    // Parse JWT
    auto claims_opt = jwt_parser.parse_token(token);
    if (!claims_opt.has_value()) {
        return std::nullopt;
    }

    return claims_opt.value().user_id;
}

} // namespace regulens
