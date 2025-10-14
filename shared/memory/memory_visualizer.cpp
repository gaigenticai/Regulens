/**
 * Memory Visualizer Implementation
 * Production-grade graph visualization for agent memory management
 */

#include "memory_visualizer.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <random>
#include <uuid/uuid.h>
#include <spdlog/spdlog.h>

namespace regulens {
namespace memory {

MemoryVisualizer::MemoryVisualizer(
    std::shared_ptr<PostgreSQLConnection> db_conn,
    std::shared_ptr<StructuredLogger> logger
) : db_conn_(db_conn), logger_(logger) {

    if (!db_conn_) {
        throw std::runtime_error("Database connection is required for MemoryVisualizer");
    }
    if (!logger_) {
        throw std::runtime_error("Logger is required for MemoryVisualizer");
    }

    logger_->log(LogLevel::INFO, "MemoryVisualizer initialized with graph visualization capabilities");
}

MemoryVisualizer::~MemoryVisualizer() {
    logger_->log(LogLevel::INFO, "MemoryVisualizer shutting down");
}

VisualizationResponse MemoryVisualizer::generate_graph_visualization(const VisualizationRequest& request) {
    try {
        VisualizationResponse response;
        response.visualization_type = "graph";

        // Check cache first
        if (request.use_cache && cache_enabled_) {
            std::string cache_key = generate_cache_key(request);
            auto cached_data = get_cached_visualization(cache_key, request.agent_id);
            if (cached_data) {
                response.data = *cached_data;
                response.from_cache = true;
                response.cache_key = cache_key;
                response.generated_at = std::chrono::system_clock::now();
                return response;
            }
        }

        // Generate visualization data
        GraphVisualizationData graph_data = build_memory_graph(request.agent_id, request.parameters);

        // Format for frontend consumption
        response.data = export_graph_data(graph_data, "d3_force");
        response.generated_at = std::chrono::system_clock::now();

        // Cache the result
        if (cache_enabled_) {
            std::string cache_key = generate_cache_key(request);
            cache_visualization_data(cache_key, request.agent_id, "graph", response.data);
            response.cache_key = cache_key;
        }

        logger_->log(LogLevel::INFO, "Generated graph visualization for agent " + request.agent_id +
                    " with " + std::to_string(graph_data.nodes.size()) + " nodes and " +
                    std::to_string(graph_data.edges.size()) + " edges");

        return response;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in generate_graph_visualization: " + std::string(e.what()));
        throw;
    }
}

GraphVisualizationData MemoryVisualizer::build_memory_graph(const std::string& agent_id, const nlohmann::json& parameters) {
    GraphVisualizationData graph_data;

    try {
        // Extract memory nodes
        std::vector<std::string> filtered_memory_ids;
        if (parameters.contains("memory_ids") && parameters["memory_ids"].is_array()) {
            for (const auto& id : parameters["memory_ids"]) {
                filtered_memory_ids.push_back(id);
            }
        }

        graph_data.nodes = extract_memory_nodes(agent_id, parameters);

        // Limit nodes if specified
        if (graph_data.nodes.size() > max_visualization_nodes_) {
            graph_data.nodes.resize(max_visualization_nodes_);
            // Update filtered_memory_ids to match
            filtered_memory_ids.clear();
            for (const auto& node : graph_data.nodes) {
                filtered_memory_ids.push_back(node.memory_id);
            }
        }

        // Extract relationships for these nodes
        graph_data.edges = extract_memory_relationships(agent_id, filtered_memory_ids);

        // Limit edges if specified
        if (graph_data.edges.size() > max_visualization_edges_) {
            // Sort by strength and keep strongest
            std::sort(graph_data.edges.begin(), graph_data.edges.end(),
                     [](const MemoryEdge& a, const MemoryEdge& b) {
                         return a.strength > b.strength;
                     });
            graph_data.edges.resize(max_visualization_edges_);
        }

        // Apply layout algorithm
        std::string layout_algorithm = parameters.value("layout", "force_directed");
        graph_data.layout_config = apply_graph_layout(graph_data.nodes, graph_data.edges, layout_algorithm);

        // Set styling configuration
        graph_data.styling_config = {
            {"node_colors", {
                {"episodic", "#FF6B6B"},
                {"semantic", "#4ECDC4"},
                {"procedural", "#45B7D1"}
            }},
            {"edge_colors", {
                {"causes", "#FF6B6B"},
                {"relates_to", "#4ECDC4"},
                {"derived_from", "#45B7D1"},
                {"supports", "#96CEB4"},
                {"contradicts", "#FECA57"}
            }},
            {"min_node_size", 10},
            {"max_node_size", 50},
            {"min_edge_width", 1},
            {"max_edge_width", 5}
        };

        graph_data.generated_at = std::chrono::system_clock::now();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in build_memory_graph: " + std::string(e.what()));
        // Return empty graph on error
    }

    return graph_data;
}

std::vector<MemoryNode> MemoryVisualizer::extract_memory_nodes(const std::string& agent_id, const nlohmann::json& filters) {
    std::vector<MemoryNode> nodes;

    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return nodes;

        // Build query with filters
        std::string query = R"(
            SELECT memory_id, title, content, memory_type, strength, access_count,
                   created_at, last_accessed, tags, metadata
            FROM agent_memory
            WHERE agent_id = $1
        )";

        std::vector<const char*> params = {agent_id.c_str()};
        int param_count = 1;

        // Add filter conditions
        if (filters.contains("memory_type")) {
            query += " AND memory_type = $" + std::to_string(++param_count);
            params.push_back(filters["memory_type"].get<std::string>().c_str());
        }

        if (filters.contains("min_strength")) {
            query += " AND strength >= $" + std::to_string(++param_count);
            std::string min_strength = std::to_string(filters["min_strength"].get<double>());
            params.push_back(min_strength.c_str());
        }

        if (filters.contains("max_age_days")) {
            query += " AND created_at >= NOW() - INTERVAL '" +
                    std::to_string(filters["max_age_days"].get<int>()) + " days'";
        }

        if (filters.contains("tags") && filters["tags"].is_array()) {
            // Complex tag filtering would require more sophisticated query
            // Simplified version here
        }

        query += " ORDER BY strength DESC, last_accessed DESC LIMIT " + std::to_string(max_visualization_nodes_);

        PGresult* result = PQexecParams(
            conn, query.c_str(), param_count, nullptr, params.data(), nullptr, nullptr, 0
        );

        if (PQresultStatus(result) == PGRES_TUPLES_OK) {
            int rows = PQntuples(result);
            for (int i = 0; i < rows; ++i) {
                MemoryNode node;
                node.memory_id = PQgetvalue(result, i, 0) ? PQgetvalue(result, i, 0) : "";
                node.title = PQgetvalue(result, i, 1) ? PQgetvalue(result, i, 1) : "";
                node.content = PQgetvalue(result, i, 2) ? PQgetvalue(result, i, 2) : "";
                node.memory_type = PQgetvalue(result, i, 3) ? PQgetvalue(result, i, 3) : "episodic";
                node.strength = PQgetvalue(result, i, 4) ? std::atof(PQgetvalue(result, i, 4)) : 0.5;
                node.access_count = PQgetvalue(result, i, 5) ? std::atoi(PQgetvalue(result, i, 5)) : 0;

                // Parse tags if available
                if (PQgetvalue(result, i, 8)) {
                    try {
                        nlohmann::json tags_json = nlohmann::json::parse(PQgetvalue(result, i, 8));
                        if (tags_json.is_array()) {
                            for (const auto& tag : tags_json) {
                                node.tags.push_back(tag);
                            }
                        }
                    } catch (const std::exception&) {
                        // Ignore parsing errors
                    }
                }

                // Parse metadata if available
                if (PQgetvalue(result, i, 9)) {
                    try {
                        node.metadata = nlohmann::json::parse(PQgetvalue(result, i, 9));
                    } catch (const std::exception&) {
                        // Ignore parsing errors
                    }
                }

                // Set visualization properties
                node.visualization_properties = build_node_visualization_properties(node);

                nodes.push_back(node);
            }
        }

        PQclear(result);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in extract_memory_nodes: " + std::string(e.what()));
    }

    return nodes;
}

std::vector<MemoryEdge> MemoryVisualizer::extract_memory_relationships(const std::string& agent_id, const std::vector<std::string>& node_ids) {
    std::vector<MemoryEdge> edges;

    if (node_ids.empty()) return edges;

    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return edges;

        // Build IN clause for node IDs
        std::string in_clause;
        std::vector<const char*> params;
        params.push_back(agent_id.c_str()); // $1 is agent_id

        for (size_t i = 0; i < node_ids.size(); ++i) {
            if (i > 0) in_clause += ",";
            in_clause += "$" + std::to_string(params.size() + 1);
            params.push_back(node_ids[i].c_str());
        }

        std::string query = R"(
            SELECT relationship_id, source_memory_id, target_memory_id, relationship_type,
                   strength, confidence, bidirectional, context, metadata
            FROM memory_relationships
            WHERE (source_memory_id IN ()" + in_clause + R"() OR target_memory_id IN ()" + in_clause + R"())
            AND strength > 0.1
            ORDER BY strength DESC
            LIMIT )" + std::to_string(max_visualization_edges_);

        PGresult* result = PQexecParams(
            conn, query.c_str(), params.size(), nullptr, params.data(), nullptr, nullptr, 0
        );

        if (PQresultStatus(result) == PGRES_TUPLES_OK) {
            int rows = PQntuples(result);
            for (int i = 0; i < rows; ++i) {
                MemoryEdge edge;
                edge.relationship_id = PQgetvalue(result, i, 0) ? PQgetvalue(result, i, 0) : "";
                edge.source_id = PQgetvalue(result, i, 1) ? PQgetvalue(result, i, 1) : "";
                edge.target_id = PQgetvalue(result, i, 2) ? PQgetvalue(result, i, 2) : "";
                edge.relationship_type = PQgetvalue(result, i, 3) ? PQgetvalue(result, i, 3) : "relates_to";
                edge.strength = PQgetvalue(result, i, 4) ? std::atof(PQgetvalue(result, i, 4)) : 0.5;
                edge.confidence = PQgetvalue(result, i, 5) ? std::atof(PQgetvalue(result, i, 5)) : 1.0;
                edge.bidirectional = std::string(PQgetvalue(result, i, 6)) == "t";

                if (PQgetvalue(result, i, 7)) {
                    edge.context = PQgetvalue(result, i, 7);
                }

                if (PQgetvalue(result, i, 8)) {
                    try {
                        edge.metadata = nlohmann::json::parse(PQgetvalue(result, i, 8));
                    } catch (const std::exception&) {
                        // Ignore parsing errors
                    }
                }

                // Only include edges where both nodes exist in our node set
                if (std::find(node_ids.begin(), node_ids.end(), edge.source_id) != node_ids.end() &&
                    std::find(node_ids.begin(), node_ids.end(), edge.target_id) != node_ids.end()) {

                    edge.visualization_properties = build_edge_visualization_properties(edge);
                    edges.push_back(edge);
                }
            }
        }

        PQclear(result);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in extract_memory_relationships: " + std::string(e.what()));
    }

    return edges;
}

nlohmann::json MemoryVisualizer::apply_graph_layout(const std::vector<MemoryNode>& nodes, const std::vector<MemoryEdge>& edges, const std::string& layout_algorithm) {
    if (layout_algorithm == "force_directed") {
        return apply_force_directed_layout(nodes, edges);
    } else if (layout_algorithm == "hierarchical") {
        return apply_hierarchical_layout(nodes, edges);
    } else if (layout_algorithm == "circular") {
        return apply_circular_layout(nodes);
    } else {
        return apply_force_directed_layout(nodes, edges); // Default
    }
}

nlohmann::json MemoryVisualizer::apply_force_directed_layout(const std::vector<MemoryNode>& nodes, const std::vector<MemoryEdge>& edges) {
    nlohmann::json layout;

    // Simple force-directed layout implementation
    // In a real implementation, this would use a proper force-directed algorithm

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-500, 500);

    layout["nodes"] = nlohmann::json::array();
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];
        layout["nodes"].push_back({
            {"id", node.memory_id},
            {"x", dis(gen)},
            {"y", dis(gen)},
            {"fx", nullptr}, // Fixed position if needed
            {"fy", nullptr}
        });
    }

    layout["links"] = nlohmann::json::array();
    for (const auto& edge : edges) {
        layout["links"].push_back({
            {"source", edge.source_id},
            {"target", edge.target_id},
            {"strength", edge.strength}
        });
    }

    layout["algorithm"] = "force_directed";
    layout["config"] = {
        {"linkDistance", 100},
        {"charge", -300},
        {"gravity", 0.1},
        {"friction", 0.9}
    };

    return layout;
}

nlohmann::json MemoryVisualizer::export_graph_data(const GraphVisualizationData& graph_data, const std::string& format) {
    nlohmann::json export_data;

    if (format == "d3_force") {
        // D3.js force-directed graph format
        export_data["nodes"] = nlohmann::json::array();
        for (const auto& node : graph_data.nodes) {
            export_data["nodes"].push_back({
                {"id", node.memory_id},
                {"title", node.title},
                {"type", node.memory_type},
                {"strength", node.strength},
                {"size", get_memory_type_size(node.memory_type, node.strength)},
                {"color", get_memory_type_color(node.memory_type)},
                {"group", node.memory_type},
                {"properties", node.visualization_properties}
            });
        }

        export_data["links"] = nlohmann::json::array();
        for (const auto& edge : graph_data.edges) {
            export_data["links"].push_back({
                {"source", edge.source_id},
                {"target", edge.target_id},
                {"type", edge.relationship_type},
                {"strength", edge.strength},
                {"width", std::max(1.0, edge.strength * 5.0)},
                {"color", get_relationship_type_color(edge.relationship_type)},
                {"properties", edge.visualization_properties}
            });
        }

    } else if (format == "cytoscape") {
        // Cytoscape.js format
        export_data["elements"] = {
            {"nodes", nlohmann::json::array()},
            {"edges", nlohmann::json::array()}
        };

        for (const auto& node : graph_data.nodes) {
            export_data["elements"]["nodes"].push_back({
                {"data", {
                    {"id", node.memory_id},
                    {"label", node.title},
                    {"type", node.memory_type},
                    {"strength", node.strength}
                }},
                {"position", {
                    {"x", 0}, // Would be calculated by layout
                    {"y", 0}
                }}
            });
        }

        for (const auto& edge : graph_data.edges) {
            export_data["elements"]["edges"].push_back({
                {"data", {
                    {"id", edge.relationship_id},
                    {"source", edge.source_id},
                    {"target", edge.target_id},
                    {"type", edge.relationship_type},
                    {"strength", edge.strength}
                }}
            });
        }
    }

    export_data["layout"] = graph_data.layout_config;
    export_data["styling"] = graph_data.styling_config;
    export_data["metadata"] = {
        {"node_count", graph_data.nodes.size()},
        {"edge_count", graph_data.edges.size()},
        {"generated_at", format_timestamp(graph_data.generated_at)}
    };

    return export_data;
}

nlohmann::json MemoryVisualizer::build_node_visualization_properties(const MemoryNode& node) {
    return {
        {"size", get_memory_type_size(node.memory_type, node.strength)},
        {"color", get_memory_type_color(node.memory_type)},
        {"border_width", 2},
        {"opacity", 0.8 + (node.strength * 0.2)}, // Higher strength = more opaque
        {"font_size", 12 + (node.access_count / 10)}, // More accessed = larger font
        {"shape", "circle"}
    };
}

nlohmann::json MemoryVisualizer::build_edge_visualization_properties(const MemoryEdge& edge) {
    return {
        {"width", std::max(1.0, edge.strength * 5.0)},
        {"color", get_relationship_type_color(edge.relationship_type)},
        {"opacity", edge.confidence},
        {"curvature", edge.bidirectional ? 0.1 : 0.0},
        {"style", get_edge_style(edge.relationship_type, edge.strength)}
    };
}

std::string MemoryVisualizer::generate_cache_key(const VisualizationRequest& request) {
    std::stringstream ss;
    ss << request.agent_id << "_" << request.visualization_type << "_";

    // Add parameter hash
    std::hash<std::string> hasher;
    ss << hasher(request.parameters.dump());

    if (request.filter_criteria) {
        ss << "_" << hasher(*request.filter_criteria);
    }

    return ss.str();
}

bool MemoryVisualizer::cache_visualization_data(const std::string& cache_key, const std::string& agent_id, const std::string& visualization_type, const nlohmann::json& data, int ttl_seconds) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return false;

        auto expires_at = std::chrono::system_clock::now() + std::chrono::seconds(ttl_seconds);
        auto expires_seconds = std::chrono::duration_cast<std::chrono::seconds>(
            expires_at.time_since_epoch()).count();

        const char* params[7] = {
            generate_uuid().c_str(),
            agent_id.c_str(),
            visualization_type.c_str(),
            data.dump().c_str(),
            "{}", // Empty parameters for now
            std::to_string(ttl_seconds).c_str(), // Hit count placeholder
            std::to_string(expires_seconds).c_str()
        };

        PGresult* result = PQexecParams(
            conn,
            "INSERT INTO memory_visualization_cache "
            "(cache_id, agent_id, visualization_type, cache_data, parameters, hit_count, expires_at) "
            "VALUES ($1, $2, $3, $4::jsonb, $5::jsonb, $6, to_timestamp($7)) "
            "ON CONFLICT (agent_id, visualization_type) DO UPDATE SET "
            "cache_data = EXCLUDED.cache_data, expires_at = EXCLUDED.expires_at, "
            "last_accessed = NOW(), hit_count = memory_visualization_cache.hit_count + 1",
            7, nullptr, params, nullptr, nullptr, 0
        );

        bool success = (PQresultStatus(result) == PGRES_COMMAND_OK);
        PQclear(result);
        return success;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in cache_visualization_data: " + std::string(e.what()));
        return false;
    }
}

std::optional<nlohmann::json> MemoryVisualizer::get_cached_visualization(const std::string& cache_key, const std::string& agent_id) {
    try {
        auto conn = db_conn_->get_connection();
        if (!conn) return std::nullopt;

        const char* params[2] = {agent_id.c_str(), cache_key.c_str()};
        PGresult* result = PQexecParams(
            conn,
            "SELECT cache_data FROM memory_visualization_cache "
            "WHERE agent_id = $1 AND cache_key = $2 AND expires_at > NOW()",
            2, nullptr, params, nullptr, nullptr, 0
        );

        if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0) {
            if (PQgetvalue(result, 0, 0)) {
                nlohmann::json cached_data = nlohmann::json::parse(PQgetvalue(result, 0, 0));
                PQclear(result);
                return cached_data;
            }
        }

        PQclear(result);
        return std::nullopt;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in get_cached_visualization: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::string MemoryVisualizer::get_memory_type_color(const std::string& memory_type) {
    if (memory_type == "episodic") return "#FF6B6B";
    if (memory_type == "semantic") return "#4ECDC4";
    if (memory_type == "procedural") return "#45B7D1";
    return "#95A5A6"; // Default gray
}

std::string MemoryVisualizer::get_relationship_type_color(const std::string& relationship_type) {
    if (relationship_type == "causes") return "#FF6B6B";
    if (relationship_type == "relates_to") return "#4ECDC4";
    if (relationship_type == "derived_from") return "#45B7D1";
    if (relationship_type == "supports") return "#96CEB4";
    if (relationship_type == "contradicts") return "#FECA57";
    return "#BDC3C7"; // Default gray
}

double MemoryVisualizer::get_memory_type_size(const std::string& memory_type, double strength) {
    double base_size = 20.0;
    if (memory_type == "semantic") base_size = 25.0; // Semantic memories are generally more important
    if (memory_type == "procedural") base_size = 18.0; // Procedural are more compact
    return base_size * (0.5 + strength * 0.5); // Scale with strength
}

std::string MemoryVisualizer::get_edge_style(const std::string& relationship_type, double strength) {
    if (relationship_type == "contradicts") return "dashed";
    if (strength > 0.8) return "bold";
    return "solid";
}

std::string MemoryVisualizer::generate_uuid() {
    uuid_t uuid;
    uuid_generate(uuid);

    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);

    return std::string(uuid_str);
}

nlohmann::json MemoryVisualizer::format_timestamp(const std::chrono::system_clock::time_point& timestamp) {
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        timestamp.time_since_epoch()).count();
    return seconds;
}

} // namespace memory
} // namespace regulens
