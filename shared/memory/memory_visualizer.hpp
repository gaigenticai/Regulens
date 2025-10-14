/**
 * Memory Visualizer
 * Graph visualization and data formatting for agent memory management
 */

#ifndef REGULENS_MEMORY_VISUALIZER_HPP
#define REGULENS_MEMORY_VISUALIZER_HPP

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {
namespace memory {

struct MemoryNode {
    std::string memory_id;
    std::string title;
    std::string content;
    std::string memory_type; // 'episodic', 'semantic', 'procedural'
    double strength = 0.5;
    int access_count = 0;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_accessed;
    std::vector<std::string> tags;
    nlohmann::json metadata;
    nlohmann::json visualization_properties; // Position, color, size, etc.
};

struct MemoryEdge {
    std::string relationship_id;
    std::string source_id;
    std::string target_id;
    std::string relationship_type; // 'causes', 'relates_to', 'derived_from', etc.
    double strength = 0.5;
    double confidence = 1.0;
    bool bidirectional = false;
    std::optional<std::string> context;
    nlohmann::json metadata;
    nlohmann::json visualization_properties; // Style, curvature, etc.
};

struct GraphVisualizationData {
    std::vector<MemoryNode> nodes;
    std::vector<MemoryEdge> edges;
    nlohmann::json layout_config;
    nlohmann::json styling_config;
    std::chrono::system_clock::time_point generated_at;
    std::string cache_key;
};

struct TimelineVisualizationData {
    std::vector<nlohmann::json> timeline_events;
    nlohmann::json time_ranges;
    nlohmann::json category_colors;
    std::chrono::system_clock::time_point generated_at;
};

struct ClusterVisualizationData {
    std::vector<nlohmann::json> clusters;
    nlohmann::json cluster_hierarchy;
    nlohmann::json similarity_matrix;
    std::chrono::system_clock::time_point generated_at;
};

struct StrengthDistributionData {
    std::vector<nlohmann::json> strength_buckets;
    nlohmann::json distribution_stats;
    nlohmann::json decay_patterns;
    std::chrono::system_clock::time_point generated_at;
};

struct VisualizationRequest {
    std::string agent_id;
    std::string visualization_type; // 'graph', 'timeline', 'cluster', 'strength_distribution'
    nlohmann::json parameters;
    bool use_cache = true;
    int max_nodes = 1000;
    int max_edges = 5000;
    std::optional<std::string> filter_criteria;
};

struct VisualizationResponse {
    std::string visualization_type;
    nlohmann::json data;
    bool from_cache = false;
    std::chrono::system_clock::time_point generated_at;
    std::optional<std::string> cache_key;
    nlohmann::json metadata;
};

class MemoryVisualizer {
public:
    MemoryVisualizer(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger
    );

    ~MemoryVisualizer();

    // Core visualization methods
    VisualizationResponse generate_graph_visualization(const VisualizationRequest& request);
    VisualizationResponse generate_timeline_visualization(const VisualizationRequest& request);
    VisualizationResponse generate_cluster_visualization(const VisualizationRequest& request);
    VisualizationResponse generate_strength_distribution_visualization(const VisualizationRequest& request);

    // Graph-specific methods
    GraphVisualizationData build_memory_graph(const std::string& agent_id, const nlohmann::json& parameters);
    std::vector<MemoryNode> extract_memory_nodes(const std::string& agent_id, const nlohmann::json& filters);
    std::vector<MemoryEdge> extract_memory_relationships(const std::string& agent_id, const std::vector<std::string>& node_ids);
    nlohmann::json apply_graph_layout(const std::vector<MemoryNode>& nodes, const std::vector<MemoryEdge>& edges, const std::string& layout_algorithm = "force_directed");

    // Timeline-specific methods
    TimelineVisualizationData build_memory_timeline(const std::string& agent_id, const nlohmann::json& parameters);
    std::vector<nlohmann::json> aggregate_timeline_events(const std::string& agent_id, const std::string& time_range);
    nlohmann::json calculate_time_ranges(const std::vector<nlohmann::json>& events);

    // Clustering methods
    ClusterVisualizationData build_memory_clusters(const std::string& agent_id, const nlohmann::json& parameters);
    std::vector<nlohmann::json> perform_memory_clustering(const std::vector<MemoryNode>& nodes, const std::string& clustering_algorithm = "content_similarity");
    nlohmann::json build_cluster_hierarchy(const std::vector<nlohmann::json>& clusters);

    // Strength distribution methods
    StrengthDistributionData analyze_strength_distribution(const std::string& agent_id, const nlohmann::json& parameters);
    std::vector<nlohmann::json> calculate_strength_buckets(const std::string& agent_id);
    nlohmann::json analyze_decay_patterns(const std::string& agent_id);

    // Caching methods
    bool cache_visualization_data(const std::string& cache_key, const std::string& agent_id, const std::string& visualization_type, const nlohmann::json& data, int ttl_seconds = 3600);
    std::optional<nlohmann::json> get_cached_visualization(const std::string& cache_key, const std::string& agent_id);
    void cleanup_expired_cache();

    // Export methods
    nlohmann::json export_graph_data(const GraphVisualizationData& graph_data, const std::string& format = "cytoscape");
    nlohmann::json export_timeline_data(const TimelineVisualizationData& timeline_data, const std::string& format = "d3_timeline");
    std::string export_visualization_config(const std::string& visualization_type);

    // Configuration
    void set_max_visualization_nodes(int max_nodes);
    void set_cache_enabled(bool enabled);
    void set_cache_ttl_seconds(int ttl_seconds);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;

    // Configuration
    int max_visualization_nodes_ = 1000;
    int max_visualization_edges_ = 5000;
    bool cache_enabled_ = true;
    int cache_ttl_seconds_ = 3600;
    int max_cache_entries_per_agent_ = 10;

    // Internal methods
    std::string generate_cache_key(const VisualizationRequest& request);
    nlohmann::json build_node_visualization_properties(const MemoryNode& node);
    nlohmann::json build_edge_visualization_properties(const MemoryEdge& edge);
    std::vector<std::string> filter_memory_ids(const std::string& agent_id, const nlohmann::json& filters);

    // Graph algorithms
    nlohmann::json apply_force_directed_layout(const std::vector<MemoryNode>& nodes, const std::vector<MemoryEdge>& edges);
    nlohmann::json apply_hierarchical_layout(const std::vector<MemoryNode>& nodes, const std::vector<MemoryEdge>& edges);
    nlohmann::json apply_circular_layout(const std::vector<MemoryNode>& nodes);

    // Similarity calculations
    double calculate_memory_similarity(const MemoryNode& node1, const MemoryNode& node2);
    nlohmann::json build_similarity_matrix(const std::vector<MemoryNode>& nodes);

    // Statistical methods
    nlohmann::json calculate_memory_statistics(const std::string& agent_id);
    std::map<std::string, int> calculate_memory_type_distribution(const std::string& agent_id);
    std::map<std::string, double> calculate_relationship_type_distribution(const std::string& agent_id);

    // Utility methods
    std::string generate_uuid();
    nlohmann::json format_timestamp(const std::chrono::system_clock::time_point& timestamp);
    std::vector<std::string> tokenize_content(const std::string& content);
    double calculate_node_importance(const MemoryNode& node, const std::vector<MemoryEdge>& edges);

    // Color and styling utilities
    std::string get_memory_type_color(const std::string& memory_type);
    std::string get_relationship_type_color(const std::string& relationship_type);
    double get_memory_type_size(const std::string& memory_type, double strength);
    std::string get_edge_style(const std::string& relationship_type, double strength);

    // Database query helpers
    std::vector<MemoryNode> query_memory_nodes(const std::string& agent_id, const std::vector<std::string>& memory_ids = {});
    std::vector<MemoryEdge> query_memory_relationships(const std::string& agent_id, const std::vector<std::string>& node_ids = {});
    nlohmann::json query_memory_stats(const std::string& agent_id);

    // Validation methods
    bool validate_visualization_request(const VisualizationRequest& request);
    bool validate_agent_access(const std::string& agent_id, const std::string& user_id = "");
};

} // namespace memory
} // namespace regulens

#endif // REGULENS_MEMORY_VISUALIZER_HPP
