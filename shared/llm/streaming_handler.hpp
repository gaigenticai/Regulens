/**
 * Streaming Response Handler - Enterprise Streaming Interface
 *
 * Production-grade streaming response support for real-time LLM interactions.
 * Provides Server-Sent Events (SSE) parsing, callback-based processing,
 * and comprehensive error handling for streaming responses.
 *
 * Features:
 * - SSE parsing and event processing
 * - Callback-based streaming interface
 * - Partial response accumulation and validation
 * - Rate limiting and error handling
 * - Thread-safe streaming session management
 * - Connection pooling for streaming requests
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <nlohmann/json.hpp>

#include "../logging/structured_logger.hpp"
#include "../error_handler.hpp"

namespace regulens {

// Forward declarations
class StreamingSession;

/**
 * @brief Streaming event types for different LLM providers
 */
enum class StreamingEventType {
    START,          // Stream started
    TOKEN,          // New token received
    COMPLETION,     // Completion finished
    ERROR,          // Error occurred
    DONE            // Stream ended
};

/**
 * @brief Streaming event data structure
 */
struct StreamingEvent {
    StreamingEventType type;
    std::string data;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> metadata;

    StreamingEvent(StreamingEventType t, const std::string& d = "",
                   const std::unordered_map<std::string, std::string>& m = {})
        : type(t), data(d), timestamp(std::chrono::system_clock::now()), metadata(m) {}
};

/**
 * @brief Callback function type for streaming events
 */
using StreamingCallback = std::function<void(const StreamingEvent&)>;

/**
 * @brief Completion callback for final response
 */
using CompletionCallback = std::function<void(const nlohmann::json&)>;

/**
 * @brief Error callback for streaming errors
 */
using ErrorCallback = std::function<void(const std::string&)>;

/**
 * @brief Streaming configuration
 */
struct StreamingConfig {
    bool enable_streaming = true;
    size_t max_buffer_size = 1024 * 1024;  // 1MB max buffer
    std::chrono::milliseconds connection_timeout = std::chrono::seconds(30);
    std::chrono::milliseconds read_timeout = std::chrono::seconds(60);
    size_t max_retries = 3;
    std::chrono::milliseconds retry_delay = std::chrono::milliseconds(1000);
    bool validate_partial_responses = true;

    nlohmann::json to_json() const {
        return {
            {"enable_streaming", enable_streaming},
            {"max_buffer_size", max_buffer_size},
            {"connection_timeout_ms", connection_timeout.count()},
            {"read_timeout_ms", read_timeout.count()},
            {"max_retries", max_retries},
            {"retry_delay_ms", retry_delay.count()},
            {"validate_partial_responses", validate_partial_responses}
        };
    }
};

/**
 * @brief SSE Parser for Server-Sent Events
 */
class SSEParser {
public:
    SSEParser(StructuredLogger* logger);

    /**
     * @brief Parse SSE data chunk
     * @param data Raw SSE data
     * @return Parsed streaming events
     */
    std::vector<StreamingEvent> parse_chunk(const std::string& data);

    /**
     * @brief Check if data contains complete SSE event
     * @param data Data to check
     * @return True if complete event
     */
    bool has_complete_event(const std::string& data) const;

    /**
     * @brief Extract complete events from buffer
     * @param buffer Data buffer
     * @return Vector of complete events
     */
    std::vector<std::string> extract_events(std::string& buffer);

private:
    StructuredLogger* logger_;
    std::string event_buffer_;

    /**
     * @brief Parse individual SSE line
     * @param line SSE line
     * @return Parsed event data
     */
    std::unordered_map<std::string, std::string> parse_sse_line(const std::string& line) const;
};

/**
 * @brief Streaming response accumulator
 */
class StreamingAccumulator {
public:
    StreamingAccumulator(StructuredLogger* logger);

    /**
     * @brief Add streaming event to accumulation
     * @param event Streaming event
     */
    void add_event(const StreamingEvent& event);

    /**
     * @brief Get accumulated content
     * @return Complete accumulated content
     */
    std::string get_accumulated_content() const;

    /**
     * @brief Get accumulated metadata
     * @return Accumulated metadata
     */
    nlohmann::json get_accumulated_metadata() const;

    /**
     * @brief Validate accumulated response
     * @return True if valid
     */
    bool validate_accumulation() const;

    /**
     * @brief Reset accumulator
     */
    void reset();

    /**
     * @brief Get current token count
     * @return Number of tokens processed
     */
    size_t get_token_count() const { return token_count_; }

private:
    StructuredLogger* logger_;
    std::string accumulated_content_;
    nlohmann::json accumulated_metadata_;
    size_t token_count_;
    bool has_completion_;

    /**
     * @brief Extract content from token event
     * @param event Streaming event
     * @return Content string
     */
    std::string extract_token_content(const StreamingEvent& event) const;
};

/**
 * @brief Streaming session management
 */
class StreamingSession {
public:
    StreamingSession(const std::string& session_id,
                     StructuredLogger* logger,
                     ErrorHandler* error_handler);

    ~StreamingSession();

    /**
     * @brief Start streaming session
     * @param streaming_callback Token callback
     * @param completion_callback Completion callback
     * @param error_callback Error callback
     */
    void start(StreamingCallback streaming_callback,
               CompletionCallback completion_callback,
               ErrorCallback error_callback);

    /**
     * @brief Process streaming data
     * @param data Raw streaming data
     */
    void process_data(const std::string& data);

    /**
     * @brief Complete streaming session
     * @param final_response Final complete response
     */
    void complete(const nlohmann::json& final_response);

    /**
     * @brief Fail streaming session with error
     * @param error Error message
     */
    void fail(const std::string& error);

    /**
     * @brief Check if session is active
     * @return True if active
     */
    bool is_active() const { return active_.load(); }

    /**
     * @brief Get session ID
     * @return Session ID
     */
    const std::string& get_session_id() const { return session_id_; }

    /**
     * @brief Get accumulated response
     * @return Accumulated response data
     */
    nlohmann::json get_accumulated_response() const;

private:
    std::string session_id_;
    StructuredLogger* logger_;
    ErrorHandler* error_handler_;  // For future error reporting enhancements

    std::atomic<bool> active_;
    std::mutex session_mutex_;
    std::condition_variable session_cv_;

    StreamingCallback streaming_callback_;
    CompletionCallback completion_callback_;
    ErrorCallback error_callback_;

    SSEParser sse_parser_;
    StreamingAccumulator accumulator_;

    nlohmann::json final_response_;
    std::string error_message_;
    bool completed_;
    bool failed_;

    /**
     * @brief Process parsed streaming events
     * @param events Parsed events
     */
    void process_events(const std::vector<StreamingEvent>& events);
};

/**
 * @brief Streaming response handler interface
 */
class StreamingResponseHandler {
public:
    StreamingResponseHandler(std::shared_ptr<ConfigurationManager> config,
                             StructuredLogger* logger,
                             ErrorHandler* error_handler);

    /**
     * @brief Create new streaming session
     * @param session_id Unique session ID
     * @return Streaming session instance
     */
    std::shared_ptr<StreamingSession> create_session(const std::string& session_id);

    /**
     * @brief Get active streaming session
     * @param session_id Session ID
     * @return Streaming session or nullptr if not found
     */
    std::shared_ptr<StreamingSession> get_session(const std::string& session_id);

    /**
     * @brief Remove completed streaming session
     * @param session_id Session ID
     */
    void remove_session(const std::string& session_id);

    /**
     * @brief Get streaming configuration
     * @return Current configuration
     */
    const StreamingConfig& get_config() const { return config_; }

    /**
     * @brief Update streaming configuration
     * @param config New configuration
     */
    void update_config(const StreamingConfig& config) { config_ = config; }

    /**
     * @brief Get active session count
     * @return Number of active sessions
     */
    size_t get_active_session_count() const;

    /**
     * @brief Cleanup expired sessions
     */
    void cleanup_expired_sessions();

private:
    std::shared_ptr<ConfigurationManager> config_manager_;
    StructuredLogger* logger_;
    ErrorHandler* error_handler_;

    StreamingConfig config_;
    std::unordered_map<std::string, std::shared_ptr<StreamingSession>> active_sessions_;
    std::mutex sessions_mutex_;

    std::atomic<size_t> total_sessions_created_;
    std::atomic<size_t> total_sessions_completed_;
    std::atomic<size_t> total_sessions_failed_;
};

} // namespace regulens
