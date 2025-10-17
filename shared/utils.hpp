/**
 * Utility Functions
 * Production-grade utility functions for common operations
 */

#ifndef REGULENS_UTILS_HPP
#define REGULENS_UTILS_HPP

#include <string>

namespace regulens {

/**
 * Escape a string for JSON output
 * @param str The string to escape
 * @return The escaped string
 */
std::string escape_json_string(const std::string& str);

/**
 * Initialize rate limiting for API endpoints
 */
void initialize_rate_limits();

/**
 * Broadcast a message to websocket clients on a specific path
 * @param message The message to broadcast
 * @param path The websocket path to broadcast to
 */
void broadcast_to_websockets(const std::string& message, const std::string& path);

} // namespace regulens

#endif // REGULENS_UTILS_HPP