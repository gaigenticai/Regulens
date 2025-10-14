/**
 * Memory API Handlers
 * REST API endpoints for memory management and visualization
 */

#ifndef REGULENS_MEMORY_API_HANDLERS_HPP
#define REGULENS_MEMORY_API_HANDLERS_HPP

#include <string>
#include <map>
#include <memory>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"
#include "memory_visualizer.hpp"

namespace regulens {
namespace memory {

class MemoryAPIHandlers {
public:
    MemoryAPIHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger,
        std::shared_ptr<MemoryVisualizer> memory_visualizer
    );

    // Memory visualization endpoints
    std::string handle_get_memory_graph(const std::string& agent_id, const std::string& user_id, const std::map<std::string, std::string>& query_params);
    std::string handle_get_memory_nodes(const std::string& agent_id, const std::string& user_id, const std::map<std::string, std::string>& query_params);
    std::string handle_get_memory_edges(const std::string& agent_id, const std::string& user_id, const std::map<std::string, std::string>& query_params);

    // Memory search endpoint
    std::string handle_search_memory(const std::string& agent_id, const std::string& user_id, const std::string& request_body);

    // Memory consolidation endpoint
    std::string handle_consolidate_memory(const std::string& agent_id, const std::string& user_id, const std::string& request_body);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;
    std::shared_ptr<MemoryVisualizer> memory_visualizer_;

    // Helper methods
    bool validate_agent_access(const std::string& agent_id, const std::string& user_id);
    nlohmann::json parse_visualization_parameters(const std::map<std::string, std::string>& query_params);
};

} // namespace memory
} // namespace regulens

#endif // REGULENS_MEMORY_API_HANDLERS_HPP
