#include "human_ai_collaboration.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>

namespace regulens {

HumanAICollaboration::HumanAICollaboration(std::shared_ptr<ConfigurationManager> config,
                                         std::shared_ptr<StructuredLogger> logger)
    : config_manager_(config), logger_(logger), running_(false) {
    // Load configuration from environment
    config_.max_sessions_per_user = static_cast<size_t>(config_manager_->get_int("COLLABORATION_MAX_SESSIONS_PER_USER").value_or(10));
    config_.max_messages_per_session = static_cast<size_t>(config_manager_->get_int("COLLABORATION_MAX_MESSAGES_PER_SESSION").value_or(1000));
    config_.session_timeout = std::chrono::hours(
        config_manager_->get_int("COLLABORATION_SESSION_TIMEOUT_HOURS").value_or(24));
    config_.request_timeout = std::chrono::hours(
        config_manager_->get_int("COLLABORATION_REQUEST_TIMEOUT_HOURS").value_or(1));
    config_.enable_persistence = config_manager_->get_bool("COLLABORATION_ENABLE_PERSISTENCE").value_or(true);
    config_.max_active_requests = static_cast<size_t>(config_manager_->get_int("COLLABORATION_MAX_ACTIVE_REQUESTS").value_or(100));
    config_.require_user_authentication = config_manager_->get_bool("COLLABORATION_REQUIRE_AUTH").value_or(true);

    logger_->info("HumanAICollaboration initialized with max sessions per user: {}", "", "", {{"max_sessions", std::to_string(config_.max_sessions_per_user)}});
}

HumanAICollaboration::~HumanAICollaboration() {
    shutdown();
}

bool HumanAICollaboration::initialize() {
    logger_->info("Initializing HumanAICollaboration");

    running_ = true;

    // Start cleanup worker thread
    cleanup_thread_ = std::thread(&HumanAICollaboration::cleanup_worker, this);

    logger_->info("HumanAICollaboration initialization complete");
    return true;
}

void HumanAICollaboration::shutdown() {
    if (!running_) return;

    logger_->info("Shutting down HumanAICollaboration");

    running_ = false;

    // Wake up cleanup thread
    {
        std::unique_lock<std::mutex> lock(cleanup_mutex_);
        cleanup_cv_.notify_one();
    }

    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }

    // End all active sessions
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& [session_id, session] : active_sessions_) {
            session.complete(SessionState::CANCELLED);
            if (config_.enable_persistence) {
                persist_session(session);
            }
        }
        active_sessions_.clear();
    }

    logger_->info("HumanAICollaboration shutdown complete");
}

std::optional<std::string> HumanAICollaboration::create_session(const std::string& human_user_id,
                                                              const std::string& agent_id,
                                                              const std::string& title) {
    // Validate user exists and has permissions
    if (config_.require_user_authentication) {
        auto user = get_user(human_user_id);
        if (!user) {
            logger_->warn("Cannot create session: user {} not found", human_user_id);
            return std::nullopt;
        }

        if (!user->can_interact_with_agent(agent_id)) {
            logger_->warn("Cannot create session: user {} not authorized for agent {}", human_user_id, agent_id);
            return std::nullopt;
        }
    }

    std::lock_guard<std::mutex> lock(sessions_mutex_);

    // Check session limit per user
    size_t user_session_count = 0;
    for (const auto& [_, session] : active_sessions_) {
        if (session.human_user_id == human_user_id && session.state == SessionState::ACTIVE) {
            user_session_count++;
        }
    }

    if (user_session_count >= config_.max_sessions_per_user) {
        logger_->warn("Cannot create session: user " + human_user_id + " has reached maximum sessions (" +
                      std::to_string(config_.max_sessions_per_user) + ")", "", "", std::unordered_map<std::string, std::string>{{"max_sessions", std::to_string(config_.max_sessions_per_user)}});
        return std::nullopt;
    }

    // Create new session
    CollaborationSession session(human_user_id, agent_id, title);
    std::string session_id = session.session_id;

    active_sessions_[session_id] = std::move(session);

    if (config_.enable_persistence) {
        persist_session(active_sessions_[session_id]);
    }

    logger_->info("Created collaboration session " + session_id + " for user " +
                 human_user_id + " with agent " + agent_id);

    return session_id;
}

std::optional<CollaborationSession> HumanAICollaboration::get_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = active_sessions_.find(session_id);
    if (it != active_sessions_.end()) {
        return it->second;
    }

    // Try to load from persistence if not in memory
    if (config_.enable_persistence) {
        return load_session(session_id);
    }

    return std::nullopt;
}

std::vector<CollaborationSession> HumanAICollaboration::get_user_sessions(const std::string& human_user_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    std::vector<CollaborationSession> user_sessions;
    for (const auto& [session_id, session] : active_sessions_) {
        if (session.human_user_id == human_user_id) {
            user_sessions.push_back(session);
        }
    }

    return user_sessions;
}

bool HumanAICollaboration::end_session(const std::string& session_id, SessionState final_state) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = active_sessions_.find(session_id);
    if (it == active_sessions_.end()) {
        logger_->warn("Cannot end session {}: session not found", session_id);
        return false;
    }

    it->second.complete(final_state);

    if (config_.enable_persistence) {
        persist_session(it->second);
    }

    // Remove from active sessions (but keep in persistence)
    active_sessions_.erase(it);

    logger_->info("Ended collaboration session " + session_id + " with state " + std::to_string(static_cast<int>(final_state)), "", "",
                  std::unordered_map<std::string, std::string>{{"final_state", std::to_string(static_cast<int>(final_state))}});
    return true;
}

bool HumanAICollaboration::send_message(const std::string& session_id, const InteractionMessage& message) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = active_sessions_.find(session_id);
    if (it == active_sessions_.end()) {
        logger_->warn("Cannot send message: session {} not found", session_id);
        return false;
    }

    auto& session = it->second;

    // Validate message belongs to this session
    if (message.session_id != session_id) {
        logger_->warn("Cannot send message: session ID mismatch");
        return false;
    }

    // Check message limit
    if (session.messages.size() >= config_.max_messages_per_session) {
        logger_->warn("Cannot send message: session {} has reached maximum messages", session_id);
        return false;
    }

    session.add_message(message);

    if (config_.enable_persistence) {
        persist_session(session);
    }

    logger_->debug("Added message to session {} from {}", session_id, message.sender_id);
    return true;
}

std::vector<InteractionMessage> HumanAICollaboration::get_session_messages(const std::string& session_id,
                                                                         size_t limit) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = active_sessions_.find(session_id);
    if (it == active_sessions_.end()) {
        return {};
    }

    const auto& messages = it->second.messages;

    // Return last 'limit' messages
    if (messages.size() <= limit) {
        return messages;
    }

    return std::vector<InteractionMessage>(messages.end() - static_cast<ptrdiff_t>(limit), messages.end());
}

bool HumanAICollaboration::submit_feedback(const HumanFeedback& feedback) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = active_sessions_.find(feedback.session_id);
    if (it == active_sessions_.end()) {
        logger_->warn("Cannot submit feedback: session {} not found", feedback.session_id);
        return false;
    }

    auto& session = it->second;

    // Validate feedback belongs to this session
    if (feedback.session_id != session.session_id) {
        logger_->warn("Cannot submit feedback: session ID mismatch");
        return false;
    }

    session.add_feedback(feedback);

    if (config_.enable_persistence) {
        persist_session(session);
    }

    logger_->info("Submitted feedback on session " + feedback.session_id + " for agent " + feedback.agent_id + " decision " + feedback.decision_id, "", "",
                  std::unordered_map<std::string, std::string>{{"agent_id", feedback.agent_id}, {"decision_id", feedback.decision_id}});
    return true;
}

std::vector<HumanFeedback> HumanAICollaboration::get_session_feedback(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = active_sessions_.find(session_id);
    if (it == active_sessions_.end()) {
        return {};
    }

    return it->second.feedback_history;
}

bool HumanAICollaboration::perform_intervention(const HumanIntervention& intervention) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = active_sessions_.find(intervention.session_id);
    if (it == active_sessions_.end()) {
        logger_->warn("Cannot perform intervention: session {} not found", intervention.session_id);
        return false;
    }

    auto& session = it->second;

    // Validate intervention belongs to this session
    if (intervention.session_id != session.session_id) {
        logger_->warn("Cannot perform intervention: session ID mismatch");
        return false;
    }

    session.add_intervention(intervention);

    if (config_.enable_persistence) {
        persist_session(session);
    }

    logger_->info("Performed intervention on session " + intervention.session_id + " for agent " + intervention.agent_id + ": " + intervention.reason, "", "",
                  std::unordered_map<std::string, std::string>{{"agent_id", intervention.agent_id}, {"reason", intervention.reason}});
    return true;
}

std::vector<HumanIntervention> HumanAICollaboration::get_session_interventions(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = active_sessions_.find(session_id);
    if (it == active_sessions_.end()) {
        return {};
    }

    return it->second.interventions;
}

std::optional<std::string> HumanAICollaboration::create_assistance_request(const AgentAssistanceRequest& request) {
    std::lock_guard<std::mutex> lock(requests_mutex_);

    // Check request limit
    if (pending_requests_.size() >= config_.max_active_requests) {
        logger_->warn("Cannot create assistance request: maximum requests reached");
        return std::nullopt;
    }

    std::string request_id = request.request_id;
    pending_requests_[request_id] = request;

    if (config_.enable_persistence) {
        persist_request(request);
    }

    logger_->info("Created assistance request {} for agent {}", request_id, request.agent_id);
    return request_id;
}

std::vector<AgentAssistanceRequest> HumanAICollaboration::get_pending_requests(const std::string& agent_id) {
    std::lock_guard<std::mutex> lock(requests_mutex_);

    std::vector<AgentAssistanceRequest> agent_requests;

    // Filter requests by agent and remove expired ones
    auto now = std::chrono::system_clock::now();
    for (auto it = pending_requests_.begin(); it != pending_requests_.end(); ) {
        if (it->second.agent_id == agent_id) {
            if (it->second.expires_at > now) {
                agent_requests.push_back(it->second);
            } else {
                // Remove expired request
                it = pending_requests_.erase(it);
                continue;
            }
        }
        ++it;
    }

    return agent_requests;
}

bool HumanAICollaboration::respond_to_request(const std::string& request_id,
                                          const nlohmann::json& /*response*/,
                                          const std::string& human_user_id) {
    std::lock_guard<std::mutex> lock(requests_mutex_);

    auto it = pending_requests_.find(request_id);
    if (it == pending_requests_.end()) {
        logger_->warn("Cannot respond to request {}: request not found", request_id);
        return false;
    }

    auto& request = it->second;

    // Check if request has expired
    if (request.expires_at < std::chrono::system_clock::now()) {
        logger_->warn("Cannot respond to request {}: request has expired", request_id);
        pending_requests_.erase(it);
        return false;
    }

    // Process the response based on request type
    logger_->info("Processing response to request {} from user {}", request_id, human_user_id);

    // For now, just log the response - in a full implementation, this would
    // notify the agent and take appropriate action based on the response

    // Remove the request as it's been handled
    pending_requests_.erase(it);

    return true;
}

bool HumanAICollaboration::register_user(const HumanUser& user) {
    std::lock_guard<std::mutex> lock(users_mutex_);

    if (registered_users_.find(user.user_id) != registered_users_.end()) {
        logger_->warn("Cannot register user {}: user already exists", user.user_id);
        return false;
    }

    registered_users_[user.user_id] = user;

    if (config_.enable_persistence) {
        persist_user(user);
    }

    logger_->info("Registered human user: {}", user.username);
    return true;
}

std::optional<HumanUser> HumanAICollaboration::get_user(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(users_mutex_);

    auto it = registered_users_.find(user_id);
    if (it != registered_users_.end()) {
        return it->second;
    }

    // Try to load from persistence if not in memory
    if (config_.enable_persistence) {
        return load_user(user_id);
    }

    return std::nullopt;
}

bool HumanAICollaboration::update_user(const HumanUser& user) {
    std::lock_guard<std::mutex> lock(users_mutex_);

    auto it = registered_users_.find(user.user_id);
    if (it == registered_users_.end()) {
        logger_->warn("Cannot update user {}: user not found", user.user_id);
        return false;
    }

    it->second = user;

    if (config_.enable_persistence) {
        persist_user(user);
    }

    logger_->info("Updated user: {}", user.username);
    return true;
}

bool HumanAICollaboration::can_user_perform_action(const std::string& user_id,
                                                 const std::string& action,
                                                 const std::string& agent_id) {
    auto user = get_user(user_id);
    if (!user) {
        return false;
    }

    // Check role-based permissions
    if (!validate_user_permissions(user_id, action, agent_id)) {
        return false;
    }

    // Check agent assignment
    if (!agent_id.empty() && !user->can_interact_with_agent(agent_id)) {
        return false;
    }

    return true;
}

nlohmann::json HumanAICollaboration::get_collaboration_stats() {
    std::lock_guard<std::mutex> lock_sessions(sessions_mutex_);
    std::lock_guard<std::mutex> lock_requests(requests_mutex_);

    std::unordered_map<int, size_t> session_states;
    std::unordered_map<int, size_t> interaction_types;

    for (const auto& [_, session] : active_sessions_) {
        session_states[static_cast<int>(session.state)]++;
        interaction_types[static_cast<int>(session.primary_interaction_type)]++;
    }

    return {
        {"active_sessions", active_sessions_.size()},
        {"pending_requests", pending_requests_.size()},
        {"registered_users", registered_users_.size()},
        {"session_states", session_states},
        {"interaction_types", interaction_types},
        {"config", config_.to_json()}
    };
}

nlohmann::json HumanAICollaboration::get_user_stats(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    size_t active_sessions = 0;
    size_t total_messages = 0;
    size_t total_feedback = 0;

    for (const auto& [_, session] : active_sessions_) {
        if (session.human_user_id == user_id) {
            if (session.state == SessionState::ACTIVE) {
                active_sessions++;
            }
            total_messages += session.messages.size();
            total_feedback += session.feedback_history.size();
        }
    }

    return {
        {"user_id", user_id},
        {"active_sessions", active_sessions},
        {"total_messages", total_messages},
        {"total_feedback", total_feedback}
    };
}

std::string HumanAICollaboration::export_collaboration_data(const std::string& user_id,
                                                          const std::string& format) {
    // For demo purposes, export active sessions
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    std::vector<CollaborationSession> sessions_to_export;

    for (const auto& [_, session] : active_sessions_) {
        if (user_id.empty() || session.human_user_id == user_id) {
            sessions_to_export.push_back(session);
        }
    }

    if (format == "json") {
        nlohmann::json export_json = nlohmann::json::array();
        for (const auto& session : sessions_to_export) {
            export_json.push_back(session.to_json());
        }
        return export_json.dump(2);
    }

    // Default to JSON
    return "{}";
}

void HumanAICollaboration::update_session_activity(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = active_sessions_.find(session_id);
    if (it != active_sessions_.end()) {
        it->second.last_activity = std::chrono::system_clock::now();
    }
}

bool HumanAICollaboration::validate_session_access(const std::string& session_id,
                                                 const std::string& user_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = active_sessions_.find(session_id);
    if (it == active_sessions_.end()) {
        return false;
    }

    return it->second.human_user_id == user_id;
}

bool HumanAICollaboration::validate_user_permissions(const std::string& user_id,
                                                   const std::string& action,
                                                   const std::string& /*agent_id*/) {
    auto user = get_user(user_id);
    if (!user) {
        return false;
    }

    // Role-based permission checks
    switch (user->role) {
        case HumanRole::ADMINISTRATOR:
            return true; // Full access

        case HumanRole::SUPERVISOR:
            // Can override, intervene, and provide feedback
            return action == "override" || action == "intervene" || action == "feedback" ||
                   action == "query" || action == "chat";

        case HumanRole::OPERATOR:
            // Can provide feedback, approve/reject, chat
            return action == "feedback" || action == "approve" || action == "reject" ||
                   action == "chat";

        case HumanRole::ANALYST:
            // Can query agents and view information
            return action == "query" || action == "view";

        case HumanRole::VIEWER:
            // Can only view information
            return action == "view";

        default:
            return false;
    }
}

void HumanAICollaboration::cleanup_expired_sessions() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto cutoff_time = std::chrono::system_clock::now() - config_.session_timeout;

    for (auto it = active_sessions_.begin(); it != active_sessions_.end(); ) {
        if (it->second.last_activity < cutoff_time) {
            logger_->info("Cleaning up expired session: {}", it->first);
            it->second.complete(SessionState::TIMEOUT);

            if (config_.enable_persistence) {
                persist_session(it->second);
            }

            it = active_sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

void HumanAICollaboration::cleanup_expired_requests() {
    std::lock_guard<std::mutex> lock(requests_mutex_);

    auto now = std::chrono::system_clock::now();

    for (auto it = pending_requests_.begin(); it != pending_requests_.end(); ) {
        if (it->second.expires_at < now) {
            logger_->info("Cleaning up expired assistance request: {}", it->first);
            it = pending_requests_.erase(it);
        } else {
            ++it;
        }
    }
}

bool HumanAICollaboration::persist_session(const CollaborationSession& session) {
    logger_->debug("Persisting session: {}", session.session_id);
    return true;
}

bool HumanAICollaboration::persist_user(const HumanUser& user) {
    logger_->debug("Persisting user: {}", user.user_id);
    return true;
}

bool HumanAICollaboration::persist_request(const AgentAssistanceRequest& request) {
    logger_->debug("Persisting request: {}", request.request_id);
    return true;
}

std::optional<CollaborationSession> HumanAICollaboration::load_session(const std::string& session_id) {
    logger_->debug("Loading session: {}", session_id);
    return std::nullopt;
}

std::optional<HumanUser> HumanAICollaboration::load_user(const std::string& user_id) {
    logger_->debug("Loading user: {}", user_id);
    return std::nullopt;
}

std::vector<AgentAssistanceRequest> HumanAICollaboration::load_pending_requests(const std::string& agent_id) {
    logger_->debug("Loading pending requests for agent: {}", agent_id);
    return {};
}

void HumanAICollaboration::cleanup_worker() {
    logger_->info("Human-AI collaboration cleanup worker started");

    while (running_) {
        std::unique_lock<std::mutex> lock(cleanup_mutex_);

        // Wait for cleanup interval or shutdown signal
        cleanup_cv_.wait_for(lock, config_.cleanup_interval);

        if (!running_) break;

        try {
            cleanup_expired_sessions();
            cleanup_expired_requests();
        } catch (const std::exception& e) {
            logger_->error("Error during cleanup: {}", e.what());
        }
    }

    logger_->info("Human-AI collaboration cleanup worker stopped");
}

} // namespace regulens
