/**
 * Database Migration System - Header
 * 
 * Production-grade database migration system for schema updates and data seeding.
 * Supports versioning, rollback, and transactional migrations.
 */

#ifndef REGULENS_DATABASE_MIGRATION_SYSTEM_HPP
#define REGULENS_DATABASE_MIGRATION_SYSTEM_HPP

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <chrono>
#include <postgresql/libpq-fe.h>

namespace regulens {

// Migration status enum
enum class MigrationStatus {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    ROLLED_BACK
};

// Migration record
struct Migration {
    std::string version;                          // Version number (e.g., "001", "002")
    std::string name;                             // Human-readable name
    std::string description;                      // Description of what this migration does
    std::string up_sql;                           // SQL to apply the migration
    std::string down_sql;                         // SQL to rollback the migration
    MigrationStatus status;                       // Current status
    std::chrono::system_clock::time_point applied_at;  // When it was applied
    std::chrono::system_clock::time_point rolled_back_at;  // When it was rolled back
    int execution_time_ms;                        // How long it took to execute
    std::string error_message;                    // Error message if failed
};

// Migration callback function type
using MigrationCallback = std::function<bool(PGconn*)>;

/**
 * Database Migration System
 * 
 * Manages database schema changes and data seeding in a controlled,
 * versioned manner. Supports:
 * - Forward migrations (up)
 * - Rollback migrations (down)
 * - Transaction safety
 * - Version tracking
 * - Migration locking (prevents concurrent migrations)
 * - Automated backup before migration
 * - Dry-run mode for testing
 */
class DatabaseMigrationSystem {
public:
    /**
     * Constructor
     * 
     * @param connection_string PostgreSQL connection string
     * @param migration_table_name Name of the table to track migrations (default: "schema_migrations")
     */
    explicit DatabaseMigrationSystem(
        const std::string& connection_string,
        const std::string& migration_table_name = "schema_migrations"
    );

    ~DatabaseMigrationSystem();

    /**
     * Initialize the migration system
     * Creates the migration tracking table if it doesn't exist
     * 
     * @return true if successful, false otherwise
     */
    bool initialize();

    /**
     * Register a migration
     * 
     * @param version Version number (e.g., "001", "002")
     * @param name Human-readable name
     * @param description Description of the migration
     * @param up_sql SQL to apply the migration
     * @param down_sql SQL to rollback the migration
     * @return true if registered successfully
     */
    bool register_migration(
        const std::string& version,
        const std::string& name,
        const std::string& description,
        const std::string& up_sql,
        const std::string& down_sql
    );

    /**
     * Register a migration from SQL files
     * 
     * @param version Version number
     * @param name Human-readable name
     * @param description Description
     * @param up_sql_file Path to SQL file for migration
     * @param down_sql_file Path to SQL file for rollback
     * @return true if registered successfully
     */
    bool register_migration_from_files(
        const std::string& version,
        const std::string& name,
        const std::string& description,
        const std::string& up_sql_file,
        const std::string& down_sql_file
    );

    /**
     * Register a migration with callbacks
     * Allows programmatic migrations (not just SQL)
     * 
     * @param version Version number
     * @param name Human-readable name
     * @param description Description
     * @param up_callback Function to apply migration
     * @param down_callback Function to rollback migration
     * @return true if registered successfully
     */
    bool register_migration_with_callbacks(
        const std::string& version,
        const std::string& name,
        const std::string& description,
        MigrationCallback up_callback,
        MigrationCallback down_callback
    );

    /**
     * Run pending migrations
     * Applies all migrations that haven't been applied yet
     * 
     * @param dry_run If true, don't actually apply migrations, just check what would be done
     * @return true if all migrations applied successfully
     */
    bool migrate(bool dry_run = false);

    /**
     * Rollback the last migration
     * 
     * @param dry_run If true, don't actually rollback, just check what would be done
     * @return true if rollback successful
     */
    bool rollback(bool dry_run = false);

    /**
     * Rollback to a specific version
     * 
     * @param target_version Version to rollback to
     * @param dry_run If true, don't actually rollback
     * @return true if rollback successful
     */
    bool rollback_to(const std::string& target_version, bool dry_run = false);

    /**
     * Get current database version
     * 
     * @return Current version string, empty if no migrations applied
     */
    std::string get_current_version() const;

    /**
     * Get all migrations
     * 
     * @return Vector of all registered migrations
     */
    std::vector<Migration> get_all_migrations() const;

    /**
     * Get pending migrations
     * 
     * @return Vector of migrations that haven't been applied yet
     */
    std::vector<Migration> get_pending_migrations() const;

    /**
     * Get applied migrations
     * 
     * @return Vector of migrations that have been applied
     */
    std::vector<Migration> get_applied_migrations() const;

    /**
     * Check if migration system is initialized
     * 
     * @return true if initialized
     */
    bool is_initialized() const;

    /**
     * Create a backup of the database before migration
     * 
     * @param backup_path Path where backup should be stored
     * @return true if backup successful
     */
    bool create_backup(const std::string& backup_path);

    /**
     * Restore database from backup
     * 
     * @param backup_path Path to backup file
     * @return true if restore successful
     */
    bool restore_backup(const std::string& backup_path);

    /**
     * Validate all migrations
     * Checks that all migrations are properly formed
     * 
     * @return true if all migrations are valid
     */
    bool validate_migrations() const;

    /**
     * Lock migrations (prevent concurrent migrations)
     * 
     * @return true if lock acquired
     */
    bool acquire_migration_lock();

    /**
     * Release migration lock
     * 
     * @return true if lock released
     */
    bool release_migration_lock();

    /**
     * Check if migrations are locked
     * 
     * @return true if locked
     */
    bool is_locked() const;

private:
    std::string connection_string_;
    std::string migration_table_name_;
    std::string migration_lock_table_name_;
    PGconn* connection_;
    bool initialized_;
    bool has_lock_;

    std::map<std::string, Migration> registered_migrations_;
    std::map<std::string, MigrationCallback> up_callbacks_;
    std::map<std::string, MigrationCallback> down_callbacks_;

    /**
     * Create migration tracking table
     */
    bool create_migration_table();

    /**
     * Create migration lock table
     */
    bool create_lock_table();

    /**
     * Apply a single migration
     */
    bool apply_migration(const Migration& migration, bool dry_run);

    /**
     * Rollback a single migration
     */
    bool rollback_migration(const Migration& migration, bool dry_run);

    /**
     * Record migration application in database
     */
    bool record_migration(const Migration& migration);

    /**
     * Remove migration record from database
     */
    bool remove_migration_record(const std::string& version);

    /**
     * Load applied migrations from database
     */
    bool load_applied_migrations();

    /**
     * Execute SQL in a transaction
     */
    bool execute_sql_transaction(const std::string& sql, int& execution_time_ms, std::string& error_message);

    /**
     * Read SQL file content
     */
    std::string read_sql_file(const std::string& file_path);

    /**
     * Connect to database
     */
    bool connect();

    /**
     * Disconnect from database
     */
    void disconnect();

    /**
     * Check if connection is alive
     */
    bool is_connected() const;
};

} // namespace regulens

#endif // REGULENS_DATABASE_MIGRATION_SYSTEM_HPP

