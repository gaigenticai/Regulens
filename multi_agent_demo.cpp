/**
 * Multi-Agent Communication Enhancement Demo
 *
 * Demonstrates the complete multi-agent communication system with:
 * - Inter-agent messaging with LLM-mediated translation
 * - Collaborative decision-making and consensus algorithms
 * - Conflict resolution and negotiation capabilities
 * - Real-time agent communication patterns
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

#include "shared/config/configuration_manager.hpp"
#include "shared/logging/structured_logger.hpp"
#include "shared/error_handler.hpp"
#include "shared/llm/openai_client.hpp"
#include "shared/llm/anthropic_client.hpp"
#include "core/agent/agent_communication.hpp"
#include "core/agent/message_translator.hpp"
#include "core/agent/consensus_engine.hpp"

namespace regulens {

class MultiAgentDemo {
public:
    MultiAgentDemo()
        : logger_(StructuredLogger::get_instance()),
          config_manager_(&ConfigurationManager::get_instance()) {}

    bool initialize() {
        logger_.info("Initializing Multi-Agent Communication Demo");

        // Create error handler
        error_handler_ = std::make_shared<ErrorHandler>(config_manager_, &logger_);

        // Create LLM clients for message translation
        openai_client_ = std::make_shared<OpenAIClient>(config_manager_, &logger_, error_handler_.get());
        anthropic_client_ = std::make_shared<AnthropicClient>(config_manager_, &logger_, error_handler_.get());

        // Initialize communication components
        agent_registry_ = create_agent_registry(config_manager_, &logger_, error_handler_.get());
        communicator_ = create_inter_agent_communicator(config_manager_, agent_registry_, &logger_, error_handler_.get());
        translator_ = create_message_translator(config_manager_, openai_client_, anthropic_client_, &logger_, error_handler_.get());
        consensus_engine_ = create_consensus_engine(config_manager_, communicator_, translator_, &logger_, error_handler_.get());

        // Register demo agents
        register_demo_agents();

        logger_.info("Multi-Agent Communication Demo initialized successfully");
        return true;
    }

    void run_demo() {
        logger_.info("Starting Multi-Agent Communication Demo");

        // Demo 1: Basic inter-agent messaging
        demonstrate_basic_messaging();

        // Demo 2: Message translation between different agent types
        demonstrate_message_translation();

        // Demo 3: Collaborative decision-making
        demonstrate_collaborative_decision();

        // Demo 4: Conflict resolution
        demonstrate_conflict_resolution();

        // Demo 5: Communication statistics
        demonstrate_statistics();

        logger_.info("Multi-Agent Communication Demo completed");
    }

private:
    StructuredLogger& logger_;
    ConfigurationManager* config_manager_;
    std::shared_ptr<ErrorHandler> error_handler_;
    std::shared_ptr<OpenAIClient> openai_client_;
    std::shared_ptr<AnthropicClient> anthropic_client_;
    std::shared_ptr<AgentRegistry> agent_registry_;
    std::shared_ptr<InterAgentCommunicator> communicator_;
    std::shared_ptr<IntelligentMessageTranslator> translator_;
    std::shared_ptr<ConsensusEngine> consensus_engine_;

    void register_demo_agents() {
        logger_.info("Registering demo agents");

        // AML Agent
        AgentCapabilities aml_capabilities{
            {"AML", "FINANCIAL", "COMPLIANCE"},           // domains
            {"transaction_monitoring", "entity_screening"}, // specializations
            {"en"},                                        // languages
            {{"aml_expertise", 9}, {"risk_assessment", 8}}, // skills
            false, // supports_negotiation
            true,  // supports_collaboration
            true   // can_escalate
        };
        agent_registry_->register_agent("aml_agent", "AML_AGENT", aml_capabilities);

        // KYC Agent
        AgentCapabilities kyc_capabilities{
            {"KYC", "IDENTITY", "COMPLIANCE"},
            {"identity_verification", "document_validation"},
            {"en"},
            {{"kyc_expertise", 9}, {"identity_verification", 8}},
            true,  // supports_negotiation
            true,  // supports_collaboration
            false  // can_escalate
        };
        agent_registry_->register_agent("kyc_agent", "KYC_AGENT", kyc_capabilities);

        // Regulatory Agent
        AgentCapabilities regulatory_capabilities{
            {"SEC", "FINRA", "CFTC"},
            {"regulatory_reporting", "compliance_monitoring"},
            {"en"},
            {{"regulatory_expertise", 10}, {"reporting", 9}},
            true,  // supports_negotiation
            true,  // supports_collaboration
            true   // can_escalate
        };
        agent_registry_->register_agent("regulatory_agent", "REGULATORY_AGENT", regulatory_capabilities);

        // Risk Assessment Agent
        AgentCapabilities risk_capabilities{
            {"RISK", "ANALYTICS"},
            {"risk_modeling", "scenario_analysis"},
            {"en"},
            {{"risk_modeling", 9}, {"analytics", 8}},
            false, // supports_negotiation
            true,  // supports_collaboration
            true   // can_escalate
        };
        agent_registry_->register_agent("risk_agent", "RISK_AGENT", risk_capabilities);
    }

    void demonstrate_basic_messaging() {
        logger_.info("=== Demo 1: Basic Inter-Agent Messaging ===");

        // Send a direct message from AML agent to KYC agent
        nlohmann::json message_content = {
            {"text", "Please verify the identity documents for transaction TXN-2024-001"},
            {"transaction_id", "TXN-2024-001"},
            {"priority", "high"},
            {"requester", "aml_agent"}
        };

        bool sent = communicator_->send_message(AgentMessage(
            "aml_agent", "AML_AGENT", "kyc_agent", "KYC_AGENT",
            MessageType::REQUEST, message_content, MessagePriority::HIGH
        ));

        if (sent) {
            logger_.info("✓ Message sent successfully from AML to KYC agent");
        } else {
            logger_.error("✗ Failed to send message");
        }

        // Send a broadcast message
        nlohmann::json broadcast_content = {
            {"text", "System-wide alert: New regulatory update available"},
            {"update_type", "regulatory_change"},
            {"effective_date", "2024-12-01"},
            {"priority", "medium"}
        };

        bool broadcast = communicator_->send_broadcast("system", "SYSTEM",
            MessageType::NOTIFICATION, broadcast_content, MessagePriority::NORMAL);

        if (broadcast) {
            logger_.info("✓ Broadcast message sent successfully");
        }

        // Receive messages for KYC agent
        auto messages = communicator_->receive_messages("kyc_agent", 10);
        logger_.info("KYC agent received {} messages", messages.size());

        for (const auto& msg : messages) {
            logger_.info("  - Message from {}: {}", msg.sender_agent_id,
                        msg.content.value("text", "No text"));
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    void demonstrate_message_translation() {
        logger_.info("=== Demo 2: Message Translation Between Agent Types ===");

        // Create a technical message from Risk Assessment agent
        nlohmann::json technical_content = {
            {"text", "Stochastic risk model indicates 23.7% probability of AML violation with 95% confidence interval"},
            {"model_used", "stochastic_risk_model_v3"},
            {"confidence_interval", 0.95},
            {"risk_score", 23.7},
            {"technical_details", "Bayesian network analysis with Monte Carlo simulation"}
        };

        // Define agent contexts
        AgentCommunicationContext risk_context{
            "RISK_AGENT",
            {"RISK", "ANALYTICS"},
            "technical",
            "expert",
            {"risk_score", "probability", "confidence_interval", "stochastic"},
            {},
            "en"
        };

        AgentCommunicationContext regulatory_context{
            "REGULATORY_AGENT",
            {"SEC", "FINRA"},
            "formal",
            "intermediate",
            {"compliance", "violation", "regulation", "reporting"},
            {},
            "en"
        };

        // Create translation request
        AgentMessage original_message("risk_agent", "RISK_AGENT", "regulatory_agent", "REGULATORY_AGENT",
                                    MessageType::NOTIFICATION, technical_content);

        TranslationRequest request(original_message, risk_context, regulatory_context,
                                 "simplify technical content for regulatory audience");

        // Perform translation
        TranslationResult result = translator_->translate(request);

        if (result.success) {
            logger_.info("✓ Message translation successful");
            logger_.info("  Original: {}", technical_content["text"]);
            logger_.info("  Translated: {}", result.translated_message.content["text"]);
            logger_.info("  Approach: {}", result.translation_approach);
            logger_.info("  Confidence: {:.2f}", result.confidence_score);
        } else {
            logger_.warn("✗ Message translation failed: {}", result.error_message.value_or("Unknown error"));
        }
    }

    void demonstrate_collaborative_decision() {
        logger_.info("=== Demo 3: Collaborative Decision-Making ===");

        // Start a consensus session for transaction approval
        std::vector<std::string> participants = {"aml_agent", "kyc_agent", "risk_agent"};
        auto session_id = consensus_engine_->start_consensus_session(
            "Evaluate transaction TXN-2024-002 for compliance approval",
            participants,
            ConsensusAlgorithm::WEIGHTED_VOTE
        );

        if (!session_id) {
            logger_.error("✗ Failed to start consensus session");
            return;
        }

        logger_.info("✓ Started consensus session: {}", *session_id);

        // AML Agent contribution
        nlohmann::json aml_decision = {
            {"decision", "conditional_approval"},
            {"conditions", {"enhanced_due_diligence", "source_of_funds_verification"}},
            {"risk_level", "medium"},
            {"reasoning", "Transaction pattern matches AML risk indicators but no direct matches"}
        };
        consensus_engine_->submit_decision(*session_id, "aml_agent", aml_decision, 0.8);

        // KYC Agent contribution
        nlohmann::json kyc_decision = {
            {"decision", "approve"},
            {"verification_status", "completed"},
            {"identity_confidence", "high"},
            {"reasoning", "All identity documents verified, biometric match confirmed"}
        };
        consensus_engine_->submit_decision(*session_id, "kyc_agent", kyc_decision, 0.9);

        // Risk Agent contribution
        nlohmann::json risk_decision = {
            {"decision", "conditional_approval"},
            {"risk_score", 15.2},
            {"recommended_actions", {"additional_monitoring", "transaction_limits"}},
            {"reasoning", "Risk score below threshold but warrants enhanced monitoring"}
        };
        consensus_engine_->submit_decision(*session_id, "risk_agent", risk_decision, 0.85);

        // Wait a moment for consensus processing
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Get consensus result
        auto result = consensus_engine_->get_consensus_result(*session_id);

        if (result) {
            logger_.info("✓ Consensus reached:");
            logger_.info("  Decision: {}", result->final_decision.dump(2));
            logger_.info("  Consensus Strength: {:.2f}", result->consensus_strength);
            logger_.info("  Confidence Score: {:.2f}", result->confidence_score);
            logger_.info("  Algorithm: {}", static_cast<int>(result->algorithm_used));
        } else {
            logger_.warn("✗ Consensus not yet reached or failed");
        }
    }

    void demonstrate_conflict_resolution() {
        logger_.info("=== Demo 4: Conflict Resolution ===");

        // Create conflicting messages
        std::vector<AgentMessage> conflicting_messages;

        // Message 1: Approve transaction
        nlohmann::json approve_content = {
            {"decision", "approve"},
            {"confidence", 0.8},
            {"reasoning", "All checks passed"}
        };
        conflicting_messages.emplace_back("aml_agent", "AML_AGENT", "orchestrator", "SYSTEM",
                                        MessageType::RESPONSE, approve_content);

        // Message 2: Deny transaction
        nlohmann::json deny_content = {
            {"decision", "deny"},
            {"confidence", 0.9},
            {"reasoning", "High risk indicators detected"}
        };
        conflicting_messages.emplace_back("risk_agent", "RISK_AGENT", "orchestrator", "SYSTEM",
                                        MessageType::RESPONSE, deny_content);

        // Create communication mediator for conflict resolution
        auto mediator = create_communication_mediator(communicator_, translator_, &logger_, error_handler_.get());

        // Resolve conflicts
        nlohmann::json resolution = mediator->resolve_conflicts(conflicting_messages);

        logger_.info("✓ Conflict resolution result:");
        logger_.info("  Method: {}", resolution.value("resolution_method", "unknown"));
        logger_.info("  Winning Decision: {}", resolution.value("winning_message", nlohmann::json()).dump());
        logger_.info("  Confidence Score: {:.2f}", resolution.value("confidence_score", 0.0));
        logger_.info("  Agreement Level: {:.2f}", resolution["synthesized_outcome"].value("agreement_level", 0.0));
    }

    void demonstrate_statistics() {
        logger_.info("=== Demo 5: Communication Statistics ===");

        // Get communication statistics
        auto comm_stats = communicator_->get_communication_stats();
        logger_.info("Communication Stats:");
        logger_.info("  Messages Sent: {}", comm_stats.value("messages_sent", 0));
        logger_.info("  Messages Received: {}", comm_stats.value("messages_received", 0));
        logger_.info("  Messages Processed: {}", comm_stats.value("messages_processed", 0));
        logger_.info("  Queue Size: {}", comm_stats.value("queue_size", 0));
        logger_.info("  Active Agents: {}", comm_stats.value("active_agents", 0));

        // Get consensus statistics
        auto consensus_stats = consensus_engine_->get_statistics();
        logger_.info("Consensus Stats:");
        logger_.info("  Sessions Created: {}", consensus_stats.value("sessions_created", 0));
        logger_.info("  Sessions Completed: {}", consensus_stats.value("sessions_completed", 0));
        logger_.info("  Sessions Failed: {}", consensus_stats.value("sessions_failed", 0));
        logger_.info("  Success Rate: {:.2f}", consensus_stats.value("success_rate", 0.0));

        // Get translation statistics
        auto translation_stats = translator_->get_translation_stats();
        logger_.info("Translation Stats:");
        logger_.info("  Translations Performed: {}", translation_stats.value("translations_performed", 0));
        logger_.info("  LLM Translations: {}", translation_stats.value("llm_translations", 0));
        logger_.info("  Rule-based Translations: {}", translation_stats.value("rule_based_translations", 0));
        logger_.info("  Registered Agent Contexts: {}", translation_stats.value("registered_agent_contexts", 0));
    }
};

} // namespace regulens

int main(int argc, char* argv[]) {
    try {
        regulens::MultiAgentDemo demo;

        if (!demo.initialize()) {
            std::cerr << "Failed to initialize multi-agent demo" << std::endl;
            return 1;
        }

        demo.run_demo();

        std::cout << "\nMulti-Agent Communication Demo completed successfully!" << std::endl;
        std::cout << "✓ Inter-agent messaging demonstrated" << std::endl;
        std::cout << "✓ LLM-mediated message translation demonstrated" << std::endl;
        std::cout << "✓ Collaborative decision-making demonstrated" << std::endl;
        std::cout << "✓ Conflict resolution demonstrated" << std::endl;
        std::cout << "✓ Communication statistics demonstrated" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Demo failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
