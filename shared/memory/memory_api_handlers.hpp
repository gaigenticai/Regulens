/**
 * Memory Management API Handlers - Header File
 * Production-grade memory visualization and search endpoint declarations
 * Implements graph visualization for agent memory and memory search
 */

#ifndef REGULENS_MEMORY_API_HANDLERS_HPP
#define REGULENS_MEMORY_API_HANDLERS_HPP

#include <string>
#include <map>
#include <libpq-fe.h>

namespace regulens {
namespace memory {

// Memory Visualization
std::string generate_graph_visualization(PGconn* db_conn, const std::string& request_body);
std::string get_memory_graph(PGconn* db_conn, const std::map<std::string, std::string>& query_params);
std::string get_memory_node_details(PGconn* db_conn, const std::string& node_id);

// Memory Search
std::string search_memory(PGconn* db_conn, const std::string& request_body);
std::string get_memory_relationships(PGconn* db_conn, const std::string& node_id, const std::map<std::string, std::string>& query_params);

// Memory Analytics
std::string get_memory_stats(PGconn* db_conn, const std::map<std::string, std::string>& query_params);
std::string get_memory_clusters(PGconn* db_conn, const std::map<std::string, std::string>& query_params);

// Memory CRUD
std::string create_memory_node(PGconn* db_conn, const std::string& request_body, const std::string& user_id);
std::string update_memory_node(PGconn* db_conn, const std::string& node_id, const std::string& request_body);
std::string delete_memory_node(PGconn* db_conn, const std::string& node_id);

// Memory Relationships
std::string create_memory_relationship(PGconn* db_conn, const std::string& request_body, const std::string& user_id);
std::string update_memory_relationship(PGconn* db_conn, const std::string& relationship_id, const std::string& request_body);
std::string delete_memory_relationship(PGconn* db_conn, const std::string& relationship_id);

// Helper functions
std::string generate_graph_data(PGconn* db_conn, const std::string& agent_id, const std::string& visualization_type);
std::string calculate_memory_importance(PGconn* db_conn, const std::string& node_id);
std::vector<std::string> find_memory_path(PGconn* db_conn, const std::string& source_id, const std::string& target_id);

} // namespace memory
} // namespace regulens

#endif // REGULENS_MEMORY_API_HANDLERS_HPP
