/**
 * TLS Configuration Manager - Header
 * 
 * Production-grade TLS/SSL configuration with certificate management.
 * Supports HTTPS, certificate pinning, HSTS, and modern cipher suites.
 */

#ifndef REGULENS_TLS_CONFIGURATION_HPP
#define REGULENS_TLS_CONFIGURATION_HPP

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <openssl/ssl.h>
#include <openssl/x509.h>

namespace regulens {

// TLS version enum
enum class TLSVersion {
    TLS_1_2,
    TLS_1_3,
    TLS_1_2_AND_1_3
};

// Certificate type enum
enum class CertificateType {
    SERVER,            // Server certificate
    CLIENT,            // Client certificate
    CA,               // Certificate Authority
    INTERMEDIATE      // Intermediate CA certificate
};

// Certificate status enum
enum class CertificateStatus {
    VALID,
    EXPIRED,
    REVOKED,
    PENDING_RENEWAL,
    INVALID
};

// Certificate metadata
struct CertificateMetadata {
    std::string certificate_id;
    CertificateType type;
    CertificateStatus status;
    std::string subject;                                    // Certificate subject (DN)
    std::string issuer;                                     // Certificate issuer (DN)
    std::string serial_number;                              // Serial number
    std::chrono::system_clock::time_point valid_from;      // Validity start
    std::chrono::system_clock::time_point valid_to;        // Validity end
    std::vector<std::string> san_entries;                  // Subject Alternative Names
    std::string signature_algorithm;                        // Signature algorithm
    int key_length_bits;                                   // Public key length
    std::string fingerprint_sha256;                        // SHA-256 fingerprint
    std::string file_path;                                 // Path to certificate file
    std::string key_file_path;                            // Path to private key file
};

// TLS configuration
struct TLSConfig {
    TLSVersion min_version;
    TLSVersion max_version;
    std::vector<std::string> cipher_suites;                // Allowed cipher suites
    bool client_auth_required;                             // Require client certificates
    bool enable_hsts;                                      // Enable HSTS
    int hsts_max_age_seconds;                             // HSTS max-age
    bool hsts_include_subdomains;                         // Include subdomains in HSTS
    bool hsts_preload;                                    // HSTS preload
    bool enable_cert_pinning;                             // Enable certificate pinning
    std::vector<std::string> pinned_cert_hashes;          // Pinned certificate SHA-256 hashes
    bool enable_ocsp_stapling;                            // Enable OCSP stapling
    bool verify_client_cert;                              // Verify client certificates
    std::string ca_cert_path;                             // CA certificate path
    std::string cert_revocation_list_path;                // CRL path
};

/**
 * TLS Configuration Manager
 * 
 * Manages TLS/SSL configuration for secure communication.
 * Features:
 * - TLS 1.2 and 1.3 support
 * - Modern cipher suite configuration
 * - Certificate management and validation
 * - Certificate pinning
 * - HSTS (HTTP Strict Transport Security)
 * - OCSP stapling
 * - Client certificate authentication
 * - Certificate rotation and renewal
 * - CRL (Certificate Revocation List) checking
 */
class TLSConfigurationManager {
public:
    /**
     * Constructor
     * 
     * @param config_path Path to TLS configuration directory
     */
    explicit TLSConfigurationManager(const std::string& config_path);

    ~TLSConfigurationManager();

    /**
     * Initialize TLS configuration
     * 
     * @return true if successful
     */
    bool initialize();

    /**
     * Load TLS configuration
     * 
     * @param config TLS configuration
     * @return true if successful
     */
    bool load_configuration(const TLSConfig& config);

    /**
     * Load certificate
     * 
     * @param cert_file_path Path to certificate file
     * @param key_file_path Path to private key file
     * @param type Certificate type
     * @return Certificate ID if successful
     */
    std::string load_certificate(
        const std::string& cert_file_path,
        const std::string& key_file_path,
        CertificateType type
    );

    /**
     * Generate self-signed certificate
     * 
     * @param subject Certificate subject
     * @param san_entries Subject Alternative Names
     * @param validity_days Validity period in days
     * @param key_length_bits Key length (2048 or 4096)
     * @return Certificate ID if successful
     */
    std::string generate_self_signed_certificate(
        const std::string& subject,
        const std::vector<std::string>& san_entries,
        int validity_days = 365,
        int key_length_bits = 4096
    );

    /**
     * Generate Certificate Signing Request (CSR)
     * 
     * @param subject Certificate subject
     * @param san_entries Subject Alternative Names
     * @param key_length_bits Key length
     * @param csr_output Output buffer for CSR
     * @return true if successful
     */
    bool generate_csr(
        const std::string& subject,
        const std::vector<std::string>& san_entries,
        int key_length_bits,
        std::string& csr_output
    );

    /**
     * Verify certificate
     * 
     * @param cert_id Certificate ID
     * @return true if valid
     */
    bool verify_certificate(const std::string& cert_id);

    /**
     * Check certificate expiration
     * 
     * @param cert_id Certificate ID
     * @param days_threshold Threshold in days for renewal warning
     * @return true if certificate expires within threshold
     */
    bool is_certificate_expiring(const std::string& cert_id, int days_threshold = 30);

    /**
     * Renew certificate
     * 
     * @param cert_id Certificate ID to renew
     * @param validity_days New validity period
     * @return New certificate ID
     */
    std::string renew_certificate(const std::string& cert_id, int validity_days = 365);

    /**
     * Revoke certificate
     * 
     * @param cert_id Certificate ID
     * @param reason Revocation reason
     * @return true if successful
     */
    bool revoke_certificate(const std::string& cert_id, const std::string& reason);

    /**
     * Get certificate metadata
     * 
     * @param cert_id Certificate ID
     * @return Certificate metadata
     */
    CertificateMetadata get_certificate_metadata(const std::string& cert_id);

    /**
     * List all certificates
     * 
     * @param type Optional filter by type
     * @param status Optional filter by status
     * @return Vector of certificate metadata
     */
    std::vector<CertificateMetadata> list_certificates(
        CertificateType type = CertificateType::SERVER,
        CertificateStatus status = CertificateStatus::VALID
    );

    /**
     * Create SSL context
     * 
     * @param cert_id Certificate to use
     * @return SSL_CTX pointer (caller must free)
     */
    SSL_CTX* create_ssl_context(const std::string& cert_id);

    /**
     * Configure cipher suites
     * 
     * @param cipher_list Colon-separated list of cipher suites
     * @return true if successful
     */
    bool configure_cipher_suites(const std::string& cipher_list);

    /**
     * Get recommended cipher suites
     * Returns modern, secure cipher suites
     * 
     * @param version TLS version
     * @return Cipher suite list
     */
    static std::string get_recommended_cipher_suites(TLSVersion version);

    /**
     * Validate TLS handshake
     * 
     * @param ssl SSL connection
     * @return true if handshake valid
     */
    bool validate_tls_handshake(SSL* ssl);

    /**
     * Pin certificate
     * 
     * @param cert_id Certificate to pin
     * @return true if successful
     */
    bool pin_certificate(const std::string& cert_id);

    /**
     * Verify pinned certificate
     * 
     * @param cert Certificate to verify
     * @return true if matches pinned certificate
     */
    bool verify_pinned_certificate(X509* cert);

    /**
     * Enable HSTS
     * 
     * @param max_age_seconds Max age in seconds
     * @param include_subdomains Include subdomains
     * @param preload HSTS preload
     * @return true if successful
     */
    bool enable_hsts(
        int max_age_seconds = 31536000,  // 1 year
        bool include_subdomains = true,
        bool preload = false
    );

    /**
     * Get HSTS header
     * 
     * @return HSTS header value
     */
    std::string get_hsts_header() const;

    /**
     * Check OCSP status
     * 
     * @param cert_id Certificate ID
     * @return true if certificate is valid per OCSP
     */
    bool check_ocsp_status(const std::string& cert_id);

    /**
     * Update Certificate Revocation List
     * 
     * @param crl_url URL to download CRL from
     * @return true if successful
     */
    bool update_crl(const std::string& crl_url);

    /**
     * Export certificate
     * 
     * @param cert_id Certificate ID
     * @param format Format ("PEM" or "DER")
     * @param output Output buffer
     * @return true if successful
     */
    bool export_certificate(
        const std::string& cert_id,
        const std::string& format,
        std::string& output
    );

    /**
     * Import certificate
     * 
     * @param cert_data Certificate data
     * @param format Format ("PEM" or "DER")
     * @param type Certificate type
     * @return Certificate ID if successful
     */
    std::string import_certificate(
        const std::string& cert_data,
        const std::string& format,
        CertificateType type
    );

    /**
     * Get TLS statistics
     * 
     * @return JSON statistics
     */
    std::string get_statistics();

private:
    std::string config_path_;
    TLSConfig config_;
    bool initialized_;

    std::map<std::string, CertificateMetadata> certificates_;
    std::vector<std::string> pinned_hashes_;
    SSL_CTX* ssl_context_;

    /**
     * Load OpenSSL library
     */
    bool load_openssl();

    /**
     * Parse certificate
     */
    CertificateMetadata parse_certificate(X509* cert);

    /**
     * Calculate certificate fingerprint
     */
    std::string calculate_fingerprint(X509* cert);

    /**
     * Verify certificate chain
     */
    bool verify_certificate_chain(X509* cert, STACK_OF(X509)* chain);

    /**
     * Check certificate revocation
     */
    bool check_certificate_revocation(X509* cert);

    /**
     * Generate certificate ID
     */
    std::string generate_certificate_id();

    /**
     * Save certificate to file
     */
    bool save_certificate_to_file(
        X509* cert,
        EVP_PKEY* key,
        const std::string& cert_path,
        const std::string& key_path
    );

    /**
     * Load certificate from file
     */
    X509* load_certificate_from_file(const std::string& cert_path);

    /**
     * Load private key from file
     */
    EVP_PKEY* load_private_key_from_file(const std::string& key_path);
};

} // namespace regulens

#endif // REGULENS_TLS_CONFIGURATION_HPP

