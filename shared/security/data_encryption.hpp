/**
 * Data Encryption & Privacy Module - Phase 7B.3
 * E2E encryption, PII masking, GDPR compliance
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace regulens {
namespace security {

using json = nlohmann::json;

// Data classification levels
enum class DataClassificationLevel {
  PUBLIC = 0,
  INTERNAL = 1,
  CONFIDENTIAL = 2,
  RESTRICTED = 3
};

// PII types
enum class PIIType {
  SSN,
  EMAIL,
  PHONE,
  ADDRESS,
  NAME,
  FINANCIAL_ACCOUNT,
  CREDIT_CARD,
  BIOMETRIC,
  CUSTOM
};

// Encryption mode
enum class EncryptionMode {
  AES_256_GCM,
  AES_256_CBC,
  CHACHA20_POLY1305
};

// Encryption key metadata
struct EncryptionKey {
  std::string key_id;
  EncryptionMode mode;
  std::string key_material;  // Encrypted key material
  std::string salt;
  std::string iv;
  int rotation_count = 0;
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point rotated_at;
  bool is_active = true;
};

// PII masking pattern
struct PIIMaskingPattern {
  PIIType pii_type;
  std::string regex_pattern;
  std::string mask_format;  // e.g., "***-**-####" for SSN
  int visible_chars = 0;  // Number of chars to show (start/end)
};

// GDPR consent record
struct GDPRConsent {
  std::string consent_id;
  std::string user_id;
  std::string data_purpose;
  bool consent_given = false;
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point expires_at;
};

// Data retention policy
struct DataRetentionPolicy {
  std::string policy_id;
  DataClassificationLevel classification;
  int retention_days = 0;  // 0 = indefinite
  bool auto_delete = false;
  bool requires_consent = true;
};

class DataEncryptionEngine {
public:
  DataEncryptionEngine();
  ~DataEncryptionEngine();

  // Encryption Operations
  std::string encrypt_data(
      const std::string& plaintext,
      const std::string& key_id,
      DataClassificationLevel classification = DataClassificationLevel::CONFIDENTIAL);
  
  std::string decrypt_data(
      const std::string& ciphertext,
      const std::string& key_id);

  // Key Management
  std::string generate_key(EncryptionMode mode = EncryptionMode::AES_256_GCM);
  
  bool rotate_key(const std::string& key_id);
  
  EncryptionKey get_key_metadata(const std::string& key_id);
  
  std::vector<EncryptionKey> get_all_active_keys();

  // PII Detection & Masking
  json detect_pii(const json& data);
  
  json mask_pii(
      const json& data,
      const std::vector<PIIType>& pii_types = {});
  
  std::string mask_value(const std::string& value, PIIType pii_type);
  
  bool register_pii_pattern(const PIIMaskingPattern& pattern);
  
  json unmask_pii(const std::string& masked_value, PIIType pii_type);

  // GDPR Compliance
  std::string record_consent(const GDPRConsent& consent);
  
  bool revoke_consent(const std::string& consent_id);
  
  bool has_valid_consent(
      const std::string& user_id,
      const std::string& data_purpose);
  
  json export_user_data(const std::string& user_id);
  
  bool delete_user_data(const std::string& user_id);
  
  bool update_retention_policy(const DataRetentionPolicy& policy);
  
  json generate_gdpr_audit_report(int days = 90);

  // Data Classification & Labeling
  void classify_data(const std::string& data_id, DataClassificationLevel level);
  
  DataClassificationLevel get_data_classification(const std::string& data_id);
  
  json apply_classification_encryption(
      const json& data,
      DataClassificationLevel classification);

  // Audit Trail for Encryption
  json get_encryption_audit_log(int days = 30);
  
  bool log_encryption_operation(
      const std::string& operation,
      const std::string& data_id,
      const std::string& user_id);

  // Database operations
  bool initialize_database();
  bool save_to_database();
  bool load_from_database();

private:
  std::map<std::string, EncryptionKey> encryption_keys_;
  std::map<PIIType, PIIMaskingPattern> pii_patterns_;
  std::vector<GDPRConsent> gdpr_consents_;
  std::vector<DataRetentionPolicy> retention_policies_;
  std::map<std::string, DataClassificationLevel> data_classifications_;

  std::mutex encryption_lock_;

  // Internal helpers
  std::string generate_random_bytes(int length);
  json scan_json_for_pii(const json& data);
  bool matches_pii_pattern(const std::string& value, PIIType pii_type);
};

}  // namespace security
}  // namespace regulens
