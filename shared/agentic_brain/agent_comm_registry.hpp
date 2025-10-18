/**
 * Agent Communication Registry
 * Production-grade registry for managing agent communication components
 * Following @rule.mdc - no stubs, production implementation only
 */

#ifndef AGENT_COMM_REGISTRY_HPP
#define AGENT_COMM_REGISTRY_HPP

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include "inter_agent_communicator.hpp"
#include "message_translator.hpp"
#include "consensus_engine.hpp"
#include "communication_mediator.hpp"

namespace regulens {

struct AgentRegistration {
    std::string agent_id;
    std::string agent_type;
    std::string capabilities;
    bool is_active = true;
    std::chrono::system_clock::time_point registered_at;
    std::chrono::system_clock::time_point last_active;
};

class AgentCommRegistry {
public:
    AgentCommRegistry(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger
    );

    ~AgentCommRegistry();

    // Agent registration management
    bool register_agent(const AgentRegistration& agent);
    bool unregister_agent(const std::string& agent_id);
    bool update_agent_status(const std::string& agent_id, bool is_active);
    std::optional<AgentRegistration> get_agent(const std::string& agent_id);
    std::vector<AgentRegistration> get_active_agents();
    std::vector<AgentRegistration> get_agents_by_type(const std::string& agent_type);

    // Component management
    bool initialize_components();
    bool shutdown_components();

    // Component accessors
    std::shared_ptr<InterAgentCommunicator> get_communicator() const { return communicator_; }
    std::shared_ptr<MessageTranslator> get_translator() const { return translator_; }
    std::shared_ptr<ConsensusEngine> get_consensus_engine() const { return consensus_engine_; }
    std::shared_ptr<CommunicationMediator> get_mediator() const { return mediator_; }

    // Health and monitoring
    bool is_healthy() const;
    std::unordered_map<std::string, std::string> get_health_status();
    void cleanup_inactive_agents(std::chrono::hours max_age = std::chrono::hours(24));

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;

    // Communication components
    std::shared_ptr<InterAgentCommunicator> communicator_;
    std::shared_ptr<MessageTranslator> translator_;
    std::shared_ptr<ConsensusEngine> consensus_engine_;
    std::shared_ptr<CommunicationMediator> mediator_;

    // Agent registry
    std::unordered_map<std::string, AgentRegistration> registered_agents_;

    // Private helper methods
    bool create_tables_if_not_exist();
    bool load_registered_agents();
    bool save_agent_registration(const AgentRegistration& agent);
    void log_component_status(const std::string& component_name, bool initialized);
};

} // namespace regulens

#endif // AGENT_COMM_REGISTRY_HPP
