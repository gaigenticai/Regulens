/**
 * Embeddings Explorer
 * Interactive exploration of embedding spaces with dimensionality reduction and visualization
 */

#ifndef REGULENS_EMBEDDINGS_EXPLORER_HPP
#define REGULENS_EMBEDDINGS_EXPLORER_HPP

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {
namespace embeddings {

struct EmbeddingPoint {
    std::string id;
    std::vector<float> vector;
    nlohmann::json metadata;
    std::string label;
    std::string category;
    double confidence = 1.0;
};

struct VisualizationResult {
    std::string visualization_id;
    std::string visualization_type; // 'tsne', 'umap', 'pca', 'mds', 'isomap'
    std::string embedding_model;
    std::vector<std::vector<double>> coordinates; // 2D or 3D coordinates
    std::vector<EmbeddingPoint> points;
    nlohmann::json parameters;
    nlohmann::json quality_metrics;
    int sample_size;
    int total_embeddings;
    std::chrono::system_clock::time_point created_at;
    std::string cache_key;
};

struct SearchQuery {
    std::string query_text;
    std::vector<float> query_vector;
    std::string similarity_metric = "cosine"; // cosine, euclidean, manhattan
    int top_k = 10;
    std::optional<std::string> category_filter;
    std::optional<double> confidence_threshold;
};

struct SearchResult {
    std::string result_id;
    EmbeddingPoint point;
    double similarity_score;
    int rank;
    nlohmann::json search_metadata;
};

struct ClusterAnalysis {
    std::string cluster_id;
    std::string algorithm; // kmeans, dbscan, hdbscan, gmm, hierarchical
    std::vector<int> labels;
    std::vector<std::vector<double>> centers;
    nlohmann::json metrics;
    nlohmann::json parameters;
    std::vector<EmbeddingPoint> points;
};

struct ModelComparison {
    std::string comparison_id;
    std::string model_a;
    std::string model_b;
    std::string comparison_type; // cosine_similarity, euclidean_distance, alignment_score
    nlohmann::json results;
    int sample_size;
    nlohmann::json statistical_significance;
    std::chrono::system_clock::time_point created_at;
};

struct SamplingStrategy {
    std::string strategy_id;
    std::string name;
    std::string type; // random, stratified, clustered, diversity, importance
    nlohmann::json parameters;
    nlohmann::json quality_metrics;
};

class EmbeddingsExplorer {
public:
    EmbeddingsExplorer(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger
    );

    ~EmbeddingsExplorer();

    // Core exploration methods
    VisualizationResult generate_visualization(
        const std::string& embedding_model,
        const std::string& visualization_type,
        const std::vector<EmbeddingPoint>& points,
        const nlohmann::json& parameters = nlohmann::json(),
        bool use_cache = true
    );

    std::vector<SearchResult> semantic_search(
        const SearchQuery& query,
        const std::string& embedding_model,
        bool use_cache = true
    );

    ClusterAnalysis perform_clustering(
        const std::vector<EmbeddingPoint>& points,
        const std::string& algorithm,
        const nlohmann::json& parameters = nlohmann::json()
    );

    ModelComparison compare_models(
        const std::string& model_a,
        const std::string& model_b,
        const std::string& comparison_type,
        int sample_size = 1000
    );

    // Data management
    bool store_embeddings(
        const std::string& model_name,
        const std::vector<EmbeddingPoint>& points
    );

    std::vector<EmbeddingPoint> load_embeddings(
        const std::string& model_name,
        int limit = 10000,
        int offset = 0
    );

    bool update_embedding_metadata(
        const std::string& model_name,
        const std::string& embedding_id,
        const nlohmann::json& metadata
    );

    // Sampling methods
    std::vector<EmbeddingPoint> sample_embeddings(
        const std::vector<EmbeddingPoint>& points,
        const SamplingStrategy& strategy,
        int sample_size
    );

    std::vector<SamplingStrategy> get_sampling_strategies();

    // Analytics and metadata
    nlohmann::json get_model_metadata(const std::string& model_name);
    std::vector<std::string> get_available_models();
    nlohmann::json get_exploration_stats(const std::string& time_range = "7d");

    // Caching methods
    bool cache_visualization_result(
        const VisualizationResult& result,
        int ttl_seconds = 86400 // 24 hours
    );

    std::optional<VisualizationResult> get_cached_visualization(
        const std::string& visualization_type,
        const std::string& model_name,
        const std::string& cache_key
    );

    void cleanup_expired_cache();

    // Bookmarks and saved views
    bool save_bookmark(
        const std::string& user_id,
        const std::string& bookmark_name,
        const std::string& visualization_type,
        const std::string& model_name,
        const nlohmann::json& view_parameters,
        const nlohmann::json& selected_points = nlohmann::json(),
        const nlohmann::json& annotations = nlohmann::json()
    );

    std::vector<nlohmann::json> get_user_bookmarks(const std::string& user_id);
    bool delete_bookmark(const std::string& bookmark_id, const std::string& user_id);

    // Performance tracking
    void record_performance_metric(
        const std::string& operation_type,
        const std::string& model_name,
        long execution_time_ms,
        double memory_usage_mb = 0.0,
        double quality_score = 0.0,
        const std::string& error_details = ""
    );

    // Configuration
    void set_cache_enabled(bool enabled);
    void set_max_sample_size(int max_size);
    void set_default_visualization_dimensions(int dimensions);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;

    // Configuration
    bool cache_enabled_ = true;
    int max_sample_size_ = 10000;
    int default_visualization_dimensions_ = 2;
    int cache_ttl_seconds_ = 86400;

    // Internal methods
    std::string generate_cache_key(
        const std::string& visualization_type,
        const std::string& model_name,
        const nlohmann::json& parameters
    );

    std::vector<std::vector<double>> perform_dimensionality_reduction(
        const std::vector<std::vector<float>>& embeddings,
        const std::string& algorithm,
        const nlohmann::json& parameters
    );

    std::vector<double> calculate_similarity(
        const std::vector<float>& query_vector,
        const std::vector<float>& target_vector,
        const std::string& metric
    );

    std::vector<SearchResult> rank_search_results(
        const std::vector<std::pair<EmbeddingPoint, double>>& scored_results,
        int top_k
    );

    std::vector<int> perform_clustering_algorithm(
        const std::vector<std::vector<float>>& embeddings,
        const std::string& algorithm,
        const nlohmann::json& parameters
    );

    nlohmann::json calculate_clustering_metrics(
        const std::vector<std::vector<float>>& embeddings,
        const std::vector<int>& labels,
        const std::vector<std::vector<double>>& centers
    );

    nlohmann::json calculate_model_comparison_metrics(
        const std::vector<std::vector<float>>& embeddings_a,
        const std::vector<std::vector<float>>& embeddings_b,
        const std::string& comparison_type
    );

    std::vector<EmbeddingPoint> apply_sampling_strategy(
        const std::vector<EmbeddingPoint>& points,
        const SamplingStrategy& strategy,
        int sample_size
    );

    // Random sampling
    std::vector<EmbeddingPoint> random_sample(
        const std::vector<EmbeddingPoint>& points,
        int sample_size
    );

    // Stratified sampling
    std::vector<EmbeddingPoint> stratified_sample(
        const std::vector<EmbeddingPoint>& points,
        int sample_size,
        const std::string& category_field = "category"
    );

    // Diversity sampling using Farthest First Traversal
    std::vector<EmbeddingPoint> diversity_sample(
        const std::vector<EmbeddingPoint>& points,
        int sample_size
    );

    // Utility methods
    std::string generate_uuid();
    nlohmann::json format_timestamp(const std::chrono::system_clock::time_point& timestamp);
    double cosine_similarity(const std::vector<float>& a, const std::vector<float>& b);
    double euclidean_distance(const std::vector<float>& a, const std::vector<float>& b);
    double manhattan_distance(const std::vector<float>& a, const std::vector<float>& b);

    // Database operations
    bool store_visualization_cache(const VisualizationResult& result, int ttl_seconds);
    bool store_search_cache(const SearchQuery& query, const std::vector<SearchResult>& results, int ttl_seconds);
    bool store_clustering_result(const ClusterAnalysis& analysis);

    std::optional<nlohmann::json> load_visualization_cache(const std::string& cache_key);
    std::optional<nlohmann::json> load_search_cache(const SearchQuery& query);

    // Validation
    bool validate_embedding_model(const std::string& model_name);
    bool validate_visualization_type(const std::string& visualization_type);
    bool validate_clustering_algorithm(const std::string& algorithm);

    // Quality metrics
    nlohmann::json calculate_visualization_quality_metrics(
        const std::vector<std::vector<float>>& original_embeddings,
        const std::vector<std::vector<double>>& reduced_coordinates,
        const std::string& algorithm
    );

    // Logging helpers
    void log_visualization_generation(
        const std::string& model_name,
        const std::string& visualization_type,
        int sample_size,
        long execution_time_ms
    );

    void log_search_performance(
        const std::string& model_name,
        int top_k,
        long execution_time_ms
    );

    void log_clustering_operation(
        const std::string& algorithm,
        int num_points,
        int num_clusters,
        long execution_time_ms
    );
};

} // namespace embeddings
} // namespace regulens

#endif // REGULENS_EMBEDDINGS_EXPLORER_HPP
