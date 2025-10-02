#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace regulens {

/**
 * @brief HTTP response structure
 */
struct HttpResponse {
    int status_code;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    std::string error_message;
    bool success;

    HttpResponse() : status_code(0), success(false) {}
};

/**
 * @brief Production-grade HTTP client for agent communications
 *
 * Supports HTTPS, custom headers, timeouts, and proper error handling.
 * Used by agents to connect to real regulatory websites and APIs.
 */
class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Delete copy operations
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    // Allow move operations
    HttpClient(HttpClient&&) noexcept;
    HttpClient& operator=(HttpClient&&) noexcept;

    /**
     * @brief Perform GET request
     * @param url Target URL
     * @param headers Optional custom headers
     * @return HTTP response
     */
    HttpResponse get(const std::string& url,
                    const std::unordered_map<std::string, std::string>& headers = {});

    /**
     * @brief Perform POST request
     * @param url Target URL
     * @param data POST data
     * @param headers Optional custom headers
     * @return HTTP response
     */
    HttpResponse post(const std::string& url,
                     const std::string& data,
                     const std::unordered_map<std::string, std::string>& headers = {});

    /**
     * @brief Set connection timeout
     * @param seconds Timeout in seconds
     */
    void set_timeout(long seconds);

    /**
     * @brief Set user agent
     * @param user_agent User agent string
     */
    void set_user_agent(const std::string& user_agent);

    /**
     * @brief Enable/disable SSL verification
     * @param verify True to verify SSL certificates
     */
    void set_ssl_verify(bool verify);

    /**
     * @brief Set proxy settings
     * @param proxy Proxy URL
     */
    void set_proxy(const std::string& proxy);

private:
    /**
     * @brief Initialize CURL handle
     */
    void initialize_curl();

    /**
     * @brief Cleanup CURL handle
     */
    void cleanup_curl();

    /**
     * @brief CURL write callback
     */
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);

    /**
     * @brief CURL header callback
     */
    static size_t header_callback(void* contents, size_t size, size_t nmemb, void* userp);

    CURL* curl_handle_;
    bool initialized_;
    long timeout_seconds_;
    std::string user_agent_;
    bool ssl_verify_;
    std::string proxy_;
};

/**
 * @brief Email client for agent notifications
 */
class EmailClient {
public:
    EmailClient();
    ~EmailClient() = default;

    /**
     * @brief Send email notification
     * @param to Recipient email
     * @param subject Email subject
     * @param body Email body
     * @param from Sender email (optional)
     * @return true if sent successfully
     */
    bool send_email(const std::string& to,
                   const std::string& subject,
                   const std::string& body,
                   const std::string& from = "regulens@gaigentic.ai");

    /**
     * @brief Configure SMTP settings
     * @param smtp_server SMTP server
     * @param smtp_port SMTP port
     * @param username SMTP username
     * @param password SMTP password
     */
    void configure_smtp(const std::string& smtp_server,
                       int smtp_port,
                       const std::string& username,
                       const std::string& password);

private:
    // Email content creation and callback helpers
    std::string create_email_content(const std::string& from, const std::string& to,
                                   const std::string& subject, const std::string& body);
    static size_t email_read_callback(void* ptr, size_t size, size_t nmemb, void* userp);
    std::string smtp_server_;
    int smtp_port_;
    std::string smtp_username_;
    std::string smtp_password_;
    std::string from_email_;
};

} // namespace regulens

