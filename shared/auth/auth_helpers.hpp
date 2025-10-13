#ifndef AUTH_HELPERS_H
#define AUTH_HELPERS_H

#include <string>
#include <optional>
#include <map>
#include "jwt_parser.hpp"

namespace regulens {

// Helper function to extract user_id from Authorization header
std::optional<std::string> extract_user_id_from_request(
    const std::map<std::string, std::string>& headers,
    regulens::JWTParser& jwt_parser);

} // namespace regulens

#endif // AUTH_HELPERS_H
