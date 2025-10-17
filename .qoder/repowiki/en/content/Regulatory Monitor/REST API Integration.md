# REST API Integration

<cite>
**Referenced Files in This Document**   
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp)
- [rest_api_server.hpp](file://regulatory_monitor/rest_api_server.hpp)
- [production_regulatory_monitor.hpp](file://regulatory_monitor/production_regulatory_monitor.hpp)
- [openapi_generator.cpp](file://shared/api_docs/openapi_generator.cpp)
- [web_ui_handlers.cpp](file://shared/web_ui/web_ui_handlers.cpp)
</cite>

## Table of Contents
1. [Introduction](#introduction)
2. [API Endpoints](#api-endpoints)
3. [Authentication and Security](#authentication-and-security)
4. [CORS Implementation](#cors-implementation)
5. [Rate Limiting](#rate-limiting)
6. [Request Routing](#request-routing)
7. [Protocol-Specific Examples](#protocol-specific-examples)
8. [Security Considerations](#security-considerations)
9. [Client Implementation Issues](#client-implementation-issues)
10. [Performance Optimization](#performance-optimization)

## Introduction
The Regulatory Monitor's REST API provides programmatic access to regulatory change monitoring functionality, allowing clients to retrieve regulatory changes, manage monitoring sources, obtain monitoring statistics, and trigger manual checks. The API is implemented as a C++ server with comprehensive security features including JWT and API key authentication, CORS protection, and rate limiting. This documentation details the API endpoints, authentication methods, security implementation, and best practices for client integration.

## API Endpoints

### /regulatory-changes Endpoint
The `/regulatory-changes` endpoint provides access to regulatory changes detected by the monitoring system.

**HTTP Methods and URL Patterns**
- `GET /api/regulatory-changes`: Retrieve a list of recent regulatory changes
- `POST /api/regulatory-changes`: Create a new regulatory change (manual entry)

**Request/Response Schema**
For GET requests:
```json
{
  "id": "string",
  "source": "string",
  "title": "string",
  "description": "string",
  "content_url": "string",
  "change_type": "string",
  "severity": "string",
  "detected_at": "number",
  "published_at": "number"
}
```

Query parameters:
- `limit`: Maximum number of changes to return (default: 50, maximum: 1000)

For POST requests:
```json
{
  "source": "string",
  "title": "string",
  "description": "string",
  "content_url": "string",
  "change_type": "string",
  "severity": "string"
}
```

**Status Codes**
- 200: Successful retrieval of regulatory changes
- 201: Regulatory change created successfully
- 400: Invalid request body or parameters
- 401: Authentication required
- 405: Method not allowed
- 500: Internal server error

**Section sources**
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L368-L427)

### /sources Endpoint
The `/sources` endpoint provides access to regulatory monitoring sources.

**HTTP Methods and URL Patterns**
- `GET /api/sources`: Retrieve a list of all monitoring sources

**Request/Response Schema**
```json
{
  "id": "string",
  "name": "string",
  "base_url": "string",
  "source_type": "string",
  "check_interval_minutes": "number",
  "active": "boolean",
  "consecutive_failures": "number",
  "last_check": "number"
}
```

**Status Codes**
- 200: Successful retrieval of sources
- 401: Authentication required
- 405: Method not allowed
- 500: Internal server error

**Section sources**
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L427-L468)

### /monitoring-stats Endpoint
The `/monitoring-stats` endpoint provides statistical information about the regulatory monitoring system.

**HTTP Methods and URL Patterns**
- `GET /api/monitoring/stats`: Retrieve monitoring statistics

**Request/Response Schema**
```json
{
  "total_sources": "integer",
  "active_sources": "integer",
  "total_changes": "integer",
  "changes_today": "integer",
  "last_update": "string",
  "sources_by_jurisdiction": "object"
}
```

**Status Codes**
- 200: Successful retrieval of monitoring statistics
- 401: Authentication required
- 405: Method not allowed
- 500: Internal server error

**Section sources**
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L468-L499)
- [openapi_generator.cpp](file://shared/api_docs/openapi_generator.cpp#L917-L943)

### /force-check Endpoint
The `/force-check` endpoint allows triggering manual checks of regulatory sources.

**HTTP Methods and URL Patterns**
- `POST /api/monitoring/force-check`: Trigger a manual check of regulatory sources

**Request/Response Schema**
Request body (optional):
```json
{
  "source_id": "string"
}
```

Response:
```json
{
  "message": "string",
  "source_id": "string",
  "sources_triggered": "number"
}
```

**Status Codes**
- 200: Force check initiated successfully
- 401: Authentication required
- 404: Source not found
- 405: Method not allowed
- 500: Internal server error

**Section sources**
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L499-L568)

## Authentication and Security

### JWT Authentication
The API supports JWT (JSON Web Token) authentication for secure access. Clients must include a Bearer token in the Authorization header.

**Authentication Flow**
1. Client sends credentials to `/api/auth/login` endpoint
2. Server validates credentials against the database
3. Server generates a JWT token with HMAC-SHA256 signature
4. Client includes the token in subsequent requests

**Token Structure**
- Algorithm: HS256
- Issuer: regulens_api
- Audience: regulens_clients
- Subject: username
- Issued at: timestamp
- Expiration: 1 hour from issue
- Roles: admin, user

**Section sources**
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L741-L778)
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L939-L980)

### API Key Authentication
The API supports API key authentication as an alternative to JWT.

**Authentication Method**
Clients must include their API key in the Authorization header with the format:
```
Authorization: API-Key <api_key_value>
```

**Key Validation**
- API keys are stored in the database with SHA-256 hashing
- Keys must be at least 32 characters long
- Keys can have expiration dates and rate limits
- Active status is verified during validation

**Section sources**
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L741-L778)
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L800-L899)

## CORS Implementation
The API implements a secure CORS (Cross-Origin Resource Sharing) policy to control which domains can access the API.

**Implementation Details**
- CORS validation is performed in the `validate_cors` method
- Allowed origins are configured via the `ALLOWED_CORS_ORIGINS` environment variable
- Multiple origins can be specified, separated by commas
- Requests without an Origin header are allowed
- The environment variable must be configured in production

**Security Requirements**
- The `ALLOWED_CORS_ORIGINS` environment variable is required
- No default origins are allowed for production security
- If the environment variable is not configured, all CORS requests are rejected

**Section sources**
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L700-L739)

## Rate Limiting
The API implements rate limiting to prevent abuse and ensure service availability.

**Implementation Details**
- Rate limiting is implemented in the `rate_limit_check` method
- Limits are tracked per client IP address
- Default configuration:
  - Maximum requests: 100 per window
  - Time window: 60 seconds
- A sliding window algorithm is used to track requests
- Excessive requests are logged with warning level

**Rate Limit Response**
When the rate limit is exceeded:
- HTTP status code: 429 Too Many Requests
- Response body: Not explicitly shown, but the request is denied
- Warning is logged with client IP and request count

**Section sources**
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L700-L739)

## Request Routing
The API request routing is handled through the `handle_request` method chain, with the main routing logic in the `route_request` method.

**Routing Process**
1. OPTIONS requests are handled first for CORS
2. Authentication is checked for non-public endpoints
3. The request path is matched to the appropriate handler
4. The handler processes the request and generates a response

**Public Endpoints**
- `/api/health`: Health check endpoint
- `/api/auth/login`: Authentication endpoint

**Protected Endpoints**
All other endpoints require authentication via JWT or API key.

**Section sources**
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L300-L399)

## Protocol-Specific Examples

### Retrieving Active Sources
To retrieve a list of active regulatory sources:

```bash
curl -X GET "https://api.regulens.com/api/sources" \
  -H "Authorization: Bearer <your_jwt_token>" \
  -H "Content-Type: application/json"
```

Or with API key:

```bash
curl -X GET "https://api.regulens.com/api/sources" \
  -H "Authorization: API-Key <your_api_key>" \
  -H "Content-Type: application/json"
```

### Triggering a Manual Check
To trigger a manual check of all sources:

```bash
curl -X POST "https://api.regulens.com/api/monitoring/force-check" \
  -H "Authorization: Bearer <your_jwt_token>" \
  -H "Content-Type: application/json"
```

To trigger a manual check of a specific source:

```bash
curl -X POST "https://api.regulens.com/api/monitoring/force-check" \
  -H "Authorization: Bearer <your_jwt_token>" \
  -H "Content-Type: application/json" \
  -d '{"source_id": "source_123"}'
```

### Obtaining Monitoring Statistics
To retrieve monitoring statistics:

```bash
curl -X GET "https://api.regulens.com/api/monitoring/stats" \
  -H "Authorization: Bearer <your_jwt_token>" \
  -H "Content-Type: application/json"
```

## Security Considerations

### Password Hashing
User passwords are securely hashed using PBKDF2-HMAC-SHA256 with the following parameters:
- Iterations: 100,000 (OWASP recommendation)
- Key length: 256 bits
- Random salt: 16 bytes
- Storage format: "pbkdf2_sha256$iterations$salt$hash"

The implementation includes protections against timing attacks during password comparison.

**Section sources**
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L1200-L1399)

### HMAC Signature Generation
JWT tokens are secured with HMAC-SHA256 signatures using a secret key from the `JWT_SECRET_KEY` environment variable.

**Security Requirements**
- The JWT secret key must be at least 32 characters long
- Default/example values are rejected (e.g., "CHANGE", "EXAMPLE", "DEFAULT")
- The environment variable must be configured
- Different implementations for macOS (CommonCrypto) and Linux (OpenSSL)

**Section sources**
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L1000-L1199)

### Token Refresh Mechanism
The API supports token refresh using refresh tokens stored securely in the database.

**Refresh Process**
1. Client sends refresh token to `/api/auth/refresh` endpoint
2. Server validates the refresh token against the database
3. Server checks if the token is revoked or expired
4. Server generates a new access token
5. Client receives the new access token

Refresh tokens are hashed with SHA-256 before storage and include expiration and revocation status.

**Section sources**
- [rest_api_server.cpp](file://regulatory_monitor/rest_api_server.cpp#L939-L980)

## Client Implementation Issues

### Handling 429 Rate Limit Responses
When encountering rate limit responses (HTTP 429), clients should implement exponential backoff strategies:

```javascript
async function makeApiRequest(url, options, maxRetries = 5) {
  let retryCount = 0;
  
  while (retryCount < maxRetries) {
    try {
      const response = await fetch(url, options);
      
      if (response.status === 429) {
        // Extract retry-after header if available, otherwise use exponential backoff
        const retryAfter = response.headers.get('retry-after') || 
                          Math.pow(2, retryCount) * 1000;
        await new Promise(resolve => setTimeout(resolve, retryAfter));
        retryCount++;
        continue;
      }
      
      return response;
    } catch (error) {
      if (retryCount >= maxRetries) throw error;
      // Exponential backoff for other errors
      await new Promise(resolve => setTimeout(resolve, Math.pow(2, retryCount) * 1000));
      retryCount++;
    }
  }
}
```

### Maintaining Authentication State
Clients should implement proper authentication state management:

1. Store access tokens securely (e.g., in memory, not localStorage for web apps)
2. Implement token refresh before expiration
3. Handle authentication failures gracefully
4. Provide clear error messages for authentication issues

Example token refresh implementation:

```javascript
class ApiClient {
  constructor() {
    this.accessToken = null;
    this.refreshToken = null;
  }
  
  async ensureValidToken() {
    if (!this.accessToken || this.isTokenExpired(this.accessToken)) {
      await this.refreshAccessToken();
    }
  }
  
  async refreshAccessToken() {
    const response = await fetch('/api/auth/refresh', {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${this.refreshToken}`,
        'Content-Type': 'application/json'
      }
    });
    
    if (response.ok) {
      const data = await response.json();
      this.accessToken = data.access_token;
    } else {
      // Handle refresh failure - prompt user to log in again
      throw new Error('Authentication expired');
    }
  }
  
  isTokenExpired(token) {
    // Parse JWT and check expiration
    const payload = JSON.parse(atob(token.split('.')[1]));
    return Date.now() >= (payload.exp * 1000);
  }
}
```

## Performance Optimization

### Bulk Data Retrieval
For efficient bulk data retrieval, clients should:

1. Use appropriate limit parameters to avoid excessive data transfer
2. Implement pagination for large datasets
3. Use compression when supported
4. Cache responses when appropriate

Example with pagination:

```bash
# Retrieve regulatory changes in pages of 100
curl -X GET "https://api.regulens.com/api/regulatory-changes?limit=100&offset=0"
curl -X GET "https://api.regulens.com/api/regulatory-changes?limit=100&offset=100"
```

### Efficient Polling Strategies
Instead of frequent polling, clients should consider:

1. Using longer intervals for polling
2. Implementing exponential backoff
3. Using conditional requests with ETags when available
4. Considering webhook-based notifications if supported

Example polling with exponential backoff:

```javascript
async function pollForChanges() {
  let interval = 30000; // Start with 30 seconds
  const maxInterval = 300000; // Maximum 5 minutes
  
  while (true) {
    try {
      const response = await fetch('/api/regulatory-changes?limit=10');
      const changes = await response.json();
      
      // Process changes
      processChanges(changes);
      
      // Reset interval after successful response
      interval = 30000;
    } catch (error) {
      // Increase interval on errors
      interval = Math.min(interval * 2, maxInterval);
    }
    
    // Wait before next poll
    await new Promise(resolve => setTimeout(resolve, interval));
  }
}
```