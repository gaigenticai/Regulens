#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

#include "../shared/config/configuration_manager.hpp"
#include "../shared/logging/structured_logger.hpp"
#include "../shared/models/regulatory_change.hpp"

namespace regulens {

/**
 * @brief Change detection method
 */
enum class ChangeDetectionMethod {
    CONTENT_HASH,       // Hash-based comparison
    STRUCTURAL_DIFF,    // Structural document comparison
    SEMANTIC_ANALYSIS,  // AI-powered semantic change detection
    TIMESTAMP_BASED     // Timestamp-based detection
};

/**
 * @brief Result of change detection
 */
struct ChangeDetectionResult {
    bool has_changes;
    std::vector<RegulatoryChange> detected_changes;
    std::string detection_method;
    double confidence_score; // 0.0 to 1.0
    std::chrono::milliseconds processing_time;

    ChangeDetectionResult(bool changes = false,
                         std::vector<RegulatoryChange> changes_list = {},
                         std::string method = "",
                         double confidence = 0.0,
                         std::chrono::milliseconds time = std::chrono::milliseconds(0))
        : has_changes(changes), detected_changes(std::move(changes_list)),
          detection_method(std::move(method)), confidence_score(confidence),
          processing_time(time) {}
};

/**
 * @brief Change detector for regulatory documents
 *
 * Detects changes in regulatory content using various methods
 * including content hashing, structural analysis, and semantic comparison.
 */
class ChangeDetector {
public:
    ChangeDetector(std::shared_ptr<ConfigurationManager> config,
                  std::shared_ptr<StructuredLogger> logger);

    ~ChangeDetector();

    /**
     * @brief Initialize the change detector
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Detect changes between baseline and new content
     * @param source_id ID of the regulatory source
     * @param baseline_content Previous content
     * @param new_content Current content
     * @param metadata Additional metadata about the content
     * @return Change detection result
     */
    ChangeDetectionResult detect_changes(const std::string& source_id,
                                       const std::string& baseline_content,
                                       const std::string& new_content,
                                       const RegulatoryChangeMetadata& metadata);

    /**
     * @brief Get stored baseline content for a source
     * @param source_id Source identifier
     * @return Baseline content or empty string if not found
     */
    std::string get_baseline_content(const std::string& source_id) const;

    /**
     * @brief Update baseline content for a source
     * @param source_id Source identifier
     * @param content New baseline content
     * @param metadata Associated metadata
     */
    void update_baseline_content(const std::string& source_id,
                               const std::string& content,
                               const RegulatoryChangeMetadata& metadata);

    /**
     * @brief Get detection statistics
     * @return JSON with detection statistics
     */
    nlohmann::json get_detection_stats() const;

    /**
     * @brief Clear all stored baselines (for testing/reset)
     */
    void clear_baselines();

private:
    /**
     * @brief Hash-based change detection
     * @param baseline_hash Previous content hash
     * @param new_hash Current content hash
     * @return true if content has changed
     */
    bool detect_hash_changes(const std::string& baseline_hash,
                           const std::string& new_hash) const;

    /**
     * @brief Structural change detection
     * @param baseline_content Previous content
     * @param new_content Current content
     * @return List of structural differences
     */
    std::vector<std::string> detect_structural_changes(const std::string& baseline_content,
                                                     const std::string& new_content) const;

    /**
     * @brief Semantic change detection
     * @param baseline_content Previous content
     * @param new_content Current content
     * @param metadata Content metadata for context
     * @return Semantic change score (0.0 to 1.0)
     */
    double detect_semantic_changes(const std::string& baseline_content,
                                 const std::string& new_content,
                                 const RegulatoryChangeMetadata& metadata) const;

    /**
     * @brief Calculate content hash
     * @param content Content to hash
     * @return SHA-256 hash string
     */
    std::string calculate_content_hash(const std::string& content) const;

    /**
     * @brief Extract important keywords from content
     * @param content Document content
     * @return List of extracted keywords
     */
    std::vector<std::string> extract_keywords(const std::string& content) const;

    /**
     * @brief Determine if detected changes are significant
     * @param changes List of detected changes
     * @param metadata Content metadata
     * @return true if changes are significant enough to report
     */
    bool are_changes_significant(const std::vector<std::string>& changes,
                               const RegulatoryChangeMetadata& metadata) const;

    /**
     * @brief Calculate confidence score for change detection
     * @param method Detection method used
     * @param changes Detected changes
     * @return Confidence score (0.0 to 1.0)
     */
    double calculate_confidence_score(ChangeDetectionMethod method,
                                    const std::vector<std::string>& changes) const;

    /**
     * @brief Advanced diff algorithms
     */
    enum class EditOp {
        INSERT,
        DELETE,
        MATCH,
        REPLACE
    };

    struct Edit {
        EditOp operation;
        size_t baseline_index;
        size_t new_index;
        std::string content;

        Edit(EditOp op, size_t b_idx, size_t n_idx, std::string c = "")
            : operation(op), baseline_index(b_idx), new_index(n_idx), content(std::move(c)) {}
    };

    struct DiffChunk {
        size_t baseline_start;
        size_t baseline_end;
        size_t new_start;
        size_t new_end;
        std::vector<std::string> deleted_lines;
        std::vector<std::string> inserted_lines;
        double significance_score;

        DiffChunk() : baseline_start(0), baseline_end(0), new_start(0), new_end(0), significance_score(0.0) {}
    };

    struct ChangeSummary {
        std::string title;
        std::string category;
        double impact_score;
        std::vector<std::string> details;
    };

    std::vector<Edit> compute_myers_diff(const std::vector<std::string>& baseline_lines,
                                         const std::vector<std::string>& new_lines) const;
    std::vector<Edit> backtrack_myers_diff(const std::vector<std::string>& baseline_lines,
                                           const std::vector<std::string>& new_lines,
                                           const std::vector<std::vector<int>>& trace,
                                           size_t max_d) const;
    std::vector<Edit> compute_simple_diff(const std::vector<std::string>& baseline_lines,
                                          const std::vector<std::string>& new_lines) const;
    std::vector<DiffChunk> compute_advanced_diff(const std::string& baseline_content,
                                                 const std::string& new_content) const;
    double calculate_chunk_significance(const DiffChunk& chunk) const;
    double calculate_structural_confidence(const std::vector<DiffChunk>& chunks) const;

    /**
     * @brief Semantic analysis methods
     */
    std::unordered_map<std::string, double> calculate_term_frequency(const std::string& content) const;
    double calculate_cosine_similarity(const std::unordered_map<std::string, double>& tf1,
                                       const std::unordered_map<std::string, double>& tf2) const;
    double calculate_structural_similarity(const std::string& baseline_content,
                                          const std::string& new_content) const;
    std::vector<std::string> extract_structural_elements(const std::string& content) const;

    /**
     * @brief Change analysis and categorization
     */
    std::vector<ChangeSummary> analyze_diff_chunks(const std::vector<DiffChunk>& chunks,
                                                   const RegulatoryChangeMetadata& metadata) const;
    std::string categorize_chunk(const DiffChunk& chunk) const;
    std::string create_category_title(const std::string& category, size_t change_count) const;

    /**
     * @brief Utility methods
     */
    std::vector<std::string> split_into_lines(const std::string& content) const;
    std::string normalize_content(const std::string& content) const;
    std::string to_lowercase(const std::string& str) const;

    // Configuration and dependencies
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<StructuredLogger> logger_;

    // Baseline storage (source_id -> content hash)
    mutable std::mutex baselines_mutex_;
    std::unordered_map<std::string, std::string> content_hashes_;
    std::unordered_map<std::string, std::string> baseline_content_;
    std::unordered_map<std::string, RegulatoryChangeMetadata> baseline_metadata_;

    // Detection statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> total_detections_;
    std::atomic<size_t> hash_based_detections_;
    std::atomic<size_t> structural_detections_;
    std::atomic<size_t> semantic_detections_;
    std::atomic<size_t> false_positives_;
    std::chrono::system_clock::time_point last_detection_time_;

    // Configuration parameters
    double semantic_threshold_;      // Minimum semantic change score
    size_t min_content_length_;      // Minimum content length for analysis
    std::vector<std::string> ignored_patterns_; // Patterns to ignore in diff
};

/**
 * @brief Document parser for extracting regulatory information
 */
class DocumentParser {
public:
    DocumentParser(std::shared_ptr<ConfigurationManager> config,
                  std::shared_ptr<StructuredLogger> logger);

    ~DocumentParser();

    /**
     * @brief Initialize the document parser
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Parse regulatory document content
     * @param content Raw document content
     * @param content_type Content type (HTML, PDF, XML, etc.)
     * @return Parsed regulatory change metadata
     */
    RegulatoryChangeMetadata parse_document(const std::string& content,
                                          const std::string& content_type);

    /**
     * @brief Extract document title from content
     * @param content Document content
     * @param content_type Content type
     * @return Extracted title or empty string
     */
    std::string extract_title(const std::string& content,
                            const std::string& content_type) const;

    /**
     * @brief Extract effective date from document
     * @param content Document content
     * @return Extracted date or nullopt
     */
    std::optional<std::chrono::system_clock::time_point> extract_effective_date(
        const std::string& content) const;

    /**
     * @brief Get parsing statistics
     * @return JSON with parsing statistics
     */
    nlohmann::json get_parsing_stats() const;

private:
    /**
     * @brief Parse HTML content
     * @param html HTML content
     * @return Extracted metadata
     */
    RegulatoryChangeMetadata parse_html(const std::string& html);

    /**
     * @brief Parse XML/RSS content
     * @param xml XML content
     * @return Extracted metadata
     */
    RegulatoryChangeMetadata parse_xml(const std::string& xml);

    /**
     * @brief Parse plain text content
     * @param text Plain text content
     * @return Extracted metadata
     */
    RegulatoryChangeMetadata parse_text(const std::string& text);

    /**
     * @brief Extract regulatory body from content
     * @param content Document content
     * @return Regulatory body identifier
     */
    std::string extract_regulatory_body(const std::string& content) const;

    /**
     * @brief Extract document type from content
     * @param content Document content
     * @return Document type
     */
    std::string extract_document_type(const std::string& content) const;

    /**
     * @brief Extract document number/reference
     * @param content Document content
     * @return Document number
     */
    std::string extract_document_number(const std::string& content) const;

    /**
     * @brief Extract affected entities from content
     * @param content Document content
     * @return List of affected entities
     */
    std::vector<std::string> extract_affected_entities(const std::string& content) const;

    /**
     * @brief Utility methods
     */
    std::vector<std::string> split_into_lines(const std::string& content) const;
    std::string to_lowercase(const std::string& str) const;
    std::string strip_html_tags(const std::string& html) const;
    std::string strip_xml_tags(const std::string& xml) const;
    std::string extract_xml_field(const std::string& xml, const std::string& field_pattern) const;
    std::vector<std::string> extract_keywords_from_text(const std::string& text) const;

    // Configuration and dependencies
    std::shared_ptr<ConfigurationManager> config_;
    std::shared_ptr<StructuredLogger> logger_;

    // Parsing statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> documents_parsed_;
    std::atomic<size_t> html_documents_;
    std::atomic<size_t> xml_documents_;
    std::atomic<size_t> text_documents_;
    std::atomic<size_t> parsing_errors_;

    // Parsing patterns and rules
    std::unordered_map<std::string, std::vector<std::string>> regulatory_body_patterns_;
    std::unordered_map<std::string, std::vector<std::string>> document_type_patterns_;
};

} // namespace regulens

