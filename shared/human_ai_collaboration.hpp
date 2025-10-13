#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>

#include "models/human_ai_interaction.hpp"
#include "logging/structured_logger.hpp"
#include <condition_variable>
#include "config/configuration_manager.hpp"

namespace regulens {

/**
 * @brief Human-AI collaboration system
 *
 * Manages interactive collaboration between human users and AI agents,
 * including chat interfaces, feedback collection, decision overrides,
 * and collaborative decision-making sessions.
 */
class HumanAICollaboration {
public:
    HumanAICollaboration(std::shared_ptr<ConfigurationManager> config,
                        std::shared_ptr<StructuredLogger> logger);

    ~HumanAICollaboration();

    /**
     * @brief Initialize the collaboration system
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown the collaboration system
     */
    void shutdown();

    // Session Management
    /**
     * @brief Create a new collaboration session
     * @param human_user_id Human user ID
     * @param agent_id Agent ID
     * @param title Session title
     * @return Session ID if created successfully
     */
    std::optional<std::string> create_session(const std::string& human_user_id,
                                            const std::string& agent_id,
                                            const std::string& title = "");

    /**
     * @brief Get collaboration session by ID
     * @param session_id Session ID
     * @return Session if found
     */
    std::optional<CollaborationSession> get_session(const std::string& session_id);

    /**
     * @brief List active sessions for a user
     * @param human_user_id User ID
     * @return Vector of active sessions
     */
    std::vector<CollaborationSession> get_user_sessions(const std::string& human_user_id);

    /**
     * @brief End a collaboration session
     * @param session_id Session ID
     * @param final_state Final session state
     * @return true if session ended successfully
     */
    bool end_session(const std::string& session_id,
                    SessionState final_state = SessionState::COMPLETED);

    // Message Handling
    /**
     * @brief Send message in a collaboration session
     * @param session_id Session ID
     * @param message Message to send
     * @return true if message sent successfully
     */
    bool send_message(const std::string& session_id, const InteractionMessage& message);

    /**
     * @brief Get messages for a session
     * @param session_id Session ID
     * @param limit Maximum number of messages to return
     * @return Vector of messages
     */
    std::vector<InteractionMessage> get_session_messages(const std::string& session_id,
                                                        size_t limit = 50);

    // Feedback Management
    /**
     * @brief Submit feedback on an agent decision
     * @param feedback Feedback to submit
     * @return true if feedback submitted successfully
     */
    bool submit_feedback(const HumanFeedback& feedback);

    /**
     * @brief Get feedback history for a session
     * @param session_id Session ID
     * @return Vector of feedback items
     */
    std::vector<HumanFeedback> get_session_feedback(const std::string& session_id);

    // Intervention Management
    /**
     * @brief Perform human intervention on an agent
     * @param intervention Intervention to perform
     * @return true if intervention performed successfully
     */
    bool perform_intervention(const HumanIntervention& intervention);

    /**
     * @brief Get intervention history for a session
     * @param session_id Session ID
     * @return Vector of interventions
     */
    std::vector<HumanIntervention> get_session_interventions(const std::string& session_id);

    // Agent Assistance Requests
    /**
     * @brief Create an agent assistance request
     * @param request Request details
     * @return Request ID if created successfully
     */
    std::optional<std::string> create_assistance_request(const AgentAssistanceRequest& request);

    /**
     * @brief Get pending assistance requests for an agent
     * @param agent_id Agent ID
     * @return Vector of pending requests
     */
    std::vector<AgentAssistanceRequest> get_pending_requests(const std::string& agent_id);

    /**
     * @brief Respond to an assistance request
     * @param request_id Request ID
     * @param response Response details (approval, rejection, clarification, etc.)
     * @param human_user_id User providing response
     * @return true if response processed successfully
     */
    bool respond_to_request(const std::string& request_id,
                          const nlohmann::json& response,
                          const std::string& human_user_id);

    // User Management
    /**
     * @brief Register a human user
     * @param user User details
     * @return true if user registered successfully
     */
    bool register_user(const HumanUser& user);

    /**
     * @brief Get user by ID
     * @param user_id User ID
     * @return User if found
     */
    std::optional<HumanUser> get_user(const std::string& user_id);

    /**
     * @brief Update user information
     * @param user Updated user details
     * @return true if user updated successfully
     */
    bool update_user(const HumanUser& user);

    /**
     * @brief Check if user can perform an action
     * @param user_id User ID
     * @param action Action to check
     * @param agent_id Target agent ID (if applicable)
     * @return true if action is permitted
     */
    bool can_user_perform_action(const std::string& user_id,
                               const std::string& action,
                               const std::string& agent_id = "");

    // Analytics and Statistics
    /**
     * @brief Get collaboration statistics
     * @return JSON with system statistics
     */
    nlohmann::json get_collaboration_stats();

    /**
     * @brief Get user activity statistics
     * @param user_id User ID
     * @return JSON with user statistics
     */
    nlohmann::json get_user_stats(const std::string& user_id);

    /**
     * @brief Export collaboration data
     * @param user_id User ID (optional filter)
     * @param format Export format ("json", "csv")
     * @return Exported data as string
     */
    std::string export_collaboration_data(const std::string& user_id = "",
                                        const std::string& format = "json");

private:
    std::shared_ptr<ConfigurationManager> config_manager_;
    std::shared_ptr<StructuredLogger> logger_;

    CollaborationConfig config_;
    
    // Optional integrations for production features
    void* agent_orchestrator_;  // AgentOrchestrator* - forward declaration issue, using void*
    void* db_connection_;        // DatabaseConnection* - forward declaration issue, using void*
    void* metrics_;              // MetricsCollector* - forward declaration issue, using void*

    // Thread-safe storage
    std::mutex sessions_mutex_;
    std::unordered_map<std::string, CollaborationSession> active_sessions_;

    std::mutex users_mutex_;
    std::unordered_map<std::string, HumanUser> registered_users_;

    std::mutex requests_mutex_;
    std::unordered_map<std::string, AgentAssistanceRequest> pending_requests_;

    // Background processing
    std::thread cleanup_thread_;
    std::atomic<bool> running_;
    std::mutex cleanup_mutex_;
    std::condition_variable cleanup_cv_;

    // Helper methods
    void update_session_activity(const std::string& session_id);
    bool validate_session_access(const std::string& session_id, const std::string& user_id);
    bool validate_user_permissions(const std::string& user_id, const std::string& action,
                                 const std::string& agent_id = "");
    void cleanup_expired_sessions();
    void cleanup_expired_requests();
    void cleanup_worker();

    // Persistence methods (when enabled)
    bool persist_session(const CollaborationSession& session);
    bool persist_user(const HumanUser& user);
    bool persist_request(const AgentAssistanceRequest& request);

    std::optional<CollaborationSession> load_session(const std::string& session_id);
    std::optional<HumanUser> load_user(const std::string& user_id);
    std::vector<AgentAssistanceRequest> load_pending_requests(const std::string& agent_id);
};


} // namespace regulens
