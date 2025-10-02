/**
 * Data Ingestion Framework - Production-Grade Multi-Source Data Pipeline
 *
 * Unified framework for ingesting data from diverse sources with quality assurance,
 * transformation, and storage. Builds upon existing HTTP clients and database
 * connections while providing extensible architecture for future data sources.
 *
 * Retrospective Benefits:
 * - Enhances existing regulatory monitoring with standardized ingestion
 * - Improves HTTP client usage with connection pooling and retry logic
 * - Standardizes database operations across all POCs
 * - Provides foundation for real-time event processing
 */

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <queue>
#include <thread>
#include <atomic>
#include <chrono>
#include "../database/postgresql_connection.hpp"
#include "../network/http_client.hpp"
#include "../logging/structured_logger.hpp"
#include <nlohmann/json.hpp>

namespace regulens {

// Forward declarations
class DataSource;
class IngestionPipeline;
class DataTransformer;
class DataValidator;
class StorageAdapter;
class IngestionMetrics;

enum class DataSourceType {
    API_REST,
    API_GRAPHQL,
    DATABASE_SQL,
    DATABASE_NOSQL,
    FILE_CSV,
    FILE_JSON,
    FILE_XML,
    MESSAGE_QUEUE,
    WEBSOCKET_STREAM,
    WEB_SCRAPING,
    EMAIL_IMAP,
    FTP_SFTP
};

enum class IngestionMode {
    BATCH,
    STREAMING,
    REAL_TIME,
    SCHEDULED
};

enum class DataQuality {
    RAW,
    VALIDATED,
    TRANSFORMED,
    ENRICHED,
    GOLD_STANDARD
};

enum class IngestionStatus {
    PENDING,
    PROCESSING,
    COMPLETED,
    FAILED,
    RETRYING,
    CANCELLED
};

struct DataIngestionConfig {
    std::string source_id;
    std::string source_name;
    DataSourceType source_type;
    IngestionMode mode;
    std::chrono::seconds poll_interval;
    int max_retries = 3;
    std::chrono::seconds retry_delay = std::chrono::seconds(30);
    int batch_size = 100;
    std::unordered_map<std::string, std::string> connection_params;
    nlohmann::json source_config;
    nlohmann::json transformation_rules;
    nlohmann::json validation_rules;
};

struct IngestionBatch {
    std::string batch_id;
    std::string source_id;
    IngestionStatus status;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    int records_processed = 0;
    int records_succeeded = 0;
    int records_failed = 0;
    std::vector<nlohmann::json> raw_data;
    std::vector<nlohmann::json> processed_data;
    std::vector<std::string> errors;
    nlohmann::json metadata;
};

struct DataRecord {
    std::string record_id;
    std::string source_id;
    DataQuality quality;
    nlohmann::json data;
    std::chrono::system_clock::time_point ingested_at;
    std::chrono::system_clock::time_point processed_at;
    std::string processing_pipeline;
    nlohmann::json metadata;
    std::vector<std::string> tags;
};

// Core Data Ingestion Framework
class DataIngestionFramework {
public:
    DataIngestionFramework(
        std::shared_ptr<ConnectionPool> db_pool,
        std::shared_ptr<HttpClient> http_client,
        StructuredLogger* logger
    );

    ~DataIngestionFramework();

    // Framework Lifecycle
    bool initialize();
    void shutdown();
    bool is_running() const;

    // Source Management
    bool register_data_source(const DataIngestionConfig& config);
    bool unregister_data_source(const std::string& source_id);
    std::vector<std::string> list_data_sources() const;
    std::optional<DataIngestionConfig> get_source_config(const std::string& source_id) const;

    // Ingestion Control
    bool start_ingestion(const std::string& source_id);
    bool stop_ingestion(const std::string& source_id);
    bool pause_ingestion(const std::string& source_id);
    bool resume_ingestion(const std::string& source_id);

    // Data Operations
    std::vector<DataRecord> ingest_data(const std::string& source_id, const nlohmann::json& data);
    std::vector<DataRecord> process_batch(const IngestionBatch& batch);
    bool store_records(const std::vector<DataRecord>& records);

    // Query and Analytics
    std::vector<DataRecord> query_records(const std::string& source_id,
                                        const std::chrono::system_clock::time_point& start_time,
                                        const std::chrono::system_clock::time_point& end_time);
    nlohmann::json get_ingestion_stats(const std::string& source_id);
    nlohmann::json get_framework_health();

    // Retrospective Enhancement APIs (for existing regulatory monitoring)
    bool enhance_regulatory_monitoring(const std::string& regulatory_source_id);
    bool migrate_existing_data(const std::string& source_id);
    std::vector<nlohmann::json> get_backlog_data(const std::string& source_id);

private:
    // Internal Components
    std::unique_ptr<DataSource> create_data_source(const DataIngestionConfig& config);
    std::unique_ptr<IngestionPipeline> create_pipeline(const DataIngestionConfig& config);
    std::unique_ptr<StorageAdapter> get_storage_adapter(const std::string& source_id);

    // Processing Threads
    void processing_worker();
    void monitoring_worker();
    void cleanup_worker();

    // Batch Management
    std::string generate_batch_id();
    void queue_batch(const IngestionBatch& batch);
    IngestionBatch dequeue_batch();

    // Health and Monitoring
    void update_metrics(const std::string& source_id, const IngestionBatch& batch);
    void check_source_health();
    void handle_failed_batches();

    // Internal State
    std::shared_ptr<ConnectionPool> db_pool_;
    std::shared_ptr<HttpClient> http_client_;
    StructuredLogger* logger_;

    std::unordered_map<std::string, DataIngestionConfig> source_configs_;
    std::unordered_map<std::string, std::unique_ptr<DataSource>> active_sources_;
    std::unordered_map<std::string, std::unique_ptr<IngestionPipeline>> active_pipelines_;

    std::queue<IngestionBatch> batch_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    std::vector<std::thread> worker_threads_;
    std::thread monitoring_thread_;
    std::thread cleanup_thread_;

    std::unique_ptr<IngestionMetrics> metrics_;
    std::atomic<bool> running_;
    std::atomic<int> active_workers_;

    const int MAX_WORKER_THREADS = 8;
    const int BATCH_QUEUE_SIZE = 1000;
    const std::chrono::seconds HEALTH_CHECK_INTERVAL = std::chrono::seconds(30);
};

// Data Source Interface
class DataSource {
public:
    DataSource(const DataIngestionConfig& config,
               std::shared_ptr<HttpClient> http_client,
               StructuredLogger* logger);
    virtual ~DataSource() = default;

    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    virtual std::vector<nlohmann::json> fetch_data() = 0;
    virtual bool validate_connection() = 0;

    const std::string& get_source_id() const { return config_.source_id; }
    DataSourceType get_source_type() const { return config_.source_type; }

protected:
    DataIngestionConfig config_;
    std::shared_ptr<HttpClient> http_client_;
    StructuredLogger* logger_;
};

// Ingestion Pipeline Interface
class IngestionPipeline {
public:
    IngestionPipeline(const DataIngestionConfig& config, StructuredLogger* logger);
    virtual ~IngestionPipeline() = default;

    virtual IngestionBatch process_batch(const std::vector<nlohmann::json>& raw_data) = 0;
    virtual bool validate_batch(const IngestionBatch& batch) = 0;
    virtual nlohmann::json transform_data(const nlohmann::json& data) = 0;

protected:
    DataIngestionConfig config_;
    StructuredLogger* logger_;
};

// Storage Adapter Interface
class StorageAdapter {
public:
    StorageAdapter(std::shared_ptr<ConnectionPool> db_pool, StructuredLogger* logger);
    virtual ~StorageAdapter() = default;

    virtual bool store_batch(const IngestionBatch& batch) = 0;
    virtual std::vector<DataRecord> retrieve_records(const std::string& source_id,
                                                   const std::chrono::system_clock::time_point& start_time,
                                                   const std::chrono::system_clock::time_point& end_time) = 0;
    virtual bool update_record_quality(const std::string& record_id, DataQuality quality) = 0;

protected:
    std::shared_ptr<ConnectionPool> db_pool_;
    StructuredLogger* logger_;
};

// Metrics and Monitoring
class IngestionMetrics {
public:
    IngestionMetrics(StructuredLogger* logger);

    void record_batch_processed(const std::string& source_id, const IngestionBatch& batch);
    void record_ingestion_error(const std::string& source_id, const std::string& error);
    void record_source_health(const std::string& source_id, bool healthy);

    nlohmann::json get_source_metrics(const std::string& source_id) const;
    nlohmann::json get_global_metrics() const;
    std::vector<std::string> get_failing_sources() const;

private:
    StructuredLogger* logger_;
    mutable std::mutex metrics_mutex_;

    struct SourceMetrics {
        int64_t total_batches = 0;
        int64_t successful_batches = 0;
        int64_t failed_batches = 0;
        int64_t total_records = 0;
        int64_t successful_records = 0;
        int64_t failed_records = 0;
        std::chrono::system_clock::time_point last_successful_batch;
        bool is_healthy = true;
        std::vector<std::string> recent_errors;
    };

    std::unordered_map<std::string, SourceMetrics> source_metrics_;
};

} // namespace regulens
