# LLM Integration

<cite>
**Referenced Files in This Document**   
- [llm_interface.hpp](file://shared/agentic_brain/llm_interface.hpp)
- [llm_interface.cpp](file://shared/agentic_brain/llm_interface.cpp)
- [openai_client.hpp](file://shared/llm/openai_client.hpp)
- [openai_client.cpp](file://shared/llm/openai_client.cpp)
- [anthropic_client.hpp](file://shared/llm/anthropic_client.hpp)
- [anthropic_client.cpp](file://shared/llm/anthropic_client.cpp)
- [function_calling.hpp](file://shared/llm/function_calling.hpp)
- [policy_generation_service.hpp](file://shared/llm/policy_generation_service.hpp)
- [text_analysis_service.hpp](file://shared/llm/text_analysis_service.hpp)
- [llm_api_handlers.cpp](file://shared/llm/llm_api_handlers.cpp)
- [useLLM.ts](file://frontend/src/hooks/useLLM.ts)
- [LLMIntegration.tsx](file://frontend/src/pages/LLMIntegration.tsx)
</cite>

## Table of Contents
1. [Introduction](#introduction)
2. [Project Structure](#project-structure)
3. [Core Components](#core-components)
4. [Architecture Overview](#architecture-overview)
5. [Detailed Component Analysis](#detailed-component-analysis)
6. [Dependency Analysis](#dependency-analysis)
7. [Performance Considerations](#performance-considerations)
8. [Troubleshooting Guide](#troubleshooting-guide)
9. [Conclusion](#conclusion)

## Introduction
The LLM Integration system provides a comprehensive multi-provider interface for Large Language Model interactions within the Regulens platform. This architecture enables seamless integration with OpenAI and Anthropic services while supporting local LLM deployments. The system is designed for enterprise-grade compliance applications, featuring robust security, rate limiting, caching, and failover capabilities. It serves as the foundation for policy generation, text analysis, function calling, and regulatory intelligence across the platform.

## Project Structure
The LLM integration components are organized across multiple directories with clear separation of concerns. The core LLM interface resides in the shared/agentic_brain directory, while provider-specific implementations are located in shared/llm. Frontend integration is handled through React hooks and components in the frontend/src directory.

```mermaid
graph TD
subgraph "Backend"
A[shared/agentic_brain]
B[shared/llm]
C[shared/config]
D[shared/cache]
E[shared/database]
end
subgraph "Frontend"
F[frontend/src/hooks]
G[frontend/src/pages]
H[frontend/src/services]
end
A --> B
B --> C
B --> D
B --> E
F --> G
F --> H
```

**Diagram sources**
- [llm_interface.hpp](file://shared/agentic_brain/llm_interface.hpp)
- [openai_client.hpp](file://shared/llm/openai_client.hpp)
- [anthropic_client.hpp](file://shared/llm/anthropic_client.hpp)
- [useLLM.ts](file://frontend/src/hooks/useLLM.ts)

**Section sources**
- [llm_interface.hpp](file://shared/agentic_brain/llm_interface.hpp)
- [openai_client.hpp](file://shared/llm/openai_client.hpp)
- [anthropic_client.hpp](file://shared/llm/anthropic_client.hpp)

## Core Components
The LLM integration system consists of several core components that work together to provide a robust multi-provider interface. The LLMInterface serves as the primary abstraction layer, while OpenAIClient and AnthropicClient handle provider-specific implementations. Supporting components include function calling frameworks, policy generation services, and text analysis utilities.

**Section sources**
- [llm_interface.hpp](file://shared/agentic_brain/llm_interface.hpp)
- [openai_client.hpp](file://shared/llm/openai_client.hpp)
- [anthropic_client.hpp](file://shared/llm/anthropic_client.hpp)

## Architecture Overview
The LLM integration architecture follows a layered design pattern with clear separation between the interface layer, provider implementations, and application services. The system supports multiple LLM providers through a unified interface while maintaining provider-specific optimizations.

```mermaid
graph TB
subgraph "Frontend"
UI[React Components]
Hooks[useLLM Hook]
end
subgraph "API Layer"
API[LLM API Handlers]
end
subgraph "Service Layer"
Interface[LLM Interface]
FunctionCalling[Function Calling Framework]
PolicyGen[Policy Generation Service]
TextAnalysis[Text Analysis Service]
end
subgraph "Provider Layer"
OpenAI[OpenAI Client]
Anthropic[Anthropic Client]
Local[Local LLM Client]
end
subgraph "Infrastructure"
Config[Configuration Manager]
Cache[Redis Client]
DB[PostgreSQL]
Logger[Structured Logger]
end
UI --> Hooks
Hooks --> API
API --> Interface
Interface --> FunctionCalling
Interface --> PolicyGen
Interface --> TextAnalysis
Interface --> OpenAI
Interface --> Anthropic
Interface --> Local
OpenAI --> Config
OpenAI --> Cache
OpenAI --> DB
OpenAI --> Logger
Anthropic --> Config
Anthropic --> Cache
Anthropic --> DB
Anthropic --> Logger
Local --> Config
Local --> Cache
Local --> DB
Local --> Logger
```

**Diagram sources**
- [llm_interface.hpp](file://shared/agentic_brain/llm_interface.hpp)
- [openai_client.hpp](file://shared/llm/openai_client.hpp)
- [anthropic_client.hpp](file://shared/llm/anthropic_client.hpp)
- [function_calling.hpp](file://shared/llm/function_calling.hpp)
- [policy_generation_service.hpp](file://shared/llm/policy_generation_service.hpp)
- [text_analysis_service.hpp](file://shared/llm/text_analysis_service.hpp)

## Detailed Component Analysis

### LLM Interface Analysis
The LLMInterface provides a unified abstraction over multiple LLM providers, enabling seamless switching between OpenAI, Anthropic, and local LLMs. It handles configuration, rate limiting, and provider selection while exposing a consistent API for downstream services.

```mermaid
classDiagram
class LLMInterface {
+LLMProvider current_provider_
+LLMModel current_model_
+configure_provider(LLMProvider, json) bool
+set_model(LLMModel) bool
+generate_completion(LLMRequest) LLMResponse
+analyze_text(string, string) LLMResponse
+make_decision(json, string) LLMResponse
-call_openai(LLMRequest) LLMResponse
-call_anthropic(LLMRequest) LLMResponse
-prepare_openai_request(LLMRequest) json
-prepare_anthropic_request(LLMRequest) json
}
class LLMProvider {
<<enumeration>>
OPENAI
ANTHROPIC
LOCAL_LLM
}
class LLMModel {
<<enumeration>>
GPT_4_TURBO
GPT_4
GPT_3_5_TURBO
CLAUDE_3_OPUS
CLAUDE_3_SONNET
CLAUDE_3_HAIKU
LLAMA_3_70B
MISTRAL_7B
}
class LLMRequest {
+string model
+vector~LLMMessage~ messages
+string system_prompt
+double temperature
+int max_tokens
}
class LLMResponse {
+bool success
+string content
+double confidence_score
+int tokens_used
+LLMModel model_used
}
class LLMMessage {
+string role
+string content
}
class RateLimiter {
-int max_requests_
-chrono : : steady_clock : : duration window_duration_
+allow_request() bool
}
LLMInterface --> LLMProvider
LLMInterface --> LLMModel
LLMInterface --> LLMRequest
LLMInterface --> LLMResponse
LLMRequest --> LLMMessage
LLMInterface --> RateLimiter : "uses"
```

**Diagram sources**
- [llm_interface.hpp](file://shared/agentic_brain/llm_interface.hpp#L20-L200)

**Section sources**
- [llm_interface.hpp](file://shared/agentic_brain/llm_interface.hpp#L20-L200)
- [llm_interface.cpp](file://shared/agentic_brain/llm_interface.cpp#L20-L200)

### OpenAI Client Analysis
The OpenAIClient implements provider-specific functionality for OpenAI's API, including advanced features like function calling, streaming responses, and response caching. It handles authentication, rate limiting, and error recovery with enterprise-grade reliability.

```mermaid
sequenceDiagram
participant Frontend
participant API
participant OpenAIClient
participant OpenAI
participant Redis
participant DB
Frontend->>API : POST /llm/analyze
API->>OpenAIClient : analyze_text()
OpenAIClient->>Redis : Check cache
Redis-->>OpenAIClient : No cached result
OpenAIClient->>OpenAIClient : Check rate limit
OpenAIClient->>OpenAIClient : Apply circuit breaker
OpenAIClient->>OpenAI : API Request
OpenAI-->>OpenAIClient : Response
OpenAIClient->>OpenAIClient : Parse response
OpenAIClient->>DB : Store analysis
OpenAIClient->>Redis : Cache result
OpenAIClient-->>API : LLMResponse
API-->>Frontend : Analysis result
```

**Diagram sources**
- [openai_client.hpp](file://shared/llm/openai_client.hpp#L20-L200)
- [openai_client.cpp](file://shared/llm/openai_client.cpp#L20-L200)

**Section sources**
- [openai_client.hpp](file://shared/llm/openai_client.hpp#L20-L200)
- [openai_client.cpp](file://shared/llm/openai_client.cpp#L20-L200)

### Anthropic Client Analysis
The AnthropicClient provides specialized integration with Anthropic's Claude models, supporting advanced reasoning capabilities and long context windows. It implements similar reliability features as the OpenAI client while adapting to Anthropic's API specifications.

```mermaid
flowchart TD
A[Initialize Client] --> B{Configuration Valid?}
B --> |No| C[Log Error]
B --> |Yes| D[Load API Key]
D --> E[Set Base URL]
E --> F[Configure Rate Limiter]
F --> G[Initialize Redis Cache]
G --> H[Test Connection]
H --> I{Connection Successful?}
I --> |No| J[Log Warning]
I --> |Yes| K[Client Ready]
K --> L[Process Requests]
L --> M[Check Rate Limit]
M --> N{Within Limit?}
N --> |No| O[Return Rate Limit Error]
N --> |Yes| P[Check Cache]
P --> Q{Cached Result?}
Q --> |Yes| R[Return Cached Response]
Q --> |No| S[Make API Call]
S --> T[Parse Response]
T --> U[Update Usage Stats]
U --> V[Cache Response]
V --> W[Return Result]
```

**Diagram sources**
- [anthropic_client.hpp](file://shared/llm/anthropic_client.hpp#L20-L200)
- [anthropic_client.cpp](file://shared/llm/anthropic_client.cpp#L20-L200)

**Section sources**
- [anthropic_client.hpp](file://shared/llm/anthropic_client.hpp#L20-L200)
- [anthropic_client.cpp](file://shared/llm/anthropic_client.cpp#L20-L200)

### Function Calling Framework
The function calling framework enables secure execution of external functions through LLM-generated calls, with comprehensive validation and auditing. This allows the LLM to interact with external systems while maintaining security and compliance.

```mermaid
classDiagram
class FunctionRegistry {
+register_function(FunctionDefinition) bool
+unregister_function(string) bool
+execute_function(FunctionCall, FunctionContext) FunctionResult
+get_function_definitions_for_api(vector~string~) json
}
class FunctionDefinition {
+string name
+string description
+json parameters_schema
+function~FunctionResult(json, FunctionContext)~ executor
+vector~string~ required_permissions
}
class FunctionCall {
+string name
+json arguments
+string call_id
+validate_parameters(json) ValidationResult
}
class FunctionResult {
+bool success
+json result
+string error_message
+chrono : : milliseconds execution_time
}
class FunctionContext {
+string agent_id
+string agent_type
+vector~string~ permissions
+string correlation_id
}
FunctionRegistry --> FunctionDefinition : "contains"
FunctionRegistry --> FunctionCall : "processes"
FunctionRegistry --> FunctionResult : "returns"
FunctionCall --> FunctionContext : "executes with"
```

**Diagram sources**
- [function_calling.hpp](file://shared/llm/function_calling.hpp#L20-L200)

**Section sources**
- [function_calling.hpp](file://shared/llm/function_calling.hpp#L20-L200)

### Policy Generation Service
The policy generation service leverages LLM capabilities to convert natural language descriptions into structured compliance rules. It supports multiple output formats and includes validation and testing capabilities.

```mermaid
sequenceDiagram
participant User
participant Frontend
participant PolicyService
participant OpenAIClient
participant DB
User->>Frontend : Submit policy request
Frontend->>PolicyService : generate_policy()
PolicyService->>PolicyService : Validate request
PolicyService->>PolicyService : Build prompt
PolicyService->>OpenAIClient : generate_completion()
OpenAIClient-->>PolicyService : LLM response
PolicyService->>PolicyService : Parse rule
PolicyService->>PolicyService : Validate rule
PolicyService->>PolicyService : Generate tests
PolicyService->>DB : Store rule
PolicyService-->>Frontend : PolicyGenerationResult
Frontend-->>User : Display generated policy
```

**Diagram sources**
- [policy_generation_service.hpp](file://shared/llm/policy_generation_service.hpp#L20-L200)

**Section sources**
- [policy_generation_service.hpp](file://shared/llm/policy_generation_service.hpp#L20-L200)

### Text Analysis Service
The text analysis service provides multi-task NLP capabilities including sentiment analysis, entity extraction, summarization, and classification. It supports caching and batch processing for improved performance.

```mermaid
flowchart TD
A[analyze_text] --> B{Enable Caching?}
B --> |Yes| C[Generate text hash]
C --> D[Check Redis cache]
D --> E{Cached result?}
E --> |Yes| F[Return cached result]
E --> |No| G[Process tasks]
B --> |No| G
G --> H[For each task]
H --> I{Task type?}
I --> |Sentiment| J[analyze_sentiment]
I --> |Entities| K[extract_entities]
I --> |Summary| L[summarize_text]
I --> |Classification| M[classify_topics]
H --> N{More tasks?}
N --> |Yes| H
N --> |No| O[Combine results]
O --> P[Store in cache]
P --> Q[Return result]
```

**Diagram sources**
- [text_analysis_service.hpp](file://shared/llm/text_analysis_service.hpp#L20-L200)

**Section sources**
- [text_analysis_service.hpp](file://shared/llm/text_analysis_service.hpp#L20-L200)

## Dependency Analysis
The LLM integration system has well-defined dependencies on configuration management, caching, database, and logging components. These dependencies enable the system to maintain state, improve performance, and ensure reliability.

```mermaid
graph LR
LLMInterface --> ConfigurationManager
LLMInterface --> StructuredLogger
LLMInterface --> HttpClient
OpenAIClient --> ConfigurationManager
OpenAIClient --> StructuredLogger
OpenAIClient --> ErrorHandler
OpenAIClient --> RedisClient
OpenAIClient --> HttpClient
AnthropicClient --> ConfigurationManager
AnthropicClient --> StructuredLogger
AnthropicClient --> ErrorHandler
AnthropicClient --> RedisClient
AnthropicClient --> HttpClient
PolicyGenerationService --> PostgreSQLConnection
PolicyGenerationService --> OpenAIClient
TextAnalysisService --> PostgreSQLConnection
TextAnalysisService --> OpenAIClient
TextAnalysisService --> RedisClient
FunctionRegistry --> ConfigurationManager
FunctionRegistry --> StructuredLogger
FunctionRegistry --> ErrorHandler
classDef dependency fill:#f9f,stroke:#333;
class ConfigurationManager,StructuredLogger,HttpClient,RedisClient,ErrorHandler,PostgreSQLConnection dependency;
```

**Diagram sources**
- [llm_interface.hpp](file://shared/agentic_brain/llm_interface.hpp)
- [openai_client.hpp](file://shared/llm/openai_client.hpp)
- [anthropic_client.hpp](file://shared/llm/anthropic_client.hpp)
- [policy_generation_service.hpp](file://shared/llm/policy_generation_service.hpp)
- [text_analysis_service.hpp](file://shared/llm/text_analysis_service.hpp)
- [function_calling.hpp](file://shared/llm/function_calling.hpp)

**Section sources**
- [llm_interface.hpp](file://shared/agentic_brain/llm_interface.hpp)
- [openai_client.hpp](file://shared/llm/openai_client.hpp)
- [anthropic_client.hpp](file://shared/llm/anthropic_client.hpp)

## Performance Considerations
The LLM integration system incorporates several performance optimization strategies including Redis-based response caching, rate limiting, connection pooling, and efficient memory management. The system is designed to handle high volumes of requests while maintaining low latency through asynchronous processing and streaming responses.

**Section sources**
- [openai_client.cpp](file://shared/llm/openai_client.cpp#L100-L200)
- [anthropic_client.cpp](file://shared/llm/anthropic_client.cpp#L100-L200)

## Troubleshooting Guide
Common issues with the LLM integration system typically relate to configuration, authentication, or network connectivity. The system provides comprehensive logging through the StructuredLogger component, with detailed error messages and correlation IDs for tracing requests across components.

**Section sources**
- [llm_interface.cpp](file://shared/agentic_brain/llm_interface.cpp#L500-L600)
- [openai_client.cpp](file://shared/llm/openai_client.cpp#L800-L900)
- [anthropic_client.cpp](file://shared/llm/anthropic_client.cpp#L800-L900)

## Conclusion
The LLM integration architecture provides a robust, enterprise-grade interface for multiple LLM providers with comprehensive support for policy generation, text analysis, and function calling. The system's modular design enables easy extension to additional providers while maintaining high reliability through rate limiting, caching, and circuit breaker patterns. Security is prioritized through proper API key management and permission-based function execution, making it suitable for regulated financial compliance applications.