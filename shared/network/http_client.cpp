#include "http_client.hpp"
#include "../config/configuration_manager.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <spdlog/spdlog.h>

namespace regulens {

HttpClient::HttpClient()
    : curl_handle_(nullptr), initialized_(false),
      timeout_seconds_(30), user_agent_("Regulens-Agent/1.0"),
      ssl_verify_(true) {
    initialize_curl();
}

HttpClient::~HttpClient() {
    cleanup_curl();
}

HttpClient::HttpClient(HttpClient&& other) noexcept
    : curl_handle_(other.curl_handle_), initialized_(other.initialized_),
      timeout_seconds_(other.timeout_seconds_), user_agent_(std::move(other.user_agent_)),
      ssl_verify_(other.ssl_verify_), proxy_(std::move(other.proxy_)) {
    other.curl_handle_ = nullptr;
    other.initialized_ = false;
}

HttpClient& HttpClient::operator=(HttpClient&& other) noexcept {
    if (this != &other) {
        cleanup_curl();
        curl_handle_ = other.curl_handle_;
        initialized_ = other.initialized_;
        timeout_seconds_ = other.timeout_seconds_;
        user_agent_ = std::move(other.user_agent_);
        ssl_verify_ = other.ssl_verify_;
        proxy_ = std::move(other.proxy_);
        other.curl_handle_ = nullptr;
        other.initialized_ = false;
    }
    return *this;
}

void HttpClient::initialize_curl() {
    if (initialized_) return;

    curl_handle_ = curl_easy_init();
    if (!curl_handle_) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    initialized_ = true;

    // Set default options
    curl_easy_setopt(curl_handle_, CURLOPT_USERAGENT, user_agent_.c_str());
    curl_easy_setopt(curl_handle_, CURLOPT_TIMEOUT, timeout_seconds_);
    curl_easy_setopt(curl_handle_, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle_, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl_handle_, CURLOPT_SSL_VERIFYPEER, ssl_verify_ ? 1L : 0L);
    curl_easy_setopt(curl_handle_, CURLOPT_SSL_VERIFYHOST, ssl_verify_ ? 2L : 0L);
}

void HttpClient::cleanup_curl() {
    if (curl_handle_) {
        curl_easy_cleanup(curl_handle_);
        curl_handle_ = nullptr;
    }
    initialized_ = false;
}

size_t HttpClient::write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* response = static_cast<HttpResponse*>(userp);
    size_t total_size = size * nmemb;
    response->body.append(static_cast<char*>(contents), total_size);
    return total_size;
}

size_t HttpClient::header_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* response = static_cast<HttpResponse*>(userp);
    std::string header_line(static_cast<char*>(contents), size * nmemb);

    // Remove trailing whitespace
    header_line.erase(header_line.find_last_not_of(" \t\r\n") + 1);

    // Parse header
    size_t colon_pos = header_line.find(':');
    if (colon_pos != std::string::npos) {
        std::string header_name = header_line.substr(0, colon_pos);
        std::string header_value = header_line.substr(colon_pos + 1);

        // Trim leading whitespace from value
        header_value.erase(0, header_value.find_first_not_of(" \t"));

        response->headers[header_name] = header_value;
    }

    return size * nmemb;
}

HttpResponse HttpClient::get(const std::string& url,
                           const std::unordered_map<std::string, std::string>& headers) {
    HttpResponse response;

    if (!initialized_) {
        response.error_message = "HTTP client not initialized";
        return response;
    }

    // Reset response
    response.body.clear();
    response.headers.clear();
    response.error_message.clear();

    // Set URL
    curl_easy_setopt(curl_handle_, CURLOPT_URL, url.c_str());

    // Set method to GET
    curl_easy_setopt(curl_handle_, CURLOPT_HTTPGET, 1L);

    // Set callbacks
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl_handle_, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl_handle_, CURLOPT_HEADERDATA, &response);

    // Set custom headers
    struct curl_slist* header_list = nullptr;
    for (const auto& header : headers) {
        std::string header_str = header.first + ": " + header.second;
        header_list = curl_slist_append(header_list, header_str.c_str());
    }

    if (header_list) {
        curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER, header_list);
    }

    // Set proxy if configured
    if (!proxy_.empty()) {
        curl_easy_setopt(curl_handle_, CURLOPT_PROXY, proxy_.c_str());
    }

    // Perform request
    CURLcode res = curl_easy_perform(curl_handle_);

    // Get status code
    long response_code;
    curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &response_code);
    response.status_code = static_cast<int>(response_code);

    // Clean up headers
    if (header_list) {
        curl_slist_free_all(header_list);
    }

    if (res != CURLE_OK) {
        response.error_message = curl_easy_strerror(res);
        response.success = false;
        spdlog::error("HTTP GET failed for {}: {}", url, response.error_message);
    } else {
        response.success = (response.status_code >= 200 && response.status_code < 300);
        spdlog::info("HTTP GET succeeded for {}: {} bytes received", url, response.body.size());
    }

    return response;
}

HttpResponse HttpClient::post(const std::string& url,
                            const std::string& data,
                            const std::unordered_map<std::string, std::string>& headers) {
    HttpResponse response;

    if (!initialized_) {
        response.error_message = "HTTP client not initialized";
        return response;
    }

    // Reset response
    response.body.clear();
    response.headers.clear();
    response.error_message.clear();

    // Set URL
    curl_easy_setopt(curl_handle_, CURLOPT_URL, url.c_str());

    // Set method to POST
    curl_easy_setopt(curl_handle_, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDSIZE, data.size());

    // Set callbacks
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl_handle_, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl_handle_, CURLOPT_HEADERDATA, &response);

    // Set custom headers
    struct curl_slist* header_list = nullptr;
    for (const auto& header : headers) {
        std::string header_str = header.first + ": " + header.second;
        header_list = curl_slist_append(header_list, header_str.c_str());
    }

    if (header_list) {
        curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER, header_list);
    }

    // Set proxy if configured
    if (!proxy_.empty()) {
        curl_easy_setopt(curl_handle_, CURLOPT_PROXY, proxy_.c_str());
    }

    // Perform request
    CURLcode res = curl_easy_perform(curl_handle_);

    // Get status code
    long response_code;
    curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &response_code);
    response.status_code = static_cast<int>(response_code);

    // Clean up headers
    if (header_list) {
        curl_slist_free_all(header_list);
    }

    if (res != CURLE_OK) {
        response.error_message = curl_easy_strerror(res);
        response.success = false;
        spdlog::error("HTTP POST failed for {}: {}", url, response.error_message);
    } else {
        response.success = (response.status_code >= 200 && response.status_code < 300);
        spdlog::info("HTTP POST succeeded for {}: {} bytes received", url, response.body.size());
    }

    return response;
}

void HttpClient::set_timeout(long seconds) {
    timeout_seconds_ = seconds;
    if (initialized_) {
        curl_easy_setopt(curl_handle_, CURLOPT_TIMEOUT, timeout_seconds_);
    }
}

void HttpClient::set_user_agent(const std::string& user_agent) {
    user_agent_ = user_agent;
    if (initialized_) {
        curl_easy_setopt(curl_handle_, CURLOPT_USERAGENT, user_agent_.c_str());
    }
}

void HttpClient::set_ssl_verify(bool verify) {
    ssl_verify_ = verify;
    if (initialized_) {
        curl_easy_setopt(curl_handle_, CURLOPT_SSL_VERIFYPEER, ssl_verify_ ? 1L : 0L);
        curl_easy_setopt(curl_handle_, CURLOPT_SSL_VERIFYHOST, ssl_verify_ ? 2L : 0L);
    }
}

void HttpClient::set_proxy(const std::string& proxy) {
    proxy_ = proxy;
}

// Email client implementation (production-grade)
EmailClient::EmailClient()
    : smtp_port_(587) {
    // Load SMTP configuration from centralized configuration manager
    auto& config_manager = ConfigurationManager::get_instance();
    SMTPConfig smtp_config = config_manager.get_smtp_config();

    smtp_server_ = smtp_config.host;
    smtp_port_ = smtp_config.port;
    smtp_username_ = smtp_config.user;
    smtp_password_ = smtp_config.password;
    from_email_ = smtp_config.from_email;
}

bool EmailClient::send_email(const std::string& to,
                           const std::string& subject,
                           const std::string& body,
                           const std::string& from) {
    try {
        // Production-grade email sending using SMTP with CURL
        spdlog::info("📧 Sending email to {}", to);

        CURL* curl = curl_easy_init();
        if (!curl) {
            spdlog::error("Failed to initialize CURL for email sending");
            return false;
        }

        // Configure SMTP server
        std::string smtp_url = "smtp://" + smtp_server_ + ":" + std::to_string(smtp_port_);
        curl_easy_setopt(curl, CURLOPT_URL, smtp_url.c_str());

        // Authentication
        curl_easy_setopt(curl, CURLOPT_USERNAME, smtp_username_.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, smtp_password_.c_str());

        // SSL/TLS configuration
        curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        // Email configuration
        struct curl_slist* recipients = nullptr;
        recipients = curl_slist_append(recipients, to.c_str());
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        // Use configured from email
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, ("<" + from_email_ + ">").c_str());

        // Prepare email content
        std::string email_data = create_email_content(from, to, subject, body);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, email_read_callback);
        curl_easy_setopt(curl, CURLOPT_READDATA, &email_data);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE, (long)email_data.size());
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        // Send the email
        CURLcode res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            spdlog::error("CURL error sending email: {}", curl_easy_strerror(res));
            curl_slist_free_all(recipients);
            curl_easy_cleanup(curl);
            return false;
        }

        // Get response information
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

        // Cleanup
        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);

        if (response_code >= 200 && response_code < 300) {
            spdlog::info("📧 Email sent successfully to {}", to);
            return true;
        } else {
            spdlog::error("SMTP server returned error code: {}", response_code);
            return false;
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception sending email: {}", e.what());
        return false;
    }
}

std::string EmailClient::create_email_content(const std::string& /*from*/, const std::string& to,
                                             const std::string& subject, const std::string& body) {
    std::stringstream email;

    // Email headers
    email << "From: " << from_email_ << " <" << from_email_ << ">\r\n";
    email << "To: " << to << "\r\n";
    email << "Subject: " << subject << "\r\n";
    email << "MIME-Version: 1.0\r\n";
    email << "Content-Type: text/plain; charset=UTF-8\r\n";
    email << "\r\n"; // Empty line separates headers from body

    // Email body
    email << body << "\r\n";

    return email.str();
}

size_t EmailClient::email_read_callback(void* ptr, size_t size, size_t nmemb, void* userp) {
    std::string* content = static_cast<std::string*>(userp);
    if (!content || content->empty()) {
        return 0;
    }

    size_t bytes_to_copy = std::min(size * nmemb, content->size());
    memcpy(ptr, content->data(), bytes_to_copy);
    *content = content->substr(bytes_to_copy);

    return bytes_to_copy;
}

void EmailClient::configure_smtp(const std::string& smtp_server,
                               int smtp_port,
                               const std::string& username,
                               const std::string& password) {
    smtp_server_ = smtp_server;
    smtp_port_ = smtp_port;
    smtp_username_ = username;
    smtp_password_ = password;
}

} // namespace regulens

