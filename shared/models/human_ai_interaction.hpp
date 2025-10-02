#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <nlohmann/json.hpp>
#include <optional>

#include "compliance_event.hpp"
#include "agent_decision.hpp"

namespace regulens {

/**
 * @brief Types of human-AI interactions
 */
enum class InteractionType {
    HUMAN_QUERY,        // Human asking agent for information/advice
    HUMAN_COMMAND,      // Human giving direct command to agent
    HUMAN_FEEDBACK,     // Human providing feedback on agent decision/action
    HUMAN_OVERRIDE,     // Human overriding agent decision
    HUMAN_APPROVAL,     // Human approving agent action
    HUMAN_REJECTION,    // Human rejecting agent action
    AGENT_RESPONSE,     // Agent responding to human interaction
    AGENT_REQUEST,      // Agent requesting human input/approval
    COLLABORATION_SESSION, // Ongoing collaborative session
    INTERVENTION        // Human intervention in agent process
};

/**
 * @brief Human user roles and permissions
 */
enum class HumanRole {
    VIEWER,         // Can view agent activities but not interact
    ANALYST,        // Can query agents and view detailed information
    OPERATOR,       // Can provide feedback and approve/reject actions
    SUPERVISOR,     // Can override decisions and intervene
    ADMINISTRATOR   // Full system control
};

/**
 * @brief Human-AI interaction session states
 */
enum class SessionState {
    ACTIVE,         // Session is currently active
    PENDING,        // Waiting for human or agent response
    COMPLETED,      // Session completed successfully
    CANCELLED,      // Session cancelled by user
    TIMEOUT,        // Session timed out
    ERROR           // Session ended with error
};

/**
 * @brief Human feedback on agent decisions
 */
enum class HumanFeedbackType {
    AGREEMENT,      // Human agrees with decision
    DISAGREEMENT,   // Human disagrees with decision
    PARTIAL_AGREEMENT, // Human partially agrees
    UNCERTAIN,      // Human is uncertain about decision
    REQUEST_CLARIFICATION, // Human needs more information
    SUGGEST_ALTERNATIVE    // Human suggests alternative approach
};

/**
 * @brief Human feedback structure
 */
struct HumanFeedback {
    std::string feedback_id;
    std::string session_id;
    std::string agent_id;
    std::string decision_id;
    HumanFeedbackType feedback_type;
    std::string feedback_text;
    std::unordered_map<std::string, std::string> metadata;
    std::chrono::system_clock::time_point timestamp;

    HumanFeedback(std::string sid, std::string aid, std::string did,
                 HumanFeedbackType type, std::string text = "")
        : feedback_id(generate_feedback_id(sid, aid, did)),
          session_id(std::move(sid)), agent_id(std::move(aid)),
          decision_id(std::move(did)), feedback_type(type),
          feedback_text(std::move(text)), timestamp(std::chrono::system_clock::now()) {}

    nlohmann::json to_json() const {
        nlohmann::json metadata_json;
        for (const auto& [key, value] : metadata) {
            metadata_json[key] = value;
        }

        return {
            {"feedback_id", feedback_id},
            {"session_id", session_id},
            {"agent_id", agent_id},
            {"decision_id", decision_id},
            {"feedback_type", static_cast<int>(feedback_type)},
            {"feedback_text", feedback_text},
            {"metadata", metadata_json},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count()}
        };
    }

private:
    static std::string generate_feedback_id(const std::string& session_id,
                                          const std::string& agent_id,
                                          const std::string& decision_id) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return "feedback_" + session_id + "_" + agent_id + "_" + decision_id + "_" + std::to_string(ms);
    }
};

/**
 * @brief Intervention action types
 */
enum class InterventionAction {
    PAUSE_AGENT,        // Temporarily pause agent
    RESUME_AGENT,       // Resume paused agent
    TERMINATE_TASK,     // Terminate current task
    MODIFY_PARAMETERS,  // Change agent parameters
    TAKE_CONTROL,       // Human takes direct control
    RELEASE_CONTROL,    // Return control to agent
    RESET_AGENT         // Reset agent to initial state
};

/**
 * @brief Human intervention in agent process
 */
struct HumanIntervention {
    std::string intervention_id;
    std::string session_id;
    std::string agent_id;
    std::string reason;
    InterventionAction action;
    std::unordered_map<std::string, std::string> parameters;
    std::chrono::system_clock::time_point timestamp;

    HumanIntervention(std::string sid, std::string aid, std::string rsn,
                     InterventionAction act)
        : intervention_id(generate_intervention_id(sid, aid)),
          session_id(std::move(sid)), agent_id(std::move(aid)),
          reason(std::move(rsn)), action(act),
          timestamp(std::chrono::system_clock::now()) {}

    nlohmann::json to_json() const {
        nlohmann::json params_json;
        for (const auto& [key, value] : parameters) {
            params_json[key] = value;
        }

        return {
            {"intervention_id", intervention_id},
            {"session_id", session_id},
            {"agent_id", agent_id},
            {"reason", reason},
            {"action", static_cast<int>(action)},
            {"parameters", params_json},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count()}
        };
    }

private:
    static std::string generate_intervention_id(const std::string& session_id,
                                              const std::string& agent_id) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return "intervention_" + session_id + "_" + agent_id + "_" + std::to_string(ms);
    }
};

/**
 * @brief Collaboration configuration
 */
struct CollaborationConfig {
    size_t max_sessions_per_user = 10;        // Maximum concurrent sessions per user
    size_t max_messages_per_session = 1000;   // Maximum messages per session
    std::chrono::hours session_timeout = std::chrono::hours(24);  // Session timeout
    std::chrono::hours request_timeout = std::chrono::hours(1);   // Request timeout
    bool enable_persistence = true;           // Whether to persist data
    size_t max_active_requests = 100;         // Maximum pending requests
    bool require_user_authentication = true;  // Whether to require user auth

    nlohmann::json to_json() const {
        return {
            {"max_sessions_per_user", max_sessions_per_user},
            {"max_messages_per_session", max_messages_per_session},
            {"session_timeout_hours", session_timeout.count()},
            {"request_timeout_hours", request_timeout.count()},
            {"enable_persistence", enable_persistence},
            {"max_active_requests", max_active_requests},
            {"require_user_authentication", require_user_authentication}
        };
    }
};

/**
 * @brief Human-AI interaction message
 */
struct InteractionMessage {
    std::string message_id;
    std::string session_id;
    std::string sender_id;  // Human user ID or agent ID
    bool is_from_human;
    std::string message_type; // "text", "command", "decision", "feedback"
    std::string content;
    std::unordered_map<std::string, std::string> metadata;
    std::chrono::system_clock::time_point timestamp;

    InteractionMessage(std::string sid, std::string sender, bool human,
                      std::string type, std::string cnt = "")
        : message_id(generate_message_id(sid, sender)),
          session_id(std::move(sid)), sender_id(std::move(sender)),
          is_from_human(human), message_type(std::move(type)),
          content(std::move(cnt)), timestamp(std::chrono::system_clock::now()) {}

    nlohmann::json to_json() const {
        nlohmann::json metadata_json;
        for (const auto& [key, value] : metadata) {
            metadata_json[key] = value;
        }

        return {
            {"message_id", message_id},
            {"session_id", session_id},
            {"sender_id", sender_id},
            {"is_from_human", is_from_human},
            {"message_type", message_type},
            {"content", content},
            {"metadata", metadata_json},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count()}
        };
    }

private:
    static std::string generate_message_id(const std::string& session_id,
                                         const std::string& sender_id) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return "msg_" + session_id + "_" + sender_id + "_" + std::to_string(ms);
    }
};

/**
 * @brief Human-AI collaboration session
 */
struct CollaborationSession {
    std::string session_id;
    std::string human_user_id;
    std::string agent_id;
    std::string title;
    std::string description;
    SessionState state;
    InteractionType primary_interaction_type;
    std::vector<InteractionMessage> messages;
    std::vector<HumanFeedback> feedback_history;
    std::vector<HumanIntervention> interventions;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_activity;
    std::chrono::system_clock::time_point completed_at;

    CollaborationSession(std::string hid, std::string aid, std::string ttl = "")
        : session_id(generate_session_id(hid, aid)),
          human_user_id(std::move(hid)), agent_id(std::move(aid)),
          title(std::move(ttl)), state(SessionState::ACTIVE),
          primary_interaction_type(InteractionType::HUMAN_QUERY),
          created_at(std::chrono::system_clock::now()),
          last_activity(created_at) {
        if (title.empty()) {
            title = "Collaboration with " + agent_id;
        }
    }

    nlohmann::json to_json() const {
        nlohmann::json messages_json = nlohmann::json::array();
        for (const auto& msg : messages) {
            messages_json.push_back(msg.to_json());
        }

        nlohmann::json feedback_json = nlohmann::json::array();
        for (const auto& fb : feedback_history) {
            feedback_json.push_back(fb.to_json());
        }

        nlohmann::json interventions_json = nlohmann::json::array();
        for (const auto& interv : interventions) {
            interventions_json.push_back(interv.to_json());
        }

        nlohmann::json result = {
            {"session_id", session_id},
            {"human_user_id", human_user_id},
            {"agent_id", agent_id},
            {"title", title},
            {"description", description},
            {"state", static_cast<int>(state)},
            {"primary_interaction_type", static_cast<int>(primary_interaction_type)},
            {"messages", messages_json},
            {"feedback_history", feedback_json},
            {"interventions", interventions_json},
            {"created_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                created_at.time_since_epoch()).count()},
            {"last_activity", std::chrono::duration_cast<std::chrono::milliseconds>(
                last_activity.time_since_epoch()).count()}
        };

        if (completed_at != std::chrono::system_clock::time_point{}) {
            result["completed_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                completed_at.time_since_epoch()).count();
        }

        return result;
    }

    void add_message(const InteractionMessage& message) {
        messages.push_back(message);
        last_activity = std::chrono::system_clock::now();
    }

    void add_feedback(const HumanFeedback& feedback) {
        feedback_history.push_back(feedback);
        last_activity = std::chrono::system_clock::now();
    }

    void add_intervention(const HumanIntervention& intervention) {
        interventions.push_back(intervention);
        last_activity = std::chrono::system_clock::now();
    }

    void complete(SessionState final_state = SessionState::COMPLETED) {
        state = final_state;
        completed_at = std::chrono::system_clock::now();
        last_activity = completed_at;
    }

private:
    static std::string generate_session_id(const std::string& human_id,
                                         const std::string& agent_id) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return "session_" + human_id + "_" + agent_id + "_" + std::to_string(ms);
    }
};

/**
 * @brief Human user profile and permissions
 */
struct HumanUser {
    std::string user_id;
    std::string username;
    std::string display_name;
    HumanRole role;
    std::vector<std::string> permissions;
    std::vector<std::string> assigned_agents;  // Agents this user can interact with
    std::unordered_map<std::string, std::string> preferences;
    std::chrono::system_clock::time_point last_login;
    bool is_active;

    HumanUser(std::string uid, std::string uname, std::string dname, HumanRole r)
        : user_id(std::move(uid)), username(std::move(uname)),
          display_name(std::move(dname)), role(r),
          last_login(std::chrono::system_clock::now()), is_active(true) {}

    bool can_interact_with_agent(const std::string& agent_id) const {
        return assigned_agents.empty() ||
               std::find(assigned_agents.begin(), assigned_agents.end(), agent_id) != assigned_agents.end();
    }

    bool has_permission(const std::string& permission) const {
        return std::find(permissions.begin(), permissions.end(), permission) != permissions.end();
    }

    nlohmann::json to_json() const {
        nlohmann::json prefs_json;
        for (const auto& [key, value] : preferences) {
            prefs_json[key] = value;
        }

        return {
            {"user_id", user_id},
            {"username", username},
            {"display_name", display_name},
            {"role", static_cast<int>(role)},
            {"permissions", permissions},
            {"assigned_agents", assigned_agents},
            {"preferences", prefs_json},
            {"last_login", std::chrono::duration_cast<std::chrono::milliseconds>(
                last_login.time_since_epoch()).count()},
            {"is_active", is_active}
        };
    }
};

/**
 * @brief Agent assistance request
 */
struct AgentAssistanceRequest {
    std::string request_id;
    std::string agent_id;
    std::string request_type; // "approval", "clarification", "help", "override"
    std::string description;
    std::optional<AgentDecision> pending_decision;
    std::unordered_map<std::string, std::string> context;
    std::chrono::system_clock::time_point requested_at;
    std::chrono::system_clock::time_point expires_at;

    AgentAssistanceRequest(std::string aid, std::string rtype, std::string desc)
        : request_id(generate_request_id(aid)),
          agent_id(std::move(aid)), request_type(std::move(rtype)),
          description(std::move(desc)),
          requested_at(std::chrono::system_clock::now()),
          expires_at(requested_at + std::chrono::hours(1)) {} // Default 1 hour expiry

    nlohmann::json to_json() const {
        nlohmann::json context_json;
        for (const auto& [key, value] : context) {
            context_json[key] = value;
        }

        nlohmann::json result = {
            {"request_id", request_id},
            {"agent_id", agent_id},
            {"request_type", request_type},
            {"description", description},
            {"context", context_json},
            {"requested_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                requested_at.time_since_epoch()).count()},
            {"expires_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                expires_at.time_since_epoch()).count()}
        };

        if (pending_decision) {
            result["pending_decision"] = pending_decision->to_json();
        }

        return result;
    }

private:
    static std::string generate_request_id(const std::string& agent_id) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return "request_" + agent_id + "_" + std::to_string(ms);
    }
};

} // namespace regulens
