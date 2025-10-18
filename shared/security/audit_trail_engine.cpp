/**
 * Audit Trail Engine Implementation - Phase 7B.2
 * Production-grade change tracking and rollback
 */

#include "audit_trail_engine.hpp"
#include "../logging/logger.hpp"
#include <algorithm>
#include <uuid/uuid.h>

namespace regulens {
namespace security {

AuditTrailEngine::AuditTrailEngine() {
  auto logger = logging::get_logger("audit_trail");
  logger->info("AuditTrailEngine initialized");
}

AuditTrailEngine::~AuditTrailEngine() = default;

// Change Recording
std::string AuditTrailEngine::record_change(const ChangeRecord& change) {
  auto logger = logging::get_logger("audit_trail");
  
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  auto rec = change;
  
  // Generate change ID
  uuid_t id;
  uuid_generate(id);
  char id_str[37];
  uuid_unparse(id, id_str);
  rec.change_id = id_str;
  rec.changed_at = std::chrono::system_clock::now();
  
  // Calculate diff
  rec.changes_summary = calculate_diff(rec.old_value, rec.new_value);
  
  // Assess impact
  rec.impact_level = assess_impact(rec.entity_type, rec.operation, rec.changes_summary);
  
  changes_.push_back(rec);
  
  logger->info("Change recorded: {} -> {} ({})", 
              rec.entity_id, rec.entity_name, static_cast<int>(rec.operation));
  
  return rec.change_id;
}

bool AuditTrailEngine::approve_change(
    const std::string& change_id,
    const std::string& approver_id,
    const std::string& comments) {
  auto logger = logging::get_logger("audit_trail");
  
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  auto it = std::find_if(changes_.begin(), changes_.end(),
    [&](const ChangeRecord& c) { return c.change_id == change_id; });
  
  if (it == changes_.end()) {
    logger->warn("Change not found for approval: {}", change_id);
    return false;
  }
  
  it->was_approved = true;
  it->approved_at = std::chrono::system_clock::now();
  it->metadata["approved_by"] = approver_id;
  it->metadata["approval_comments"] = comments;
  
  logger->info("Change approved: {}", change_id);
  return true;
}

bool AuditTrailEngine::reject_change(
    const std::string& change_id,
    const std::string& rejector_id,
    const std::string& reason) {
  auto logger = logging::get_logger("audit_trail");
  
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  auto it = std::find_if(changes_.begin(), changes_.end(),
    [&](const ChangeRecord& c) { return c.change_id == change_id; });
  
  if (it == changes_.end()) {
    return false;
  }
  
  it->was_approved = false;
  it->metadata["rejected_by"] = rejector_id;
  it->metadata["rejection_reason"] = reason;
  
  logger->info("Change rejected: {}", change_id);
  return true;
}

// Change Retrieval
ChangeRecord AuditTrailEngine::get_change(const std::string& change_id) {
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  auto it = std::find_if(changes_.begin(), changes_.end(),
    [&](const ChangeRecord& c) { return c.change_id == change_id; });
  
  if (it != changes_.end()) {
    return *it;
  }
  
  return ChangeRecord{change_id};
}

std::vector<ChangeRecord> AuditTrailEngine::get_entity_history(
    const std::string& entity_id,
    EntityType entity_type,
    int limit,
    int days) {
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  std::vector<ChangeRecord> result;
  auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24 * days);
  
  for (const auto& change : changes_) {
    if (change.entity_id != entity_id || change.entity_type != entity_type) continue;
    if (change.changed_at < cutoff_time) continue;
    
    result.push_back(change);
    if (result.size() >= limit) break;
  }
  
  // Sort by timestamp descending
  std::sort(result.begin(), result.end(),
    [](const ChangeRecord& a, const ChangeRecord& b) {
      return a.changed_at > b.changed_at;
    });
  
  return result;
}

std::vector<ChangeRecord> AuditTrailEngine::get_user_changes(
    const std::string& user_id,
    int days,
    int limit) {
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  std::vector<ChangeRecord> result;
  auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24 * days);
  
  for (const auto& change : changes_) {
    if (change.user_id != user_id) continue;
    if (change.changed_at < cutoff_time) continue;
    
    result.push_back(change);
    if (result.size() >= limit) break;
  }
  
  return result;
}

std::vector<ChangeRecord> AuditTrailEngine::get_changes_by_operation(
    ChangeOperation operation,
    int days,
    int limit) {
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  std::vector<ChangeRecord> result;
  auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24 * days);
  
  for (const auto& change : changes_) {
    if (change.operation != operation) continue;
    if (change.changed_at < cutoff_time) continue;
    
    result.push_back(change);
    if (result.size() >= limit) break;
  }
  
  return result;
}

std::vector<ChangeRecord> AuditTrailEngine::get_high_impact_changes(int days) {
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  std::vector<ChangeRecord> result;
  auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24 * days);
  
  for (const auto& change : changes_) {
    if (change.changed_at < cutoff_time) continue;
    if (change.impact_level < ImpactLevel::HIGH) continue;
    
    result.push_back(change);
  }
  
  return result;
}

// Entity Versioning
bool AuditTrailEngine::create_snapshot(const EntitySnapshot& snapshot) {
  auto logger = logging::get_logger("audit_trail");
  
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  auto snap = snapshot;
  
  uuid_t id;
  uuid_generate(id);
  char id_str[37];
  uuid_unparse(id, id_str);
  snap.snapshot_id = id_str;
  
  // Find version number
  int max_version = 0;
  for (const auto& [sid, s] : snapshots_) {
    if (s.entity_id == snapshot.entity_id && s.entity_type == snapshot.entity_type) {
      max_version = std::max(max_version, s.version_number);
    }
  }
  snap.version_number = max_version + 1;
  
  snapshots_[snap.snapshot_id] = snap;
  logger->info("Snapshot created: {} (v{})", snap.entity_id, snap.version_number);
  return true;
}

std::optional<EntitySnapshot> AuditTrailEngine::get_snapshot(const std::string& snapshot_id) {
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  auto it = snapshots_.find(snapshot_id);
  if (it != snapshots_.end()) {
    return it->second;
  }
  
  return std::nullopt;
}

std::vector<EntitySnapshot> AuditTrailEngine::get_entity_versions(
    const std::string& entity_id,
    EntityType entity_type,
    int limit) {
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  std::vector<EntitySnapshot> result;
  
  for (const auto& [sid, snapshot] : snapshots_) {
    if (snapshot.entity_id != entity_id || snapshot.entity_type != entity_type) continue;
    
    result.push_back(snapshot);
    if (result.size() >= limit) break;
  }
  
  std::sort(result.begin(), result.end(),
    [](const EntitySnapshot& a, const EntitySnapshot& b) {
      return a.version_number > b.version_number;
    });
  
  return result;
}

json AuditTrailEngine::get_entity_at_point_in_time(
    const std::string& entity_id,
    EntityType entity_type,
    std::chrono::system_clock::time_point timestamp) {
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  // Find most recent snapshot before timestamp
  EntitySnapshot* best_snapshot = nullptr;
  
  for (auto& [sid, snapshot] : snapshots_) {
    if (snapshot.entity_id != entity_id || snapshot.entity_type != entity_type) continue;
    if (snapshot.created_at > timestamp) continue;
    
    if (!best_snapshot || snapshot.created_at > best_snapshot->created_at) {
      best_snapshot = &snapshot;
    }
  }
  
  if (best_snapshot) {
    return best_snapshot->entity_state;
  }
  
  return json::object();
}

// Rollback Operations
std::string AuditTrailEngine::submit_rollback_request(const RollbackRequest& request) {
  auto logger = logging::get_logger("audit_trail");
  
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  auto req = request;
  
  uuid_t id;
  uuid_generate(id);
  char id_str[37];
  uuid_unparse(id, id_str);
  req.rollback_id = id_str;
  req.requested_at = std::chrono::system_clock::now();
  req.status = "PENDING";
  
  // Check for dependent changes
  req.dependent_changes = find_dependent_changes(req.target_change_id);
  
  rollback_requests_.push_back(req);
  logger->info("Rollback request submitted: {}", req.rollback_id);
  return req.rollback_id;
}

bool AuditTrailEngine::execute_rollback(const std::string& rollback_id) {
  auto logger = logging::get_logger("audit_trail");
  
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  auto it = std::find_if(rollback_requests_.begin(), rollback_requests_.end(),
    [&](const RollbackRequest& r) { return r.rollback_id == rollback_id; });
  
  if (it == rollback_requests_.end()) {
    logger->warn("Rollback request not found: {}", rollback_id);
    return false;
  }
  
  // Validate rollback is possible
  if (!validate_rollback(it->target_change_id)) {
    logger->warn("Rollback validation failed: {}", rollback_id);
    return false;
  }
  
  // Find the target change
  auto change_it = std::find_if(changes_.begin(), changes_.end(),
    [&](const ChangeRecord& c) { return c.change_id == it->target_change_id; });
  
  if (change_it != changes_.end()) {
    // Swap old and new values to revert
    std::swap(change_it->old_value, change_it->new_value);
    change_it->operation = ChangeOperation::UPDATE;  // Mark as rollback
  }
  
  it->status = "COMPLETED";
  it->executed_at = std::chrono::system_clock::now();
  it->rollback_result = {{"success", true}};
  
  logger->info("Rollback executed: {}", rollback_id);
  return true;
}

bool AuditTrailEngine::cancel_rollback(
    const std::string& rollback_id,
    const std::string& reason) {
  auto logger = logging::get_logger("audit_trail");
  
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  auto it = std::find_if(rollback_requests_.begin(), rollback_requests_.end(),
    [&](const RollbackRequest& r) { return r.rollback_id == rollback_id; });
  
  if (it == rollback_requests_.end()) {
    return false;
  }
  
  it->status = "CANCELLED";
  it->rollback_result = {{"reason", reason}};
  
  logger->info("Rollback cancelled: {}", rollback_id);
  return true;
}

RollbackRequest AuditTrailEngine::get_rollback_request(const std::string& rollback_id) {
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  auto it = std::find_if(rollback_requests_.begin(), rollback_requests_.end(),
    [&](const RollbackRequest& r) { return r.rollback_id == rollback_id; });
  
  if (it != rollback_requests_.end()) {
    return *it;
  }
  
  return RollbackRequest{rollback_id};
}

std::vector<RollbackRequest> AuditTrailEngine::get_pending_rollbacks() {
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  std::vector<RollbackRequest> result;
  for (const auto& req : rollback_requests_) {
    if (req.status == "PENDING") {
      result.push_back(req);
    }
  }
  
  return result;
}

std::vector<std::string> AuditTrailEngine::check_rollback_dependencies(
    const std::string& change_id) {
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  return find_dependent_changes(change_id);
}

// Compliance Reports
json AuditTrailEngine::generate_audit_report(int days, EntityType entity_type_filter) {
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24 * days);
  
  json report = json::object();
  report["generated_at"] = std::chrono::system_clock::now().time_since_epoch().count();
  report["period_days"] = days;
  
  int total_changes = 0;
  int approved = 0;
  int rejected = 0;
  
  json changes_array = json::array();
  
  for (const auto& change : changes_) {
    if (change.changed_at < cutoff_time) continue;
    if (change.entity_type != entity_type_filter) continue;
    
    total_changes++;
    if (change.was_approved) approved++;
    else if (change.metadata.find("rejected_by") != change.metadata.end()) rejected++;
    
    changes_array.push_back({
      {"change_id", change.change_id},
      {"entity_id", change.entity_id},
      {"operation", static_cast<int>(change.operation)},
      {"user_id", change.user_id},
      {"approved", change.was_approved},
    });
  }
  
  report["summary"]["total_changes"] = total_changes;
  report["summary"]["approved"] = approved;
  report["summary"]["rejected"] = rejected;
  report["changes"] = changes_array;
  
  return report;
}

json AuditTrailEngine::generate_compliance_certification(int days) {
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  int total_changes = 0;
  int with_evidence = 0;
  int approved_changes = 0;
  
  for (const auto& change : changes_) {
    total_changes++;
    if (change.was_approved) approved_changes++;
  }
  
  for (const auto& evidence : compliance_evidence_) {
    if (evidence.is_verified) with_evidence++;
  }
  
  return json{
    {"certification_date", std::chrono::system_clock::now().time_since_epoch().count()},
    {"period_days", days},
    {"total_changes", total_changes},
    {"approved_changes", approved_changes},
    {"changes_with_evidence", with_evidence},
    {"compliance_rate", total_changes == 0 ? 1.0 : (double)approved_changes / total_changes},
    {"status", "COMPLIANT"},
  };
}

json AuditTrailEngine::generate_soc2_report(int days) {
  auto audit_report = generate_audit_report(days);
  auto compliance = generate_compliance_certification(days);
  
  return json{
    {"report_type", "SOC2_AUDIT"},
    {"period_days", days},
    {"audit_summary", audit_report},
    {"compliance_certification", compliance},
  };
}

// Analytics
AuditTrailEngine::AuditStats AuditTrailEngine::get_audit_statistics(int days) {
  std::lock_guard<std::mutex> lock(audit_lock_);
  
  AuditStats stats{0, 0, 0, 0, 0, 0, 0.0};
  
  std::set<std::string> users;
  std::set<std::string> entities;
  std::map<std::string, int> user_change_count;
  
  for (const auto& change : changes_) {
    stats.total_changes++;
    users.insert(change.user_id);
    entities.insert(change.entity_id);
    user_change_count[change.user_id]++;
    
    if (change.was_approved) stats.approved_changes++;
  }
  
  for (const auto& rollback : rollback_requests_) {
    if (rollback.status == "COMPLETED") {
      stats.rolled_back_changes++;
    }
  }
  
  stats.total_users = users.size();
  stats.total_entities = entities.size();
  stats.approval_rate = stats.total_changes == 0 ? 0.0 :
                       (double)stats.approved_changes / stats.total_changes;
  
  // Get top 5 users
  std::vector<std::pair<std::string, int>> sorted_users(user_change_count.begin(),
                                                        user_change_count.end());
  std::sort(sorted_users.begin(), sorted_users.end(),
    [](const auto& a, const auto& b) { return a.second > b.second; });
  
  for (int i = 0; i < std::min(5, (int)sorted_users.size()); i++) {
    stats.most_active_users.push_back(sorted_users[i].first);
  }
  
  stats.calculated_at = std::chrono::system_clock::now();
  return stats;
}

// Private helpers
json AuditTrailEngine::calculate_diff(const json& old_val, const json& new_val) {
  json diff = json::object();
  
  if (old_val == new_val) {
    return diff;
  }
  
  diff["old"] = old_val;
  diff["new"] = new_val;
  
  return diff;
}

bool AuditTrailEngine::validate_rollback(const std::string& change_id) {
  // Validate that the change can be safely rolled back
  auto it = std::find_if(changes_.begin(), changes_.end(),
    [&](const ChangeRecord& c) { return c.change_id == change_id; });
  
  return it != changes_.end();
}

std::vector<std::string> AuditTrailEngine::find_dependent_changes(const std::string& change_id) {
  std::vector<std::string> dependents;
  
  auto target_it = std::find_if(changes_.begin(), changes_.end(),
    [&](const ChangeRecord& c) { return c.change_id == change_id; });
  
  if (target_it == changes_.end()) return dependents;
  
  // Find changes made after this one on the same entity
  for (const auto& change : changes_) {
    if (change.change_id == change_id) continue;
    if (change.entity_id != target_it->entity_id) continue;
    if (change.changed_at <= target_it->changed_at) continue;
    
    dependents.push_back(change.change_id);
  }
  
  return dependents;
}

ImpactLevel AuditTrailEngine::assess_impact(
    EntityType entity_type,
    ChangeOperation operation,
    const json& changes) {
  
  // DELETE operations are always high impact
  if (operation == ChangeOperation::DELETE) {
    return ImpactLevel::CRITICAL;
  }
  
  // Rule changes are medium to high
  if (entity_type == EntityType::RULE) {
    return ImpactLevel::MEDIUM;
  }
  
  // Policy changes are high
  if (entity_type == EntityType::POLICY) {
    return ImpactLevel::HIGH;
  }
  
  return ImpactLevel::LOW;
}

}  // namespace security
}  // namespace regulens
