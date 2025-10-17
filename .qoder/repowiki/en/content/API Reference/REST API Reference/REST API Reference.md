# REST API Reference

<cite>
**Referenced Files in This Document**   
- [auth_api_handlers.cpp](file://shared/auth/auth_api_handlers.cpp)
- [decision_api_handlers.cpp](file://shared/decisions/decision_api_handlers.cpp)
- [knowledge_api_handlers.cpp](file://shared/knowledge_base/knowledge_api_handlers.cpp)
- [llm_api_handlers.cpp](file://shared/llm/llm_api_handlers.cpp)
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp)
- [error_handling_config.json](file://shared/api_config/error_handling_config.json)
- [api_endpoints_config.json](file://shared/api_config/api_endpoints_config.json)
- [openapi_generator.cpp](file://shared/api_docs/openapi_generator.cpp)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp)
</cite>

## Table of Contents
1. [Introduction](#introduction)
2. [Authentication](#authentication)
3. [Decision Engine](#decision-engine)
4. [Knowledge Base](#knowledge-base)
5. [LLM Integration](#llm-integration)
6. [Regulatory Monitor](#regulatory-monitor)
7. [Error Handling](#error-handling)
8. [Rate Limiting and CORS](#rate-limiting-and-cors)

## Introduction
This document provides comprehensive REST API documentation for all Regulens endpoints. The API follows OpenAPI 3.0 specification format and implements JWT-based authentication for secure access. The system is organized into major modules including agent management, decision engine, regulatory monitor, rule engine, knowledge base, and LLM integration.

All endpoints require authentication using JWT tokens, except for public endpoints like login and health checks. The API uses standard HTTP status codes and returns JSON responses with consistent error formatting. Request and response bodies follow specific schemas defined in the OpenAPI specification.

**Section sources**
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L320-L356)
- [api_endpoints_config.json](file://shared/api_config/api_endpoints_config.json#L1-L921)

## Authentication
The authentication system implements JWT-based authentication with secure token management, including login, logout, token refresh, and user profile retrieval.

### POST /api/auth/login
Authenticate user and generate JWT tokens.

**Request Parameters**
- Body (application/json): 
  - username (string, required): User's username
  - password (string, required): User's password

**Response (200 OK)**
```json
{
  "accessToken": "string",
  "refreshToken": "string",
  "tokenType": "Bearer",
  "expiresIn": 86400,
  "user": {
    "id": "string",
    "username": "string",
    "roles": ["string"],
    "permissions": ["string"]
  }
}
```

**Error Responses**
- 400 Bad Request: Missing required fields
- 401 Unauthorized: Invalid username or password
- 403 Forbidden: Account disabled or locked

**Business Logic**
Validates credentials against the user_authentication table, verifies password using secure hashing, generates JWT access token with 24-hour expiration, generates refresh token with 30-day expiration, stores refresh token in database, resets failed login attempts counter, and returns user profile with roles and permissions.

**Example Request**
```json
{
  "username": "john_doe",
  "password": "secure_password"
}
```

**Section sources**
- [auth_api_handlers.cpp](file://shared/auth/auth_api_handlers.cpp#L64-L184)

### POST /api/auth/logout
Logout user and revoke refresh token.

**Request Parameters**
- Headers:
  - Authorization (string): Bearer token (refresh token)

**Response (200 OK)**
```json
{
  "message": "Logged out successfully"
}
```

**Error Responses**
- 400 Bad Request: No refresh token provided
- 401 Unauthorized: Invalid or missing authentication token

**Business Logic**
Extracts refresh token from authorization header, revokes the refresh token by marking it as revoked in the database, and logs the logout activity.

**Section sources**
- [auth_api_handlers.cpp](file://shared/auth/auth_api_handlers.cpp#L186-L222)

### GET /api/auth/me
Get current user profile.

**Request Parameters**
- Headers:
  - Authorization (string, required): Bearer token (access token)

**Response (200 OK)**
```json
{
  "id": "string",
  "username": "string",
  "email": "string",
  "isActive": true,
  "createdAt": "string",
  "lastLoginAt": "string",
  "failedLoginAttempts": 0,
  "roles": ["string"]
}
```

**Error Responses**
- 401 Unauthorized: Invalid or missing authentication token
- 404 Not Found: User not found

**Business Logic**
Validates JWT access token, extracts user ID from token claims, queries user profile from database, and returns user information including roles and login statistics.

**Section sources**
- [auth_api_handlers.cpp](file://shared/auth/auth_api_handlers.cpp#L224-L278)

### POST /api/auth/refresh
Refresh JWT access token using refresh token.

**Request Parameters**
- Body (application/json):
  - refreshToken (string, required): Refresh token

**Response (200 OK)**
```json
{
  "accessToken": "string",
  "refreshToken": "string",
  "tokenType": "Bearer",
  "expiresIn": 86400
}
```

**Error Responses**
- 400 Bad Request: Missing refresh token
- 401 Unauthorized: Invalid or expired refresh token
- 403 Forbidden: Account disabled

**Business Logic**
Validates refresh token against database, checks if token is revoked or expired, generates new access token with 24-hour expiration, generates new refresh token, revokes old refresh token, stores new refresh token in database, and returns new tokens.

**Section sources**
- [auth_api_handlers.cpp](file://shared/auth/auth_api_handlers.cpp#L280-L358)

## Decision Engine
The decision engine provides endpoints for creating, retrieving, and visualizing decisions using multi-criteria decision analysis (MCDA).

### GET /api/decisions/tree
Retrieve decision trees with MCDA analysis.

**Request Parameters**
- Query:
  - decisionId (string, optional): Specific decision ID to retrieve
  - includeAnalysis (string, optional): Whether to include MCDA analysis (default: true)

**Response (200 OK)**
```json
{
  "decisionId": "string",
  "type": "string",
  "description": "string",
  "context": {},
  "agentId": "string",
  "confidenceScore": 0.95,
  "createdAt": "string",
  "updatedAt": "string",
  "treeNodes": [
    {
      "nodeId": "string",
      "parentNodeId": "string",
      "nodeType": "string",
      "nodeLabel": "string",
      "nodeValue": "string",
      "nodePosition": "string",
      "level": 0
    }
  ],
  "criteria": [
    {
      "name": "string",
      "weight": 0.5,
      "type": "string",
      "description": "string"
    }
  ],
  "alternatives": [
    {
      "name": "string",
      "scores": {},
      "totalScore": 0.8,
      "ranking": 1,
      "selected": true
    }
  ],
  "analysisMethod": "MCDA",
  "optimizerVersion": "1.0"
}
```

**Error Responses**
- 400 Bad Request: Invalid request parameters
- 500 Internal Server Error: Database query failed

**Business Logic**
Queries decision data from database, retrieves decision tree structure with nodes, fetches criteria and alternatives for MCDA analysis, enhances with DecisionTreeOptimizer analysis if requested, and returns comprehensive decision tree with analysis metadata.

**Section sources**
- [decision_api_handlers.cpp](file://shared/decisions/decision_api_handlers.cpp#L100-L250)

### POST /api/decisions/visualize
Generate decision visualization.

**Request Parameters**
- Body (application/json):
  - decisionId (string, required): Decision ID to visualize
  - format (string, optional): Output format (json, graphviz, d3; default: json)
  - includeScores (boolean, optional): Whether to include scores (default: true)
  - includeMetadata (boolean, optional): Whether to include metadata (default: false)

**Response (200 OK)**
```json
{
  "format": "string",
  "decisionId": "string",
  "alternatives": [
    {
      "id": "string",
      "name": "string",
      "scores": {}
    }
  ],
  "metadata": {
    "generatedAt": 0,
    "generatedBy": "string",
    "engine": "string",
    "version": "string"
  }
}
```

**Error Responses**
- 400 Bad Request: Missing decisionId
- 404 Not Found: Decision not found
- 500 Internal Server Error: Visualization generation failed

**Business Logic**
Retrieves decision data from database, fetches alternatives with their scores, uses DecisionTreeOptimizer for sophisticated visualization if available, generates visualization data in requested format, includes metadata if requested, and returns visualization data.

**Section sources**
- [decision_api_handlers.cpp](file://shared/decisions/decision_api_handlers.cpp#L252-L380)

### POST /api/decisions
Create decision with multi-criteria analysis.

**Request Parameters**
- Body (application/json):
  - problem (string, required): Decision problem description
  - alternatives (array, required): List of alternatives
  - method (string, optional): MCDA method (WEIGHTED_SUM, WEIGHTED_PRODUCT, TOPSIS, etc.; default: WEIGHTED_SUM)
  - context (string, optional): Context for AI analysis
  - useAI (boolean, optional): Whether to use AI for recommendation (default: false)

**Response (201 Created)**
```json
{
  "decisionId": "string",
  "problem": "string",
  "method": "string",
  "recommendedAlternative": "string",
  "ranking": ["string"],
  "scores": {},
  "sensitivityAnalysis": {},
  "aiAnalysis": {},
  "createdAt": 0,
  "createdBy": "string"
}
```

**Error Responses**
- 400 Bad Request: Missing required fields
- 500 Internal Server Error: Failed to create decision

**Business Logic**
Validates required fields, parses MCDA method, creates decision analysis result, uses DecisionTreeOptimizer for MCDA analysis, performs sensitivity analysis if configured, persists decision to database with criteria and alternatives, and returns decision details with analysis results.

**Section sources**
- [decision_api_handlers.cpp](file://shared/decisions/decision_api_handlers.cpp#L382-L541)

## Knowledge Base
The knowledge base provides endpoints for managing knowledge entries, cases, and RAG-based Q&A.

### POST /api/knowledge/entries
Create knowledge entry with automatic embedding generation.

**Request Parameters**
- Body (application/json):
  - title (string, required): Entry title
  - content (string, required): Entry content
  - category (string, optional): Category (default: general)
  - tags (array, optional): Tags array
  - metadata (object, optional): Metadata object

**Response (201 Created)**
```json
{
  "entryId": "string",
  "title": "string",
  "category": "string",
  "createdAt": "string",
  "createdBy": "string",
  "embeddingsGenerated": true
}
```

**Error Responses**
- 400 Bad Request: Missing required fields
- 500 Internal Server Error: Failed to create entry

**Business Logic**
Validates required fields, stores content in KnowledgeBase, inserts entry into database, generates embeddings using EmbeddingsClient, stores embeddings in database, and returns entry details with creation metadata.

**Section sources**
- [knowledge_api_handlers.cpp](file://shared/knowledge_base/knowledge_api_handlers.cpp#L100-L180)

### PUT /api/knowledge/entries/{id}
Update knowledge entry and regenerate embeddings.

**Request Parameters**
- Path:
  - id (string, required): Entry ID to update
- Body (application/json):
  - title (string, optional): New title
  - content (string, optional): New content
  - category (string, optional): New category

**Response (200 OK)**
```json
{
  "entryId": "string",
  "updated": true,
  "embeddingsRegenerated": true
}
```

**Error Responses**
- 400 Bad Request: No fields to update
- 404 Not Found: Entry not found
- 500 Internal Server Error: Failed to update entry

**Business Logic**
Builds dynamic UPDATE query based on provided fields, updates entry in database, updates content in KnowledgeBase if content changed, deletes old embeddings if content changed, generates new embeddings, stores new embeddings in database, and returns update status.

**Section sources**
- [knowledge_api_handlers.cpp](file://shared/knowledge_base/knowledge_api_handlers.cpp#L182-L280)

### DELETE /api/knowledge/entries/{id}
Delete knowledge entry and cleanup embeddings.

**Request Parameters**
- Path:
  - id (string, required): Entry ID to delete

**Response (200 OK)**
```json
{
  "entryId": "string",
  "deleted": true
}
```

**Error Responses**
- 404 Not Found: Entry not found
- 500 Internal Server Error: Failed to delete entry

**Business Logic**
Deletes entry from database (CASCADE handles embeddings and relationships), returns deletion status.

**Section sources**
- [knowledge_api_handlers.cpp](file://shared/knowledge_base/knowledge_api_handlers.cpp#L282-L320)

### GET /api/knowledge/entries/{entryId}/similar
Find similar entries using vector similarity search.

**Request Parameters**
- Path:
  - entryId (string, required): Entry ID to find similar entries for
- Query:
  - limit (integer, optional): Maximum number of results (default: 10)
  - minSimilarity (number, optional): Minimum similarity score (default: 0.7)

**Response (200 OK)**
```json
{
  "entryId": "string",
  "similarEntries": [
    {
      "entryId": "string",
      "title": "string",
      "content": "string",
      "category": "string",
      "createdAt": "string",
      "similarityScore": 0.85
    }
  ],
  "total": 5,
  "method": "vector_similarity"
}
```

**Error Responses**
- 500 Internal Server Error: Database query failed

**Business Logic**
Uses VectorKnowledgeBase to find similar entries if available, retrieves original entry content, performs vector similarity search, queries database for entry details, falls back to database relationships if vector search not available, and returns similar entries with similarity scores.

**Section sources**
- [knowledge_api_handlers.cpp](file://shared/knowledge_base/knowledge_api_handlers.cpp#L322-L420)

### POST /api/knowledge/ask
RAG-based Q&A using VectorKnowledgeBase + LLM.

**Request Parameters**
- Body (application/json):
  - question (string, required): Question to ask
  - maxSources (integer, optional): Maximum number of sources (default: 5)

**Response (200 OK)**
```json
{
  "sessionId": "string",
  "question": "string",
  "answer": "string",
  "confidence": 0.85,
  "sources": [
    {
      "entryId": "string",
      "title": "string",
      "content": "string"
    }
  ],
  "sourcesUsed": 3
}
```

**Error Responses**
- 400 Bad Request: Missing question
- 500 Internal Server Error: Failed to store Q&A session

**Business Logic**
Retrieves context from knowledge base using similarity search, generates answer (in production would use OpenAIClient for RAG), stores Q&A session in database, and returns answer with sources and confidence score.

**Section sources**
- [knowledge_api_handlers.cpp](file://shared/knowledge_base/knowledge_api_handlers.cpp#L422-L520)

## LLM Integration
The LLM integration provides endpoints for text analysis, conversation management, batch processing, and model management.

### GET /api/llm/models/{modelId}
Get LLM model details with real-time availability.

**Request Parameters**
- Path:
  - modelId (string, required): Model ID to retrieve

**Response (200 OK)**
```json
{
  "modelId": "string",
  "name": "string",
  "provider": "string",
  "version": "string",
  "type": "string",
  "contextLength": 0,
  "maxTokens": 0,
  "costPerInTokens": 0,
  "costPerOutTokens": 0,
  "capabilities": {},
  "isAvailable": true,
  "clientHealthy": true
}
```

**Error Responses**
- 404 Not Found: Model not found

**Business Logic**
Queries model details from llm_model_registry table, checks real-time availability from OpenAIClient or AnthropicClient, and returns comprehensive model information with health status.

**Section sources**
- [llm_api_handlers.cpp](file://shared/llm/llm_api_handlers.cpp#L100-L150)

### POST /api/llm/analyze
Analyze text using OpenAIClient or AnthropicClient.

**Request Parameters**
- Body (application/json):
  - text (string, required): Text to analyze
  - analysisType (string, optional): Type of analysis (compliance, risk, etc.; default: compliance)
  - provider (string, optional): LLM provider (openai, anthropic; default: openai)
  - context (string, optional): Context for analysis

**Response (200 OK)**
```json
{
  "analysisId": "string",
  "analysisType": "string",
  "result": "string",
  "provider": "string",
  "tokensUsed": 0,
  "cost": 0
}
```

**Error Responses**
- 400 Bad Request: Missing text
- 500 Internal Server Error: Failed to analyze text

**Business Logic**
Uses OpenAIClient or AnthropicClient to analyze text based on provider, stores analysis in database, returns analysis result with usage statistics and cost.

**Section sources**
- [llm_api_handlers.cpp](file://shared/llm/llm_api_handlers.cpp#L152-L250)

### POST /api/llm/conversations
Create new LLM conversation.

**Request Parameters**
- Body (application/json):
  - title (string, optional): Conversation title (default: New Conversation)
  - modelId (string, optional): Model ID (default: gpt-4)
  - systemPrompt (string, optional): System prompt (default: You are a helpful compliance assistant.)

**Response (201 Created)**
```json
{
  "conversationId": "string",
  "title": "string",
  "modelId": "string",
  "createdAt": "string"
}
```

**Error Responses**
- 500 Internal Server Error: Failed to create conversation

**Business Logic**
Generates UUID for conversation, inserts conversation into llm_conversations table, and returns conversation details with creation timestamp.

**Section sources**
- [llm_api_handlers.cpp](file://shared/llm/llm_api_handlers.cpp#L252-L300)

### POST /api/llm/conversations/{conversationId}/messages
Add message to conversation and get LLM response.

**Request Parameters**
- Path:
  - conversationId (string, required): Conversation ID
- Body (application/json):
  - content (string, required): Message content
  - role (string, optional): Role (user, assistant; default: user)

**Response (200 OK)**
```json
{
  "userMessageId": "string",
  "assistantMessageId": "string",
  "assistantResponse": "string",
  "tokensUsed": 0,
  "cost": 0
}
```

**Error Responses**
- 400 Bad Request: Missing content
- 404 Not Found: Conversation not found
- 500 Internal Server Error: Failed to generate response

**Business Logic**
Retrieves conversation details, stores user message in llm_messages table, generates LLM response using OpenAIClient, stores assistant response, updates conversation statistics, and returns response with message IDs.

**Section sources**
- [llm_api_handlers.cpp](file://shared/llm/llm_api_handlers.cpp#L302-L450)

## Regulatory Monitor
The regulatory monitor provides endpoints for monitoring regulatory changes, sources, and system health.

### GET /api/regulatory-changes
Get recent regulatory changes.

**Request Parameters**
- Query:
  - limit (integer, optional): Maximum number of changes (default: 50, max: 1000)

**Response (200 OK)**
```json
[
  {
    "id": "string",
    "source": "string",
    "title": "string",
    "description": "string",
    "content_url": "string",
    "change_type": "string",
    "severity": "string",
    "detected_at": 0,
    "published_at": 0
  }
]
```

**Error Responses**
- 405 Method Not Allowed: Method not allowed
- 500 Internal Server Error: Error retrieving changes

**Business Logic**
Retrieves recent regulatory changes from monitor_->get_recent_changes(), formats changes with timestamps in milliseconds, and returns array of change objects.

**Section sources**
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L450-L500)

### POST /api/regulatory-changes
Create new regulatory change.

**Request Parameters**
- Body (application/json):
  - source (string, optional): Source of change (default: API)
  - title (string, optional): Title of change (default: Unknown)
  - description (string, optional): Description
  - content_url (string, optional): URL to content
  - change_type (string, optional): Type of change (default: manual_entry)
  - severity (string, optional): Severity level (default: MEDIUM)

**Response (201 Created)**
```json
{
  "message": "Regulatory change created",
  "id": "string"
}
```

**Error Responses**
- 400 Bad Request: Request body required or invalid JSON
- 500 Internal Server Error: Failed to store regulatory change

**Business Logic**
Parses JSON body, creates RegulatoryChange object with current timestamps, generates ID using generate_change_id, stores change using monitor_->store_change(), and returns success message with change ID.

**Section sources**
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L502-L550)

### GET /api/sources
Get regulatory data sources.

**Response (200 OK)**
```json
[
  {
    "id": "string",
    "name": "string",
    "base_url": "string",
    "source_type": "string",
    "check_interval_minutes": 0,
    "active": true,
    "consecutive_failures": 0,
    "last_check": 0
  }
]
```

**Error Responses**
- 405 Method Not Allowed: Only GET allowed
- 500 Internal Server Error: Error retrieving sources

**Business Logic**
Retrieves all sources from monitor_->get_sources(), formats sources with last_check timestamp in milliseconds, and returns array of source objects.

**Section sources**
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L552-L580)

### POST /api/monitoring/force-check
Force check regulatory sources.

**Request Parameters**
- Body (application/json, optional):
  - source_id (string): Specific source ID to check

**Response (200 OK)**
```json
{
  "message": "Force check initiated for all sources",
  "sources_triggered": 0
}
```

```json
{
  "message": "Force check initiated",
  "source_id": "string"
}
```

**Error Responses**
- 404 Not Found: Source not found
- 500 Internal Server Error: Error initiating force check

**Business Logic**
If source_id provided, forces check on specific source using monitor_->force_check_source(). If no source_id, forces check on all sources and returns count of successfully triggered sources.

**Section sources**
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L582-L630)

### GET /api/health
Check system health.

**Response (200 OK)**
```json
{
  "status": "healthy",
  "timestamp": 0,
  "services": {
    "database": true,
    "monitor": true
  }
}
```

**Error Responses**
- 500 Internal Server Error: System unhealthy

**Business Logic**
Checks database connectivity by getting connection from pool and pinging, checks monitor status using monitor_->is_running(), and returns health status with timestamp.

**Section sources**
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L632-L680)

## Error Handling
The API implements comprehensive error handling with standardized error responses and detailed error codes.

### Error Response Format
All error responses follow the standard format defined in error_handling_config.json:

```json
{
  "error": {
    "code": "MACHINE_READABLE_ERROR_CODE",
    "message": "Human readable error message",
    "details": "Optional detailed error information",
    "field": "Optional field that caused the error",
    "timestamp": "ISO 8601 timestamp",
    "request_id": "Unique request identifier",
    "path": "API endpoint path",
    "method": "HTTP method used"
  },
  "meta": {
    "version": "API version",
    "compatibility_mode": "Whether compatibility mode is active",
    "deprecation_warning": "Optional deprecation notice"
  }
}
```

### Error Codes
The following error codes are used across the API:

| Error Code | HTTP Status | Description | Retryable |
|------------|-------------|-------------|-----------|
| VALIDATION_ERROR | 400 | Request validation failed | No |
| AUTHENTICATION_ERROR | 401 | Authentication credentials invalid or missing | No |
| AUTHORIZATION_ERROR | 403 | User does not have permission | No |
| NOT_FOUND | 404 | Requested resource does not exist | No |
| CONFLICT | 409 | Request conflicts with current state | No |
| RATE_LIMIT_EXCEEDED | 429 | Too many requests in given time period | Yes |
| INTERNAL_ERROR | 500 | Unexpected internal server error | Yes |
| SERVICE_UNAVAILABLE | 503 | Service is temporarily unavailable | Yes |
| MAINTENANCE_MODE | 503 | Service is under maintenance | Yes |

**Section sources**
- [error_handling_config.json](file://shared/api_config/error_handling_config.json#L1-L279)

## Rate Limiting and CORS
The API implements rate limiting and CORS configuration as defined in the API registry.

### Rate Limiting
The API enforces rate limiting with the following configuration:
- Default limit: 60 requests per minute per client
- Configurable via APIRegistryConfig.rate_limit_requests_per_minute
- Exceeding limit returns 429 Too Many Requests with RATE_LIMIT_EXCEEDED error code
- Retry-After header indicates when client can retry

### CORS Configuration
The API implements CORS with the following configuration:
- Enabled by default (APIRegistryConfig.enable_cors)
- Allowed origins: "*" (configurable via APIRegistryConfig.cors_allowed_origins)
- Allowed methods: GET, POST, PUT, DELETE, PATCH, OPTIONS
- Allowed headers: Authorization, Content-Type, X-API-Key, X-Requested-With
- Exposed headers: Content-Type, X-API-Key, X-RateLimit-Limit, X-RateLimit-Remaining
- Credentials allowed: true

**Section sources**
- [api_registry.hpp](file://shared/api_registry/api_registry.hpp#L69-L109)
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L320-L356)