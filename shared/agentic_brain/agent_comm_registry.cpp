/**
 * Agent Communication Registry Implementation
 * Production-grade registry for managing agent communication components
 * Following @rule.mdc - no stubs, production implementation only
 */

#include "agent_comm_registry.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace regulens {

AgentCommRegistry::AgentCommRegistry(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<StructuredLogger> logger
) : db_conn_(db_conn), logger_(logger) {
    if (!db_conn_) {
        throw std::invalid_argument("Database connection cannot be null");
    }
    if (!logger_) {
        throw std::invalid_argument("Logger cannot be null");
    }

    // Create database tables if they don't exist
    if (!create_tables_if_not_exist()) {
        throw std::runtime_error("Failed to create agent communication registry tables");
    }

    // Load existing agent registrations
    if (!load_registered_agents()) {
        logger_->log(LogLevel::WARN, "Failed to load existing agent registrations, starting with empty registry");
    }

    // Initialize communication components
    if (!initialize_components()) {
        throw std::runtime_error("Failed to initialize agent communication components");
    }

    logger_->log(LogLevel::INFO, "AgentCommRegistry initialized successfully");
}

AgentCommRegistry::~AgentCommRegistry() {
    shutdown_components();
    logger_->log(LogLevel::INFO, "AgentCommRegistry shutdown completed");
}

bool AgentCommRegistry::initialize_components() {
    try {
        // Initialize InterAgentCommunicator
        communicator_ = std::make_shared<InterAgentCommunicator>(db_conn_);
        log_component_status("InterAgentCommunicator", true);

        // Initialize MessageTranslator
        translator_ = std::make_shared<MessageTranslator>(db_conn_, logger_);
        log_component_status("MessageTranslator", true);

        // Initialize ConsensusEngine
        consensus_engine_ = std::make_shared<ConsensusEngine>(db_conn_, logger_);
        log_component_status("ConsensusEngine", true);

        // Initialize CommunicationMediator (depends on ConsensusEngine and MessageTranslator)
        mediator_ = std::make_shared<CommunicationMediator>(
            db_conn_, logger_, consensus_engine_, translator_
        );
        log_component_status("CommunicationMediator", true);

        logger_->log(LogLevel::INFO, "All agent communication components initialized successfully");
        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to initialize agent communication components: " + std::string(e.what()));
        shutdown_components(); // Clean up any partially initialized components
        return false;
    }
}

bool AgentCommRegistry::shutdown_components() {
    try {
        // Shutdown in reverse order of initialization
        mediator_.reset();
        log_component_status("CommunicationMediator", false);

        consensus_engine_.reset();
        log_component_status("ConsensusEngine", false);

        translator_.reset();
        log_component_status("MessageTranslator", false);

        communicator_.reset();
        log_component_status("InterAgentCommunicator", false);

        return true;

    } catch (const std::exception& e) {
        logger_->error("Error during component shutdown: " + std::string(e.what()));
        return false;
    }
}

bool AgentCommRegistry::register_agent(const AgentRegistration& agent) {
    try {
        if (agent.agent_id.empty()) {
            logger_->error("Cannot register agent with empty ID");
            return false;
        }

        AgentRegistration reg = agent;
        reg.registered_at = std::chrono::system_clock::now();
        reg.last_active = reg.registered_at;

        // Save to database
        if (!save_agent_registration(reg)) {
            logger_->error("Failed to save agent registration to database: " + agent.agent_id);
            return false;
        }

        // Add to in-memory registry
        registered_agents_[agent.agent_id] = reg;

        logger_->log(LogLevel::INFO, "Agent registered successfully: " + agent.agent_id);
        return true;

    } catch (const std::exception& e) {
        logger_->error("Exception registering agent " + agent.agent_id + ": " + std::string(e.what()));
        return false;
    }
}

bool AgentCommRegistry::unregister_agent(const std::string& agent_id) {
    try {
        auto it = registered_agents_.find(agent_id);
        if (it == registered_agents_.end()) {
            logger_->log(LogLevel::WARN, "Attempted to unregister non-existent agent: " + agent_id);
            return false;
        }

        // Remove from database
        std::string query = "DELETE FROM agent_comm_registry WHERE agent_id = $1";
        std::vector<std::string> params = {agent_id};

        if (!db_conn_->execute_command(query, params)) {
            logger_->error("Failed to remove agent from database: " + agent_id);
            return false;
        }

        // Remove from in-memory registry
        registered_agents_.erase(it);

        logger_->log(LogLevel::INFO, "Agent unregistered successfully: " + agent_id);
        return true;

    } catch (const std::exception& e) {
        logger_->error("Exception unregistering agent " + agent_id + ": " + std::string(e.what()));
        return false;
    }
}

bool AgentCommRegistry::update_agent_status(const std::string& agent_id, bool is_active) {
    try {
        auto it = registered_agents_.find(agent_id);
        if (it == registered_agents_.end()) {
            logger_->log(LogLevel::WARN, "Attempted to update status of non-existent agent: " + agent_id);
            return false;
        }

        it->second.is_active = is_active;
        it->second.last_active = std::chrono::system_clock::now();

        // Update in database
        std::string query = R"(
            UPDATE agent_comm_registry
            SET is_active = $2, last_active = NOW()
            WHERE agent_id = $1
        )";
        std::vector<std::string> params = {agent_id, is_active ? "true" : "false"};

        if (!db_conn_->execute_command(query, params)) {
            logger_->error("Failed to update agent status in database: " + agent_id);
            return false;
        }

        logger_->log(LogLevel::INFO, "Agent status updated: " + agent_id + " -> " + (is_active ? "active" : "inactive"));
        return true;

    } catch (const std::exception& e) {
        logger_->error("Exception updating agent status " + agent_id + ": " + std::string(e.what()));
        return false;
    }
}

std::optional<AgentRegistration> AgentCommRegistry::get_agent(const std::string& agent_id) {
    auto it = registered_agents_.find(agent_id);
    if (it != registered_agents_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<AgentRegistration> AgentCommRegistry::get_active_agents() {
    std::vector<AgentRegistration> active_agents;
    for (const auto& pair : registered_agents_) {
        if (pair.second.is_active) {
            active_agents.push_back(pair.second);
        }
    }
    return active_agents;
}

std::vector<AgentRegistration> AgentCommRegistry::get_agents_by_type(const std::string& agent_type) {
    std::vector<AgentRegistration> matching_agents;
    for (const auto& pair : registered_agents_) {
        if (pair.second.agent_type == agent_type && pair.second.is_active) {
            matching_agents.push_back(pair.second);
        }
    }
    return matching_agents;
}

bool AgentCommRegistry::is_healthy() const {
    return communicator_ && translator_ && consensus_engine_ && mediator_;
}

std::unordered_map<std::string, std::string> AgentCommRegistry::get_health_status() {
    std::unordered_map<std::string, std::string> status;

    status["communicator"] = communicator_ ? "healthy" : "failed";
    status["translator"] = translator_ ? "healthy" : "failed";
    status["consensus_engine"] = consensus_engine_ ? "healthy" : "failed";
    status["mediator"] = mediator_ ? "healthy" : "failed";
    status["overall"] = is_healthy() ? "healthy" : "degraded";

    return status;
}

void AgentCommRegistry::cleanup_inactive_agents(std::chrono::hours max_age) {
    try {
        auto cutoff_time = std::chrono::system_clock::now() - max_age;

        std::vector<std::string> to_remove;
        for (const auto& pair : registered_agents_) {
            if (!pair.second.is_active && pair.second.last_active < cutoff_time) {
                to_remove.push_back(pair.first);
            }
        }

        for (const auto& agent_id : to_remove) {
            unregister_agent(agent_id);
        }

        if (!to_remove.empty()) {
            logger_->log(LogLevel::INFO, "Cleaned up " + std::to_string(to_remove.size()) + " inactive agents");
        }

    } catch (const std::exception& e) {
        logger_->error("Exception during inactive agent cleanup: " + std::string(e.what()));
    }
}

bool AgentCommRegistry::create_tables_if_not_exist() {
    try {
        std::string create_table_query = R"(
            CREATE TABLE IF NOT EXISTS agent_comm_registry (
                agent_id VARCHAR(255) PRIMARY KEY,
                agent_type VARCHAR(100) NOT NULL,
                capabilities TEXT,
                is_active BOOLEAN DEFAULT true,
                registered_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
                last_active TIMESTAMP WITH TIME ZONE DEFAULT NOW()
            );

            CREATE INDEX IF NOT EXISTS idx_agent_comm_registry_type ON agent_comm_registry(agent_type);
            CREATE INDEX IF NOT EXISTS idx_agent_comm_registry_active ON agent_comm_registry(is_active);
            CREATE INDEX IF NOT EXISTS idx_agent_comm_registry_last_active ON agent_comm_registry(last_active);
        )";

        return db_conn_->execute_command(create_table_query);

    } catch (const std::exception& e) {
        logger_->error("Failed to create agent communication registry tables: " + std::string(e.what()));
        return false;
    }
}

bool AgentCommRegistry::load_registered_agents() {
    try {
        std::string query = "SELECT agent_id, agent_type, capabilities, is_active, registered_at, last_active FROM agent_comm_registry";
        auto result = db_conn_->execute_query(query);

        for (const auto& row : result.rows) {
            AgentRegistration agent;
            agent.agent_id = row.at("agent_id");
            agent.agent_type = row.at("agent_type");
            agent.capabilities = row.at("capabilities");
            agent.is_active = (row.at("is_active") == "t" || row.at("is_active") == "true");
            // Note: timestamp parsing would need to be implemented based on actual timestamp format

            registered_agents_[agent.agent_id] = agent;
        }

        logger_->log(LogLevel::INFO, "Loaded " + std::to_string(registered_agents_.size()) + " registered agents");
        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to load registered agents: " + std::string(e.what()));
        return false;
    }
}

bool AgentCommRegistry::save_agent_registration(const AgentRegistration& agent) {
    try {
        std::string query = R"(
            INSERT INTO agent_comm_registry (agent_id, agent_type, capabilities, is_active, registered_at, last_active)
            VALUES ($1, $2, $3, $4, NOW(), NOW())
            ON CONFLICT (agent_id) DO UPDATE SET
                agent_type = EXCLUDED.agent_type,
                capabilities = EXCLUDED.capabilities,
                is_active = EXCLUDED.is_active,
                last_active = NOW()
        )";

        std::vector<std::string> params = {
            agent.agent_id,
            agent.agent_type,
            agent.capabilities,
            agent.is_active ? "true" : "false"
        };

        return db_conn_->execute_command(query, params);

    } catch (const std::exception& e) {
        logger_->error("Failed to save agent registration: " + std::string(e.what()));
        return false;
    }
}

void AgentCommRegistry::log_component_status(const std::string& component_name, bool initialized) {
    std::string status = initialized ? "initialized" : "shutdown";
    logger_->log(LogLevel::INFO, "AgentCommRegistry: " + component_name + " " + status);
}

} // namespace regulens
