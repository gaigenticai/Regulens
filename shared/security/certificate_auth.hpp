/**
 * Certificate-Based Authentication - Header
 * 
 * Production-grade mTLS (mutual TLS) authentication for API-to-API communication.
 * Supports X.509 certificates, client authentication, and certificate validation.
 */

#ifndef REGULENS_CERTIFICATE_AUTH_HPP
#define REGULENS_CERTIFICATE_AUTH_HPP

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <openssl/ssl.h>
#include <openssl/x509.h>

namespace regulens {

// Certificate authentication result
enum class CertAuthResult {
    SUCCESS,
    CERT_NOT_PROVIDED,
    CERT_INVALID,
    CERT_EXPIRED,
    CERT_REVOKED,
    CERT_NOT_TRUSTED,
    CERT_CN_MISMATCH,
    CERT_USAGE_VIOLATION,
    INTERNAL_ERROR
};

// Client certificate info
struct ClientCertificateInfo {
    std::string subject_dn;                                    // Subject Distinguished Name
    std::string issuer_dn;                                     // Issuer Distinguished Name
    std::string serial_number;                                 // Serial number
    std::string common_name;                                   // Common Name (CN)
    std::vector<std::string> organization;                     // Organization (O)
    std::vector<std::string> organizational_unit;              // Organizational Unit (OU)
    std::string country;                                       // Country (C)
    std::string state;                                         // State/Province (ST)
    std::string locality;                                      // Locality (L)
    std::string email;                                         // Email address
    std::vector<std::string> san_dns;                         // DNS names in SAN
    std::vector<std::string> san_ip;                          // IP addresses in SAN
    std::chrono::system_clock::time_point not_before;         // Validity start
    std::chrono::system_clock::time_point not_after;          // Validity end
    std::string fingerprint_sha256;                           // SHA-256 fingerprint
    int key_length_bits;                                      // Public key length
};

// Certificate authentication context
struct CertAuthContext {
    ClientCertificateInfo client_cert;
    CertAuthResult result;
    std::string error_message;
    std::chrono::system_clock::time_point auth_time;
    std::string client_ip;
    std::map<std::string, std::string> additional_claims;     // Additional extracted claims
};

/**
 * Certificate-Based Authentication Manager
 * 
 * Implements mTLS (mutual TLS) authentication for secure API-to-API communication.
 * Features:
 * - X.509 certificate validation
 * - Certificate chain verification
 * - CRL (Certificate Revocation List) checking
 * - OCSP (Online Certificate Status Protocol) verification
 * - Certificate pinning
 * - Custom certificate policies
 * - Certificate-based authorization
 * - Audit logging of certificate authentication
 */
class CertificateAuthManager {
public:
    /**
     * Constructor
     * 
     * @param ca_cert_path Path to CA certificate(s)
     * @param crl_path Path to Certificate Revocation List
     */
    explicit CertificateAuthManager(
        const std::string& ca_cert_path,
        const std::string& crl_path = ""
    );

    ~CertificateAuthManager();

    /**
     * Initialize certificate authentication
     * 
     * @return true if successful
     */
    bool initialize();

    /**
     * Authenticate client certificate
     * 
     * @param cert Client certificate (X509)
     * @param client_ip Client IP address
     * @return Authentication context
     */
    CertAuthContext authenticate_certificate(X509* cert, const std::string& client_ip);

    /**
     * Authenticate client certificate from PEM
     * 
     * @param cert_pem PEM-encoded certificate
     * @param client_ip Client IP address
     * @return Authentication context
     */
    CertAuthContext authenticate_certificate_pem(
        const std::string& cert_pem,
        const std::string& client_ip
    );

    /**
     * Verify certificate chain
     * 
     * @param cert Certificate to verify
     * @param chain Certificate chain
     * @return true if chain is valid
     */
    bool verify_certificate_chain(X509* cert, STACK_OF(X509)* chain);

    /**
     * Check certificate revocation
     * 
     * @param cert Certificate to check
     * @return true if certificate is not revoked
     */
    bool check_certificate_revocation(X509* cert);

    /**
     * Verify OCSP
     * 
     * @param cert Certificate to verify
     * @return true if certificate is valid per OCSP
     */
    bool verify_ocsp(X509* cert);

    /**
     * Add trusted CA certificate
     * 
     * @param ca_cert_path Path to CA certificate
     * @return true if successful
     */
    bool add_trusted_ca(const std::string& ca_cert_path);

    /**
     * Add pinned certificate
     * Pins a specific certificate by its fingerprint
     * 
     * @param fingerprint_sha256 SHA-256 fingerprint
     * @return true if successful
     */
    bool add_pinned_certificate(const std::string& fingerprint_sha256);

    /**
     * Verify pinned certificate
     * 
     * @param cert Certificate to verify
     * @return true if certificate matches a pinned certificate
     */
    bool verify_pinned_certificate(X509* cert);

    /**
     * Set certificate policy
     * 
     * @param policy_name Policy name
     * @param policy_config Policy configuration
     * @return true if successful
     */
    bool set_certificate_policy(
        const std::string& policy_name,
        const std::map<std::string, std::string>& policy_config
    );

    /**
     * Extract certificate info
     * 
     * @param cert X509 certificate
     * @return Certificate information
     */
    ClientCertificateInfo extract_certificate_info(X509* cert);

    /**
     * Authorize based on certificate
     * Checks if certificate has required permissions
     * 
     * @param cert_info Certificate information
     * @param required_permissions Required permissions
     * @return true if authorized
     */
    bool authorize_certificate(
        const ClientCertificateInfo& cert_info,
        const std::vector<std::string>& required_permissions
    );

    /**
     * Get allowed operations for certificate
     * 
     * @param cert_info Certificate information
     * @return Vector of allowed operations
     */
    std::vector<std::string> get_allowed_operations(const ClientCertificateInfo& cert_info);

    /**
     * Revoke client certificate
     * Adds certificate to local revocation list
     * 
     * @param serial_number Certificate serial number
     * @param reason Revocation reason
     * @return true if successful
     */
    bool revoke_client_certificate(
        const std::string& serial_number,
        const std::string& reason
    );

    /**
     * Update CRL
     * Downloads and updates Certificate Revocation List
     * 
     * @param crl_url URL to download CRL from
     * @return true if successful
     */
    bool update_crl(const std::string& crl_url);

    /**
     * Get authentication statistics
     * 
     * @return JSON statistics
     */
    std::string get_authentication_statistics();

    /**
     * Get authentication audit log
     * 
     * @param start_time Start time for log entries
     * @param end_time End time for log entries
     * @return JSON audit log
     */
    std::string get_audit_log(
        const std::chrono::system_clock::time_point& start_time,
        const std::chrono::system_clock::time_point& end_time
    );

private:
    std::string ca_cert_path_;
    std::string crl_path_;
    bool initialized_;

    X509_STORE* cert_store_;                                   // Certificate store
    std::vector<std::string> pinned_fingerprints_;            // Pinned certificate fingerprints
    std::map<std::string, std::map<std::string, std::string>> cert_policies_;  // Certificate policies
    std::map<std::string, std::vector<std::string>> cert_permissions_;  // Certificate -> permissions mapping

    /**
     * Load CA certificates
     */
    bool load_ca_certificates();

    /**
     * Load CRL
     */
    bool load_crl();

    /**
     * Verify certificate signature
     */
    bool verify_certificate_signature(X509* cert);

    /**
     * Verify certificate validity period
     */
    bool verify_certificate_validity(X509* cert);

    /**
     * Verify certificate usage
     */
    bool verify_certificate_usage(X509* cert);

    /**
     * Calculate certificate fingerprint
     */
    std::string calculate_fingerprint(X509* cert);

    /**
     * Extract X509 name
     */
    std::string extract_x509_name(X509_NAME* name);

    /**
     * Extract SAN (Subject Alternative Name) entries
     */
    void extract_san_entries(
        X509* cert,
        std::vector<std::string>& san_dns,
        std::vector<std::string>& san_ip
    );

    /**
     * Check if certificate is in CRL
     */
    bool is_certificate_in_crl(X509* cert);

    /**
     * Log authentication attempt
     */
    void log_authentication(const CertAuthContext& context);

    /**
     * Apply certificate policy
     */
    bool apply_certificate_policy(
        X509* cert,
        const std::string& policy_name
    );

    /**
     * Parse certificate extension
     */
    std::string parse_certificate_extension(X509* cert, int nid);
};

/**
 * Helper function to create mTLS SSL context
 * 
 * @param server_cert_path Server certificate path
 * @param server_key_path Server private key path
 * @param ca_cert_path CA certificate path
 * @param require_client_cert Whether to require client certificate
 * @return SSL_CTX pointer (caller must free)
 */
SSL_CTX* create_mtls_context(
    const std::string& server_cert_path,
    const std::string& server_key_path,
    const std::string& ca_cert_path,
    bool require_client_cert = true
);

/**
 * Helper function to verify client certificate in SSL connection
 * 
 * @param ssl SSL connection
 * @return Authentication result
 */
CertAuthResult verify_client_certificate(SSL* ssl);

} // namespace regulens

#endif // REGULENS_CERTIFICATE_AUTH_HPP

