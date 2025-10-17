# Security Considerations

<cite>
**Referenced Files in This Document**   
- [llm_key_manager.hpp](file://shared/llm/llm_key_manager.hpp)
- [llm_key_manager.cpp](file://shared/llm/llm_key_manager.cpp)
- [llm_key_api_handlers.hpp](file://shared/llm/llm_key_api_handlers.hpp)
- [jwt_parser.hpp](file://shared/auth/jwt_parser.hpp)
- [auth_helpers.hpp](file://shared/auth/auth_helpers.hpp)
- [useLLMKeys.ts](file://frontend/src/hooks/useLLMKeys.ts)
- [api.ts](file://frontend/src/services/api.ts)
- [schema.sql](file://schema.sql)
- [key_rotation_manager.hpp](file://shared/llm/key_rotation_manager.hpp)
- [LLMKeyManagement.tsx](file://frontend/src/pages/LLMKeyManagement.tsx)
</cite>

## Table of Contents
1. [Introduction](#introduction)
2. [API Key Management Implementation](#api-key-management-implementation)
3. [Data Privacy and Encryption](#data-privacy-and-encryption)
4. [Secure Communication Patterns](#secure-communication-patterns)
5. [LLMKeyManager Class Analysis](#llmkeymanager-class-analysis)
6. [Key Rotation and Lifecycle Management](#key-rotation-and-lifecycle-management)
7. [Authentication System Integration](#authentication-system-integration)
8. [Common Security Risks and Mitigations](#common-security-risks-and-mitigations)
9. [Secure LLM Integration Best Practices](#secure-llm-integration-best-practices)
10. [Conclusion](#conclusion)

## Introduction
The Regulens platform implements a comprehensive security framework for LLM integration, focusing on API key management, data privacy, and secure communication patterns. This document details the implementation of the LLMKeyManager class and its role in securely storing and rotating API credentials, as well as the integration with the authentication system for secure API access. The security architecture addresses key risks such as key leakage, unauthorized access, and data exfiltration through a multi-layered approach combining encryption, access controls, and monitoring.

**Section sources**
- [llm_key_manager.hpp](file://shared/llm/llm_key_manager.hpp#L1-L262)
- [llm_key_api_handlers.hpp](file://shared/llm/llm_key_api_handlers.hpp#L1-L129)

## API Key Management Implementation
The API key management system in Regulens provides a robust framework for creating, retrieving, updating, and deleting LLM API keys with comprehensive access controls. The system supports multiple LLM providers including OpenAI, Anthropic, Google, Azure, and custom providers, with provider-specific validation and integration.

The LLMKeyManager class implements a complete API key lifecycle management system with support for key creation, retrieval, updates, and deletion. Each API key is associated with metadata including provider, environment (development, staging, production), rotation schedule, and usage tracking. The system enforces access controls through the validate_key_access method, ensuring that users can only access keys they have permission to view or modify.

```mermaid
classDiagram
class LLMKey {
+string key_id
+string key_name
+string provider
+string encrypted_key
+string key_hash
+string key_last_four
+string status
+string created_by
+datetime created_at
+datetime updated_at
+optional datetime expires_at
+optional datetime last_used_at
+int usage_count
+int error_count
+optional int rate_limit_remaining
+optional datetime rate_limit_reset_at
+string rotation_schedule
+optional datetime last_rotated_at
+int rotation_reminder_days
+bool auto_rotate
+vector~string~ tags
+json metadata
+bool is_default
+string environment
}
class CreateKeyRequest {
+string key_name
+string provider
+optional string model
+string plain_key
+string created_by
+optional datetime expires_at
+optional string rotation_schedule
+bool auto_rotate
+vector~string~ tags
+json metadata
+bool is_default
+string environment
}
class UpdateKeyRequest {
+string key_name
+optional string model
+optional datetime expires_at
+optional string rotation_schedule
+optional bool auto_rotate
+optional vector~string~ tags
+optional json metadata
+optional bool is_default
+optional string status
}
class KeyUsageStats {
+string usage_id
+string key_id
+datetime request_timestamp
+string provider
+string model
+string operation_type
+optional int tokens_used
+optional double cost_usd
+optional int response_time_ms
+bool success
+optional string error_type
+optional string error_message
+string user_id
+string session_id
+json metadata
}
LLMKeyManager --> LLMKey : "manages"
LLMKeyManager --> CreateKeyRequest : "uses"
LLMKeyManager --> UpdateKeyRequest : "uses"
LLMKeyManager --> KeyUsageStats : "tracks"
LLMKeyManager --> KeyRotationRecord : "rotates"
LLMKeyManager --> KeyHealthStatus : "monitors"
LLMKeyManager --> KeyAlert : "creates"
```

**Diagram sources**
- [llm_key_manager.hpp](file://shared/llm/llm_key_manager.hpp#L1-L262)

**Section sources**
- [llm_key_manager.hpp](file://shared/llm/llm_key_manager.hpp#L1-L262)
- [llm_key_api_handlers.hpp](file://shared/llm/llm_key_api_handlers.hpp#L1-L129)

## Data Privacy and Encryption
Regulens implements a comprehensive data privacy framework for LLM API keys, with all sensitive data encrypted at rest using AES encryption. The system uses a dedicated encryption key to protect API keys stored in the database, ensuring that even with direct database access, the actual API key values cannot be retrieved without the encryption key.

The LLMKeyManager class handles encryption and decryption through dedicated methods that use OpenSSL's AES implementation. When a new API key is created, it is immediately encrypted before being stored in the database. The encrypted_key field in the llm_api_keys table contains the base64-encoded encrypted key, while the key_hash field stores a SHA-256 hash for verification purposes. The system also stores the last four characters of the key (key_last_four) to allow users to identify keys without exposing the full value.

```mermaid
flowchart TD
A["Create API Key"] --> B["Validate Input"]
B --> C["Generate UUID"]
C --> D["Encrypt Key with AES"]
D --> E["Hash Key with SHA-256"]
E --> F["Extract Last 4 Characters"]
F --> G["Store in Database"]
G --> H["Return Key ID and Last 4"]
H --> I["Key Ready for Use"]
J["Retrieve API Key"] --> K["Validate Access"]
K --> L["Query Database"]
L --> M["Decrypt Key"]
M --> N["Return Decrypted Key"]
N --> O["Use in LLM Request"]
P["Key Rotation"] --> Q["Generate New Key"]
Q --> R["Encrypt New Key"]
R --> S["Update Database"]
S --> T["Update Rotation History"]
T --> U["Invalidate Old Key"]
```

**Diagram sources**
- [llm_key_manager.cpp](file://shared/llm/llm_key_manager.cpp#L0-L45)
- [llm_key_manager.hpp](file://shared/llm/llm_key_manager.hpp#L1-L262)

**Section sources**
- [llm_key_manager.cpp](file://shared/llm/llm_key_manager.cpp#L0-L45)
- [llm_key_manager.hpp](file://shared/llm/llm_key_manager.hpp#L1-L262)
- [schema.sql](file://schema.sql#L3057-L3256)

## Secure Communication Patterns
The Regulens platform implements secure communication patterns for LLM integration through a combination of HTTPS, JWT-based authentication, and request validation. All API endpoints are protected with JWT tokens that are validated on each request, ensuring that only authenticated users can access LLM functionality.

The API client in the frontend implements automatic token refresh and request interception to ensure secure communication with the backend. When making requests to LLM-related endpoints, the client automatically includes the JWT token in the Authorization header. The system also implements rate limiting and request validation to prevent abuse and ensure system stability.

```mermaid
sequenceDiagram
participant Frontend
participant API Client
participant Backend
participant LLM Provider
Frontend->>API Client : Request LLM Service
API Client->>API Client : Add JWT Token
API Client->>Backend : HTTPS Request with JWT
Backend->>Backend : Validate JWT Signature
Backend->>Backend : Check Token Expiration
Backend->>Backend : Verify User Permissions
Backend->>Backend : Process Request
Backend->>Backend : Retrieve Decrypted API Key
Backend->>LLM Provider : Forward Request with API Key
LLM Provider-->>Backend : Return Response
Backend-->>API Client : Return Response
API Client-->>Frontend : Return Result
```

**Diagram sources**
- [api.ts](file://frontend/src/services/api.ts#L0-L799)
- [jwt_parser.hpp](file://shared/auth/jwt_parser.hpp#L0-L46)

**Section sources**
- [api.ts](file://frontend/src/services/api.ts#L0-L799)
- [jwt_parser.hpp](file://shared/auth/jwt_parser.hpp#L0-L46)
- [auth_helpers.hpp](file://shared/auth/auth_helpers.hpp#L0-L18)

## LLMKeyManager Class Analysis
The LLMKeyManager class is the core component of the API key management system, providing a comprehensive interface for managing LLM credentials securely. The class is designed with security as a primary concern, implementing encryption, access controls, and comprehensive logging.

The class constructor requires both a database connection and a structured logger, ensuring that all operations are properly logged and can be audited. The manager implements a wide range of functionality including key creation, retrieval, updates, deletion, rotation, usage tracking, health monitoring, and alerting. Each operation includes appropriate validation and error handling to prevent security vulnerabilities.

```mermaid
classDiagram
class LLMKeyManager {
+LLMKeyManager(db_conn, logger)
+~LLMKeyManager()
+create_key(request)
+get_key(key_id)
+get_keys(user_id, provider, status, limit, offset)
+update_key(key_id, request)
+delete_key(key_id, deleted_by)
+get_decrypted_key(key_id)
+get_active_key_for_provider(provider, environment)
+rotate_key(request)
+get_rotation_history(key_id, limit)
+record_usage(usage)
+get_usage_history(key_id, limit)
+get_usage_analytics(key_id, time_range)
+check_key_health(key_id, check_type)
+get_health_history(key_id, limit)
+create_alert(alert)
+get_active_alerts(key_id)
+resolve_alert(alert_id, resolved_by)
+process_scheduled_rotations()
+check_key_expirations()
+update_daily_usage_stats()
+cleanup_old_data(retention_days)
+get_provider_usage_summary(time_range)
+get_key_performance_metrics(key_id)
+get_keys_due_for_rotation(days_ahead)
+set_encryption_key(key)
+set_max_keys_per_user(max_keys)
+set_default_rotation_schedule(schedule)
}
LLMKeyManager --> PostgreSQLConnection : "uses"
LLMKeyManager --> StructuredLogger : "uses"
LLMKeyManager --> LLMKey : "manages"
LLMKeyManager --> KeyRotationRecord : "creates"
LLMKeyManager --> KeyUsageStats : "tracks"
LLMKeyManager --> KeyHealthStatus : "monitors"
LLMKeyManager --> KeyAlert : "generates"
```

**Diagram sources**
- [llm_key_manager.hpp](file://shared/llm/llm_key_manager.hpp#L1-L262)

**Section sources**
- [llm_key_manager.hpp](file://shared/llm/llm_key_manager.hpp#L1-L262)
- [llm_key_manager.cpp](file://shared/llm/llm_key_manager.cpp#L0-L45)

## Key Rotation and Lifecycle Management
The key rotation system in Regulens provides automated and manual rotation capabilities to ensure API keys are regularly refreshed, reducing the risk of long-term exposure. The system supports multiple rotation schedules including daily, weekly, monthly, quarterly, and manual rotation, with configurable rotation reminders.

The KeyRotationManager class works in conjunction with the LLMKeyManager to handle rotation jobs, scheduling, and execution. Rotation records are stored in the key_rotation_history table, which maintains a complete audit trail of all rotation activities including the old and new key hashes, rotation reason, and success status. This provides a comprehensive history for security audits and incident response.

```mermaid
classDiagram
class KeyRotationRecord {
+string rotation_id
+string key_id
+string rotated_by
+string rotation_reason
+string rotation_method
+string old_key_last_four
+string new_key_last_four
+string old_key_hash
+string new_key_hash
+bool rotation_success
+optional string error_message
+datetime rotated_at
+json metadata
}
class RotationSchedule {
+string schedule_type
+hours interval_hours
+string calendar_expression
+long usage_threshold
+vector~string~ trigger_events
}
class RotationJob {
+string job_id
+string key_id
+string key_name
+string provider
+RotationSchedule schedule
+datetime next_rotation_at
+datetime last_rotation_at
+string status
+int rotation_count
+bool auto_rotate
+json metadata
}
class RotationResult {
+string job_id
+string key_id
+bool success
+string old_key_last_four
+string new_key_last_four
+optional string error_message
+int tokens_used
+double cost_incurred
+milliseconds duration
+json metadata
}
class RotationConfig {
+bool enabled
+int max_concurrent_rotations
+int rotation_timeout_seconds
+int retry_attempts
+seconds retry_delay
+bool backup_before_rotation
+int backup_retention_days
+string default_provider_url
+json provider_configs
}
LLMKeyManager --> KeyRotationRecord : "creates"
KeyRotationManager --> RotationJob : "manages"
KeyRotationManager --> RotationResult : "returns"
KeyRotationManager --> RotationConfig : "uses"
KeyRotationManager --> RotationSchedule : "applies"
```

**Diagram sources**
- [key_rotation_manager.hpp](file://shared/llm/key_rotation_manager.hpp#L1-L182)
- [llm_key_manager.hpp](file://shared/llm/llm_key_manager.hpp#L1-L262)

**Section sources**
- [key_rotation_manager.hpp](file://shared/llm/key_rotation_manager.hpp#L1-L182)
- [llm_key_manager.hpp](file://shared/llm/llm_key_manager.hpp#L1-L262)
- [schema.sql](file://schema.sql#L7465-L7490)

## Authentication System Integration
The authentication system in Regulens integrates with the LLM key management through JWT parsing and user access validation. The JWTParser class handles token validation, signature verification, and expiration checking, ensuring that only valid tokens can be used to access LLM functionality.

The auth_helpers module provides utility functions for extracting user information from requests, which is used to enforce access controls on API key operations. When a user attempts to access or modify an API key, the system validates that the user has the necessary permissions through the validate_key_access method in the LLMKeyAPIHandlers class.

```mermaid
sequenceDiagram
participant User
participant Frontend
participant Backend
participant JWTParser
participant LLMKeyManager
User->>Frontend : Login with Credentials
Frontend->>Backend : POST /auth/login
Backend->>Backend : Authenticate User
Backend-->>Frontend : Return JWT Token
Frontend->>Frontend : Store Token in Local Storage
Frontend->>Backend : Request with JWT in Authorization Header
Backend->>JWTParser : Parse and Validate Token
JWTParser-->>Backend : Return User Claims
Backend->>LLMKeyManager : Process Request with User ID
LLMKeyManager-->>Backend : Return Result
Backend-->>Frontend : Return Response
```

**Diagram sources**
- [jwt_parser.hpp](file://shared/auth/jwt_parser.hpp#L0-L46)
- [auth_helpers.hpp](file://shared/auth/auth_helpers.hpp#L0-L18)
- [llm_key_api_handlers.hpp](file://shared/llm/llm_key_api_handlers.hpp#L1-L129)

**Section sources**
- [jwt_parser.hpp](file://shared/auth/jwt_parser.hpp#L0-L46)
- [auth_helpers.hpp](file://shared/auth/auth_helpers.hpp#L0-L18)
- [llm_key_api_handlers.hpp](file://shared/llm/llm_key_api_handlers.hpp#L1-L129)

## Common Security Risks and Mitigations
The Regulens platform addresses several common security risks associated with LLM integration through a comprehensive set of mitigations. Key leakage is prevented through encryption at rest, limited exposure of key information (only showing last four characters), and strict access controls. Unauthorized access is mitigated through JWT-based authentication, role-based access control, and request validation.

Data exfiltration risks are addressed through monitoring and alerting on unusual usage patterns, rate limiting to prevent abuse, and comprehensive audit logging. The system also implements key rotation to limit the window of exposure if a key is compromised, and provides mechanisms for immediate key revocation in case of suspected compromise.

```mermaid
flowchart TD
A["Security Risks"] --> B["Key Leakage"]
A --> C["Unauthorized Access"]
A --> D["Data Exfiltration"]
A --> E["Key Compromise"]
B --> F["Encrypt Keys at Rest"]
B --> G["Show Only Last 4 Characters"]
B --> H["Restrict Key Access"]
C --> I["JWT Authentication"]
C --> J["Role-Based Access Control"]
C --> K["Request Validation"]
D --> L["Usage Monitoring"]
D --> M["Rate Limiting"]
D --> N["Audit Logging"]
E --> O["Regular Key Rotation"]
E --> P["Immediate Revocation"]
E --> Q["Compromise Alerts"]
F --> R["Reduced Risk"]
G --> R
H --> R
I --> R
J --> R
K --> R
L --> R
M --> R
N --> R
O --> R
P --> R
Q --> R
```

**Diagram sources**
- [llm_key_manager.hpp](file://shared/llm/llm_key_manager.hpp#L1-L262)
- [llm_key_api_handlers.hpp](file://shared/llm/llm_key_api_handlers.hpp#L1-L129)
- [jwt_parser.hpp](file://shared/auth/jwt_parser.hpp#L0-L46)

**Section sources**
- [llm_key_manager.hpp](file://shared/llm/llm_key_manager.hpp#L1-L262)
- [llm_key_api_handlers.hpp](file://shared/llm/llm_key_api_handlers.hpp#L1-L129)
- [jwt_parser.hpp](file://shared/auth/jwt_parser.hpp#L0-L46)

## Secure LLM Integration Best Practices
Based on the implementation in Regulens, several best practices emerge for secure LLM integration. Network security should be implemented through HTTPS with TLS 1.3, strict CORS policies, and WAF protection. Audit logging should capture all key operations including creation, access, rotation, and deletion, with logs stored securely and protected from tampering.

Compliance with data protection regulations is achieved through data minimization (only storing necessary key information), encryption at rest and in transit, access controls, and regular security audits. The system should also implement monitoring and alerting for suspicious activities such as unusual usage patterns, failed access attempts, and potential data exfiltration.

```mermaid
flowchart TD
A["Secure LLM Integration"] --> B["Network Security"]
A --> C["Audit Logging"]
A --> D["Compliance"]
A --> E["Monitoring"]
B --> F["HTTPS with TLS 1.3"]
B --> G["CORS Policies"]
B --> H["WAF Protection"]
B --> I["Rate Limiting"]
C --> J["Log All Key Operations"]
C --> K["Immutable Logs"]
C --> L["Log Retention"]
C --> M["Log Access Controls"]
D --> N["Data Minimization"]
D --> O["Encryption at Rest"]
D --> P["Encryption in Transit"]
D --> Q["Access Controls"]
D --> R["Security Audits"]
E --> S["Usage Monitoring"]
E --> T["Anomaly Detection"]
E --> U["Alerting"]
E --> V["Incident Response"]
F --> W["Secure Integration"]
G --> W
H --> W
I --> W
J --> W
K --> W
L --> W
M --> W
N --> W
O --> W
P --> W
Q --> W
R --> W
S --> W
T --> W
U --> W
V --> W
```

**Diagram sources**
- [llm_key_manager.hpp](file://shared/llm/llm_key_manager.hpp#L1-L262)
- [llm_key_api_handlers.hpp](file://shared/llm/llm_key_api_handlers.hpp#L1-L129)
- [structured_logger.hpp](file://shared/logging/structured_logger.hpp#L1-L279)

**Section sources**
- [llm_key_manager.hpp](file://shared/llm/llm_key_manager.hpp#L1-L262)
- [llm_key_api_handlers.hpp](file://shared/llm/llm_key_api_handlers.hpp#L1-L129)
- [structured_logger.hpp](file://shared/logging/structured_logger.hpp#L1-L279)

## Conclusion
The security framework for LLM integration in Regulens provides a comprehensive approach to managing API keys, protecting data privacy, and ensuring secure communication. The LLMKeyManager class serves as the central component for API key management, implementing encryption, access controls, and comprehensive logging. The integration with the JWT-based authentication system ensures that only authorized users can access LLM functionality, while the key rotation system reduces the risk of long-term key exposure.

By following the best practices demonstrated in this implementation, organizations can securely integrate LLMs into their applications while mitigating common security risks. The combination of encryption, access controls, monitoring, and regular key rotation creates a robust security posture that protects sensitive API credentials and ensures compliance with data protection regulations.