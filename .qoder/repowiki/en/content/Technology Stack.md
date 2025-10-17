# Technology Stack

<cite>
**Referenced Files in This Document**   
- [package.json](file://package.json)
- [vite.config.ts](file://frontend/vite.config.ts)
- [tailwind.config.js](file://frontend/tailwind.config.js)
- [tsconfig.json](file://frontend/tsconfig.json)
- [playwright.config.ts](file://playwright.config.ts)
- [CMakeLists.txt](file://CMakeLists.txt)
- [compliance_agent_crd.yaml](file://infrastructure/k8s/crds/compliance_agent_crd.yaml)
- [alerting-rules.yml](file://infrastructure/monitoring/prometheus/alerting-rules.yml)
- [system-overview.json](file://infrastructure/monitoring/grafana/dashboards/system-overview.json)
- [redis_client.hpp](file://shared/cache/redis_client.hpp)
- [postgresql_connection.hpp](file://shared/database/postgresql_connection.hpp)
- [websocket_server.hpp](file://shared/websocket/websocket_server.hpp)
- [prometheus_client.hpp](file://shared/metrics/prometheus_client.hpp)
- [prometheus_metrics.hpp](file://shared/metrics/prometheus_metrics.hpp)
- [schema.sql](file://schema.sql)
</cite>

## Table of Contents
1. [Frontend Stack](#frontend-stack)
2. [Backend Stack](#backend-stack)
3. [Infrastructure Technologies](#infrastructure-technologies)
4. [Integration Patterns](#integration-patterns)
5. [Development Tooling](#development-tooling)
6. [Scalability Characteristics](#scalability-characteristics)

## Frontend Stack

The Regulens frontend stack is built on modern web technologies optimized for real-time compliance monitoring and regulatory intelligence. The architecture combines React 18's concurrent rendering capabilities with TypeScript's type safety, Vite's rapid development server, Tailwind CSS for utility-first styling, TanStack Query for data synchronization, and Playwright for comprehensive end-to-end testing.

React 18 provides the foundation for building dynamic user interfaces with features like automatic batching, transition APIs, and suspense for data fetching. The component architecture in the `frontend/src/components` directory demonstrates a well-organized structure with specialized components for compliance monitoring, decision engines, rule management, and simulation tools. The use of React Context API in `frontend/src/contexts/AuthContext.tsx` enables global state management for authentication state across the application.

TypeScript is configured with relaxed strictness settings in `frontend/tsconfig.json` to balance type safety with development velocity, particularly important for a complex compliance system with evolving requirements. The configuration targets ES2020 and uses ESNext modules, ensuring compatibility with modern browsers while leveraging contemporary JavaScript features.

Vite serves as the build tool and development server, configured in `frontend/vite.config.ts` with React plugin support, TypeScript path aliases, and proxy configuration for seamless integration with the backend API. The development server proxies API requests to the backend service, enabling smooth development workflows. Vite's optimized build configuration includes code splitting for vendor libraries like React, D3, and Recharts, improving initial load performance.

Tailwind CSS provides the styling framework with a custom color palette defined in `frontend/tailwind.config.js`, featuring primary and dark color schemes optimized for compliance dashboards. The configuration enables dark mode support and scans all TypeScript and JSX files for class usage, ensuring optimal CSS output.

TanStack Query (formerly React Query) is implicitly used through custom hooks in `frontend/src/hooks` such as `useRuleEngine.ts`, `useSimulator.ts`, and `useRegulatoryChanges.ts`, providing efficient data fetching, caching, and synchronization between the frontend and backend services. This approach minimizes unnecessary API calls and ensures data consistency across multiple components.

Playwright enables comprehensive end-to-end testing with cross-browser support for Chromium, Firefox, and WebKit, as configured in `playwright.config.ts`. The testing framework supports multiple execution modes including headed, debug, and UI modes, with detailed reporting in HTML, JSON, and JUnit formats. Mobile viewport testing is included for responsive design validation on Pixel 5 and iPhone 13 devices.

**Section sources**
- [package.json](file://package.json)
- [vite.config.ts](file://frontend/vite.config.ts)
- [tailwind.config.js](file://frontend/tailwind.config.js)
- [tsconfig.json](file://frontend/tsconfig.json)
- [playwright.config.ts](file://playwright.config.ts)

## Backend Stack

The Regulens backend is implemented in C++17 with a custom event-driven framework designed for high-performance compliance processing and real-time regulatory monitoring. The architecture leverages modern C++ standards (C++20) with strict compilation flags for production reliability, as defined in the root `CMakeLists.txt` file.

The C++ codebase follows a modular structure with specialized components for different compliance domains, including audit intelligence, regulatory assessment, and transaction monitoring agents. These agents are implemented as C++ classes with header files in the `agents` directory, demonstrating object-oriented design principles for compliance logic encapsulation.

The custom framework incorporates several key architectural patterns:
- **Event-driven architecture**: Implemented through the event system in `shared/event_system`, enabling asynchronous communication between compliance components
- **REST API implementation**: Built on a custom web server framework with API endpoint configuration in `shared/api_config` and API registry in `shared/api_registry`
- **Database integration**: PostgreSQL client connectivity through `libpqxx` with connection pooling and prepared statements in `shared/database/postgresql_connection.hpp`
- **Caching layer**: Redis integration via `hiredis` with advanced features like connection pooling, health monitoring, and pub/sub capabilities in `shared/cache/redis_client.hpp`

The PostgreSQL schema, defined in `schema.sql`, is optimized for compliance data management with specialized tables for regulatory changes, compliance events, agent decisions, and knowledge base storage. The schema leverages PostgreSQL extensions including `pgvector` for vector embeddings, `pg_trgm` for text search, and `uuid-ossp` for UUID generation. Table partitioning and comprehensive indexing strategies ensure efficient querying of compliance data.

Redis serves as the primary caching layer with multiple use cases:
- LLM response caching with intelligent TTL based on prompt complexity
- Regulatory document caching with source-based expiration
- Agent session management with configurable TTL
- User preferences storage
- Temporary data with automatic eviction

The WebSocket server in `shared/websocket/websocket_server.hpp` enables real-time push notifications for regulatory updates, transaction alerts, and system notifications. The implementation supports topic-based subscriptions, message queuing, and connection management for thousands of concurrent clients.

**Section sources**
- [CMakeLists.txt](file://CMakeLists.txt)
- [redis_client.hpp](file://shared/cache/redis_client.hpp)
- [postgresql_connection.hpp](file://shared/database/postgresql_connection.hpp)
- [websocket_server.hpp](file://shared/websocket/websocket_server.hpp)
- [schema.sql](file://schema.sql)

## Infrastructure Technologies

Regulens employs a sophisticated infrastructure stack centered around Kubernetes for orchestration, Prometheus for monitoring, Grafana for visualization, and NGINX as the ingress controller. This infrastructure enables scalable, resilient deployment of compliance services with comprehensive observability.

Kubernetes is extended with custom operators defined in `infrastructure/k8s/crds/compliance_agent_crd.yaml`, which introduces a `ComplianceAgent` custom resource that encapsulates the lifecycle management of compliance agents. The CRD defines comprehensive configuration options including:
- Agent type specification (transaction_guardian, audit_intelligence, regulatory_assessor)
- Resource requests and limits for CPU and memory
- Configuration parameters for log levels, processing timeouts, and concurrency
- LLM integration settings with provider selection and model configuration
- Regulatory source polling with configurable intervals and priorities
- Horizontal pod autoscaling based on CPU and memory utilization
- Monitoring configuration with Prometheus integration

The custom operators in `infrastructure/k8s/operator` implement controllers for managing compliance agent deployments, ensuring desired state reconciliation and automated scaling based on workload demands. This approach enables declarative management of compliance workloads with self-healing capabilities.

Prometheus provides comprehensive monitoring with a rich set of alerting rules defined in `infrastructure/monitoring/prometheus/alerting-rules.yml`. The alerting system covers multiple dimensions:
- **System health**: Service availability, error rates, memory and CPU usage
- **Circuit breaker**: State changes, failure rates, and latency thresholds
- **LLM performance**: API availability, error rates, response latency, and cost monitoring
- **Redis cache**: Availability, error rates, cache hit rates, memory usage, and eviction rates
- **Compliance operations**: Decision failure rates, accuracy, processing latency, and SLA compliance
- **Data ingestion**: Error rates, latency, data freshness, and throughput
- **Kubernetes operator**: Operator availability, reconciliation latency, and scaling failures

Grafana dashboards visualize system metrics and compliance KPIs, with the system overview dashboard in `infrastructure/monitoring/grafana/dashboards/system-overview.json` providing real-time monitoring of service health, active connections, CPU and memory usage, request rates, and error rates. Additional dashboards cover circuit breaker monitoring, compliance operations, LLM performance, Redis cache performance, and regulatory data ingestion.

NGINX functions as the ingress controller, routing traffic to appropriate services and providing SSL termination, rate limiting, and request buffering. The configuration ensures secure and reliable access to the Regulens API and web interface.

**Section sources**
- [compliance_agent_crd.yaml](file://infrastructure/k8s/crds/compliance_agent_crd.yaml)
- [alerting-rules.yml](file://infrastructure/monitoring/prometheus/alerting-rules.yml)
- [system-overview.json](file://infrastructure/monitoring/grafana/dashboards/system-overview.json)

## Integration Patterns

The Regulens technology stack employs several integration patterns that enable seamless communication between frontend, backend, and infrastructure components:

**Frontend-Backend Integration**
The frontend communicates with the backend through a REST API with proxy configuration in `frontend/vite.config.ts` that routes `/api` requests to the backend service. The API supports JSON payloads with comprehensive error handling and authentication via JWT tokens. TanStack Query manages data synchronization with automatic refetching and caching strategies.

**Event-Driven Communication**
The backend uses an event-driven architecture where compliance agents publish events to an event bus, which are then processed by subscribers. This pattern enables loose coupling between components and supports real-time processing of regulatory changes and transaction monitoring.

**Caching Strategy**
A multi-layer caching strategy combines Redis for distributed caching with in-memory caching for frequently accessed data. The Redis client implements intelligent TTL calculation based on content type and importance, optimizing cache hit rates while ensuring data freshness.

**Real-Time Updates**
WebSocket connections provide real-time push notifications for regulatory updates, transaction alerts, and system notifications. The WebSocket server supports topic-based subscriptions, allowing clients to receive only relevant updates.

**Metrics Integration**
Prometheus metrics are exposed through a dedicated endpoint, with custom metrics collectors in `shared/metrics/prometheus_metrics.hpp` capturing business KPIs alongside system performance metrics. This integration enables alerting on both technical and business metrics.

**Configuration Management**
The system uses a hierarchical configuration model with environment-specific settings, allowing dynamic reconfiguration of compliance agents without restarts.

**Section sources**
- [vite.config.ts](file://frontend/vite.config.ts)
- [redis_client.hpp](file://shared/cache/redis_client.hpp)
- [websocket_server.hpp](file://shared/websocket/websocket_server.hpp)
- [prometheus_metrics.hpp](file://shared/metrics/prometheus_metrics.hpp)

## Development Tooling

Regulens provides comprehensive tooling for development, testing, and debugging across the technology stack:

**Frontend Development**
- Vite development server with hot module replacement for rapid iteration
- TypeScript with path aliases configured in `tsconfig.json`
- Tailwind CSS with custom theme configuration
- Playwright for end-to-end testing with cross-browser support
- Code generation capabilities through Playwright's codegen feature

**Backend Development**
- CMake build system with comprehensive configuration for production builds
- Modern C++ compiler flags for strict type checking and optimization
- Thread and address sanitizers for development builds
- Code coverage support for testing
- C++ debugger integration for troubleshooting complex compliance logic

**Database Tools**
- PostgreSQL client with connection pooling and prepared statements
- Database migration system for schema evolution
- Comprehensive schema with indexes and constraints defined in `schema.sql`
- Seed data for development and testing

**Testing Infrastructure**
- Playwright for frontend end-to-end testing with HTML, JSON, and JUnit reports
- C++ unit and integration tests with custom test framework
- API integration tests to validate backend functionality
- Critical data flow testing to ensure compliance accuracy

**Monitoring and Debugging**
- Prometheus metrics collection with custom business KPIs
- Grafana dashboards for system and compliance monitoring
- Structured logging with multiple logger implementations
- Health check endpoints for service monitoring
- Circuit breaker metrics for resilience monitoring

**Section sources**
- [vite.config.ts](file://frontend/vite.config.ts)
- [playwright.config.ts](file://playwright.config.ts)
- [CMakeLists.txt](file://CMakeLists.txt)
- [postgresql_connection.hpp](file://shared/database/postgresql_connection.hpp)
- [prometheus_client.hpp](file://shared/metrics/prometheus_client.hpp)

## Scalability Characteristics

The Regulens technology stack is designed for horizontal scalability across all components to handle the demanding requirements of real-time compliance monitoring:

**Frontend Scalability**
The React frontend is optimized for performance with code splitting, lazy loading, and efficient state management. Vite's build optimization minimizes bundle sizes, while TanStack Query's caching reduces backend load. The static assets can be served from a CDN for global distribution.

**Backend Scalability**
The C++ backend demonstrates excellent scalability characteristics:
- **Multi-threading support**: The event-driven architecture can leverage multiple CPU cores for parallel processing
- **Connection pooling**: Database and Redis connections are pooled to minimize overhead
- **Stateless design**: Compliance agents can be scaled horizontally without shared state
- **Message queuing**: Events are processed asynchronously, enabling load leveling

**Database Scalability**
PostgreSQL is configured for high performance with:
- Comprehensive indexing strategy including GIN indexes for JSONB data
- Table partitioning for time-series compliance data
- Connection pooling to handle thousands of concurrent connections
- Read replicas for query offloading
- Write-optimized storage configuration

**Caching Scalability**
Redis provides distributed caching with:
- Connection pooling for efficient resource utilization
- Pub/sub capabilities for real-time updates
- Configurable eviction policies to manage memory usage
- Cluster support for horizontal scaling

**Infrastructure Scalability**
Kubernetes enables dynamic scaling based on multiple metrics:
- CPU and memory utilization
- Request rates and latency
- Custom business metrics
- Time-based scaling for predictable workloads

The custom operators automatically scale compliance agents based on workload, with configurable minimum and maximum replica counts. The system can handle sudden spikes in regulatory monitoring activity or transaction volumes by automatically provisioning additional resources.

**Section sources**
- [CMakeLists.txt](file://CMakeLists.txt)
- [redis_client.hpp](file://shared/cache/redis_client.hpp)
- [postgresql_connection.hpp](file://shared/database/postgresql_connection.hpp)
- [compliance_agent_crd.yaml](file://infrastructure/k8s/crds/compliance_agent_crd.yaml)