#include "knowledge_base.hpp"

namespace regulens {

KnowledgeBase::KnowledgeBase(std::shared_ptr<ConfigurationManager> config,
                           std::shared_ptr<StructuredLogger> logger)
    : config_(config), logger_(logger) {}

KnowledgeBase::~KnowledgeBase() {
    shutdown();
}

bool KnowledgeBase::initialize() {
    logger_->info("Knowledge base initialized");
    return true;
}

void KnowledgeBase::shutdown() {
    knowledge_store_.clear();
    logger_->info("Knowledge base shutdown");
}

bool KnowledgeBase::store_information(const std::string& key, const std::string& value) {
    knowledge_store_[key] = value;
    return true;
}

std::optional<std::string> KnowledgeBase::retrieve_information(const std::string& key) const {
    auto it = knowledge_store_.find(key);
    if (it != knowledge_store_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::string> KnowledgeBase::search_similar(const std::string& query, size_t limit) const {
    std::vector<std::string> results;

    // Simple substring search for now
    for (const auto& [key, value] : knowledge_store_) {
        if (key.find(query) != std::string::npos ||
            value.find(query) != std::string::npos) {
            results.push_back(key);
            if (results.size() >= limit) break;
        }
    }

    return results;
}

} // namespace regulens


