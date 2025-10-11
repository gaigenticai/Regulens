/**
 * Financial Compliance - Header
 * 
 * Production-grade financial compliance system for SOX, GDPR, PCI-DSS, and other regulations.
 * Ensures authentication and data handling meet regulatory requirements.
 */

#ifndef REGULENS_FINANCIAL_COMPLIANCE_HPP
#define REGULENS_FINANCIAL_COMPLIANCE_HPP

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <functional>

namespace regulens {

// Compliance standard
enum class ComplianceStandard {
    SOX,           // Sarbanes-Oxley Act
    GDPR,          // General Data Protection Regulation
    PCI_DSS,       // Payment Card Industry Data Security Standard
    HIPAA,         // Health Insurance Portability and Accountability Act
    CCPA,          // California Consumer Privacy Act
    GLBA,          // Gramm-Leach-Bliley Act
    FINRA,         // Financial Industry Regulatory Authority
    MiFID_II,      // Markets in Financial Instruments Directive II
    BASEL_III,     // Basel III Banking Regulations
    DODD_FRANK     // Dodd-Frank Wall Street Reform
};

// Compliance requirement level
enum class ComplianceLevel {
    CRITICAL,      // Must be implemented
    HIGH,          // Should be implemented
    MEDIUM,        // Recommended
    LOW            // Optional
};

// Compliance status
enum class ComplianceStatus {
    COMPLIANT,
    NON_COMPLIANT,
    PARTIALLY_COMPLIANT,
    UNDER_REVIEW,
    NOT_APPLICABLE
};

// Data classification
enum class DataClassification {
    PUBLIC,
    INTERNAL,
    CONFIDENTIAL,
    RESTRICTED,
    PII,           // Personally Identifiable Information
    PHI,           // Protected Health Information
    PCI,           // Payment Card Information
    FINANCIAL      // Financial data
};

// Compliance requirement
struct ComplianceRequirement {
    std::string requirement_id;
    ComplianceStandard standard;
    std::string name;
    std::string description;
    ComplianceLevel level;
    std::vector<std::string> controls;
    std::vector<std::string> evidence_required;
    bool automated_check;
};

// Compliance control
struct ComplianceControl {
    std::string control_id;
    std::string name;
    std::string description;
    std::vector<ComplianceStandard> applicable_standards;
    bool implemented;
    std::string implementation_notes;
    std::chrono::system_clock::time_point last_tested;
    ComplianceStatus status;
};

// Compliance audit entry
struct ComplianceAuditEntry {
    std::string audit_id;
    std::chrono::system_clock::time_point timestamp;
    ComplianceStandard standard;
    std::string user_id;
    std::string action;
    std::string resource;
    std::string outcome;
    std::string ip_address;
    std::map<std::string, std::string> metadata;
    bool compliant;
    std::vector<std::string> violations;
};

// Data retention policy
struct DataRetentionPolicy {
    std::string policy_id;
    DataClassification classification;
    int retention_days;
    bool auto_delete;
    std::vector<ComplianceStandard> required_by;
    std::string deletion_method;  // secure_wipe, anonymize, archive
};

/**
 * Financial Compliance Manager
 * 
 * Comprehensive compliance management for financial regulations.
 * Features:
 * - SOX compliance (segregation of duties, audit trails, controls testing)
 * - GDPR compliance (right to be forgotten, data portability, consent management)
 * - PCI-DSS compliance (encryption, access controls, monitoring)
 * - Automated compliance checking
 * - Audit trail generation
 * - Data classification and handling
 * - Retention policy enforcement
 * - Compliance reporting
 * - Evidence collection and management
 */
class FinancialComplianceManager {
public:
    /**
     * Constructor
     * 
     * @param db_connection Database connection string
     */
    explicit FinancialComplianceManager(const std::string& db_connection = "");

    ~FinancialComplianceManager();

    /**
     * Initialize compliance manager
     * 
     * @return true if successful
     */
    bool initialize();

    /**
     * Check SOX compliance
     * Validates Sarbanes-Oxley requirements
     * 
     * @return Compliance status
     */
    ComplianceStatus check_sox_compliance();

    /**
     * Check GDPR compliance
     * Validates GDPR requirements
     * 
     * @return Compliance status
     */
    ComplianceStatus check_gdpr_compliance();

    /**
     * Check PCI-DSS compliance
     * Validates PCI-DSS requirements
     * 
     * @return Compliance status
     */
    ComplianceStatus check_pci_dss_compliance();

    /**
     * Check all compliance standards
     * 
     * @return Map of standard -> status
     */
    std::map<ComplianceStandard, ComplianceStatus> check_all_compliance();

    /**
     * Log compliance audit entry
     * 
     * @param entry Audit entry
     * @return true if logged
     */
    bool log_audit_entry(const ComplianceAuditEntry& entry);

    /**
     * Validate access control (SOX Requirement)
     * Ensures segregation of duties and least privilege
     * 
     * @param user_id User ID
     * @param resource Resource being accessed
     * @param action Action being performed
     * @return true if access is compliant
     */
    bool validate_access_control(
        const std::string& user_id,
        const std::string& resource,
        const std::string& action
    );

    /**
     * Enforce data retention policy
     * 
     * @param classification Data classification
     * @return Number of records processed
     */
    int enforce_retention_policy(DataClassification classification);

    /**
     * Handle GDPR right to be forgotten
     * 
     * @param user_id User ID
     * @return true if successful
     */
    bool handle_right_to_be_forgotten(const std::string& user_id);

    /**
     * Export user data (GDPR data portability)
     * 
     * @param user_id User ID
     * @param format Export format (json, csv, xml)
     * @return Exported data
     */
    std::string export_user_data(
        const std::string& user_id,
        const std::string& format = "json"
    );

    /**
     * Validate password policy (PCI-DSS Requirement 8.2)
     * 
     * @param password Password to validate
     * @return true if compliant
     */
    bool validate_password_policy(const std::string& password);

    /**
     * Check encryption compliance
     * Validates that sensitive data is encrypted
     * 
     * @param data_type Data classification
     * @return true if encryption is compliant
     */
    bool check_encryption_compliance(DataClassification data_type);

    /**
     * Perform segregation of duties check (SOX)
     * 
     * @param user_id User ID
     * @param requested_role Role being requested
     * @return true if segregation is maintained
     */
    bool check_segregation_of_duties(
        const std::string& user_id,
        const std::string& requested_role
    );

    /**
     * Test internal controls (SOX 404)
     * 
     * @return Test results
     */
    std::vector<std::string> test_internal_controls();

    /**
     * Generate compliance report
     * 
     * @param standard Compliance standard
     * @param start_date Report start date
     * @param end_date Report end date
     * @return JSON compliance report
     */
    std::string generate_compliance_report(
        ComplianceStandard standard,
        const std::chrono::system_clock::time_point& start_date,
        const std::chrono::system_clock::time_point& end_date
    );

    /**
     * Get audit trail
     * 
     * @param user_id Optional user filter
     * @param start_time Start time
     * @param end_time End time
     * @return Vector of audit entries
     */
    std::vector<ComplianceAuditEntry> get_audit_trail(
        const std::string& user_id = "",
        const std::chrono::system_clock::time_point& start_time = std::chrono::system_clock::time_point::min(),
        const std::chrono::system_clock::time_point& end_time = std::chrono::system_clock::now()
    );

    /**
     * Classify data
     * 
     * @param data Data to classify
     * @param context Context information
     * @return Data classification
     */
    DataClassification classify_data(
        const std::string& data,
        const std::map<std::string, std::string>& context = {}
    );

    /**
     * Apply data masking
     * Masks sensitive data according to compliance requirements
     * 
     * @param data Data to mask
     * @param classification Data classification
     * @return Masked data
     */
    std::string apply_data_masking(
        const std::string& data,
        DataClassification classification
    );

    /**
     * Validate session timeout (PCI-DSS 8.1.8)
     * 
     * @param session_idle_time Idle time in seconds
     * @return true if compliant
     */
    bool validate_session_timeout(int session_idle_time);

    /**
     * Check multi-factor authentication (PCI-DSS 8.3)
     * 
     * @param user_id User ID
     * @return true if MFA is enabled
     */
    bool check_mfa_enabled(const std::string& user_id);

    /**
     * Validate consent management (GDPR Article 7)
     * 
     * @param user_id User ID
     * @param purpose Purpose of data processing
     * @return true if consent is valid
     */
    bool validate_consent(
        const std::string& user_id,
        const std::string& purpose
    );

    /**
     * Record consent
     * 
     * @param user_id User ID
     * @param purpose Purpose
     * @param consent_given Whether consent was given
     * @return true if recorded
     */
    bool record_consent(
        const std::string& user_id,
        const std::string& purpose,
        bool consent_given
    );

    /**
     * Check data breach notification requirements
     * 
     * @param breach_type Type of breach
     * @param affected_users Number of affected users
     * @return Notification requirements
     */
    std::vector<std::string> check_breach_notification_requirements(
        const std::string& breach_type,
        int affected_users
    );

    /**
     * Generate evidence package
     * Collects evidence for compliance audits
     * 
     * @param standard Compliance standard
     * @return Evidence package (JSON)
     */
    std::string generate_evidence_package(ComplianceStandard standard);

    /**
     * Get compliance dashboard
     * 
     * @return JSON dashboard data
     */
    std::string get_compliance_dashboard();

    /**
     * Set retention policy
     * 
     * @param policy Retention policy
     * @return true if set
     */
    bool set_retention_policy(const DataRetentionPolicy& policy);

    /**
     * Get applicable standards
     * Returns all applicable compliance standards for the system
     * 
     * @return Vector of applicable standards
     */
    std::vector<ComplianceStandard> get_applicable_standards();

private:
    std::string db_connection_;
    bool initialized_;

    std::map<ComplianceStandard, std::vector<ComplianceRequirement>> requirements_;
    std::map<std::string, ComplianceControl> controls_;
    std::vector<ComplianceAuditEntry> audit_trail_;
    std::map<DataClassification, DataRetentionPolicy> retention_policies_;

    /**
     * Load compliance requirements
     */
    bool load_requirements();

    /**
     * Initialize SOX controls
     */
    void initialize_sox_controls();

    /**
     * Initialize GDPR controls
     */
    void initialize_gdpr_controls();

    /**
     * Initialize PCI-DSS controls
     */
    void initialize_pci_dss_controls();

    /**
     * Check control implementation
     */
    bool check_control_implemented(const std::string& control_id);

    /**
     * Persist audit entry to database
     */
    bool persist_audit_entry(const ComplianceAuditEntry& entry);

    /**
     * Detect PII in data
     */
    bool detect_pii(const std::string& data);

    /**
     * Detect PCI data in data
     */
    bool detect_pci_data(const std::string& data);

    /**
     * Detect financial data
     */
    bool detect_financial_data(const std::string& data);

    /**
     * Anonymize data
     */
    std::string anonymize_data(const std::string& data);

    /**
     * Secure delete data
     */
    bool secure_delete(const std::string& table, const std::string& condition);

    /**
     * Check password strength
     */
    bool check_password_strength(const std::string& password);

    /**
     * Check password history
     */
    bool check_password_history(const std::string& user_id, const std::string& password);

    /**
     * Get user roles
     */
    std::vector<std::string> get_user_roles(const std::string& user_id);

    /**
     * Check conflicting roles
     */
    bool has_conflicting_roles(const std::vector<std::string>& roles);
};

} // namespace regulens

#endif // REGULENS_FINANCIAL_COMPLIANCE_HPP

