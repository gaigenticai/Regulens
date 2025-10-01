#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <nlohmann/json.hpp>

namespace regulens {

/**
 * @brief Types of decisions agents can make
 */
enum class DecisionType {
    APPROVE,
    DENY,
    ESCALATE,
    MONITOR,
    INVESTIGATE,
    REPORT,
    ALERT,
    NO_ACTION
};

/**
 * @brief Decision confidence levels
 */
enum class ConfidenceLevel {
    LOW,
    MEDIUM,
    HIGH,
    VERY_HIGH
};

/**
 * @brief Decision reasoning component
 */
struct DecisionReasoning {
    std::string factor;         // What factor influenced the decision
    std::string evidence;       // Supporting evidence
    double weight;             // Importance weight (0.0 to 1.0)
    std::string source;        // Source of this reasoning

    nlohmann::json to_json() const {
        return {
            {"factor", factor},
            {"evidence", evidence},
            {"weight", weight},
            {"source", source}
        };
    }
};

/**
 * @brief Recommended actions from agent decision
 */
struct RecommendedAction {
    std::string action_type;    // "approve", "deny", "escalate", "monitor", etc.
    std::string description;    // Human-readable description
    Priority priority;          // Action priority
    std::chrono::system_clock::time_point deadline;  // Action deadline
    std::unordered_map<std::string, std::string> parameters;  // Action parameters

    nlohmann::json to_json() const {
        nlohmann::json params_json;
        for (const auto& [key, value] : parameters) {
            params_json[key] = value;
        }

        return {
            {"action_type", action_type},
            {"description", description},
            {"priority", static_cast<int>(priority)},
            {"deadline", std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline.time_since_epoch()).count()},
            {"parameters", params_json}
        };
    }
};

/**
 * @brief Compliance risk assessment
 */
struct RiskAssessment {
    double risk_score;         // 0.0 (no risk) to 1.0 (maximum risk)
    std::string risk_level;    // "low", "medium", "high", "critical"
    std::vector<std::string> risk_factors;  // Contributing risk factors
    std::chrono::system_clock::time_point assessment_time;

    nlohmann::json to_json() const {
        return {
            {"risk_score", risk_score},
            {"risk_level", risk_level},
            {"risk_factors", risk_factors},
            {"assessment_time", std::chrono::duration_cast<std::chrono::milliseconds>(
                assessment_time.time_since_epoch()).count()}
        };
    }
};

/**
 * @brief Agent decision structure
 *
 * Represents a decision made by an agent along with reasoning,
 * confidence, and recommended actions.
 */
class AgentDecision {
public:
    AgentDecision(DecisionType type, ConfidenceLevel confidence,
                 std::string agent_id, std::string event_id)
        : decision_type_(type), confidence_(confidence),
          agent_id_(std::move(agent_id)), event_id_(std::move(event_id)),
          timestamp_(std::chrono::system_clock::now()),
          decision_id_(generate_decision_id()) {}

    // Getters
    DecisionType get_type() const { return decision_type_; }
    ConfidenceLevel get_confidence() const { return confidence_; }
    const std::string& get_agent_id() const { return agent_id_; }
    const std::string& get_event_id() const { return event_id_; }
    const std::string& get_decision_id() const { return decision_id_; }
    std::chrono::system_clock::time_point get_timestamp() const { return timestamp_; }

    // Decision details
    const std::vector<DecisionReasoning>& get_reasoning() const { return reasoning_; }
    const std::vector<RecommendedAction>& get_actions() const { return actions_; }
    const std::optional<RiskAssessment>& get_risk_assessment() const { return risk_assessment_; }

    // Setters
    void add_reasoning(const DecisionReasoning& reasoning) {
        reasoning_.push_back(reasoning);
    }

    void add_action(const RecommendedAction& action) {
        actions_.push_back(action);
    }

    void set_risk_assessment(const RiskAssessment& assessment) {
        risk_assessment_ = assessment;
    }

    // Utility methods
    bool requires_action() const {
        return decision_type_ == DecisionType::ESCALATE ||
               decision_type_ == DecisionType::INVESTIGATE ||
               decision_type_ == DecisionType::REPORT ||
               decision_type_ == DecisionType::ALERT;
    }

    std::string get_decision_summary() const {
        std::string summary = "Decision: " + decision_type_to_string(decision_type_) +
                             " (Confidence: " + confidence_to_string(confidence_) + ")";
        if (risk_assessment_) {
            summary += " Risk: " + risk_assessment_->risk_level +
                      " (" + std::to_string(risk_assessment_->risk_score) + ")";
        }
        return summary;
    }

    // JSON serialization
    nlohmann::json to_json() const {
        nlohmann::json reasoning_json;
        for (const auto& r : reasoning_) {
            reasoning_json.push_back(r.to_json());
        }

        nlohmann::json actions_json;
        for (const auto& a : actions_) {
            actions_json.push_back(a.to_json());
        }

        nlohmann::json json = {
            {"decision_id", decision_id_},
            {"decision_type", static_cast<int>(decision_type_)},
            {"confidence", static_cast<int>(confidence_)},
            {"agent_id", agent_id_},
            {"event_id", event_id_},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp_.time_since_epoch()).count()},
            {"reasoning", reasoning_json},
            {"actions", actions_json}
        };

        if (risk_assessment_) {
            json["risk_assessment"] = risk_assessment_->to_json();
        }

        return json;
    }

    // JSON deserialization
    static std::optional<AgentDecision> from_json(const nlohmann::json& json) {
        try {
            AgentDecision decision(
                static_cast<DecisionType>(json["decision_type"].get<int>()),
                static_cast<ConfidenceLevel>(json["confidence"].get<int>()),
                json["agent_id"].get<std::string>(),
                json["event_id"].get<std::string>()
            );

            // Restore original timestamp and ID
            auto timestamp_ms = json["timestamp"].get<int64_t>();
            decision.timestamp_ = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(timestamp_ms));
            decision.decision_id_ = json["decision_id"].get<std::string>();

            // Restore reasoning
            if (json.contains("reasoning")) {
                for (const auto& r : json["reasoning"]) {
                    decision.reasoning_.push_back({
                        r["factor"].get<std::string>(),
                        r["evidence"].get<std::string>(),
                        r["weight"].get<double>(),
                        r["source"].get<std::string>()
                    });
                }
            }

            // Restore actions
            if (json.contains("actions")) {
                for (const auto& a : json["actions"]) {
                    std::unordered_map<std::string, std::string> params;
                    if (a.contains("parameters")) {
                        for (const auto& [key, value] : a["parameters"].items()) {
                            params[key] = value.get<std::string>();
                        }
                    }

                    decision.actions_.push_back({
                        a["action_type"].get<std::string>(),
                        a["description"].get<std::string>(),
                        static_cast<Priority>(a["priority"].get<int>()),
                        std::chrono::system_clock::time_point(
                            std::chrono::milliseconds(a["deadline"].get<int64_t>())),
                        params
                    });
                }
            }

            // Restore risk assessment
            if (json.contains("risk_assessment")) {
                const auto& ra = json["risk_assessment"];
                decision.risk_assessment_ = RiskAssessment{
                    ra["risk_score"].get<double>(),
                    ra["risk_level"].get<std::string>(),
                    ra["risk_factors"].get<std::vector<std::string>>(),
                    std::chrono::system_clock::time_point(
                        std::chrono::milliseconds(ra["assessment_time"].get<int64_t>()))
                };
            }

            return decision;
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

private:
    static std::string generate_decision_id() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
        return "dec_" + std::to_string(timestamp) + "_" + std::to_string(++counter);
    }

    DecisionType decision_type_;
    ConfidenceLevel confidence_;
    std::string agent_id_;
    std::string event_id_;
    std::string decision_id_;
    std::chrono::system_clock::time_point timestamp_;

    std::vector<DecisionReasoning> reasoning_;
    std::vector<RecommendedAction> actions_;
    std::optional<RiskAssessment> risk_assessment_;
};

// Helper functions
inline std::string decision_type_to_string(DecisionType type) {
    switch (type) {
        case DecisionType::APPROVE: return "APPROVE";
        case DecisionType::DENY: return "DENY";
        case DecisionType::ESCALATE: return "ESCALATE";
        case DecisionType::MONITOR: return "MONITOR";
        case DecisionType::INVESTIGATE: return "INVESTIGATE";
        case DecisionType::REPORT: return "REPORT";
        case DecisionType::ALERT: return "ALERT";
        case DecisionType::NO_ACTION: return "NO_ACTION";
        default: return "UNKNOWN";
    }
}

inline std::string confidence_to_string(ConfidenceLevel level) {
    switch (level) {
        case ConfidenceLevel::LOW: return "LOW";
        case ConfidenceLevel::MEDIUM: return "MEDIUM";
        case ConfidenceLevel::HIGH: return "HIGH";
        case ConfidenceLevel::VERY_HIGH: return "VERY_HIGH";
        default: return "UNKNOWN";
    }
}

} // namespace regulens
