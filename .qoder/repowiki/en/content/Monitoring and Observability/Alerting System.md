# Alerting System

<cite>
**Referenced Files in This Document**
- [alerting-rules.yml](file://infrastructure/monitoring/prometheus/alerting-rules.yml)
- [prometheus-alerting-configmap.yaml](file://infrastructure/monitoring/prometheus/prometheus-alerting-configmap.yaml)
- [alert_evaluation_engine.cpp](file://shared/alerts/alert_evaluation_engine.cpp)
- [alert_evaluation_engine.hpp](file://shared/alerts/alert_evaluation_engine.hpp)
- [notification_service.cpp](file://shared/alerts/notification_service.cpp)
- [notification_service.hpp](file://shared/alerts/notification_service.hpp)
- [alert_management_handlers.cpp](file://shared/alerts/alert_management_handlers.cpp)
- [alert_management_handlers.hpp](file://shared/alerts/alert_management_handlers.hpp)
- [AlertManagement.tsx](file://frontend/src/pages/AlertManagement.tsx)
- [useAlerts.ts](file://frontend/src/hooks/useAlerts.ts)
- [api.ts](file://frontend/src/services/api.ts)
- [api.ts](file://frontend/src/types/api.ts)
</cite>

## Table of Contents
1. [Introduction](#introduction)
2. [System Architecture](#system-architecture)
3. [Prometheus Alerting Rules](#prometheus-alerting-rules)
4. [Alert Evaluation Engine](#alert-evaluation-engine)
5. [Notification Service](#notification-service)
6. [Alert Management Handlers](#alert-management-handlers)
7. [Frontend Integration](#frontend-integration)
8. [Configuration Management](#configuration-management)
9. [Best Practices](#best-practices)
10. [Troubleshooting](#troubleshooting)

## Introduction

The Regulens Alerting System is a comprehensive monitoring and notification framework designed to detect system health issues, agent performance problems, and compliance events. Built on Prometheus for metrics collection and Alertmanager for alert routing, the system integrates with an internal notification service to deliver alerts through multiple channels including email, webhooks, and UI notifications.

The system operates on a multi-layered architecture that combines Prometheus-based alerting rules with a custom evaluation engine for complex alert conditions, pattern detection, and anomaly identification. This ensures robust coverage of critical system components while minimizing alert fatigue through intelligent filtering and deduplication mechanisms.

## System Architecture

The alerting system follows a distributed architecture with clear separation of concerns across multiple components:

```mermaid
graph TB
subgraph "Monitoring Layer"
PM[Prometheus Metrics]
AM[Alertmanager]
end
subgraph "Evaluation Engine"
AEE[Alert Evaluation Engine]
DB[(PostgreSQL Database)]
ME[Metrics Collector]
end
subgraph "Notification Service"
NS[Notification Service]
ES[Email Service]
WS[Webhook Service]
SS[Slack Service]
end
subgraph "Management Interface"
API[REST API]
UI[Web UI]
HANDLERS[Management Handlers]
end
PM --> AM
AM --> AEE
AEE --> DB
AEE --> ME
AEE --> NS
NS --> ES
NS --> WS
NS --> SS
API --> HANDLERS
HANDLERS --> AEE
UI --> API
```

**Diagram sources**
- [alert_evaluation_engine.cpp](file://shared/alerts/alert_evaluation_engine.cpp#L1-L50)
- [notification_service.cpp](file://shared/alerts/notification_service.cpp#L1-L50)

The architecture consists of four primary layers:

1. **Monitoring Layer**: Collects system metrics and manages alert routing
2. **Evaluation Engine**: Processes complex alert conditions and generates incidents
3. **Notification Service**: Delivers alerts through multiple channels
4. **Management Interface**: Provides administrative controls and monitoring

**Section sources**
- [alert_evaluation_engine.cpp](file://shared/alerts/alert_evaluation_engine.cpp#L1-L100)
- [notification_service.cpp](file://shared/alerts/notification_service.cpp#L1-L100)

## Prometheus Alerting Rules

The system utilizes Prometheus for basic alerting rules covering system health, performance metrics, and operational indicators. The alerting rules are defined in YAML configuration files and deployed through Kubernetes ConfigMaps.

### System Health Alerts

Critical system health monitoring covers essential infrastructure components:

```yaml
- alert: RegulensServiceDown
  expr: up{job="regulens"} == 0
  for: 5m
  labels:
    severity: critical
    service: regulens
  annotations:
    summary: "Regulens service is down"
    description: "Regulens service {{ $labels.instance }} has been down for more than 5 minutes."

- alert: RegulensHighErrorRate
  expr: rate(http_requests_total{status=~"[5-9].*"}[5m]) / rate(http_requests_total[5m]) * 100 > 10
  for: 5m
  labels:
    severity: warning
    service: regulens
  annotations:
    summary: "High error rate detected"
    description: "Error rate for {{ $labels.instance }} is {{ $value }}% (threshold: 10%)"
```

### Circuit Breaker Monitoring

The system monitors circuit breaker states and failure rates to prevent cascading failures:

```yaml
- alert: CircuitBreakerOpen
  expr: regulens_circuit_breaker_state{state="OPEN"} == 1
  for: 1m
  labels:
    severity: critical
    component: circuit_breaker
  annotations:
    summary: "Circuit breaker is OPEN"
    description: "Circuit breaker for {{ $labels.service }} is OPEN, blocking all requests"

- alert: CircuitBreakerHighFailureRate
  expr: rate(regulens_circuit_breaker_failures_total[5m]) / rate(regulens_circuit_breaker_requests_total[5m]) * 100 > 20
  for: 5m
  labels:
    severity: warning
    component: circuit_breaker
  annotations:
    summary: "High circuit breaker failure rate"
    description: "Failure rate for {{ $labels.service }} is {{ $value }}% (threshold: 20%)"
```

### LLM Performance Monitoring

Language model performance is closely monitored to ensure optimal service delivery:

```yaml
- alert: LLMHighLatency
  expr: histogram_quantile(0.95, rate(regulens_llm_request_duration_seconds_bucket[5m])) > 30
  for: 5m
  labels:
    severity: warning
    component: llm
  annotations:
    summary: "High LLM response latency"
    description: "95th percentile latency for {{ $labels.provider }} is {{ $value }}s (threshold: 30s)"

- alert: LLMHighCost
  expr: rate(regulens_llm_estimated_cost_usd[5m]) > 100
  for: 5m
  labels:
    severity: warning
    component: llm
  annotations:
    summary: "High LLM API costs"
    description: "LLM costs for {{ $labels.provider }} are ${{ $value }}/minute (threshold: $100/min)"
```

### Compliance Operation Monitoring

Compliance-related operations require strict monitoring to maintain regulatory adherence:

```yaml
- alert: ComplianceDecisionFailure
  expr: rate(regulens_compliance_decision_failures_total[5m]) / rate(regulens_compliance_decisions_total[5m]) * 100 > 10
  for: 5m
  labels:
    severity: critical
    component: compliance
  annotations:
    summary: "High compliance decision failure rate"
    description: "Compliance decision failure rate is {{ $value }}% (threshold: 10%)"

- alert: ComplianceSLABreach
  expr: regulens_compliance_sla_met_percentage < 99.5
  for: 5m
  labels:
    severity: critical
    component: compliance
  annotations:
    summary: "Compliance SLA breach"
    description: "Compliance SLA met percentage is {{ $value }}% (threshold: 99.5%)"
```

**Section sources**
- [alerting-rules.yml](file://infrastructure/monitoring/prometheus/alerting-rules.yml#L1-L446)
- [prometheus-alerting-configmap.yaml](file://infrastructure/monitoring/prometheus/prometheus-alerting-configmap.yaml#L1-L187)

## Alert Evaluation Engine

The Alert Evaluation Engine serves as the core component responsible for processing complex alert conditions that go beyond simple threshold-based monitoring. It evaluates various rule types including threshold conditions, pattern matching, anomaly detection, and scheduled evaluations.

### Architecture and Design

```mermaid
classDiagram
class AlertEvaluationEngine {
-PostgreSQLConnection db_conn_
-StructuredLogger logger_
-std : : atomic~bool~ running_
-std : : chrono : : seconds evaluation_interval_
-std : : thread evaluation_thread_
-std : : atomic~bool~ should_trigger_evaluation_
-std : : mutex metrics_mutex_
-EvaluationMetrics metrics_
+start() void
+stop() void
+is_running() bool
+set_evaluation_interval(seconds) void
+trigger_evaluation() void
+get_metrics() EvaluationMetrics
+reset_metrics() void
-evaluation_loop() void
-evaluate_all_rules() void
-evaluate_threshold_rule(json, string) void
-evaluate_pattern_rule(json, string) void
-evaluate_anomaly_rule(json, string) void
-evaluate_scheduled_rule(json, string) void
-collect_metric_data(string) json
-compare_values(double, string, double) bool
-create_alert_incident(string, json, json) void
-is_rule_in_cooldown(string) bool
}
class EvaluationMetrics {
+uint64_t total_evaluations
+uint64_t alerts_triggered
+uint64_t rules_evaluated
+uint64_t evaluation_errors
+std : : chrono : : milliseconds last_evaluation_duration
+std : : chrono : : system_clock : : time_point last_evaluation_time
}
class AlertRuleType {
<<enumeration>>
THRESHOLD
PATTERN
ANOMALY
SCHEDULED
}
AlertEvaluationEngine --> EvaluationMetrics : "tracks"
AlertEvaluationEngine --> AlertRuleType : "processes"
```

**Diagram sources**
- [alert_evaluation_engine.hpp](file://shared/alerts/alert_evaluation_engine.hpp#L1-L48)
- [alert_evaluation_engine.cpp](file://shared/alerts/alert_evaluation_engine.cpp#L53-L80)

### Rule Types and Evaluation

The engine supports four distinct rule types, each designed for specific monitoring scenarios:

#### Threshold Rules
Simple numeric comparisons against predefined thresholds:

```cpp
void AlertEvaluationEngine::evaluate_threshold_rule(const nlohmann::json& rule, const std::string& rule_id) {
    nlohmann::json condition = rule["condition"];
    std::string metric_name = condition["metric"];
    
    // Collect current metric data
    nlohmann::json current_data = collect_metric_data(metric_name);
    if (current_data.empty() || !current_data.contains("value")) {
        logger_->log(LogLevel::WARN, "No data available for metric: " + metric_name);
        return;
    }
    
    double current_value = current_data["value"];
    std::string operator_str = condition["operator"];
    double threshold = condition["threshold"];
    
    // Evaluate condition
    bool condition_met = compare_values(current_value, operator_str, threshold);
    
    if (condition_met) {
        // Create incident and notify
        create_alert_incident(rule_id, rule, incident_data);
    }
}
```

#### Pattern Detection Rules
Regex-based pattern matching for complex conditions:

```cpp
bool AlertEvaluationEngine::evaluate_pattern_match(const std::string& pattern, const nlohmann::json& data) {
    try {
        // Convert pattern to regex
        std::regex regex_pattern(pattern, std::regex_constants::icase);
        
        // Convert data to string for pattern matching
        std::string data_str = data.dump();
        
        return std::regex_search(data_str, regex_pattern);
    } catch (const std::regex_error& e) {
        logger_->log(LogLevel::ERROR, "Invalid regex pattern: " + pattern + " - " + e.what());
        return false;
    }
}
```

#### Anomaly Detection Rules
Statistical analysis using historical data and Z-score calculations:

```cpp
bool AlertEvaluationEngine::evaluate_anomaly_detection(const nlohmann::json& baseline, const nlohmann::json& current) {
    if (!baseline.contains("mean") || !baseline.contains("std_dev") || !current.contains("value")) {
        return false;
    }
    
    double mean = baseline["mean"];
    double std_dev = baseline["std_dev"];
    double current_value = current["value"];
    
    // Calculate Z-score
    double z_score = std::abs((current_value - mean) / std_dev);
    
    // Consider it an anomaly if Z-score > 2 (beyond 2 standard deviations)
    return z_score > 2.0;
}
```

### Metric Collection and Storage

The engine collects metrics from multiple sources and maintains historical data for trend analysis:

```mermaid
sequenceDiagram
participant Engine as Alert Evaluation Engine
participant Collector as Metrics Collector
participant DB as PostgreSQL Database
participant History as Metric History
Engine->>Collector : collect_metric_data(metric_name)
Collector->>DB : query current metrics
DB-->>Collector : metric values
Collector-->>Engine : formatted metrics
Engine->>History : store_metric_history()
History->>DB : INSERT INTO metric_history
Engine->>Engine : evaluate_condition()
Engine->>Engine : create_alert_incident()
```

**Diagram sources**
- [alert_evaluation_engine.cpp](file://shared/alerts/alert_evaluation_engine.cpp#L400-L500)

**Section sources**
- [alert_evaluation_engine.cpp](file://shared/alerts/alert_evaluation_engine.cpp#L1-L914)
- [alert_evaluation_engine.hpp](file://shared/alerts/alert_evaluation_engine.hpp#L1-L48)

## Notification Service

The Notification Service handles the delivery of alerts through multiple channels, ensuring reliable communication across different systems and stakeholders.

### Channel Support and Architecture

```mermaid
classDiagram
class NotificationService {
-PostgreSQLConnection db_conn_
-StructuredLogger logger_
-std : : atomic~bool~ running_
-int max_concurrent_notifications_
-int max_retry_attempts_
-std : : chrono : : seconds base_retry_delay_
-std : : vector~std : : thread~ worker_threads_
-std : : thread retry_thread_
-std : : mutex queue_mutex_
-std : : condition_variable queue_cv_
-std : : queue~NotificationRequest~ notification_queue_
-std : : queue~NotificationRequest~ retry_queue_
-NotificationMetrics metrics_
+start() void
+stop() void
+is_running() bool
+send_notification(string, string, json) string
+send_notification_async(string, string, json) void
+send_notifications_batch(vector) void
+test_channel(string, json) bool
+set_max_concurrent_notifications(int) void
+set_retry_attempts(int) void
+set_retry_delay(seconds) void
+get_metrics() NotificationMetrics
+reset_metrics() void
-notification_worker_loop() void
-retry_worker_loop() void
-deliver_notification(NotificationRequest) bool
-send_email_notification(NotificationRequest) bool
-send_webhook_notification(NotificationRequest) bool
-send_slack_notification(NotificationRequest) bool
-send_sms_notification(NotificationRequest) bool
-send_pagerduty_notification(NotificationRequest) bool
}
class NotificationRequest {
+string notification_id
+string incident_id
+string channel_id
+string channel_type
+json channel_config
+json alert_data
+int retry_count
+std : : chrono : : system_clock : : time_point scheduled_time
}
class NotificationMetrics {
+uint64_t total_sent
+uint64_t successful_deliveries
+uint64_t failed_deliveries
+uint64_t retries_attempted
+std : : unordered_map~string, uint64_t~ deliveries_by_channel
+std : : unordered_map~string, uint64_t~ failures_by_channel
+std : : chrono : : milliseconds avg_delivery_time
+std : : chrono : : system_clock : : time_point last_notification_time
}
NotificationService --> NotificationRequest : "processes"
NotificationService --> NotificationMetrics : "tracks"
```

**Diagram sources**
- [notification_service.hpp](file://shared/alerts/notification_service.hpp#L1-L50)
- [notification_service.cpp](file://shared/alerts/notification_service.cpp#L1-L100)

### Multi-Channel Delivery

The service supports multiple notification channels with specialized handlers:

#### Email Notifications
Production-grade SMTP implementation with authentication and error handling:

```cpp
bool NotificationService::send_smtp_email(const nlohmann::json& email_payload) {
    try {
        // Get SMTP configuration from environment
        const char* smtp_host = std::getenv("SMTP_HOST");
        const char* smtp_port = std::getenv("SMTP_PORT");
        const char* smtp_user = std::getenv("SMTP_USER");
        const char* smtp_pass = std::getenv("SMTP_PASS");
        const char* smtp_from = std::getenv("SMTP_FROM");

        if (!smtp_host || !smtp_port || !smtp_user || !smtp_pass || !smtp_from) {
            logger_->log(LogLevel::ERROR, "SMTP configuration not complete - missing environment variables");
            return false;
        }

        // Create socket and establish connection
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        // SMTP protocol implementation...
        
        return true;
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in SMTP email sending: " + std::string(e.what()));
        return false;
    }
}
```

#### Webhook Notifications
HTTP-based delivery for external integrations:

```cpp
bool NotificationService::send_webhook_notification(const NotificationRequest& request) {
    try {
        auto webhook_payload = format_webhook_payload(request);
        
        if (!request.channel_config.contains("url")) {
            logger_->log(LogLevel::ERROR, "Missing webhook URL in channel config");
            return false;
        }
        
        std::string url = request.channel_config["url"];
        std::string response;
        std::string error;
        
        // Prepare headers
        std::map<std::string, std::string> headers = {
            {"Content-Type", "application/json"}
        };
        
        // Add custom headers from config
        if (request.channel_config.contains("headers")) {
            for (auto& [key, value] : request.channel_config["headers"].items()) {
                headers[key] = value.get<std::string>();
            }
        }
        
        // Send HTTP request
        bool success = send_http_request(url, webhook_payload, headers, &response_str, &error_str);
        
        return success;
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Failed to send webhook notification: " + std::string(e.what()));
        return false;
    }
}
```

### Retry Mechanism and Reliability

The notification service implements sophisticated retry logic with exponential backoff:

```mermaid
flowchart TD
Start([Notification Request]) --> CheckRetry{"Retry Count<br/>< Max Attempts?"}
CheckRetry --> |No| MarkFailed[Mark as Failed]
CheckRetry --> |Yes| CalcDelay[Calculate Backoff Delay]
CalcDelay --> QueueRetry[Add to Retry Queue]
QueueRetry --> WaitDelay[Wait for Delay]
WaitDelay --> CheckTime{"Current Time >=<br/>Scheduled Time?"}
CheckTime --> |No| WaitDelay
CheckTime --> |Yes| AttemptSend[Attempt Delivery]
AttemptSend --> Success{"Delivery<br/>Successful?"}
Success --> |Yes| MarkSuccess[Mark as Delivered]
Success --> |No| CheckRetry
MarkFailed --> End([End])
MarkSuccess --> End
```

**Diagram sources**
- [notification_service.cpp](file://shared/alerts/notification_service.cpp#L400-L500)

**Section sources**
- [notification_service.cpp](file://shared/alerts/notification_service.cpp#L1-L1248)
- [notification_service.hpp](file://shared/alerts/notification_service.hpp#L1-L50)

## Alert Management Handlers

The Alert Management Handlers provide REST API endpoints for managing alert rules, incidents, and notification channels through a comprehensive administrative interface.

### API Endpoints and Operations

```mermaid
classDiagram
class AlertManagementHandlers {
-PostgreSQLConnection db_conn_
-StructuredLogger logger_
+handle_get_alert_rules(map) string
+handle_get_alert_rule_by_id(string) string
+handle_create_alert_rule(string, string) string
+handle_update_alert_rule(string, string) string
+handle_delete_alert_rule(string) string
+handle_get_alert_history(map) string
+handle_acknowledge_alert(string, string, string) string
+handle_resolve_alert(string, string, string) string
+handle_get_notification_channels(map) string
+handle_create_notification_channel(string) string
+handle_update_notification_channel(string, string) string
+handle_delete_notification_channel(string) string
-validate_alert_condition(json) bool
-serialize_alert_rule(PGresult, int) json
-serialize_alert_incident(PGresult, int) json
-serialize_notification_channel(PGresult, int) json
}
class AlertRule {
+string rule_id
+string rule_name
+string description
+string rule_type
+string severity
+json condition
+json notification_channels
+json notification_config
+int cooldown_minutes
+bool is_enabled
+string created_by
+string created_at
+string updated_at
+string last_triggered_at
}
class AlertIncident {
+string incident_id
+string rule_id
+string severity
+string title
+string message
+json incident_data
+string triggered_at
+string acknowledged_at
+string acknowledged_by
+string resolved_at
+string resolved_by
+string resolution_notes
+string status
+string notification_status
}
AlertManagementHandlers --> AlertRule : "manages"
AlertManagementHandlers --> AlertIncident : "processes"
```

**Diagram sources**
- [alert_management_handlers.hpp](file://shared/alerts/alert_management_handlers.hpp#L1-L50)
- [alert_management_handlers.cpp](file://shared/alerts/alert_management_handlers.cpp#L1-L100)

### Rule Management Operations

The system provides comprehensive CRUD operations for alert rules:

#### Creating Alert Rules
```cpp
std::string AlertManagementHandlers::handle_create_alert_rule(const std::string& request_body, const std::string& user_id) {
    try {
        json request = json::parse(request_body);

        // Validate required fields
        if (!request.contains("rule_name") || !request.contains("rule_type") || 
            !request.contains("severity") || !request.contains("condition")) {
            return R"({"error": "Missing required fields"})";
        }

        // Validate alert condition
        if (!validate_alert_condition(request["condition"])) {
            return R"({"error": "Invalid alert condition format"})";
        }

        // Insert into database
        const char* params[9] = {
            rule_name.c_str(),
            description.c_str(),
            rule_type.c_str(),
            severity.c_str(),
            condition_str.c_str(),
            notification_channels_str.c_str(),
            notification_config_str.c_str(),
            cooldown_str.c_str(),
            user_id.c_str()
        };

        PGresult* result = PQexecParams(/* SQL INSERT */);

        // Return success response
        json response = {
            {"rule_id", PQgetvalue(result, 0, 0)},
            {"rule_name", rule_name},
            {"is_enabled", true},
            {"created_by", user_id}
        };

        return response.dump();

    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, "Exception in handle_create_alert_rule: " + std::string(e.what()));
        return R"({"error": "Internal server error"})";
    }
}
```

#### Incident Lifecycle Management
The system tracks alert incidents through their complete lifecycle:

```mermaid
stateDiagram-v2
[*] --> Active
Active --> Acknowledged : User Acknowledges
Active --> Resolved : User Resolves
Acknowledged --> Resolved : User Resolves
Resolved --> [*]
Active --> Active : Auto-resolve after cooldown
Acknowledged --> Acknowledged : Re-acknowledge
Resolved --> Active : Reopen incident
```

**Diagram sources**
- [alert_management_handlers.cpp](file://shared/alerts/alert_management_handlers.cpp#L600-L700)

### Validation and Sanitization

The handlers implement comprehensive validation for alert conditions:

```cpp
bool AlertManagementHandlers::validate_alert_condition(const json& condition) {
    // Validate required fields
    if (!condition.contains("type") || !condition.contains("metric")) {
        return false;
    }
    
    std::string type = condition["type"];
    if (type == "threshold") {
        return condition.contains("operator") && condition.contains("threshold");
    } else if (type == "pattern") {
        return condition.contains("pattern");
    } else if (type == "anomaly") {
        return condition.contains("sensitivity");
    }
    
    return false;
}
```

**Section sources**
- [alert_management_handlers.cpp](file://shared/alerts/alert_management_handlers.cpp#L1-L2104)
- [alert_management_handlers.hpp](file://shared/alerts/alert_management_handlers.hpp#L1-L50)

## Frontend Integration

The frontend provides a comprehensive user interface for managing alerts, viewing notification logs, and configuring alert rules through React-based components and hooks.

### React Query Integration

```mermaid
sequenceDiagram
participant UI as Alert Management UI
participant Hook as useAlerts Hook
participant API as API Client
participant Handler as Alert Management Handler
participant Engine as Alert Evaluation Engine
UI->>Hook : useAlertRules()
Hook->>API : apiClient.getAlertRules()
API->>Handler : GET /api/alert-rules
Handler->>Engine : Query database rules
Engine-->>Handler : Rule data
Handler-->>API : JSON response
API-->>Hook : React Query data
Hook-->>UI : Rendered component
UI->>Hook : useCreateAlertRule()
Hook->>API : apiClient.createAlertRule()
API->>Handler : POST /api/alert-rules
Handler->>Engine : Create rule in database
Engine-->>Handler : Created rule
Handler-->>API : Success response
API-->>Hook : Mutation result
Hook-->>UI : Updated state
```

**Diagram sources**
- [useAlerts.ts](file://frontend/src/hooks/useAlerts.ts#L1-L47)
- [api.ts](file://frontend/src/services/api.ts#L1-L100)

### Alert Management Interface

The main alert management page provides comprehensive functionality:

```typescript
const AlertManagement: React.FC = () => {
  const [showCreateModal, setShowCreateModal] = useState(false);
  
  const { data: rules = [], isLoading: rulesLoading, refetch: refetchRules } = useAlertRules();
  const { data: deliveryLog = [], isLoading: logLoading } = useAlertDeliveryLog();
  const { data: stats, isLoading: statsLoading } = useAlertStats();
  const createRule = useCreateAlertRule();

  const handleCreateRule = async (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    const formData = new FormData(e.currentTarget);
    
    const recipients = (formData.get('recipients') as string).split(',').map(r => r.trim());
    
    await createRule.mutateAsync({
      name: formData.get('name') as string,
      description: formData.get('description') as string,
      recipients,
      enabled: true,
      severity_filter: ['critical', 'high'],
      notification_channels: ['email'],
      throttle_minutes: 60,
    });
    
    setShowCreateModal(false);
    e.currentTarget.reset();
  };

  return (
    <div className="space-y-6">
      {/* Header with statistics */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-gray-900">Alert Management</h1>
          <p className="text-gray-600 mt-1">Regulatory change alerts and email notifications</p>
        </div>
        
        <div className="flex gap-3">
          <button onClick={() => refetchRules()} className="flex items-center gap-2 px-4 py-2 bg-white border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors">
            <RefreshCw className={`w-5 h-5 ${rulesLoading ? 'animate-spin' : ''}`} />
            Refresh
          </button>
          
          <button onClick={() => setShowCreateModal(true)} className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors">
            <Plus className="w-5 h-5" />
            New Alert Rule
          </button>
        </div>
      </div>

      {/* Statistics grid */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-6">
        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-3 mb-2">
            <div className="p-2 bg-blue-100 rounded-lg">
              <Bell className="w-5 h-5 text-blue-600" />
            </div>
            <h3 className="text-sm font-medium text-gray-600">Total Rules</h3>
          </div>
          <p className="text-2xl font-bold text-gray-900">{stats?.total_rules || 0}</p>
          <p className="text-xs text-gray-500 mt-1">{stats?.active_rules || 0} active</p>
        </div>
        {/* More statistics cards */}
      </div>

      {/* Alert Rules List */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200">
        <div className="p-6 border-b border-gray-200">
          <h2 className="text-lg font-semibold text-gray-900">Alert Rules</h2>
          <p className="text-sm text-gray-600 mt-1">Configure notification rules for regulatory changes</p>
        </div>

        <div className="divide-y divide-gray-200">
          {rules.map((rule) => (
            <div key={rule.rule_id} className="p-6 hover:bg-gray-50 transition-colors">
              <div className="flex items-start justify-between">
                <div className="flex-1">
                  <div className="flex items-center gap-3 mb-2">
                    <h3 className="text-lg font-semibold text-gray-900">{rule.name}</h3>
                    <span className={`px-2 py-1 text-xs font-medium rounded-full ${rule.enabled ? 'bg-green-100 text-green-800' : 'bg-gray-100 text-gray-800'}`}>
                      {rule.enabled ? 'Enabled' : 'Disabled'}
                    </span>
                  </div>
                  <p className="text-gray-600 text-sm mb-3">{rule.description}</p>
                  <div className="flex items-center gap-6 text-sm text-gray-500">
                    <div className="flex items-center gap-1">
                      <Mail className="w-4 h-4" />
                      <span>{rule.recipients.length} recipient(s)</span>
                    </div>
                    <div className="flex items-center gap-1">
                      <Clock className="w-4 h-4" />
                      <span>{rule.throttle_minutes}min throttle</span>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Delivery Log Table */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200">
        <div className="p-6 border-b border-gray-200">
          <h2 className="text-lg font-semibold text-gray-900">Recent Deliveries</h2>
          <p className="text-sm text-gray-600 mt-1">Email delivery status and history</p>
        </div>

        <div className="overflow-x-auto">
          <table className="w-full">
            <thead className="bg-gray-50">
              <tr>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Recipient</th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Channel</th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Status</th>
                <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Sent At</th>
              </tr>
            </thead>
            <tbody className="bg-white divide-y divide-gray-200">
              {deliveryLog.map((log) => (
                <tr key={log.delivery_id} className="hover:bg-gray-50">
                  <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">{log.recipient}</td>
                  <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">{log.channel}</td>
                  <td className="px-6 py-4 whitespace-nowrap">
                    <span className={`px-2 py-1 text-xs font-medium rounded-full ${getStatusColor(log.status)}`}>
                      {log.status}
                    </span>
                  </td>
                  <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                    {new Date(log.sent_at).toLocaleString()}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );
};
```

### Type Safety and API Integration

The frontend uses strongly typed APIs for robust development:

```typescript
// API types from backend
export interface AlertRule {
  rule_id: string;
  name: string;
  description: string;
  enabled: boolean;
  severity_filter: string[];
  source_filter?: string[];
  keyword_filters?: string[];
  notification_channels: string[];
  recipients: string[];
  throttle_minutes: number;
  created_by?: string;
  created_at: string;
  updated_at: string;
  last_triggered_at?: string;
  trigger_count: number;
  metadata?: Record<string, unknown>;
}

export interface AlertDeliveryLog {
  delivery_id: string;
  rule_id: string;
  regulatory_change_id?: string;
  recipient: string;
  channel: string;
  status: 'pending' | 'sent' | 'failed' | 'throttled' | 'bounced';
  sent_at: string;
  delivered_at?: string;
  error_message?: string;
  retry_count: number;
  message_id?: string;
  metadata?: Record<string, unknown>;
}

// React Query hooks
export function useAlertRules() {
  return useQuery({
    queryKey: ['alert-rules'],
    queryFn: () => apiClient.getAlertRules(),
    refetchInterval: 30000, // Refetch every 30 seconds
  });
}

export function useCreateAlertRule() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: (request: CreateAlertRuleRequest) =>
      apiClient.createAlertRule(request),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['alert-rules'] });
      queryClient.invalidateQueries({ queryKey: ['alert-stats'] });
    },
  });
}
```

**Section sources**
- [AlertManagement.tsx](file://frontend/src/pages/AlertManagement.tsx#L1-L324)
- [useAlerts.ts](file://frontend/src/hooks/useAlerts.ts#L1-L47)
- [api.ts](file://frontend/src/services/api.ts#L1-L1550)
- [api.ts](file://frontend/src/types/api.ts#L800-L900)

## Configuration Management

The alerting system provides comprehensive configuration management through database-driven rule definitions, environment-based channel configurations, and runtime parameter adjustments.

### Database Schema and Structure

The system uses PostgreSQL for persistent storage of alert rules and configuration:

```sql
-- Alert Rules Table
CREATE TABLE alert_rules (
    rule_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    rule_name VARCHAR(255) NOT NULL,
    description TEXT,
    rule_type VARCHAR(50) NOT NULL,
    severity VARCHAR(20) NOT NULL,
    condition JSONB NOT NULL,
    notification_channels JSONB NOT NULL,
    notification_config JSONB,
    cooldown_minutes INTEGER DEFAULT 5,
    is_enabled BOOLEAN DEFAULT true,
    created_by VARCHAR(255),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    last_triggered_at TIMESTAMP WITH TIME ZONE
);

-- Alert Incidents Table
CREATE TABLE alert_incidents (
    incident_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    rule_id UUID REFERENCES alert_rules(rule_id),
    severity VARCHAR(20) NOT NULL,
    title VARCHAR(255) NOT NULL,
    message TEXT NOT NULL,
    incident_data JSONB,
    triggered_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    acknowledged_at TIMESTAMP WITH TIME ZONE,
    acknowledged_by VARCHAR(255),
    resolved_at TIMESTAMP WITH TIME ZONE,
    resolved_by VARCHAR(255),
    resolution_notes TEXT,
    status VARCHAR(20) DEFAULT 'active',
    notification_status VARCHAR(20) DEFAULT 'pending'
);

-- Notification Channels Table
CREATE TABLE notification_channels (
    channel_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    channel_type VARCHAR(50) NOT NULL,
    channel_name VARCHAR(255) NOT NULL,
    configuration JSONB NOT NULL,
    is_enabled BOOLEAN DEFAULT true,
    last_tested_at TIMESTAMP WITH TIME ZONE,
    test_status VARCHAR(20),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);
```

### Environment-Based Configuration

The system supports environment-based configuration for different deployment scenarios:

```bash
# SMTP Configuration
export SMTP_HOST=smtp.gmail.com
export SMTP_PORT=587
export SMTP_USER=alerts@company.com
export SMTP_PASS=your-app-password
export SMTP_FROM=alerts@company.com

# Notification Service Configuration
export MAX_CONCURRENT_NOTIFICATIONS=10
export MAX_RETRY_ATTEMPTS=3
export BASE_RETRY_DELAY_SECONDS=30

# Database Configuration
export DATABASE_URL=postgresql://user:password@localhost/regulens
export DATABASE_POOL_SIZE=10
```

### Runtime Parameter Tuning

The alert evaluation engine supports runtime parameter adjustment:

```cpp
void AlertEvaluationEngine::set_evaluation_interval(std::chrono::seconds interval) {
    evaluation_interval_ = interval;
    logger_->log(LogLevel::INFO, "Evaluation interval set to: " + std::to_string(interval.count()) + " seconds");
}

void NotificationService::set_max_concurrent_notifications(int max_concurrent) {
    max_concurrent_notifications_ = max_concurrent;
    logger_->log(LogLevel::INFO, "Max concurrent notifications set to: " + std::to_string(max_concurrent));
}

void NotificationService::set_retry_attempts(int max_attempts) {
    max_retry_attempts_ = max_attempts;
    logger_->log(LogLevel::INFO, "Max retry attempts set to: " + std::to_string(max_attempts));
}
```

**Section sources**
- [alert_evaluation_engine.cpp](file://shared/alerts/alert_evaluation_engine.cpp#L60-L80)
- [notification_service.cpp](file://shared/alerts/notification_service.cpp#L150-L170)

## Best Practices

### Effective Alerting Rule Design

1. **Start with Clear Objectives**: Define what you want to monitor and why it matters
2. **Use Appropriate Thresholds**: Set thresholds based on historical data and business impact
3. **Implement Cooldown Periods**: Prevent alert fatigue by adding reasonable cooldown periods
4. **Prioritize by Severity**: Use different severity levels for different types of issues
5. **Include Actionable Information**: Ensure alerts provide sufficient context for remediation

### Rule Type Selection Guidelines

```mermaid
flowchart TD
Start([Need to Monitor Something]) --> DataAvailable{"Historical Data<br/>Available?"}
DataAvailable --> |Yes| TrendAnalysis{"Looking for<br/>Trends?"}
DataAvailable --> |No| ImmediateIssue{"Immediate<br/>Issue?"}
TrendAnalysis --> |Yes| Anomaly[Anomaly Detection Rule]
TrendAnalysis --> |No| Pattern[Pattern Matching Rule]
ImmediateIssue --> |Yes| Threshold[Threshold Rule]
ImmediateIssue --> |No| Scheduled[Scheduled Rule]
Anomaly --> Implementation[Implement with Sensitivity Config]
Pattern --> Implementation
Threshold --> Implementation
Scheduled --> Implementation
```

### Performance Optimization

1. **Optimize Evaluation Frequency**: Balance responsiveness with system load
2. **Index Database Queries**: Ensure proper indexing on frequently queried columns
3. **Batch Processing**: Group similar operations to reduce overhead
4. **Memory Management**: Implement proper cleanup and resource management
5. **Monitoring Self-Monitoring**: Track alert evaluation performance

### Security Considerations

1. **Secure Credential Storage**: Use environment variables or secure vaults for sensitive data
2. **Access Control**: Implement proper authentication and authorization for alert management
3. **Audit Logging**: Maintain comprehensive logs of all alert-related operations
4. **Encryption**: Encrypt sensitive data in transit and at rest
5. **Rate Limiting**: Implement rate limiting to prevent abuse

## Troubleshooting

### Common Issues and Solutions

#### Alert Fatigue
**Problem**: Too many alerts causing desensitization
**Solution**: 
- Review and adjust threshold sensitivities
- Implement better alert grouping and deduplication
- Add more sophisticated pattern matching
- Increase cooldown periods

#### False Positives
**Problem**: Alerts triggering for non-issues
**Solution**:
- Improve rule specificity with additional conditions
- Add historical baseline comparisons
- Implement confidence scoring
- Use machine learning for pattern refinement

#### Notification Delivery Failures
**Problem**: Alerts not reaching intended recipients
**Solution**:
- Verify SMTP configuration and credentials
- Check network connectivity and firewall settings
- Review retry logic and backoff strategies
- Monitor delivery logs for specific error patterns

#### Performance Issues
**Problem**: Slow alert evaluation or notification delivery
**Solution**:
- Optimize database queries and indexing
- Reduce evaluation frequency for less critical rules
- Implement asynchronous processing where appropriate
- Scale horizontally using multiple worker threads

### Debugging Tools and Techniques

#### Enable Debug Logging
```cpp
// In AlertEvaluationEngine
logger_->log(LogLevel::DEBUG, "Alert evaluation completed in " + 
            std::to_string(duration.count()) + "ms");
```

#### Monitor Metrics
```cpp
// Access evaluation metrics
AlertEvaluationEngine::EvaluationMetrics metrics = engine.get_metrics();
std::cout << "Total evaluations: " << metrics.total_evaluations << std::endl;
std::cout << "Alerts triggered: " << metrics.alerts_triggered << std::endl;
std::cout << "Evaluation errors: " << metrics.evaluation_errors << std::endl;
```

#### Database Query Analysis
```sql
-- Monitor rule evaluation performance
SELECT rule_name, AVG(eval_duration) as avg_duration, COUNT(*) as eval_count
FROM alert_evaluations 
WHERE created_at > NOW() - INTERVAL '1 hour'
GROUP BY rule_name
ORDER BY avg_duration DESC;

-- Check notification delivery status
SELECT channel_type, status, COUNT(*) as count
FROM alert_deliveries 
WHERE created_at > NOW() - INTERVAL '1 day'
GROUP BY channel_type, status;
```

### Recovery Procedures

#### Restart Alert Evaluation Engine
```bash
# Stop the engine
docker-compose stop alert-engine

# Apply configuration updates
docker-compose up -d alert-engine
```

#### Reset Alert State
```sql
-- Reset all alert incidents to active state
UPDATE alert_incidents 
SET status = 'active', acknowledged_at = NULL, resolved_at = NULL 
WHERE status IN ('acknowledged', 'resolved');

-- Clear evaluation metrics
TRUNCATE TABLE alert_evaluations;
```

#### Restore Notification Channels
```sql
-- Backup current channels
CREATE TABLE notification_channels_backup AS 
SELECT * FROM notification_channels;

-- Restore from backup
INSERT INTO notification_channels 
SELECT * FROM notification_channels_backup;
```

**Section sources**
- [alert_evaluation_engine.cpp](file://shared/alerts/alert_evaluation_engine.cpp#L82-L115)
- [notification_service.cpp](file://shared/alerts/notification_service.cpp#L200-L300)