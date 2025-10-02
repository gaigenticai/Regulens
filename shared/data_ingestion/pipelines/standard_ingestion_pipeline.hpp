/**
 * Standard Ingestion Pipeline - Data Processing Workflow
 *
 * Production-grade data processing pipeline with:
 * - Multi-stage validation and transformation
 * - Quality assurance and enrichment
 * - Error handling and recovery
 * - Performance monitoring and optimization
 *
 * Retrospective Enhancement: Standardizes data processing across all sources
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <chrono>
#include "../data_ingestion_framework.hpp"
#include "../../logging/structured_logger.hpp"
#include <nlohmann/json.hpp>

namespace regulens {

enum class PipelineStage {
    VALIDATION,
    CLEANING,
    TRANSFORMATION,
    ENRICHMENT,
    QUALITY_CHECK,
    DUPLICATE_DETECTION,
    COMPLIANCE_CHECK,
    STORAGE_PREPARATION
};

enum class ValidationRule {
    REQUIRED_FIELDS,
    DATA_TYPE_CHECK,
    RANGE_CHECK,
    FORMAT_VALIDATION,
    REFERENCE_INTEGRITY,
    BUSINESS_RULES
};

enum class TransformationType {
    FIELD_MAPPING,
    DATA_TYPE_CONVERSION,
    VALUE_NORMALIZATION,
    ENCRYPTION_MASKING,
    AGGREGATION,
    DERIVED_FIELDS
};

struct ValidationRuleConfig {
    std::string rule_name;
    ValidationRule rule_type;
    std::unordered_map<std::string, nlohmann::json> parameters;
    bool fail_on_error = true;
    std::string error_message;
};

struct TransformationConfig {
    std::string transformation_name;
    TransformationType transformation_type;
    std::unordered_map<std::string, std::string> field_mappings;
    std::unordered_map<std::string, nlohmann::json> parameters;
    bool conditional = false;
    std::string condition_field;
    nlohmann::json condition_value;
};

struct EnrichmentRule {
    std::string rule_name;
    std::string target_field;
    std::string source_type; // "lookup_table", "api_call", "calculation"
    nlohmann::json enrichment_config;
    bool cache_results = true;
    std::chrono::seconds cache_ttl = std::chrono::seconds(3600);
};

struct PipelineConfig {
    std::vector<PipelineStage> enabled_stages;
    std::vector<ValidationRuleConfig> validation_rules;
    std::vector<TransformationConfig> transformations;
    std::vector<EnrichmentRule> enrichment_rules;
    bool enable_duplicate_detection = true;
    std::vector<std::string> duplicate_key_fields;
    bool enable_compliance_checking = true;
    nlohmann::json compliance_rules;
    int batch_size = 1000;
    std::chrono::seconds processing_timeout = std::chrono::seconds(300);
    bool enable_error_recovery = true;
    int max_retry_attempts = 3;
};

class StandardIngestionPipeline : public IngestionPipeline {
public:
    StandardIngestionPipeline(const DataIngestionConfig& config, StructuredLogger* logger);

    ~StandardIngestionPipeline() override;

    // IngestionPipeline interface implementation
    IngestionBatch process_batch(const std::vector<nlohmann::json>& raw_data) override;
    bool validate_batch(const IngestionBatch& batch) override;
    nlohmann::json transform_data(const nlohmann::json& data) override;

    // Pipeline configuration and control
    void set_pipeline_config(const PipelineConfig& config);
    bool enable_stage(PipelineStage stage);
    bool disable_stage(PipelineStage stage);
    std::vector<PipelineStage> get_enabled_stages() const;

    // Processing methods
    std::vector<nlohmann::json> validate_data(const std::vector<nlohmann::json>& data);
    std::vector<nlohmann::json> clean_data(const std::vector<nlohmann::json>& data);
    std::vector<nlohmann::json> transform_batch(const std::vector<nlohmann::json>& data);
    std::vector<nlohmann::json> enrich_data(const std::vector<nlohmann::json>& data);
    std::vector<nlohmann::json> check_quality(const std::vector<nlohmann::json>& data);
    std::vector<nlohmann::json> detect_duplicates(const std::vector<nlohmann::json>& data);
    std::vector<nlohmann::json> check_compliance(const std::vector<nlohmann::json>& data);

private:
    // Validation methods
    bool validate_required_fields(const nlohmann::json& data, const ValidationRuleConfig& rule);
    bool validate_data_types(const nlohmann::json& data, const ValidationRuleConfig& rule);
    bool validate_ranges(const nlohmann::json& data, const ValidationRuleConfig& rule);
    bool validate_formats(const nlohmann::json& data, const ValidationRuleConfig& rule);
    bool validate_references(const nlohmann::json& data, const ValidationRuleConfig& rule);
    bool validate_business_rules(const nlohmann::json& data, const ValidationRuleConfig& rule);

    // Transformation methods
    nlohmann::json apply_field_mapping(const nlohmann::json& data, const TransformationConfig& transform);
    nlohmann::json convert_data_types(const nlohmann::json& data, const TransformationConfig& transform);
    nlohmann::json normalize_values(const nlohmann::json& data, const TransformationConfig& transform);
    nlohmann::json apply_encryption_masking(const nlohmann::json& data, const TransformationConfig& transform);
    nlohmann::json perform_aggregation(const nlohmann::json& data, const TransformationConfig& transform);
    nlohmann::json create_derived_fields(const nlohmann::json& data, const TransformationConfig& transform);

    // Enrichment methods
    nlohmann::json enrich_from_lookup_table(const nlohmann::json& data, const EnrichmentRule& rule);
    nlohmann::json enrich_from_api_call(const nlohmann::json& data, const EnrichmentRule& rule);
    nlohmann::json enrich_from_calculation(const nlohmann::json& data, const EnrichmentRule& rule);

    // Quality and compliance
    double calculate_data_quality_score(const nlohmann::json& data);
    bool check_compliance_rules(const nlohmann::json& data, const nlohmann::json& rules);
    std::vector<std::string> identify_data_issues(const nlohmann::json& data);

    // Duplicate detection
    std::string generate_duplicate_key(const nlohmann::json& data, const std::vector<std::string>& key_fields);
    bool is_duplicate(const std::string& duplicate_key);
    void mark_as_processed(const std::string& duplicate_key);

    // Error handling and recovery
    bool handle_validation_error(const nlohmann::json& data, const std::string& error, int attempt);
    bool handle_transformation_error(const nlohmann::json& data, const std::string& error, int attempt);
    IngestionBatch create_error_batch(const std::vector<nlohmann::json>& failed_data, const std::string& error);

    // Performance optimization
    void optimize_pipeline_for_source();
    bool should_skip_stage(PipelineStage stage, const nlohmann::json& data);
    void batch_process_stage(PipelineStage stage, std::vector<nlohmann::json>& data);

    // Monitoring and metrics
    void record_stage_metrics(PipelineStage stage, const std::chrono::microseconds& duration, int records_processed);
    void record_error_metrics(const std::string& error_type, const std::string& error_message);
    nlohmann::json get_pipeline_performance_stats();

    // Caching for performance
    nlohmann::json get_cached_enrichment(const std::string& cache_key);
    void set_cached_enrichment(const std::string& cache_key, const nlohmann::json& data);
    void cleanup_expired_cache();

    // Internal state
    PipelineConfig pipeline_config_;
    std::unordered_set<PipelineStage> enabled_stages_;

    // Caches for performance
    std::unordered_map<std::string, nlohmann::json> enrichment_cache_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> cache_timestamps_;
    std::unordered_set<std::string> processed_duplicate_keys_;

    // Metrics and monitoring
    int total_records_processed_;
    int successful_records_;
    int failed_records_;
    std::chrono::microseconds total_processing_time_;
    std::unordered_map<PipelineStage, std::chrono::microseconds> stage_times_;
    std::unordered_map<std::string, int> error_counts_;

    // Constants
    const std::chrono::seconds CACHE_CLEANUP_INTERVAL = std::chrono::seconds(300);
    const std::chrono::hours CACHE_RETENTION_PERIOD = std::chrono::hours(24);
    const int MAX_DUPLICATE_KEY_CACHE_SIZE = 100000;
};

} // namespace regulens
