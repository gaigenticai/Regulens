/**
 * Granular RBAC System - Phase 7B
 * Feature-level and data-level access control with approval workflows
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <nlohmann/json.hpp>
#include <chrono>

namespace regulens {
namespace security {

using json = nlohmann::json;

// Resource types
enum class ResourceType {
  RULE,
  DECISION,
  ANALYTICS,
  POLICY,
  ALERT,
  USER_MANAGEMENT,
  AUDIT_LOG,
  SYSTEM_CONFIG
};

// Action types
enum class Action {
  CREATE,
  READ,
  UPDATE,
  DELETE,
  EXECUTE,
  APPROVE,
  REJECT,
  EXPORT
};

// Permission level
enum class PermissionLevel {
  DENY = 0,
  READ_ONLY = 1,
  MODIFY = 2,
  ADMIN = 3
};

// Approval requirement
enum class ApprovalLevel {
  NONE = 0,
  MANAGER = 1,
  DIRECTOR = 2,
  EXECUTIVE = 3,
  COMPLIANCE = 4
};

// Role definition with hierarchy
struct Role {
  std::string role_id;
  std::string role_name;
  std::string description;
  int hierarchy_level = 0;  // 0 = viewer, 10 = admin
  std::vector<std::string> feature_permissions;  // e.g., "rule_creation", "decision_analysis"
  std::map<std::string, std::string> data_classification_access;  // data_type -> access_level
  bool can_approve_decisions = false;
  bool can_modify_policies = false;
  bool can_audit_logs = false;
  std::chrono::system_clock::time_point created_at;
};

// User role assignment
struct UserRole {
  std::string user_id;
  std::string role_id;
  std::string assigned_by;
  std::string assignment_reason;
  std::chrono::system_clock::time_point assigned_at;
  std::chrono::system_clock::time_point expires_at;  // Temporary assignments
  bool is_active = true;
};

// Feature permission mapping
struct FeaturePermission {
  std::string feature_name;
  std::vector<Action> required_actions;
  PermissionLevel minimum_level;
  ApprovalLevel requires_approval = ApprovalLevel::NONE;
  std::vector<std::string> prerequisite_features;  // Features that must be accessible first
  bool requires_audit_log = true;
};

// Data classification (for data-level access)
struct DataClassification {
  std::string data_id;
  std::string data_type;
  std::string classification_level;  // PUBLIC, INTERNAL, CONFIDENTIAL, RESTRICTED
  std::set<std::string> authorized_roles;  // Only these roles can access
  std::set<std::string> authorized_users;  // User-specific overrides
  bool requires_export_approval = false;
  std::chrono::system_clock::time_point classified_at;
};

// Approval workflow record
struct ApprovalRequest {
  std::string request_id;
  std::string requested_by;
  std::string action_type;  // "rule_modification", "decision_override", etc.
  std::string resource_id;
  json request_details;
  std::string status;  // PENDING, APPROVED, REJECTED
  std::vector<std::string> approval_chain;  // Required approvers in order
  int current_approver_index = 0;
  json approval_comments;
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point resolved_at;
};

// Access audit trail
struct AccessAuditRecord {
  std::string audit_id;
  std::string user_id;
  std::string action;
  std::string resource_type;
  std::string resource_id;
  bool was_allowed = true;
  std::string denial_reason;
  json context;
  std::chrono::system_clock::time_point accessed_at;
  std::string ip_address;
};

class GranularRBACEngine {
public:
  GranularRBACEngine();
  ~GranularRBACEngine();

  // Role Management
  bool create_role(const Role& role);
  bool update_role(const std::string& role_id, const Role& updated_role);
  bool delete_role(const std::string& role_id);
  Role get_role(const std::string& role_id);
  std::vector<Role> get_all_roles();

  // User Role Assignment
  bool assign_user_role(const UserRole& assignment);
  bool revoke_user_role(const std::string& user_id, const std::string& role_id);
  bool update_user_role_expiry(const std::string& user_id, const std::string& role_id, 
                               std::chrono::system_clock::time_point new_expiry);
  std::vector<UserRole> get_user_roles(const std::string& user_id);
  std::vector<std::string> get_user_active_roles(const std::string& user_id);

  // Feature Permissions
  bool register_feature_permission(const FeaturePermission& feature);
  FeaturePermission get_feature_permission(const std::string& feature_name);
  bool can_access_feature(const std::string& user_id, const std::string& feature_name, Action action);

  // Data Classification
  bool classify_data(const DataClassification& classification);
  DataClassification get_data_classification(const std::string& data_id);
  bool can_access_data(const std::string& user_id, const std::string& data_id);
  bool can_export_data(const std::string& user_id, const std::string& data_id);

  // Access Control Decision
  struct AccessDecision {
    bool allowed = false;
    ApprovalLevel required_approval = ApprovalLevel::NONE;
    std::string denial_reason;
    std::vector<std::string> required_approvers;
  };

  AccessDecision check_access(
      const std::string& user_id,
      const std::string& resource_id,
      ResourceType resource_type,
      Action action,
      const json& context = json());

  // Approval Workflows
  std::string submit_approval_request(const ApprovalRequest& request);
  bool approve_request(const std::string& request_id, const std::string& approver_id, 
                      const std::string& comments = "");
  bool reject_request(const std::string& request_id, const std::string& rejector_id, 
                     const std::string& reason);
  ApprovalRequest get_approval_request(const std::string& request_id);
  std::vector<ApprovalRequest> get_pending_approvals_for_user(const std::string& user_id);

  // Audit Trail
  bool log_access(const AccessAuditRecord& record);
  std::vector<AccessAuditRecord> get_audit_trail(
      const std::string& user_id = "",
      const std::string& resource_type = "",
      int days = 30);
  json generate_compliance_report(int days = 90);

  // Delegation (temporary permission grants)
  bool delegate_permission(
      const std::string& from_user_id,
      const std::string& to_user_id,
      const std::string& feature_name,
      int duration_hours = 4);

  bool revoke_delegation(
      const std::string& delegation_id);

  // Statistics
  struct RBACStats {
    int total_users;
    int total_roles;
    int total_active_assignments;
    int pending_approvals;
    int audit_records_30days;
    double access_denial_rate;
    std::chrono::system_clock::time_point calculated_at;
  };

  RBACStats get_rbac_statistics();

  // Database operations
  bool initialize_database();
  bool save_to_database();
  bool load_from_database();

private:
  std::map<std::string, Role> roles_;
  std::vector<UserRole> user_role_assignments_;
  std::map<std::string, FeaturePermission> feature_permissions_;
  std::map<std::string, DataClassification> data_classifications_;
  std::vector<ApprovalRequest> approval_requests_;
  std::vector<AccessAuditRecord> audit_records_;

  std::mutex rbac_lock_;

  // Internal helpers
  PermissionLevel get_user_permission_level(const std::string& user_id, ResourceType resource_type);
  std::vector<std::string> resolve_approval_chain(ApprovalLevel required_level);
  bool is_role_active(const UserRole& assignment);
  json build_access_context(
      const std::string& user_id,
      const std::string& resource_id,
      const json& additional_context);
};

}  // namespace security
}  // namespace regulens
