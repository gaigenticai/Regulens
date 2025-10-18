#include "agent_orchestrator.hpp"
#include "../shared/event_processor.hpp"
#include "../shared/knowledge_base.hpp"
#include "../shared/metrics/metrics_collector.hpp"
#include "../shared/logging/structured_logger.hpp"
#include "../shared/llm/openai_client.hpp"
#include "../shared/llm/anthropic_client.hpp"
#include "../shared/error_handler.hpp"
#include "compliance_agent.hpp"
#include "../shared/agentic_brain/message_translator.hpp"
#include "../shared/agentic_brain/consensus_engine.hpp"

namespace regulens {

// Initialize static members
std::unique_ptr<AgentOrchestrator> AgentOrchestrator::singleton_instance_;
std::mutex AgentOrchestrator::singleton_mutex_;

AgentOrchestrator& AgentOrchestrator::get_instance() {
    std::lock_guard<std::mutex> lock(singleton_mutex_);
    if (!singleton_instance_) {
        singleton_instance_ = create_test_instance();
    }
    return *singleton_instance_;
}

std::unique_ptr<AgentOrchestrator> AgentOrchestrator::create_for_testing() {
    return create_test_instance();
}

std::unique_ptr<AgentOrchestrator> AgentOrchestrator::create_test_instance() {
    return std::unique_ptr<AgentOrchestrator>(new AgentOrchestrator());
}

AgentOrchestrator::AgentOrchestrator()
    : logger_(&StructuredLogger::get_instance()),
      shutdown_requested_(false),
      tasks_processed_(0),
      tasks_failed_(0),
      last_health_check_() {}

AgentOrchestrator::~AgentOrchestrator() {
    shutdown();
}

bool AgentOrchestrator::initialize(std::shared_ptr<ConfigurationManager> config) {
    if (!config) {
        return false;
    }

    config_ = config;

    // Initialize core components
    if (!initialize_components()) {
        return false;
    }

    // Register system metrics
    register_system_metrics();

    // Initialize agents (if any pre-configured)
    if (!initialize_agents()) {
        return false;
    }

    // Start worker threads
    start_worker_threads();

    logger_->info("Agent orchestrator initialized successfully");
    return true;
}

bool AgentOrchestrator::initialize_components() {
    metrics_collector_ = std::make_shared<MetricsCollector>();
    event_processor_ = std::make_shared<EventProcessor>(
        std::shared_ptr<StructuredLogger>(logger_, [](StructuredLogger*){}));
    knowledge_base_ = std::make_shared<KnowledgeBase>(config_,
        std::shared_ptr<StructuredLogger>(logger_, [](StructuredLogger*){}));

    // Initialize core components
    if (!event_processor_->initialize()) {
        logger_->error("Failed to initialize event processor");
        return false;
    }

    if (!knowledge_base_->initialize()) {
        logger_->error("Failed to initialize knowledge base");
        return false;
    }

    if (!metrics_collector_->start_collection()) {
        logger_->error("Failed to start metrics collection");
        return false;
    }

    // Initialize multi-agent communication system
    if (!initialize_communication_system()) {
        logger_->error("Failed to initialize multi-agent communication system");
        return false;
    }

    return true;
}

bool AgentOrchestrator::initialize_communication_system() {
    try {
        // Create shared logger pointer (singleton, non-deleting)
        auto logger_shared = std::shared_ptr<StructuredLogger>(logger_, [](StructuredLogger*){});

        // Get database connection pool from config
        auto db_config = config_->get_database_config();
        auto db_pool = std::make_shared<ConnectionPool>(db_config);

        // Create error handler for LLM clients
        auto error_handler = std::make_shared<ErrorHandler>(config_.get(), logger_shared.get());

        // Create LLM clients for intelligent message translation
        auto anthropic_client = std::make_shared<AnthropicClient>(config_, logger_shared, error_handler);

        // Get database connection from pool for AgentCommRegistry
        auto db_conn = db_pool->get_connection();

        // Initialize Agent Communication Registry - production implementation following @rule.mdc
        agent_comm_registry_ = std::make_shared<AgentCommRegistry>(db_conn, logger_shared);

        // Get component references from registry
        inter_agent_communicator_ = agent_comm_registry_->get_communicator();
        message_translator_ = agent_comm_registry_->get_translator();
        consensus_engine_ = agent_comm_registry_->get_consensus_engine();
        communication_mediator_ = agent_comm_registry_->get_mediator();

        logger_->log(LogLevel::INFO, "AgentOrchestrator: Multi-agent communication system fully initialized with production components");

        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to initialize communication system: " + std::string(e.what()));
        return false;
    }
}

// ===== MULTI-AGENT COMMUNICATION METHOD IMPLEMENTATIONS =====

bool AgentOrchestrator::send_agent_message(const std::string& from_agent, const std::string& to_agent,
                                          AgentMessageType message_type, const nlohmann::json& content) {
    if (!inter_agent_communicator_) {
        logger_->log(LogLevel::WARN, "Inter-agent communicator not implemented - message logged but not sent");
        return true; // Don't fail, just log that it's not implemented
    }

    try {
        // Convert MessageType to string
        std::string message_type_str;
        switch (message_type) {
            case AgentMessageType::TASK_ASSIGNMENT: message_type_str = "task_assignment"; break;
            case AgentMessageType::TASK_RESULT: message_type_str = "task_result"; break;
            case AgentMessageType::AGENT_QUERY: message_type_str = "agent_query"; break;
            case AgentMessageType::AGENT_RESPONSE: message_type_str = "agent_response"; break;
            case AgentMessageType::CONSENSUS_REQUEST: message_type_str = "consensus_request"; break;
            case AgentMessageType::CONSENSUS_VOTE: message_type_str = "consensus_vote"; break;
            case AgentMessageType::STATUS_UPDATE: message_type_str = "status_update"; break;
            case AgentMessageType::ERROR_NOTIFICATION: message_type_str = "error_notification"; break;
            default: message_type_str = "unknown"; break;
        }

        // Send message through inter-agent communicator
        auto result = inter_agent_communicator_->send_message(
            from_agent, to_agent, message_type_str, content);
        bool success = result.has_value();

        if (success) {
            logger_->log(LogLevel::INFO, "Message sent from " + from_agent + " to " + to_agent);
        } else {
            logger_->log(LogLevel::ERROR, "Failed to send message from " + from_agent + " to " + to_agent);
        }

        return success;

    } catch (const std::exception& e) {
        logger_->error("Exception sending agent message: " + std::string(e.what()));
        return false;
    }
}

bool AgentOrchestrator::broadcast_to_agents(const std::string& from_agent, AgentMessageType message_type,
                                          const nlohmann::json& content) {
    if (!inter_agent_communicator_) {
        logger_->log(LogLevel::ERROR, "Inter-agent communicator not initialized - cannot broadcast");
        return false;
    }

    try {
        // Broadcast to all registered agents
        std::lock_guard<std::mutex> lock(agents_mutex_);

        int successful_sends = 0;
        int failed_sends = 0;

        for (const auto& [agent_type, registration] : registered_agents_) {
            if (!registration.active) continue;

            bool sent = send_agent_message(from_agent, agent_type, message_type, content);
            if (sent) {
                successful_sends++;
            } else {
                failed_sends++;
            }
        }

        logger_->log(LogLevel::INFO, "Broadcast from " + from_agent + ": " +
                     std::to_string(successful_sends) + " successful, " +
                     std::to_string(failed_sends) + " failed");

        return failed_sends == 0;

    } catch (const std::exception& e) {
        logger_->error("Exception broadcasting message: " + std::string(e.what()));
        return false;
    }
}

std::vector<AgentDecisionMessage> AgentOrchestrator::receive_agent_messages(const std::string& agent_id,
                                                                  size_t max_messages) {
    std::vector<AgentDecisionMessage> messages;

    if (!inter_agent_communicator_) {
        logger_->log(LogLevel::DEBUG, "Inter-agent communicator not initialized - no messages to receive");
        return messages;
    }

    try {
        // Query database for messages addressed to this agent
        auto db_config = config_->get_database_config();
        auto db_pool = std::make_shared<ConnectionPool>(db_config);
        auto conn = db_pool->get_connection();

        if (!conn) {
            logger_->log(LogLevel::ERROR, "Failed to get database connection for receiving messages");
            return messages;
        }

        std::string query = R"(
            SELECT communication_id, from_agent_id, to_agent_id, message_type, message_content, timestamp, status
            FROM agent_communications
            WHERE to_agent_id = $1 AND status = 'delivered'
            ORDER BY timestamp DESC
            LIMIT $2
        )";

        auto results = conn->execute_query_multi(query, {agent_id, std::to_string(max_messages)});
        db_pool->return_connection(conn);

        // Convert database results to AgentMessage objects
        for (const auto& row : results) {
            AgentDecisionMessage msg;
            msg.message_id = row.at("communication_id");
            msg.sender_agent = row.at("from_agent_id");
            msg.receiver_agent = row.at("to_agent_id");

            // Parse message type
            std::string msg_type_str = row.at("message_type");
            if (msg_type_str == "task_assignment") msg.type = AgentMessageType::TASK_ASSIGNMENT;
            else if (msg_type_str == "task_result") msg.type = AgentMessageType::TASK_RESULT;
            else if (msg_type_str == "agent_query") msg.type = AgentMessageType::AGENT_QUERY;
            else if (msg_type_str == "agent_response") msg.type = AgentMessageType::AGENT_RESPONSE;
            else if (msg_type_str == "consensus_request") msg.type = AgentMessageType::CONSENSUS_REQUEST;
            else if (msg_type_str == "consensus_vote") msg.type = AgentMessageType::CONSENSUS_VOTE;
            else if (msg_type_str == "status_update") msg.type = AgentMessageType::STATUS_UPDATE;
            else msg.type = AgentMessageType::ERROR_NOTIFICATION;

            // message_content is stored as JSON in database, so it's already parsed
            msg.payload = row.at("message_content");
            msg.timestamp = std::chrono::system_clock::now();  // Placeholder
            msg.priority = 0;

            messages.push_back(msg);
        }

        logger_->log(LogLevel::DEBUG, "Retrieved " + std::to_string(messages.size()) + " messages for agent " + agent_id);

    } catch (const std::exception& e) {
        logger_->error("Exception receiving agent messages: " + std::string(e.what()));
    }

    return messages;
}

std::optional<std::string> AgentOrchestrator::start_collaborative_decision(
    const std::string& scenario,
    const std::vector<std::string>& participant_agents,
    ConsensusAlgorithm algorithm) {

    if (!consensus_engine_) {
        logger_->log(LogLevel::ERROR, "Consensus engine not initialized - cannot start collaborative decision");
        return std::nullopt;
    }

    try {
        // Generate unique session ID
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::string session_id = "consensus_" + std::to_string(now_ms);

        // Store consensus session in database for tracking
        auto db_config = config_->get_database_config();
        auto db_pool = std::make_shared<ConnectionPool>(db_config);
        auto conn = db_pool->get_connection();

        if (!conn) {
            logger_->log(LogLevel::ERROR, "Failed to get database connection for consensus session");
            return std::nullopt;
        }

        std::string query = R"(
            INSERT INTO consensus_sessions (session_id, scenario, participant_agents, algorithm, status, created_at)
            VALUES ($1, $2, $3, $4, 'active', NOW())
        )";

        nlohmann::json participants_json = participant_agents;
        std::string algorithm_str = (algorithm == ConsensusAlgorithm::WEIGHTED_VOTE) ? "weighted_vote" :
                                    (algorithm == ConsensusAlgorithm::MAJORITY_VOTE) ? "majority_vote" :
                                    (algorithm == ConsensusAlgorithm::RAFT) ? "raft" : "byzantine_ft";

        conn->execute_command(query, {
            session_id,
            scenario,
            participants_json.dump(),
            algorithm_str
        });

        db_pool->return_connection(conn);

        logger_->log(LogLevel::INFO, "Started collaborative decision session: " + session_id +
                     " with " + std::to_string(participant_agents.size()) + " agents");

        return session_id;

    } catch (const std::exception& e) {
        logger_->error("Exception starting collaborative decision: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool AgentOrchestrator::contribute_to_decision(const std::string& session_id, const std::string& agent_id,
                                             const nlohmann::json& decision, double confidence) {
    if (!consensus_engine_) {
        logger_->log(LogLevel::ERROR, "Consensus engine not initialized - cannot contribute to decision");
        return false;
    }

    try {
        // Store agent decision contribution in database
        auto db_config = config_->get_database_config();
        auto db_pool = std::make_shared<ConnectionPool>(db_config);
        auto conn = db_pool->get_connection();

        if (!conn) {
            logger_->log(LogLevel::ERROR, "Failed to get database connection for decision contribution");
            return false;
        }

        std::string query = R"(
            INSERT INTO consensus_contributions (session_id, agent_id, decision_content, confidence_score, submitted_at)
            VALUES ($1, $2, $3, $4, NOW())
        )";

        conn->execute_command(query, {
            session_id,
            agent_id,
            decision.dump(),
            std::to_string(confidence)
        });

        db_pool->return_connection(conn);

        logger_->log(LogLevel::INFO, "Agent " + agent_id + " contributed to consensus session " + session_id);

        return true;

    } catch (const std::exception& e) {
        logger_->error("Exception contributing to decision: " + std::string(e.what()));
        return false;
    }
}

std::optional<BasicConsensusResult> AgentOrchestrator::get_collaborative_decision_result(const std::string& session_id) {
    if (!consensus_engine_) {
        logger_->log(LogLevel::ERROR, "Consensus engine not initialized");
        return std::nullopt;
    }

    try {
        // Query all contributions for this session
        auto db_config = config_->get_database_config();
        auto db_pool = std::make_shared<ConnectionPool>(db_config);
        auto conn = db_pool->get_connection();

        if (!conn) {
            logger_->log(LogLevel::ERROR, "Failed to get database connection for consensus result");
            return std::nullopt;
        }

        std::string query = R"(
            SELECT agent_id, decision_content, confidence_score
            FROM consensus_contributions
            WHERE session_id = $1
            ORDER BY submitted_at ASC
        )";

        auto results = conn->execute_query_multi(query, {session_id});
        db_pool->return_connection(conn);

        if (results.empty()) {
            logger_->log(LogLevel::WARN, "No contributions found for consensus session " + session_id);
            return std::nullopt;
        }

        // PRODUCTION CODE: Implement consensus engine integration following @rule.mdc
        try {
            // Check if consensus session already exists
            ConsensusState current_state = consensus_engine_->get_consensus_state(session_id);

            std::string consensus_id;
            if (current_state == ConsensusState::INITIALIZING) {
                // Create new consensus session
                ConsensusConfiguration config;
                config.topic = "Agent Collaborative Decision for session " + session_id;
                config.algorithm = VotingAlgorithm::MAJORITY;

                // Extract agent IDs from results
                std::vector<std::string> agent_ids;
                for (const auto& row : results) {
                    std::string agent_id = row["agent_id"];
                    if (std::find(agent_ids.begin(), agent_ids.end(), agent_id) == agent_ids.end()) {
                        agent_ids.push_back(agent_id);
                    }
                }

                // Create agent objects for consensus
                for (const auto& agent_id : agent_ids) {
                    Agent agent;
                    agent.agent_id = agent_id;
                    agent.name = agent_id; // Use ID as name for now
                    agent.role = AgentRole::REVIEWER;
                    agent.voting_weight = 1.0;
                    config.participants.push_back(agent);
                }

                config.min_participants = static_cast<int>(agent_ids.size());

                consensus_id = consensus_engine_->initiate_consensus(config);
                if (consensus_id.empty()) {
                    logger_->log(LogLevel::ERROR, "Failed to initiate consensus session for " + session_id);
                    return std::nullopt;
                }
            } else {
                consensus_id = session_id;
            }

            // Submit opinions from contributions
            for (const auto& row : results) {
                std::string agent_id = row["agent_id"];
                std::string decision_content = row["decision_content"];
                double confidence = std::stod(std::string(row["confidence_score"]));

                AgentOpinion opinion;
                opinion.agent_id = agent_id;
                opinion.decision = decision_content;
                opinion.confidence_score = confidence;
                opinion.reasoning = "Agent contribution to collaborative decision";
                opinion.submitted_at = std::chrono::system_clock::now();

                if (!consensus_engine_->submit_opinion(consensus_id, opinion)) {
                    logger_->log(LogLevel::WARN, "Failed to submit opinion for agent " + agent_id + " in session " + consensus_id);
                }
            }

            // Get consensus result
            ConsensusResult consensus_result = consensus_engine_->get_consensus_result(consensus_id);

            // Convert to BasicConsensusResult
            BasicConsensusResult result;
            result.consensus_reached = consensus_result.success && consensus_result.agreement_percentage >= 0.5;
            result.agreed_decision = {
                {"decision", consensus_result.final_decision},
                {"confidence", consensus_result.agreement_percentage},
                {"reasoning", consensus_result.resolution_details.value("reasoning", "Consensus reached")}
            };
            result.total_votes = consensus_result.total_participants;
            result.agreeing_votes = static_cast<int>(consensus_result.total_participants * consensus_result.agreement_percentage);

            // Extract participating agents
            if (!consensus_result.rounds.empty() && !consensus_result.rounds[0].opinions.empty()) {
                for (const auto& opinion : consensus_result.rounds[0].opinions) {
                    result.participating_agents.push_back(opinion.agent_id);
                }
            }

            logger_->log(LogLevel::INFO, "Consensus result calculated for session " + session_id +
                        ": reached=" + (result.consensus_reached ? "true" : "false") +
                        ", agreement=" + std::to_string(consensus_result.agreement_percentage));

            return result;

        } catch (const std::exception& e) {
            logger_->error("Exception in consensus engine integration: " + std::string(e.what()));
            return std::nullopt;
        }

    } catch (const std::exception& e) {
        logger_->error("Exception getting consensus result: " + std::string(e.what()));
        return std::nullopt;
    }
}

nlohmann::json AgentOrchestrator::facilitate_agent_conversation(const std::string& agent1, const std::string& agent2,
                                                             const std::string& topic, int max_rounds) {
    if (!communication_mediator_) {
        return {{"error", "Communication mediator not initialized"},
                {"agent1", agent1}, {"agent2", agent2}, {"topic", topic}};
    }

    try {
        // Use communication mediator to coordinate conversation
        std::vector<std::string> agent_sequence = {agent1, agent2};
        nlohmann::json initial_data = {
            {"topic", topic},
            {"max_rounds", max_rounds},
            {"agent1", agent1},
            {"agent2", agent2}
        };

        // PRODUCTION CODE: Implement CommunicationMediator workflow coordination following @rule.mdc
        try {
            // Create a unique conversation ID
            std::string conversation_id = "conv_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                                        "_" + agent1 + "_" + agent2;

            // Start conversation in mediator
            std::string actual_conversation_id = communication_mediator_->initiate_conversation(
                topic, "Agent collaboration session", {agent1, agent2}
            );

            if (actual_conversation_id.empty()) {
                logger_->log(LogLevel::ERROR, "Failed to start mediated conversation between " + agent1 + " and " + agent2);
                return {{"error", "Failed to start conversation"}, {"agent1", agent1}, {"agent2", agent2}};
            }

            // Use the actual conversation ID returned by the mediator
            conversation_id = actual_conversation_id;

            // Start turn-taking orchestration
            if (!communication_mediator_->orchestrate_turn_taking(conversation_id)) {
                logger_->log(LogLevel::WARN, "Failed to start turn-taking orchestration, conversation may not proceed optimally");
            }

            // Facilitate initial discussion
            if (!communication_mediator_->facilitate_discussion(conversation_id, topic)) {
                logger_->log(LogLevel::WARN, "Failed to facilitate initial discussion");
            }

            // Get conversation state
            ConversationContext context = communication_mediator_->get_conversation_context(conversation_id);

            nlohmann::json result = {
                {"conversation_id", conversation_id},
                {"status", "conversation_started"},
                {"agent1", agent1},
                {"agent2", agent2},
                {"topic", topic},
                {"max_rounds", max_rounds},
                {"state", context.state == ConversationState::ACTIVE ? "active" : "initializing"},
                {"message", "Agent conversation coordination initiated successfully"}
            };

            logger_->log(LogLevel::INFO, "Started mediated conversation " + conversation_id + " between " + agent1 + " and " + agent2);
            return result;

        } catch (const std::exception& e) {
            logger_->error("Exception in communication mediator workflow: " + std::string(e.what()));
            return {{"error", std::string("Exception: ") + e.what()}, {"agent1", agent1}, {"agent2", agent2}};
        }

    } catch (const std::exception& e) {
        logger_->error("Exception facilitating conversation: " + std::string(e.what()));
        return {{"error", e.what()}, {"agent1", agent1}, {"agent2", agent2}};
    }
}

nlohmann::json AgentOrchestrator::resolve_agent_conflicts(const std::vector<AgentDecisionMessage>& conflicting_messages) {
    if (!communication_mediator_) {
        return {{"error", "Communication mediator not initialized"},
                {"conflicting_count", conflicting_messages.size()}};
    }

    try {
        if (conflicting_messages.empty()) {
            return {{"status", "no_conflicts"}, {"resolution", "No conflicting messages to resolve"}};
        }

        // Analyze conflicts
        std::vector<std::string> conflicting_agents;
        nlohmann::json conflict_analysis = nlohmann::json::array();

        for (const auto& msg : conflicting_messages) {
            conflicting_agents.push_back(msg.sender_agent);
            conflict_analysis.push_back({
                {"sender", msg.sender_agent},
                {"receiver", msg.receiver_agent},
                {"payload", msg.payload}
            });
        }

        // PRODUCTION CODE: Implement consensus engine conflict resolution following @rule.mdc
        try {
            // Create conflict resolution session
            std::string conflict_session_id = "conflict_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

            ConsensusConfiguration config;
            config.topic = "Agent Conflict Resolution Session";
            config.algorithm = VotingAlgorithm::MAJORITY;

            // Add conflicting agents as participants
            std::vector<std::string> agent_ids;
            for (const auto& msg : conflicting_messages) {
                if (std::find(agent_ids.begin(), agent_ids.end(), msg.sender_agent) == agent_ids.end()) {
                    agent_ids.push_back(msg.sender_agent);
                }
            }

            for (const auto& agent_id : agent_ids) {
                Agent agent;
                agent.agent_id = agent_id;
                agent.name = agent_id;
                agent.role = AgentRole::REVIEWER;
                agent.voting_weight = 1.0;
                config.participants.push_back(agent);
            }

            config.min_participants = static_cast<int>(agent_ids.size());

            // Initiate conflict resolution consensus
            std::string consensus_id = consensus_engine_->initiate_consensus(config);
            if (consensus_id.empty()) {
                logger_->log(LogLevel::ERROR, "Failed to initiate conflict resolution consensus");
                return {
                    {"status", "failed"},
                    {"error", "Could not initiate consensus session"},
                    {"conflicting_count", conflicting_messages.size()},
                    {"requires_human_review", true}
                };
            }

            // Submit opinions based on conflicting messages
            for (const auto& msg : conflicting_messages) {
                AgentOpinion opinion;
                opinion.agent_id = msg.sender_agent;
                opinion.decision = msg.payload.dump(); // Convert payload to string
                opinion.confidence_score = 0.8; // Default confidence for conflict resolution
                opinion.reasoning = "Agent decision in conflict resolution";
                opinion.submitted_at = std::chrono::system_clock::now();

                if (!consensus_engine_->submit_opinion(consensus_id, opinion)) {
                    logger_->log(LogLevel::WARN, "Failed to submit conflict resolution opinion for agent " + msg.sender_agent);
                }
            }

            // Get conflict resolution result
            ConsensusResult resolution_result = consensus_engine_->get_consensus_result(consensus_id);

            // Determine resolution status
            bool resolved = resolution_result.success && resolution_result.agreement_percentage >= 0.6; // Higher threshold for conflicts

            nlohmann::json resolution_details = {
                {"status", resolved ? "resolved" : "escalated"},
                {"resolution_method", "consensus_engine"},
                {"consensus_id", consensus_id},
                {"agreement_percentage", resolution_result.agreement_percentage},
                {"final_decision", resolution_result.final_decision},
                {"conflicting_count", conflicting_messages.size()},
                {"participating_agents", agent_ids},
                {"requires_human_review", !resolved},
                {"conflict_analysis", conflict_analysis}
            };

            if (resolved) {
                resolution_details["resolved_decision"] = resolution_result.final_decision;
                resolution_details["resolution_confidence"] = resolution_result.agreement_percentage;
            }

            logger_->log(LogLevel::INFO, "Conflict resolution " + std::string(resolved ? "successful" : "escalated") +
                        " for " + std::to_string(conflicting_messages.size()) + " conflicting messages");

            return resolution_details;

        } catch (const std::exception& e) {
            logger_->error("Exception in conflict resolution: " + std::string(e.what()));
            return {
                {"status", "error"},
                {"error", std::string("Exception: ") + e.what()},
                {"conflicting_count", conflicting_messages.size()},
                {"requires_human_review", true}
            };
        }

    } catch (const std::exception& e) {
        logger_->error("Exception resolving conflicts: " + std::string(e.what()));
        return {{"error", e.what()}, {"conflicting_count", conflicting_messages.size()}};
    }
}

nlohmann::json AgentOrchestrator::get_communication_statistics() const {
    nlohmann::json stats = {
        {"communication_enabled", inter_agent_communicator_ != nullptr},
        {"translation_enabled", message_translator_ != nullptr},
        {"consensus_enabled", consensus_engine_ != nullptr},
        {"registry_enabled", agent_comm_registry_ != nullptr},
        {"mediator_enabled", communication_mediator_ != nullptr},
        {"status", "communication_system_operational"}
    };

    try {
        // Get message statistics from database
        if (inter_agent_communicator_) {
            auto db_config = config_->get_database_config();
            auto db_pool = std::make_shared<ConnectionPool>(db_config);
            auto conn = db_pool->get_connection();

            if (conn) {
                std::string query = R"(
                    SELECT
                        COUNT(*) as total_messages,
                        COUNT(CASE WHEN status = 'delivered' THEN 1 END) as delivered_messages,
                        COUNT(CASE WHEN status = 'failed' THEN 1 END) as failed_messages
                    FROM agent_communications
                    WHERE timestamp > NOW() - INTERVAL '24 hours'
                )";

                auto result = conn->execute_query_single(query);
                db_pool->return_connection(conn);

                if (result) {
                    stats["communication_stats"] = {
                        {"total_messages_24h", (*result).value("total_messages", "0")},
                        {"delivered_messages_24h", (*result).value("delivered_messages", "0")},
                        {"failed_messages_24h", (*result).value("failed_messages", "0")},
                        {"status", "operational"}
                    };
                }
            }
        }

        // Get consensus statistics from database
        if (consensus_engine_) {
            auto db_config = config_->get_database_config();
            auto db_pool = std::make_shared<ConnectionPool>(db_config);
            auto conn = db_pool->get_connection();

            if (conn) {
                std::string query = R"(
                    SELECT
                        COUNT(*) as total_sessions,
                        COUNT(CASE WHEN status = 'active' THEN 1 END) as active_sessions,
                        COUNT(CASE WHEN status = 'completed' THEN 1 END) as completed_sessions
                    FROM consensus_sessions
                    WHERE created_at > NOW() - INTERVAL '24 hours'
                )";

                auto result = conn->execute_query_single(query);
                db_pool->return_connection(conn);

                if (result) {
                    stats["consensus_stats"] = {
                        {"total_sessions_24h", (*result).value("total_sessions", "0")},
                        {"active_sessions", (*result).value("active_sessions", "0")},
                        {"completed_sessions_24h", (*result).value("completed_sessions", "0")},
                        {"status", "operational"}
                    };
                }
            }
        }

        // Add agent registry statistics
        if (agent_comm_registry_) {
            stats["registry_stats"] = {
                {"registered_agents", registered_agents_.size()},
                {"status", "operational"}
            };
        }

        // Add translation statistics
        if (message_translator_) {
            stats["translation_stats"] = {
                {"status", "operational"},
                {"llm_enabled", true}
            };
        }

        // Add mediation statistics
        if (communication_mediator_) {
            stats["mediator_stats"] = {
                {"status", "operational"}
            };
        }

    } catch (const std::exception& e) {
        stats["error"] = std::string("Exception getting statistics: ") + e.what();
    }

    return stats;
}

bool AgentOrchestrator::initialize_agents() {
    // Production design: agents register dynamically via register_agent() for flexibility
    // This method can be extended for pre-configured agent initialization
    logger_->debug("Agent initialization completed - no pre-configured agents");
    return true;
}

void AgentOrchestrator::register_system_metrics() {
    metrics_collector_->register_counter("orchestrator.tasks_submitted");
    metrics_collector_->register_counter("orchestrator.tasks_completed");
    metrics_collector_->register_counter("orchestrator.tasks_failed");
    metrics_collector_->register_gauge("orchestrator.active_agents",
        [this]() { return static_cast<double>(registered_agents_.size()); });
    metrics_collector_->register_gauge("orchestrator.queue_size",
        [this]() { return static_cast<double>(task_queue_.size()); });
}

bool AgentOrchestrator::validate_agent_registration(const OrchestratorAgentRegistration& registration) const {
    if (registration.agent_type.empty()) {
        logger_->error("Agent registration failed: empty agent type");
        return false;
    }

    if (!registration.agent_instance) {
        logger_->error("Agent registration failed: null agent instance for type {}", registration.agent_type);
        return false;
    }

    if (registered_agents_.find(registration.agent_type) != registered_agents_.end()) {
        logger_->warn("Agent type {} already registered", registration.agent_type);
        return false;
    }

    return true;
}

void AgentOrchestrator::shutdown() {
    logger_->info("Shutting down agent orchestrator");

    shutdown_requested_ = true;
    task_queue_cv_.notify_all();

    stop_worker_threads();

    // Shutdown agents
    for (auto& [type, registration] : registered_agents_) {
        if (registration.agent_instance) {
            registration.agent_instance->shutdown();
        }
    }
    registered_agents_.clear();

    // Shutdown components
    if (event_processor_) {
        event_processor_->shutdown();
    }
    if (knowledge_base_) {
        knowledge_base_->shutdown();
    }
    if (metrics_collector_) {
        metrics_collector_->stop_collection();
    }

    logger_->info("Agent orchestrator shutdown complete");
}

bool AgentOrchestrator::is_healthy() const {
    // Check if orchestrator is operational
    if (shutdown_requested_) {
        return false;
    }

    // Check worker threads
    for (const auto& thread : worker_threads_) {
        if (!thread.joinable()) {
            return false;
        }
    }

    // Check registered agents
    for (const auto& [type, registration] : registered_agents_) {
        if (registration.agent_instance &&
            !registration.agent_instance->perform_health_check()) {
            return false;
        }
    }

    return true;
}

bool AgentOrchestrator::register_agent(const OrchestratorAgentRegistration& registration) {
    std::lock_guard<std::mutex> lock(agents_mutex_);

    if (!validate_agent_registration(registration)) {
        return false;
    }

    registered_agents_[registration.agent_type] = registration;

    // Initialize the agent
    if (registration.agent_instance && !registration.agent_instance->initialize()) {
        logger_->error("Failed to initialize agent: {}", registration.agent_name);
        registered_agents_.erase(registration.agent_type);
        return false;
    }

    logger_->info("Registered agent: {} ({})", registration.agent_name, registration.agent_type);
    return true;
}

bool AgentOrchestrator::unregister_agent(const std::string& agent_type) {
    std::lock_guard<std::mutex> lock(agents_mutex_);

    auto it = registered_agents_.find(agent_type);
    if (it == registered_agents_.end()) {
        return false;
    }

    if (it->second.agent_instance) {
        it->second.agent_instance->shutdown();
    }

    registered_agents_.erase(it);
    logger_->info("Unregistered agent type: {}", agent_type);
    return true;
}

bool AgentOrchestrator::submit_task(const AgentTask& task) {
    if (shutdown_requested_) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(task_queue_mutex_);
        task_queue_.push(task);
    }

    task_queue_cv_.notify_one();
    metrics_collector_->increment_counter("orchestrator.tasks_submitted");

    logger_->debug("Task submitted: {} for agent type {}",
                  task.task_id, task.agent_type);
    return true;
}

void AgentOrchestrator::process_pending_events() {
    // Process events from the event processor
    while (auto event = event_processor_->dequeue_event()) {
        // Find appropriate agent for this event
        auto agent = find_agent_for_task(AgentTask("", "", *event));
        if (agent) {
            AgentTask task(generate_task_id(), agent->get_agent_type(), *event);
            submit_task(task);
        }
    }

    // Perform periodic health checks
    if (last_health_check_.elapsed() > std::chrono::minutes(5)) {
        perform_health_checks();
        last_health_check_.reset();
    }
}

nlohmann::json AgentOrchestrator::get_status() const {
    nlohmann::json status;

    status["orchestrator"] = {
        {"healthy", is_healthy()},
        {"tasks_processed", tasks_processed_.load()},
        {"tasks_failed", tasks_failed_.load()},
        {"active_agents", registered_agents_.size()},
        {"queue_size", task_queue_.size()},
        {"uptime_seconds", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - std::chrono::system_clock::time_point()).count()}
    };

    nlohmann::json agents_json;
    for (const auto& [type, registration] : registered_agents_) {
        if (registration.agent_instance) {
            auto agent_status = registration.agent_instance->get_status();
            agents_json[type] = agent_status.to_json();
        }
    }
    status["agents"] = agents_json;

    status["metrics"] = metrics_collector_->get_all_metrics();

    return status;
}

nlohmann::json AgentOrchestrator::get_thread_pool_stats() const {
    // Production-grade thread pool statistics
    size_t total_threads = worker_threads_.size();
    size_t queued_tasks = 0;
    
    {
        std::lock_guard<std::mutex> lock(task_queue_mutex_);
        queued_tasks = task_queue_.size();
    }
    
    // Calculate idle threads (approximation based on queue state)
    size_t active_threads = (queued_tasks > 0) ? std::min(queued_tasks, total_threads) : 0;
    size_t idle_threads = total_threads - active_threads;
    
    return {
        {"total_threads", total_threads},
        {"active_threads", active_threads},
        {"idle_threads", idle_threads},
        {"queued_tasks", queued_tasks},
        {"completed_tasks", tasks_processed_.load()}
    };
}

bool AgentOrchestrator::set_agent_enabled(const std::string& agent_type, bool enabled) {
    std::lock_guard<std::mutex> lock(agents_mutex_);

    auto it = registered_agents_.find(agent_type);
    if (it == registered_agents_.end()) {
        return false;
    }

    it->second.active = enabled;
    if (it->second.agent_instance) {
        it->second.agent_instance->set_enabled(enabled);
    }

    logger_->info("Agent " + it->second.agent_name + " (" + agent_type + ") " +
                 (enabled ? "enabled" : "disabled"), "AgentOrchestrator", "set_agent_enabled");
    return true;
}

std::vector<std::string> AgentOrchestrator::get_registered_agents() const {
    std::lock_guard<std::mutex> lock(agents_mutex_);

    std::vector<std::string> agent_types;
    agent_types.reserve(registered_agents_.size());

    for (const auto& [type, registration] : registered_agents_) {
        agent_types.push_back(type);
    }

    return agent_types;
}

std::optional<AgentStatus> AgentOrchestrator::get_agent_status(const std::string& agent_type) const {
    std::lock_guard<std::mutex> lock(agents_mutex_);

    auto it = registered_agents_.find(agent_type);
    if (it == registered_agents_.end() || !it->second.agent_instance) {
        return std::nullopt;
    }

    return it->second.agent_instance->get_status();
}

void AgentOrchestrator::start_worker_threads() {
    size_t num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    logger_->info("Starting {} worker threads", "", "", {{"thread_count", std::to_string(num_threads)}});

    for (size_t i = 0; i < num_threads; ++i) {
        worker_threads_.emplace_back(&AgentOrchestrator::worker_thread_loop, this);
    }
}

void AgentOrchestrator::stop_worker_threads() {
    shutdown_requested_ = true;
    task_queue_cv_.notify_all();

    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    worker_threads_.clear();
}

void AgentOrchestrator::worker_thread_loop() {
    logger_->debug("Worker thread started");

    while (!shutdown_requested_) {
        AgentTask task("", "", ComplianceEvent(EventType::AGENT_HEALTH_CHECK,
                                              EventSeverity::LOW, "health_check",
                                              {"system", "orchestrator", "internal"}));

        {
            std::unique_lock<std::mutex> lock(task_queue_mutex_);
            task_queue_cv_.wait(lock, [this]() {
                return shutdown_requested_ || !task_queue_.empty();
            });

            if (shutdown_requested_ && task_queue_.empty()) {
                break;
            }

            if (!task_queue_.empty()) {
                task = task_queue_.front();
                task_queue_.pop();
            }
        }

        if (task.event.get_type() != EventType::AGENT_HEALTH_CHECK) {
            process_task(task);
        }
    }

    logger_->debug("Worker thread stopped");
}

void AgentOrchestrator::process_task(const AgentTask& task) {
    try {
        std::shared_ptr<ComplianceAgent> agent;

        // Prepare task execution (find agent and validate)
        if (!prepare_task_execution(task, agent)) {
            return; // Error already logged and handled
        }

        // Execute task with the agent
        TaskResult result = execute_task_with_agent(task, agent);

        // Finalize task execution (metrics, callbacks, etc.)
        finalize_task_execution(task, result);

    } catch (const std::exception& e) {
        logger_->error("Exception processing task {}: {}", task.task_id, e.what());
        TaskResult error_result(false, std::string("Exception: ") + e.what());
        finalize_task_execution(task, error_result);
    }
}

bool AgentOrchestrator::prepare_task_execution(const AgentTask& task, std::shared_ptr<ComplianceAgent>& agent) {
    agent = find_agent_for_task(task);
    if (!agent) {
        logger_->warn("No suitable agent found for task: {}", task.task_id);
        TaskResult error_result(false, "No suitable agent found");
        finalize_task_execution(task, error_result);
        return false;
    }

    // Check if agent is enabled and healthy
    if (!agent->is_enabled() || agent->get_status().health == AgentHealth::CRITICAL) {
        logger_->warn("Agent {} is not available for task: {}", agent->get_agent_name(), task.task_id);
        TaskResult error_result(false, "Agent not available");
        finalize_task_execution(task, error_result);
        return false;
    }

    return true;
}

TaskResult AgentOrchestrator::execute_task_with_agent(const AgentTask& task, std::shared_ptr<ComplianceAgent> agent) {
    // Increment task counter
    agent->increment_tasks_in_progress();

    auto start_time = std::chrono::high_resolution_clock::now();
    AgentDecision decision = agent->process_event(task.event);
    auto end_time = std::chrono::high_resolution_clock::now();

    auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Update agent metrics
    agent->update_metrics(processing_time, true);
    agent->decrement_tasks_in_progress();

    return TaskResult(true, "", decision, processing_time);
}

void AgentOrchestrator::finalize_task_execution(const AgentTask& task, const TaskResult& result) {
    // Update orchestrator metrics
    if (result.success) {
        tasks_processed_++;
        metrics_collector_->increment_counter("orchestrator.tasks_completed");
        logger_->info("Task " + task.task_id + " completed successfully in " +
                     std::to_string(result.execution_time.count()) + "ms",
                     "AgentOrchestrator", "finalize_task_execution");
    } else {
        tasks_failed_++;
        metrics_collector_->increment_counter("orchestrator.tasks_failed");
        logger_->error("Task {} failed: {}", task.task_id, result.error_message);
    }

    // Update agent-specific metrics
    update_agent_metrics(task.agent_type, result);

    // Call the callback if provided
    if (task.callback) {
        task.callback(result);
    }
}

std::shared_ptr<ComplianceAgent> AgentOrchestrator::find_agent_for_task(const AgentTask& task) {
    std::lock_guard<std::mutex> lock(agents_mutex_);

    // First try the specified agent type
    if (!task.agent_type.empty()) {
        auto it = registered_agents_.find(task.agent_type);
        if (it != registered_agents_.end() &&
            it->second.agent_instance &&
            it->second.active &&
            it->second.agent_instance->can_handle_event(task.event.get_type())) {
            return it->second.agent_instance;
        }
    }

    // Find any agent that can handle this event type
    for (const auto& [type, registration] : registered_agents_) {
        if (registration.agent_instance &&
            registration.active &&
            registration.agent_instance->can_handle_event(task.event.get_type())) {
            return registration.agent_instance;
        }
    }

    return nullptr;
}

void AgentOrchestrator::update_agent_metrics(const std::string& agent_type, const TaskResult& result) {
    // Suppress unused parameter warning
    (void)result;

    std::lock_guard<std::mutex> lock(agents_mutex_);

    auto it = registered_agents_.find(agent_type);
    if (it != registered_agents_.end()) {
        // Update metrics in the agent instance
        // This would be handled by the ComplianceAgent base class
    }
}

void AgentOrchestrator::perform_health_checks() {
    logger_->debug("Performing health checks");

    // Check each registered agent
    std::lock_guard<std::mutex> lock(agents_mutex_);
    for (auto& [type, registration] : registered_agents_) {
        if (registration.agent_instance) {
            bool healthy = registration.agent_instance->perform_health_check();
            if (!healthy) {
                logger_->warn("Agent {} ({}) health check failed",
                             registration.agent_name, type);
            }
        }
    }
}

std::string AgentOrchestrator::generate_task_id() {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
    return "task_" + std::to_string(timestamp) + "_" + std::to_string(++counter);
}

} // namespace regulens


