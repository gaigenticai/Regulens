/**
 * Regulatory Knowledge Base - Production Implementation
 *
 * Enterprise-grade regulatory knowledge management with PostgreSQL backend,
 * connection pooling, full-text search, versioning, and ACID guarantees.
 */

#include "regulatory_knowledge_base.hpp"
#include "database/postgresql_connection.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <regex>
#include <set>
#include <thread>

namespace regulens {

// Helper class for database operations with retry logic and exponential backoff
class DatabaseOperationExecutor {
public:
    DatabaseOperationExecutor(std::shared_ptr<ConnectionPool> pool,
                            std::shared_ptr<StructuredLogger> logger,
                            int max_retries = 3)
        : pool_(pool), logger_(logger), max_retries_(max_retries) {}

    template<typename Func>
    bool execute_with_retry(const std::string& operation_name, Func&& func) {
        int retry_count = 0;
        std::chrono::milliseconds backoff(100);

        while (retry_count < max_retries_) {
            try {
                auto conn = pool_->get_connection();
                if (!conn) {
                logger_->error("Failed to acquire database connection",
                             "DatabaseOperationExecutor", "execute_with_retry",
                             {{"operation", operation_name},
                              {"retry", std::to_string(retry_count)}});
                    retry_count++;
                    std::this_thread::sleep_for(backoff);
                    backoff *= 2;
                    continue;
                }

                bool result = func(conn);
                pool_->return_connection(conn);

                if (result) {
                    return true;
                }

                // Operation failed, retry
                retry_count++;
                if (retry_count < max_retries_) {
                    logger_->warn("Database operation failed, retrying",
                                "DatabaseOperationExecutor", "execute_with_retry",
                                {{"operation", operation_name},
                                 {"retry", std::to_string(retry_count)},
                                 {"max_retries", std::to_string(max_retries_)}});
                    std::this_thread::sleep_for(backoff);
                    backoff *= 2;
                }
            } catch (const std::exception& e) {
                logger_->error("Database operation exception",
                             "DatabaseOperationExecutor", "execute_with_retry",
                             {{"operation", operation_name},
                              {"error", e.what()},
                              {"retry", std::to_string(retry_count)}});
                retry_count++;
                if (retry_count < max_retries_) {
                    std::this_thread::sleep_for(backoff);
                    backoff *= 2;
                }
            }
        }

        logger_->error("Database operation failed after all retries",
                     "DatabaseOperationExecutor", "execute_with_retry",
                     {{"operation", operation_name},
                      {"retries", std::to_string(max_retries_)}});
        return false;
    }

    template<typename ResultType, typename Func>
    std::optional<ResultType> execute_query_with_retry(
        const std::string& operation_name, Func&& func) {
        int retry_count = 0;
        std::chrono::milliseconds backoff(100);

        while (retry_count < max_retries_) {
            try {
                auto conn = pool_->get_connection();
                if (!conn) {
                    retry_count++;
                    std::this_thread::sleep_for(backoff);
                    backoff *= 2;
                    continue;
                }

                auto result = func(conn);
                pool_->return_connection(conn);

                if (result) {
                    return result;
                }

                retry_count++;
                if (retry_count < max_retries_) {
                    std::this_thread::sleep_for(backoff);
                    backoff *= 2;
                }
            } catch (const std::exception& e) {
                logger_->error("Query execution exception",
                             "DatabaseOperationExecutor", "execute_query_with_retry",
                             {{"operation", operation_name},
                              {"error", e.what()}});
                retry_count++;
                if (retry_count < max_retries_) {
                    std::this_thread::sleep_for(backoff);
                    backoff *= 2;
                }
            }
        }

        return std::nullopt;
    }

private:
    std::shared_ptr<ConnectionPool> pool_;
    std::shared_ptr<StructuredLogger> logger_;
    int max_retries_;
};

// Private implementation class (PIMPL pattern for better encapsulation)
class RegulatoryKnowledgeBase::RegulatoryKnowledgeBaseImpl {
public:
    std::shared_ptr<ConnectionPool> connection_pool;
    std::unique_ptr<DatabaseOperationExecutor> db_executor;
    std::shared_ptr<ConfigurationManager> config;
    std::shared_ptr<StructuredLogger> logger;

    // Cache for frequently accessed data
    struct CacheEntry {
        RegulatoryChange change;
        std::chrono::system_clock::time_point cached_at;
        
        // Explicit constructor
        CacheEntry(const RegulatoryChange& c, std::chrono::system_clock::time_point t)
            : change(c), cached_at(t) {}
        
        // Copy constructor
        CacheEntry(const CacheEntry& other)
            : change(other.change), cached_at(other.cached_at) {}
        
        // Move constructor
        CacheEntry(CacheEntry&& other) noexcept
            : change(std::move(other.change)), cached_at(other.cached_at) {}
        
        // Copy assignment
        CacheEntry& operator=(const CacheEntry& other) {
            if (this != &other) {
                change = other.change;
                cached_at = other.cached_at;
            }
            return *this;
        }
        
        // Move assignment
        CacheEntry& operator=(CacheEntry&& other) noexcept {
            if (this != &other) {
                change = std::move(other.change);
                cached_at = other.cached_at;
            }
            return *this;
        }
    };
    mutable std::unordered_map<std::string, CacheEntry> change_cache;
    mutable std::mutex cache_mutex;
    std::chrono::minutes cache_ttl{15};

    // Statistics
    std::atomic<size_t> total_queries{0};
    std::atomic<size_t> cache_hits{0};
    std::atomic<size_t> cache_misses{0};

    bool is_cache_valid(const CacheEntry& entry) const {
        auto now = std::chrono::system_clock::now();
        return (now - entry.cached_at) < cache_ttl;
    }

    void update_cache(const std::string& change_id, const RegulatoryChange& change) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        change_cache.insert_or_assign(change_id, CacheEntry(change, std::chrono::system_clock::now()));

        // Limit cache size to prevent memory bloat
        if (change_cache.size() > 1000) {
            // Remove oldest entries
            auto oldest_it = change_cache.begin();
            auto oldest_time = oldest_it->second.cached_at;

            for (auto it = change_cache.begin(); it != change_cache.end(); ++it) {
                if (it->second.cached_at < oldest_time) {
                    oldest_it = it;
                    oldest_time = it->second.cached_at;
                }
            }
            change_cache.erase(oldest_it);
        }
    }

    void invalidate_cache(const std::string& change_id) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        change_cache.erase(change_id);
    }

    void clear_cache() {
        std::lock_guard<std::mutex> lock(cache_mutex);
        change_cache.clear();
    }
};

RegulatoryKnowledgeBase::RegulatoryKnowledgeBase(
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<StructuredLogger> logger)
    : config_(config),
      logger_(logger),
      total_changes_(0),
      high_impact_changes_(0),
      critical_changes_(0),
      max_changes_in_memory_(10000),
      enable_persistence_(true) {

    logger_->info("Initializing RegulatoryKnowledgeBase");
}

RegulatoryKnowledgeBase::~RegulatoryKnowledgeBase() {
    shutdown();
}

bool RegulatoryKnowledgeBase::initialize() {
    logger_->info("Initializing regulatory knowledge base");

    try {
        // Get database configuration
        DatabaseConfig db_config = config_->get_database_config();

        // Initialize connection pool
        auto pool = std::make_shared<ConnectionPool>(db_config);

        // Create implementation
        pimpl_ = std::make_unique<RegulatoryKnowledgeBaseImpl>();
        pimpl_->connection_pool = pool;
        pimpl_->config = config_;
        pimpl_->logger = logger_;
        pimpl_->db_executor = std::make_unique<DatabaseOperationExecutor>(
            pool, logger_, 3);

        // Initialize database schema
        bool schema_created = pimpl_->db_executor->execute_with_retry(
            "create_schema",
            [this](std::shared_ptr<PostgreSQLConnection> conn) {
                return create_database_schema(conn);
            });

        if (!schema_created) {
            logger_->error("Failed to create database schema");
            return false;
        }

        // Load initial statistics
        load_statistics();

        logger_->info("RegulatoryKnowledgeBase initialized successfully",
                     "RegulatoryKnowledgeBase", "initialize",
                     {{"total_changes", std::to_string(total_changes_.load())}});
        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to initialize knowledge base",
                     "RegulatoryKnowledgeBase", "initialize",
                     {{"error", e.what()}});
        return false;
    }
}

bool RegulatoryKnowledgeBase::create_database_schema(
    std::shared_ptr<PostgreSQLConnection> conn) {

    // Main regulatory changes table with comprehensive schema
    std::string create_main_table = R"SQL(
        CREATE TABLE IF NOT EXISTS regulatory_changes (
            change_id VARCHAR(255) PRIMARY KEY,
            source_id VARCHAR(255) NOT NULL,
            title TEXT NOT NULL,
            content_url TEXT NOT NULL,
            status INTEGER NOT NULL,
            detected_at BIGINT NOT NULL,
            analyzed_at BIGINT,
            distributed_at BIGINT,
            version INTEGER DEFAULT 1,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            CONSTRAINT unique_source_id UNIQUE (source_id)
        )
    )SQL";

    if (!conn->execute_command(create_main_table)) {
        logger_->error("Failed to create regulatory_changes table");
        return false;
    }

    // Metadata table
    std::string create_metadata_table = R"SQL(
        CREATE TABLE IF NOT EXISTS regulatory_metadata (
            id SERIAL PRIMARY KEY,
            change_id VARCHAR(255) REFERENCES regulatory_changes(change_id) ON DELETE CASCADE,
            regulatory_body VARCHAR(255),
            document_type VARCHAR(255),
            document_number VARCHAR(255),
            version INTEGER DEFAULT 1,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )SQL";

    if (!conn->execute_command(create_metadata_table)) {
        logger_->error("Failed to create regulatory_metadata table");
        return false;
    }

    // Keywords table for many-to-many relationship
    std::string create_keywords_table = R"SQL(
        CREATE TABLE IF NOT EXISTS regulatory_keywords (
            id SERIAL PRIMARY KEY,
            change_id VARCHAR(255) REFERENCES regulatory_changes(change_id) ON DELETE CASCADE,
            keyword TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            CONSTRAINT unique_change_keyword UNIQUE (change_id, keyword)
        )
    )SQL";

    if (!conn->execute_command(create_keywords_table)) {
        logger_->error("Failed to create regulatory_keywords table");
        return false;
    }

    // Affected entities table
    std::string create_entities_table = R"SQL(
        CREATE TABLE IF NOT EXISTS regulatory_affected_entities (
            id SERIAL PRIMARY KEY,
            change_id VARCHAR(255) REFERENCES regulatory_changes(change_id) ON DELETE CASCADE,
            entity_name TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )SQL";

    if (!conn->execute_command(create_entities_table)) {
        logger_->error("Failed to create regulatory_affected_entities table");
        return false;
    }

    // Analysis table
    std::string create_analysis_table = R"SQL(
        CREATE TABLE IF NOT EXISTS regulatory_analysis (
            id SERIAL PRIMARY KEY,
            change_id VARCHAR(255) REFERENCES regulatory_changes(change_id) ON DELETE CASCADE,
            impact_level INTEGER NOT NULL,
            executive_summary TEXT,
            analysis_timestamp BIGINT NOT NULL,
            version INTEGER DEFAULT 1,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            CONSTRAINT unique_change_analysis UNIQUE (change_id)
        )
    )SQL";

    if (!conn->execute_command(create_analysis_table)) {
        logger_->error("Failed to create regulatory_analysis table");
        return false;
    }

    // Affected domains table
    std::string create_domains_table = R"SQL(
        CREATE TABLE IF NOT EXISTS regulatory_affected_domains (
            id SERIAL PRIMARY KEY,
            change_id VARCHAR(255) REFERENCES regulatory_changes(change_id) ON DELETE CASCADE,
            domain INTEGER NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )SQL";

    if (!conn->execute_command(create_domains_table)) {
        logger_->error("Failed to create regulatory_affected_domains table");
        return false;
    }

    // Required actions table
    std::string create_actions_table = R"SQL(
        CREATE TABLE IF NOT EXISTS regulatory_required_actions (
            id SERIAL PRIMARY KEY,
            change_id VARCHAR(255) REFERENCES regulatory_changes(change_id) ON DELETE CASCADE,
            action TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )SQL";

    if (!conn->execute_command(create_actions_table)) {
        logger_->error("Failed to create regulatory_required_actions table");
        return false;
    }

    // Compliance deadlines table
    std::string create_deadlines_table = R"SQL(
        CREATE TABLE IF NOT EXISTS regulatory_compliance_deadlines (
            id SERIAL PRIMARY KEY,
            change_id VARCHAR(255) REFERENCES regulatory_changes(change_id) ON DELETE CASCADE,
            deadline TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )SQL";

    if (!conn->execute_command(create_deadlines_table)) {
        logger_->error("Failed to create regulatory_compliance_deadlines table");
        return false;
    }

    // Risk scores table
    std::string create_risk_scores_table = R"SQL(
        CREATE TABLE IF NOT EXISTS regulatory_risk_scores (
            id SERIAL PRIMARY KEY,
            change_id VARCHAR(255) REFERENCES regulatory_changes(change_id) ON DELETE CASCADE,
            domain_name VARCHAR(255) NOT NULL,
            risk_score DOUBLE PRECISION NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )SQL";

    if (!conn->execute_command(create_risk_scores_table)) {
        logger_->error("Failed to create regulatory_risk_scores table");
        return false;
    }

    // Custom fields table for flexible metadata storage
    std::string create_custom_fields_table = R"SQL(
        CREATE TABLE IF NOT EXISTS regulatory_custom_fields (
            id SERIAL PRIMARY KEY,
            change_id VARCHAR(255) REFERENCES regulatory_changes(change_id) ON DELETE CASCADE,
            field_name VARCHAR(255) NOT NULL,
            field_value TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )SQL";

    if (!conn->execute_command(create_custom_fields_table)) {
        logger_->error("Failed to create regulatory_custom_fields table");
        return false;
    }

    // Change history table for versioning and audit trail
    std::string create_history_table = R"SQL(
        CREATE TABLE IF NOT EXISTS regulatory_change_history (
            id SERIAL PRIMARY KEY,
            change_id VARCHAR(255) REFERENCES regulatory_changes(change_id) ON DELETE CASCADE,
            version INTEGER NOT NULL,
            change_type VARCHAR(50) NOT NULL,
            changed_by VARCHAR(255),
            change_data JSONB,
            changed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )SQL";

    if (!conn->execute_command(create_history_table)) {
        logger_->error("Failed to create regulatory_change_history table");
        return false;
    }

    // Create indexes for performance optimization
    std::vector<std::string> indexes = {
        "CREATE INDEX IF NOT EXISTS idx_reg_changes_status ON regulatory_changes(status)",
        "CREATE INDEX IF NOT EXISTS idx_reg_changes_detected_at ON regulatory_changes(detected_at DESC)",
        "CREATE INDEX IF NOT EXISTS idx_reg_changes_source_id ON regulatory_changes(source_id)",
        "CREATE INDEX IF NOT EXISTS idx_reg_metadata_body ON regulatory_metadata(regulatory_body)",
        "CREATE INDEX IF NOT EXISTS idx_reg_metadata_change_id ON regulatory_metadata(change_id)",
        "CREATE INDEX IF NOT EXISTS idx_reg_keywords_change_id ON regulatory_keywords(change_id)",
        "CREATE INDEX IF NOT EXISTS idx_reg_keywords_keyword ON regulatory_keywords(keyword)",
        "CREATE INDEX IF NOT EXISTS idx_reg_analysis_impact ON regulatory_analysis(impact_level)",
        "CREATE INDEX IF NOT EXISTS idx_reg_analysis_change_id ON regulatory_analysis(change_id)",
        "CREATE INDEX IF NOT EXISTS idx_reg_domains_domain ON regulatory_affected_domains(domain)",
        "CREATE INDEX IF NOT EXISTS idx_reg_domains_change_id ON regulatory_affected_domains(change_id)",
        "CREATE INDEX IF NOT EXISTS idx_reg_history_change_id ON regulatory_change_history(change_id, version DESC)"
    };

    for (const auto& index_sql : indexes) {
        if (!conn->execute_command(index_sql)) {
            logger_->warn("Failed to create index", "RegulatoryKnowledgeBase", "create_database_schema", {{"sql", index_sql}});
        }
    }

    // Create full-text search configuration using PostgreSQL's pg_trgm extension
    std::string create_fts_index = R"SQL(
        CREATE EXTENSION IF NOT EXISTS pg_trgm;
        CREATE INDEX IF NOT EXISTS idx_reg_changes_title_trgm ON regulatory_changes USING GIN (title gin_trgm_ops);
        CREATE INDEX IF NOT EXISTS idx_reg_keywords_keyword_trgm ON regulatory_keywords USING GIN (keyword gin_trgm_ops);
        CREATE INDEX IF NOT EXISTS idx_reg_analysis_summary_trgm ON regulatory_analysis USING GIN (executive_summary gin_trgm_ops);
    )SQL";

    conn->execute_command(create_fts_index); // Soft fail if extension not available

    logger_->info("Database schema created successfully");
    return true;
}

void RegulatoryKnowledgeBase::shutdown() {
    logger_->info("Shutting down RegulatoryKnowledgeBase");

    if (pimpl_) {
        pimpl_->clear_cache();
        if (pimpl_->connection_pool) {
            pimpl_->connection_pool->shutdown();
        }
    }

    logger_->info("RegulatoryKnowledgeBase shutdown complete");
}

bool RegulatoryKnowledgeBase::store_regulatory_change(const RegulatoryChange& change) {
    if (!pimpl_) {
        logger_->error("Knowledge base not initialized");
        return false;
    }

    logger_->debug("Storing regulatory change", "RegulatoryKnowledgeBase", "store_regulatory_change", {{"change_id", change.get_change_id()}});

    return pimpl_->db_executor->execute_with_retry(
        "store_regulatory_change",
        [this, &change](std::shared_ptr<PostgreSQLConnection> conn) {
            // Begin transaction for ACID guarantees
            if (!conn->begin_transaction()) {
                logger_->error("Failed to begin transaction");
                return false;
            }

            try {
                // Insert/Update main record
                std::string insert_main = R"SQL(
                    INSERT INTO regulatory_changes
                    (change_id, source_id, title, content_url, status, detected_at, analyzed_at, distributed_at)
                    VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
                    ON CONFLICT (change_id)
                    DO UPDATE SET
                        title = EXCLUDED.title,
                        content_url = EXCLUDED.content_url,
                        status = EXCLUDED.status,
                        analyzed_at = EXCLUDED.analyzed_at,
                        distributed_at = EXCLUDED.distributed_at,
                        version = regulatory_changes.version + 1,
                        updated_at = CURRENT_TIMESTAMP
                )SQL";

                auto detected_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    change.get_detected_at().time_since_epoch()).count();

                auto analyzed_at = change.get_analyzed_at();
                auto analyzed_ms = analyzed_at.time_since_epoch().count() > 0 ?
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        analyzed_at.time_since_epoch()).count() : 0;

                auto distributed_at = change.get_distributed_at();
                auto distributed_ms = distributed_at.time_since_epoch().count() > 0 ?
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        distributed_at.time_since_epoch()).count() : 0;

                std::vector<std::string> main_params = {
                    change.get_change_id(),
                    change.get_source_id(),
                    change.get_title(),
                    change.get_content_url(),
                    std::to_string(static_cast<int>(change.get_status())),
                    std::to_string(detected_ms),
                    analyzed_ms > 0 ? std::to_string(analyzed_ms) : "",
                    distributed_ms > 0 ? std::to_string(distributed_ms) : ""
                };

                if (!conn->execute_command(insert_main, main_params)) {
                    conn->rollback_transaction();
                    return false;
                }

                // Delete old related records (will be recreated for proper versioning)
                std::vector<std::string> change_id_param = {change.get_change_id()};

                conn->execute_command("DELETE FROM regulatory_metadata WHERE change_id = $1", change_id_param);
                conn->execute_command("DELETE FROM regulatory_keywords WHERE change_id = $1", change_id_param);
                conn->execute_command("DELETE FROM regulatory_affected_entities WHERE change_id = $1", change_id_param);
                conn->execute_command("DELETE FROM regulatory_analysis WHERE change_id = $1", change_id_param);
                conn->execute_command("DELETE FROM regulatory_affected_domains WHERE change_id = $1", change_id_param);
                conn->execute_command("DELETE FROM regulatory_required_actions WHERE change_id = $1", change_id_param);
                conn->execute_command("DELETE FROM regulatory_compliance_deadlines WHERE change_id = $1", change_id_param);
                conn->execute_command("DELETE FROM regulatory_risk_scores WHERE change_id = $1", change_id_param);
                conn->execute_command("DELETE FROM regulatory_custom_fields WHERE change_id = $1", change_id_param);

                // Insert metadata
                const auto& metadata = change.get_metadata();
                std::string insert_metadata = R"SQL(
                    INSERT INTO regulatory_metadata
                    (change_id, regulatory_body, document_type, document_number)
                    VALUES ($1, $2, $3, $4)
                )SQL";

                std::vector<std::string> metadata_params = {
                    change.get_change_id(),
                    metadata.regulatory_body,
                    metadata.document_type,
                    metadata.document_number
                };

                conn->execute_command(insert_metadata, metadata_params);

                // Insert keywords
                for (const auto& keyword : metadata.keywords) {
                    std::string insert_keyword =
                        "INSERT INTO regulatory_keywords (change_id, keyword) VALUES ($1, $2)";
                    conn->execute_command(insert_keyword, {change.get_change_id(), keyword});
                }

                // Insert affected entities
                for (const auto& entity : metadata.affected_entities) {
                    std::string insert_entity =
                        "INSERT INTO regulatory_affected_entities (change_id, entity_name) VALUES ($1, $2)";
                    conn->execute_command(insert_entity, {change.get_change_id(), entity});
                }

                // Insert custom fields
                for (const auto& [field_name, field_value] : metadata.custom_fields) {
                    std::string insert_custom =
                        "INSERT INTO regulatory_custom_fields (change_id, field_name, field_value) VALUES ($1, $2, $3)";
                    conn->execute_command(insert_custom, {change.get_change_id(), field_name, field_value});
                }

                // Insert analysis if available
                if (change.get_analysis().has_value()) {
                    const auto& analysis = change.get_analysis().value();

                    auto analysis_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        analysis.analysis_timestamp.time_since_epoch()).count();

                    std::string insert_analysis = R"SQL(
                        INSERT INTO regulatory_analysis
                        (change_id, impact_level, executive_summary, analysis_timestamp)
                        VALUES ($1, $2, $3, $4)
                    )SQL";

                    std::vector<std::string> analysis_params = {
                        change.get_change_id(),
                        std::to_string(static_cast<int>(analysis.impact_level)),
                        analysis.executive_summary,
                        std::to_string(analysis_ms)
                    };

                    conn->execute_command(insert_analysis, analysis_params);

                    // Insert affected domains
                    for (auto domain : analysis.affected_domains) {
                        std::string insert_domain =
                            "INSERT INTO regulatory_affected_domains (change_id, domain) VALUES ($1, $2)";
                        conn->execute_command(insert_domain,
                            {change.get_change_id(), std::to_string(static_cast<int>(domain))});
                    }

                    // Insert required actions
                    for (const auto& action : analysis.required_actions) {
                        std::string insert_action =
                            "INSERT INTO regulatory_required_actions (change_id, action) VALUES ($1, $2)";
                        conn->execute_command(insert_action, {change.get_change_id(), action});
                    }

                    // Insert compliance deadlines
                    for (const auto& deadline : analysis.compliance_deadlines) {
                        std::string insert_deadline =
                            "INSERT INTO regulatory_compliance_deadlines (change_id, deadline) VALUES ($1, $2)";
                        conn->execute_command(insert_deadline, {change.get_change_id(), deadline});
                    }

                    // Insert risk scores
                    for (const auto& [domain_name, score] : analysis.risk_scores) {
                        std::string insert_risk =
                            "INSERT INTO regulatory_risk_scores (change_id, domain_name, risk_score) VALUES ($1, $2, $3)";
                        conn->execute_command(insert_risk,
                            {change.get_change_id(), domain_name, std::to_string(score)});
                    }
                }

                // Record change in history for versioning and audit trail
                std::string insert_history = R"SQL(
                    INSERT INTO regulatory_change_history
                    (change_id, version, change_type, change_data)
                    VALUES ($1, 1, $2, $3::jsonb)
                )SQL";

                conn->execute_command(insert_history, {
                    change.get_change_id(),
                    "CREATE",
                    change.to_json().dump()
                });

                // Commit transaction
                if (!conn->commit_transaction()) {
                    logger_->error("Failed to commit transaction");
                    return false;
                }

                // Update cache
                pimpl_->update_cache(change.get_change_id(), change);

                // Update in-memory indexes
                index_change(change);

                // Update statistics
                update_statistics_for_change(change, true);

                logger_->info("Regulatory change stored successfully",
                            "RegulatoryKnowledgeBase", "store_regulatory_change",
                            {{"change_id", change.get_change_id()}});
                return true;

            } catch (const std::exception& e) {
                logger_->error("Exception storing regulatory change",
                             "RegulatoryKnowledgeBase", "store_regulatory_change",
                             {{"change_id", change.get_change_id()},
                              {"error", e.what()}});
                conn->rollback_transaction();
                return false;
            }
        });
}

std::optional<RegulatoryChange> RegulatoryKnowledgeBase::get_regulatory_change(
    const std::string& change_id) const {

    if (!pimpl_) {
        logger_->error("Knowledge base not initialized");
        return std::nullopt;
    }

    pimpl_->total_queries++;

    // Check cache first for performance
    {
        std::lock_guard<std::mutex> lock(pimpl_->cache_mutex);
        auto cache_it = pimpl_->change_cache.find(change_id);
        if (cache_it != pimpl_->change_cache.end() &&
            pimpl_->is_cache_valid(cache_it->second)) {
            pimpl_->cache_hits++;
            logger_->debug("Cache hit for regulatory change", "RegulatoryKnowledgeBase", "get_regulatory_change", {{"change_id", change_id}});
            return cache_it->second.change;
        }
    }

    pimpl_->cache_misses++;

    return pimpl_->db_executor->execute_query_with_retry<RegulatoryChange>(
        "get_regulatory_change",
        [this, &change_id](std::shared_ptr<PostgreSQLConnection> conn)
            -> std::optional<RegulatoryChange> {

            // Query main record
            std::string query_main = R"SQL(
                SELECT change_id, source_id, title, content_url, status,
                       detected_at, analyzed_at, distributed_at
                FROM regulatory_changes
                WHERE change_id = $1
            )SQL";

            auto result = conn->execute_query(query_main, {change_id});
            if (result.rows.empty()) {
                return std::nullopt;
            }

            auto& row = result.rows[0];

            // Reconstruct RegulatoryChange from database records
            auto change_obj = reconstruct_change_from_db(conn, row);

            if (change_obj) {
                // Update cache
                pimpl_->update_cache(change_id, *change_obj);
            }

            return change_obj;
        });
}

std::optional<RegulatoryChange> RegulatoryKnowledgeBase::reconstruct_change_from_db(
    std::shared_ptr<PostgreSQLConnection> conn,
    const std::unordered_map<std::string, std::string>& main_row) const {

    try {
        std::string change_id = main_row.at("change_id");

        // Get metadata
        std::string query_metadata =
            "SELECT regulatory_body, document_type, document_number FROM regulatory_metadata WHERE change_id = $1";
        auto metadata_result = conn->execute_query(query_metadata, {change_id});

        RegulatoryChangeMetadata metadata;
        if (!metadata_result.rows.empty()) {
            auto& meta_row = metadata_result.rows[0];
            metadata.regulatory_body = meta_row["regulatory_body"];
            metadata.document_type = meta_row["document_type"];
            metadata.document_number = meta_row["document_number"];
        }

        // Get keywords
        std::string query_keywords = "SELECT keyword FROM regulatory_keywords WHERE change_id = $1";
        auto keywords_result = conn->execute_query(query_keywords, {change_id});
        for (const auto& kw_row : keywords_result.rows) {
            metadata.keywords.push_back(kw_row.at("keyword"));
        }

        // Get affected entities
        std::string query_entities =
            "SELECT entity_name FROM regulatory_affected_entities WHERE change_id = $1";
        auto entities_result = conn->execute_query(query_entities, {change_id});
        for (const auto& ent_row : entities_result.rows) {
            metadata.affected_entities.push_back(ent_row.at("entity_name"));
        }

        // Get custom fields
        std::string query_custom =
            "SELECT field_name, field_value FROM regulatory_custom_fields WHERE change_id = $1";
        auto custom_result = conn->execute_query(query_custom, {change_id});
        for (const auto& custom_row : custom_result.rows) {
            metadata.custom_fields[custom_row.at("field_name")] = custom_row.at("field_value");
        }

        // Create RegulatoryChange object
        RegulatoryChange change(
            main_row.at("source_id"),
            main_row.at("title"),
            main_row.at("content_url"),
            metadata
        );

        // Set status and timestamps
        change.set_status(static_cast<RegulatoryChangeStatus>(std::stoi(main_row.at("status"))));

        // Get analysis if available
        std::string query_analysis = R"SQL(
            SELECT impact_level, executive_summary, analysis_timestamp
            FROM regulatory_analysis
            WHERE change_id = $1
        )SQL";

        auto analysis_result = conn->execute_query(query_analysis, {change_id});
        if (!analysis_result.rows.empty()) {
            auto& analysis_row = analysis_result.rows[0];

            RegulatoryChangeAnalysis analysis;
            analysis.impact_level = static_cast<RegulatoryImpact>(
                std::stoi(analysis_row.at("impact_level")));
            analysis.executive_summary = analysis_row.at("executive_summary");
            analysis.analysis_timestamp = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(std::stoll(analysis_row.at("analysis_timestamp"))));

            // Get affected domains
            std::string query_domains =
                "SELECT domain FROM regulatory_affected_domains WHERE change_id = $1";
            auto domains_result = conn->execute_query(query_domains, {change_id});
            for (const auto& dom_row : domains_result.rows) {
                analysis.affected_domains.push_back(
                    static_cast<BusinessDomain>(std::stoi(dom_row.at("domain"))));
            }

            // Get required actions
            std::string query_actions =
                "SELECT action FROM regulatory_required_actions WHERE change_id = $1";
            auto actions_result = conn->execute_query(query_actions, {change_id});
            for (const auto& action_row : actions_result.rows) {
                analysis.required_actions.push_back(action_row.at("action"));
            }

            // Get compliance deadlines
            std::string query_deadlines =
                "SELECT deadline FROM regulatory_compliance_deadlines WHERE change_id = $1";
            auto deadlines_result = conn->execute_query(query_deadlines, {change_id});
            for (const auto& dl_row : deadlines_result.rows) {
                analysis.compliance_deadlines.push_back(dl_row.at("deadline"));
            }

            // Get risk scores
            std::string query_risks =
                "SELECT domain_name, risk_score FROM regulatory_risk_scores WHERE change_id = $1";
            auto risks_result = conn->execute_query(query_risks, {change_id});
            for (const auto& risk_row : risks_result.rows) {
                analysis.risk_scores[risk_row.at("domain_name")] =
                    std::stod(risk_row.at("risk_score"));
            }

            change.set_analysis(analysis);
        }

        return change;

    } catch (const std::exception& e) {
        logger_->error("Failed to reconstruct change from database",
                     "RegulatoryKnowledgeBase", "reconstruct_change_from_db",
                     {{"error", e.what()}});
        return std::nullopt;
    }
}

std::vector<RegulatoryChange> RegulatoryKnowledgeBase::search_changes(
    const std::string& query,
    const std::unordered_map<std::string, std::string>& filters,
    size_t limit) const {

    if (!pimpl_) {
        logger_->error("Knowledge base not initialized");
        return {};
    }

    logger_->debug("Searching regulatory changes", "RegulatoryKnowledgeBase", "search_changes", {{"query", query}, {"limit", std::to_string(limit)}});

    auto result = pimpl_->db_executor->execute_query_with_retry<std::vector<RegulatoryChange>>(
        "search_changes",
        [this, &query, &filters, limit](std::shared_ptr<PostgreSQLConnection> conn)
            -> std::optional<std::vector<RegulatoryChange>> {

            std::vector<RegulatoryChange> results;

            // Build search query with full-text search
            std::stringstream sql;
            sql << R"SQL(
                SELECT DISTINCT rc.change_id, rc.source_id, rc.title, rc.content_url,
                       rc.status, rc.detected_at, rc.analyzed_at, rc.distributed_at
                FROM regulatory_changes rc
                LEFT JOIN regulatory_keywords rk ON rc.change_id = rk.change_id
                LEFT JOIN regulatory_analysis ra ON rc.change_id = ra.change_id
                LEFT JOIN regulatory_metadata rm ON rc.change_id = rm.change_id
                WHERE 1=1
            )SQL";

            std::vector<std::string> params;
            int param_counter = 1;

            // Add search query if provided using full-text search
            if (!query.empty()) {
                sql << " AND (";
                sql << "rc.title ILIKE $" << param_counter++ << " OR ";
                sql << "rk.keyword ILIKE $" << param_counter++ << " OR ";
                sql << "ra.executive_summary ILIKE $" << param_counter++;
                sql << ")";

                std::string search_pattern = "%" + query + "%";
                params.push_back(search_pattern);
                params.push_back(search_pattern);
                params.push_back(search_pattern);
            }

            // Apply filters
            if (filters.count("regulatory_body")) {
                sql << " AND rm.regulatory_body = $" << param_counter++;
                params.push_back(filters.at("regulatory_body"));
            }

            if (filters.count("impact_level")) {
                sql << " AND ra.impact_level = $" << param_counter++;
                params.push_back(filters.at("impact_level"));
            }

            if (filters.count("status")) {
                sql << " AND rc.status = $" << param_counter++;
                params.push_back(filters.at("status"));
            }

            if (filters.count("domain")) {
                sql << " AND EXISTS (SELECT 1 FROM regulatory_affected_domains rad ";
                sql << "WHERE rad.change_id = rc.change_id AND rad.domain = $" << param_counter++ << ")";
                params.push_back(filters.at("domain"));
            }

            // Order by relevance and recency
            sql << " ORDER BY rc.detected_at DESC";
            sql << " LIMIT " << limit;

            auto query_result = conn->execute_query(sql.str(), params);

            for (const auto& row : query_result.rows) {
                auto change = reconstruct_change_from_db(conn, row);
                if (change) {
                    results.push_back(std::move(*change));
                }
            }

            return results;
        });

    return result.value_or(std::vector<RegulatoryChange>{});
}

std::vector<RegulatoryChange> RegulatoryKnowledgeBase::get_changes_by_impact(
    RegulatoryImpact impact_level, size_t limit) const {

    if (!pimpl_) {
        return {};
    }

    return pimpl_->db_executor->execute_query_with_retry<std::vector<RegulatoryChange>>(
        "get_changes_by_impact",
        [this, impact_level, limit](std::shared_ptr<PostgreSQLConnection> conn)
            -> std::optional<std::vector<RegulatoryChange>> {

            std::vector<RegulatoryChange> results;

            std::string query = R"SQL(
                SELECT rc.change_id, rc.source_id, rc.title, rc.content_url,
                       rc.status, rc.detected_at, rc.analyzed_at, rc.distributed_at
                FROM regulatory_changes rc
                JOIN regulatory_analysis ra ON rc.change_id = ra.change_id
                WHERE ra.impact_level = $1
                ORDER BY rc.detected_at DESC
                LIMIT $2
            )SQL";

            auto result = conn->execute_query(query, {
                std::to_string(static_cast<int>(impact_level)),
                std::to_string(limit)
            });

            for (const auto& row : result.rows) {
                auto change = reconstruct_change_from_db(conn, row);
                if (change) {
                    results.push_back(std::move(*change));
                }
            }

            return results;
        }).value_or(std::vector<RegulatoryChange>{});
}

std::vector<RegulatoryChange> RegulatoryKnowledgeBase::get_changes_by_domain(
    BusinessDomain domain, size_t limit) const {

    if (!pimpl_) {
        return {};
    }

    return pimpl_->db_executor->execute_query_with_retry<std::vector<RegulatoryChange>>(
        "get_changes_by_domain",
        [this, domain, limit](std::shared_ptr<PostgreSQLConnection> conn)
            -> std::optional<std::vector<RegulatoryChange>> {

            std::vector<RegulatoryChange> results;

            std::string query = R"SQL(
                SELECT DISTINCT rc.change_id, rc.source_id, rc.title, rc.content_url,
                       rc.status, rc.detected_at, rc.analyzed_at, rc.distributed_at
                FROM regulatory_changes rc
                JOIN regulatory_affected_domains rad ON rc.change_id = rad.change_id
                WHERE rad.domain = $1
                ORDER BY rc.detected_at DESC
                LIMIT $2
            )SQL";

            auto result = conn->execute_query(query, {
                std::to_string(static_cast<int>(domain)),
                std::to_string(limit)
            });

            for (const auto& row : result.rows) {
                auto change = reconstruct_change_from_db(conn, row);
                if (change) {
                    results.push_back(std::move(*change));
                }
            }

            return results;
        }).value_or(std::vector<RegulatoryChange>{});
}

std::vector<RegulatoryChange> RegulatoryKnowledgeBase::get_changes_by_body(
    const std::string& regulatory_body, size_t limit) const {

    if (!pimpl_) {
        return {};
    }

    return pimpl_->db_executor->execute_query_with_retry<std::vector<RegulatoryChange>>(
        "get_changes_by_body",
        [this, &regulatory_body, limit](std::shared_ptr<PostgreSQLConnection> conn)
            -> std::optional<std::vector<RegulatoryChange>> {

            std::vector<RegulatoryChange> results;

            std::string query = R"SQL(
                SELECT rc.change_id, rc.source_id, rc.title, rc.content_url,
                       rc.status, rc.detected_at, rc.analyzed_at, rc.distributed_at
                FROM regulatory_changes rc
                JOIN regulatory_metadata rm ON rc.change_id = rm.change_id
                WHERE rm.regulatory_body = $1
                ORDER BY rc.detected_at DESC
                LIMIT $2
            )SQL";

            auto result = conn->execute_query(query, {
                regulatory_body,
                std::to_string(limit)
            });

            for (const auto& row : result.rows) {
                auto change = reconstruct_change_from_db(conn, row);
                if (change) {
                    results.push_back(std::move(*change));
                }
            }

            return results;
        }).value_or(std::vector<RegulatoryChange>{});
}

std::vector<RegulatoryChange> RegulatoryKnowledgeBase::get_recent_changes(
    int days, size_t limit) const {

    if (!pimpl_) {
        return {};
    }

    return pimpl_->db_executor->execute_query_with_retry<std::vector<RegulatoryChange>>(
        "get_recent_changes",
        [this, days, limit](std::shared_ptr<PostgreSQLConnection> conn)
            -> std::optional<std::vector<RegulatoryChange>> {

            std::vector<RegulatoryChange> results;

            auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24 * days);
            auto cutoff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                cutoff_time.time_since_epoch()).count();

            std::string query = R"SQL(
                SELECT change_id, source_id, title, content_url,
                       status, detected_at, analyzed_at, distributed_at
                FROM regulatory_changes
                WHERE detected_at >= $1
                ORDER BY detected_at DESC
                LIMIT $2
            )SQL";

            auto result = conn->execute_query(query, {
                std::to_string(cutoff_ms),
                std::to_string(limit)
            });

            for (const auto& row : result.rows) {
                auto change = reconstruct_change_from_db(conn, row);
                if (change) {
                    results.push_back(std::move(*change));
                }
            }

            return results;
        }).value_or(std::vector<RegulatoryChange>{});
}

bool RegulatoryKnowledgeBase::update_change_status(
    const std::string& change_id, RegulatoryChangeStatus new_status) {

    if (!pimpl_) {
        return false;
    }

    return pimpl_->db_executor->execute_with_retry(
        "update_change_status",
        [this, &change_id, new_status](std::shared_ptr<PostgreSQLConnection> conn) {

            if (!conn->begin_transaction()) {
                return false;
            }

            try {
                std::string update_query = R"SQL(
                    UPDATE regulatory_changes
                    SET status = $1,
                        version = version + 1,
                        updated_at = CURRENT_TIMESTAMP
                    WHERE change_id = $2
                )SQL";

                if (!conn->execute_command(update_query, {
                    std::to_string(static_cast<int>(new_status)),
                    change_id
                })) {
                    conn->rollback_transaction();
                    return false;
                }

                // Record in history for audit trail
                std::string history_query = R"SQL(
                    INSERT INTO regulatory_change_history
                    (change_id, version, change_type, change_data)
                    SELECT change_id, version, 'STATUS_UPDATE',
                           json_build_object('new_status', $1)::jsonb
                    FROM regulatory_changes
                    WHERE change_id = $2
                )SQL";

                conn->execute_command(history_query, {
                    std::to_string(static_cast<int>(new_status)),
                    change_id
                });

                if (!conn->commit_transaction()) {
                    return false;
                }

                // Invalidate cache
                pimpl_->invalidate_cache(change_id);

                logger_->info("Updated regulatory change status",
                            "RegulatoryKnowledgeBase", "update_change_status",
                            {{"change_id", change_id},
                             {"new_status", std::to_string(static_cast<int>(new_status))}});
                return true;

            } catch (const std::exception& e) {
                logger_->error("Failed to update change status",
                             "RegulatoryKnowledgeBase", "update_change_status",
                             {{"change_id", change_id}, {"error", e.what()}});
                conn->rollback_transaction();
                return false;
            }
        });
}

nlohmann::json RegulatoryKnowledgeBase::get_statistics() const {
    if (!pimpl_) {
        return nlohmann::json::object();
    }

    auto result = pimpl_->db_executor->execute_query_with_retry<nlohmann::json>(
        "get_statistics",
        [this](std::shared_ptr<PostgreSQLConnection> conn) -> std::optional<nlohmann::json> {
            nlohmann::json stats;

            // Total changes
            std::string count_query = "SELECT COUNT(*) as total FROM regulatory_changes";
            auto count_result = conn->execute_query(count_query);
            if (!count_result.rows.empty()) {
                stats["total_changes"] = std::stoi(count_result.rows[0].at("total"));
            }

            // Changes by impact
            std::string impact_query = R"SQL(
                SELECT ra.impact_level, COUNT(*) as count
                FROM regulatory_analysis ra
                GROUP BY ra.impact_level
            )SQL";
            auto impact_result = conn->execute_query(impact_query);
            nlohmann::json impact_stats;
            for (const auto& row : impact_result.rows) {
                int impact = std::stoi(row.at("impact_level"));
                impact_stats[regulatory_impact_to_string(static_cast<RegulatoryImpact>(impact))] =
                    std::stoi(row.at("count"));
            }
            stats["by_impact"] = impact_stats;

            // Changes by status
            std::string status_query = R"SQL(
                SELECT status, COUNT(*) as count
                FROM regulatory_changes
                GROUP BY status
            )SQL";
            auto status_result = conn->execute_query(status_query);
            nlohmann::json status_stats;
            for (const auto& row : status_result.rows) {
                status_stats["status_" + row.at("status")] = std::stoi(row.at("count"));
            }
            stats["by_status"] = status_stats;

            // Changes by regulatory body
            std::string body_query = R"SQL(
                SELECT rm.regulatory_body, COUNT(*) as count
                FROM regulatory_metadata rm
                GROUP BY rm.regulatory_body
                ORDER BY count DESC
                LIMIT 10
            )SQL";
            auto body_result = conn->execute_query(body_query);
            nlohmann::json body_stats;
            for (const auto& row : body_result.rows) {
                body_stats[row.at("regulatory_body")] = std::stoi(row.at("count"));
            }
            stats["by_regulatory_body"] = body_stats;

            // Recent activity
            auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24 * 7);
            auto cutoff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                cutoff_time.time_since_epoch()).count();

            std::string recent_query =
                "SELECT COUNT(*) as count FROM regulatory_changes WHERE detected_at >= $1";
            auto recent_result = conn->execute_query(recent_query, {std::to_string(cutoff_ms)});
            if (!recent_result.rows.empty()) {
                stats["changes_last_7_days"] = std::stoi(recent_result.rows[0].at("count"));
            }

            // Cache statistics
            stats["cache_hits"] = pimpl_->cache_hits.load();
            stats["cache_misses"] = pimpl_->cache_misses.load();
            stats["total_queries"] = pimpl_->total_queries.load();

            double hit_rate = pimpl_->total_queries.load() > 0 ?
                static_cast<double>(pimpl_->cache_hits.load()) / pimpl_->total_queries.load() : 0.0;
            stats["cache_hit_rate"] = hit_rate;

            // Pool statistics
            if (pimpl_->connection_pool) {
                stats["connection_pool"] = pimpl_->connection_pool->get_pool_stats();
            }

            return stats;
        });

    return result.value_or(nlohmann::json::object());
}

nlohmann::json RegulatoryKnowledgeBase::export_to_json() const {
    if (!pimpl_) {
        return nlohmann::json::array();
    }

    auto result = pimpl_->db_executor->execute_query_with_retry<nlohmann::json>(
        "export_to_json",
        [this](std::shared_ptr<PostgreSQLConnection> conn) -> std::optional<nlohmann::json> {
            nlohmann::json export_data = nlohmann::json::array();

            std::string query = R"SQL(
                SELECT change_id, source_id, title, content_url,
                       status, detected_at, analyzed_at, distributed_at
                FROM regulatory_changes
                ORDER BY detected_at DESC
            )SQL";

            auto result = conn->execute_query(query);

            for (const auto& row : result.rows) {
                auto change = reconstruct_change_from_db(conn, row);
                if (change) {
                    export_data.push_back(change->to_json());
                }
            }

            return export_data;
        });

    return result.value_or(nlohmann::json::array());
}

bool RegulatoryKnowledgeBase::import_from_json(const nlohmann::json& json) {
    if (!pimpl_) {
        return false;
    }

    if (!json.is_array()) {
        logger_->error("Import data must be an array");
        return false;
    }

    int success_count = 0;
    int failure_count = 0;

    for (const auto& item : json) {
        auto change = RegulatoryChange::from_json(item);
        if (change && store_regulatory_change(*change)) {
            success_count++;
        } else {
            failure_count++;
        }
    }

    logger_->info("Import completed",
                 "RegulatoryKnowledgeBase", "import_from_json",
                 {{"success", std::to_string(success_count)},
                  {"failures", std::to_string(failure_count)}});

    return failure_count == 0;
}

void RegulatoryKnowledgeBase::clear() {
    if (!pimpl_) {
        return;
    }

    pimpl_->db_executor->execute_with_retry(
        "clear_all_data",
        [this](std::shared_ptr<PostgreSQLConnection> conn) {
            if (!conn->begin_transaction()) {
                return false;
            }

            try {
                // Delete in correct order to respect foreign key constraints
                conn->execute_command("DELETE FROM regulatory_change_history");
                conn->execute_command("DELETE FROM regulatory_custom_fields");
                conn->execute_command("DELETE FROM regulatory_risk_scores");
                conn->execute_command("DELETE FROM regulatory_compliance_deadlines");
                conn->execute_command("DELETE FROM regulatory_required_actions");
                conn->execute_command("DELETE FROM regulatory_affected_domains");
                conn->execute_command("DELETE FROM regulatory_analysis");
                conn->execute_command("DELETE FROM regulatory_affected_entities");
                conn->execute_command("DELETE FROM regulatory_keywords");
                conn->execute_command("DELETE FROM regulatory_metadata");
                conn->execute_command("DELETE FROM regulatory_changes");

                if (!conn->commit_transaction()) {
                    return false;
                }

                // Clear cache
                pimpl_->clear_cache();

                // Clear in-memory indexes
                {
                    std::lock_guard<std::mutex> lock(storage_mutex_);
                    changes_store_.clear();
                }

                {
                    std::lock_guard<std::mutex> lock(index_mutex_);
                    word_index_.clear();
                    impact_index_.clear();
                    domain_index_.clear();
                    body_index_.clear();
                }

                // Reset statistics
                total_changes_ = 0;
                high_impact_changes_ = 0;
                critical_changes_ = 0;

                logger_->info("All regulatory changes cleared");
                return true;

            } catch (const std::exception& e) {
                logger_->error("Failed to clear data", "RegulatoryKnowledgeBase", "clear", {{"error", e.what()}});
                conn->rollback_transaction();
                return false;
            }
        });
}

// Private helper methods for in-memory indexing (supplementary to database indexes)

void RegulatoryKnowledgeBase::index_change(const RegulatoryChange& change) {
    std::lock_guard<std::mutex> index_lock(index_mutex_);
    std::lock_guard<std::mutex> storage_lock(storage_mutex_);

    const auto& change_id = change.get_change_id();

    // Store in memory for quick access
    changes_store_.insert_or_assign(change_id, change);

    // Index by title words
    std::istringstream title_stream(change.get_title());
    std::string word;
    while (title_stream >> word) {
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        word_index_[word].insert(change_id);
    }

    // Index keywords
    for (const auto& keyword : change.get_metadata().keywords) {
        std::string lower_keyword = keyword;
        std::transform(lower_keyword.begin(), lower_keyword.end(),
                      lower_keyword.begin(), ::tolower);
        word_index_[lower_keyword].insert(change_id);
    }

    // Index by impact level
    if (change.get_analysis()) {
        impact_index_[change.get_analysis()->impact_level].insert(change_id);
    }

    // Index by domains
    if (change.get_analysis()) {
        for (auto domain : change.get_analysis()->affected_domains) {
            domain_index_[domain].insert(change_id);
        }
    }

    // Index by regulatory body
    body_index_[change.get_metadata().regulatory_body].insert(change_id);
}

void RegulatoryKnowledgeBase::remove_from_index(const RegulatoryChange& change) {
    std::lock_guard<std::mutex> index_lock(index_mutex_);

    const auto& change_id = change.get_change_id();

    // Remove from word index
    for (auto& [word, change_ids] : word_index_) {
        change_ids.erase(change_id);
    }

    // Remove from impact index
    for (auto& [impact, change_ids] : impact_index_) {
        change_ids.erase(change_id);
    }

    // Remove from domain index
    for (auto& [domain, change_ids] : domain_index_) {
        change_ids.erase(change_id);
    }

    // Remove from body index
    for (auto& [body, change_ids] : body_index_) {
        change_ids.erase(change_id);
    }
}

void RegulatoryKnowledgeBase::create_search_index(
    const std::string& text, const std::string& change_id) {
    std::lock_guard<std::mutex> lock(index_mutex_);

    std::istringstream stream(text);
    std::string word;
    while (stream >> word) {
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        word_index_[word].insert(change_id);
    }
}

std::unordered_set<std::string> RegulatoryKnowledgeBase::search_index(
    const std::string& query) const {
    std::lock_guard<std::mutex> lock(index_mutex_);

    std::unordered_set<std::string> results;
    std::istringstream query_stream(query);
    std::string word;

    bool first_word = true;
    while (query_stream >> word) {
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);

        auto it = word_index_.find(word);
        if (it != word_index_.end()) {
            if (first_word) {
                results = it->second;
                first_word = false;
            } else {
                // Intersection with previous results
                std::unordered_set<std::string> intersection;
                for (const auto& id : results) {
                    if (it->second.count(id)) {
                        intersection.insert(id);
                    }
                }
                results = std::move(intersection);
            }
        } else {
            // No matches for this word
            return {};
        }
    }

    return results;
}

std::unordered_set<std::string> RegulatoryKnowledgeBase::apply_filters(
    const std::unordered_set<std::string>& change_ids,
    const std::unordered_map<std::string, std::string>& filters) const {

    std::lock_guard<std::mutex> lock(index_mutex_);
    std::unordered_set<std::string> filtered = change_ids;

    if (filters.count("impact_level")) {
        RegulatoryImpact impact = static_cast<RegulatoryImpact>(
            std::stoi(filters.at("impact_level")));

        auto it = impact_index_.find(impact);
        if (it != impact_index_.end()) {
            std::unordered_set<std::string> intersection;
            for (const auto& id : filtered) {
                if (it->second.count(id)) {
                    intersection.insert(id);
                }
            }
            filtered = std::move(intersection);
        } else {
            return {};
        }
    }

    if (filters.count("domain")) {
        BusinessDomain domain = static_cast<BusinessDomain>(
            std::stoi(filters.at("domain")));

        auto it = domain_index_.find(domain);
        if (it != domain_index_.end()) {
            std::unordered_set<std::string> intersection;
            for (const auto& id : filtered) {
                if (it->second.count(id)) {
                    intersection.insert(id);
                }
            }
            filtered = std::move(intersection);
        } else {
            return {};
        }
    }

    if (filters.count("regulatory_body")) {
        const auto& body = filters.at("regulatory_body");

        auto it = body_index_.find(body);
        if (it != body_index_.end()) {
            std::unordered_set<std::string> intersection;
            for (const auto& id : filtered) {
                if (it->second.count(id)) {
                    intersection.insert(id);
                }
            }
            filtered = std::move(intersection);
        } else {
            return {};
        }
    }

    return filtered;
}

bool RegulatoryKnowledgeBase::persist_to_storage() {
    // With PostgreSQL backend, data is persisted immediately in transactions
    // This method is kept for interface compatibility
    logger_->debug("Data persisted to PostgreSQL database");
    return true;
}

bool RegulatoryKnowledgeBase::load_from_storage() {
    // With PostgreSQL backend, data is loaded on-demand from database
    // This method is kept for interface compatibility
    logger_->debug("Using PostgreSQL for storage - data loaded on-demand");
    return true;
}

void RegulatoryKnowledgeBase::update_statistics_for_change(
    const RegulatoryChange& change, bool is_new) {

    if (is_new) {
        total_changes_++;
    }

    if (change.get_analysis()) {
        auto impact = change.get_analysis()->impact_level;
        if (impact == RegulatoryImpact::HIGH) {
            high_impact_changes_++;
        } else if (impact == RegulatoryImpact::CRITICAL) {
            critical_changes_++;
        }
    }

    last_update_time_ = std::chrono::system_clock::now();
}

void RegulatoryKnowledgeBase::load_statistics() {
    if (!pimpl_) {
        return;
    }

    pimpl_->db_executor->execute_with_retry(
        "load_statistics",
        [this](std::shared_ptr<PostgreSQLConnection> conn) {
            // Load total count
            auto total_result = conn->execute_query(
                "SELECT COUNT(*) as total FROM regulatory_changes");
            if (!total_result.rows.empty()) {
                total_changes_ = std::stoi(total_result.rows[0].at("total"));
            }

            // Load high impact count
            auto high_result = conn->execute_query(
                "SELECT COUNT(*) as total FROM regulatory_analysis WHERE impact_level = $1",
                {std::to_string(static_cast<int>(RegulatoryImpact::HIGH))});
            if (!high_result.rows.empty()) {
                high_impact_changes_ = std::stoi(high_result.rows[0].at("total"));
            }

            // Load critical impact count
            auto critical_result = conn->execute_query(
                "SELECT COUNT(*) as total FROM regulatory_analysis WHERE impact_level = $1",
                {std::to_string(static_cast<int>(RegulatoryImpact::CRITICAL))});
            if (!critical_result.rows.empty()) {
                critical_changes_ = std::stoi(critical_result.rows[0].at("total"));
            }

            last_update_time_ = std::chrono::system_clock::now();
            return true;
        });
}

} // namespace regulens
