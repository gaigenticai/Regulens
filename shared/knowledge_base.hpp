#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "shared/logging/structured_logger.hpp"
#include "shared/config/configuration_manager.hpp"

namespace regulens {

/**
 * @brief Knowledge base for storing and retrieving compliance information
 */
class KnowledgeBase {
public:
    KnowledgeBase(std::shared_ptr<ConfigurationManager> config,
                 std::shared_ptr<StructuredLogger> logger);
    ~KnowledgeBase();

    bool initialize();
    void shutdown();

    bool store_information(const std::string& key, const std::string& value);
    std::optional<std::string> retrieve_information(const std::string& key) const;
    std::vector<std::string> search_similar(const std::string& query, size_t limit = 10) const;

private:
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<StructuredLogger> logger_;
    std::unordered_map<std::string, std::string> knowledge_store_;
};

} // namespace regulens
