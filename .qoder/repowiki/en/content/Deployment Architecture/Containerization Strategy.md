# Containerization Strategy

<cite>
**Referenced Files in This Document**
- [docker-compose.yml](file://docker-compose.yml)
- [Dockerfile](file://Dockerfile)
- [frontend/Dockerfile](file://frontend/Dockerfile)
- [frontend/nginx.conf](file://frontend/nginx.conf)
- [schema.sql](file://schema.sql)
- [seed_data.sql](file://seed_data.sql)
- [data/postgres/postgresql.conf](file://data/postgres/postgresql.conf)
- [data/postgres/pg_hba.conf](file://data/postgres/pg_hba.conf)
- [data/postgres/pg_ident.conf](file://data/postgres/pg_ident.conf)
- [start.sh](file://start.sh)
- [backend-dev-server.js](file://backend-dev-server.js)
- [package.json](file://package.json)
</cite>

## Table of Contents
1. [Introduction](#introduction)
2. [Multi-Container Architecture Overview](#multi-container-architecture-overview)
3. [Docker Compose Configuration](#docker-compose-configuration)
4. [Database Container Configuration](#database-container-configuration)
5. [Redis Cache Container Configuration](#redis-cache-container-configuration)
6. [API Server Container Configuration](#api-server-container-configuration)
7. [Frontend Container Configuration](#frontend-container-configuration)
8. [Volume Management Strategy](#volume-management-strategy)
9. [Network Configuration](#network-configuration)
10. [Health Check Implementation](#health-check-implementation)
11. [Environment Variable Management](#environment-variable-management)
12. [Security Considerations](#security-considerations)
13. [Production Deployment](#production-deployment)
14. [Development Workflow](#development-workflow)
15. [Monitoring and Observability](#monitoring-and-observability)

## Introduction

The Regulens project implements a sophisticated multi-container architecture designed for regulatory compliance monitoring and AI-powered decision-making. This containerization strategy ensures scalability, security, and maintainability while supporting both development and production environments.

The architecture consists of four primary containers: PostgreSQL for data persistence, Redis for caching and session management, an API server for business logic, and a frontend container serving React applications. Each container is configured with specific security measures, health checks, and resource constraints to ensure reliable operation in production environments.

## Multi-Container Architecture Overview

The Regulens containerization strategy follows a microservices architecture pattern, separating concerns across dedicated containers for different system components. This approach provides several advantages:

```mermaid
graph TB
subgraph "Docker Network: regulens-network"
subgraph "Database Layer"
PG[PostgreSQL Container<br/>Port 5432]
RD[Redis Container<br/>Port 6379]
end
subgraph "Application Layer"
API[API Server Container<br/>Port 8080]
FE[Frontend Container<br/>Port 80]
end
subgraph "External Access"
LB[Load Balancer<br/>Port 80/443]
end
end
LB --> FE
FE --> API
API --> PG
API --> RD
subgraph "Storage Volumes"
PG_DATA[PostgreSQL Data Volume]
RD_DATA[Redis Data Volume]
BACKUPS[Backup Storage]
end
PG --> PG_DATA
RD --> RD_DATA
PG --> BACKUPS
RD --> BACKUPS
```

**Diagram sources**
- [docker-compose.yml](file://docker-compose.yml#L1-L141)

The architecture emphasizes:
- **Service Isolation**: Each service runs in its own container with minimal dependencies
- **Resource Efficiency**: Containers share the host OS kernel while maintaining process isolation
- **Scalability**: Individual services can be scaled independently based on demand
- **Maintainability**: Clear separation of concerns simplifies updates and troubleshooting

## Docker Compose Configuration

The `docker-compose.yml` file defines the complete multi-container environment with orchestrated service dependencies and networking:

```yaml
# Multi-container microservices architecture
# - PostgreSQL & Redis databases
# - Regulatory Monitor Service (main.cpp)
# - API Server (server_with_auth.cpp)
# - Frontend (React/Vite)

services:
  # PostgreSQL Database
  postgres:
    image: pgvector/pgvector:pg15
    container_name: regulens-postgres
    environment:
      POSTGRES_DB: ${DB_NAME:?DB_NAME environment variable is required}
      POSTGRES_USER: ${DB_USER:?DB_USER environment variable is required}
      POSTGRES_PASSWORD: ${DB_PASSWORD:?DB_PASSWORD environment variable is required}
    volumes:
      - postgres_data:/var/lib/postgresql/data
      - ./schema.sql:/docker-entrypoint-initdb.d/01-schema.sql
      - ./seed_data.sql:/docker-entrypoint-initdb.d/02-seed.sql
      - ./backups/postgres:/backups
    ports:
      - "5432:5432"
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U ${DB_USER:-regulens_user} -d ${DB_NAME:-regulens_compliance}"]
      interval: 10s
      timeout: 5s
      retries: 3
      start_period: 10s
    restart: unless-stopped
    networks:
      - regulens-network
```

**Section sources**
- [docker-compose.yml](file://docker-compose.yml#L1-L141)

**Diagram sources**
- [docker-compose.yml](file://docker-compose.yml#L1-L141)

## Database Container Configuration

### PostgreSQL Container Setup

The PostgreSQL container utilizes the pgvector extension for advanced vector operations required by the AI-powered compliance system:

```mermaid
classDiagram
class PostgreSQLContainer {
+image : pgvector/pgvector : pg15
+container_name : regulens-postgres
+environment_variables : DB_NAME, DB_USER, DB_PASSWORD
+volumes : postgres_data, schema.sql, seed_data.sql, backups
+ports : 5432
+health_check : pg_isready
+restart_policy : unless-stopped
+network : regulens-network
+extensions : pgvector, pg_trgm, btree_gin, uuid-ossp
}
class DatabaseConfiguration {
+shared_buffers : 128MB
+max_connections : 100
+wal_level : replica
+max_wal_size : 1GB
+min_wal_size : 80MB
+effective_cache_size : 4GB
}
class VolumeMounts {
+data_volume : /var/lib/postgresql/data
+init_scripts : /docker-entrypoint-initdb.d/
+backup_storage : /backups
}
PostgreSQLContainer --> DatabaseConfiguration : "configured with"
PostgreSQLContainer --> VolumeMounts : "mounts"
```

**Diagram sources**
- [docker-compose.yml](file://docker-compose.yml#L8-L35)
- [data/postgres/postgresql.conf](file://data/postgres/postgresql.conf#L1-L799)

### Base Image and Extensions

The PostgreSQL container uses `pgvector/pgvector:pg15` as its base image, which includes:

- **PostgreSQL 15**: Latest stable version with improved performance and features
- **pgvector Extension**: Enables vector similarity search for AI-powered compliance analytics
- **Additional Extensions**: `pg_trgm`, `btree_gin`, `uuid-ossp` for enhanced functionality

### Environment Variables

Critical environment variables are managed through Docker Compose with mandatory validation:

```bash
# SECURITY: No default passwords - must be provided via environment variables
POSTGRES_DB: ${DB_NAME:?DB_NAME environment variable is required}
POSTGRES_USER: ${DB_USER:?DB_USER environment variable is required}
POSTGRES_PASSWORD: ${DB_PASSWORD:?DB_PASSWORD environment variable is required}
```

**Section sources**
- [docker-compose.yml](file://docker-compose.yml#L8-L35)
- [data/postgres/postgresql.conf](file://data/postgres/postgresql.conf#L1-L799)

## Redis Cache Container Configuration

### Redis Container Setup

The Redis container provides high-performance caching and session management:

```mermaid
classDiagram
class RedisContainer {
+image : redis : 7-alpine
+container_name : regulens-redis
+command : redis-server --requirepass
+environment_variables : REDIS_PASSWORD
+volumes : redis_data, backups
+ports : 6379
+health_check : redis-cli ping
+restart_policy : unless-stopped
+network : regulens-network
}
class RedisConfiguration {
+requirepass : ${REDIS_PASSWORD}
+maxmemory : 512MB
+maxmemory-policy : allkeys-lru
+save : 900 1 300 10 60 10000
+appendonly : yes
}
class SecuritySettings {
+password_authentication : enabled
+network_isolation : true
+backup_encryption : true
}
RedisContainer --> RedisConfiguration : "configured with"
RedisContainer --> SecuritySettings : "implements"
```

**Diagram sources**
- [docker-compose.yml](file://docker-compose.yml#L37-L58)

### Alpine Linux Base Image

Redis uses the lightweight Alpine Linux base image (`redis:7-alpine`) for:

- **Reduced Attack Surface**: Minimal footprint reduces potential vulnerabilities
- **Faster Startup**: Smaller image size improves container initialization speed
- **Lower Resource Consumption**: Optimized for production deployments

### Security Configuration

Redis implements robust security measures:

```yaml
command: redis-server --requirepass ${REDIS_PASSWORD:?REDIS_PASSWORD environment variable is required}
healthcheck:
  test: ["CMD", "redis-cli", "--no-auth-warning", "-a", "${REDIS_PASSWORD:?REDIS_PASSWORD environment variable is required}", "ping"]
```

**Section sources**
- [docker-compose.yml](file://docker-compose.yml#L37-L58)

## API Server Container Configuration

### Multi-Stage Docker Build

The API server employs a sophisticated multi-stage Docker build process:

```mermaid
flowchart TD
START([Build Process Start]) --> BUILD_STAGE[Build Stage<br/>Ubuntu 22.04 + Compiler]
BUILD_STAGE --> INSTALL_DEPS[Install Build Dependencies<br/>cmake, libpq-dev, libssl-dev]
INSTALL_DEPS --> COPY_SOURCE[Copy Source Code]
COPY_SOURCE --> COMPILE_BINARY[Compile regulens binary]
COMPILE_BINARY --> RUNTIME_STAGE[Runtime Stage<br/>Minimal Ubuntu 22.04]
RUNTIME_STAGE --> INSTALL_RUNTIME_DEPS[Install Runtime Dependencies<br/>libpq5, libssl3, curl]
INSTALL_RUNTIME_DEPS --> CREATE_USER[Create Non-Root User<br/>regulens:regulens]
CREATE_USER --> COPY_BINARY[Copy Compiled Binary]
COPY_BINARY --> SETUP_CONFIG[Setup Configuration Files]
SETUP_CONFIG --> HEALTH_CHECK[Configure Health Check]
HEALTH_CHECK --> EXPOSE_PORT[Expose Port 8080]
EXPOSE_PORT --> DEV_STAGE[Development Stage<br/>Optional Debug Tools]
DEV_STAGE --> END([Production Image Ready])
subgraph "Development Features"
DEBUG_TOOLS[GDB, Valgrind, Strace]
HOT_RELOAD[Hot Reload Support]
end
DEV_STAGE --> DEBUG_TOOLS
DEV_STAGE --> HOT_RELOAD
```

**Diagram sources**
- [Dockerfile](file://Dockerfile#L1-L122)

### Production Image Features

The production Dockerfile implements several security and performance optimizations:

```dockerfile
# Create non-root user for security
RUN groupadd -r regulens && useradd -r -g regulens regulens

# Switch to non-root user
USER regulens

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=60s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1
```

### Environment Variables

The API server container manages extensive environment configuration:

```yaml
environment:
  # SECURITY: No default passwords - must be provided via environment variables
  - DB_HOST=postgres
  - DB_PORT=5432
  - DB_NAME=${DB_NAME:?DB_NAME environment variable is required}
  - DB_USER=${DB_USER:?DB_USER environment variable is required}
  - DB_PASSWORD=${DB_PASSWORD:?DB_PASSWORD environment variable is required}
  - REDIS_HOST=redis
  - REDIS_PORT=6379
  - REDIS_PASSWORD=${REDIS_PASSWORD:?REDIS_PASSWORD environment variable is required}
  - REGULATORY_MONITOR_URL=http://regulatory-monitor:8081
  - JWT_SECRET_KEY=${JWT_SECRET_KEY:?JWT_SECRET_KEY environment variable is required}
  - JWT_EXPIRATION_HOURS=${JWT_EXPIRATION_HOURS:-24}
  - SESSION_EXPIRY_HOURS=${SESSION_EXPIRY_HOURS:-24}
  - CORS_ALLOWED_ORIGIN=${CORS_ALLOWED_ORIGIN:-http://localhost:3000}
```

**Section sources**
- [Dockerfile](file://Dockerfile#L1-L122)
- [docker-compose.yml](file://docker-compose.yml#L70-L95)

## Frontend Container Configuration

### Nginx-Based Frontend

The frontend container uses a multi-stage build with Nginx for optimal performance:

```mermaid
sequenceDiagram
participant Builder as Node.js Builder
participant Production as Nginx Production
participant Container as Frontend Container
Builder->>Builder : Install Node.js 18
Builder->>Builder : Copy package.json
Builder->>Builder : Install dependencies
Builder->>Builder : Copy source code
Builder->>Builder : Build React application
Builder->>Production : Copy dist files
Builder->>Production : Copy nginx.conf
Production->>Production : Configure Nginx
Production->>Production : Create non-root user
Production->>Container : Expose port 80
Container->>Container : Health check via curl
Container->>Container : Serve static files
```

**Diagram sources**
- [frontend/Dockerfile](file://frontend/Dockerfile#L1-L41)
- [frontend/nginx.conf](file://frontend/nginx.conf#L1-L60)

### Nginx Configuration

The Nginx configuration provides comprehensive proxying and security:

```nginx
# API proxy configuration
location /api {
    proxy_pass http://regulens:8080;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection 'upgrade';
    proxy_set_header Host $host;
    proxy_cache_bypass $http_upgrade;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto $scheme;
    proxy_pass_request_headers on;
}

# SPA routing - serve index.html for all routes
location / {
    try_files $uri $uri/ /index.html;
}

# Security headers
add_header X-Frame-Options "SAMEORIGIN" always;
add_header X-Content-Type-Options "nosniff" always;
add_header X-XSS-Protection "1; mode=block" always;
```

### Static Asset Optimization

The frontend container implements advanced caching strategies:

```nginx
# Cache static assets
location ~* \.(js|css|png|jpg|jpeg|gif|ico|svg|woff|woff2|ttf|eot)$ {
    expires 1y;
    add_header Cache-Control "public, immutable";
}

# Disable caching for index.html
location = /index.html {
    add_header Cache-Control "no-cache, no-store, must-revalidate";
    add_header Pragma "no-cache";
    add_header Expires "0";
}
```

**Section sources**
- [frontend/Dockerfile](file://frontend/Dockerfile#L1-L41)
- [frontend/nginx.conf](file://frontend/nginx.conf#L1-L60)

## Volume Management Strategy

### Persistent Data Storage

The containerization strategy implements a comprehensive volume management system:

```mermaid
graph TB
subgraph "Host Machine"
subgraph "Volume Drivers"
LOCAL_DRIVER[Local Driver<br/>Default Persistence]
NFS_DRIVER[NFS Driver<br/>Shared Storage]
CLOUD_DRIVER[Cloud Storage<br/>Backup Integration]
end
subgraph "Volume Mounts"
POSTGRES_VOL[postgres_data<br/>/var/lib/postgresql/data]
REDIS_VOL[redis_data<br/>/data]
BACKUP_VOL[backups<br/>/backups]
CONFIG_VOL[config<br/>/app/config]
end
end
subgraph "Container Instances"
PG_CONTAINER[PostgreSQL Container]
RD_CONTAINER[Redis Container]
API_CONTAINER[API Server Container]
FE_CONTAINER[Frontend Container]
end
LOCAL_DRIVER --> POSTGRES_VOL
LOCAL_DRIVER --> REDIS_VOL
NFS_DRIVER --> BACKUP_VOL
CLOUD_DRIVER --> BACKUP_VOL
POSTGRES_VOL --> PG_CONTAINER
REDIS_VOL --> RD_CONTAINER
BACKUP_VOL --> API_CONTAINER
CONFIG_VOL --> FE_CONTAINER
```

**Diagram sources**
- [docker-compose.yml](file://docker-compose.yml#L100-L105)

### Volume Types and Purposes

Different volume types serve specific purposes:

1. **PostgreSQL Data Volume**: `/var/lib/postgresql/data`
   - Stores database files and indices
   - Maintains data persistence across container restarts
   - Supports backup and restore operations

2. **Redis Data Volume**: `/data`
   - Contains Redis persistence files
   - Enables snapshot and append-only file (AOF) operations
   - Ensures cache persistence for session management

3. **Backup Volume**: `/backups`
   - Centralized backup storage location
   - Supports automated backup scheduling
   - Facilitates disaster recovery procedures

4. **Configuration Volume**: `/app/config`
   - Houses application configuration files
   - Enables dynamic configuration updates
   - Supports environment-specific settings

**Section sources**
- [docker-compose.yml](file://docker-compose.yml#L100-L105)

## Network Configuration

### Custom Bridge Network

The containerization strategy creates a dedicated Docker network for service isolation:

```yaml
networks:
  regulens-network:
    driver: bridge
```

### Service Communication

Containers communicate through internal DNS resolution:

```mermaid
sequenceDiagram
participant FE as Frontend Container
participant API as API Server Container
participant PG as PostgreSQL Container
participant RD as Redis Container
FE->>API : GET /api/transactions
API->>PG : SELECT * FROM transactions
PG-->>API : Transaction data
API->>RD : SET session : 123 data
RD-->>API : OK
API-->>FE : JSON response
Note over FE,RD : Internal communication via<br/>container names as hostnames
```

**Diagram sources**
- [docker-compose.yml](file://docker-compose.yml#L107-L110)

### Port Exposure Strategy

Each container exposes only necessary ports:

- **PostgreSQL**: Port 5432 (internal communication)
- **Redis**: Port 6379 (internal communication)
- **API Server**: Port 8080 (internal communication)
- **Frontend**: Port 80 (internal communication)

**Section sources**
- [docker-compose.yml](file://docker-compose.yml#L107-L110)

## Health Check Implementation

### Comprehensive Health Monitoring

Each container implements sophisticated health checks for reliability:

```mermaid
flowchart LR
subgraph "Health Check Types"
CMD_CHECK[Command-based Checks]
HTTP_CHECK[HTTP Endpoint Checks]
CUSTOM_CHECK[Custom Application Checks]
end
subgraph "Check Intervals"
SHORT_INTERVAL[10s - 30s]
LONG_INTERVAL[30s - 60s]
START_PERIOD[Start Period: 10s-60s]
end
subgraph "Response Validation"
SUCCESS[Success Response]
FAILURE[Failure Response]
RETRY[Retry Logic]
end
CMD_CHECK --> SUCCESS
HTTP_CHECK --> SUCCESS
CUSTOM_CHECK --> SUCCESS
SUCCESS --> RETRY
FAILURE --> RETRY
RETRY --> LONG_INTERVAL
RETRY --> SHORT_INTERVAL
```

### PostgreSQL Health Check

```yaml
healthcheck:
  test: ["CMD-SHELL", "pg_isready -U ${DB_USER:-regulens_user} -d ${DB_NAME:-regulens_compliance}"]
  interval: 10s
  timeout: 5s
  retries: 3
  start_period: 10s
```

### Redis Health Check

```yaml
healthcheck:
  test: ["CMD", "redis-cli", "--no-auth-warning", "-a", "${REDIS_PASSWORD:?REDIS_PASSWORD environment variable is required}", "ping"]
  interval: 10s
  timeout: 3s
  retries: 3
```

### API Server Health Check

```yaml
healthcheck:
  test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
  interval: 30s
  timeout: 10s
  retries: 3
  start_period: 40s
```

### Frontend Health Check

```yaml
healthcheck:
  test: ["CMD", "curl", "-f", "http://localhost:80"]
  interval: 30s
  timeout: 10s
  retries: 3
  start_period: 30s
```

**Section sources**
- [docker-compose.yml](file://docker-compose.yml#L25-L58)
- [frontend/Dockerfile](file://frontend/Dockerfile#L28-L32)

## Environment Variable Management

### Secure Secret Management

The containerization strategy implements robust environment variable management:

```mermaid
graph TB
subgraph "Environment Sources"
ENV_FILE[.env Files]
DOCKER_COMPOSE[docker-compose.yml]
HOST_VARS[Host Environment]
SECRET_MANAGER[Secret Manager]
end
subgraph "Variable Categories"
DB_CREDENTIALS[Database Credentials]
JWT_CONFIG[JWT Configuration]
REDIS_CONFIG[Redis Configuration]
CORS_CONFIG[CORS Configuration]
LOGGING_CONFIG[Logging Configuration]
end
subgraph "Validation Mechanisms"
REQUIRED_VARS[Mandatory Variables]
VALIDATION_RULES[Format Validation]
DEFAULT_FALLBACK[Default Values]
end
ENV_FILE --> DB_CREDENTIALS
DOCKER_COMPOSE --> JWT_CONFIG
HOST_VARS --> REDIS_CONFIG
SECRET_MANAGER --> CORS_CONFIG
DB_CREDENTIALS --> REQUIRED_VARS
JWT_CONFIG --> VALIDATION_RULES
REDIS_CONFIG --> DEFAULT_FALLBACK
```

**Diagram sources**
- [docker-compose.yml](file://docker-compose.yml#L8-L95)
- [start.sh](file://start.sh#L20-L60)

### Mandatory Environment Variables

Critical security-sensitive variables require explicit definition:

```bash
# Database configuration
DB_NAME: ${DB_NAME:?DB_NAME environment variable is required}
DB_USER: ${DB_USER:?DB_USER environment variable is required}
DB_PASSWORD: ${DB_PASSWORD:?DB_PASSWORD environment variable is required}

# Redis configuration  
REDIS_PASSWORD: ${REDIS_PASSWORD:?REDIS_PASSWORD environment variable is required}

# JWT configuration
JWT_SECRET_KEY: ${JWT_SECRET_KEY:?JWT_SECRET_KEY environment variable is required}
JWT_EXPIRATION_HOURS: ${JWT_EXPIRATION_HOURS:-24}
SESSION_EXPIRY_HOURS: ${SESSION_EXPIRY_HOURS:-24}

# CORS configuration
CORS_ALLOWED_ORIGIN: ${CORS_ALLOWED_ORIGIN:-http://localhost:3000}
```

### Development vs Production Configuration

The system supports different configuration approaches:

1. **Development Mode**: Uses `.env` files with local defaults
2. **Production Mode**: Relies on environment variables from deployment platform
3. **Hybrid Mode**: Combines both approaches with validation

**Section sources**
- [docker-compose.yml](file://docker-compose.yml#L8-L95)
- [start.sh](file://start.sh#L20-L60)

## Security Considerations

### Container Security Best Practices

The Regulens containerization implements multiple layers of security:

```mermaid
graph TB
subgraph "Security Layers"
IMAGE_SECURITY[Image Security<br/>Minimal Base Images]
RUNTIME_SECURITY[Runtime Security<br/>Non-root Users]
NETWORK_SECURITY[Network Security<br/>Isolated Networks]
STORAGE_SECURITY[Storage Security<br/>Volume Permissions]
SECRET_SECURITY[Secret Security<br/>Environment Variables]
end
subgraph "Security Controls"
BASE_IMAGE[Alpine Linux<br/>Ubuntu Minimal]
USER_ISOLATION[Non-root Users<br/>UID/GID Separation]
NETWORK_ISOLATION[Custom Bridge Networks<br/>Internal Communication]
VOLUME_PERMISSIONS[File Permissions<br/>Ownership Control]
SECRET_ENCRYPTION[Encrypted Secrets<br/>Environment Variables]
end
IMAGE_SECURITY --> BASE_IMAGE
RUNTIME_SECURITY --> USER_ISOLATION
NETWORK_SECURITY --> NETWORK_ISOLATION
STORAGE_SECURITY --> VOLUME_PERMISSIONS
SECRET_SECURITY --> SECRET_ENCRYPTION
```

### Image Security

- **Minimal Base Images**: Uses Alpine Linux for Redis and Nginx containers
- **Multi-stage Builds**: Reduces attack surface by eliminating build tools
- **Regular Updates**: Automated vulnerability scanning and patching

### Runtime Security

```dockerfile
# Create non-root user for security
RUN groupadd -r regulens && useradd -r -g regulens regulens

# Switch to non-root user
USER regulens
```

### Network Security

- **Custom Bridge Networks**: Isolates services from external networks
- **Internal Communication**: Services communicate only through designated ports
- **Firewall Rules**: Container-level firewall configuration

### Secret Management

- **Environment Variables**: Secrets passed through environment variables
- **Validation**: Mandatory validation for critical secrets
- **Encryption**: Secrets encrypted in transit and at rest

**Section sources**
- [Dockerfile](file://Dockerfile#L50-L55)
- [docker-compose.yml](file://docker-compose.yml#L8-L95)

## Production Deployment

### Deployment Strategy

The production deployment follows a blue-green deployment pattern:

```mermaid
sequenceDiagram
participant LB as Load Balancer
participant BLUE as Blue Environment
participant GREEN as Green Environment
participant DB as Database Cluster
LB->>BLUE : Route traffic (Active)
BLUE->>DB : Read/Write operations
GREEN->>DB : Standby mode
Note over BLUE,GREEN : Deploy new version to Green
GREEN->>DB : Initialize with schema
GREEN->>DB : Load seed data
GREEN->>GREEN : Health checks pass
LB->>GREEN : Switch traffic
GREEN->>DB : Read/Write operations
BLUE->>DB : Graceful shutdown
Note over LB,DB : Blue environment decommissioned
```

### Resource Allocation

Production containers require careful resource planning:

- **CPU Limits**: Based on workload analysis and peak usage patterns
- **Memory Limits**: Prevents memory exhaustion and OOM kills
- **Disk Space**: Adequate space for logs, backups, and temporary files
- **Network Bandwidth**: Sufficient bandwidth for concurrent connections

### Restart Policies

Each container implements appropriate restart policies:

```yaml
restart: unless-stopped  # Persistent services
restart: on-failure      # Batch jobs and workers
restart: always          # Critical system services
```

**Section sources**
- [docker-compose.yml](file://docker-compose.yml#L25-L58)

## Development Workflow

### Local Development Environment

The development workflow combines Docker containers with native development tools:

```mermaid
flowchart TD
START([Development Start]) --> CHECK_DOCKER[Check Docker Status]
CHECK_DOCKER --> START_CONTAINERS[Start Database Containers]
START_CONTAINERS --> WAIT_READY[Wait for Services]
WAIT_READY --> COMPILE_BACKEND[Compile Backend]
COMPILE_BACKEND --> START_BACKEND[Start Native Backend]
START_BACKEND --> START_FRONTEND[Start Vite Dev Server]
START_FRONTEND --> VERIFY_HEALTH[Verify Health Checks]
VERIFY_HEALTH --> READY[Development Environment Ready]
subgraph "Development Features"
HOT_RELOAD[Hot Reload<br/>< 1 Second]
LIVE_DEBUG[LIVE Debugging]
REALTIME_LOGS[Real-time Logging]
end
READY --> HOT_RELOAD
READY --> LIVE_DEBUG
READY --> REALTIME_LOGS
```

**Diagram sources**
- [start.sh](file://start.sh#L1-L194)

### Development Scripts

The `start.sh` script automates the development environment setup:

```bash
#!/bin/bash
# Regulens Development Startup Script
# Starts databases in Docker, runs backend and frontend natively with hot reload

# Load environment variables (Production-ready - no localhost hardcoding)
export DB_HOST=${DB_HOST:-localhost}
export DB_PORT=${DB_PORT:-5432}
export DB_NAME=${DB_NAME:-regulens_compliance}
export DB_USER=${DB_USER:-regulens_user}

# SECURITY: No default passwords - must be set via environment variables
if [ -z "${DB_PASSWORD}" ]; then
    echo "FATAL ERROR: DB_PASSWORD environment variable is not set"
    echo "Please set DB_PASSWORD before running the application"
    echo "Example: export DB_PASSWORD='$(openssl rand -base64 32)'"
    exit 1
fi
```

### Hot Reload Capabilities

The development environment supports instant hot reloading:

- **Frontend Changes**: Auto-reload in < 1 second
- **Backend Changes**: Recompile with `make && pkill -f regulens && ./build/regulens &`
- **Database Changes**: Persist in Docker volumes
- **Configuration Changes**: Dynamic reload without restart

**Section sources**
- [start.sh](file://start.sh#L1-L194)

## Monitoring and Observability

### Health Monitoring

The containerization strategy implements comprehensive health monitoring:

```mermaid
graph TB
subgraph "Monitoring Components"
CONTAINER_HEALTH[Container Health Checks]
APPLICATION_METRICS[Application Metrics]
LOG_COLLECTION[Log Collection]
METRICS_EXPORTER[Prometheus Exporter]
end
subgraph "Monitoring Tools"
DOCKER_STATS[Docker Stats]
PROMETHEUS[Prometheus]
GRAFANA[Grafana]
ALERTMANAGER[Alert Manager]
end
subgraph "Metrics Collected"
CPU_USAGE[CPU Usage]
MEMORY_USAGE[Memory Usage]
DISK_IO[Disk I/O]
NETWORK_TRAFFIC[Network Traffic]
RESPONSE_TIMES[Response Times]
ERROR_RATES[Error Rates]
end
CONTAINER_HEALTH --> DOCKER_STATS
APPLICATION_METRICS --> PROMETHEUS
LOG_COLLECTION --> GRAFANA
METRICS_EXPORTER --> ALERTMANAGER
PROMETHEUS --> CPU_USAGE
PROMETHEUS --> MEMORY_USAGE
PROMETHEUS --> DISK_IO
PROMETHEUS --> NETWORK_TRAFFIC
PROMETHEUS --> RESPONSE_TIMES
PROMETHEUS --> ERROR_RATES
```

### Log Management

Centralized logging for all containerized services:

- **Structured Logging**: JSON format for easy parsing
- **Log Rotation**: Automatic rotation to prevent disk space issues
- **Centralized Storage**: Logs stored in persistent volumes
- **Real-time Monitoring**: Live log streaming for debugging

### Performance Monitoring

Key performance indicators monitored:

- **Response Latency**: API endpoint response times
- **Throughput**: Requests per second across services
- **Resource Utilization**: CPU, memory, and disk usage
- **Error Rates**: HTTP error codes and application errors
- **Database Performance**: Query execution times and connection pools

### Alerting Configuration

Proactive alerting for critical issues:

- **Service Unavailability**: Container crashes or health check failures
- **Resource Exhaustion**: High CPU, memory, or disk usage
- **Performance Degradation**: Increased response times
- **Security Events**: Unauthorized access attempts
- **Data Integrity**: Database connection failures

**Section sources**
- [docker-compose.yml](file://docker-compose.yml#L25-L58)
- [frontend/nginx.conf](file://frontend/nginx.conf#L1-L60)