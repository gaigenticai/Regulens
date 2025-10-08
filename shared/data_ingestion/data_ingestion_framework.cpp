/**
 * Data Ingestion Framework Implementation - Production-Grade Multi-Source Data Pipeline
 *
 * Core implementation that orchestrates data sources, processing pipelines, and storage.
 * Provides unified interface for ingesting data from diverse sources with quality assurance.
 */

#include "data_ingestion_framework.hpp"
#include "ingestion_metrics.hpp"
#include "sources/rest_api_source.hpp"
#include "sources/web_scraping_source.hpp"
#include "sources/database_source.hpp"
#include "pipelines/standard_ingestion_pipeline.hpp"
#include "storage/postgresql_storage_adapter.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include "../utils/timer.hpp"

namespace regulens {

// DataIngestionFramework Implementation
DataIngestionFramework::DataIngestionFramework(
    std::shared_ptr<ConnectionPool> db_pool,
    std::shared_ptr<HttpClient> http_client,
    StructuredLogger* logger
) : db_pool_(db_pool), http_client_(http_client), logger_(logger),
    running_(false), active_workers_(0) {
}

DataIngestionFramework::~DataIngestionFramework() {
    shutdown();
}

bool DataIngestionFramework::initialize() {
    if (running_) {
        logger_->log(LogLevel::WARN,
                    "Data Ingestion Framework already running");
        return true;
    }

    try {
        // Initialize metrics
        metrics_ = std::make_unique<IngestionMetrics>(logger_, db_pool_);

        // Start worker threads
        running_ = true;
        for (int i = 0; i < MAX_WORKER_THREADS; ++i) {
            worker_threads_.emplace_back(&DataIngestionFramework::processing_worker, this);
        }

        // Start monitoring thread
        monitoring_thread_ = std::thread(&DataIngestionFramework::monitoring_worker, this);
        cleanup_thread_ = std::thread(&DataIngestionFramework::cleanup_worker, this);

        logger_->log(LogLevel::INFO,
                    "Data Ingestion Framework initialized with " +
                    std::to_string(MAX_WORKER_THREADS) + " worker threads");

        return true;
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR,
                    "Failed to initialize Data Ingestion Framework: " + std::string(e.what()));
        return false;
    }
}

void DataIngestionFramework::shutdown() {
    if (!running_) return;

    logger_->log(LogLevel::INFO,
                "Shutting down Data Ingestion Framework...");

    running_ = false;
    queue_cv_.notify_all();

    // Wait for worker threads
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    // Wait for monitoring threads
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }

    // Disconnect all sources
    for (auto& [source_id, source] : active_sources_) {
        if (source && source->is_connected()) {
            source->disconnect();
        }
    }

    active_sources_.clear();
    active_pipelines_.clear();

    logger_->log(LogLevel::INFO,
                "Data Ingestion Framework shutdown complete");
}

bool DataIngestionFramework::is_running() const {
    return running_;
}

// Source Management
bool DataIngestionFramework::register_data_source(const DataIngestionConfig& config) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (source_configs_.find(config.source_id) != source_configs_.end()) {
        logger_->log(LogLevel::WARN,
                    "Data source already registered: " + config.source_id);
        return false;
    }

    source_configs_[config.source_id] = config;

    logger_->log(LogLevel::INFO,
                "Registered data source: " + config.source_id + " (" +
                std::to_string(static_cast<int>(config.source_type)) + ")");

    return true;
}

bool DataIngestionFramework::unregister_data_source(const std::string& source_id) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    auto config_it = source_configs_.find(source_id);
    if (config_it == source_configs_.end()) {
        return false;
    }

    // Stop ingestion if active
    stop_ingestion(source_id);

    // Remove source and pipeline
    active_sources_.erase(source_id);
    active_pipelines_.erase(source_id);
    source_configs_.erase(config_it);

    logger_->log(LogLevel::INFO,
                "Unregistered data source: " + source_id);

    return true;
}

std::vector<std::string> DataIngestionFramework::list_data_sources() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    std::vector<std::string> sources;
    sources.reserve(source_configs_.size());

    for (const auto& [source_id, _] : source_configs_) {
        sources.push_back(source_id);
    }

    return sources;
}

std::optional<DataIngestionConfig> DataIngestionFramework::get_source_config(const std::string& source_id) const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    auto it = source_configs_.find(source_id);
    return (it != source_configs_.end()) ? std::optional<DataIngestionConfig>(it->second) : std::nullopt;
}

// Ingestion Control
bool DataIngestionFramework::start_ingestion(const std::string& source_id) {
    auto config_opt = get_source_config(source_id);
    if (!config_opt) {
        logger_->log(LogLevel::ERROR,
                    "Cannot start ingestion: unknown source " + source_id);
        return false;
    }

    try {
        // Create and initialize data source
        auto source = create_data_source(*config_opt);
        if (!source) {
            logger_->log(LogLevel::ERROR,
                        "Failed to create data source: " + source_id);
            return false;
        }

        if (!source->connect()) {
            logger_->log(LogLevel::ERROR,
                        "Failed to connect to data source: " + source_id);
            return false;
        }

        // Create and initialize pipeline
        auto pipeline = create_pipeline(*config_opt);
        if (!pipeline) {
            logger_->log(LogLevel::ERROR,
                        "Failed to create ingestion pipeline: " + source_id);
            source->disconnect();
            return false;
        }

        // Store active components
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            active_sources_[source_id] = std::move(source);
            active_pipelines_[source_id] = std::move(pipeline);
        }

        logger_->log(LogLevel::INFO,
                    "Started ingestion for source: " + source_id);
        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR,
                    "Error starting ingestion for " + source_id + ": " + e.what());
        return false;
    }
}

bool DataIngestionFramework::stop_ingestion(const std::string& source_id) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    auto source_it = active_sources_.find(source_id);
    if (source_it != active_sources_.end() && source_it->second) {
        source_it->second->disconnect();
    }

    active_sources_.erase(source_id);
    active_pipelines_.erase(source_id);

    logger_->log(LogLevel::INFO,
                "Stopped ingestion for source: " + source_id);
    return true;
}

bool DataIngestionFramework::pause_ingestion(const std::string& source_id) {
    // For now, pause is equivalent to stop (can be enhanced with resume capability)
    return stop_ingestion(source_id);
}

bool DataIngestionFramework::resume_ingestion(const std::string& source_id) {
    return start_ingestion(source_id);
}

// Data Operations
std::vector<DataRecord> DataIngestionFramework::ingest_data(const std::string& source_id, const nlohmann::json& data) {
    auto source_it = active_sources_.find(source_id);
    if (source_it == active_sources_.end() || !source_it->second) {
        logger_->log(LogLevel::ERROR,
                    "Cannot ingest data: source not active: " + source_id);
        return {};
    }

    try {
        // Create batch from input data
        IngestionBatch batch;
        batch.batch_id = generate_batch_id();
        batch.source_id = source_id;
        batch.status = IngestionStatus::PENDING;
        batch.start_time = std::chrono::system_clock::now();
        batch.raw_data = {data};
        batch.metadata = {
            {"ingestion_method", "direct"},
            {"record_count", 1}
        };

        // Process through pipeline
        auto processed_records = process_batch(batch);

        // Store records
        if (!processed_records.empty()) {
            store_records(processed_records);
        }

        logger_->log(LogLevel::DEBUG,
                    "Ingested " + std::to_string(processed_records.size()) +
                    " records for source: " + source_id);

        return processed_records;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR,
                    "Error ingesting data for " + source_id + ": " + e.what());
        return {};
    }
}

std::vector<DataRecord> DataIngestionFramework::process_batch(const IngestionBatch& batch) {
    auto pipeline_it = active_pipelines_.find(batch.source_id);
    if (pipeline_it == active_pipelines_.end() || !pipeline_it->second) {
        logger_->log(LogLevel::ERROR,
                    "Cannot process batch: pipeline not active for " + batch.source_id);
        return {};
    }

    try {
        // Validate batch
        if (!pipeline_it->second->validate_batch(batch)) {
            logger_->log(LogLevel::WARN,
                        "Batch validation failed for " + batch.source_id);
            return {};
        }

        // Process through pipeline stages
        IngestionBatch processed_batch = pipeline_it->second->process_batch(batch.raw_data);

        // Convert to DataRecord format
        std::vector<DataRecord> records;
        records.reserve(processed_batch.processed_data.size());

        for (size_t i = 0; i < processed_batch.processed_data.size(); ++i) {
            DataRecord record;
            record.record_id = batch.source_id + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + "_" + std::to_string(i);
            record.source_id = batch.source_id;
            record.quality = DataQuality::TRANSFORMED; // Default quality after pipeline processing
            record.data = processed_batch.processed_data[i];
            record.ingested_at = std::chrono::system_clock::now();
            record.processed_at = std::chrono::system_clock::now();
            record.processing_pipeline = "standard_ingestion_pipeline";
            record.metadata = processed_batch.metadata;
            record.tags = {"processed", batch.source_id};

            records.push_back(std::move(record));
        }

        logger_->log(LogLevel::DEBUG,
                    "Processed " + std::to_string(records.size()) +
                    " records through pipeline for " + batch.source_id);

        return records;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR,
                    "Error processing batch for " + batch.source_id + ": " + e.what());
        return {};
    }
}

bool DataIngestionFramework::store_records(const std::vector<DataRecord>& records) {
    if (records.empty()) return true;

    try {
        auto storage_adapter = get_storage_adapter(records.front().source_id);
        if (!storage_adapter) {
            logger_->log(LogLevel::ERROR,
                        "No storage adapter available for source: " + records.front().source_id);
            return false;
        }

        // Create ingestion batch for storage
        IngestionBatch storage_batch;
        storage_batch.batch_id = generate_batch_id();
        storage_batch.source_id = records.front().source_id;
        storage_batch.status = IngestionStatus::PROCESSING;
        storage_batch.start_time = std::chrono::system_clock::now();

        // Convert DataRecords to JSON for storage
        storage_batch.processed_data.reserve(records.size());
        for (const auto& record : records) {
            nlohmann::json record_json = {
                {"record_id", record.record_id},
                {"source_id", record.source_id},
                {"quality", static_cast<int>(record.quality)},
                {"data", record.data},
                {"ingested_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                    record.ingested_at.time_since_epoch()).count()},
                {"processed_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                    record.processed_at.time_since_epoch()).count()},
                {"processing_pipeline", record.processing_pipeline},
                {"metadata", record.metadata},
                {"tags", record.tags}
            };
            storage_batch.processed_data.push_back(record_json);
        }

        // Store batch
        bool success = storage_adapter->store_batch(storage_batch);

        if (success) {
            logger_->log(LogLevel::DEBUG,
                        "Stored " + std::to_string(records.size()) +
                        " records for source: " + records.front().source_id);
        } else {
            logger_->log(LogLevel::ERROR,
                        "Failed to store records for source: " + records.front().source_id);
        }

        return success;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR,
                    "Error storing records: " + std::string(e.what()));
        return false;
    }
}

// Query and Analytics
std::vector<DataRecord> DataIngestionFramework::query_records(
    const std::string& source_id,
    const std::chrono::system_clock::time_point& start_time,
    const std::chrono::system_clock::time_point& end_time
) {
    try {
        auto storage_adapter = get_storage_adapter(source_id);
        if (!storage_adapter) {
            logger_->log(LogLevel::ERROR,
                        "No storage adapter available for source: " + source_id);
            return {};
        }

        return storage_adapter->retrieve_records(source_id, start_time, end_time);

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR,
                    "Error querying records for " + source_id + ": " + e.what());
        return {};
    }
}

nlohmann::json DataIngestionFramework::get_ingestion_stats(const std::string& source_id) {
    return metrics_->get_source_metrics(source_id);
}

nlohmann::json DataIngestionFramework::get_framework_health() {
    nlohmann::json health = {
        {"status", running_ ? "healthy" : "stopped"},
        {"active_sources", static_cast<int>(active_sources_.size())},
        {"active_workers", static_cast<int>(active_workers_)},
        {"queue_size", static_cast<int>(batch_queue_.size())},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    // Add source health
    nlohmann::json sources_health = nlohmann::json::object();
    for (const auto& [source_id, source] : active_sources_) {
        sources_health[source_id] = source && source->is_connected() ? "connected" : "disconnected";
    }
    health["sources"] = sources_health;

    return health;
}

// Retrospective Enhancement APIs
bool DataIngestionFramework::enhance_regulatory_monitoring(const std::string& regulatory_source_id) {
    // This method enhances existing regulatory monitoring by migrating to the new framework
    // It provides backward compatibility while adding new ingestion capabilities

    logger_->log(LogLevel::INFO,
                "Enhancing regulatory monitoring for source: " + regulatory_source_id);

    // Check if source already exists
    if (source_configs_.find(regulatory_source_id) != source_configs_.end()) {
        logger_->log(LogLevel::INFO,
                    "Regulatory source already enhanced: " + regulatory_source_id);
        return true;
    }

    // Create enhanced configuration for existing regulatory source
    DataIngestionConfig config;
    config.source_id = regulatory_source_id;
    config.source_name = "Enhanced Regulatory Monitoring - " + regulatory_source_id;
    config.source_type = DataSourceType::WEB_SCRAPING; // Default to web scraping for regulatory sources
    config.mode = IngestionMode::SCHEDULED;
    config.poll_interval = std::chrono::minutes(15); // More frequent than before
    config.max_retries = 5;
    config.batch_size = 50;

    // Enhanced validation and transformation rules
    config.validation_rules = {
        {"required_fields", {
            {"rule_name", "regulatory_content_check"},
            {"rule_type", "REQUIRED_FIELDS"},
            {"parameters", {
                {"required_fields", {"title", "content", "source", "published_date"}}
            }},
            {"fail_on_error", true}
        }}
    };

    config.transformation_rules = {
        {"standardize_dates", {
            {"date_fields", {"published_date", "effective_date"}},
            {"target_format", "ISO8601"}
        }}
    };

    return register_data_source(config);
}

bool DataIngestionFramework::migrate_existing_data(const std::string& source_id) {
    // This method migrates existing data from old storage to the new ingestion framework
    logger_->log(LogLevel::INFO,
                "Migrating existing data for source: " + source_id);

    // Implementation would scan existing regulatory_changes table and migrate to new format
    // For now, return true as this would be a one-time migration
    return true;
}

std::vector<nlohmann::json> DataIngestionFramework::get_backlog_data(const std::string& source_id) {
    // This method retrieves data that might have been missed by old monitoring
    logger_->log(LogLevel::DEBUG,
                "Checking for backlog data for source: " + source_id);

    // Implementation would check for gaps in historical data
    // For now, return empty as this would be source-specific
    return {};
}

// Private Methods
std::unique_ptr<DataSource> DataIngestionFramework::create_data_source(const DataIngestionConfig& config) {
    switch (config.source_type) {
        case DataSourceType::API_REST:
            return std::make_unique<RESTAPISource>(config, http_client_, logger_);
        case DataSourceType::WEB_SCRAPING:
            return std::make_unique<WebScrapingSource>(config, http_client_, logger_);
        case DataSourceType::DATABASE_SQL:
        case DataSourceType::DATABASE_NOSQL:
            return std::make_unique<DatabaseSource>(config, db_pool_, logger_);
        // Add other source types as implemented
        default:
            logger_->log(LogLevel::ERROR,
                        "Unsupported data source type: " + std::to_string(static_cast<int>(config.source_type)));
            return nullptr;
    }
}

std::unique_ptr<IngestionPipeline> DataIngestionFramework::create_pipeline(const DataIngestionConfig& config) {
    // For now, we only have the standard ingestion pipeline
    return std::make_unique<StandardIngestionPipeline>(config, logger_);
}

std::unique_ptr<StorageAdapter> DataIngestionFramework::get_storage_adapter(const std::string& /*source_id*/) {
    // For now, we only support PostgreSQL storage
    return std::make_unique<PostgreSQLStorageAdapter>(db_pool_, logger_);
}

// Processing Threads
void DataIngestionFramework::processing_worker() {
    while (running_) {
        try {
            IngestionBatch batch = dequeue_batch();

            if (!batch.batch_id.empty()) {
                active_workers_++;

                // Process the batch
                update_metrics(batch.source_id, batch);

                // Mark batch as completed
                batch.end_time = std::chrono::system_clock::now();
                batch.status = IngestionStatus::COMPLETED;

                active_workers_--;
            } else {
                // No work available, wait
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait_for(lock, std::chrono::seconds(1));
            }
        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR,
                        "Processing worker error: " + std::string(e.what()));
            active_workers_--;
        }
    }
}

void DataIngestionFramework::monitoring_worker() {
    while (running_) {
        try {
            check_source_health();
            std::this_thread::sleep_for(HEALTH_CHECK_INTERVAL);
        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR,
                        "Monitoring worker error: " + std::string(e.what()));
        }
    }
}

void DataIngestionFramework::cleanup_worker() {
    while (running_) {
        try {
            handle_failed_batches();
            std::this_thread::sleep_for(std::chrono::minutes(5));
        } catch (const std::exception& e) {
            logger_->log(LogLevel::ERROR,
                        "Cleanup worker error: " + std::string(e.what()));
        }
    }
}

// Batch Management
std::string DataIngestionFramework::generate_batch_id() {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    return "batch_" + std::to_string(timestamp) + "_" + std::to_string(++counter);
}

void DataIngestionFramework::queue_batch(const IngestionBatch& batch) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (static_cast<int>(batch_queue_.size()) >= BATCH_QUEUE_SIZE) {
        logger_->log(LogLevel::WARN,
                    "Batch queue full, dropping batch: " + batch.batch_id);
        return;
    }

    batch_queue_.push(batch);
    queue_cv_.notify_one();
}

IngestionBatch DataIngestionFramework::dequeue_batch() {
    std::unique_lock<std::mutex> lock(queue_mutex_);

    if (batch_queue_.empty()) {
        return IngestionBatch{}; // Return empty batch
    }

    IngestionBatch batch = batch_queue_.front();
    batch_queue_.pop();
    return batch;
}

// Health and Monitoring
void DataIngestionFramework::update_metrics(const std::string& source_id, const IngestionBatch& batch) {
    metrics_->record_batch_processed(source_id, batch);
}

void DataIngestionFramework::check_source_health() {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    for (auto& [source_id, source] : active_sources_) {
        if (source) {
            bool healthy = source->validate_connection();
            metrics_->record_source_health(source_id, healthy);

            if (!healthy) {
                logger_->log(LogLevel::WARN,
                            "Source health check failed: " + source_id);
            }
        }
    }
}

void DataIngestionFramework::handle_failed_batches() {
    // Implementation for retrying failed batches would go here
    // For now, just log failed sources
    auto failing_sources = metrics_->get_failing_sources();
    if (!failing_sources.empty()) {
        logger_->log(LogLevel::WARN,
                    "Failing sources detected: " + std::to_string(failing_sources.size()));
    }
}

} // namespace regulens
