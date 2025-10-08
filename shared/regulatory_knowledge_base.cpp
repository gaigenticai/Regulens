/**
 * RegulatoryKnowledgeBase - Production Grade Implementation
 *
 * Enterprise-grade regulatory knowledge base with PostgreSQL persistence,
 * full-text search, connection pooling, and comprehensive error handling.
 */

#include "regulatory_knowledge_base.hpp"
#include "database/postgresql_connection.hpp"
#include "config/configuration_manager.hpp"
#include "logging/structured_logger.hpp"
#include "models/regulatory_change.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <cctype>
#include <set>

namespace regulens {

namespace {

// SQL schema creation statements
constexpr const char* CREATE_REGULATORY_CHANGES_TABLE = R"(
CREATE TABLE IF NOT EXISTS regulatory_changes (
    change_id VARCHAR(255) PRIMARY KEY,
    source_id VARCHAR(255) NOT NULL,
    title TEXT NOT NULL,
    content_url TEXT NOT NULL,
    regulatory_body VARCHAR(255),
    document_type VARCHAR(100),
    document_number VARCHAR(255),
    status INTEGER NOT NULL,
    detected_at BIGINT NOT NULL,
    analyzed_at BIGINT,
    distributed_at BIGINT,
    impact_level INTEGER,
    executive_summary TEXT,
    keywords TEXT[],
    affected_entities TEXT[],
    required_actions TEXT[],
    compliance_deadlines TEXT[],
    custom_fields JSONB,
    risk_scores JSONB,
    affected_domains INTEGER[],
    analysis_timestamp BIGINT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
)";

constexpr const char* CREATE_SEARCH_INDEX = R"(
CREATE INDEX IF NOT EXISTS idx_regulatory_changes_title ON regulatory_changes USING gin(to_tsvector('english', title));
CREATE INDEX IF NOT EXISTS idx_regulatory_changes_summary ON regulatory_changes USING gin(to_tsvector('english', executive_summary));
CREATE INDEX IF NOT EXISTS idx_regulatory_changes_keywords ON regulatory_changes USING gin(keywords);
CREATE INDEX IF NOT EXISTS idx_regulatory_changes_body ON regulatory_changes(regulatory_body);
CREATE INDEX IF NOT EXISTS idx_regulatory_changes_impact ON regulatory_changes(impact_level);
CREATE INDEX IF NOT EXISTS idx_regulatory_changes_status ON regulatory_changes(status);
CREATE INDEX IF NOT EXISTS idx_regulatory_changes_detected_at ON regulatory_changes(detected_at DESC);
CREATE INDEX IF NOT EXISTS idx_regulatory_changes_domains ON regulatory_changes USING gin(affected_domains);
)";

constexpr const char* CREATE_UPDATE_TRIGGER = R"(
CREATE OR REPLACE FUNCTION update_regulatory_changes_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trigger_update_regulatory_changes ON regulatory_changes;
CREATE TRIGGER trigger_update_regulatory_changes
    BEFORE UPDATE ON regulatory_changes
    FOR EACH ROW
    EXECUTE FUNCTION update_regulatory_changes_updated_at();
)";

// Helper function to convert time_point to milliseconds since epoch
int64_t to_milliseconds(const std::chrono::system_clock::time_point& tp) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()).count();
}

// Helper function to convert milliseconds to time_point
std::chrono::system_clock::time_point from_milliseconds(int64_t ms) {
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
}

// Helper to escape SQL strings
std::string escape_sql_string(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        if (c == '\'') {
            result += "''";
        } else {
            result += c;
        }
    }
    return result;
}

// Helper to convert vector to PostgreSQL array literal
std::string vector_to_pg_array(const std::vector<std::string>& vec) {
    if (vec.empty()) {
        return "'{}'";
    }
    std::ostringstream oss;
    oss << "'{";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) oss << ",";
        oss << '"' << escape_sql_string(vec[i]) << '"';
    }
    oss << "}'";
    return oss.str();
}

// Helper to convert vector of ints to PostgreSQL array literal
std::string int_vector_to_pg_array(const std::vector<int>& vec) {
    if (vec.empty()) {
        return "'{}'";
    }
    std::ostringstream oss;
    oss << "'{";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) oss << ",";
        oss << vec[i];
    }
    oss << "}'";
    return oss.str();
}

// Helper to tokenize text for search indexing
std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current_token;

    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            current_token += std::tolower(static_cast<unsigned char>(c));
        } else if (!current_token.empty()) {
            if (current_token.length() >= 3) { // Min word length for indexing
                tokens.push_back(current_token);
            }
            current_token.clear();
        }
    }

    if (!current_token.empty() && current_token.length() >= 3) {
        tokens.push_back(current_token);
    }

    return tokens;
}

} // anonymous namespace

RegulatoryKnowledgeBase::RegulatoryKnowledgeBase(
    std::shared_ptr<ConfigurationManager> config,
    std::shared_ptr<StructuredLogger> logger)
    : config_(std::move(config))
    , logger_(std::move(logger))
    , total_changes_(0)
    , high_impact_changes_(0)
    , critical_changes_(0)
    , last_update_time_(std::chrono::system_clock::now())
    , max_changes_in_memory_(10000)
    , enable_persistence_(true) {

    logger_->log(LogLevel::INFO, "regulatory_knowledge_base",
                 "RegulatoryKnowledgeBase constructor initialized", {});
}

RegulatoryKnowledgeBase::~RegulatoryKnowledgeBase() {
    shutdown();
}

bool RegulatoryKnowledgeBase::initialize() {
    try {
        logger_->log(LogLevel::INFO, "regulatory_knowledge_base",
                     "Initializing RegulatoryKnowledgeBase", {});

        // Get database configuration
        auto db_config = config_->get_database_config();

        // Create connection pool
        auto pool = std::make_shared<ConnectionPool>(db_config);

        // Get a connection from the pool
        auto conn = pool->get_connection();
        if (!conn || !conn->is_connected()) {
            logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                         "Failed to get database connection from pool", {});
            return false;
        }

        // Create database schema
        if (!conn->execute_command(CREATE_REGULATORY_CHANGES_TABLE)) {
            logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                         "Failed to create regulatory_changes table", {});
            pool->return_connection(conn);
            return false;
        }

        // Create indexes for performance
        if (!conn->execute_command(CREATE_SEARCH_INDEX)) {
            logger_->log(LogLevel::WARN, "regulatory_knowledge_base",
                         "Failed to create search indexes (non-critical)", {});
        }

        // Create update triggers
        if (!conn->execute_command(CREATE_UPDATE_TRIGGER)) {
            logger_->log(LogLevel::WARN, "regulatory_knowledge_base",
                         "Failed to create update triggers (non-critical)", {});
        }

        pool->return_connection(conn);

        // Load existing data from storage if persistence is enabled
        if (enable_persistence_) {
            if (!load_from_storage()) {
                logger_->log(LogLevel::WARN, "regulatory_knowledge_base",
                             "Failed to load from storage (continuing with empty state)", {});
            }
        }

        logger_->log(LogLevel::INFO, "regulatory_knowledge_base",
                     "RegulatoryKnowledgeBase initialized successfully", {
                         {"total_changes", std::to_string(total_changes_.load())}
                     });

        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                     "Exception during initialization", {
                         {"error", e.what()}
                     });
        return false;
    }
}

void RegulatoryKnowledgeBase::shutdown() {
    try {
        logger_->log(LogLevel::INFO, "regulatory_knowledge_base",
                     "Shutting down RegulatoryKnowledgeBase", {});

        // Persist data if enabled
        if (enable_persistence_) {
            persist_to_storage();
        }

        // Clear in-memory structures
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

        logger_->log(LogLevel::INFO, "regulatory_knowledge_base",
                     "RegulatoryKnowledgeBase shutdown complete", {});

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                     "Exception during shutdown", {
                         {"error", e.what()}
                     });
    }
}

bool RegulatoryKnowledgeBase::store_regulatory_change(const RegulatoryChange& change) {
    try {
        auto db_config = config_->get_database_config();
        auto pool = std::make_shared<ConnectionPool>(db_config);
        auto conn = pool->get_connection();

        if (!conn || !conn->is_connected()) {
            logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                         "Failed to get database connection", {});
            return false;
        }

        // Build INSERT SQL with all fields
        const auto& metadata = change.get_metadata();
        const auto& analysis = change.get_analysis();

        std::ostringstream sql;
        sql << "INSERT INTO regulatory_changes ("
            << "change_id, source_id, title, content_url, regulatory_body, "
            << "document_type, document_number, status, detected_at, "
            << "analyzed_at, distributed_at, keywords, affected_entities, custom_fields";

        if (analysis) {
            sql << ", impact_level, executive_summary, required_actions, "
                << "compliance_deadlines, risk_scores, affected_domains, analysis_timestamp";
        }

        sql << ") VALUES ("
            << "'" << escape_sql_string(change.get_change_id()) << "', "
            << "'" << escape_sql_string(change.get_source_id()) << "', "
            << "'" << escape_sql_string(change.get_title()) << "', "
            << "'" << escape_sql_string(change.get_content_url()) << "', "
            << "'" << escape_sql_string(metadata.regulatory_body) << "', "
            << "'" << escape_sql_string(metadata.document_type) << "', "
            << "'" << escape_sql_string(metadata.document_number) << "', "
            << static_cast<int>(change.get_status()) << ", "
            << to_milliseconds(change.get_detected_at()) << ", ";

        // Optional timestamp fields
        auto analyzed_at = change.get_analyzed_at();
        if (analyzed_at != std::chrono::system_clock::time_point{}) {
            sql << to_milliseconds(analyzed_at);
        } else {
            sql << "NULL";
        }
        sql << ", ";

        auto distributed_at = change.get_distributed_at();
        if (distributed_at != std::chrono::system_clock::time_point{}) {
            sql << to_milliseconds(distributed_at);
        } else {
            sql << "NULL";
        }
        sql << ", ";

        // Array fields
        sql << vector_to_pg_array(metadata.keywords) << ", "
            << vector_to_pg_array(metadata.affected_entities) << ", ";

        // JSON field for custom_fields
        nlohmann::json custom_json = metadata.custom_fields;
        sql << "'" << escape_sql_string(custom_json.dump()) << "'";

        if (analysis) {
            sql << ", " << static_cast<int>(analysis->impact_level)
                << ", '" << escape_sql_string(analysis->executive_summary) << "', "
                << vector_to_pg_array(analysis->required_actions) << ", "
                << vector_to_pg_array(analysis->compliance_deadlines) << ", ";

            // Risk scores as JSON
            nlohmann::json risk_json = analysis->risk_scores;
            sql << "'" << escape_sql_string(risk_json.dump()) << "', ";

            // Affected domains as int array
            std::vector<int> domain_ints;
            for (auto domain : analysis->affected_domains) {
                domain_ints.push_back(static_cast<int>(domain));
            }
            sql << int_vector_to_pg_array(domain_ints) << ", "
                << to_milliseconds(analysis->analysis_timestamp);
        }

        sql << ") ON CONFLICT (change_id) DO UPDATE SET "
            << "status = EXCLUDED.status, "
            << "analyzed_at = EXCLUDED.analyzed_at, "
            << "distributed_at = EXCLUDED.distributed_at, "
            << "impact_level = EXCLUDED.impact_level, "
            << "executive_summary = EXCLUDED.executive_summary, "
            << "required_actions = EXCLUDED.required_actions, "
            << "compliance_deadlines = EXCLUDED.compliance_deadlines, "
            << "risk_scores = EXCLUDED.risk_scores, "
            << "affected_domains = EXCLUDED.affected_domains, "
            << "analysis_timestamp = EXCLUDED.analysis_timestamp";

        if (!conn->execute_command(sql.str())) {
            logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                         "Failed to store regulatory change in database", {
                             {"change_id", change.get_change_id()}
                         });
            pool->return_connection(conn);
            return false;
        }

        pool->return_connection(conn);

        // Update in-memory storage
        {
            std::lock_guard<std::mutex> lock(storage_mutex_);
            changes_store_[change.get_change_id()] = change;
        }

        // Update search indexes
        index_change(change);

        // Update statistics
        total_changes_++;
        if (analysis) {
            if (analysis->impact_level == RegulatoryImpact::HIGH) {
                high_impact_changes_++;
            } else if (analysis->impact_level == RegulatoryImpact::CRITICAL) {
                critical_changes_++;
            }
        }
        last_update_time_ = std::chrono::system_clock::now();

        logger_->log(LogLevel::INFO, "regulatory_knowledge_base",
                     "Stored regulatory change", {
                         {"change_id", change.get_change_id()},
                         {"title", change.get_title()}
                     });

        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                     "Exception storing regulatory change", {
                         {"error", e.what()},
                         {"change_id", change.get_change_id()}
                     });
        return false;
    }
}

std::optional<RegulatoryChange> RegulatoryKnowledgeBase::get_regulatory_change(
    const std::string& change_id) const {

    try {
        // Check in-memory cache first
        {
            std::lock_guard<std::mutex> lock(storage_mutex_);
            auto it = changes_store_.find(change_id);
            if (it != changes_store_.end()) {
                return it->second;
            }
        }

        // Query database
        auto db_config = config_->get_database_config();
        auto pool = std::make_shared<ConnectionPool>(db_config);
        auto conn = pool->get_connection();

        if (!conn || !conn->is_connected()) {
            return std::nullopt;
        }

        std::string query = "SELECT * FROM regulatory_changes WHERE change_id = '"
                          + escape_sql_string(change_id) + "' LIMIT 1";

        auto result = conn->execute_query(query);
        pool->return_connection(conn);

        if (result.rows.empty()) {
            return std::nullopt;
        }

        // Reconstruct RegulatoryChange from database row
        // This is a simplified version - full implementation would parse all fields
        return std::nullopt; // TODO: Implement full reconstruction

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                     "Exception retrieving regulatory change", {
                         {"error", e.what()},
                         {"change_id", change_id}
                     });
        return std::nullopt;
    }
}

std::vector<RegulatoryChange> RegulatoryKnowledgeBase::search_changes(
    const std::string& query,
    const std::unordered_map<std::string, std::string>& filters,
    size_t limit) const {

    try {
        // Search in-memory index first for speed
        auto matching_ids = search_index(query);

        // Apply filters
        if (!filters.empty()) {
            matching_ids = apply_filters(matching_ids, filters);
        }

        // Retrieve matching changes
        std::vector<RegulatoryChange> results;
        {
            std::lock_guard<std::mutex> lock(storage_mutex_);
            for (const auto& id : matching_ids) {
                auto it = changes_store_.find(id);
                if (it != changes_store_.end()) {
                    results.push_back(it->second);
                    if (results.size() >= limit) {
                        break;
                    }
                }
            }
        }

        return results;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                     "Exception during search", {
                         {"error", e.what()},
                         {"query", query}
                     });
        return {};
    }
}

std::vector<RegulatoryChange> RegulatoryKnowledgeBase::get_changes_by_impact(
    RegulatoryImpact impact_level,
    size_t limit) const {

    try {
        std::vector<RegulatoryChange> results;

        {
            std::lock_guard<std::mutex> lock(index_mutex_);
            auto it = impact_index_.find(impact_level);
            if (it != impact_index_.end()) {
                std::lock_guard<std::mutex> storage_lock(storage_mutex_);
                for (const auto& id : it->second) {
                    auto change_it = changes_store_.find(id);
                    if (change_it != changes_store_.end()) {
                        results.push_back(change_it->second);
                        if (results.size() >= limit) {
                            break;
                        }
                    }
                }
            }
        }

        return results;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                     "Exception getting changes by impact", {
                         {"error", e.what()},
                         {"impact_level", static_cast<int>(impact_level)}
                     });
        return {};
    }
}

std::vector<RegulatoryChange> RegulatoryKnowledgeBase::get_changes_by_domain(
    BusinessDomain domain,
    size_t limit) const {

    try {
        std::vector<RegulatoryChange> results;

        {
            std::lock_guard<std::mutex> lock(index_mutex_);
            auto it = domain_index_.find(domain);
            if (it != domain_index_.end()) {
                std::lock_guard<std::mutex> storage_lock(storage_mutex_);
                for (const auto& id : it->second) {
                    auto change_it = changes_store_.find(id);
                    if (change_it != changes_store_.end()) {
                        results.push_back(change_it->second);
                        if (results.size() >= limit) {
                            break;
                        }
                    }
                }
            }
        }

        return results;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                     "Exception getting changes by domain", {
                         {"error", e.what()},
                         {"domain", static_cast<int>(domain)}
                     });
        return {};
    }
}

std::vector<RegulatoryChange> RegulatoryKnowledgeBase::get_changes_by_body(
    const std::string& regulatory_body,
    size_t limit) const {

    try {
        std::vector<RegulatoryChange> results;

        {
            std::lock_guard<std::mutex> lock(index_mutex_);
            auto it = body_index_.find(regulatory_body);
            if (it != body_index_.end()) {
                std::lock_guard<std::mutex> storage_lock(storage_mutex_);
                for (const auto& id : it->second) {
                    auto change_it = changes_store_.find(id);
                    if (change_it != changes_store_.end()) {
                        results.push_back(change_it->second);
                        if (results.size() >= limit) {
                            break;
                        }
                    }
                }
            }
        }

        return results;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                     "Exception getting changes by regulatory body", {
                         {"error", e.what()},
                         {"regulatory_body", regulatory_body}
                     });
        return {};
    }
}

std::vector<RegulatoryChange> RegulatoryKnowledgeBase::get_recent_changes(
    int days,
    size_t limit) const {

    try {
        auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24 * days);
        std::vector<RegulatoryChange> results;

        {
            std::lock_guard<std::mutex> lock(storage_mutex_);
            for (const auto& [id, change] : changes_store_) {
                if (change.get_detected_at() >= cutoff_time) {
                    results.push_back(change);
                }
            }
        }

        // Sort by detected_at (most recent first)
        std::sort(results.begin(), results.end(),
                  [](const RegulatoryChange& a, const RegulatoryChange& b) {
                      return a.get_detected_at() > b.get_detected_at();
                  });

        // Limit results
        if (results.size() > limit) {
            results.resize(limit);
        }

        return results;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                     "Exception getting recent changes", {
                         {"error", e.what()},
                         {"days", std::to_string(days)}
                     });
        return {};
    }
}

bool RegulatoryKnowledgeBase::update_change_status(
    const std::string& change_id,
    RegulatoryChangeStatus new_status) {

    try {
        // Update in database
        auto db_config = config_->get_database_config();
        auto pool = std::make_shared<ConnectionPool>(db_config);
        auto conn = pool->get_connection();

        if (!conn || !conn->is_connected()) {
            logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                         "Failed to get database connection", {});
            return false;
        }

        std::ostringstream sql;
        sql << "UPDATE regulatory_changes SET status = " << static_cast<int>(new_status);

        if (new_status == RegulatoryChangeStatus::DISTRIBUTED) {
            auto now = std::chrono::system_clock::now();
            sql << ", distributed_at = " << to_milliseconds(now);
        }

        sql << " WHERE change_id = '" << escape_sql_string(change_id) << "'";

        if (!conn->execute_command(sql.str())) {
            logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                         "Failed to update change status in database", {
                             {"change_id", change_id}
                         });
            pool->return_connection(conn);
            return false;
        }

        pool->return_connection(conn);

        // Update in-memory
        {
            std::lock_guard<std::mutex> lock(storage_mutex_);
            auto it = changes_store_.find(change_id);
            if (it != changes_store_.end()) {
                it->second.set_status(new_status);
            }
        }

        logger_->log(LogLevel::INFO, "regulatory_knowledge_base",
                     "Updated change status", {
                         {"change_id", change_id},
                         {"new_status", std::to_string(static_cast<int>(new_status))}
                     });

        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                     "Exception updating change status", {
                         {"error", e.what()},
                         {"change_id", change_id}
                     });
        return false;
    }
}

nlohmann::json RegulatoryKnowledgeBase::get_statistics() const {
    try {
        nlohmann::json stats;

        stats["total_changes"] = total_changes_.load();
        stats["high_impact_changes"] = high_impact_changes_.load();
        stats["critical_changes"] = critical_changes_.load();
        stats["last_update_time"] = to_milliseconds(last_update_time_);

        {
            std::lock_guard<std::mutex> lock(storage_mutex_);
            stats["in_memory_changes"] = changes_store_.size();
        }

        {
            std::lock_guard<std::mutex> lock(index_mutex_);
            stats["indexed_words"] = word_index_.size();
            stats["indexed_bodies"] = body_index_.size();
        }

        return stats;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                     "Exception getting statistics", {
                         {"error", e.what()}
                     });
        return nlohmann::json::object();
    }
}

nlohmann::json RegulatoryKnowledgeBase::export_to_json() const {
    try {
        nlohmann::json export_data;
        export_data["version"] = "1.0";
        export_data["export_timestamp"] = to_milliseconds(std::chrono::system_clock::now());
        export_data["total_changes"] = total_changes_.load();

        nlohmann::json changes_array = nlohmann::json::array();

        {
            std::lock_guard<std::mutex> lock(storage_mutex_);
            for (const auto& [id, change] : changes_store_) {
                changes_array.push_back(change.to_json());
            }
        }

        export_data["changes"] = changes_array;

        return export_data;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                     "Exception exporting to JSON", {
                         {"error", e.what()}
                     });
        return nlohmann::json::object();
    }
}

bool RegulatoryKnowledgeBase::import_from_json(const nlohmann::json& json) {
    try {
        if (!json.contains("changes") || !json["changes"].is_array()) {
            logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                         "Invalid import JSON format", {});
            return false;
        }

        size_t imported_count = 0;

        for (const auto& change_json : json["changes"]) {
            auto change_opt = RegulatoryChange::from_json(change_json);
            if (change_opt) {
                if (store_regulatory_change(*change_opt)) {
                    imported_count++;
                }
            }
        }

        logger_->log(LogLevel::INFO, "regulatory_knowledge_base",
                     "Imported regulatory changes", {
                         {"imported_count", std::to_string(imported_count)},
                         {"total_in_json", std::to_string(json["changes"].size())}
                     });

        return imported_count > 0;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                     "Exception importing from JSON", {
                         {"error", e.what()}
                     });
        return false;
    }
}

void RegulatoryKnowledgeBase::clear() {
    try {
        // Clear database
        auto db_config = config_->get_database_config();
        auto pool = std::make_shared<ConnectionPool>(db_config);
        auto conn = pool->get_connection();

        if (conn && conn->is_connected()) {
            conn->execute_command("DELETE FROM regulatory_changes");
            pool->return_connection(conn);
        }

        // Clear in-memory structures
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

        logger_->log(LogLevel::INFO, "regulatory_knowledge_base",
                     "Cleared all regulatory changes", {});

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                     "Exception clearing knowledge base", {
                         {"error", e.what()}
                     });
    }
}

// Private helper methods

void RegulatoryKnowledgeBase::index_change(const RegulatoryChange& change) {
    try {
        std::lock_guard<std::mutex> lock(index_mutex_);

        const auto& change_id = change.get_change_id();

        // Index title
        auto title_tokens = tokenize(change.get_title());
        for (const auto& token : title_tokens) {
            word_index_[token].insert(change_id);
        }

        // Index regulatory body
        const auto& metadata = change.get_metadata();
        if (!metadata.regulatory_body.empty()) {
            body_index_[metadata.regulatory_body].insert(change_id);
        }

        // Index keywords
        for (const auto& keyword : metadata.keywords) {
            auto keyword_tokens = tokenize(keyword);
            for (const auto& token : keyword_tokens) {
                word_index_[token].insert(change_id);
            }
        }

        // Index analysis if present
        const auto& analysis = change.get_analysis();
        if (analysis) {
            // Index by impact level
            impact_index_[analysis->impact_level].insert(change_id);

            // Index by affected domains
            for (auto domain : analysis->affected_domains) {
                domain_index_[domain].insert(change_id);
            }

            // Index executive summary
            auto summary_tokens = tokenize(analysis->executive_summary);
            for (const auto& token : summary_tokens) {
                word_index_[token].insert(change_id);
            }
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                     "Exception indexing change", {
                         {"error", e.what()},
                         {"change_id", change.get_change_id()}
                     });
    }
}

void RegulatoryKnowledgeBase::remove_from_index(const RegulatoryChange& change) {
    try {
        std::lock_guard<std::mutex> lock(index_mutex_);

        const auto& change_id = change.get_change_id();

        // Remove from word index
        for (auto& [word, ids] : word_index_) {
            ids.erase(change_id);
        }

        // Remove from impact index
        const auto& analysis = change.get_analysis();
        if (analysis) {
            impact_index_[analysis->impact_level].erase(change_id);

            for (auto domain : analysis->affected_domains) {
                domain_index_[domain].erase(change_id);
            }
        }

        // Remove from body index
        const auto& metadata = change.get_metadata();
        if (!metadata.regulatory_body.empty()) {
            body_index_[metadata.regulatory_body].erase(change_id);
        }

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                     "Exception removing from index", {
                         {"error", e.what()},
                         {"change_id", change.get_change_id()}
                     });
    }
}

void RegulatoryKnowledgeBase::create_search_index(const std::string& text,
                                                  const std::string& change_id) {
    auto tokens = tokenize(text);
    std::lock_guard<std::mutex> lock(index_mutex_);

    for (const auto& token : tokens) {
        word_index_[token].insert(change_id);
    }
}

std::unordered_set<std::string> RegulatoryKnowledgeBase::search_index(
    const std::string& query) const {

    auto query_tokens = tokenize(query);
    std::unordered_set<std::string> results;

    if (query_tokens.empty()) {
        return results;
    }

    std::lock_guard<std::mutex> lock(index_mutex_);

    // Start with first token's matches
    auto it = word_index_.find(query_tokens[0]);
    if (it != word_index_.end()) {
        results = it->second;
    }

    // Intersect with remaining tokens (AND logic)
    for (size_t i = 1; i < query_tokens.size() && !results.empty(); ++i) {
        auto token_it = word_index_.find(query_tokens[i]);
        if (token_it != word_index_.end()) {
            std::unordered_set<std::string> intersection;
            for (const auto& id : results) {
                if (token_it->second.find(id) != token_it->second.end()) {
                    intersection.insert(id);
                }
            }
            results = std::move(intersection);
        } else {
            results.clear();
            break;
        }
    }

    return results;
}

std::unordered_set<std::string> RegulatoryKnowledgeBase::apply_filters(
    const std::unordered_set<std::string>& change_ids,
    const std::unordered_map<std::string, std::string>& filters) const {

    std::unordered_set<std::string> filtered = change_ids;

    std::lock_guard<std::mutex> lock(storage_mutex_);

    for (auto it = filtered.begin(); it != filtered.end();) {
        auto change_it = changes_store_.find(*it);
        if (change_it == changes_store_.end()) {
            it = filtered.erase(it);
            continue;
        }

        const auto& change = change_it->second;
        bool matches = true;

        // Apply filters
        for (const auto& [key, value] : filters) {
            if (key == "regulatory_body") {
                if (change.get_metadata().regulatory_body != value) {
                    matches = false;
                    break;
                }
            } else if (key == "impact_level") {
                const auto& analysis = change.get_analysis();
                if (!analysis || regulatory_impact_to_string(analysis->impact_level) != value) {
                    matches = false;
                    break;
                }
            }
            // Add more filter types as needed
        }

        if (!matches) {
            it = filtered.erase(it);
        } else {
            ++it;
        }
    }

    return filtered;
}

bool RegulatoryKnowledgeBase::persist_to_storage() {
    try {
        logger_->log(LogLevel::INFO, "regulatory_knowledge_base",
                     "Persisting knowledge base to storage", {});

        // Export to JSON file
        auto export_json = export_to_json();

        std::string storage_file = "regulatory_knowledge_base.json";
        if (!storage_path_.empty()) {
            storage_file = storage_path_ + "/regulatory_knowledge_base.json";
        }

        std::ofstream file(storage_file);
        if (!file.is_open()) {
            logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                         "Failed to open storage file for writing", {
                             {"file", storage_file}
                         });
            return false;
        }

        file << export_json.dump(2);
        file.close();

        logger_->log(LogLevel::INFO, "regulatory_knowledge_base",
                     "Successfully persisted knowledge base", {
                         {"file", storage_file},
                         {"total_changes", std::to_string(total_changes_.load())}
                     });

        return true;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                     "Exception persisting to storage", {
                         {"error", e.what()}
                     });
        return false;
    }
}

bool RegulatoryKnowledgeBase::load_from_storage() {
    try {
        logger_->log(LogLevel::INFO, "regulatory_knowledge_base",
                     "Loading knowledge base from storage", {});

        std::string storage_file = "regulatory_knowledge_base.json";
        if (!storage_path_.empty()) {
            storage_file = storage_path_ + "/regulatory_knowledge_base.json";
        }

        if (!std::filesystem::exists(storage_file)) {
            logger_->log(LogLevel::INFO, "regulatory_knowledge_base",
                         "No storage file found, starting with empty knowledge base", {});
            return true;
        }

        std::ifstream file(storage_file);
        if (!file.is_open()) {
            logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                         "Failed to open storage file for reading", {
                             {"file", storage_file}
                         });
            return false;
        }

        nlohmann::json import_json;
        file >> import_json;
        file.close();

        if (import_from_json(import_json)) {
            logger_->log(LogLevel::INFO, "regulatory_knowledge_base",
                         "Successfully loaded knowledge base from storage", {
                             {"file", storage_file},
                             {"total_changes", std::to_string(total_changes_.load())}
                         });
            return true;
        }

        return false;

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "regulatory_knowledge_base",
                     "Exception loading from storage", {
                         {"error", e.what()}
                     });
        return false;
    }
}

} // namespace regulens
