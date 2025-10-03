# Regulens Prometheus Alerting Rules

This directory contains comprehensive Prometheus alerting rules for the Regulens enterprise compliance platform.

## Alert Categories

### System Health Alerts
- **RegulensServiceDown**: Critical alert when Regulens service is unavailable
- **RegulensHighErrorRate**: Warning when HTTP error rate exceeds 10%
- **RegulensHighMemoryUsage**: Warning when memory usage exceeds 85%
- **RegulensHighCPUUsage**: Warning when CPU usage exceeds 80%
- **RegulensLowDiskSpace**: Warning when disk space drops below 15%

### Circuit Breaker Alerts
- **CircuitBreakerOpen**: Critical alert when circuit breaker opens
- **CircuitBreakerHighFailureRate**: Warning when failure rate exceeds 20%
- **CircuitBreakerHighLatency**: Warning when P95 latency exceeds 10 seconds

### LLM Performance Alerts
- **LLMAPIUnavailable**: Critical alert when no LLM requests processed for 5+ minutes
- **LLMHighErrorRate**: Warning when LLM error rate exceeds 15%
- **LLMHighLatency**: Warning when LLM P95 latency exceeds 30 seconds
- **LLMHighCost**: Warning when LLM costs exceed $100/minute
- **LLMLowCacheHitRate**: Info alert when LLM cache hit rate drops below 70%

### Redis Cache Alerts
- **RedisUnavailable**: Critical alert when Redis is down
- **RedisHighErrorRate**: Warning when Redis error rate exceeds 5%
- **RedisLowCacheHitRate**: Warning when cache hit rate drops below 60%
- **RedisHighMemoryUsage**: Warning when Redis memory exceeds 8GB
- **RedisHighEvictionRate**: Warning when eviction rate exceeds 100/min

### Compliance Operation Alerts
- **ComplianceDecisionFailure**: Critical when decision failure rate exceeds 10%
- **ComplianceLowAccuracy**: Warning when decision accuracy drops below 95%
- **ComplianceHighProcessingLatency**: Warning when P95 processing time exceeds 60 seconds
- **ComplianceSLABreach**: Critical when SLA compliance drops below 99.5%
- **RiskAssessmentFailure**: Warning when risk assessment failure rate exceeds 5%

### Data Ingestion Alerts
- **DataIngestionFailure**: Critical when ingestion error rate exceeds 15%
- **DataIngestionHighLatency**: Warning when data source P95 latency exceeds 300 seconds
- **RegulatoryDataStale**: Warning when regulatory data is older than 24 hours
- **DataIngestionLowThroughput**: Warning when document processing rate drops below 1/min

### Kubernetes Operator Alerts
- **KubernetesOperatorDown**: Critical when operator is down for 5+ minutes
- **KubernetesOperatorHighReconciliationLatency**: Warning when reconciliation P95 exceeds 30 seconds
- **KubernetesResourceUnhealthy**: Warning when managed resources become unhealthy
- **KubernetesScalingFailure**: Warning when resource scaling operations fail

### Business Logic Alerts
- **AgentActivityLow**: Info alert when agent activity drops below 1/min
- **HumanAICollaborationStale**: Info alert when no human-AI interactions for 2+ hours
- **PatternRecognitionDrift**: Warning when pattern recognition accuracy drops below 90%
- **FeedbackIncorporationBacklog**: Warning when feedback backlog exceeds 1000 items

### Security Alerts
- **SuspiciousActivityDetected**: Critical alert for high-severity security events
- **AuthenticationFailures**: Warning when auth failure rate exceeds 10%
- **UnauthorizedAccessAttempts**: Warning when unauthorized access exceeds 10/min

## Alert Severity Levels

- **Critical**: Immediate action required, potential service disruption
- **Warning**: Action needed soon, performance degradation
- **Info**: Awareness notification, no immediate action required

## Configuration

### Alert Thresholds

All thresholds are configurable and should be adjusted based on your environment:

```yaml
# Example customizations
groups:
  - name: custom_system_health
    rules:
      - alert: CustomMemoryAlert
        expr: (1 - system_memory_available_bytes / system_memory_total_bytes) * 100 > 90  # Higher threshold
        for: 10m  # Longer evaluation time
```

### Alert Routing

Configure alert routing in your alert manager:

```yaml
route:
  group_by: ['alertname', 'severity']
  group_wait: 10s
  group_interval: 10s
  repeat_interval: 1h
  receiver: 'default'
  routes:
  - match:
      severity: critical
    receiver: 'critical-pager'
  - match:
      severity: warning
    receiver: 'warning-email'

receivers:
- name: 'critical-pager'
  pagerduty_configs:
  - service_key: 'your-pagerduty-key'
- name: 'warning-email'
  email_configs:
  - to: 'alerts@yourcompany.com'
```

### Alert Labels

Alerts include these standard labels:
- `severity`: critical, warning, info
- `service`: Affected service name
- `component`: System component (llm, redis, compliance, etc.)
- `instance`: Service instance identifier

## Deployment

### Prometheus Configuration

Include the alerting rules in your Prometheus configuration:

```yaml
rule_files:
  - "alerting-rules.yml"

alerting:
  alertmanagers:
    - static_configs:
        - targets:
          - alertmanager:9093
```

### Kubernetes Deployment

Deploy using the provided ConfigMap:

```bash
kubectl apply -f prometheus-alerting-configmap.yaml
```

Then update your Prometheus deployment to mount the ConfigMap:

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: prometheus
spec:
  template:
    spec:
      containers:
      - name: prometheus
        volumeMounts:
        - name: alerting-rules
          mountPath: /etc/prometheus/alerting
      volumes:
      - name: alerting-rules
        configMap:
          name: regulens-prometheus-alerting
```

## Testing Alerts

### Manual Alert Testing

Test alerts using Prometheus query interface:

```bash
# Test circuit breaker alert
curl -X POST http://prometheus:9090/-/reload
# Then check alert status in UI
```

### Alert Validation

1. **False Positives**: Monitor for alerts that trigger incorrectly
2. **Missing Alerts**: Ensure critical conditions are properly detected
3. **Alert Quality**: Verify alert descriptions are clear and actionable

## Best Practices

### Alert Design
- **One alert per problem**: Don't create multiple alerts for the same issue
- **Actionable alerts**: Each alert should have a clear resolution path
- **Appropriate thresholds**: Set based on historical data and business requirements
- **Avoid alert fatigue**: Use appropriate severity levels and evaluation times

### Alert Maintenance
- **Regular review**: Quarterly review of alert effectiveness
- **Threshold tuning**: Adjust based on system changes and performance trends
- **Documentation**: Keep runbooks updated for all alerts
- **Testing**: Regularly test alert delivery and response procedures

### Runbooks

Each alert includes a `runbook_url` annotation pointing to detailed resolution procedures. Maintain these runbooks with:

- **Problem description**: What triggered the alert
- **Impact assessment**: Business and technical impact
- **Investigation steps**: How to diagnose the issue
- **Resolution procedures**: Step-by-step fix instructions
- **Prevention measures**: How to avoid future occurrences

## Troubleshooting

### Common Issues

1. **Alerts not firing**: Check Prometheus rule evaluation and target health
2. **False alerts**: Review alert expressions and thresholds
3. **Alert storms**: Implement alert aggregation and inhibition rules
4. **Missing labels**: Ensure metrics include required labels

### Debug Queries

```promql
# Check alert state
ALERTS{alertname="RegulensServiceDown"}

# Check metric availability
count(regulens_llm_requests_total)

# Test alert condition
rate(http_requests_total{status=~"5.."}[5m]) / rate(http_requests_total[5m]) * 100 > 10
```

## Integration

### Alert Manager Templates

Use these templates for consistent alert formatting:

```yaml
templates:
- '/etc/alertmanager/templates/regulens-alerts.tmpl'
```

### External Systems

Integrate with:
- **PagerDuty**: For critical alerts requiring immediate response
- **Slack/Teams**: For team notifications
- **JIRA/ServiceNow**: For incident tracking
- **Grafana**: For alert visualization and correlation

## Support

For alert configuration issues:
1. Check Prometheus and Alertmanager logs
2. Validate alert expressions in Prometheus UI
3. Test alert routing in Alertmanager
4. Review runbook accuracy and completeness
