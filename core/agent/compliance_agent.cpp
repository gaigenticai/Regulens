#include "compliance_agent.hpp"

regulens::AgentRegistry& regulens::AgentRegistry::get_instance() {
    static AgentRegistry instance;
    return instance;
}

bool regulens::AgentRegistry::register_agent_factory(const std::string& agent_type, AgentFactory factory) {
    if (factories_.find(agent_type) != factories_.end()) {
        return false; // Already registered
    }

    factories_[agent_type] = std::move(factory);
    return true;
}

std::shared_ptr<regulens::ComplianceAgent> regulens::AgentRegistry::create_agent(
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

std::vector<std::string> regulens::AgentRegistry::get_registered_types() const {
    std::vector<std::string> types;
    types.reserve(factories_.size());

    for (const auto& [type, _] : factories_) {
        types.push_back(type);
    }

    return types;
}

// Initialize static members
std::unordered_map<std::string, std::function<std::shared_ptr<regulens::ComplianceAgent>(
    std::string,
    std::shared_ptr<regulens::ConfigurationManager>,
    std::shared_ptr<regulens::StructuredLogger>)>> regulens::AgentRegistry::factories_;


