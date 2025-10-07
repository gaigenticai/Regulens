# WARP.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

## Project Overview

**Regulens** is a production-grade Agentic AI Compliance System for financial regulatory monitoring and decision-making. It implements sophisticated multi-agent AI architecture with real-time regulatory compliance monitoring, MCDA (Multi-Criteria Decision Analysis) algorithms, and enterprise-grade security.

**Status**: Production-ready (95% complete) with zero stub code, zero placeholders, and zero hardcoded secrets.

## Development Commands

### Build & Development
```bash
# Backend (C++ with CMake)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
./regulens

# Production build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Frontend (React + TypeScript + Vite)
cd frontend
npm install
npm run dev          # Start development server on port 3000
npm run build        # Production build
npm run type-check   # TypeScript validation
npm run lint         # ESLint

# Full stack development
docker-compose up -d postgres redis  # Start dependencies
./build/regulens &                   # Start backend on port 8080
cd frontend && npm run dev           # Start frontend on port 3000
```

### Testing
```bash
# Comprehensive test suite
./scripts/run_tests.sh                     # Full test suite with coverage
./scripts/run_tests.sh --skip-build        # Skip build step
./scripts/run_tests.sh --no-coverage       # Skip coverage report

# Unit tests only
cd build && make regulens_tests
./tests/regulens_tests

# E2E tests (Playwright)
npm test                          # All E2E tests
npm run test:headed               # With browser UI
npm run test:debug                # Debug mode
npm run test:chromium             # Chrome only
npm run test:ui                   # Interactive UI mode

# Frontend tests
cd frontend
npm run type-check                # TypeScript validation
```

### Database Management
```bash
# Setup database
psql -f schema.sql
psql -f seed_data.sql
psql -c "CREATE EXTENSION pg_stat_statements;"

# Test database connection
psql -h localhost -U regulens_user -d regulens_compliance

# Database with Docker
docker-compose up postgres
```

### Deployment
```bash
# Docker deployment
docker-compose up                              # Full stack
docker-compose --profile monitoring up        # With Prometheus/Grafana
docker-compose --profile with-proxy up        # With Nginx proxy

# Kubernetes deployment
kubectl apply -f infrastructure/k8s/crds/
kubectl apply -f infrastructure/k8s/deployments/

# Health checks
curl http://localhost:8080/health    # Application health
curl http://localhost:8080/ready     # Kubernetes readiness
curl http://localhost:8080/metrics   # Prometheus metrics
```

## High-Level Architecture

### Multi-Agent System Architecture
Regulens implements a sophisticated multi-agent AI system with the following core agents:

```
┌─────────────────────────────────────────────────────────────────┐
│                    AGENTIC ORCHESTRATOR                        │
│              (Central Intelligence & Coordination)             │
└─────────────────────────┬───────────────────────────────────────┘
                          │
        ┌─────────────────┼─────────────────┐
        │                 │                 │
        ▼                 ▼                 ▼
┌─────────────┐   ┌─────────────┐   ┌─────────────┐
│Transaction  │◄─►│ Decision    │◄─►│Regulatory   │
│ Guardian    │   │  Engine     │   │ Assessor    │
│(Fraud/Risk) │   │  (MCDA)     │   │(Compliance) │
└─────────────┘   └─────────────┘   └─────────────┘
        │                 │                 │
        └─────────────────┼─────────────────┘
                          │
                          ▼
                ┌─────────────┐
                │   Audit     │
                │Intelligence │
                │ (Analysis)  │
                └─────────────┘
```

### Data Flow Architecture
```
External Sources (SEC, FCA, ECB, RSS feeds)
           ▼ (REST APIs, Web Scraping, RSS parsing)
    Data Ingestion Framework
           ▼ (GDPR/CCPA compliance validation, PII detection)
    PostgreSQL + Redis Storage
           ▼ (Pattern recognition, anomaly detection)
    Multi-Agent Decision System (MCDA algorithms)
           ▼ (Audit trail, explainable decisions)
    Web UI Dashboard + REST API
```

### Technology Stack
- **Backend**: C++17/20, Boost libraries, pqxx (PostgreSQL), hiredis (Redis)
- **Database**: PostgreSQL with JSONB, Redis for caching
- **Frontend**: React 18.3.1, TypeScript 5.7.3, Vite 6.0.7, Tailwind CSS
- **HTTP Server**: Custom C++ server with kqueue/epoll
- **AI Integration**: OpenAI API, Anthropic Claude API
- **MCDA Algorithms**: TOPSIS, ELECTRE III, PROMETHEE II, AHP, VIKOR
- **Security**: PBKDF2 password hashing, JWT authentication, OAuth 2.0
- **Deployment**: Docker, Kubernetes with custom operators, Prometheus monitoring
- **Testing**: Google Test (C++), Playwright (E2E), comprehensive test coverage

## Key Architectural Components

### Core Agents
1. **Transaction Guardian Agent** (`agents/real_agent.cpp`)
   - Real-time fraud detection and transaction monitoring
   - Risk scoring with configurable thresholds
   - Velocity analysis and pattern detection
   - Integration with sanctioned entity lists

2. **Decision Engine** (`shared/decision_tree_optimizer.cpp`)
   - Implements production-grade MCDA algorithms (TOPSIS, ELECTRE III, PROMETHEE II, AHP)
   - Explainable AI decisions with confidence scores
   - Sensitivity analysis and scenario planning
   - Real mathematical implementations (not simplified versions)

3. **Regulatory Assessor Agent** (`regulatory_monitor/`)
   - Live monitoring of regulatory sources (SEC, FCA, ECB)
   - Change detection using Jaccard similarity
   - Impact assessment and risk analysis
   - Automated compliance validation

4. **Audit Intelligence** (`shared/audit/`)
   - Comprehensive audit trail management
   - Decision history analysis
   - Pattern recognition in audit events
   - Compliance reporting and analytics

### Data Ingestion Framework
Located in `data_ingestion/`, supports:
- **REST API Sources** with OAuth 2.0, Basic Auth, 4 pagination strategies
- **Web Scraping** with robots.txt compliance, CSS selectors, JSONPath parsing
- **Database Sources** with slow query detection and optimization recommendations
- **RSS/Atom Feeds** with content deduplication
- **Compliance Validation** for GDPR, CCPA, SOC2, PCI DSS

### MCDA Decision Engine
Production implementations in `shared/decision_tree_optimizer.cpp`:
- **TOPSIS**: Ideal/negative-ideal solutions with Euclidean distance
- **ELECTRE III**: Concordance/discordance matrices with veto thresholds
- **PROMETHEE II**: 6 preference function types with net flow calculations
- **AHP**: Eigenvector method with consistency ratio checking
- **VIKOR**: Compromise solution with utility measures

### Security Architecture
- **Authentication**: JWT tokens with configurable expiration, PBKDF2 password hashing (100K iterations)
- **Authorization**: Role-based access control with permission checking
- **Input Validation**: SQL injection prevention, XSS protection, rate limiting
- **Secrets Management**: No hardcoded secrets, environment variable enforcement
- **Network Security**: HTTPS enforcement, secure headers, constant-time comparisons

### Database Schema
Key tables in `schema.sql`:
- `regulatory_changes`: Real-time regulatory updates with impact scoring
- `agent_decisions`: Decision history with full MCDA calculations
- `audit_events`: Comprehensive audit trail with metadata
- `transaction_analysis`: Transaction monitoring and fraud detection results
- `agent_communication`: Inter-agent message passing and consensus
- `pattern_recognition`: ML-based pattern detection and learning

## Development Practices & Rules

### Coding Standards (from `.cursor/rules/rule.mdc`)
1. **NO STUBS OR PLACEHOLDERS**: Code must be production-grade with no fake/mock/dummy implementations
2. **Modular Design**: All features must be extensible without breaking existing functionality  
3. **Cloud-Ready**: Code must be deployable on-premise or cloud (AWS, GCP, Azure)
4. **Feature Understanding**: Must understand existing features before adding new ones
5. **Environment Configuration**: All variables must be in `.env.example`, database schema in `schema.sql`
6. **UI Testing**: Every feature requires proper UI components for testing
7. **Production Quality**: No makeshift workarounds or simplified versions
8. **Proper Naming**: Use actual feature names, not "Phase1", "Task1", etc.

### Critical Configuration Requirements
**REQUIRED Environment Variables** (application refuses to start without these):
```bash
export JWT_SECRET_KEY="<64-character-random-string>"     # Enforced minimum length
export DATABASE_URL="postgresql://user:pass@host:5432/regulens"
export OPENAI_API_KEY="sk-..."                          # Optional, fallback logic exists
export ANTHROPIC_API_KEY="sk-ant-..."                   # Optional, fallback logic exists
```

### API Endpoints (60+ production endpoints)
The system exposes a comprehensive REST API documented in `IMPLEMENTATION_COMPLETE.md`:
- **Dashboard**: `/api/activity`, `/api/activity/stats`
- **Decisions**: `/api/decisions`, `/api/decisions/tree`, `/api/decisions/visualize`
- **Regulatory**: `/api/regulatory-changes`, `/api/regulatory/sources`
- **Audit**: `/api/audit-trail`, `/api/audit/export`, `/api/audit/analytics`
- **Agents**: `/api/agents`, `/api/agents/status`, `/api/communication`
- **LLM Integration**: `/api/llm/completion`, `/api/llm/analysis`
- **Monitoring**: `/api/health`, `/metrics` (Prometheus), `/ready` (K8s probe)

### Frontend Architecture (`frontend/`)
Production-ready React application with:
- **No Mock Data**: All API calls connect to real backend endpoints
- **TypeScript Strict**: 500+ lines of type definitions, no `any` types
- **Real Authentication**: JWT token management, auto-refresh, protected routes  
- **State Management**: TanStack Query for server state, Zustand for client state
- **UI Components**: Responsive design, error boundaries, loading states
- **Build Optimization**: Code splitting, vendor chunks, lazy loading

### Performance & Scalability
Expected production performance:
- **API Response Time**: <50ms (p95)
- **Database Queries**: <10ms (p95) 
- **Decision Making**: <100ms for MCDA calculations
- **Throughput**: 1000+ requests/second
- **Concurrent Users**: 10,000+ supported

### Monitoring & Observability
- **Prometheus Metrics**: Exposed on `/metrics` endpoint
- **Structured Logging**: JSON logs with correlation IDs
- **Health Checks**: Kubernetes readiness/liveness probes
- **Circuit Breakers**: Resilience patterns for external service calls
- **Audit Trails**: Complete decision and action tracking

## Important Files & Directories

- `main.cpp` - Main application with component initialization and health checks
- `CMakeLists.txt` - Production-grade build configuration with cross-platform support
- `docker-compose.yml` - Full stack deployment with monitoring profiles
- `schema.sql` - Complete database schema with all required tables
- `.env.example` - 400+ configuration variables with production defaults
- `frontend/` - Production React application with no mock data
- `shared/` - Core business logic, agents, database connections, utilities
- `agents/` - Real agent implementations with actual regulatory data fetching
- `regulatory_monitor/` - Live regulatory change monitoring system
- `data_ingestion/` - Production data pipeline with compliance validation
- `infrastructure/k8s/` - Kubernetes operators and deployment manifests
- `tests/e2e/` - Comprehensive Playwright test suite (200+ test cases)
- `scripts/run_tests.sh` - Automated testing with coverage reporting

## Common Patterns & Best Practices

When working with this codebase:

1. **Database Operations**: Always use connection pooling, prepared statements, and transactions
2. **Agent Communication**: Use the `AgentOrchestrator` for inter-agent coordination
3. **Error Handling**: Implement circuit breakers and retry logic for external calls
4. **Configuration**: Use `ConfigurationManager` singleton for all settings
5. **Logging**: Use `StructuredLogger` with proper correlation IDs
6. **Security**: Validate all inputs, use constant-time comparisons for secrets
7. **MCDA Decisions**: Use `DecisionTreeOptimizer` for complex decision analysis
8. **Web UI**: Connect all React components to real API endpoints, no fallback mock data

This system represents a sophisticated, production-ready enterprise application with advanced AI capabilities, comprehensive testing, and deployment-ready infrastructure.