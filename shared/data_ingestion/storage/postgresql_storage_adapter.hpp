/**
 * PostgreSQL Storage Adapter - Production-Grade Data Storage
 *
 * Advanced PostgreSQL storage with:
 * - Connection pooling and transaction management
 * - Dynamic table creation and schema management
 * - Batch inserts and upsert operations
 * - Data partitioning and indexing
 * - Performance monitoring and optimization
 *
 * Retrospective Enhancement: Standardizes data storage across all POCs
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include "../data_ingestion_framework.hpp"
#include "../../database/postgresql_connection.hpp"
#include "../../logging/structured_logger.hpp"
#include <nlohmann/json.hpp>

namespace regulens {

enum class StorageStrategy {
    INSERT_ONLY,
    UPSERT_ON_CONFLICT,
    MERGE_UPDATE,
    BULK_LOAD,
    PARTITIONED_STORAGE
};

enum class IndexStrategy {
    NONE,
    SINGLE_COLUMN,
    COMPOSITE_INDEX,
    PARTIAL_INDEX,
    GIN_INDEX_JSONB,
    GIST_INDEX_SPATIAL
};

enum class PartitionStrategy {
    NONE,
    TIME_BASED,
    RANGE_BASED,
    HASH_BASED,
    LIST_BASED
};

struct StorageTableConfig {
    std::string table_name;
    std::string schema_name = "public";
    StorageStrategy storage_strategy = StorageStrategy::UPSERT_ON_CONFLICT;
    std::vector<std::string> primary_key_columns;
    std::vector<std::string> conflict_columns; // For UPSERT
    std::vector<std::pair<std::string, IndexStrategy>> indexes;
    PartitionStrategy partition_strategy = PartitionStrategy::NONE;
    std::string partition_column;
    std::chrono::seconds partition_interval = std::chrono::hours(24);
    bool enable_audit_trail = true;
    int batch_size = 1000;
    std::chrono::seconds batch_timeout = std::chrono::seconds(30);
};

struct StorageOperation {
    std::string operation_id;
    std::string table_name;
    StorageStrategy strategy;
    std::vector<DataRecord> records;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    int records_processed = 0;
    int records_succeeded = 0;
    int records_failed = 0;
    IngestionStatus status = IngestionStatus::PENDING;
    std::vector<std::string> errors;
    nlohmann::json metadata;
};

class PostgreSQLStorageAdapter : public StorageAdapter {
public:
    PostgreSQLStorageAdapter(std::shared_ptr<ConnectionPool> db_pool, StructuredLogger* logger);

    ~PostgreSQLStorageAdapter() override;

    // StorageAdapter interface implementation
    bool store_batch(const IngestionBatch& batch) override;
    std::vector<DataRecord> retrieve_records(const std::string& source_id,
                                           const std::chrono::system_clock::time_point& start_time,
                                           const std::chrono::system_clock::time_point& end_time) override;
    bool update_record_quality(const std::string& record_id, DataQuality quality) override;

    // Table management
    bool create_table_if_not_exists(const std::string& table_name, const nlohmann::json& schema);
    bool alter_table_schema(const std::string& table_name, const nlohmann::json& changes);
    nlohmann::json get_table_schema(const std::string& table_name);
    std::vector<std::string> list_tables();

    // Storage configuration
    void set_table_config(const std::string& table_name, const StorageTableConfig& config);
    StorageTableConfig get_table_config(const std::string& table_name) const;

    // Batch operations
    StorageOperation store_records_batch(const std::string& table_name, const std::vector<DataRecord>& records);
    bool execute_batch_operation(const StorageOperation& operation);

    // Query operations
    std::vector<nlohmann::json> query_table(const std::string& table_name,
                                          const nlohmann::json& conditions = nullptr,
                                          int limit = 1000,
                                          int offset = 0);
    nlohmann::json aggregate_data(const std::string& table_name,
                                const std::string& group_by,
                                const std::string& aggregate_function,
                                const nlohmann::json& conditions = nullptr);

    // Maintenance operations
    bool create_indexes(const std::string& table_name);
    bool create_partitions(const std::string& table_name);
    bool vacuum_table(const std::string& table_name);
    bool analyze_table(const std::string& table_name);

private:
    // Table operations
    bool create_table_from_schema(const std::string& table_name, const nlohmann::json& schema);
    bool table_exists(const std::string& table_name);
    nlohmann::json infer_schema_from_records(const std::vector<DataRecord>& records);

    // Storage strategies implementation
    bool execute_insert_only(const std::string& table_name, const std::vector<DataRecord>& records, StorageOperation& operation);
    bool execute_upsert(const std::string& table_name, const std::vector<DataRecord>& records, const StorageTableConfig& config, StorageOperation& operation);
    bool execute_merge_update(const std::string& table_name, const std::vector<DataRecord>& records, StorageOperation& operation);
    bool execute_bulk_load(const std::string& table_name, const std::vector<DataRecord>& records, StorageOperation& operation);

    // Data mapping and conversion
    std::vector<std::string> generate_insert_columns(const std::vector<DataRecord>& records);
    std::vector<std::vector<std::string>> generate_insert_values(const std::vector<DataRecord>& records);
    std::string generate_upsert_clause(const StorageTableConfig& config);
    nlohmann::json map_record_to_json(const DataRecord& record);

    // Index management
    bool create_single_column_index(const std::string& table_name, const std::string& column);
    bool create_composite_index(const std::string& table_name, const std::vector<std::string>& columns);
    bool create_partial_index(const std::string& table_name, const std::string& column, const std::string& condition);
    bool create_gin_index(const std::string& table_name, const std::string& column);
    bool create_gist_index(const std::string& table_name, const std::string& column);

    // Partition management
    bool create_time_based_partitions(const std::string& table_name, const StorageTableConfig& config);
    bool create_range_based_partitions(const std::string& table_name, const StorageTableConfig& config);
    bool create_hash_based_partitions(const std::string& table_name, const StorageTableConfig& config);
    bool create_list_based_partitions(const std::string& table_name, const StorageTableConfig& config);
    bool attach_partition(const std::string& parent_table, const std::string& partition_name, const std::string& condition);

    // Transaction management
    bool begin_transaction();
    bool commit_transaction();
    bool rollback_transaction();
    bool execute_in_transaction(const std::function<bool()>& operation);

    // Error handling and recovery
    bool handle_storage_error(const std::string& error, StorageOperation& operation);
    bool retry_failed_operation(StorageOperation& operation, int attempt);
    void log_operation_metrics(const StorageOperation& operation);

    // Performance optimization
    void prepare_statements();
    bool use_prepared_statement(const std::string& statement_name, const std::vector<std::string>& params);
    void optimize_batch_size(const std::string& table_name);
    void analyze_query_performance();

    // Data quality tracking
    bool update_quality_metrics(const std::string& record_id, DataQuality quality);
    nlohmann::json get_quality_distribution(const std::string& table_name);

    // Audit trail
    bool log_storage_operation(const StorageOperation& operation);
    std::vector<nlohmann::json> get_audit_trail(const std::string& table_name,
                                              const std::chrono::system_clock::time_point& start_time,
                                              const std::chrono::system_clock::time_point& end_time);

    // Internal state
    std::unordered_map<std::string, StorageTableConfig> table_configs_;
    std::unordered_map<std::string, nlohmann::json> table_schemas_;
    std::unordered_map<std::string, std::string> prepared_statements_;

    // Performance metrics
    int total_operations_executed_;
    int successful_operations_;
    int failed_operations_;
    std::chrono::microseconds total_operation_time_;
    std::unordered_map<std::string, int> table_operation_counts_;
    std::unordered_map<std::string, std::chrono::microseconds> table_operation_times_;

    // Constants
    const int DEFAULT_BATCH_SIZE = 1000;
    const std::chrono::seconds DEFAULT_BATCH_TIMEOUT = std::chrono::seconds(30);
    const int MAX_RETRY_ATTEMPTS = 3;
    const std::chrono::seconds RETRY_BASE_DELAY = std::chrono::seconds(1);
    const int MAX_PREPARED_STATEMENTS = 50;
};

} // namespace regulens
