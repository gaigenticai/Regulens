#ifndef REGULENS_ALERT_MANAGEMENT_HANDLERS_HPP
#define REGULENS_ALERT_MANAGEMENT_HANDLERS_HPP

#include <string>
#include <map>
#include <memory>
#include <libpq-fe.h>
#include <nlohmann/json.hpp>
#include "../database/postgresql_connection.hpp"
#include "../logging/structured_logger.hpp"

namespace regulens {
namespace alerts {

class AlertManagementHandlers {
public:
    AlertManagementHandlers(
        std::shared_ptr<PostgreSQLConnection> db_conn,
        std::shared_ptr<StructuredLogger> logger
    );

    // Alert rule management
    std::string handle_get_alert_rules(const std::map<std::string, std::string>& query_params);
    std::string handle_get_alert_rule_by_id(const std::string& rule_id);
    std::string handle_create_alert_rule(const std::string& request_body, const std::string& user_id);
    std::string handle_update_alert_rule(const std::string& rule_id, const std::string& request_body);
    std::string handle_delete_alert_rule(const std::string& rule_id);

    // Alert incident management
    std::string handle_get_alert_history(const std::map<std::string, std::string>& query_params);
    std::string handle_acknowledge_alert(const std::string& incident_id, const std::string& request_body, const std::string& user_id);
    std::string handle_resolve_alert(const std::string& incident_id, const std::string& request_body, const std::string& user_id);

    // Notification channel management
    std::string handle_get_notification_channels(const std::map<std::string, std::string>& query_params);
    std::string handle_create_notification_channel(const std::string& request_body, const std::string& user_id);
    std::string handle_update_notification_channel(const std::string& channel_id, const std::string& request_body);
    std::string handle_delete_notification_channel(const std::string& channel_id);
    std::string handle_test_notification_channel(const std::string& channel_id);

    // Alert testing
    std::string handle_test_alert_delivery(const std::string& request_body, const std::string& user_id);

    // Dashboard metrics
    std::string handle_get_alert_metrics(const std::map<std::string, std::string>& query_params);

private:
    std::shared_ptr<PostgreSQLConnection> db_conn_;
    std::shared_ptr<StructuredLogger> logger_;

    // Helper methods
    nlohmann::json serialize_alert_rule(PGresult* result, int row);
    nlohmann::json serialize_alert_incident(PGresult* result, int row);
    nlohmann::json serialize_notification_channel(PGresult* result, int row);
    nlohmann::json serialize_alert_notification(PGresult* result, int row);
    
    std::string extract_user_id_from_jwt(const std::map<std::string, std::string>& headers);
    bool validate_json_schema(const nlohmann::json& data, const std::string& schema_type);
    bool validate_alert_condition(const nlohmann::json& condition);
    bool validate_notification_config(const nlohmann::json& config, const std::string& channel_type);
    
    // Utility functions
    std::string base64_decode(const std::string& encoded_string);
    
    std::string generate_alert_title(const nlohmann::json& rule, const nlohmann::json& incident_data);
    std::string generate_alert_message(const nlohmann::json& rule, const nlohmann::json& incident_data);
    
    // Background evaluation methods
    void evaluate_alert_rules();
    bool check_rule_cooldown(const std::string& rule_id);
    void update_rule_last_triggered(const std::string& rule_id);
    void create_alert_incident(const std::string& rule_id, const nlohmann::json& rule, const nlohmann::json& incident_data);
    
    // Notification methods
    void send_alert_notifications(const std::string& incident_id, const nlohmann::json& rule);
    bool send_email_notification(const nlohmann::json& config, const nlohmann::json& alert_data);
    bool send_webhook_notification(const nlohmann::json& config, const nlohmann::json& alert_data);
    bool send_slack_notification(const nlohmann::json& config, const nlohmann::json& alert_data);
    void log_notification_attempt(const std::string& incident_id, const std::string& channel_id, 
                                 const std::string& status, const std::string& error_message = "");
    
    // Retry mechanism for failed notifications
    void retry_failed_notifications();
    void schedule_notification_retry(const std::string& notification_id, int retry_count);
};

} // namespace alerts
} // namespace regulens

#endif // REGULENS_ALERT_MANAGEMENT_HANDLERS_HPP