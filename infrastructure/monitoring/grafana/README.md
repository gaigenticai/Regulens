# Regulens Grafana Dashboards

This directory contains comprehensive Grafana dashboards for monitoring the Regulens enterprise compliance platform.

## Dashboard Overview

### 1. System Overview (`system-overview.json`)
- Overall system health and performance metrics
- CPU, memory, and network utilization
- Request rates and error percentages
- Health check status summary

### 2. Circuit Breaker Monitoring (`circuit-breaker-monitoring.json`)
- Circuit breaker states (CLOSED/OPEN/HALF_OPEN)
- Failure rates and success rates
- State transition monitoring
- Response time percentiles
- Circuit breaker status table

### 3. LLM Performance (`llm-performance.json`)
- OpenAI and Anthropic API performance
- Request rates and success rates
- Token usage and cost monitoring
- Cache hit rates and response times
- Rate limiting and error tracking

### 4. Redis Cache Performance (`redis-cache-performance.json`)
- Redis connection health and operations
- Cache hit/miss rates and performance
- Connection pool utilization
- Memory usage and eviction metrics
- Cache operations by type (LLM, regulatory, session)

### 5. Compliance Operations (`compliance-operations.json`)
- Compliance decision rates and accuracy
- Processing time percentiles
- Risk assessment distribution
- Regulatory coverage by jurisdiction
- SLA compliance metrics

### 6. Kubernetes Operators (`kubernetes-operators.json`)
- Operator health and resource counts
- Agent orchestrator and compliance agent metrics
- Data source processing rates
- Reconciliation performance
- Kubernetes API usage

### 7. Regulatory Data Ingestion (`regulatory-data-ingestion.json`)
- Data source health and processing rates
- Document and data volume metrics
- Ingestion success rates and response times
- Cache effectiveness and data quality
- Scaling events and compliance coverage

## Installation

### Prerequisites
- Grafana 8.0+
- Prometheus data source configured
- Regulens metrics exported to Prometheus

### Import Dashboards

1. Open Grafana in your web browser
2. Navigate to **Dashboards** → **Import**
3. Click **Upload JSON file**
4. Select one of the dashboard JSON files from this directory
5. Configure the data source (should be your Prometheus instance)
6. Click **Import**

### Alternative Import via API

```bash
# Import all dashboards via Grafana API
for dashboard in dashboards/*.json; do
  curl -X POST -H "Content-Type: application/json" \
       -d @"$dashboard" \
       http://your-grafana-host:3000/api/dashboards/import
done
```

## Configuration

### Data Source Setup

Ensure your Prometheus data source is configured with:
- **URL**: `http://prometheus:9090`
- **Access**: Server (default) or Browser
- **Type**: Prometheus

### Metric Labels

The dashboards use the following common metric labels:
- `job`: Service name (e.g., `regulens`, `regulens-operator`)
- `instance`: Service instance identifier
- `provider`: LLM provider (e.g., `openai`, `anthropic`)
- `source`: Data source (e.g., `sec_edgar`, `fca`, `ecb`)
- `cache_type`: Cache type (e.g., `llm`, `regulatory`, `session`)
- `resource_type`: Kubernetes resource type
- `jurisdiction`: Regulatory jurisdiction

### Alert Integration

These dashboards are designed to work with Prometheus alerting rules. See the accompanying `alerting/` directory for recommended alert configurations.

## Customization

### Adding New Metrics

1. Ensure your Prometheus metrics are properly labeled
2. Add new panels to existing dashboards or create new ones
3. Test queries in Prometheus before adding to dashboards
4. Use consistent naming conventions

### Dashboard Variables

Most dashboards include template variables for filtering:
- `provider`: Filter by LLM provider
- `source`: Filter by data source
- `cache_type`: Filter by cache type

## Monitoring Best Practices

### Key Metrics to Monitor

1. **System Health**
   - Service uptime and availability
   - Resource utilization (CPU, memory, disk)
   - Error rates and response times

2. **Circuit Breaker Health**
   - State transitions and failure rates
   - API response times and success rates

3. **LLM Performance**
   - API costs and token usage
   - Cache effectiveness
   - Rate limiting status

4. **Cache Performance**
   - Hit rates and response times
   - Memory usage and evictions
   - Connection pool health

5. **Compliance Operations**
   - Decision accuracy and processing times
   - Risk assessment coverage
   - SLA compliance

6. **Data Ingestion**
   - Processing rates and success rates
   - Data quality and transformation performance
   - Source health and scaling

### Alert Thresholds

Recommended alert thresholds (adjust based on your environment):
- Service availability: < 99.9%
- Error rate: > 5%
- Response time P95: > 5 seconds
- Cache hit rate: < 80%
- Memory usage: > 85%

## Troubleshooting

### Common Issues

1. **Metrics not appearing**: Check Prometheus target health and metric names
2. **Dashboard variables empty**: Ensure proper metric labeling in your services
3. **High cardinality**: Review metric label usage to avoid performance issues

### Debug Queries

Test metric availability in Prometheus:
```promql
# Check if metrics are being collected
count(regulens_llm_requests_total)

# Check metric labels
count by (provider) (regulens_llm_requests_total)

# Check time series health
up{job="regulens"}
```

## Support

For issues with these dashboards:
1. Check Grafana logs for import errors
2. Verify Prometheus metric availability
3. Review metric naming and labeling
4. Ensure dashboard JSON syntax is valid
