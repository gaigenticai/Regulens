/**
 * Streaming Response Handler Implementation
 *
 * Production-grade implementation of streaming response processing
 * for real-time LLM interactions with comprehensive error handling.
 */

#include "streaming_handler.hpp"
#include <algorithm>
#include <sstream>
#include <regex>

namespace regulens {

// SSEParser Implementation

SSEParser::SSEParser(StructuredLogger* logger) : logger_(logger) {}

std::vector<StreamingEvent> SSEParser::parse_chunk(const std::string& data) {
    event_buffer_ += data;
    std::vector<StreamingEvent> events;

    auto complete_events = extract_events(event_buffer_);
    for (const auto& event_data : complete_events) {
        try {
            // Parse SSE event
            std::istringstream iss(event_data);
            std::string line;
            std::string event_type = "message";  // Default SSE event type
            std::string event_data_content;
            std::unordered_map<std::string, std::string> metadata;

            while (std::getline(iss, line)) {
                // Remove trailing \r if present
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                if (line.empty()) continue;

                auto parsed = parse_sse_line(line);
                for (const auto& [key, value] : parsed) {
                    if (key == "event") {
                        event_type = value;
                    } else if (key == "data") {
                        if (!event_data_content.empty()) {
                            event_data_content += "\n";
                        }
                        event_data_content += value;
                    } else {
                        metadata[key] = value;
                    }
                }
            }

            // Create streaming event based on event type
            StreamingEventType streaming_type;
            if (event_type == "completion" || event_type == "done") {
                streaming_type = StreamingEventType::COMPLETION;
            } else if (event_type == "error") {
                streaming_type = StreamingEventType::ERROR;
            } else if (!event_data_content.empty()) {
                streaming_type = StreamingEventType::TOKEN;
            } else {
                continue;  // Skip empty events
            }

            events.emplace_back(streaming_type, event_data_content, metadata);

        } catch (const std::exception& e) {
            logger_->error("Failed to parse SSE event: " + std::string(e.what()),
                          "StreamingHandler", "parse_chunk");
            // Continue processing other events
        }
    }

    return events;
}

bool SSEParser::has_complete_event(const std::string& data) const {
    return data.find("\n\n") != std::string::npos;
}

std::vector<std::string> SSEParser::extract_events(std::string& buffer) {
    std::vector<std::string> events;
    size_t pos = 0;

    while ((pos = buffer.find("\n\n", pos)) != std::string::npos) {
        std::string event = buffer.substr(0, pos);
        if (!event.empty()) {
            events.push_back(event);
        }
        buffer.erase(0, pos + 2);  // Remove event + \n\n
        pos = 0;
    }

    return events;
}

std::unordered_map<std::string, std::string> SSEParser::parse_sse_line(const std::string& line) const {
    std::unordered_map<std::string, std::string> result;

    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
        std::string field = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);

        // Remove leading space if present
        if (!value.empty() && value[0] == ' ') {
            value = value.substr(1);
        }

        result[field] = value;
    }

    return result;
}

// StreamingAccumulator Implementation

StreamingAccumulator::StreamingAccumulator(StructuredLogger* logger)
    : logger_(logger), token_count_(0), has_completion_(false) {}

void StreamingAccumulator::add_event(const StreamingEvent& event) {
    switch (event.type) {
        case StreamingEventType::TOKEN: {
            std::string content = extract_token_content(event);
            if (!content.empty()) {
                accumulated_content_ += content;
                token_count_++;
            }
            break;
        }
        case StreamingEventType::COMPLETION:
            has_completion_ = true;
            // Extract any final metadata
            if (!event.metadata.empty()) {
                for (const auto& [key, value] : event.metadata) {
                    accumulated_metadata_[key] = value;
                }
            }
            break;
        case StreamingEventType::ERROR:
            logger_->error("Streaming error received: " + event.data,
                          "StreamingAccumulator", "add_event");
            break;
        default:
            break;
    }
}

std::string StreamingAccumulator::get_accumulated_content() const {
    return accumulated_content_;
}

nlohmann::json StreamingAccumulator::get_accumulated_metadata() const {
    return accumulated_metadata_;
}

bool StreamingAccumulator::validate_accumulation() const {
    // Basic validation - ensure we have content and completion
    return has_completion_ && !accumulated_content_.empty();
}

void StreamingAccumulator::reset() {
    accumulated_content_.clear();
    accumulated_metadata_ = nlohmann::json::object();
    token_count_ = 0;
    has_completion_ = false;
}

std::string StreamingAccumulator::extract_token_content(const StreamingEvent& event) const {
    try {
        // Try to parse as JSON and extract content
        nlohmann::json data = nlohmann::json::parse(event.data);

        // OpenAI format
        if (data.contains("choices") && !data["choices"].empty()) {
            const auto& choice = data["choices"][0];
            if (choice.contains("delta") && choice["delta"].contains("content")) {
                return choice["delta"]["content"];
            }
        }

        // Anthropic format
        if (data.contains("type") && data["type"] == "content_block_delta") {
            if (data.contains("delta") && data["delta"].contains("text")) {
                return data["delta"]["text"];
            }
        }

    } catch (const std::exception& e) {
        logger_->debug("Failed to extract token content from JSON: " + std::string(e.what()),
                      "StreamingAccumulator", "extract_token_content");
    }

    return "";
}

// StreamingSession Implementation

StreamingSession::StreamingSession(const std::string& session_id,
                                   StructuredLogger* logger,
                                   ErrorHandler* error_handler)
    : session_id_(session_id), logger_(logger), error_handler_(error_handler),
      active_(false), sse_parser_(logger), accumulator_(logger),
      completed_(false), failed_(false) {}

StreamingSession::~StreamingSession() {
    if (active_.load()) {
        fail("Session destroyed while active");
    }
}

void StreamingSession::start(StreamingCallback streaming_callback,
                             CompletionCallback completion_callback,
                             ErrorCallback error_callback) {
    std::lock_guard<std::mutex> lock(session_mutex_);

    streaming_callback_ = std::move(streaming_callback);
    completion_callback_ = std::move(completion_callback);
    error_callback_ = std::move(error_callback);

    active_.store(true);
    completed_ = false;
    failed_ = false;
    accumulator_.reset();

    logger_->info("Streaming session started: " + session_id_,
                  "StreamingSession", "start");
}

void StreamingSession::process_data(const std::string& data) {
    if (!active_.load()) {
        return;
    }

    try {
        auto events = sse_parser_.parse_chunk(data);
        process_events(events);
    } catch (const std::exception& e) {
        fail("Data processing error: " + std::string(e.what()));
    }
}

void StreamingSession::complete(const nlohmann::json& final_response) {
    std::lock_guard<std::mutex> lock(session_mutex_);

    if (!active_.load()) {
        return;
    }

    active_.store(false);
    completed_ = true;
    final_response_ = final_response;

    // Call completion callback
    if (completion_callback_) {
        try {
            completion_callback_(final_response);
        } catch (const std::exception& e) {
            logger_->error("Completion callback error: " + std::string(e.what()),
                          "StreamingSession", "complete");
        }
    }

    logger_->info("Streaming session completed: " + session_id_,
                  "StreamingSession", "complete");
}

void StreamingSession::fail(const std::string& error) {
    std::lock_guard<std::mutex> lock(session_mutex_);

    if (!active_.load()) {
        return;
    }

    active_.store(false);
    failed_ = true;
    error_message_ = error;

    // Call error callback
    if (error_callback_) {
        try {
            error_callback_(error);
        } catch (const std::exception& e) {
            logger_->error("Error callback error: " + std::string(e.what()),
                          "StreamingSession", "fail");
        }
    }

    logger_->error("Streaming session failed: " + session_id_ + " - " + error,
                   "StreamingSession", "fail");
}

nlohmann::json StreamingSession::get_accumulated_response() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(session_mutex_));

    nlohmann::json response = final_response_;

    if (response.is_null()) {
        response = {
            {"content", accumulator_.get_accumulated_content()},
            {"metadata", accumulator_.get_accumulated_metadata()},
            {"token_count", accumulator_.get_token_count()},
            {"session_id", session_id_},
            {"completed", completed_},
            {"failed", failed_}
        };

        if (failed_) {
            response["error"] = error_message_;
        }
    }

    return response;
}

void StreamingSession::process_events(const std::vector<StreamingEvent>& events) {
    for (const auto& event : events) {
        accumulator_.add_event(event);

        // Call streaming callback for token events
        if (event.type == StreamingEventType::TOKEN && streaming_callback_) {
            try {
                streaming_callback_(event);
            } catch (const std::exception& e) {
                logger_->error("Streaming callback error: " + std::string(e.what()),
                              "StreamingSession", "process_events");
            }
        }

        // Handle completion
        if (event.type == StreamingEventType::COMPLETION) {
            nlohmann::json final_response = get_accumulated_response();
            complete(final_response);
            break;
        }

        // Handle errors
        if (event.type == StreamingEventType::ERROR) {
            fail("Streaming error: " + event.data);
            break;
        }
    }
}

// StreamingResponseHandler Implementation

StreamingResponseHandler::StreamingResponseHandler(std::shared_ptr<ConfigurationManager> config,
                                                   StructuredLogger* logger,
                                                   ErrorHandler* error_handler)
    : config_manager_(config), logger_(logger), error_handler_(error_handler),
      total_sessions_created_(0), total_sessions_completed_(0), total_sessions_failed_(0) {
    // Suppress unused variable warning - for future error reporting enhancements
    (void)error_handler_;
}

std::shared_ptr<StreamingSession> StreamingResponseHandler::create_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    if (active_sessions_.find(session_id) != active_sessions_.end()) {
        logger_->warn("Session already exists: " + session_id,
                      "StreamingResponseHandler", "create_session");
        return nullptr;
    }

    auto session = std::make_shared<StreamingSession>(session_id, logger_, error_handler_);
    active_sessions_[session_id] = session;
    total_sessions_created_++;

    logger_->info("Created streaming session: " + session_id,
                  "StreamingResponseHandler", "create_session");

    return session;
}

std::shared_ptr<StreamingSession> StreamingResponseHandler::get_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = active_sessions_.find(session_id);
    if (it != active_sessions_.end()) {
        return it->second;
    }

    return nullptr;
}

void StreamingResponseHandler::remove_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = active_sessions_.find(session_id);
    if (it != active_sessions_.end()) {
        auto session = it->second;
        if (session->is_active()) {
            session->fail("Session removed while active");
            total_sessions_failed_++;
        } else {
            total_sessions_completed_++;
        }

        active_sessions_.erase(it);

        logger_->info("Removed streaming session: " + session_id,
                      "StreamingResponseHandler", "remove_session");
    }
}

size_t StreamingResponseHandler::get_active_session_count() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(sessions_mutex_));
    return active_sessions_.size();
}

void StreamingResponseHandler::cleanup_expired_sessions() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    // Implementation for cleanup would go here
    // Production-grade session expiry tracking and cleanup
    auto now = std::chrono::steady_clock::now();
    std::vector<std::string> to_remove;
    
    // Find expired sessions (default: 1 hour timeout)
    const auto SESSION_TIMEOUT = std::chrono::hours(1);
    
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (const auto& [session_id, session] : active_sessions_) {
            auto session_age = now - session->created_at;
            
            if (session_age > SESSION_TIMEOUT) {
                to_remove.push_back(session_id);
                
                // Log expiration for monitoring
                if (logger_) {
                    logger_->warn("Streaming session expired: " + session_id + 
                                 " (age: " + std::to_string(std::chrono::duration_cast<std::chrono::minutes>(session_age).count()) + 
                                 " minutes)",
                                 "StreamingResponseHandler", "cleanup_expired_sessions");
                }
            }
        }
        
        // Remove expired sessions
        size_t removed_count = 0;
        for (const auto& session_id : to_remove) {
            // Close any active streams
            if (active_sessions_[session_id]->stream_active) {
                active_sessions_[session_id]->stream_active = false;
            }
            
            // Persist session metrics before removal
            if (metrics_) {
                metrics_->record_session_duration(session_id, 
                    std::chrono::duration_cast<std::chrono::seconds>(
                        now - active_sessions_[session_id]->created_at
                    ).count());
            }
            
            active_sessions_.erase(session_id);
            removed_count++;
        }
        
        if (removed_count > 0) {
            logger_->info("Cleaned up " + std::to_string(removed_count) + 
                         " expired streaming sessions",
                         "StreamingResponseHandler", "cleanup_expired_sessions");
        }
    }
    
    logger_->debug("Streaming session cleanup completed - " +
                   std::to_string(active_sessions_.size()) + " active sessions",
                   "StreamingResponseHandler", "cleanup_expired_sessions");
}

} // namespace regulens
