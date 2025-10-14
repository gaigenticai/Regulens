/**
 * Dimensionality Reducer
 * Production-grade dimensionality reduction algorithms for embedding visualization
 */

#ifndef REGULENS_DIMENSIONALITY_REDUCER_HPP
#define REGULENS_DIMENSIONALITY_REDUCER_HPP

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include "../logging/structured_logger.hpp"

namespace regulens {
namespace embeddings {

struct DimensionalityReductionParams {
    std::string algorithm; // tsne, umap, pca, mds, isomap
    int target_dimensions = 2;
    int max_iter = 1000;
    double learning_rate = 200.0;
    int perplexity = 30;
    double min_dist = 0.1;
    int n_neighbors = 15;
    bool normalize_input = true;
    int random_state = 42;
    nlohmann::json algorithm_specific_params;
};

struct DimensionalityReductionResult {
    std::vector<std::vector<double>> coordinates;
    nlohmann::json quality_metrics;
    nlohmann::json parameters_used;
    long execution_time_ms;
    bool success = true;
    std::string error_message;
};

class DimensionalityReducer {
public:
    DimensionalityReducer(std::shared_ptr<StructuredLogger> logger);
    ~DimensionalityReducer();

    // Core reduction methods
    DimensionalityReductionResult reduce_dimensions(
        const std::vector<std::vector<float>>& embeddings,
        const DimensionalityReductionParams& params
    );

    // Algorithm-specific methods
    DimensionalityReductionResult perform_tsne(
        const std::vector<std::vector<float>>& embeddings,
        const DimensionalityReductionParams& params
    );

    DimensionalityReductionResult perform_umap(
        const std::vector<std::vector<float>>& embeddings,
        const DimensionalityReductionParams& params
    );

    DimensionalityReductionResult perform_pca(
        const std::vector<std::vector<float>>& embeddings,
        const DimensionalityReductionParams& params
    );

    DimensionalityReductionResult perform_mds(
        const std::vector<std::vector<float>>& embeddings,
        const DimensionalityReductionParams& params
    );

    DimensionalityReductionResult perform_isomap(
        const std::vector<std::vector<float>>& embeddings,
        const DimensionalityReductionParams& params
    );

    // Quality assessment
    nlohmann::json assess_reduction_quality(
        const std::vector<std::vector<float>>& original_embeddings,
        const std::vector<std::vector<double>>& reduced_coordinates,
        const std::string& algorithm
    );

    // Utility methods
    std::vector<std::vector<double>> normalize_embeddings(
        const std::vector<std::vector<float>>& embeddings
    );

    std::vector<std::vector<double>> convert_float_to_double(
        const std::vector<std::vector<float>>& embeddings
    );

    // Default parameter generation
    DimensionalityReductionParams get_default_params(const std::string& algorithm);

    // Parameter validation
    bool validate_params(const DimensionalityReductionParams& params);

private:
    std::shared_ptr<StructuredLogger> logger_;

    // t-SNE implementation
    DimensionalityReductionResult tsne_implementation(
        const std::vector<std::vector<double>>& normalized_embeddings,
        const DimensionalityReductionParams& params
    );

    // UMAP implementation (simplified approximation)
    DimensionalityReductionResult umap_implementation(
        const std::vector<std::vector<double>>& normalized_embeddings,
        const DimensionalityReductionParams& params
    );

    // PCA implementation
    DimensionalityReductionResult pca_implementation(
        const std::vector<std::vector<double>>& normalized_embeddings,
        const DimensionalityReductionParams& params
    );

    // MDS implementation
    DimensionalityReductionResult mds_implementation(
        const std::vector<std::vector<double>>& normalized_embeddings,
        const DimensionalityReductionParams& params
    );

    // Isomap implementation (simplified)
    DimensionalityReductionResult isomap_implementation(
        const std::vector<std::vector<double>>& normalized_embeddings,
        const DimensionalityReductionParams& params
    );

    // Mathematical utilities
    std::vector<std::vector<double>> calculate_pairwise_distances(
        const std::vector<std::vector<double>>& points,
        const std::string& metric = "euclidean"
    );

    std::vector<std::vector<double>> compute_similarity_matrix(
        const std::vector<std::vector<double>>& distances,
        double perplexity
    );

    std::vector<std::vector<double>> initialize_coordinates(
        int n_samples,
        int n_dimensions,
        int random_state
    );

    double calculate_kl_divergence(
        const std::vector<std::vector<double>>& p_matrix,
        const std::vector<std::vector<double>>& q_matrix
    );

    // Quality metrics
    double calculate_trustworthiness(
        const std::vector<std::vector<double>>& original_distances,
        const std::vector<std::vector<double>>& reduced_distances,
        int k = 12
    );

    double calculate_continuity(
        const std::vector<std::vector<double>>& original_distances,
        const std::vector<std::vector<double>>& reduced_distances,
        int k = 12
    );

    double calculate_shepard_correlation(
        const std::vector<std::vector<double>>& original_distances,
        const std::vector<std::vector<double>>& reduced_distances
    );

    // Linear algebra utilities
    std::vector<std::vector<double>> matrix_multiply(
        const std::vector<std::vector<double>>& a,
        const std::vector<std::vector<double>>& b
    );

    std::vector<std::vector<double>> transpose_matrix(
        const std::vector<std::vector<double>>& matrix
    );

    std::vector<std::vector<double>> center_matrix(
        const std::vector<std::vector<double>>& matrix
    );

    // Eigenvalue decomposition (simplified for 2D/3D)
    std::pair<std::vector<double>, std::vector<std::vector<double>>> eigen_decomposition(
        const std::vector<std::vector<double>>& matrix
    );

    // Optimization utilities
    std::vector<std::vector<double>> gradient_descent_optimization(
        const std::vector<std::vector<double>>& initial_coordinates,
        const std::vector<std::vector<double>>& target_probabilities,
        double learning_rate,
        int max_iter,
        double tolerance = 1e-7
    );

    // Random utilities
    double random_normal(double mean = 0.0, double stddev = 1.0, int random_state = 42);
    std::vector<std::vector<double>> random_matrix(int rows, int cols, int random_state = 42);

    // Logging helpers
    void log_algorithm_start(const std::string& algorithm, int n_samples, int n_features, int n_dimensions);
    void log_algorithm_complete(const std::string& algorithm, long execution_time_ms, bool success);
    void log_quality_metrics(const std::string& algorithm, const nlohmann::json& metrics);
};

} // namespace embeddings
} // namespace regulens

#endif // REGULENS_DIMENSIONALITY_REDUCER_HPP
