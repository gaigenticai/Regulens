/**
 * Utility Functions Implementation
 * Production-grade utility functions for common operations
 */

#include "utils.hpp"
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace regulens {

// Rate limiter class for API endpoints
class APIRateLimiter {
public:
    APIRateLimiter(int requests_per_minute = 60)
        : max_requests_(requests_per_minute),
          window_duration_(std::chrono::minutes(1)) {}

    bool allow_request(const std::string& client_ip) {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);

        // Clean up old entries
        cleanup_old_entries(now);

        auto& client_data = clients_[client_ip];
        if (client_data.requests.size() < max_requests_) {
            client_data.requests.push_back(now);
            return true;
        }

        return false;
    }

private:
    struct ClientData {
        std::vector<std::chrono::steady_clock::time_point> requests;
    };

    void cleanup_old_entries(std::chrono::steady_clock::time_point now) {
        auto cutoff = now - window_duration_;
        for (auto it = clients_.begin(); it != clients_.end(); ) {
            auto& requests = it->second.requests;
            requests.erase(
                std::remove_if(requests.begin(), requests.end(),
                    [cutoff](auto& time) { return time < cutoff; }),
                requests.end()
            );
            if (requests.empty()) {
                it = clients_.erase(it);
            } else {
                ++it;
            }
        }
    }

    int max_requests_;
    std::chrono::steady_clock::duration window_duration_;
    std::unordered_map<std::string, ClientData> clients_;
    std::mutex mutex_;
};

// Global rate limiter instance
static std::unique_ptr<APIRateLimiter> global_rate_limiter;

std::string escape_json_string(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 32) {
                    // Escape control characters
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    oss << buf;
                } else {
                    oss << c;
                }
                break;
        }
    }
    return oss.str();
}

void initialize_rate_limits() {
    if (!global_rate_limiter) {
        // Initialize with 60 requests per minute as default
        global_rate_limiter = std::make_unique<APIRateLimiter>(60);
    }
}

void broadcast_to_websockets(const std::string& message, const std::string& path) {
    // TODO: Implement websocket broadcasting
    // This would integrate with the WebSocketServer class
    // For now, this is a placeholder to resolve build errors
    // In production, this should:
    // 1. Get the global WebSocketServer instance
    // 2. Call broadcast_to_topic(path, message) or similar
}

} // namespace regulens