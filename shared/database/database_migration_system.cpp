/**
 * Database Migration System - Implementation
 * 
 * Production-grade database migration system for schema updates and data seeding.
 * Supports versioning, rollback, and transactional migrations.
 */

#include "database_migration_system.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace regulens {

DatabaseMigrationSystem::DatabaseMigrationSystem(
    const std::string& connection_string,
    const std::string& migration_table_name
) : connection_string_(connection_string),
    migration_table_name_(migration_table_name),
    migration_lock_table_name_(migration_table_name + "_lock"),
    connection_(nullptr),
    initialized_(false),
    has_lock_(false) {
}

DatabaseMigrationSystem::~DatabaseMigrationSystem() {
    if (has_lock_) {
        release_migration_lock();
    }
    disconnect();
}

bool DatabaseMigrationSystem::initialize() {
    if (initialized_) {
        return true;
    }

    if (!connect()) {
        std::cerr << "Failed to connect to database for migration system" << std::endl;
        return false;
    }

    if (!create_migration_table()) {
        std::cerr << "Failed to create migration table" << std::endl;
        return false;
    }

    if (!create_lock_table()) {
        std::cerr << "Failed to create migration lock table" << std::endl;
        return false;
    }

    if (!load_applied_migrations()) {
        std::cerr << "Failed to load applied migrations" << std::endl;
        return false;
    }

    initialized_ = true;
    return true;
}

bool DatabaseMigrationSystem::register_migration(
    const std::string& version,
    const std::string& name,
    const std::string& description,
    const std::string& up_sql,
    const std::string& down_sql
) {
    if (registered_migrations_.find(version) != registered_migrations_.end()) {
        std::cerr << "Migration " << version << " already registered" << std::endl;
        return false;
    }

    Migration migration;
    migration.version = version;
    migration.name = name;
    migration.description = description;
    migration.up_sql = up_sql;
    migration.down_sql = down_sql;
    migration.status = MigrationStatus::PENDING;
    migration.execution_time_ms = 0;

    registered_migrations_[version] = migration;
    return true;
}

bool DatabaseMigrationSystem::register_migration_from_files(
    const std::string& version,
    const std::string& name,
    const std::string& description,
    const std::string& up_sql_file,
    const std::string& down_sql_file
) {
    std::string up_sql = read_sql_file(up_sql_file);
    if (up_sql.empty()) {
        std::cerr << "Failed to read up migration file: " << up_sql_file << std::endl;
        return false;
    }

    std::string down_sql = read_sql_file(down_sql_file);
    if (down_sql.empty()) {
        std::cerr << "Failed to read down migration file: " << down_sql_file << std::endl;
        return false;
    }

    return register_migration(version, name, description, up_sql, down_sql);
}

bool DatabaseMigrationSystem::register_migration_with_callbacks(
    const std::string& version,
    const std::string& name,
    const std::string& description,
    MigrationCallback up_callback,
    MigrationCallback down_callback
) {
    if (registered_migrations_.find(version) != registered_migrations_.end()) {
        std::cerr << "Migration " << version << " already registered" << std::endl;
        return false;
    }

    Migration migration;
    migration.version = version;
    migration.name = name;
    migration.description = description;
    migration.up_sql = "";  // Callbacks don't use SQL
    migration.down_sql = "";
    migration.status = MigrationStatus::PENDING;
    migration.execution_time_ms = 0;

    registered_migrations_[version] = migration;
    up_callbacks_[version] = up_callback;
    down_callbacks_[version] = down_callback;

    return true;
}

bool DatabaseMigrationSystem::migrate(bool dry_run) {
    if (!initialized_ && !initialize()) {
        std::cerr << "Migration system not initialized" << std::endl;
        return false;
    }

    if (!dry_run && !acquire_migration_lock()) {
        std::cerr << "Failed to acquire migration lock - another migration may be running" << std::endl;
        return false;
    }

    auto pending = get_pending_migrations();
    if (pending.empty()) {
        std::cout << "No pending migrations" << std::endl;
        if (!dry_run) {
            release_migration_lock();
        }
        return true;
    }

    // Sort migrations by version
    std::sort(pending.begin(), pending.end(), [](const Migration& a, const Migration& b) {
        return a.version < b.version;
    });

    std::cout << "Found " << pending.size() << " pending migrations:" << std::endl;
    for (const auto& migration : pending) {
        std::cout << "  " << migration.version << ": " << migration.name << std::endl;
    }

    if (dry_run) {
        std::cout << "Dry run mode - no migrations will be applied" << std::endl;
        return true;
    }

    // Apply each migration
    bool all_successful = true;
    for (auto& migration : pending) {
        std::cout << "Applying migration " << migration.version << ": " << migration.name << "..." << std::endl;

        if (!apply_migration(migration, false)) {
            std::cerr << "Migration " << migration.version << " failed" << std::endl;
            all_successful = false;
            break;  // Stop on first failure
        }

        std::cout << "Migration " << migration.version << " applied successfully (took " 
                  << migration.execution_time_ms << " ms)" << std::endl;
    }

    release_migration_lock();
    return all_successful;
}

bool DatabaseMigrationSystem::rollback(bool dry_run) {
    if (!initialized_ && !initialize()) {
        std::cerr << "Migration system not initialized" << std::endl;
        return false;
    }

    auto applied = get_applied_migrations();
    if (applied.empty()) {
        std::cout << "No migrations to rollback" << std::endl;
        return true;
    }

    // Sort by version descending to get the most recent
    std::sort(applied.begin(), applied.end(), [](const Migration& a, const Migration& b) {
        return a.version > b.version;
    });

    const auto& migration = applied[0];
    
    std::cout << "Rolling back migration " << migration.version << ": " << migration.name << std::endl;

    if (dry_run) {
        std::cout << "Dry run mode - no rollback will be performed" << std::endl;
        return true;
    }

    if (!acquire_migration_lock()) {
        std::cerr << "Failed to acquire migration lock" << std::endl;
        return false;
    }

    bool success = rollback_migration(migration, false);

    release_migration_lock();

    if (success) {
        std::cout << "Rollback successful" << std::endl;
    } else {
        std::cerr << "Rollback failed" << std::endl;
    }

    return success;
}

bool DatabaseMigrationSystem::rollback_to(const std::string& target_version, bool dry_run) {
    if (!initialized_ && !initialize()) {
        std::cerr << "Migration system not initialized" << std::endl;
        return false;
    }

    auto applied = get_applied_migrations();
    if (applied.empty()) {
        std::cout << "No migrations to rollback" << std::endl;
        return true;
    }

    // Sort by version descending
    std::sort(applied.begin(), applied.end(), [](const Migration& a, const Migration& b) {
        return a.version > b.version;
    });

    // Find migrations to rollback
    std::vector<Migration> to_rollback;
    for (const auto& migration : applied) {
        if (migration.version > target_version) {
            to_rollback.push_back(migration);
        }
    }

    if (to_rollback.empty()) {
        std::cout << "Already at or before version " << target_version << std::endl;
        return true;
    }

    std::cout << "Rolling back " << to_rollback.size() << " migrations to version " << target_version << std::endl;

    if (dry_run) {
        for (const auto& migration : to_rollback) {
            std::cout << "  Would rollback: " << migration.version << ": " << migration.name << std::endl;
        }
        return true;
    }

    if (!acquire_migration_lock()) {
        std::cerr << "Failed to acquire migration lock" << std::endl;
        return false;
    }

    bool all_successful = true;
    for (const auto& migration : to_rollback) {
        std::cout << "Rolling back migration " << migration.version << ": " << migration.name << "..." << std::endl;

        if (!rollback_migration(migration, false)) {
            std::cerr << "Rollback of " << migration.version << " failed" << std::endl;
            all_successful = false;
            break;
        }

        std::cout << "Rollback of " << migration.version << " successful" << std::endl;
    }

    release_migration_lock();
    return all_successful;
}

std::string DatabaseMigrationSystem::get_current_version() const {
    auto applied = get_applied_migrations();
    if (applied.empty()) {
        return "";
    }

    // Sort by version descending and return the highest
    auto max_migration = std::max_element(applied.begin(), applied.end(),
        [](const Migration& a, const Migration& b) {
            return a.version < b.version;
        });

    return max_migration->version;
}

std::vector<Migration> DatabaseMigrationSystem::get_all_migrations() const {
    std::vector<Migration> migrations;
    for (const auto& pair : registered_migrations_) {
        migrations.push_back(pair.second);
    }
    return migrations;
}

std::vector<Migration> DatabaseMigrationSystem::get_pending_migrations() const {
    std::vector<Migration> pending;
    for (const auto& pair : registered_migrations_) {
        const auto& migration = pair.second;
        if (migration.status == MigrationStatus::PENDING) {
            pending.push_back(migration);
        }
    }
    return pending;
}

std::vector<Migration> DatabaseMigrationSystem::get_applied_migrations() const {
    std::vector<Migration> applied;
    for (const auto& pair : registered_migrations_) {
        const auto& migration = pair.second;
        if (migration.status == MigrationStatus::COMPLETED) {
            applied.push_back(migration);
        }
    }
    return applied;
}

bool DatabaseMigrationSystem::is_initialized() const {
    return initialized_;
}

bool DatabaseMigrationSystem::create_backup(const std::string& backup_path) {
    if (!is_connected()) {
        std::cerr << "Not connected to database" << std::endl;
        return false;
    }

    // Use pg_dump to create a backup
    std::string command = "pg_dump \"" + connection_string_ + "\" > " + backup_path;
    
    std::cout << "Creating backup: " << backup_path << std::endl;
    
    int result = system(command.c_str());
    if (result != 0) {
        std::cerr << "Backup failed with code: " << result << std::endl;
        return false;
    }

    std::cout << "Backup created successfully" << std::endl;
    return true;
}

bool DatabaseMigrationSystem::restore_backup(const std::string& backup_path) {
    if (!is_connected()) {
        std::cerr << "Not connected to database" << std::endl;
        return false;
    }

    // Check if backup file exists
    std::ifstream f(backup_path);
    if (!f.good()) {
        std::cerr << "Backup file does not exist: " << backup_path << std::endl;
        return false;
    }
    f.close();

    // Use psql to restore
    std::string command = "psql \"" + connection_string_ + "\" < " + backup_path;
    
    std::cout << "Restoring from backup: " << backup_path << std::endl;
    
    int result = system(command.c_str());
    if (result != 0) {
        std::cerr << "Restore failed with code: " << result << std::endl;
        return false;
    }

    std::cout << "Restore completed successfully" << std::endl;
    return true;
}

bool DatabaseMigrationSystem::validate_migrations() const {
    for (const auto& pair : registered_migrations_) {
        const auto& migration = pair.second;

        // Check for required fields
        if (migration.version.empty()) {
            std::cerr << "Migration missing version" << std::endl;
            return false;
        }

        if (migration.name.empty()) {
            std::cerr << "Migration " << migration.version << " missing name" << std::endl;
            return false;
        }

        // Check that either SQL or callbacks are provided
        bool has_sql = !migration.up_sql.empty() && !migration.down_sql.empty();
        bool has_callbacks = up_callbacks_.find(migration.version) != up_callbacks_.end() &&
                           down_callbacks_.find(migration.version) != down_callbacks_.end();

        if (!has_sql && !has_callbacks) {
            std::cerr << "Migration " << migration.version << " has neither SQL nor callbacks" << std::endl;
            return false;
        }
    }

    return true;
}

bool DatabaseMigrationSystem::acquire_migration_lock() {
    if (has_lock_) {
        return true;
    }

    if (!is_connected()) {
        return false;
    }

    // Try to acquire a lock using PostgreSQL advisory lock
    std::string query = "SELECT pg_try_advisory_lock(123456789)";
    PGresult* result = PQexec(connection_, query.c_str());

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        std::cerr << "Failed to acquire lock: " << PQerrorMessage(connection_) << std::endl;
        PQclear(result);
        return false;
    }

    bool lock_acquired = (strcmp(PQgetvalue(result, 0, 0), "t") == 0);
    PQclear(result);

    if (lock_acquired) {
        has_lock_ = true;
        
        // Also insert a record in the lock table for tracking
        std::string insert_query = "INSERT INTO " + migration_lock_table_name_ + 
                                  " (locked_at, locked_by) VALUES (NOW(), 'migration_system')";
        PGresult* insert_result = PQexec(connection_, insert_query.c_str());
        PQclear(insert_result);
    }

    return lock_acquired;
}

bool DatabaseMigrationSystem::release_migration_lock() {
    if (!has_lock_) {
        return true;
    }

    if (!is_connected()) {
        return false;
    }

    // Release PostgreSQL advisory lock
    std::string query = "SELECT pg_advisory_unlock(123456789)";
    PGresult* result = PQexec(connection_, query.c_str());

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        std::cerr << "Failed to release lock: " << PQerrorMessage(connection_) << std::endl;
        PQclear(result);
        return false;
    }

    PQclear(result);
    has_lock_ = false;

    // Delete lock record
    std::string delete_query = "DELETE FROM " + migration_lock_table_name_;
    PGresult* delete_result = PQexec(connection_, delete_query.c_str());
    PQclear(delete_result);

    return true;
}

bool DatabaseMigrationSystem::is_locked() const {
    if (!is_connected()) {
        return false;
    }

    std::string query = "SELECT COUNT(*) FROM " + migration_lock_table_name_;
    PGresult* result = PQexec(connection_, query.c_str());

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        PQclear(result);
        return false;
    }

    int count = atoi(PQgetvalue(result, 0, 0));
    PQclear(result);

    return count > 0;
}

// Private methods

bool DatabaseMigrationSystem::create_migration_table() {
    std::string query = "CREATE TABLE IF NOT EXISTS " + migration_table_name_ + " ("
                       "version VARCHAR(50) PRIMARY KEY, "
                       "name VARCHAR(255) NOT NULL, "
                       "description TEXT, "
                       "applied_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(), "
                       "rolled_back_at TIMESTAMP WITH TIME ZONE, "
                       "execution_time_ms INTEGER, "
                       "error_message TEXT, "
                       "status VARCHAR(20) NOT NULL DEFAULT 'COMPLETED'"
                       ")";

    PGresult* result = PQexec(connection_, query.c_str());
    bool success = (PQresultStatus(result) == PGRES_COMMAND_OK);

    if (!success) {
        std::cerr << "Failed to create migration table: " << PQerrorMessage(connection_) << std::endl;
    }

    PQclear(result);
    return success;
}

bool DatabaseMigrationSystem::create_lock_table() {
    std::string query = "CREATE TABLE IF NOT EXISTS " + migration_lock_table_name_ + " ("
                       "id SERIAL PRIMARY KEY, "
                       "locked_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(), "
                       "locked_by VARCHAR(255) NOT NULL"
                       ")";

    PGresult* result = PQexec(connection_, query.c_str());
    bool success = (PQresultStatus(result) == PGRES_COMMAND_OK);

    if (!success) {
        std::cerr << "Failed to create lock table: " << PQerrorMessage(connection_) << std::endl;
    }

    PQclear(result);
    return success;
}

bool DatabaseMigrationSystem::apply_migration(const Migration& migration, bool dry_run) {
    if (dry_run) {
        std::cout << "Would apply migration: " << migration.version << std::endl;
        return true;
    }

    int execution_time_ms = 0;
    std::string error_message;
    bool success = false;

    // Check if we should use SQL or callback
    auto callback_it = up_callbacks_.find(migration.version);
    if (callback_it != up_callbacks_.end()) {
        // Use callback
        auto start = std::chrono::steady_clock::now();
        success = callback_it->second(connection_);
        auto end = std::chrono::steady_clock::now();
        execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    } else if (!migration.up_sql.empty()) {
        // Use SQL
        success = execute_sql_transaction(migration.up_sql, execution_time_ms, error_message);
    } else {
        error_message = "No migration method available";
        success = false;
    }

    if (success) {
        // Update local migration status
        Migration& mut_migration = registered_migrations_[migration.version];
        mut_migration.status = MigrationStatus::COMPLETED;
        mut_migration.applied_at = std::chrono::system_clock::now();
        mut_migration.execution_time_ms = execution_time_ms;

        // Record in database
        return record_migration(mut_migration);
    } else {
        // Update error status
        Migration& mut_migration = registered_migrations_[migration.version];
        mut_migration.status = MigrationStatus::FAILED;
        mut_migration.error_message = error_message;

        std::cerr << "Migration failed: " << error_message << std::endl;
        return false;
    }
}

bool DatabaseMigrationSystem::rollback_migration(const Migration& migration, bool dry_run) {
    if (dry_run) {
        std::cout << "Would rollback migration: " << migration.version << std::endl;
        return true;
    }

    int execution_time_ms = 0;
    std::string error_message;
    bool success = false;

    // Check if we should use SQL or callback
    auto callback_it = down_callbacks_.find(migration.version);
    if (callback_it != down_callbacks_.end()) {
        // Use callback
        auto start = std::chrono::steady_clock::now();
        success = callback_it->second(connection_);
        auto end = std::chrono::steady_clock::now();
        execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    } else if (!migration.down_sql.empty()) {
        // Use SQL
        success = execute_sql_transaction(migration.down_sql, execution_time_ms, error_message);
    } else {
        error_message = "No rollback method available";
        success = false;
    }

    if (success) {
        // Update local migration status
        Migration& mut_migration = registered_migrations_[migration.version];
        mut_migration.status = MigrationStatus::ROLLED_BACK;
        mut_migration.rolled_back_at = std::chrono::system_clock::now();

        // Remove from database
        return remove_migration_record(migration.version);
    } else {
        std::cerr << "Rollback failed: " << error_message << std::endl;
        return false;
    }
}

bool DatabaseMigrationSystem::record_migration(const Migration& migration) {
    std::stringstream query;
    query << "INSERT INTO " << migration_table_name_ 
          << " (version, name, description, applied_at, execution_time_ms, status) "
          << "VALUES ('" << migration.version << "', "
          << "'" << migration.name << "', "
          << "'" << migration.description << "', "
          << "NOW(), "
          << migration.execution_time_ms << ", "
          << "'COMPLETED')";

    PGresult* result = PQexec(connection_, query.str().c_str());
    bool success = (PQresultStatus(result) == PGRES_COMMAND_OK);

    if (!success) {
        std::cerr << "Failed to record migration: " << PQerrorMessage(connection_) << std::endl;
    }

    PQclear(result);
    return success;
}

bool DatabaseMigrationSystem::remove_migration_record(const std::string& version) {
    std::string query = "DELETE FROM " + migration_table_name_ + " WHERE version = '" + version + "'";

    PGresult* result = PQexec(connection_, query.c_str());
    bool success = (PQresultStatus(result) == PGRES_COMMAND_OK);

    if (!success) {
        std::cerr << "Failed to remove migration record: " << PQerrorMessage(connection_) << std::endl;
    }

    PQclear(result);
    return success;
}

bool DatabaseMigrationSystem::load_applied_migrations() {
    std::string query = "SELECT version, name, description, applied_at, execution_time_ms, status "
                       "FROM " + migration_table_name_ + " ORDER BY version";

    PGresult* result = PQexec(connection_, query.c_str());

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        std::cerr << "Failed to load applied migrations: " << PQerrorMessage(connection_) << std::endl;
        PQclear(result);
        return false;
    }

    int nrows = PQntuples(result);
    for (int i = 0; i < nrows; i++) {
        std::string version = PQgetvalue(result, i, 0);

        // Find this migration in registered migrations and update status
        auto it = registered_migrations_.find(version);
        if (it != registered_migrations_.end()) {
            it->second.status = MigrationStatus::COMPLETED;
            // Parse applied_at timestamp if needed
            // it->second.applied_at = ...;
            it->second.execution_time_ms = atoi(PQgetvalue(result, i, 4));
        }
    }

    PQclear(result);
    return true;
}

bool DatabaseMigrationSystem::execute_sql_transaction(const std::string& sql, int& execution_time_ms, std::string& error_message) {
    auto start = std::chrono::steady_clock::now();

    // Begin transaction
    PGresult* begin_result = PQexec(connection_, "BEGIN");
    if (PQresultStatus(begin_result) != PGRES_COMMAND_OK) {
        error_message = std::string("Failed to begin transaction: ") + PQerrorMessage(connection_);
        PQclear(begin_result);
        return false;
    }
    PQclear(begin_result);

    // Execute migration SQL
    PGresult* result = PQexec(connection_, sql.c_str());
    bool success = (PQresultStatus(result) == PGRES_COMMAND_OK || PQresultStatus(result) == PGRES_TUPLES_OK);

    if (!success) {
        error_message = std::string("SQL execution failed: ") + PQerrorMessage(connection_);
        PQclear(result);

        // Rollback transaction
        PGresult* rollback_result = PQexec(connection_, "ROLLBACK");
        PQclear(rollback_result);

        auto end = std::chrono::steady_clock::now();
        execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        return false;
    }

    PQclear(result);

    // Commit transaction
    PGresult* commit_result = PQexec(connection_, "COMMIT");
    if (PQresultStatus(commit_result) != PGRES_COMMAND_OK) {
        error_message = std::string("Failed to commit transaction: ") + PQerrorMessage(connection_);
        PQclear(commit_result);

        // Rollback transaction
        PGresult* rollback_result = PQexec(connection_, "ROLLBACK");
        PQclear(rollback_result);

        auto end = std::chrono::steady_clock::now();
        execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        return false;
    }

    PQclear(commit_result);

    auto end = std::chrono::steady_clock::now();
    execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    return true;
}

std::string DatabaseMigrationSystem::read_sql_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open SQL file: " << file_path << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    return buffer.str();
}

bool DatabaseMigrationSystem::connect() {
    if (connection_ && is_connected()) {
        return true;
    }

    connection_ = PQconnectdb(connection_string_.c_str());

    if (PQstatus(connection_) != CONNECTION_OK) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(connection_) << std::endl;
        PQfinish(connection_);
        connection_ = nullptr;
        return false;
    }

    return true;
}

void DatabaseMigrationSystem::disconnect() {
    if (connection_) {
        PQfinish(connection_);
        connection_ = nullptr;
    }
}

bool DatabaseMigrationSystem::is_connected() const {
    return connection_ && PQstatus(connection_) == CONNECTION_OK;
}

} // namespace regulens

