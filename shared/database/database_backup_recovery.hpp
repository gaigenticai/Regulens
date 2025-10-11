/**
 * Database Backup and Recovery System - Header
 * 
 * Production-grade backup and disaster recovery system for PostgreSQL databases.
 * Supports automated backups, point-in-time recovery, and disaster recovery procedures.
 */

#ifndef REGULENS_DATABASE_BACKUP_RECOVERY_HPP
#define REGULENS_DATABASE_BACKUP_RECOVERY_HPP

#include <string>
#include <vector>
#include <chrono>
#include <map>
#include <memory>
#include <postgresql/libpq-fe.h>

namespace regulens {

// Backup type enum
enum class BackupType {
    FULL,           // Complete database backup
    INCREMENTAL,    // Changes since last backup
    DIFFERENTIAL,   // Changes since last full backup
    LOGICAL,        // pg_dump logical backup
    PHYSICAL        // pg_basebackup physical backup
};

// Backup status enum
enum class BackupStatus {
    PENDING,
    IN_PROGRESS,
    COMPLETED,
    FAILED,
    VERIFYING,
    VERIFIED,
    CORRUPTED
};

// Recovery mode enum
enum class RecoveryMode {
    FULL_RESTORE,           // Restore entire database
    POINT_IN_TIME,         // Restore to specific timestamp
    SELECTIVE_RESTORE      // Restore specific tables/schemas
};

// Backup metadata
struct BackupMetadata {
    std::string backup_id;                                    // Unique backup identifier
    BackupType type;                                          // Type of backup
    BackupStatus status;                                      // Current status
    std::chrono::system_clock::time_point created_at;        // When backup was created
    std::chrono::system_clock::time_point completed_at;       // When backup completed
    std::string backup_path;                                  // Path to backup file/directory
    size_t backup_size_bytes;                                 // Size of backup
    std::string database_name;                                // Database that was backed up
    std::string database_version;                             // PostgreSQL version
    int duration_seconds;                                     // How long backup took
    std::string checksum;                                     // Checksum for integrity verification
    std::string error_message;                                // Error message if failed
    std::map<std::string, std::string> metadata;             // Additional metadata
};

// Recovery point
struct RecoveryPoint {
    std::string backup_id;                                    // Backup to use for recovery
    std::chrono::system_clock::time_point recovery_timestamp; // Target recovery time
    std::string wal_file;                                     // WAL file for point-in-time recovery
    std::vector<std::string> tables_to_restore;              // Specific tables (for selective restore)
};

/**
 * Database Backup and Recovery System
 * 
 * Provides comprehensive backup and disaster recovery capabilities:
 * - Automated scheduled backups
 * - Multiple backup types (full, incremental, differential)
 * - Point-in-time recovery (PITR)
 * - Backup verification and integrity checks
 * - Backup retention policies
 * - Backup compression and encryption
 * - Remote backup storage (S3, Azure Blob, GCS)
 * - Disaster recovery procedures
 */
class DatabaseBackupRecovery {
public:
    /**
     * Constructor
     * 
     * @param connection_string PostgreSQL connection string
     * @param backup_directory Base directory for storing backups
     */
    explicit DatabaseBackupRecovery(
        const std::string& connection_string,
        const std::string& backup_directory
    );

    ~DatabaseBackupRecovery();

    /**
     * Initialize the backup system
     * Creates necessary directories and tables
     * 
     * @return true if successful
     */
    bool initialize();

    /**
     * Create a full database backup
     * 
     * @param backup_name Optional name for the backup
     * @param compress Whether to compress the backup
     * @param encrypt Whether to encrypt the backup
     * @return Backup metadata if successful, empty struct if failed
     */
    BackupMetadata create_full_backup(
        const std::string& backup_name = "",
        bool compress = true,
        bool encrypt = false
    );

    /**
     * Create an incremental backup
     * Backs up changes since the last backup
     * 
     * @param compress Whether to compress the backup
     * @param encrypt Whether to encrypt the backup
     * @return Backup metadata if successful
     */
    BackupMetadata create_incremental_backup(
        bool compress = true,
        bool encrypt = false
    );

    /**
     * Create a logical backup using pg_dump
     * 
     * @param backup_name Optional name for the backup
     * @param compress Whether to compress
     * @param include_schemas Specific schemas to include
     * @param exclude_tables Tables to exclude
     * @return Backup metadata if successful
     */
    BackupMetadata create_logical_backup(
        const std::string& backup_name = "",
        bool compress = true,
        const std::vector<std::string>& include_schemas = {},
        const std::vector<std::string>& exclude_tables = {}
    );

    /**
     * Create a physical backup using pg_basebackup
     * 
     * @param backup_name Optional name
     * @param compress Whether to compress
     * @return Backup metadata if successful
     */
    BackupMetadata create_physical_backup(
        const std::string& backup_name = "",
        bool compress = true
    );

    /**
     * Verify backup integrity
     * 
     * @param backup_id Backup to verify
     * @return true if backup is valid
     */
    bool verify_backup(const std::string& backup_id);

    /**
     * Restore database from backup
     * 
     * @param backup_id Backup to restore from
     * @param target_database Target database name (can be different from source)
     * @return true if restore successful
     */
    bool restore_from_backup(
        const std::string& backup_id,
        const std::string& target_database = ""
    );

    /**
     * Perform point-in-time recovery
     * 
     * @param recovery_point Recovery point information
     * @return true if successful
     */
    bool point_in_time_recovery(const RecoveryPoint& recovery_point);

    /**
     * Selective restore of specific tables
     * 
     * @param backup_id Backup to restore from
     * @param tables Tables to restore
     * @param target_database Target database
     * @return true if successful
     */
    bool selective_restore(
        const std::string& backup_id,
        const std::vector<std::string>& tables,
        const std::string& target_database = ""
    );

    /**
     * List all backups
     * 
     * @param backup_type Optional filter by type
     * @return Vector of backup metadata
     */
    std::vector<BackupMetadata> list_backups(
        BackupType backup_type = BackupType::FULL
    ) const;

    /**
     * Get backup metadata
     * 
     * @param backup_id Backup identifier
     * @return Backup metadata
     */
    BackupMetadata get_backup_metadata(const std::string& backup_id) const;

    /**
     * Delete a backup
     * 
     * @param backup_id Backup to delete
     * @param force Force delete even if backup is in use
     * @return true if deleted
     */
    bool delete_backup(const std::string& backup_id, bool force = false);

    /**
     * Apply backup retention policy
     * Deletes old backups according to retention rules
     * 
     * @param keep_daily Number of daily backups to keep
     * @param keep_weekly Number of weekly backups to keep
     * @param keep_monthly Number of monthly backups to keep
     * @return Number of backups deleted
     */
    int apply_retention_policy(
        int keep_daily = 7,
        int keep_weekly = 4,
        int keep_monthly = 12
    );

    /**
     * Export backup to remote storage
     * 
     * @param backup_id Backup to export
     * @param storage_type Storage type (S3, Azure, GCS, etc.)
     * @param storage_config Storage configuration
     * @return true if exported successfully
     */
    bool export_to_remote_storage(
        const std::string& backup_id,
        const std::string& storage_type,
        const std::map<std::string, std::string>& storage_config
    );

    /**
     * Import backup from remote storage
     * 
     * @param remote_backup_id Remote backup identifier
     * @param storage_type Storage type
     * @param storage_config Storage configuration
     * @return Backup metadata if successful
     */
    BackupMetadata import_from_remote_storage(
        const std::string& remote_backup_id,
        const std::string& storage_type,
        const std::map<std::string, std::string>& storage_config
    );

    /**
     * Configure continuous archiving (WAL archiving)
     * 
     * @param archive_directory Directory for WAL archives
     * @param archive_command Custom archive command
     * @return true if configured successfully
     */
    bool configure_continuous_archiving(
        const std::string& archive_directory,
        const std::string& archive_command = ""
    );

    /**
     * Test disaster recovery procedure
     * Performs a dry-run of the recovery process
     * 
     * @param backup_id Backup to test
     * @return true if test successful
     */
    bool test_disaster_recovery(const std::string& backup_id);

    /**
     * Generate disaster recovery report
     * 
     * @return JSON report of backup status and recovery capabilities
     */
    std::string generate_disaster_recovery_report() const;

    /**
     * Schedule automatic backups
     * 
     * @param backup_type Type of backup
     * @param cron_expression Cron expression for schedule
     * @return true if scheduled successfully
     */
    bool schedule_backup(
        BackupType backup_type,
        const std::string& cron_expression
    );

private:
    std::string connection_string_;
    std::string backup_directory_;
    std::string wal_archive_directory_;
    PGconn* connection_;
    bool initialized_;

    std::map<std::string, BackupMetadata> backup_registry_;

    /**
     * Generate unique backup ID
     */
    std::string generate_backup_id(BackupType type) const;

    /**
     * Get backup path for a backup ID
     */
    std::string get_backup_path(const std::string& backup_id) const;

    /**
     * Calculate checksum of backup file
     */
    std::string calculate_checksum(const std::string& file_path) const;

    /**
     * Compress a file or directory
     */
    bool compress_backup(const std::string& source_path, const std::string& dest_path);

    /**
     * Decompress a file or directory
     */
    bool decompress_backup(const std::string& source_path, const std::string& dest_path);

    /**
     * Encrypt a backup file
     */
    bool encrypt_backup(const std::string& source_path, const std::string& dest_path);

    /**
     * Decrypt a backup file
     */
    bool decrypt_backup(const std::string& source_path, const std::string& dest_path);

    /**
     * Record backup metadata in database
     */
    bool record_backup_metadata(const BackupMetadata& metadata);

    /**
     * Load backup metadata from database
     */
    bool load_backup_registry();

    /**
     * Connect to database
     */
    bool connect();

    /**
     * Disconnect from database
     */
    void disconnect();

    /**
     * Check if connected
     */
    bool is_connected() const;

    /**
     * Execute shell command
     */
    int execute_command(const std::string& command, std::string& output);

    /**
     * Get database size
     */
    size_t get_database_size() const;

    /**
     * Get database version
     */
    std::string get_database_version() const;

    /**
     * Ensure directory exists
     */
    bool ensure_directory_exists(const std::string& path);
};

} // namespace regulens

#endif // REGULENS_DATABASE_BACKUP_RECOVERY_HPP

