# Simulation API

<cite>
**Referenced Files in This Document**   
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp)
- [simulator_api_handlers.hpp](file://shared/simulator/simulator_api_handlers.hpp)
- [regulatory_simulator.hpp](file://shared/simulator/regulatory_simulator.hpp)
- [regulatory_simulator.cpp](file://shared/simulator/regulatory_simulator.cpp)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp)
- [useSimulator.ts](file://frontend/src/hooks/useSimulator.ts)
- [RegulatorySimulator.tsx](file://frontend/src/pages/RegulatorySimulator.tsx)
- [schema.sql](file://schema.sql)
</cite>

## Table of Contents
1. [Introduction](#introduction)
2. [Scenario Management Endpoints](#scenario-management-endpoints)
3. [Simulation Execution Endpoints](#simulation-execution-endpoints)
4. [Template-Based Scenario Creation](#template-based-scenario-creation)
5. [Analytics Endpoints](#analytics-endpoints)
6. [Request Schemas](#request-schemas)
7. [Architecture Overview](#architecture-overview)
8. [Error Handling](#error-handling)
9. [Security and Access Control](#security-and-access-control)

## Introduction
The Simulation API provides a comprehensive interface for regulatory impact simulation, enabling users to create, manage, and execute simulation scenarios to assess the potential effects of regulatory changes. The API supports scenario management, simulation execution, result retrieval, and analytics, with a focus on regulatory compliance and risk assessment.

The system is built around a RESTful architecture with endpoints for scenario creation, modification, and deletion, as well as simulation orchestration and result processing. The implementation leverages PostgreSQL for data persistence and provides comprehensive error handling, rate limiting, and access control mechanisms.

**Section sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L1-L1260)
- [regulatory_simulator.hpp](file://shared/simulator/regulatory_simulator.hpp#L1-L226)

## Scenario Management Endpoints

### List Scenarios (GET /simulator/scenarios)
Retrieves a paginated list of simulation scenarios for the authenticated user. Supports filtering by scenario type, status, and search terms.

**Request Parameters**
- `limit` (optional): Number of scenarios to return (default: 50, max: 100)
- `offset` (optional): Number of scenarios to skip for pagination
- `scenario_type` (optional): Filter by scenario type (regulatory_change, market_change, operational_change)
- `status` (optional): Filter by status (active, inactive)
- `search` (optional): Text search on scenario name and description
- `tag` (optional): Filter by tag

**Response**
Returns a JSON object containing:
- `scenarios`: Array of scenario objects
- `count`: Total number of scenarios returned
- `limit`: Requested limit
- `offset`: Requested offset

```mermaid
sequenceDiagram
participant Client
participant API
participant Simulator
participant Database
Client->>API : GET /simulator/scenarios
API->>API : Extract user ID from JWT
API->>API : Parse query parameters
API->>Simulator : handle_get_scenarios(user_id, query_params)
Simulator->>Database : Query simulation_scenarios
Database-->>Simulator : Return scenario data
Simulator-->>API : Return formatted scenarios
API-->>Client : 200 OK with scenario list
```

**Diagram sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L344-L382)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2500-L2525)

**Section sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L344-L382)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2500-L2525)

### Get Scenario (GET /simulator/scenarios/{id})
Retrieves detailed information about a specific simulation scenario by ID.

**Response**
Returns a JSON object with the complete scenario details including:
- Scenario metadata (name, description, type)
- Regulatory changes and impact parameters
- Creation and update timestamps
- Tags and metadata
- Usage statistics

```mermaid
sequenceDiagram
participant Client
participant API
participant Simulator
participant Database
Client->>API : GET /simulator/scenarios/{id}
API->>API : Extract scenario ID from path
API->>API : Extract user ID from JWT
API->>Simulator : handle_get_scenario(scenario_id, user_id)
Simulator->>Database : Validate scenario access
Database-->>Simulator : Access validation result
alt Access granted
Simulator->>Database : Get scenario details
Database-->>Simulator : Return scenario data
Simulator-->>API : Return formatted scenario
API-->>Client : 200 OK with scenario details
else Access denied
API-->>Client : 404 Not Found
end
```

**Diagram sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L311-L344)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2546-L2561)

**Section sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L311-L344)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2546-L2561)

### Create Scenario (POST /simulator/scenarios)
Creates a new simulation scenario with the specified parameters.

**Request Body**
See [Request Schemas](#request-schemas) section for detailed structure.

**Response**
Returns a JSON object with:
- `scenario_id`: Unique identifier for the created scenario
- Complete scenario details
- Success message

```mermaid
sequenceDiagram
participant Client
participant API
participant Simulator
participant Database
Client->>API : POST /simulator/scenarios with JSON body
API->>API : Extract user ID from JWT
API->>API : Parse and validate request JSON
API->>Simulator : handle_create_scenario(request_body, user_id)
Simulator->>Simulator : parse_scenario_request()
Simulator->>Database : Insert new scenario
Database-->>Simulator : Confirmation
Simulator-->>API : Return created scenario
API-->>Client : 201 Created with scenario details
```

**Diagram sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L122-L150)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2472-L2499)

**Section sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L122-L150)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2472-L2499)

### Update Scenario (PUT /simulator/scenarios/{id})
Updates an existing simulation scenario with new parameters.

**Request Body**
Partial scenario object with fields to update.

**Response**
Returns updated scenario details with success message.

```mermaid
sequenceDiagram
participant Client
participant API
participant Simulator
participant Database
Client->>API : PUT /simulator/scenarios/{id} with JSON body
API->>API : Extract scenario ID from path
API->>API : Extract user ID from JWT
API->>Simulator : handle_update_scenario(scenario_id, request_body, user_id)
Simulator->>Database : Validate scenario access
Database-->>Simulator : Access validation result
alt Access granted
Simulator->>Simulator : Parse update request
Simulator->>Database : Update scenario record
Database-->>Simulator : Confirmation
Simulator-->>API : Return updated scenario
API-->>Client : 200 OK with updated details
else Access denied
API-->>Client : 404 Not Found
end
```

**Diagram sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L152-L180)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2579-L2594)

**Section sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L152-L180)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2579-L2594)

### Delete Scenario (DELETE /simulator/scenarios/{id})
Removes a simulation scenario from the system.

**Response**
Returns 204 No Content on success, or appropriate error code.

```mermaid
sequenceDiagram
participant Client
participant API
participant Simulator
participant Database
Client->>API : DELETE /simulator/scenarios/{id}
API->>API : Extract scenario ID from path
API->>API : Extract user ID from JWT
API->>Simulator : handle_delete_scenario(scenario_id, user_id)
Simulator->>Database : Validate scenario access
Database-->>Simulator : Access validation result
alt Access granted
Simulator->>Database : Mark scenario as inactive
Database-->>Simulator : Confirmation
Simulator-->>API : Success response
API-->>Client : 204 No Content
else Access denied
API-->>Client : 404 Not Found
end
```

**Diagram sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L208-L238)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2612-L2627)

**Section sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L208-L238)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2612-L2627)

## Simulation Execution Endpoints

### Run Simulation (POST /simulator/run)
Initiates a simulation execution for a specified scenario.

**Request Body**
```json
{
  "scenario_id": "string",
  "async_execution": boolean,
  "priority": integer,
  "custom_parameters": object,
  "test_data_override": object
}
```

**Response**
Returns execution details:
- `execution_id`: Unique identifier for the simulation execution
- `status`: Current status (running)
- `message`: Execution mode information

```mermaid
sequenceDiagram
participant Client
participant API
participant Simulator
participant Database
Client->>API : POST /simulator/run with JSON body
API->>API : Check rate limiting
API->>API : Extract user ID from JWT
API->>API : Parse and validate request
API->>Simulator : handle_run_simulation(request_body, user_id)
Simulator->>Simulator : validate_scenario_access()
Simulator->>Simulator : parse_simulation_request()
Simulator->>Simulator : run_simulation()
Simulator->>Database : Create execution record
Database-->>Simulator : Execution ID
Simulator->>Simulator : Start async execution thread
Simulator-->>API : Return execution details
API-->>Client : 200 OK with execution details
```

**Diagram sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L181-L210)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2648-L2670)

**Section sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L181-L210)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2648-L2670)

### Get Execution Status (GET /simulator/executions/{id})
Retrieves the current status of a simulation execution.

**Response**
Returns execution details including:
- Execution status (pending, running, completed, failed, cancelled)
- Progress percentage
- Timestamps (created, started, completed, cancelled)
- Error message (if applicable)

```mermaid
sequenceDiagram
participant Client
participant API
participant Simulator
participant Database
Client->>API : GET /simulator/executions/{id}
API->>API : Extract execution ID from path
API->>API : Extract user ID from JWT
API->>Simulator : handle_get_execution_status(execution_id, user_id)
Simulator->>Database : Validate execution access
Database-->>Simulator : Access validation result
alt Access granted
Simulator->>Database : Get execution status
Database-->>Simulator : Return execution data
Simulator-->>API : Return formatted execution
API-->>Client : 200 OK with execution details
else Access denied
API-->>Client : 404 Not Found
end
```

**Diagram sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L210-L242)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2725-L2747)

**Section sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L210-L242)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2725-L2747)

### Cancel Execution (DELETE /simulator/executions/{id})
Terminates a running simulation execution.

**Response**
Returns execution details with updated status.

```mermaid
sequenceDiagram
participant Client
participant API
participant Simulator
participant Database
Client->>API : DELETE /simulator/executions/{id}
API->>API : Extract execution ID from path
API->>API : Extract user ID from JWT
API->>Simulator : handle_cancel_simulation(execution_id, user_id)
Simulator->>Database : Validate execution access
Database-->>Simulator : Access validation result
alt Access granted
Simulator->>Simulator : cancel_simulation()
Simulator->>Database : Update execution status to cancelled
Database-->>Simulator : Confirmation
Simulator-->>API : Return execution details
API-->>Client : 200 OK with execution details
else Access denied
API-->>Client : 404 Not Found
end
```

**Diagram sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L271-L299)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2792-L2807)

**Section sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L271-L299)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2792-L2807)

### Retrieve Results (GET /simulator/results/{id})
Gets the complete results of a completed simulation execution.

**Response**
Returns detailed simulation results including:
- Impact analysis and summary
- Affected entities
- Recommendations
- Risk, cost, and compliance impacts
- Operational impact assessment

```mermaid
sequenceDiagram
participant Client
participant API
participant Simulator
participant Database
Client->>API : GET /simulator/results/{id}
API->>API : Extract execution ID from path
API->>API : Extract user ID from JWT
API->>Simulator : handle_get_simulation_result(execution_id, user_id)
Simulator->>Database : Validate execution access
Database-->>Simulator : Access validation result
alt Access granted
Simulator->>Database : Check execution status
Database-->>Simulator : Status data
alt Execution completed
Simulator->>Database : Get simulation result
Database-->>Simulator : Return result data
Simulator-->>API : Return formatted result
API-->>Client : 200 OK with result details
else Execution not completed
API-->>Client : 202 Accepted
end
else Access denied
API-->>Client : 404 Not Found
end
```

**Diagram sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L244-L271)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2808-L2830)

**Section sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L244-L271)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2808-L2830)

## Template-Based Scenario Creation

### Create Scenario from Template (POST /simulator/templates/{id}/create-scenario)
Creates a new simulation scenario based on a predefined template.

**Response**
Returns the newly created scenario with template-derived parameters.

```mermaid
sequenceDiagram
participant Client
participant API
participant Simulator
participant Database
Client->>API : POST /simulator/templates/{id}/create-scenario
API->>API : Extract template ID from path
API->>API : Extract user ID from JWT
API->>Simulator : handle_create_scenario_from_template(template_id, user_id)
Simulator->>Database : Get template details
Database-->>Simulator : Return template data
Simulator->>Simulator : Create scenario from template
Simulator->>Database : Insert new scenario
Database-->>Simulator : Confirmation
Simulator-->>API : Return created scenario
API-->>Client : 201 Created with scenario details
```

**Diagram sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L400-L428)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2700-L2716)

**Section sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L400-L428)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2700-L2716)

## Analytics Endpoints

### Get Analytics (GET /simulator/analytics)
Retrieves simulation analytics with caching for performance.

**Query Parameters**
- `time_range` (optional): Time period for analytics (e.g., "7d", "30d", "90d")

**Response**
Returns comprehensive analytics including:
- Total scenarios and executions
- Success rate
- Average duration
- Popular scenarios
- Execution trends

```mermaid
sequenceDiagram
participant Client
participant API
participant Simulator
participant Cache
participant Database
Client->>API : GET /simulator/analytics
API->>API : Extract user ID from JWT
API->>API : Parse query parameters
API->>Simulator : handle_get_simulation_analytics(user_id, query_params)
Simulator->>Cache : Check for cached analytics
alt Cache hit
Cache-->>Simulator : Return cached data
Simulator-->>API : Return analytics
else Cache miss
Simulator->>Database : Calculate analytics
Database-->>Simulator : Return analytics data
Simulator->>Cache : Store analytics result
Simulator-->>API : Return analytics
end
API-->>Client : 200 OK with analytics data
```

**Diagram sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L428-L456)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2881-L2898)

**Section sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L428-L456)
- [api_endpoint_registrations.cpp](file://shared/api_registry/api_endpoint_registrations.cpp#L2881-L2898)

## Request Schemas

### Scenario Creation Request
The request body for creating a new simulation scenario includes regulatory change parameters, business rules, and time horizons.

```json
{
  "scenario_name": "string",
  "description": "string",
  "scenario_type": "regulatory_change",
  "regulatory_changes": {
    "jurisdiction": "string",
    "regulatory_body": "string",
    "effective_date": "string",
    "changes": [
      {
        "rule_id": "string",
        "change_type": "new|amended|removed",
        "old_value": "string",
        "new_value": "string"
      }
    ]
  },
  "impact_parameters": {
    "time_horizon": "short|medium|long",
    "confidence_level": 0.0-1.0,
    "risk_tolerance": "conservative|moderate|aggressive"
  },
  "baseline_data": {
    "data_source": "string",
    "time_period": "string",
    "filters": {}
  },
  "test_data": {
    "data_source": "string",
    "time_period": "string",
    "filters": {}
  },
  "tags": ["string"],
  "metadata": {},
  "estimated_runtime_seconds": 300,
  "max_concurrent_simulations": 1,
  "is_template": false
}
```

**Section sources**
- [regulatory_simulator.hpp](file://shared/simulator/regulatory_simulator.hpp#L30-L80)
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L449-L486)

## Architecture Overview

### System Architecture
The Simulation API follows a layered architecture with clear separation of concerns between API handlers, business logic, and data access.

```mermaid
graph TD
A[Client Applications] --> B[API Gateway]
B --> C[Simulator API Handlers]
C --> D[Regulatory Simulator Service]
D --> E[Database Layer]
D --> F[Caching Layer]
D --> G[Logging Service]
subgraph "Simulator API Handlers"
C1[simulator_api_handlers.cpp]
C2[Request Parsing]
C3[Response Formatting]
C4[Access Control]
end
subgraph "Regulatory Simulator Service"
D1[regulatory_simulator.cpp]
D2[Scenario Management]
D3[Simulation Execution]
D4[Impact Analysis]
D5[Result Processing]
end
subgraph "Database Layer"
E1[PostgreSQL]
E2[simulation_scenarios]
E3[simulation_executions]
E4[simulation_results]
E5[simulation_templates]
end
subgraph "Caching Layer"
F1[In-Memory Cache]
F2[Analytics Results]
F3[Frequently Accessed Data]
end
subgraph "Logging Service"
G1[Structured Logger]
G2[Audit Trail]
G3[Performance Metrics]
end
```

**Diagram sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L1-L1260)
- [regulatory_simulator.cpp](file://shared/simulator/regulatory_simulator.cpp#L1-L935)
- [schema.sql](file://schema.sql#L7308-L7416)

**Section sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L1-L1260)
- [regulatory_simulator.cpp](file://shared/simulator/regulatory_simulator.cpp#L1-L935)

## Error Handling

### Error Response Structure
All API endpoints return standardized error responses with consistent structure.

```json
{
  "success": false,
  "error": "string",
  "status_code": 400,
  "timestamp": 1234567890
}
```

### Common Error Codes
- **400 Bad Request**: Invalid request format or missing required fields
- **401 Unauthorized**: Authentication required or invalid credentials
- **403 Forbidden**: Insufficient permissions for requested operation
- **404 Not Found**: Requested resource not found or access denied
- **429 Too Many Requests**: Rate limit exceeded
- **500 Internal Server Error**: Unexpected server error

```mermaid
flowchart TD
A[API Request] --> B{Valid JSON?}
B --> |No| C[400 Bad Request]
B --> |Yes| D{Required Fields Present?}
D --> |No| C
D --> |Yes| E{User Authenticated?}
E --> |No| F[401 Unauthorized]
E --> |Yes| G{Access Permitted?}
G --> |No| H[403 Forbidden]
G --> |Yes| I{Resource Exists?}
I --> |No| J[404 Not Found]
I --> |Yes| K{Rate Limit Exceeded?}
K --> |Yes| L[429 Too Many Requests]
K --> |No| M[Process Request]
M --> N{Success?}
N --> |Yes| O[Return Success Response]
N --> |No| P[500 Internal Server Error]
```

**Diagram sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L1168-L1196)
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L1196-L1225)

**Section sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L1168-L1258)

## Security and Access Control

### Access Validation
The system implements role-based access control with validation at multiple levels.

```mermaid
sequenceDiagram
participant Client
participant API
participant Security
participant Database
Client->>API : API Request
API->>Security : Extract JWT Token
Security->>Security : Validate Token Signature
Security-->>API : User Claims
API->>Security : Check Required Permissions
Security-->>API : Permission Status
API->>Database : Validate Resource Access
Database-->>API : Access Validation Result
alt Access Granted
API->>BusinessLogic : Process Request
else Access Denied
API-->>Client : 403 Forbidden
end
```

### Security Features
- JWT-based authentication and authorization
- Role-based access control (RBAC)
- Rate limiting to prevent abuse
- Input validation and sanitization
- Secure database queries with parameterized statements
- Comprehensive audit logging

**Section sources**
- [simulator_api_handlers.cpp](file://shared/simulator/simulator_api_handlers.cpp#L580-L613)
- [regulatory_simulator.hpp](file://shared/simulator/regulatory_simulator.hpp#L130-L152)