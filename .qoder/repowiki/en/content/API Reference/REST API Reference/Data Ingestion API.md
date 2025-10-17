# Data Ingestion API

<cite>
**Referenced Files in This Document**   
- [ingestion_api_handlers.cpp](file://shared/data_ingestion/ingestion_api_handlers.cpp)
- [ingestion_api_handlers.hpp](file://shared/data_ingestion/ingestion_api_handlers.hpp)
- [data_quality_handlers.cpp](file://shared/data_quality/data_quality_handlers.cpp)
- [data_quality_handlers.hpp](file://shared/data_quality/data_quality_handlers.hpp)
- [data_ingestion_framework.cpp](file://shared/data_ingestion/data_ingestion_framework.cpp)
- [data_ingestion_framework.hpp](file://shared/data_ingestion/data_ingestion_framework.hpp)
- [standard_ingestion_pipeline.hpp](file://shared/data_ingestion/pipelines/standard_ingestion_pipeline.hpp)
- [useDataIngestion.ts](file://frontend/src/hooks/useDataIngestion.ts)
- [api.ts](file://frontend/src/services/api.ts)
- [DataIngestion.tsx](file://frontend/src/pages/DataIngestion.tsx)
- [DataQualityMonitor.tsx](file://frontend/src/pages/DataQualityMonitor.tsx)
</cite>

## Update Summary
**Changes Made**   
- Added comprehensive documentation for data quality monitoring endpoints
- Updated data validation endpoint to reflect actual implementation
- Added new section for data quality rules management
- Updated implementation details to reflect actual code structure
- Added new diagram showing data quality architecture

## Table of Contents
1. [Introduction](#introduction)
2. [Pipeline Management Endpoints](#pipeline-management-endpoints)
3. [Data Source Configuration Endpoints](#data-source-configuration-endpoints)
4. [Data Validation and Metrics Endpoints](#data-validation-and-metrics-endpoints)
5. [Data Quality Rules Management](#data-quality-rules-management)
6. [Request and Response Schemas](#request-and-response-schemas)
7. [Error Handling](#error-handling)
8. [Implementation Details](#implementation-details)

## Introduction
The Data Ingestion API provides a comprehensive interface for managing data ingestion pipelines, configuring data sources, and monitoring ingestion metrics. The API enables the creation, retrieval, updating, and deletion of ingestion pipelines, as well as configuration of various data sources including databases, REST APIs, and web scraping. It also provides endpoints for data validation and monitoring ingestion metrics.

The API is implemented in C++ with PostgreSQL as the backend database, and follows RESTful principles with JSON payloads. The implementation is production-grade with real database operations for pipeline monitoring and quality checks.

**Section sources**
- [ingestion_api_handlers.cpp](file://shared/data_ingestion/ingestion_api_handlers.cpp)
- [data_ingestion_framework.cpp](file://shared/data_ingestion/data_ingestion_framework.cpp)

## Pipeline Management Endpoints
The API provides CRUD operations for managing ingestion pipelines through the `/api/ingestion/pipelines` endpoint.

### List Pipelines (GET /api/ingestion/pipelines)
Retrieves a list of all ingestion pipelines.

**Request**
```
GET /api/ingestion/pipelines HTTP/1.1
Authorization: Bearer <token>
```

**Response**
```json
[
  {
    "id": "pipeline-1",
    "name": "Regulatory Monitoring Pipeline",
    "type": "web-scraping",
    "status": "active",
    "source": "regulatory-monitoring-source",
    "destination": "postgresql",
    "recordsProcessed": 1500,
    "recordsFailed": 5,
    "lastRun": "2023-06-15T10:30:00Z",
    "nextRun": "2023-06-15T11:30:00Z"
  }
]
```

### Get Pipeline (GET /ingestion/pipelines/{id})
Retrieves a specific ingestion pipeline by ID.

**Request**
```
GET /ingestion/pipelines/pipeline-1 HTTP/1.1
Authorization: Bearer <token>
```

**Response**
```json
{
  "id": "pipeline-1",
  "name": "Regulatory Monitoring Pipeline",
  "type": "web-scraping",
  "status": "active",
  "source": "regulatory-monitoring-source",
  "destination": "postgresql",
  "recordsProcessed": 1500,
  "recordsFailed": 5,
  "lastRun": "2023-06-15T10:30:00Z",
  "nextRun": "2023-06-15T11:30:00Z",
  "config": {
    "scheduling": {
      "interval": "hourly",
      "startTime": "00:00",
      "endTime": "23:59"
    },
    "errorHandling": {
      "retryAttempts": 3,
      "retryInterval": 300,
      "errorThreshold": 10
    },
    "transformationRules": [
      {
        "ruleName": "normalize_dates",
        "type": "date_format",
        "targetFormat": "ISO8601"
      }
    ]
  }
}
```

### Create Pipeline (POST /ingestion/pipelines)
Creates a new ingestion pipeline.

**Request**
```
POST /ingestion/pipelines HTTP/1.1
Authorization: Bearer <token>
Content-Type: application/json
```

```json
{
  "name": "New Database Pipeline",
  "type": "database",
  "source": "database-source-1",
  "destination": "postgresql",
  "config": {
    "scheduling": {
      "interval": "daily",
      "startTime": "02:00",
      "endTime": "03:00"
    },
    "errorHandling": {
      "retryAttempts": 3,
      "retryInterval": 300,
      "errorThreshold": 10
    },
    "transformationRules": [
      {
        "ruleName": "normalize_dates",
        "type": "date_format",
        "targetFormat": "ISO8601"
      }
    ]
  }
}
```

**Response**
```json
{
  "id": "pipeline-2",
  "name": "New Database Pipeline",
  "type": "database",
  "status": "active",
  "source": "database-source-1",
  "destination": "postgresql",
  "recordsProcessed": 0,
  "recordsFailed": 0,
  "lastRun": null,
  "nextRun": "2023-06-16T02:00:00Z"
}
```

### Update Pipeline (PUT /ingestion/pipelines/{id})
Updates an existing ingestion pipeline.

**Request**
```
PUT /ingestion/pipelines/pipeline-1 HTTP/1.1
Authorization: Bearer <token>
Content-Type: application/json
```

```json
{
  "name": "Updated Regulatory Monitoring Pipeline",
  "config": {
    "scheduling": {
      "interval": "hourly",
      "startTime": "00:00",
      "endTime": "23:59"
    },
    "errorHandling": {
      "retryAttempts": 5,
      "retryInterval": 600,
      "errorThreshold": 20
    },
    "transformationRules": [
      {
        "ruleName": "normalize_dates",
        "type": "date_format",
        "targetFormat": "ISO8601"
      },
      {
        "ruleName": "validate_emails",
        "type": "pattern_match",
        "pattern": "^[^@]+@[^@]+\\.[^@]+$"
      }
    ]
  }
}
```

**Response**
```json
{
  "id": "pipeline-1",
  "name": "Updated Regulatory Monitoring Pipeline",
  "type": "web-scraping",
  "status": "active",
  "source": "regulatory-monitoring-source",
  "destination": "postgresql",
  "recordsProcessed": 1500,
  "recordsFailed": 5,
  "lastRun": "2023-06-15T10:30:00Z",
  "nextRun": "2023-06-15T11:30:00Z"
}
```

### Delete Pipeline (DELETE /ingestion/pipelines/{id})
Deletes an ingestion pipeline.

**Request**
```
DELETE /ingestion/pipelines/pipeline-1 HTTP/1.1
Authorization: Bearer <token>
```

**Response**
```json
{
  "message": "Pipeline deleted successfully"
}
```

**Section sources**
- [useDataIngestion.ts](file://frontend/src/hooks/useDataIngestion.ts)
- [DataIngestion.tsx](file://frontend/src/pages/DataIngestion.tsx)

## Data Source Configuration Endpoints
The API provides endpoints for configuring different types of data sources.

### Database Source (POST /ingestion/sources/database)
Configures a database source for data ingestion.

**Request**
```
POST /ingestion/sources/database HTTP/1.1
Authorization: Bearer <token>
Content-Type: application/json
```

```json
{
  "sourceId": "database-source-1",
  "name": "Customer Database",
  "connection": {
    "host": "db.example.com",
    "port": 5432,
    "database": "customers",
    "username": "user",
    "password": "password",
    "ssl": true
  },
  "query": "SELECT * FROM transactions WHERE created_at > NOW() - INTERVAL '1 hour'",
  "pollInterval": 3600
}
```

**Response**
```json
{
  "sourceId": "database-source-1",
  "status": "configured",
  "message": "Database source configured successfully"
}
```

### REST API Source (POST /ingestion/sources/rest)
Configures a REST API source for data ingestion.

**Request**
```
POST /ingestion/sources/rest HTTP/1.1
Authorization: Bearer <token>
Content-Type: application/json
```

```json
{
  "sourceId": "api-source-1",
  "name": "External API",
  "url": "https://api.example.com/data",
  "method": "GET",
  "headers": {
    "Authorization": "Bearer api-key",
    "Content-Type": "application/json"
  },
  "pollInterval": 1800,
  "pagination": {
    "type": "offset",
    "limitParam": "limit",
    "offsetParam": "offset",
    "limit": 100
  }
}
```

**Response**
```json
{
  "sourceId": "api-source-1",
  "status": "configured",
  "message": "REST API source configured successfully"
}
```

### Web Scraping Source (POST /ingestion/sources/web-scraping)
Configures a web scraping source for data ingestion.

**Request**
```
POST /ingestion/sources/web-scraping HTTP/1.1
Authorization: Bearer <token>
Content-Type: application/json
```

```json
{
  "sourceId": "web-source-1",
  "name": "Web Scraping Source",
  "url": "https://example.com/news",
  "selectors": {
    "title": "h1.article-title",
    "content": "div.article-content",
    "publishedDate": "time.published"
  },
  "pollInterval": 7200,
  "rateLimit": {
    "requests": 10,
    "perSeconds": 60
  },
  "userAgent": "Regulens-Scraper/1.0"
}
```

**Response**
```json
{
  "sourceId": "web-source-1",
  "status": "configured",
  "message": "Web scraping source configured successfully"
}
```

**Section sources**
- [data_ingestion_framework.cpp](file://shared/data_ingestion/data_ingestion_framework.cpp)
- [data_ingestion_framework.hpp](file://shared/data_ingestion/data_ingestion_framework.hpp)

## Data Validation and Metrics Endpoints
The API provides endpoints for data validation and monitoring ingestion metrics.

### Data Validation (POST /ingestion/validate)
Validates data against defined rules before ingestion.

**Request**
```
POST /ingestion/validate HTTP/1.1
Authorization: Bearer <token>
Content-Type: application/json
```

```json
{
  "sourceId": "database-source-1",
  "data": [
    {
      "id": 1,
      "name": "John Doe",
      "email": "john.doe@example.com",
      "amount": 1000.50
    }
  ],
  "rules": [
    {
      "field": "email",
      "type": "pattern",
      "pattern": "^[^@]+@[^@]+\\.[^@]+$"
    },
    {
      "field": "amount",
      "type": "range",
      "min": 0,
      "max": 1000000
    }
  ]
}
```

**Response**
```json
{
  "valid": true,
  "errors": [],
  "warnings": [
    {
      "recordId": 1,
      "field": "amount",
      "message": "Amount is close to maximum allowed value"
    }
  ],
  "statistics": {
    "totalRecords": 1,
    "validRecords": 1,
    "invalidRecords": 0,
    "warningRecords": 1
  }
}
```

### Ingestion Metrics (GET /ingestion/metrics)
Retrieves ingestion metrics for monitoring pipeline performance.

**Request**
```
GET /ingestion/metrics?source=regulatory-monitoring-source&time_range=24h&limit=100 HTTP/1.1
Authorization: Bearer <token>
```

**Response**
```json
{
  "metrics": [
    {
      "id": "metric-1",
      "sourceName": "regulatory-monitoring-source",
      "sourceType": "web-scraping",
      "pipelineName": "regulatory-monitoring-pipeline",
      "timestamp": "2023-06-15T10:30:00Z",
      "recordsIngested": 100,
      "recordsFailed": 0,
      "recordsSkipped": 5,
      "recordsUpdated": 10,
      "recordsDeleted": 0,
      "bytesProcessed": 153600,
      "durationMs": 2500,
      "throughputRecordsPerSec": 40,
      "throughputMbPerSec": 0.06,
      "errorCount": 0,
      "errorMessages": [],
      "warningCount": 2,
      "status": "success",
      "lagSeconds": 30,
      "batchId": "batch-123",
      "executionHost": "ingestion-worker-1",
      "memoryUsedMb": 256,
      "cpuUsagePercent": 45.5
    }
  ],
  "summary": {
    "totalRuns": 24,
    "totalRecords": 2400,
    "totalFailed": 12,
    "totalBytes": 3686400,
    "avgDurationMs": 2800,
    "avgThroughput": 42.5,
    "successfulRuns": 22,
    "failedRuns": 2,
    "avgLagSeconds": 45,
    "successRate": 0.9167
  },
  "sources": [
    {
      "name": "regulatory-monitoring-source",
      "type": "web-scraping",
      "runCount": 24,
      "totalRecords": 2400,
      "avgThroughput": 42.5,
      "lastRun": "2023-06-15T10:30:00Z"
    }
  ],
  "timeRange": "24h",
  "count": 1
}
```

### Data Quality Checks (GET /ingestion/quality-checks)
Retrieves data quality check results for monitoring data integrity.

**Request**
```
GET /ingestion/quality-checks?source=transactions&table=transactions&failed_only=true&limit=100 HTTP/1.1
Authorization: Bearer <token>
```

**Response**
```json
{
  "checks": [
    {
      "id": "check-1",
      "sourceName": "transactions",
      "tableName": "transactions",
      "columnName": "amount",
      "checkType": "range",
      "checkName": "Transaction Amount Range Check",
      "description": "Ensures transaction amounts are within acceptable range",
      "executedAt": "2023-06-15T10:30:00Z",
      "passed": false,
      "qualityScore": 95.5,
      "recordsChecked": 1000,
      "recordsPassed": 955,
      "recordsFailed": 45,
      "nullCount": 0,
      "failureRate": 0.045,
      "failureExamples": [
        {
          "record_id": "txn-123",
          "amount": 1500000,
          "currency": "USD",
          "error": "Amount exceeds maximum allowed value of 1000000"
        }
      ],
      "severity": "critical",
      "thresholdMin": 0,
      "thresholdMax": 1000000,
      "measuredValue": 1500000,
      "expectedValue": 1000000,
      "deviation": 50,
      "recommendation": "Review transaction validation rules",
      "remediationAction": "Implement automated alerting for high-value transactions",
      "remediationStatus": "pending"
    }
  ],
  "summary": {
    "totalChecks": 25,
    "passedChecks": 20,
    "failedChecks": 5,
    "criticalIssues": 3,
    "highIssues": 2,
    "avgQualityScore": 92.3,
    "passRate": 0.8
  },
  "tables": [
    {
      "tableName": "transactions",
      "sourceName": "transactions",
      "overallScore": 95.5,
      "completeness": 98.0,
      "accuracy": 96.0,
      "validity": 94.0,
      "totalRecords": 10000,
      "issuesCount": 5,
      "criticalIssues": 3,
      "lastChecked": "2023-06-15T10:30:00Z"
    }
  ],
  "count": 1
}
```

**Section sources**
- [ingestion_api_handlers.cpp](file://shared/data_ingestion/ingestion_api_handlers.cpp)
- [data_quality_handlers.cpp](file://shared/data_quality/data_quality_handlers.cpp)
- [DataQualityMonitor.tsx](file://frontend/src/pages/DataQualityMonitor.tsx)

## Data Quality Rules Management
The API provides comprehensive endpoints for managing data quality rules that define validation criteria for ingested data.

### List Quality Rules (GET /ingestion/quality-rules)
Retrieves a list of all data quality rules.

**Request**
```
GET /ingestion/quality-rules HTTP/1.1
Authorization: Bearer <token>
```

**Response**
```json
{
  "success": true,
  "data": [
    {
      "rule_id": "rule-1",
      "rule_name": "Transaction Amount Range Check",
      "data_source": "transactions",
      "rule_type": "range",
      "validation_logic": {
        "field": "amount",
        "min": 0,
        "max": 1000000
      },
      "severity": "critical",
      "is_enabled": true,
      "created_at": "2023-06-01T08:00:00Z"
    },
    {
      "rule_id": "rule-2",
      "rule_name": "Customer Email Format Validation",
      "data_source": "customers",
      "rule_type": "pattern",
      "validation_logic": {
        "field": "email",
        "pattern": "^[^@]+@[^@]+\\.[^@]+$"
      },
      "severity": "high",
      "is_enabled": true,
      "created_at": "2023-06-01T08:00:00Z"
    }
  ]
}
```

### Create Quality Rule (POST /ingestion/quality-rules)
Creates a new data quality rule.

**Request**
```
POST /ingestion/quality-rules HTTP/1.1
Authorization: Bearer <token>
Content-Type: application/json
```

```json
{
  "rule_name": "Transaction Currency Validation",
  "data_source": "transactions",
  "rule_type": "enum",
  "validation_logic": {
    "field": "currency",
    "valid_values": ["USD", "EUR", "GBP", "JPY"]
  },
  "severity": "high",
  "is_enabled": true
}
```

**Response**
```json
{
  "success": true,
  "data": {
    "rule_id": "rule-3",
    "message": "Data quality rule created successfully"
  }
}
```

### Get Quality Rule (GET /ingestion/quality-rules/{rule_id})
Retrieves a specific data quality rule by ID.

**Request**
```
GET /ingestion/quality-rules/rule-1 HTTP/1.1
Authorization: Bearer <token>
```

**Response**
```json
{
  "success": true,
  "data": {
    "rule_id": "rule-1",
    "rule_name": "Transaction Amount Range Check",
    "data_source": "transactions",
    "rule_type": "range",
    "validation_logic": {
      "field": "amount",
      "min": 0,
      "max": 1000000
    },
    "severity": "critical",
    "is_enabled": true,
    "created_at": "2023-06-01T08:00:00Z"
  }
}
```

### Update Quality Rule (PUT /ingestion/quality-rules/{rule_id})
Updates an existing data quality rule.

**Request**
```
PUT /ingestion/quality-rules/rule-1 HTTP/1.1
Authorization: Bearer <token>
Content-Type: application/json
```

```json
{
  "rule_name": "Updated Transaction Amount Range Check",
  "validation_logic": {
    "field": "amount",
    "min": 0,
    "max": 1500000
  },
  "severity": "critical",
  "is_enabled": true
}
```

**Response**
```json
{
  "success": true,
  "data": {
    "rule_id": "rule-1",
    "message": "Data quality rule updated successfully"
  }
}
```

### Delete Quality Rule (DELETE /ingestion/quality-rules/{rule_id})
Deletes a data quality rule.

**Request**
```
DELETE /ingestion/quality-rules/rule-1 HTTP/1.1
Authorization: Bearer <token>
```

**Response**
```json
{
  "success": true,
  "data": {
    "rule_id": "rule-1",
    "message": "Data quality rule deleted successfully"
  }
}
```

### Run Quality Check (POST /ingestion/quality-rules/{rule_id}/run)
Executes a quality check for a specific rule.

**Request**
```
POST /ingestion/quality-rules/rule-1/run HTTP/1.1
Authorization: Bearer <token>
```

**Response**
```json
{
  "success": true,
  "data": {
    "check_id": "check-1",
    "rule_id": "rule-1",
    "rule_name": "Transaction Amount Range Check",
    "records_checked": 1000,
    "records_passed": 955,
    "records_failed": 45,
    "quality_score": 95.5,
    "status": "failed",
    "execution_time_ms": 250,
    "message": "Data quality check completed successfully"
  }
}
```

**Section sources**
- [data_quality_handlers.cpp](file://shared/data_quality/data_quality_handlers.cpp)
- [data_quality_handlers.hpp](file://shared/data_quality/data_quality_handlers.hpp)

## Request and Response Schemas
This section defines the schemas for request and response payloads used in the Data Ingestion API.

### Pipeline Configuration Schema
The pipeline configuration includes scheduling, error handling, and transformation rules.

```json
{
  "scheduling": {
    "interval": "string",
    "startTime": "string",
    "endTime": "string"
  },
  "errorHandling": {
    "retryAttempts": "integer",
    "retryInterval": "integer",
    "errorThreshold": "integer"
  },
  "transformationRules": [
    {
      "ruleName": "string",
      "type": "string",
      "parameters": {}
    }
  ]
}
```

### Pipeline Status Values
The possible status values for ingestion pipelines are:
- `active`: Pipeline is running and processing data
- `paused`: Pipeline is temporarily stopped
- `error`: Pipeline encountered an error
- `stopped`: Pipeline has been stopped

### Source Configuration Schemas
Different source types have specific configuration requirements:

**Database Source**
```json
{
  "sourceId": "string",
  "name": "string",
  "connection": {
    "host": "string",
    "port": "integer",
    "database": "string",
    "username": "string",
    "password": "string",
    "ssl": "boolean"
  },
  "query": "string",
  "pollInterval": "integer"
}
```

**REST API Source**
```json
{
  "sourceId": "string",
  "name": "string",
  "url": "string",
  "method": "string",
  "headers": {},
  "pollInterval": "integer",
  "pagination": {
    "type": "string",
    "limitParam": "string",
    "offsetParam": "string",
    "limit": "integer"
  }
}
```

**Web Scraping Source**
```json
{
  "sourceId": "string",
  "name": "string",
  "url": "string",
  "selectors": {},
  "pollInterval": "integer",
  "rateLimit": {
    "requests": "integer",
    "perSeconds": "integer"
  },
  "userAgent": "string"
}
```

### Data Quality Rule Schema
The data quality rule schema defines validation criteria for data integrity.

```json
{
  "rule_id": "string",
  "rule_name": "string",
  "data_source": "string",
  "rule_type": "string",
  "validation_logic": {
    "field": "string",
    "min": "number",
    "max": "number",
    "pattern": "string",
    "valid_values": ["string"]
  },
  "severity": "string",
  "is_enabled": "boolean",
  "created_at": "string"
}
```

### Data Quality Check Schema
The data quality check schema defines the results of quality validation.

```json
{
  "id": "string",
  "sourceName": "string",
  "tableName": "string",
  "columnName": "string",
  "checkType": "string",
  "checkName": "string",
  "description": "string",
  "executedAt": "string",
  "passed": "boolean",
  "qualityScore": "number",
  "recordsChecked": "integer",
  "recordsPassed": "integer",
  "recordsFailed": "integer",
  "nullCount": "integer",
  "failureRate": "number",
  "failureExamples": [
    {
      "record_id": "string",
      "field": "string",
      "value": "any",
      "error": "string"
    }
  ],
  "severity": "string",
  "thresholdMin": "number",
  "thresholdMax": "number",
  "measuredValue": "number",
  "expectedValue": "number",
  "deviation": "number",
  "recommendation": "string",
  "remediationAction": "string",
  "remediationStatus": "string"
}
```

**Section sources**
- [standard_ingestion_pipeline.hpp](file://shared/data_ingestion/pipelines/standard_ingestion_pipeline.hpp)
- [api.ts](file://frontend/src/types/api.ts)

## Error Handling
The API follows standard HTTP status codes for error handling:

- `200 OK`: Successful GET requests
- `201 Created`: Successful POST requests
- `204 No Content`: Successful DELETE requests
- `400 Bad Request`: Invalid request parameters or body
- `401 Unauthorized`: Missing or invalid authentication token
- `403 Forbidden`: Insufficient permissions
- `404 Not Found`: Resource not found
- `500 Internal Server Error`: Server-side error

Error responses follow the standard error format:

```json
{
  "error": "string",
  "message": "string",
  "code": "string",
  "timestamp": "string"
}
```

**Section sources**
- [api.ts](file://frontend/src/services/api.ts)
- [ingestion_api_handlers.cpp](file://shared/data_ingestion/ingestion_api_handlers.cpp)

## Implementation Details
The Data Ingestion API is implemented in C++ with a modular architecture that separates concerns between data source management, pipeline processing, and storage. The implementation uses PostgreSQL as the backend database for storing ingestion metrics and pipeline configurations.

The core implementation is in the `DataIngestionFramework` class, which manages the lifecycle of ingestion pipelines and coordinates between data sources, processing pipelines, and storage adapters. The framework supports multiple worker threads for parallel processing of ingestion batches.

The API handlers are implemented in the `ingestion_api_handlers.cpp` file, which provides the HTTP interface for the ingestion system. These handlers use the data ingestion framework to perform operations and return JSON responses.

The pipeline configuration is defined in the `PipelineConfig` struct, which includes settings for validation rules, transformations, enrichment rules, duplicate detection, compliance checking, batch size, processing timeout, and error recovery.

```mermaid
classDiagram
class DataIngestionFramework {
+initialize() bool
+shutdown() void
+is_running() bool
+register_data_source(config) bool
+unregister_data_source(source_id) bool
+list_data_sources() vector~string~
+get_source_config(source_id) optional~DataIngestionConfig~
+start_ingestion(source_id) bool
+stop_ingestion(source_id) bool
+pause_ingestion(source_id) bool
+resume_ingestion(source_id) bool
+ingest_data(source_id, data) vector~DataRecord~
+process_batch(batch) vector~DataRecord~
+store_records(records) bool
+query_records(source_id, start, end) vector~DataRecord~
+get_ingestion_stats(source_id) json
+get_framework_health() json
}
class IngestionPipeline {
<<abstract>>
+validate_batch(batch) bool
+process_batch(raw_data) IngestionBatch
+apply_validation_rules(data) vector~DataRecord~
+apply_transformation_rules(data) vector~DataRecord~
+apply_enrichment_rules(data) vector~DataRecord~
}
class StandardIngestionPipeline {
+StandardIngestionPipeline(config, logger)
+~StandardIngestionPipeline()
+validate_batch(batch) bool
+process_batch(raw_data) IngestionBatch
}
class DataSource {
<<abstract>>
+connect() bool
+disconnect() bool
+is_connected() bool
+validate_connection() bool
+fetch_data() vector~nlohmann : : json~
}
class RESTAPISource {
+RESTAPISource(config, http_client, logger)
+connect() bool
+fetch_data() vector~nlohmann : : json~
}
class WebScrapingSource {
+WebScrapingSource(config, http_client, logger)
+connect() bool
+fetch_data() vector~nlohmann : : json~
}
class DatabaseSource {
+DatabaseSource(config, db_pool, logger)
+connect() bool
+fetch_data() vector~nlohmann : : json~
}
class StorageAdapter {
<<abstract>>
+store_batch(batch) bool
+retrieve_records(source_id, start, end) vector~DataRecord~
}
class PostgreSQLStorageAdapter {
+PostgreSQLStorageAdapter(db_pool, logger)
+store_batch(batch) bool
+retrieve_records(source_id, start, end) vector~DataRecord~
}
DataIngestionFramework --> IngestionPipeline : "uses"
DataIngestionFramework --> DataSource : "uses"
DataIngestionFramework --> StorageAdapter : "uses"
IngestionPipeline <|-- StandardIngestionPipeline
DataSource <|-- RESTAPISource
DataSource <|-- WebScrapingSource
DataSource <|-- DatabaseSource
StorageAdapter <|-- PostgreSQLStorageAdapter
```

The data quality monitoring system is implemented in the `DataQualityHandlers` class, which provides comprehensive endpoints for managing data quality rules and monitoring data integrity. The system supports various types of quality checks including completeness, accuracy, consistency, timeliness, and validity.

```mermaid
classDiagram
class DataQualityHandlers {
+list_quality_rules(headers) string
+create_quality_rule(body, headers) string
+get_quality_rule(rule_id, headers) string
+update_quality_rule(rule_id, body, headers) string
+delete_quality_rule(rule_id, headers) string
+get_quality_checks(headers) string
+run_quality_check(rule_id, headers) string
+get_quality_dashboard(headers) string
+get_check_history(headers) string
+get_quality_metrics(headers) string
+get_quality_trends(headers) string
}
class DataQualityRules {
+rule_id string
+rule_name string
+data_source string
+rule_type string
+validation_logic json
+severity string
+is_enabled bool
+created_at timestamp
}
class DataQualityChecks {
+check_id string
+rule_id string
+check_timestamp timestamp
+records_checked int
+records_passed int
+records_failed int
+quality_score double
+execution_time_ms int
+status string
+failed_records json
}
class DataQualityMetrics {
+total_rules int
+enabled_rules int
+total_checks int
+avg_quality_score double
+checks_by_type json
+quality_trends json
}
DataQualityHandlers --> DataQualityRules : "manages"
DataQualityHandlers --> DataQualityChecks : "executes"
DataQualityHandlers --> DataQualityMetrics : "aggregates"
DataQualityRules <|-- DataQualityChecks : "defines"
DataQualityChecks <|-- DataQualityMetrics : "contributes to"
```

**Diagram sources**
- [data_ingestion_framework.cpp](file://shared/data_ingestion/data_ingestion_framework.cpp)
- [data_ingestion_framework.hpp](file://shared/data_ingestion/data_ingestion_framework.hpp)
- [standard_ingestion_pipeline.hpp](file://shared/data_ingestion/pipelines/standard_ingestion_pipeline.hpp)
- [data_quality_handlers.cpp](file://shared/data_quality/data_quality_handlers.cpp)
- [data_quality_handlers.hpp](file://shared/data_quality/data_quality_handlers.hpp)

**Section sources**
- [data_ingestion_framework.cpp](file://shared/data_ingestion/data_ingestion_framework.cpp)
- [ingestion_api_handlers.cpp](file://shared/data_ingestion/ingestion_api_handlers.cpp)
- [data_quality_handlers.cpp](file://shared/data_quality/data_quality_handlers.cpp)