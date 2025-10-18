/**
 * Audit Trail Engine - Phase 7B.2
 * Comprehensive change tracking, history, and rollback capability
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>
#include <chrono>
#include <optional>

namespace regulens {
namespace security {

using json = nlohmann::json;

// Entity types being tracked
enum class EntityType {
  RULE,
  DECISION,
  POLICY,
  ALERT,
  USER,
  ROLE,
  DATA_CLASSIFICATION,
  SYSTEM_CONFIG
};

// Change operation types
enum class ChangeOperation {
  CREATE,
  UPDATE,
  DELETE,
  ENABLE,
  DISABLE,
  DEPLOY,
  APPROVE,
  REJECT
};

// Change impact level
enum class ImpactLevel {
  LOW = 1,
  MEDIUM = 2,
  HIGH = 3,
  CRITICAL = 4
};

// Audit change record
struct ChangeRecord {
  std::string change_id;
  std::string user_id;
  EntityType entity_type;
  std::string entity_id;
  std::string entity_name;
  ChangeOperation operation;
  ImpactLevel impact_level;
  
  json old_value;  // Previous state
  json new_value;  // New state
  json changes_summary;  // Diff of changes
  
  std::string change_reason;
  std::string approval_id;  // Reference to approval workflow if applicable
  bool requires_approval = false;
  bool was_approved = false;
  
  std::map<std::string, std::string> metadata;  // Additional context (IP, session, etc.)
  std::chrono::system_clock::time_point changed_at;
  std::chrono::system_clock::time_point approved_at;
};

// Rollback request
struct RollbackRequest {
  std::string rollback_id;
  std::string requested_by;
  std::string target_change_id;  // The change to roll back
  std::string reason;
  std::vector<std::string> dependent_changes;  // Changes that might be affected
  bool requires_approval = false;
  std::string status;  // PENDING, APPROVED, EXECUTING, COMPLETED, FAILED
  json rollback_result;
  std::chrono::system_clock::time_point requested_at;
  std::chrono::system_clock::time_point executed_at;
};

// Entity history snapshot
struct EntitySnapshot {
  std::string snapshot_id;
  EntityType entity_type;
  std::string entity_id;
  int version_number = 0;
  json entity_state;
  std::string created_by;
  std::chrono::system_clock::time_point created_at;
  bool is_active = true;
};

// Change batch (multiple related changes)
struct ChangeBatch {
  std::string batch_id;
  std::string batch_name;
  std::string created_by;
  std::vector<std::string> change_ids;
  std::string status;  // PENDING, EXECUTING, COMPLETED, FAILED, PARTIAL
  std::string reason;
  int total_changes = 0;
  int completed_changes = 0;
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point executed_at;
};

// Compliance evidence
struct ComplianceEvidence {
  std::string evidence_id;
  std::string change_id;
  std::string evidence_type;  // EMAIL_APPROVAL, SCREENSHOT, AUDIT_LOG, etc.
  std::string evidence_content;
  std::string verified_by;
  bool is_verified = false;
  std::chrono::system_clock::time_point created_at;
};

class AuditTrailEngine {
public:
  AuditTrailEngine();
  ~AuditTrailEngine();

  // Change Recording
  std::string record_change(const ChangeRecord& change);
  
  bool approve_change(const std::string& change_id, const std::string& approver_id,
                     const std::string& comments = "");
  
  bool reject_change(const std::string& change_id, const std::string& rejector_id,
                    const std::string& reason);

  // Change Retrieval & Filtering
  ChangeRecord get_change(const std::string& change_id);
  
  std::vector<ChangeRecord> get_entity_history(
      const std::string& entity_id,
      EntityType entity_type,
      int limit = 100,
      int days = 90);
  
  std::vector<ChangeRecord> get_user_changes(
      const std::string& user_id,
      int days = 30,
      int limit = 100);
  
  std::vector<ChangeRecord> get_changes_by_operation(
      ChangeOperation operation,
      int days = 30,
      int limit = 100);
  
  std::vector<ChangeRecord> get_high_impact_changes(int days = 7);

  // Entity Versioning & Snapshots
  bool create_snapshot(const EntitySnapshot& snapshot);
  
  std::optional<EntitySnapshot> get_snapshot(const std::string& snapshot_id);
  
  std::vector<EntitySnapshot> get_entity_versions(
      const std::string& entity_id,
      EntityType entity_type,
      int limit = 10);
  
  json get_entity_at_point_in_time(
      const std::string& entity_id,
      EntityType entity_type,
      std::chrono::system_clock::time_point timestamp);

  // Rollback Operations
  std::string submit_rollback_request(const RollbackRequest& request);
  
  bool execute_rollback(const std::string& rollback_id);
  
  bool cancel_rollback(const std::string& rollback_id, const std::string& reason);
  
  RollbackRequest get_rollback_request(const std::string& rollback_id);
  
  std::vector<RollbackRequest> get_pending_rollbacks();
  
  std::vector<std::string> check_rollback_dependencies(const std::string& change_id);

  // Change Batches (for coordinated multi-entity updates)
  std::string create_batch(const ChangeBatch& batch);
  
  bool add_change_to_batch(const std::string& batch_id, const std::string& change_id);
  
  bool execute_batch(const std::string& batch_id);
  
  ChangeBatch get_batch(const std::string& batch_id);

  // Compliance & Audit Reports
  bool add_compliance_evidence(const ComplianceEvidence& evidence);
  
  std::vector<ComplianceEvidence> get_change_evidence(const std::string& change_id);
  
  json generate_audit_report(
      int days = 90,
      EntityType entity_type_filter = EntityType::RULE);
  
  json generate_compliance_certification(int days = 90);
  
  json generate_soc2_report(int days = 365);

  // Analytics
  struct AuditStats {
    int total_changes;
    int approved_changes;
    int rejected_changes;
    int rolled_back_changes;
    int total_users;
    int total_entities;
    double approval_rate;
    std::vector<std::string> most_active_users;
    std::map<EntityType, int> changes_by_entity_type;
    std::chrono::system_clock::time_point calculated_at;
  };

  AuditStats get_audit_statistics(int days = 30);

  // Search & Discovery
  std::vector<ChangeRecord> search_changes(
      const std::string& search_term,
      int days = 90);
  
  std::vector<ChangeRecord> get_related_changes(
      const std::string& change_id,
      int max_depth = 3);

  // Database operations
  bool initialize_database();
  bool save_to_database();
  bool load_from_database();

private:
  std::vector<ChangeRecord> changes_;
  std::map<std::string, EntitySnapshot> snapshots_;
  std::vector<RollbackRequest> rollback_requests_;
  std::vector<ChangeBatch> change_batches_;
  std::vector<ComplianceEvidence> compliance_evidence_;
  
  std::mutex audit_lock_;

  // Internal helpers
  json calculate_diff(const json& old_val, const json& new_val);
  bool validate_rollback(const std::string& change_id);
  std::vector<std::string> find_dependent_changes(const std::string& change_id);
  ImpactLevel assess_impact(
      EntityType entity_type,
      ChangeOperation operation,
      const json& changes);
};

}  // namespace security
}  // namespace regulens
