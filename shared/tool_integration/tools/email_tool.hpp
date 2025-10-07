/**
 * Email Tool - SMTP Email Integration
 *
 * Production-grade email tool for sending notifications, alerts,
 * and reports from the agentic AI system.
 *
 * Features:
 * - SMTP protocol support with TLS/SSL
 * - HTML and plain text email templates
 * - Attachment support
 * - Rate limiting and queuing
 * - Delivery tracking and status
 * - Template management
 */

#pragma once

#include "../tool_interface.hpp"
#include <curl/curl.h>
#include <memory>
#include <string>
#include <vector>
#include <mutex>

namespace regulens {

// Email Configuration
struct EmailConfig {
    std::string smtp_server;
    int smtp_port;
    std::string username;
    std::string password;
    bool use_tls;
    bool use_ssl;
    std::string from_address;
    std::string from_name;
    std::string reply_to;
    int connection_timeout;
    int send_timeout;

    EmailConfig() : smtp_port(587), use_tls(true), use_ssl(false),
                   connection_timeout(30), send_timeout(60) {}
};

// Email Message Structure
struct EmailMessage {
    std::string to_address;
    std::string cc_address;
    std::string bcc_address;
    std::string subject;
    std::string body_html;
    std::string body_text;
    std::vector<std::string> attachments;  // File paths
    std::unordered_map<std::string, std::string> headers;
    int priority;  // 1=High, 3=Normal, 5=Low

    EmailMessage() : priority(3) {}
};

// Email Template
struct EmailTemplate {
    std::string template_id;
    std::string name;
    std::string subject_template;
    std::string html_template;
    std::string text_template;
    std::vector<std::string> required_variables;

    EmailTemplate() = default;
    EmailTemplate(const std::string& id, const std::string& n,
                 const std::string& subject, const std::string& html,
                 const std::string& text)
        : template_id(id), name(n), subject_template(subject),
          html_template(html), text_template(text) {}
};

// Email Tool Implementation
class EmailTool : public Tool {
public:
    EmailTool(const ToolConfig& config, StructuredLogger* logger);
    ~EmailTool() override;

    // Tool interface implementation
    ToolResult execute_operation(const std::string& operation,
                               const nlohmann::json& parameters) override;

    bool authenticate() override;
    bool is_authenticated() const override { return authenticated_; }
    bool disconnect() override;

    // Email-specific operations
    ToolResult send_email(const EmailMessage& message);
    ToolResult send_template_email(const std::string& template_id,
                                 const std::string& to_address,
                                 const std::unordered_map<std::string, std::string>& variables);

    // Template management
    bool add_template(const EmailTemplate& template_);
    bool remove_template(const std::string& template_id);
    EmailTemplate* get_template(const std::string& template_id);
    std::vector<std::string> get_available_templates() const;

    // Email validation
    bool validate_email_address(const std::string& email) const;

private:
    EmailConfig email_config_;
    std::unordered_map<std::string, EmailTemplate> templates_;
    mutable std::mutex templates_mutex_;

private:
    std::string get_current_rfc2822_time() const;

    // SMTP operations
    ToolResult send_email_via_smtp(const EmailMessage& message);
    std::string build_email_payload(const EmailMessage& message);
    std::string encode_base64(const std::string& input);
    std::string generate_message_id();

    // Template processing
    std::string process_template(const std::string& template_str,
                               const std::unordered_map<std::string, std::string>& variables);

    // Email address validation
    bool is_valid_email_format(const std::string& email) const;
    bool validate_local_part(const std::string& local) const;
    bool validate_domain_part(const std::string& domain) const;
    bool validate_dot_atom(const std::string& atom) const;
    bool validate_domain_dot_atom(const std::string& domain) const;
    bool validate_quoted_string(const std::string& quoted) const;
    bool validate_domain_literal(const std::string& literal) const;
    bool validate_ipv4_address(const std::string& ip) const;
    bool validate_ipv6_address(const std::string& ip) const;
    bool is_atext(char c) const;
    bool is_qtext(char c) const;
};

// Email Tool Factory Function
std::unique_ptr<Tool> create_email_tool(const ToolConfig& config, StructuredLogger* logger);

// Pre-built email templates for common use cases
namespace EmailTemplates {

const EmailTemplate REGULATORY_ALERT = {
    "regulatory_alert",
    "Regulatory Change Alert",
    "🚨 REGULATORY CHANGE ALERT: {{regulation_name}}",
    R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Regulatory Change Alert</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .alert-header { background-color: #dc3545; color: white; padding: 15px; border-radius: 5px; }
        .alert-content { background-color: #f8f9fa; padding: 20px; margin: 20px 0; border-left: 4px solid #dc3545; }
        .action-required { background-color: #fff3cd; border: 1px solid #ffeaa7; padding: 15px; margin: 20px 0; }
        .footer { font-size: 12px; color: #6c757d; margin-top: 30px; }
    </style>
</head>
<body>
    <div class="alert-header">
        <h2>🚨 REGULATORY CHANGE ALERT</h2>
        <p><strong>{{regulation_name}}</strong> - Immediate Attention Required</p>
    </div>

    <div class="alert-content">
        <h3>Change Details</h3>
        <p><strong>Effective Date:</strong> {{effective_date}}</p>
        <p><strong>Impact Level:</strong> {{impact_level}}</p>
        <p><strong>Source:</strong> {{source}}</p>

        <h3>Description</h3>
        <p>{{description}}</p>
    </div>

    <div class="action-required">
        <h3>⚠️ Action Required</h3>
        <p>{{action_required}}</p>
        <p><strong>Deadline:</strong> {{deadline}}</p>
    </div>

    <div class="footer">
        <p>This alert was generated by the Regulens AI Compliance System.</p>
        <p>Please review and take appropriate action immediately.</p>
    </div>
</body>
</html>
    )HTML",
    R"TEXT(
REGULATORY CHANGE ALERT: {{regulation_name}}

Change Details:
- Effective Date: {{effective_date}}
- Impact Level: {{impact_level}}
- Source: {{source}}

Description:
{{description}}

ACTION REQUIRED:
{{action_required}}

Deadline: {{deadline}}

This alert was generated by the Regulens AI Compliance System.
Please review and take appropriate action immediately.
    )TEXT"
};

const EmailTemplate COMPLIANCE_VIOLATION = {
    "compliance_violation",
    "Compliance Violation Notification",
    "🚨 COMPLIANCE VIOLATION: {{violation_type}} - {{severity}}",
    R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Compliance Violation Notification</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .violation-header { background-color: #dc3545; color: white; padding: 15px; border-radius: 5px; }
        .violation-content { background-color: #f8f9fa; padding: 20px; margin: 20px 0; border-left: 4px solid #dc3545; }
        .risk-assessment { background-color: #f8d7da; border: 1px solid #f5c6cb; padding: 15px; margin: 20px 0; }
        .footer { font-size: 12px; color: #6c757d; margin-top: 30px; }
    </style>
</head>
<body>
    <div class="violation-header">
        <h2>🚨 COMPLIANCE VIOLATION DETECTED</h2>
        <p><strong>{{violation_type}}</strong> - Severity: <strong>{{severity}}</strong></p>
    </div>

    <div class="violation-content">
        <h3>Violation Details</h3>
        <p><strong>Transaction ID:</strong> {{transaction_id}}</p>
        <p><strong>Timestamp:</strong> {{timestamp}}</p>
        <p><strong>Amount:</strong> {{amount}}</p>

        <h3>Description</h3>
        <p>{{description}}</p>
    </div>

    <div class="risk-assessment">
        <h3>⚠️ Risk Assessment</h3>
        <p><strong>Risk Score:</strong> {{risk_score}}/100</p>
        <p><strong>Potential Impact:</strong> {{potential_impact}}</p>
        <p><strong>Recommended Actions:</strong></p>
        <ul>
            {{recommended_actions}}
        </ul>
    </div>

    <div class="footer">
        <p>This notification was generated by the Regulens AI Compliance System.</p>
        <p>Immediate investigation and remediation is required.</p>
    </div>
</body>
</html>
    )HTML",
    R"TEXT(
COMPLIANCE VIOLATION: {{violation_type}} - {{severity}}

Violation Details:
- Transaction ID: {{transaction_id}}
- Timestamp: {{timestamp}}
- Amount: {{amount}}

Description:
{{description}}

Risk Assessment:
- Risk Score: {{risk_score}}/100
- Potential Impact: {{potential_impact}}

Recommended Actions:
{{recommended_actions}}

This notification was generated by the Regulens AI Compliance System.
Immediate investigation and remediation is required.
    )TEXT"
};

const EmailTemplate AGENT_DECISION_REVIEW = {
    "agent_decision_review",
    "Agent Decision Review Request",
    "🤖 AGENT DECISION REQUIRES HUMAN REVIEW: {{decision_type}}",
    R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Agent Decision Review Request</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .review-header { background-color: #ffc107; color: black; padding: 15px; border-radius: 5px; }
        .decision-content { background-color: #f8f9fa; padding: 20px; margin: 20px 0; border-left: 4px solid #ffc107; }
        .confidence-metrics { background-color: #e9ecef; padding: 15px; margin: 20px 0; }
        .action-buttons { margin: 30px 0; text-align: center; }
        .approve-btn { background-color: #28a745; color: white; padding: 10px 20px; text-decoration: none; border-radius: 5px; margin: 0 10px; }
        .reject-btn { background-color: #dc3545; color: white; padding: 10px 20px; text-decoration: none; border-radius: 5px; margin: 0 10px; }
        .footer { font-size: 12px; color: #6c757d; margin-top: 30px; }
    </style>
</head>
<body>
    <div class="review-header">
        <h2>🤖 AGENT DECISION REQUIRES HUMAN REVIEW</h2>
        <p><strong>{{decision_type}}</strong> - Confidence: {{confidence_level}}%</p>
    </div>

    <div class="decision-content">
        <h3>Decision Context</h3>
        <p><strong>Agent:</strong> {{agent_name}}</p>
        <p><strong>Decision ID:</strong> {{decision_id}}</p>
        <p><strong>Timestamp:</strong> {{timestamp}}</p>

        <h3>Decision Summary</h3>
        <p>{{decision_summary}}</p>

        <h3>Key Factors Considered</h3>
        <ul>
            {{key_factors}}
        </ul>
    </div>

    <div class="confidence-metrics">
        <h3>🤔 Confidence Analysis</h3>
        <p><strong>Overall Confidence:</strong> {{confidence_level}}%</p>
        <p><strong>Risk Assessment:</strong> {{risk_level}}</p>
        <p><strong>Review Reason:</strong> {{review_reason}}</p>
    </div>

    <div class="action-buttons">
        <a href="{{approve_url}}" class="approve-btn">✅ Approve Decision</a>
        <a href="{{review_url}}" class="approve-btn">🔍 Review Details</a>
        <a href="{{reject_url}}" class="reject-btn">❌ Reject & Escalate</a>
    </div>

    <div class="footer">
        <p>This review request was generated by the Regulens AI Agent System.</p>
        <p>Please review the decision within {{review_deadline}} hours.</p>
    </div>
</body>
</html>
    )HTML",
    R"TEXT(
AGENT DECISION REQUIRES HUMAN REVIEW: {{decision_type}}

Decision Context:
- Agent: {{agent_name}}
- Decision ID: {{decision_id}}
- Timestamp: {{timestamp}}
- Confidence: {{confidence_level}}%

Decision Summary:
{{decision_summary}}

Key Factors Considered:
{{key_factors}}

Confidence Analysis:
- Overall Confidence: {{confidence_level}}%
- Risk Assessment: {{risk_level}}
- Review Reason: {{review_reason}}

Please review this decision at: {{review_url}}
Review deadline: {{review_deadline}} hours

This review request was generated by the Regulens AI Agent System.
    )TEXT"
};

} // namespace EmailTemplates

} // namespace regulens
