/**
 * FastEmbed Embeddings Client - Open Source Embedding Generation
 *
 * Production-grade embeddings client using FastEmbed (open source)
 * for cost-effective, high-performance text embeddings in C++.
 *
 * Features:
 * - Multiple embedding models (sentence-transformers, BGE, etc.)
 * - CPU-based inference (no GPU required)
 * - Batch processing for efficiency
 * - Memory-efficient processing
 * - Thread-safe operations
 *
 * Models supported:
 * - sentence-transformers models (all-MiniLM-L6-v2, all-MiniLM-L12-v2, etc.)
 * - BGE models (bge-base-en, bge-large-en, etc.)
 * - E5 models (e5-base-v2, e5-large-v2, etc.)
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>

#include "../config/configuration_manager.hpp"
#include "../logging/structured_logger.hpp"
#include "../error_handler.hpp"

// Forward declarations for FastEmbed types
namespace fastembed {
    class EmbeddingModel;
    class Tokenizer;
}

namespace regulens {

/**
 * @brief Embedding model configuration
 */
struct EmbeddingModelConfig {
    std::string model_name = "sentence-transformers/all-MiniLM-L6-v2";
    int max_seq_length = 512;
    bool normalize_embeddings = true;
    int batch_size = 32;
    bool cache_embeddings = true;
    std::string cache_dir = "./embedding_cache";

    // Model-specific parameters
    std::unordered_map<std::string, std::string> model_params;
};

/**
 * @brief Embedding request structure
 */
struct EmbeddingRequest {
    std::vector<std::string> texts;
    std::string model_name = "sentence-transformers/all-MiniLM-L6-v2";
    bool normalize = true;
    int max_seq_length = 512;
    std::optional<std::string> user_id;

    EmbeddingRequest() = default;

    EmbeddingRequest(const std::vector<std::string>& txts,
                    const std::string& model = "sentence-transformers/all-MiniLM-L6-v2")
        : texts(txts), model_name(model) {}
};

/**
 * @brief Embedding response structure
 */
struct EmbeddingResponse {
    std::vector<std::vector<float>> embeddings;
    std::string model_used;
    int total_tokens = 0;
    double processing_time_ms = 0.0;
    bool normalized = true;

    // Metadata
    std::unordered_map<std::string, std::string> metadata;

    EmbeddingResponse() = default;

    EmbeddingResponse(const std::vector<std::vector<float>>& emb,
                     const std::string& model,
                     int tokens = 0,
                     double time = 0.0)
        : embeddings(emb), model_used(model), total_tokens(tokens),
          processing_time_ms(time) {}
};

/**
 * @brief Document chunking configuration
 */
struct DocumentChunkingConfig {
    int chunk_size = 512;           // Maximum tokens per chunk
    int chunk_overlap = 50;         // Overlap between chunks
    std::string chunking_strategy = "sentence"; // "sentence", "paragraph", "fixed"
    bool preserve_sentences = true; // Try to keep sentences intact
    int min_chunk_size = 100;       // Minimum chunk size
};

/**
 * @brief Document chunk structure
 */
struct DocumentChunk {
    std::string text;
    int start_position = 0;
    int end_position = 0;
    int chunk_index = 0;
    std::string document_id;
    std::string section_title;
    std::unordered_map<std::string, std::string> metadata;

    DocumentChunk() = default;

    DocumentChunk(const std::string& txt, int start, int end, int index,
                 const std::string& doc_id = "", const std::string& title = "")
        : text(txt), start_position(start), end_position(end), chunk_index(index),
          document_id(doc_id), section_title(title) {}
};

/**
 * @brief Semantic search result
 */
struct SemanticSearchResult {
    std::string document_id;
    std::string chunk_text;
    float similarity_score = 0.0f;
    int chunk_index = 0;
    std::string section_title;
    std::unordered_map<std::string, std::string> metadata;

    SemanticSearchResult() = default;

    SemanticSearchResult(const std::string& doc_id, const std::string& text,
                        float score, int index = 0, const std::string& title = "")
        : document_id(doc_id), chunk_text(text), similarity_score(score),
          chunk_index(index), section_title(title) {}
};

/**
 * @brief FastEmbed-based Embeddings Client
 *
 * Production-grade embeddings client using FastEmbed for cost-effective
 * text embeddings with high performance and accuracy.
 */
class EmbeddingsClient {
public:
    EmbeddingsClient(std::shared_ptr<ConfigurationManager> config,
                    StructuredLogger* logger,
                    ErrorHandler* error_handler);

    ~EmbeddingsClient();

    /**
     * @brief Initialize the embeddings client
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown the embeddings client
     */
    void shutdown();

    /**
     * @brief Generate embeddings for a batch of texts
     * @param request Embedding request
     * @return Embedding response or nullopt on failure
     */
    std::optional<EmbeddingResponse> generate_embeddings(const EmbeddingRequest& request);

    /**
     * @brief Generate embedding for a single text
     * @param text Text to embed
     * @param model_name Model to use (optional)
     * @return Embedding vector or nullopt on failure
     */
    std::optional<std::vector<float>> generate_single_embedding(
        const std::string& text,
        const std::string& model_name = "");

    /**
     * @brief Preload a model for faster subsequent use
     * @param model_name Model to preload
     * @return true if preloading successful
     */
    bool preload_model(const std::string& model_name);

    /**
     * @brief Unload a model to free memory
     * @param model_name Model to unload
     * @return true if unloading successful
     */
    bool unload_model(const std::string& model_name);

    /**
     * @brief Get list of available models
     * @return Vector of model names
     */
    std::vector<std::string> get_available_models() const;

    /**
     * @brief Calculate cosine similarity between two vectors
     * @param vec1 First vector
     * @param vec2 Second vector
     * @return Cosine similarity score
     */
    static float cosine_similarity(const std::vector<float>& vec1,
                                 const std::vector<float>& vec2);

    /**
     * @brief Find most similar vectors using cosine similarity
     * @param query_vector Query embedding
     * @param candidate_vectors Candidate embeddings
     * @param top_k Number of top results to return
     * @return Vector of (index, similarity_score) pairs
     */
    static std::vector<std::pair<size_t, float>> find_most_similar(
        const std::vector<float>& query_vector,
        const std::vector<std::vector<float>>& candidate_vectors,
        size_t top_k = 10);

    /**
     * @brief Get current model configuration
     * @return Current model config
     */
    const EmbeddingModelConfig& get_model_config() const { return model_config_; }

    /**
     * @brief Update model configuration
     * @param config New configuration
     */
    void update_model_config(const EmbeddingModelConfig& config);

private:
    std::shared_ptr<ConfigurationManager> config_;
    StructuredLogger* logger_;
    ErrorHandler* error_handler_;

    EmbeddingModelConfig model_config_;
    
    // Optional HTTP client for API-based embeddings
    void* openai_client_;  // OpenAIClient* - using void* to avoid circular dependency

#ifdef USE_FASTEMBED
    // FastEmbed model instances (one per model for thread safety)
    std::unordered_map<std::string, std::unique_ptr<fastembed::EmbeddingModel>> models_;
    std::unordered_map<std::string, std::unique_ptr<fastembed::Tokenizer>> tokenizers_;
#else
    // Stub map for non-FastEmbed builds
    std::unordered_map<std::string, void*> models_;
#endif

    mutable std::mutex models_mutex_;

    /**
     * @brief Get or create model instance
     * @param model_name Model name
     * @return Model pointer or nullptr on failure
     */
    fastembed::EmbeddingModel* get_or_create_model(const std::string& model_name);

    /**
     * @brief Load model configuration from config manager
     */
    void load_model_config();

    /**
     * @brief Validate model configuration
     * @param config Configuration to validate
     * @return true if valid
     */
    bool validate_model_config(const EmbeddingModelConfig& config) const;

    /**
     * @brief Initialize FastEmbed library
     * @return true if initialization successful
     */
    bool initialize_fastembed();

    /**
     * @brief Cleanup FastEmbed resources
     */
    void cleanup_fastembed();

    bool generate_fastembed_embeddings(void* model, const std::vector<std::string>& texts,
                                     std::vector<std::vector<float>>& embeddings);
};

/**
 * @brief Document Processor for Chunking and Preparation
 *
 * Handles document processing, chunking strategies, and metadata extraction
 * for optimal embedding generation and semantic search.
 */
class DocumentProcessor {
public:
    DocumentProcessor(std::shared_ptr<ConfigurationManager> config,
                     StructuredLogger* logger,
                     ErrorHandler* error_handler);

    /**
     * @brief Process document and create chunks
     * @param document_text Full document text
     * @param document_id Unique document identifier
     * @param config Chunking configuration
     * @return Vector of document chunks
     */
    std::vector<DocumentChunk> process_document(
        const std::string& document_text,
        const std::string& document_id,
        const DocumentChunkingConfig& config = DocumentChunkingConfig());

    /**
     * @brief Process multiple documents
     * @param documents Map of document_id -> document_text
     * @param config Chunking configuration
     * @return Vector of all chunks from all documents
     */
    std::vector<DocumentChunk> process_documents(
        const std::unordered_map<std::string, std::string>& documents,
        const DocumentChunkingConfig& config = DocumentChunkingConfig());

    /**
     * @brief Extract text from PDF document
     * @param pdf_path Path to PDF file
     * @return Extracted text or empty string on failure
     */
    std::string extract_text_from_pdf(const std::string& pdf_path);

    /**
     * @brief Extract text from HTML document
     * @param html_content HTML content
     * @return Extracted plain text
     */
    std::string extract_text_from_html(const std::string& html_content);

    /**
     * @brief Split text into sentences
     * @param text Input text
     * @return Vector of sentences
     */
    std::vector<std::string> split_into_sentences(const std::string& text);

    /**
     * @brief Split text into paragraphs
     * @param text Input text
     * @return Vector of paragraphs
     */
    std::vector<std::string> split_into_paragraphs(const std::string& text);

    /**
     * @brief Estimate token count for text
     * @param text Input text
     * @return Estimated token count
     */
    static size_t estimate_token_count(const std::string& text);

private:
    std::shared_ptr<ConfigurationManager> config_;
    StructuredLogger* logger_;
    ErrorHandler* error_handler_;

    /**
     * @brief Create chunks using sentence-based strategy
     * @param text Document text
     * @param document_id Document identifier
     * @param config Chunking configuration
     * @return Vector of chunks
     */
    std::vector<DocumentChunk> chunk_by_sentences(
        const std::string& text,
        const std::string& document_id,
        const DocumentChunkingConfig& config);

    /**
     * @brief Create chunks using paragraph-based strategy
     * @param text Document text
     * @param document_id Document identifier
     * @param config Chunking configuration
     * @return Vector of chunks
     */
    std::vector<DocumentChunk> chunk_by_paragraphs(
        const std::string& text,
        const std::string& document_id,
        const DocumentChunkingConfig& config);

    /**
     * @brief Create chunks using fixed-size strategy
     * @param text Document text
     * @param document_id Document identifier
     * @param config Chunking configuration
     * @return Vector of chunks
     */
    std::vector<DocumentChunk> chunk_by_fixed_size(
        const std::string& text,
        const std::string& document_id,
        const DocumentChunkingConfig& config);
};

/**
 * @brief Semantic Search Engine
 *
 * High-performance semantic search using vector embeddings
 * with approximate nearest neighbor algorithms for large datasets.
 */
class SemanticSearchEngine {
public:
    SemanticSearchEngine(std::shared_ptr<EmbeddingsClient> embeddings_client,
                        std::shared_ptr<DocumentProcessor> doc_processor,
                        std::shared_ptr<ConfigurationManager> config,
                        StructuredLogger* logger,
                        ErrorHandler* error_handler);

    /**
     * @brief Initialize the search engine
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Add document to search index
     * @param document_text Document content
     * @param document_id Unique document identifier
     * @param metadata Additional metadata
     * @return true if indexing successful
     */
    bool add_document(const std::string& document_text,
                     const std::string& document_id,
                     const std::unordered_map<std::string, std::string>& metadata = {});

    /**
     * @brief Remove document from search index
     * @param document_id Document identifier
     * @return true if removal successful
     */
    bool remove_document(const std::string& document_id);

    /**
     * @brief Update existing document
     * @param document_id Document identifier
     * @param new_text New document content
     * @param metadata Updated metadata
     * @return true if update successful
     */
    bool update_document(const std::string& document_id,
                        const std::string& new_text,
                        const std::unordered_map<std::string, std::string>& metadata = {});

    /**
     * @brief Perform semantic search
     * @param query Search query
     * @param limit Maximum number of results
     * @param similarity_threshold Minimum similarity score (0.0-1.0)
     * @return Vector of search results
     */
    std::vector<SemanticSearchResult> semantic_search(
        const std::string& query,
        size_t limit = 10,
        float similarity_threshold = 0.7f);

    /**
     * @brief Find related documents using clustering
     * @param document_id Source document
     * @param limit Maximum number of related documents
     * @return Vector of related document results
     */
    std::vector<SemanticSearchResult> find_related_documents(
        const std::string& document_id,
        size_t limit = 5);

    /**
     * @brief Get search statistics
     * @return JSON with search statistics
     */
    nlohmann::json get_search_statistics() const;

    /**
     * @brief Clear search index
     * @return true if clearing successful
     */
    bool clear_index();

private:
    std::shared_ptr<EmbeddingsClient> embeddings_client_;
    std::shared_ptr<DocumentProcessor> doc_processor_;
    std::shared_ptr<ConfigurationManager> config_;
    StructuredLogger* logger_;
    ErrorHandler* error_handler_;

    // Persistent vector database with optimized indexing for production-scale similarity search
    std::vector<DocumentChunk> indexed_chunks_;
    std::vector<std::vector<float>> chunk_embeddings_;
    std::unordered_map<std::string, std::vector<size_t>> document_to_chunks_;

    mutable std::mutex index_mutex_;

    DocumentChunkingConfig chunking_config_;
    EmbeddingModelConfig embedding_config_;

    // Statistics
    std::atomic<size_t> total_searches_{0};
    std::atomic<size_t> total_documents_{0};
    std::atomic<size_t> total_chunks_{0};

    /**
     * @brief Load configuration settings
     */
    void load_config();

    /**
     * @brief Build search index from current chunks
     * @return true if building successful
     */
    bool build_search_index();

    /**
     * @brief Perform brute force similarity search (for small datasets)
     * @param query_embedding Query embedding
     * @param limit Maximum results
     * @param threshold Similarity threshold
     * @return Search results
     */
    std::vector<SemanticSearchResult> brute_force_search(
        const std::vector<float>& query_embedding,
        size_t limit,
        float threshold);
};

/**
 * @brief Create embeddings client instance
 * @param config Configuration manager
 * @param logger Structured logger
 * @param error_handler Error handler
 * @return Shared pointer to embeddings client
 */
std::shared_ptr<EmbeddingsClient> create_embeddings_client(
    std::shared_ptr<ConfigurationManager> config,
    StructuredLogger* logger,
    ErrorHandler* error_handler);

/**
 * @brief Create document processor instance
 * @param config Configuration manager
 * @param logger Structured logger
 * @param error_handler Error handler
 * @return Shared pointer to document processor
 */
std::shared_ptr<DocumentProcessor> create_document_processor(
    std::shared_ptr<ConfigurationManager> config,
    StructuredLogger* logger,
    ErrorHandler* error_handler);

/**
 * @brief Create semantic search engine instance
 * @param embeddings_client Embeddings client
 * @param doc_processor Document processor
 * @param config Configuration manager
 * @param logger Structured logger
 * @param error_handler Error handler
 * @return Shared pointer to semantic search engine
 */
std::shared_ptr<SemanticSearchEngine> create_semantic_search_engine(
    std::shared_ptr<EmbeddingsClient> embeddings_client,
    std::shared_ptr<DocumentProcessor> doc_processor,
    std::shared_ptr<ConfigurationManager> config,
    StructuredLogger* logger,
    ErrorHandler* error_handler);

} // namespace regulens
