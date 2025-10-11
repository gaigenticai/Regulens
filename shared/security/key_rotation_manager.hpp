/**
 * Key Rotation Manager - Header
 * 
 * Production-grade encryption key rotation and management system.
 * Supports automated key rotation, key versioning, and secure key storage.
 */

#ifndef REGULENS_KEY_ROTATION_MANAGER_HPP
#define REGULENS_KEY_ROTATION_MANAGER_HPP

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <mutex>

namespace regulens {

// Key type enum
enum class KeyType {
    ENCRYPTION,          // Data encryption keys
    SIGNING,            // JWT signing keys
    API_KEY,            // API authentication keys
    DATABASE_ENCRYPTION, // Database encryption keys
    TLS_CERTIFICATE     // TLS/SSL certificates
};

// Key status enum
enum class KeyStatus {
    ACTIVE,        // Currently in use
    PENDING,       // Scheduled to become active
    RETIRED,       // No longer used for new operations
    COMPROMISED,   // Known to be compromised
    REVOKED        // Explicitly revoked
};

// Encryption algorithm enum
enum class EncryptionAlgorithm {
    AES_256_GCM,
    AES_256_CBC,
    CHACHA20_POLY1305,
    RSA_2048,
    RSA_4096
};

// Key metadata
struct KeyMetadata {
    std::string key_id;                                          // Unique key identifier
    std::string key_version;                                     // Version string
    KeyType type;                                                // Key type
    KeyStatus status;                                            // Current status
    EncryptionAlgorithm algorithm;                              // Encryption algorithm
    std::chrono::system_clock::time_point created_at;           // Creation time
    std::chrono::system_clock::time_point activated_at;         // Activation time
    std::chrono::system_clock::time_point expires_at;           // Expiration time
    std::chrono::system_clock::time_point retired_at;           // Retirement time
    std::string created_by;                                      // Who created the key
    std::string purpose;                                         // Key purpose description
    std::map<std::string, std::string> metadata;                // Additional metadata
    int key_length_bits;                                         // Key length in bits
    bool is_master_key;                                         // Whether this is a master key
    std::string derivation_path;                                // Key derivation path (if derived)
};

// Key rotation policy
struct KeyRotationPolicy {
    KeyType key_type;
    int rotation_interval_days;                                  // How often to rotate
    int grace_period_days;                                       // Grace period for old keys
    bool auto_rotate;                                           // Whether to auto-rotate
    bool require_manual_approval;                               // Require approval for rotation
    std::vector<std::string> notification_recipients;           // Who to notify
};

/**
 * Key Rotation Manager
 * 
 * Manages encryption key lifecycle and rotation for compliance and security.
 * Features:
 * - Automated key rotation based on policies
 * - Key versioning and history
 * - Secure key storage (encrypted at rest)
 * - Key derivation from master keys
 * - Integration with HSM (Hardware Security Module)
 * - Audit logging of all key operations
 * - Emergency key revocation
 * - Re-encryption of data with new keys
 * - Compliance with FIPS 140-2/3
 */
class KeyRotationManager {
public:
    /**
     * Constructor
     * 
     * @param master_key_path Path to master key file
     * @param key_storage_path Path to key storage directory
     */
    explicit KeyRotationManager(
        const std::string& master_key_path,
        const std::string& key_storage_path
    );

    ~KeyRotationManager();

    /**
     * Initialize key rotation manager
     * 
     * @return true if successful
     */
    bool initialize();

    /**
     * Generate a new key
     * 
     * @param key_type Type of key to generate
     * @param algorithm Encryption algorithm
     * @param key_length_bits Key length in bits
     * @param purpose Key purpose description
     * @return Key ID if successful, empty string if failed
     */
    std::string generate_key(
        KeyType key_type,
        EncryptionAlgorithm algorithm,
        int key_length_bits,
        const std::string& purpose
    );

    /**
     * Rotate a key
     * Creates a new key version and retires the old one
     * 
     * @param key_id Key ID to rotate
     * @param immediate Whether to rotate immediately or schedule
     * @return New key ID if successful
     */
    std::string rotate_key(const std::string& key_id, bool immediate = true);

    /**
     * Get active key for a purpose
     * 
     * @param key_type Key type
     * @param purpose Key purpose
     * @return Key ID of active key
     */
    std::string get_active_key(KeyType key_type, const std::string& purpose);

    /**
     * Get key material (decrypted)
     * 
     * @param key_id Key ID
     * @param key_material Output buffer for key material
     * @return true if successful
     */
    bool get_key_material(const std::string& key_id, std::vector<uint8_t>& key_material);

    /**
     * Get key metadata
     * 
     * @param key_id Key ID
     * @return Key metadata
     */
    KeyMetadata get_key_metadata(const std::string& key_id);

    /**
     * List all keys
     * 
     * @param key_type Optional filter by type
     * @param status Optional filter by status
     * @return Vector of key metadata
     */
    std::vector<KeyMetadata> list_keys(
        KeyType key_type = KeyType::ENCRYPTION,
        KeyStatus status = KeyStatus::ACTIVE
    );

    /**
     * Retire a key
     * Marks key as retired, can still be used to decrypt old data
     * 
     * @param key_id Key ID
     * @param grace_period_days Grace period before complete retirement
     * @return true if successful
     */
    bool retire_key(const std::string& key_id, int grace_period_days = 30);

    /**
     * Revoke a key
     * Immediately revoke a key (for compromised keys)
     * 
     * @param key_id Key ID
     * @param reason Revocation reason
     * @return true if successful
     */
    bool revoke_key(const std::string& key_id, const std::string& reason);

    /**
     * Set rotation policy
     * 
     * @param policy Rotation policy
     * @return true if successful
     */
    bool set_rotation_policy(const KeyRotationPolicy& policy);

    /**
     * Get rotation policy
     * 
     * @param key_type Key type
     * @return Rotation policy
     */
    KeyRotationPolicy get_rotation_policy(KeyType key_type);

    /**
     * Check if key rotation is due
     * 
     * @param key_id Key ID
     * @return true if rotation is due
     */
    bool is_rotation_due(const std::string& key_id);

    /**
     * Perform automated key rotation
     * Rotates all keys that are due for rotation based on policies
     * 
     * @return Number of keys rotated
     */
    int perform_automated_rotation();

    /**
     * Re-encrypt data with new key
     * 
     * @param old_key_id Old key ID
     * @param new_key_id New key ID
     * @param encrypted_data Encrypted data
     * @param re_encrypted_data Output buffer for re-encrypted data
     * @return true if successful
     */
    bool re_encrypt_data(
        const std::string& old_key_id,
        const std::string& new_key_id,
        const std::vector<uint8_t>& encrypted_data,
        std::vector<uint8_t>& re_encrypted_data
    );

    /**
     * Derive key from master key
     * 
     * @param master_key_id Master key ID
     * @param derivation_path Derivation path (e.g., "m/0/1")
     * @param purpose Purpose description
     * @return Derived key ID
     */
    std::string derive_key(
        const std::string& master_key_id,
        const std::string& derivation_path,
        const std::string& purpose
    );

    /**
     * Export key (encrypted)
     * 
     * @param key_id Key ID to export
     * @param export_key_id Key to encrypt the export with
     * @param exported_data Output buffer
     * @return true if successful
     */
    bool export_key(
        const std::string& key_id,
        const std::string& export_key_id,
        std::vector<uint8_t>& exported_data
    );

    /**
     * Import key
     * 
     * @param encrypted_key_data Encrypted key data
     * @param import_key_id Key used to encrypt the import
     * @param key_metadata Key metadata
     * @return Imported key ID
     */
    std::string import_key(
        const std::vector<uint8_t>& encrypted_key_data,
        const std::string& import_key_id,
        const KeyMetadata& key_metadata
    );

    /**
     * Backup all keys
     * 
     * @param backup_path Path to backup file
     * @param encryption_key Key to encrypt backup
     * @return true if successful
     */
    bool backup_keys(const std::string& backup_path, const std::string& encryption_key);

    /**
     * Restore keys from backup
     * 
     * @param backup_path Path to backup file
     * @param encryption_key Key to decrypt backup
     * @return true if successful
     */
    bool restore_keys(const std::string& backup_path, const std::string& encryption_key);

    /**
     * Get key rotation audit log
     * 
     * @param key_id Optional filter by key ID
     * @return JSON audit log
     */
    std::string get_audit_log(const std::string& key_id = "");

    /**
     * Get key rotation statistics
     * 
     * @return JSON statistics
     */
    std::string get_statistics();

private:
    std::string master_key_path_;
    std::string key_storage_path_;
    std::vector<uint8_t> master_key_;
    bool initialized_;

    std::map<std::string, KeyMetadata> key_registry_;
    std::map<KeyType, KeyRotationPolicy> rotation_policies_;
    std::mutex key_mutex_;

    /**
     * Load master key
     */
    bool load_master_key();

    /**
     * Generate master key
     */
    bool generate_master_key();

    /**
     * Encrypt key material
     */
    std::vector<uint8_t> encrypt_key_material(
        const std::vector<uint8_t>& key_material,
        const std::vector<uint8_t>& encryption_key
    );

    /**
     * Decrypt key material
     */
    std::vector<uint8_t> decrypt_key_material(
        const std::vector<uint8_t>& encrypted_key_material,
        const std::vector<uint8_t>& decryption_key
    );

    /**
     * Store key securely
     */
    bool store_key(const KeyMetadata& metadata, const std::vector<uint8_t>& key_material);

    /**
     * Load key from storage
     */
    bool load_key(const std::string& key_id, std::vector<uint8_t>& key_material);

    /**
     * Delete key from storage
     */
    bool delete_key_from_storage(const std::string& key_id);

    /**
     * Generate random bytes
     */
    std::vector<uint8_t> generate_random_bytes(size_t length);

    /**
     * Generate key ID
     */
    std::string generate_key_id(KeyType type);

    /**
     * Generate key version
     */
    std::string generate_key_version(const std::string& key_id);

    /**
     * Log key operation
     */
    void log_key_operation(
        const std::string& operation,
        const std::string& key_id,
        const std::string& details
    );

    /**
     * Load key registry from storage
     */
    bool load_key_registry();

    /**
     * Save key registry to storage
     */
    bool save_key_registry();

    /**
     * Check key permissions
     */
    bool check_key_permissions(const std::string& key_id, const std::string& operation);
};

} // namespace regulens

#endif // REGULENS_KEY_ROTATION_MANAGER_HPP

