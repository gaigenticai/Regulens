#include "jwt_parser.hpp"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <openssl/bio.h>
#include <sstream>
#include <ctime>
#include <iostream>
#include <vector>

namespace regulens {

JWTParser::JWTParser(const std::string& secret_key)
    : secret_key_(secret_key) {
}

std::string JWTParser::base64_url_decode(const std::string& input) {
    std::string base64 = input;
    // Replace URL-safe characters
    for (char& c : base64) {
        if (c == '-') c = '+';
        if (c == '_') c = '/';
    }

    // Add padding
    while (base64.length() % 4) {
        base64 += '=';
    }

    // Decode base64
    BIO *bio, *b64;
    const size_t decode_len = base64.length();
    std::vector<unsigned char> buffer(decode_len + 1);

    bio = BIO_new_mem_buf(base64.c_str(), -1);
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    const int length = BIO_read(bio, buffer.data(), static_cast<int>(decode_len));
    BIO_free_all(bio);

    if (length <= 0) {
        return {};
    }

    return std::string(reinterpret_cast<char*>(buffer.data()), static_cast<size_t>(length));
}

std::string JWTParser::hmac_sha256(const std::string& data, const std::string& key) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;

    HMAC(EVP_sha256(),
         key.c_str(), static_cast<int>(key.length()),
         reinterpret_cast<const unsigned char*>(data.c_str()), static_cast<int>(data.length()),
         hash, &hash_len);

    return std::string(reinterpret_cast<char*>(hash), hash_len);
}

std::optional<nlohmann::json> JWTParser::decode_payload(const std::string& token) {
    // Split token into parts
    std::istringstream iss(token);
    std::string header, payload, signature;

    if (!std::getline(iss, header, '.')) return std::nullopt;
    if (!std::getline(iss, payload, '.')) return std::nullopt;
    if (!std::getline(iss, signature, '.')) return std::nullopt;

    // Decode payload
    std::string decoded_payload = base64_url_decode(payload);

    try {
        return nlohmann::json::parse(decoded_payload);
    } catch (const nlohmann::json::exception& e) {
        return std::nullopt;
    }
}

bool JWTParser::validate_signature(const std::string& token) {
    // Split token
    size_t first_dot = token.find('.');
    size_t second_dot = token.find('.', first_dot + 1);

    if (first_dot == std::string::npos || second_dot == std::string::npos) {
        return false;
    }

    std::string message = token.substr(0, second_dot);
    std::string signature = token.substr(second_dot + 1);

    // Calculate expected signature
    std::string expected_sig = hmac_sha256(message, secret_key_);

    // Base64 URL encode expected signature
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, expected_sig.c_str(), static_cast<int>(expected_sig.length()));
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    std::string expected_sig_b64(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);

    // Remove padding from expected signature
    while (!expected_sig_b64.empty() && expected_sig_b64.back() == '=') {
        expected_sig_b64.pop_back();
    }

    // Replace + with - and / with _ for URL-safe base64
    for (char& c : expected_sig_b64) {
        if (c == '+') c = '-';
        if (c == '/') c = '_';
    }

    // Compare signatures (use constant-time comparison in production)
    return signature == expected_sig_b64;
}

bool JWTParser::is_expired(const JWTClaims& claims) {
    int64_t current_time = std::time(nullptr);
    return current_time >= claims.exp;
}

std::optional<JWTClaims> JWTParser::parse_token(const std::string& token) {
    // Validate signature first
    if (!validate_signature(token)) {
        return std::nullopt;
    }

    // Decode payload
    auto payload_opt = decode_payload(token);
    if (!payload_opt.has_value()) {
        return std::nullopt;
    }

    auto payload = payload_opt.value();

    // Extract claims
    JWTClaims claims;

    try {
        claims.user_id = payload.value("sub", "");  // Standard "sub" claim
        claims.username = payload.value("username", "");
        claims.email = payload.value("email", "");
        claims.exp = payload.value("exp", 0);
        claims.iat = payload.value("iat", 0);
        claims.jti = payload.value("jti", "");

        if (payload.contains("roles") && payload["roles"].is_array()) {
            for (const auto& role : payload["roles"]) {
                claims.roles.push_back(role.get<std::string>());
            }
        }

        // Validate required fields
        if (claims.user_id.empty() || claims.exp == 0) {
            return std::nullopt;
        }

        // Check expiration
        if (is_expired(claims)) {
            return std::nullopt;
        }

        return claims;

    } catch (const nlohmann::json::exception& e) {
        return std::nullopt;
    }
}

} // namespace regulens
