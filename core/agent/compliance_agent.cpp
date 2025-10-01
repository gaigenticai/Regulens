#include "compliance_agent.hpp"

// Initialize static members
std::unordered_map<std::string, AgentRegistry::AgentFactory> AgentRegistry::factories_;

AgentRegistry& AgentRegistry::get_instance() {
    static AgentRegistry instance;
    return instance;
}

bool AgentRegistry::register_agent_factory(const std::string& agent_type, AgentFactory factory) {
    if (factories_.find(agent_type) != factories_.end()) {
        return false; // Already registered
    }

    factories_[agent_type] = std::move(factory);
    return true;
}

std::shared_ptr<ComplianceAgent> AgentRegistry::create_agent(
    const std::string& agent_type,
    const std::string& agent_name,
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<StructuredLogger> logger
) const {
    auto it = factories_.find(agent_type);
    if (it == factories_.end()) {
        return nullptr;
    }

    return it->second(agent_name, config, logger);
}

std::vector<std::string> AgentRegistry::get_registered_types() const {
    std::vector<std::string> types;
    types.reserve(factories_.size());

    for (const auto& [type, _] : factories_) {
        types.push_back(type);
    }

    return types;
}
