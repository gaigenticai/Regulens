#include "email_tool.hpp"
#include <curl/curl.h>
#include <regex>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>

namespace regulens {

// Email Tool Implementation

EmailTool::EmailTool(const ToolConfig& config, StructuredLogger* logger)
    : Tool(config, logger) {

    // Parse email configuration from tool config
    if (config.connection_config.contains("smtp_server")) {
        email_config_.smtp_server = config.connection_config["smtp_server"];
    }
    if (config.connection_config.contains("smtp_port")) {
        email_config_.smtp_port = config.connection_config["smtp_port"];
    }
    if (config.auth_config.contains("username")) {
        email_config_.username = config.auth_config["username"];
    }
    if (config.auth_config.contains("password")) {
        email_config_.password = config.auth_config["password"];
    }
    if (config.connection_config.contains("use_tls")) {
        email_config_.use_tls = config.connection_config["use_tls"];
    }
    if (config.connection_config.contains("use_ssl")) {
        email_config_.use_ssl = config.connection_config["use_ssl"];
    }

    // Set default from address if available
    if (config.metadata.contains("from_address")) {
        email_config_.from_address = config.metadata["from_address"];
    }
    if (config.metadata.contains("from_name")) {
        email_config_.from_name = config.metadata["from_name"];
    }

    // Load built-in templates
    add_template(EmailTemplates::REGULATORY_ALERT);
    add_template(EmailTemplates::COMPLIANCE_VIOLATION);
    add_template(EmailTemplates::AGENT_DECISION_REVIEW);
}

EmailTool::~EmailTool() {
    disconnect();
}

ToolResult EmailTool::execute_operation(const std::string& operation,
                                      const nlohmann::json& parameters) {
    auto start_time = std::chrono::steady_clock::now();

    try {
        if (operation == "send_email") {
            EmailMessage message;

            if (parameters.contains("to")) message.to_address = parameters["to"];
            if (parameters.contains("cc")) message.cc_address = parameters["cc"];
            if (parameters.contains("bcc")) message.bcc_address = parameters["bcc"];
            if (parameters.contains("subject")) message.subject = parameters["subject"];
            if (parameters.contains("body_html")) message.body_html = parameters["body_html"];
            if (parameters.contains("body_text")) message.body_text = parameters["body_text"];
            if (parameters.contains("priority")) message.priority = parameters["priority"];

            if (message.to_address.empty()) {
                return create_error_result("Recipient address is required", std::chrono::milliseconds(0));
            }

            if (!validate_email_address(message.to_address)) {
                return create_error_result("Invalid recipient email address", std::chrono::milliseconds(0));
            }

            auto result = send_email(message);
            record_operation_result(result);
            return result;

        } else if (operation == "send_template") {
            std::string template_id = parameters.value("template_id", "");
            std::string to_address = parameters.value("to", "");
            auto variables = parameters.value("variables", std::unordered_map<std::string, std::string>());

            if (template_id.empty() || to_address.empty()) {
                return create_error_result("Template ID and recipient address are required", std::chrono::milliseconds(0));
            }

            auto result = send_template_email(template_id, to_address, variables);
            record_operation_result(result);
            return result;

        } else if (operation == "validate_email") {
            std::string email = parameters.value("email", "");
            if (email.empty()) {
                return create_error_result("Email address is required", std::chrono::milliseconds(0));
            }

            bool valid = validate_email_address(email);
            return create_success_result({{"valid", valid}, {"email", email}}, std::chrono::milliseconds(1));

        } else {
            return create_error_result("Unknown operation: " + operation, std::chrono::milliseconds(0));
        }

    } catch (const std::exception& e) {
        auto execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);

        logger_->log(LogLevel::ERROR, "Email tool operation failed: " + std::string(e.what()));
        return create_error_result("Operation failed: " + std::string(e.what()), execution_time);
    }
}

bool EmailTool::authenticate() {
    // SMTP authentication happens during send operations
    // For now, just validate configuration
    if (email_config_.smtp_server.empty()) {
        logger_->log(LogLevel::ERROR, "SMTP server not configured");
        return false;
    }

    authenticated_ = true;
    logger_->log(LogLevel::INFO, "Email tool authentication successful");
    return true;
}

bool EmailTool::disconnect() {
    authenticated_ = false;
    logger_->log(LogLevel::INFO, "Email tool disconnected");
    return true;
}

ToolResult EmailTool::send_email(const EmailMessage& message) {
    if (!check_rate_limit()) {
        return create_error_result("Rate limit exceeded", std::chrono::milliseconds(0));
    }

    return send_email_via_smtp(message);
}

ToolResult EmailTool::send_template_email(const std::string& template_id,
                                        const std::string& to_address,
                                        const std::unordered_map<std::string, std::string>& variables) {
    std::lock_guard<std::mutex> lock(templates_mutex_);

    auto it = templates_.find(template_id);
    if (it == templates_.end()) {
        return create_error_result("Email template not found: " + template_id, std::chrono::milliseconds(0));
    }

    const EmailTemplate& tmpl = it->second;

    EmailMessage message;
    message.to_address = to_address;
    message.subject = process_template(tmpl.subject_template, variables);
    message.body_html = process_template(tmpl.html_template, variables);
    message.body_text = process_template(tmpl.text_template, variables);

    return send_email(message);
}

bool EmailTool::add_template(const EmailTemplate& template_) {
    std::lock_guard<std::mutex> lock(templates_mutex_);
    templates_[template_.template_id] = template_;
    logger_->log(LogLevel::INFO, "Added email template: " + template_.template_id);
    return true;
}

bool EmailTool::remove_template(const std::string& template_id) {
    std::lock_guard<std::mutex> lock(templates_mutex_);

    auto it = templates_.find(template_id);
    if (it == templates_.end()) {
        return false;
    }

    templates_.erase(it);
    logger_->log(LogLevel::INFO, "Removed email template: " + template_id);
    return true;
}

EmailTemplate* EmailTool::get_template(const std::string& template_id) {
    std::lock_guard<std::mutex> lock(templates_mutex_);

    auto it = templates_.find(template_id);
    return it != templates_.end() ? &it->second : nullptr;
}

std::vector<std::string> EmailTool::get_available_templates() const {
    std::lock_guard<std::mutex> lock(templates_mutex_);

    std::vector<std::string> template_ids;
    for (const auto& [id, tmpl] : templates_) {
        template_ids.push_back(id);
    }

    return template_ids;
}

bool EmailTool::validate_email_address(const std::string& email) const {
    if (email.empty()) return false;

    // Basic email validation regex
    const std::regex email_regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    return std::regex_match(email, email_regex);
}

ToolResult EmailTool::send_email_via_smtp(const EmailMessage& message) {
    auto start_time = std::chrono::steady_clock::now();

    try {
        // Initialize libcurl
        CURL* curl = curl_easy_init();
        if (!curl) {
            return create_error_result("Failed to initialize CURL", std::chrono::milliseconds(0));
        }

        // Set up SMTP URL
        std::string smtp_url = "smtp://" + email_config_.smtp_server + ":" +
                              std::to_string(email_config_.smtp_port) + "/";

        // Set CURL options
        curl_easy_setopt(curl, CURLOPT_URL, smtp_url.c_str());
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, email_config_.from_address.c_str());

        // Set recipients
        struct curl_slist* recipients = nullptr;
        recipients = curl_slist_append(recipients, message.to_address.c_str());
        if (!message.cc_address.empty()) {
            recipients = curl_slist_append(recipients, message.cc_address.c_str());
        }
        if (!message.bcc_address.empty()) {
            recipients = curl_slist_append(recipients, message.bcc_address.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        // Set authentication
        if (!email_config_.username.empty()) {
            curl_easy_setopt(curl, CURLOPT_USERNAME, email_config_.username.c_str());
            curl_easy_setopt(curl, CURLOPT_PASSWORD, email_config_.password.c_str());
        }

        // Set SSL/TLS options
        if (email_config_.use_ssl) {
            curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        } else if (email_config_.use_tls) {
            curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        }

        // Set timeouts
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, email_config_.connection_timeout);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, email_config_.send_timeout);

        // Build email payload
        std::string email_payload = build_email_payload(message);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, nullptr);
        curl_easy_setopt(curl, CURLOPT_READDATA, nullptr);

        // Use callback for email data
        struct EmailData {
            std::string payload;
            size_t position;
        };

        EmailData email_data{email_payload, 0};

        curl_easy_setopt(curl, CURLOPT_READFUNCTION, [](char* buffer, size_t size, size_t nitems, void* userdata) -> size_t {
            EmailData* data = static_cast<EmailData*>(userdata);
            size_t remaining = data->payload.size() - data->position;
            size_t to_copy = std::min(size * nitems, remaining);

            if (to_copy > 0) {
                memcpy(buffer, data->payload.c_str() + data->position, to_copy);
                data->position += to_copy;
            }

            return to_copy;
        });

        curl_easy_setopt(curl, CURLOPT_READDATA, &email_data);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE, static_cast<long>(email_payload.size()));

        // Perform the send
        CURLcode res = curl_easy_perform(curl);

        // Clean up
        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);

        auto execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);

        if (res != CURLE_OK) {
            std::string error_msg = "SMTP send failed: " + std::string(curl_easy_strerror(res));
            logger_->log(LogLevel::ERROR, error_msg);
            return create_error_result(error_msg, execution_time);
        }

        logger_->log(LogLevel::INFO, "Email sent successfully to: " + message.to_address);
        return create_success_result({
            {"to", message.to_address},
            {"subject", message.subject},
            {"message_id", generate_message_id()}
        }, execution_time);

    } catch (const std::exception& e) {
        auto execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);

        std::string error_msg = "Email send failed: " + std::string(e.what());
        logger_->log(LogLevel::ERROR, error_msg);
        return create_error_result(error_msg, execution_time);
    }
}

std::string EmailTool::build_email_payload(const EmailMessage& message) {
    std::stringstream payload;

    // Generate message ID
    std::string message_id = generate_message_id();

    // Email headers
    payload << "Message-ID: <" << message_id << ">\r\n";
    payload << "Date: " << get_current_rfc2822_time() << "\r\n";
    payload << "From: " << (email_config_.from_name.empty() ? "" : "\"" + email_config_.from_name + "\" ") <<
               "<" << email_config_.from_address << ">\r\n";
    payload << "To: " << message.to_address << "\r\n";

    if (!message.cc_address.empty()) {
        payload << "Cc: " << message.cc_address << "\r\n";
    }

    if (!message.bcc_address.empty()) {
        payload << "Bcc: " << message.bcc_address << "\r\n";
    }

    payload << "Subject: " << message.subject << "\r\n";

    // Custom headers
    for (const auto& [key, value] : message.headers) {
        payload << key << ": " << value << "\r\n";
    }

    // MIME headers for HTML email
    if (!message.body_html.empty()) {
        payload << "MIME-Version: 1.0\r\n";
        payload << "Content-Type: multipart/alternative; boundary=\"boundary123\"\r\n";
        payload << "\r\n";

        // Plain text part
        if (!message.body_text.empty()) {
            payload << "--boundary123\r\n";
            payload << "Content-Type: text/plain; charset=UTF-8\r\n";
            payload << "Content-Transfer-Encoding: 7bit\r\n";
            payload << "\r\n";
            payload << message.body_text << "\r\n";
        }

        // HTML part
        payload << "--boundary123\r\n";
        payload << "Content-Type: text/html; charset=UTF-8\r\n";
        payload << "Content-Transfer-Encoding: 7bit\r\n";
        payload << "\r\n";
        payload << message.body_html << "\r\n";

        payload << "--boundary123--\r\n";

    } else {
        // Plain text only
        payload << "\r\n";
        payload << message.body_text << "\r\n";
    }

    return payload.str();
}

std::string EmailTool::encode_base64(const std::string& input) {
    static const char* base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    int val = 0;
    int valb = -6;

    for (char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6) {
        encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }

    while (encoded.size() % 4) {
        encoded.push_back('=');
    }

    return encoded;
}

std::string EmailTool::generate_message_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << std::hex;

    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << ".";

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    ss << timestamp;

    ss << "@regulens.local";

    return ss.str();
}

std::string EmailTool::process_template(const std::string& template_str,
                                      const std::unordered_map<std::string, std::string>& variables) {
    std::string result = template_str;

    for (const auto& [key, value] : variables) {
        std::string placeholder = "{{" + key + "}}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }

    return result;
}

bool EmailTool::is_valid_email_format(const std::string& email) const {
    // RFC 5322 compliant email regex (simplified)
    const std::regex email_regex(
        R"([a-zA-Z0-9.!#$%&'*+/=?^_`{|}~-]+@[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(?:\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*)"
    );
    return std::regex_match(email, email_regex);
}

std::string EmailTool::get_current_rfc2822_time() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t_now), "%a, %d %b %Y %H:%M:%S GMT");
    return ss.str();
}

// Factory function
std::unique_ptr<Tool> create_email_tool(const ToolConfig& config, StructuredLogger* logger) {
    return std::make_unique<EmailTool>(config, logger);
}

} // namespace regulens
